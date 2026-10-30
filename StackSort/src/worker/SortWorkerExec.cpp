#include <sstream>
#include <cstring>

#include "SortWorker.h"
#include "ResultCache.h"
#include "RefineCriteria.h"
#include "StackSortSettings.h"
#include "Log.h"
#include "SortPolicy.h"

// Caller holds cs->resultLock. Insert resizes the three vectors together;
// the minimum keeps every index in bounds regardless.
static int SlotCountLocked(const CachedSection* cs)
{
    int n = (int)cs->results.size();
    if ((int)cs->bestOrders.size() < n) n = (int)cs->bestOrders.size();
    if ((int)cs->slotDone.size() < n) n = (int)cs->slotDone.size();
    return n;
}

// Caller holds cs->resultLock. True while job may still read or write slot t
// of this entry. Insert and Invalidate bump generation under the same lock,
// so a passing check holds until the lock is released (see ResultCache.h).
static bool CanPublishLocked(const SortWorker::Job* job, const CachedSection* cs, int t)
{
    return SortPolicy::CanPublish(job->abortFlag, cs->generation, job->section.generation, t, SlotCountLocked(cs));
}

// Caller holds cs->resultLock and has passed CanPublishLocked for slot t, or
// for a higher slot with t >= 1 (only the upper bound depends on the slot).
// computedUpTo needs no CAS: every writer holds resultLock.
static void PublishSlotLocked(CachedSection* cs, int t, const Packer::Result& result,
                              const std::vector<Packer::Item>& bestOrder)
{
    cs->results[t - 1]    = result;
    cs->bestOrders[t - 1] = bestOrder;
    if (!cs->slotDone[t - 1])
    {
        cs->slotDone[t - 1] = 1;
        ++cs->numSlotsDone;
    }
    if ((LONG)t > cs->computedUpTo) InterlockedExchange(&cs->computedUpTo, (LONG)t);
    if (cs->valid == 0) InterlockedExchange(&cs->valid, 1);
}

// Build ASCII grid from packing result (debug only).
#ifdef STACKSORT_VERBOSE
static std::string BuildGridString(int gridW, int gridH, int reserveY, const Packer::Result& result,
                                   const std::vector<Packer::Item>& items)
{
    static const char TYPE_CHARS[]  = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static const int NUM_TYPE_CHARS = 36;

    std::vector<int> grid(gridW * gridH, 0);
    for (size_t i = 0; i < result.placements.size(); ++i)
    {
        const Packer::Placement& p = result.placements[i];
        int typeId                 = items[p.id].exactId;
        for (int dy = 0; dy < p.h; ++dy)
            for (int dx = 0; dx < p.w; ++dx)
                grid[(p.y + dy) * gridW + (p.x + dx)] = typeId + 1;
    }

    std::vector<unsigned char> ler(gridW * gridH, 0);
    for (int dy = 0; dy < result.lerHeight; ++dy)
        for (int dx = 0; dx < result.lerWidth; ++dx)
            ler[(result.lerY + dy) * gridW + (result.lerX + dx)] = 1;

    std::string s;
    for (int y = 0; y < gridH; ++y)
    {
        if (y == reserveY)
        {
            for (int x = 0; x < gridW; ++x)
                s += '-';
            s += " <- reserve\n";
        }
        for (int x = 0; x < gridW; ++x)
        {
            int idx = y * gridW + x;
            if (grid[idx] > 0)
            {
                int typeId = grid[idx] - 1;
                s += (typeId < NUM_TYPE_CHARS) ? TYPE_CHARS[typeId] : '#';
            }
            else if (ler[idx]) s += '~';
            else s += '.';
        }
        s += '\n';
    }
    return s;
}
#endif

// RAII trailing log for ExecuteJob. Runs on every return path, including
// unwinding from an exception on its way to WorkerProc's barrier; a throw out
// of a destructor during unwinding is std::terminate, so the body swallows its
// own (allocation) failures.
struct JobCleanup
{
    LARGE_INTEGER freq;
    LARGE_INTEGER totalStart;
    int workerIdx;
    volatile long* abortFlag;

    JobCleanup(int idx, volatile long* flag) : workerIdx(idx), abortFlag(flag)
    {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&totalStart);
    }

    ~JobCleanup()
    {
        if (GetLogLevel() < LOG_LEVEL_VERBOSE) return;

        try
        {
            LARGE_INTEGER totalEnd;
            QueryPerformanceCounter(&totalEnd);
            double totalMs = (double)(totalEnd.QuadPart - totalStart.QuadPart) / (double)freq.QuadPart * 1000.0;
            std::stringstream summary;
            summary << "[StackSort] W" << workerIdx << " finished";
            summary.precision(0);
            summary << std::fixed << ", " << totalMs << "ms total";
            if (*abortFlag != 0) summary << " (ABORTED)";
            WorkerLogVerbose(summary.str());
        }
        catch (...)
        {
        }
    }
};

void SortWorker::ExecuteJob(Job* job, int workerIdx)
{
    JobCleanup cleanup(workerIdx, &job->abortFlag);

    // Shared across every Pack call in this job so scratch allocations amortize.
    Packer::PackContext sharedCtx;

    SectionRef& ref = job->section;

    CachedSection* cs = ResultCache::Find(ref.key);
    if (!cs || cs->generation != ref.generation) return;

    // Snapshot packItems + dims under resultLock. Insert serializes on
    // the same lock, so the locals stay stable for the rest of the job.
    std::vector<Packer::Item> packItemsLocal;
    int gridWLocal             = 0;
    int gridHLocal             = 0;
    Packer::TargetDim dimLocal = Packer::TARGET_H;
    {
        ScopedLock rl(cs->resultLock);
        if (cs->generation != ref.generation) return;
        packItemsLocal = cs->packItems;
        gridWLocal     = cs->gridW;
        gridHLocal     = cs->gridH;
        dimLocal       = cs->dim;
    }

    if (job->target == 0)
    {
        // First-pass chunk: compute targets [targetStart, targetEnd]
        int gridW             = gridWLocal;
        int gridH             = gridHLocal;
        Packer::TargetDim dim = dimLocal;
        int maxSlots          = (dim == Packer::TARGET_H) ? (gridH - 1) : (gridW - 1);
        const char* dimTag    = (dim == Packer::TARGET_H) ? "H" : "W";

        int targetStart = (job->targetEnd > 0) ? job->targetStart : 1;
        int targetEnd   = (job->targetEnd > 0) ? job->targetEnd : maxSlots;
        if (targetStart < 1) targetStart = 1;
        if (targetEnd > maxSlots) targetEnd = maxSlots;

        // Always explicit, never NULL: path relinking stays off in the first
        // pass (the shipped, benchmarked configuration) whatever the packer's
        // NULL-params semantics are. Every other knob is at its compiled
        // default unless StackSort.cfg overrides the budget.
        const StackSortSettings& cfg         = StackSortSettingsStore::Get();
        Packer::SearchParams firstPassParams = Packer::SearchParams::defaults();
        firstPassParams.enablePathRelinking  = 0;
        if (cfg.firstPassRestarts > 0) firstPassParams.numRestarts = cfg.firstPassRestarts;
        if (cfg.firstPassIters > 0) firstPassParams.itersPerRestart = cfg.firstPassIters;

        std::vector<Packer::Item> lastBestOrder;
        int globalBestLERArea = 0;
        int maxLerSideCovered = 0;

        // Track target=1 skip destinations -- excluded from refinement since
        // the unconstrained result at target=1 is the widest possible LER.
        // Only meaningful for the chunk containing target=1.
        bool skipFromLoosest[64];
        memset(skipFromLoosest, 0, sizeof(skipFromLoosest));

        for (int t = targetStart; t <= targetEnd; ++t)
        {
            // Unlocked early-outs; the authoritative check is CanPublishLocked.
            if (job->abortFlag != 0) break;
            if (cs->generation != ref.generation) break;

            LARGE_INTEGER t0, t1;
            QueryPerformanceCounter(&t0);

            int skipThreshold = (t <= maxLerSideCovered) ? globalBestLERArea : 0;

            std::vector<Packer::Item> newBestOrder;
            Packer::GridSpec dims;
            dims.gridW            = gridW;
            dims.gridH            = gridH;
            dims.target           = t;
            Packer::Result result = Packer::Search::PackAnnealed(
                dims, packItemsLocal, dim, &job->abortFlag, lastBestOrder.empty() ? NULL : &lastBestOrder,
                &newBestOrder, skipThreshold, &firstPassParams, NULL, &sharedCtx);

            QueryPerformanceCounter(&t1);

            if (job->abortFlag != 0) break;
            if (cs->generation != ref.generation) break;

            // lastBestOrder now holds this target's best order: it seeds the
            // next target and is what bestOrders[t-1] must store. If the packer
            // left newBestOrder empty (rejected input), the previous target's
            // order stays, as the best seed available.
            if (!newBestOrder.empty()) lastBestOrder.swap(newBestOrder);

            int lerSide = (dim == Packer::TARGET_H) ? result.lerHeight : result.lerWidth;

            if (result.allPlaced)
            {
                if (result.lerArea > globalBestLERArea) globalBestLERArea = result.lerArea;
                if (lerSide > maxLerSideCovered) maxLerSideCovered = lerSide;
            }

            double ms = (double)(t1.QuadPart - t0.QuadPart) / (double)cleanup.freq.QuadPart * 1000.0;

            std::stringstream msg;
            msg << "[StackSort] W" << workerIdx << " [" << dimTag << "] target=" << t << ": LER " << result.lerWidth
                << "x" << result.lerHeight << "=" << result.lerArea << " at (" << result.lerX << "," << result.lerY
                << ")";
            msg.precision(1);
            msg << std::fixed << ", " << ms << "ms"
                << ", conc ";
            msg.precision(2);
            msg << result.concentration << ", str " << result.strandedCells << ", grp " << result.groupingBonus
                << (result.allPlaced ? "" : " PARTIAL");
            WorkerLogDebug(msg.str());
#ifdef STACKSORT_PACKER_GRID_LOG
            {
                int reserveLine = (dim == Packer::TARGET_H) ? (gridH - t) : gridH;
                WorkerLogDebug("\n" + BuildGridString(gridW, gridH, reserveLine, result, packItemsLocal));
            }
#endif

            // Forward skip: copy result to intervening slots in this chunk.
            // Clamped to targetEnd so chunks don't trample each other.
            int skipTo = t;
            if (result.allPlaced && lerSide > t && lerSide <= maxSlots)
                skipTo = (lerSide < targetEnd) ? lerSide : targetEnd;

            // Slot t and its skip slots publish in one locked block, so they
            // land together or not at all.
            bool published = false;
            {
                ScopedLock rl(cs->resultLock);
                if (CanPublishLocked(job, cs, skipTo))
                {
                    for (int st = t; st <= skipTo; ++st)
                        PublishSlotLocked(cs, st, result, lastBestOrder);
                    published = true;
                }
            }
            if (!published) break;

            if (skipTo > t)
            {
                if (t == 1)
                {
                    for (int skip = t + 1; skip <= skipTo && skip - 1 < 64; ++skip)
                        skipFromLoosest[skip - 1] = true;
                }
                WorkerLogDebug("[StackSort] W" + IntToStr(workerIdx) + " [" + dimTag + "] skipping target=" +
                               IntToStr(t + 1) + ".." + IntToStr(skipTo) + " (covered by target=" + IntToStr(t) + ")");
                t = skipTo;
            }
        }

        // Refinement requeue: enqueue suboptimal targets at low priority
        if (job->abortFlag == 0 && cs->generation == ref.generation)
        {
            struct RefineCandidate
            {
                int t;
                long long score;
            };
            RefineCandidate candidates[64];
            int numCandidates = 0;

            int perpendicular = (dim == Packer::TARGET_H) ? gridW : gridH;

            int totalFreeCells = gridW * gridH;
            for (size_t pi = 0; pi < packItemsLocal.size(); ++pi)
                totalFreeCells -= packItemsLocal[pi].w * packItemsLocal[pi].h;

            // candidates[] and skipFromLoosest[] are both sized 64 --
            // safe for any plausible Kenshi grid (max ~20 per side).
            bool candidateOverflow = false;
            {
                ScopedLock rl(cs->resultLock);
                for (int rt = targetStart; rt <= targetEnd; ++rt)
                {
                    if (!CanPublishLocked(job, cs, rt)) break;
                    if (!cs->slotDone[rt - 1]) continue;
                    if (rt - 1 < 64 && skipFromLoosest[rt - 1]) continue;
                    if (NeedsRefinement(cs->results[rt - 1], dim, perpendicular, rt, totalFreeCells,
                                        (int)packItemsLocal.size(), 0,
                                        cfg.refineItemThreshold > 0 ? cfg.refineItemThreshold : REFINE_ITEM_THRESHOLD))
                    {
                        if (numCandidates >= 64)
                        {
                            candidateOverflow = true;
                            break;
                        }
                        candidates[numCandidates].t     = rt;
                        candidates[numCandidates].score = cs->results[rt - 1].score;
                        ++numCandidates;
                    }
                }
            }

            // Never log while holding a lock.
            if (candidateOverflow)
                WorkerLogError("[StackSort] refinement candidate overflow"
                               " (>64); truncating. Bump candidates[] if"
                               " Kenshi ever ships grids > 64 per side.");

            // Sort worst-first (ascending score)
            for (int si = 1; si < numCandidates; ++si)
            {
                RefineCandidate tmp = candidates[si];
                int sj              = si - 1;
                while (sj >= 0 && candidates[sj].score > tmp.score)
                {
                    candidates[sj + 1] = candidates[sj];
                    --sj;
                }
                candidates[sj + 1] = tmp;
            }

            if (numCandidates > 0)
            {
                // Built before EnqueueRefinementBatch takes ownership, so a
                // bad_alloc partway frees what was made before it propagates
                // to WorkerProc's barrier (which only handles the parent).
                Job* refJobs[64];
                int made = 0;
                try
                {
                    for (; made < numCandidates; ++made)
                        refJobs[made] = new Job();
                }
                catch (...)
                {
                    for (int k = 0; k < made; ++k)
                        delete refJobs[k];
                    throw;
                }
                for (int ci = 0; ci < numCandidates; ++ci)
                {
                    Job* rjob                = refJobs[ci];
                    rjob->sourceGUI          = job->sourceGUI;
                    rjob->section.key        = ref.key;
                    rjob->section.generation = ref.generation;
                    rjob->abortFlag          = 0;
                    rjob->dim                = dim;
                    rjob->target             = candidates[ci].t;
                    rjob->targetStart        = 0;
                    rjob->targetEnd          = 0;
                    rjob->priority           = 1;
                    rjob->sweepSlot          = job->sweepSlot;
                    rjob->sweepId            = job->sweepId;
                }
                if (EnqueueRefinementBatch(job, cs, refJobs, numCandidates))
                    WorkerLogVerbose("[StackSort] W" + IntToStr(workerIdx) + " [" + dimTag + "] requeued " +
                                     IntToStr(numCandidates) + " targets for refinement");
            }
        }
    }
    else if (job->target > 0)
    {
        // Refinement: re-run LAHC for a single H with amplified parameters
        int gridW             = gridWLocal;
        int gridH             = gridHLocal;
        Packer::TargetDim dim = dimLocal;
        int target            = job->target;
        int maxSlots          = (dim == Packer::TARGET_H) ? (gridH - 1) : (gridW - 1);
        const char* dimTag    = (dim == Packer::TARGET_H) ? "H" : "W";

        if (target < 1 || target > maxSlots) return;

        // Refinement inherits the first-pass budget overrides and amplifies
        // them. Unlike the first pass, path relinking stays at its default.
        const StackSortSettings& cfg         = StackSortSettingsStore::Get();
        Packer::SearchParams firstPassParams = Packer::SearchParams::defaults();
        if (cfg.firstPassRestarts > 0) firstPassParams.numRestarts = cfg.firstPassRestarts;
        if (cfg.firstPassIters > 0) firstPassParams.itersPerRestart = cfg.firstPassIters;

        Packer::SearchParams refineParams = firstPassParams;
        refineParams.numRestarts          = (cfg.refineRestarts > 0) ? cfg.refineRestarts : REFINE_RESTARTS;
        refineParams.itersPerRestart      = (cfg.refineIters > 0) ? cfg.refineIters : REFINE_ITERS;
        refineParams.lahcHistoryLen       = (cfg.refineLahcHistory > 0) ? cfg.refineLahcHistory : REFINE_LAHC_HIST;
        refineParams.plateauThreshold     = (cfg.refinePlateau > 0) ? cfg.refinePlateau : REFINE_PLATEAU;

        // Copy seed and oldScore under resultLock with the full publish check:
        // Insert can resize the vectors at any time, and a seed from a newer
        // generation would index a different item list than packItemsLocal.
        long long oldScore = 0;
        std::vector<Packer::Item> seedCopy;
        bool hasSeed = false;
        {
            ScopedLock rl(cs->resultLock);
            if (!CanPublishLocked(job, cs, target) || !cs->slotDone[target - 1]) return;
            oldScore = cs->results[target - 1].score;
            if (!cs->bestOrders[target - 1].empty())
            {
                seedCopy = cs->bestOrders[target - 1];
                hasSeed  = true;
            }
        }

        LARGE_INTEGER t0, t1;
        QueryPerformanceCounter(&t0);

        std::vector<Packer::Item> newBestOrder;
        Packer::GridSpec dims;
        dims.gridW  = gridW;
        dims.gridH  = gridH;
        dims.target = target;
        Packer::Result refined =
            Packer::Search::PackAnnealed(dims, packItemsLocal, dim, &job->abortFlag, hasSeed ? &seedCopy : NULL,
                                         &newBestOrder, 0, &refineParams, NULL, &sharedCtx);

        QueryPerformanceCounter(&t1);

        if (job->abortFlag != 0 || cs->generation != ref.generation) return;

        double ms = (double)(t1.QuadPart - t0.QuadPart) / (double)cleanup.freq.QuadPart * 1000.0;

        bool improved = refined.allPlaced && refined.score > oldScore;
        if (improved)
        {
            ScopedLock rl(cs->resultLock);
            if (!CanPublishLocked(job, cs, target) || !cs->slotDone[target - 1]) return;

            // Against the live slot as well as oldScore, so a slower duplicate
            // can never replace a better result.
            improved = refined.score > cs->results[target - 1].score;
            if (improved)
            {
                cs->results[target - 1] = refined;
                if (!newBestOrder.empty()) cs->bestOrders[target - 1] = newBestOrder;
            }
        }

        if (improved)
        {
            std::stringstream rmsg;
            rmsg << "[StackSort] W" << workerIdx << " [" << dimTag << "] REFINED target=" << target << ": score "
                 << oldScore << " -> " << refined.score;
            rmsg.precision(1);
            rmsg << std::fixed << " (" << ms << "ms)";
            WorkerLogDebug(rmsg.str());
#ifdef STACKSORT_PACKER_GRID_LOG
            {
                int reserveLine = (dim == Packer::TARGET_H) ? (gridH - target) : gridH;
                WorkerLogDebug("\n" + BuildGridString(gridW, gridH, reserveLine, refined, packItemsLocal));
            }
#endif
        }
        else
        {
            std::stringstream rmsg;
            rmsg << "[StackSort] W" << workerIdx << " [" << dimTag << "] refine target=" << target
                 << ": no improvement";
            rmsg.precision(1);
            rmsg << std::fixed << " (" << ms << "ms)";
            WorkerLogDebug(rmsg.str());
        }
    }
}
