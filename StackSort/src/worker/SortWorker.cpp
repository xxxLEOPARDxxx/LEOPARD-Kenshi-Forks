#pragma warning(push)
#pragma warning(disable : 4091)
#include <kenshi/GameData.h>
#include <kenshi/Inventory.h>
#include <kenshi/gui/InventoryGUI.h>
#pragma warning(pop)

#include <algorithm>
#include <iomanip>
#include <map>
#include <new>
#include <sstream>

#include "SortWorker.h"
#include "ResultCache.h"
#include "StackSort.h"
#include "StackSortConfig.h"
#include "StackSortSettings.h"
#include "SectionEligibility.h"
#include "Log.h"

// Active target dim (H or W). Runtime-toggleable via ToggleDim().
// Written only under s_queueLock to serialize with EnqueueJob.
static volatile LONG s_currentDim = (LONG)Packer::TARGET_H;

// Per-GUI tracking (main-thread-only).

struct GUITracking
{
    InventoryGUI* gui;        // NULL = free slot
    Inventory* trackedInv[2]; // body + backpack
    int numTrackedInv;
    Inventory* trackedBackpack;                  // getBackpack()->inventory at the last snapshot, NULL if none
    InventorySection* sectionKeys[MAX_SECTIONS]; // keys into ResultCache
    // Parallel to sectionKeys: the inventory that section was scanned from,
    // so a mutation can be mapped to its sections without calling into the
    // game from the callback.
    Inventory* sectionInv[MAX_SECTIONS];
    // Parallel to sectionKeys: true when the last scan missed the cache and
    // re-snapshotted that section. EnqueueJob only computes those.
    bool sectionNeedsCompute[MAX_SECTIONS];
    int numSections;
    bool mutationPending;
    DWORD lastMutationTick;
    // Tick and s_pollCount at the last _NV_update of this GUI (StampSlot). A
    // GUI destroyed without show(false) stops updating, and both stamps then
    // only age: they are never adjusted after the fact, so a dead slot cannot
    // be made to look live again (see IsSlotLive, ReclaimStaleTracking).
    DWORD lastSeenTick;
    DWORD lastSeenPoll;
};

static GUITracking s_guiTracking[MAX_GUIS];

// Advanced once per PollWorkerStart call, i.e. once per _NV_update of each
// visible top-level non-trader GUI. Main thread only. Unlike wall time it
// stands still while nothing polls (loading screen, no inventory open,
// alt-tab), so "polls missed" counts update passes a slot sat out while other
// GUIs were demonstrably updating. Unsigned subtraction survives the wrap.
static DWORD s_pollCount = 0;

// A slot is live only if BOTH stamps are recent. Wall time catches a GUI that
// died while polling stopped (world teardown: every GUI stops at once, so the
// poll count alone would read ~1). The poll count catches one that died while
// others kept updating, sooner than the wall bound. A live GUI stamps every
// frame and each frame advances s_pollCount once per polling GUI, so its
// missed count stays at or below the number of visible inventory GUIs.
// Liveness is a heuristic with a window of up to TRACKING_LIVE_POLLS polls, so
// it never licenses dereferencing a slot's gui: that happens only inside the
// GUI's own _NV_update (OnTrackedGUIUpdate), the one proof it is alive.
static const LONG TRACKING_LIVE_MS     = 2000;
static const DWORD TRACKING_LIVE_POLLS = 2 * MAX_GUIS;
// Reclaim needs both bounds too. The wall bound alone would free a live GUI's
// slot on the first poll after a long stall, before that GUI's own update
// re-stamps it; the poll bound requires ~10 s of other GUIs updating without
// this one.
static const LONG TRACKING_RECLAIM_MS     = 30000;
static const DWORD TRACKING_RECLAIM_POLLS = 600;

// resnapshotsPerFrame budget, spent in OnTrackedGUIUpdate. There is no frame
// hook, but every visible GUI updates once per frame, so a new frame has begun
// when a slot that already updated since s_frameStartPoll updates again. A
// slot allocated mid-frame also restarts the count; that only lets one frame
// overshoot the budget. Main thread only.
static DWORD s_frameStartPoll     = 0;
static int s_resnapshotsThisFrame = 0;

// Latched after the first "tracking slots full" error, cleared when a slot
// frees, so a full table logs once per episode instead of every frame.
static bool s_trackingFullLogged = false;

// Main-thread only. Written from startPlugin before any worker exists.
static bool s_mutationTrackingAvailable = true;

// Signed so a stamp taken after `then` was sampled reads as age <= 0 rather
// than wrapping to ~49 days; also survives the GetTickCount wrap.
static LONG TickAge(DWORD now, DWORD then)
{
    return (LONG)(now - then);
}

static void StampSlot(GUITracking& t)
{
    t.lastSeenTick = GetTickCount();
    t.lastSeenPoll = s_pollCount;
}

// True when the slot's GUI updated recently. Reads only the stamps, never
// t.gui, and is no license to dereference t.gui (see TRACKING_LIVE_MS).
static bool IsSlotLive(const GUITracking& t, DWORD now)
{
    return TickAge(now, t.lastSeenTick) <= TRACKING_LIVE_MS &&
           (DWORD)(s_pollCount - t.lastSeenPoll) <= TRACKING_LIVE_POLLS;
}

// Latched after the first exception caught at a main-thread entry point, so a
// persistent std::bad_alloc logs once instead of every frame.
static bool s_mainExceptionLogged = false;

// Main thread, inside a catch(...). /EHsc catches C++ exceptions only
// (realistically std::bad_alloc); SEH faults deliberately still propagate.
// Never throws: the log line itself may fail to allocate.
static void ReportMainThreadException(const char* where)
{
    if (s_mainExceptionLogged) return;
    s_mainExceptionLogged = true;
    try
    {
        LogError(std::string("[StackSort] ") + where +
                 " threw (out of memory?); affected inventories re-snapshot later");
    }
    catch (...)
    {
    }
}

// Values read from live items only. Skips NULL slots, in section order, which
// is the order ScanInventory builds packItems in.
//
// With KenshiRotate the dims are stored as the unordered pair (min, max):
// applyLayout rotates items to its placements' orientation under ApplyGuard,
// so no callback refreshes the snapshot, and an orientation-exact identity
// would reject every later click and reopen after a rotated apply.
// applyLayout reconciles orientation against p.w/p.h either way, and the
// unordered pair still rejects a differently sized item at a reused address.
// Without KenshiRotate nothing can swap or reconcile dims, so they stay exact.
static void FillIdentity(Item* item, bool orientationFree, ItemIdentity& id)
{
    const hand& h      = item->handle;
    id.item            = item;
    id.index           = h.index;
    id.serial          = h.serial;
    id.container       = h.container;
    id.containerSerial = h.containerSerial;
    id.type            = (int)h.type;
    id.w               = item->itemWidth;
    id.h               = item->itemHeight;
    if (orientationFree && id.w > id.h) std::swap(id.w, id.h);
}

static void ReadLiveIdentities(InventorySection* section, std::vector<ItemIdentity>& out)
{
    out.clear();
    const bool orientationFree                                            = StackSort_RotateAvailable();
    const Ogre::vector<InventorySection::SectionItem>::type& sectionItems = section->getItems();
    out.reserve(sectionItems.size());
    for (size_t i = 0; i < sectionItems.size(); ++i)
    {
        Item* item = sectionItems[i].item;
        if (!item) continue;

        ItemIdentity id;
        FillIdentity(item, orientationFree, id);
        out.push_back(id);
    }
}

void SortWorker::ReadItemIdentity(Item* item, ItemIdentity& out)
{
    FillIdentity(item, StackSort_RotateAvailable(), out);
}

typedef SortWorker::SectionRef SectionRef;
typedef SortWorker::Job Job;

static std::vector<Job*> s_jobQueue;
static CRITICAL_SECTION s_queueLock;

// Resolved from num_workers in Init, before any thread exists. Arrays are
// sized for the setting's maximum so the count can shrink without reallocating.
static int s_numWorkers = 4;
static HANDLE s_threads[MAX_WORKERS];
static HANDLE s_workSemaphore   = NULL;
static volatile LONG s_shutdown = 0;
static volatile Job* s_activeJobs[MAX_WORKERS];
// Set by the first worker to catch a job exception, so the error logs once.
static volatile LONG s_workerExceptionLogged = 0;
// Main-thread only: ApplyGuard writes, inventory mutation hooks read.
static bool s_applying = false;

// Lines queued by worker threads (and LogSweep) for the main thread to emit; see
// SortWorker::QueueLog. s_logLock is a leaf: it is taken alone, never while
// another lock is held and never around a call that takes one, and no Log
// function runs while it is held. Initialized in Init before any thread exists;
// s_logReady gates every use, since function-local statics are not thread-safe
// on VS2010.
struct LogEntry
{
    int kind;
    std::string line;
};

static std::vector<LogEntry> s_logQueue;
static CRITICAL_SECTION s_logLock;
static volatile LONG s_logReady = 0;

// One sweep per section snapshot: opened when its first-pass chunks are
// enqueued, closed when the last job of that snapshot completes or is dropped.
// Every field is guarded by s_queueLock. The digest is computed and the line is
// logged outside every lock -- see LogSweep.
struct SweepStats
{
    bool active;
    int id;
    int outstanding;
    int firstPass;
    int refine;
    int aborted;
    LARGE_INTEGER start;
    Packer::TargetDim dim;
    int gridW;
    int gridH;
    int items;
    InventorySection* sectionKey;
    // The entry generation every job of this sweep carries. LogSweep compares
    // it under resultLock, the lock every generation bump holds.
    LONG generation;
};

// Worst case in flight: one first-pass chunk per worker for every tracked
// section, plus one refinement job per target (ExecuteJob caps a batch at 64).
static const LONG MAX_QUEUED_JOBS = MAX_GUIS * MAX_SECTIONS * (MAX_WORKERS + 64);

static SweepStats s_sweeps[MAX_GUIS * MAX_SECTIONS];
static int s_nextSweepId = 1;
static LARGE_INTEGER s_qpcFreq;

// Returns -1 when the table is full; the caller logs that after releasing
// s_queueLock, since nothing may log while the lock is held.
static int OpenSweepLocked(CachedSection* cs, Packer::TargetDim dim, int chunkCount, int& outId)
{
    for (int i = 0; i < MAX_GUIS * MAX_SECTIONS; ++i)
    {
        if (s_sweeps[i].active) continue;

        SweepStats& s = s_sweeps[i];
        s.active      = true;
        s.id          = s_nextSweepId++;
        s.outstanding = chunkCount;
        s.firstPass   = chunkCount;
        s.refine      = 0;
        s.aborted     = 0;
        QueryPerformanceCounter(&s.start);
        s.dim        = dim;
        s.gridW      = cs->gridW;
        s.gridH      = cs->gridH;
        s.items      = cs->itemCount;
        s.sectionKey = cs->section;
        s.generation = cs->generation;
        outId        = s.id;
        return i;
    }
    return -1;
}

static void AddRefineLocked(int slot, int sweepId, int count)
{
    if (slot < 0 || slot >= MAX_GUIS * MAX_SECTIONS) return;

    SweepStats& s = s_sweeps[slot];
    if (!s.active || s.id != sweepId) return;

    s.outstanding += count;
    s.refine += count;
}

// Returns true, with a copy in out, when this decrement closed the sweep; the
// slot is freed in that case so the caller can log without holding the lock.
static bool CloseIfDoneLocked(int slot, int sweepId, int abortedCount, SweepStats& out)
{
    if (slot < 0 || slot >= MAX_GUIS * MAX_SECTIONS) return false;

    SweepStats& s = s_sweeps[slot];
    if (!s.active || s.id != sweepId) return false;

    s.aborted += abortedCount;
    --s.outstanding;
    if (s.outstanding > 0) return false;

    out      = s;
    s.active = false;
    return true;
}

// Abort paths collect closed sweeps under s_queueLock and log them afterwards.
// Capacity is reserved before the lock (one entry per sweep slot, the most a
// single abort can close) so the in-lock append never allocates: an abort must
// finish deleting and unlinking its jobs even when memory is short. If the
// reserve fails, the lines are dropped instead.
static void ReserveSweepLog(std::vector<SweepStats>& v)
{
    try
    {
        v.reserve(MAX_GUIS * MAX_SECTIONS);
    }
    catch (...)
    {
    }
}

static void KeepSweepLog(std::vector<SweepStats>& v, const SweepStats& s)
{
    if (v.size() < v.capacity()) v.push_back(s);
}

static void FnvMix(unsigned int& h, long long v)
{
    const unsigned char* p = (const unsigned char*)&v;
    for (size_t i = 0; i < sizeof(v); ++i)
    {
        h ^= p[i];
        h *= 16777619u;
    }
}

// Caller holds cs->resultLock. Under the publication invariant computedUpTo
// never exceeds the slot count; the clamp keeps readers in bounds regardless.
// Slots at or below the limit may still be unpublished -- check slotDone.
static int ComputedLimitLocked(const CachedSection* cs)
{
    int computed = (int)cs->computedUpTo;
    if (computed > (int)cs->results.size()) computed = (int)cs->results.size();
    if (computed > (int)cs->slotDone.size()) computed = (int)cs->slotDone.size();
    return computed;
}

// Digest is FNV-1a over (target, score, lerArea, lerWidth, lerHeight) of every
// result the sweep left in the cache. The packer seeds deterministically from
// (target, numItems), so two builds that agree on the digest agree on scoring.
// Any thread: routed through the log queue because workers close sweeps too.
// The closing job's aborted/dropped verdict is read before s_queueLock, so an
// Insert or Invalidate can still land between that read and this walk and
// reset the slots. The generation is therefore re-checked under the same
// resultLock the walk holds (every bump and slot reset happens under it): a
// replaced snapshot reports every job as aborted, since none of the sweep's
// results survive, and pgo_ab_parse.py drops the row.
static void LogSweep(const SweepStats& s)
{
    // Skips the resultLock walk too: LogInfo would drop the line anyway.
    if (GetLogLevel() < LOG_LEVEL_INFO) return;

    unsigned int digest = 2166136261u;
    int aborted         = s.aborted;
    bool replaced       = true;

    CachedSection* cs = ResultCache::Find(s.sectionKey);
    if (cs && cs->resultLockInit)
    {
        ScopedLock rl(cs->resultLock);
        replaced     = cs->generation != s.generation;
        int computed = replaced ? 0 : ComputedLimitLocked(cs);
        for (int t = 1; t <= computed; ++t)
        {
            if (!cs->slotDone[t - 1]) continue;
            const Packer::Result& r = cs->results[t - 1];
            FnvMix(digest, (long long)t);
            FnvMix(digest, r.score);
            FnvMix(digest, (long long)r.lerArea);
            FnvMix(digest, (long long)r.lerWidth);
            FnvMix(digest, (long long)r.lerHeight);
        }
    }
    if (replaced) aborted = s.firstPass + s.refine;

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double wallMs = 0.0;
    if (s_qpcFreq.QuadPart) wallMs = (double)(now.QuadPart - s.start.QuadPart) / (double)s_qpcFreq.QuadPart * 1000.0;

    std::stringstream ss;
    ss << "[StackSort] sweep #" << s.id << " dim=" << ((s.dim == Packer::TARGET_H) ? "H" : "W") << " grid=" << s.gridW
       << "x" << s.gridH << " items=" << s.items << " jobs=" << s.firstPass << "+" << s.refine << " aborted=" << aborted
       << " wall_ms=" << (long long)(wallMs + 0.5) << " digest=" << std::hex << std::setw(8) << std::setfill('0')
       << digest;
    WorkerLogInfo(ss.str());
}

void SortWorker::QueueLog(LogKind kind, const std::string& line)
{
    if (!s_logReady) return;

    // String copy made before the lock to keep the critical section to one push.
    LogEntry e;
    e.kind = (int)kind;
    e.line = line;

    ScopedLock guard(s_logLock);
    s_logQueue.push_back(e);
}

void SortWorker::FlushWorkerLog()
{
    if (!s_logReady) return;

    std::vector<LogEntry> drained;
    {
        ScopedLock guard(s_logLock);
        drained.swap(s_logQueue);
    }

    for (size_t i = 0; i < drained.size(); ++i)
    {
        const LogEntry& e = drained[i];
        switch (e.kind)
        {
        case LOG_KIND_ERROR:
            LogError(e.line);
            break;
        case LOG_KIND_INFO:
            LogInfo(e.line);
            break;
        case LOG_KIND_VERBOSE:
            LogVerbose(e.line);
            break;
        default:
            LogDebug(e.line);
            break;
        }
    }
}

int SortWorker::FindTracking(InventoryGUI* gui)
{
    for (int i = 0; i < MAX_GUIS; ++i)
    {
        if (s_guiTracking[i].gui == gui) return i;
    }
    return -1;
}

int SortWorker::AllocTracking(InventoryGUI* gui)
{
    for (int i = 0; i < MAX_GUIS; ++i)
    {
        if (s_guiTracking[i].gui == NULL)
        {
            GUITracking& t     = s_guiTracking[i];
            t.gui              = gui;
            t.numTrackedInv    = 0;
            t.trackedBackpack  = NULL;
            t.numSections      = 0;
            t.mutationPending  = false;
            t.lastMutationTick = 0;
            StampSlot(t);
            return i;
        }
    }
    // OnInventoryOpened retries every frame while the table is full.
    if (!s_trackingFullLogged)
    {
        s_trackingFullLogged = true;
        LogError("[StackSort] All GUI tracking slots full (max " + IntToStr(MAX_GUIS) + ")");
    }
    return -1;
}

void SortWorker::FreeTracking(int idx)
{
    GUITracking& t       = s_guiTracking[idx];
    t.gui                = NULL;
    t.numTrackedInv      = 0;
    t.trackedBackpack    = NULL;
    t.numSections        = 0;
    t.mutationPending    = false;
    t.lastMutationTick   = 0;
    t.lastSeenTick       = 0;
    t.lastSeenPoll       = 0;
    s_trackingFullLogged = false;
}

void SortWorker::ScanInventory(int trackIdx, Inventory* inv, bool* outNeedsCompute)
{
    if (!inv) return;

    GUITracking& t = s_guiTracking[trackIdx];

    lektor<InventorySection*>& sections = inv->getAllSections();
    for (lektor<InventorySection*>::iterator it = sections.begin(); it != sections.end(); ++it)
    {
        if (t.numSections >= MAX_SECTIONS)
        {
            LogError("[StackSort] Inventory has more than " + IntToStr(MAX_SECTIONS) +
                     " eligible sections; the rest are not sorted");
            break;
        }

        InventorySection* section = *it;

        if (!StackSort_EligibleSection(section)) continue;
        if (!section->enabled) continue;

        const Ogre::vector<InventorySection::SectionItem>::type& sectionItems = section->getItems();
        if (sectionItems.empty()) continue;

        int gridW             = section->width;
        int gridH             = section->height;
        Packer::TargetDim dim = GetDim();

        // identities[i] describes packItems[i]: both skip NULL slots in
        // section order.
        std::vector<ItemIdentity> identities;
        ReadLiveIdentities(section, identities);
        if (identities.empty()) continue;
        int itemCount = (int)identities.size();

        // Reuse needs the live item set to equal the snapshot's 1:1, not just
        // the count: while no GUI tracks this section nothing invalidates it.
        // Without mutation tracking a cached snapshot can never be trusted.
        std::vector<ItemIdentity> liveSorted(identities);
        std::sort(liveSorted.begin(), liveSorted.end());
        if (s_mutationTrackingAvailable && ResultCache::IsReusable(section, liveSorted, gridW, gridH, dim))
        {
            LogDebug("[StackSort] Cache hit: section=" + IntToStr((int)(intptr_t)section) + " '" + section->name + "'" +
                     " (" + IntToStr(itemCount) + " items)");
            t.sectionKeys[t.numSections]         = section;
            t.sectionInv[t.numSections]          = inv;
            t.sectionNeedsCompute[t.numSections] = false;
            ++t.numSections;
            continue;
        }

        // Cache miss or stale — build fresh snapshot
        std::vector<Packer::Item> packItems;

        // GameData pointer dedup — same template → same exactId. Other tier
        // fields read off the template directly; no cache needed.
        std::map<GameData*, int> exactIdMap;
        int nextExactId = 0;

        // GameData::bdata is a boost::unordered_map with a custom Ogre
        // allocator. C++03 has no `auto`, so the map type must be spelled
        // out to call .find(). Pattern copied from CatalogDump.cpp:199.
        typedef boost::unordered::unordered_map<
            std::string, bool, boost::hash<std::string>, std::equal_to<std::string>,
            Ogre::STLAllocator<std::pair<std::string const, bool>, Ogre::GeneralAllocPolicy> >
            BdataMap;

        for (size_t i = 0; i < sectionItems.size(); ++i)
        {
            const InventorySection::SectionItem& si = sectionItems[i];
            if (!si.item) continue;

            GameData* gd = si.item->data;

            Packer::Item pi;
            pi.id        = (int)packItems.size();
            pi.w         = si.item->itemWidth;
            pi.h         = si.item->itemHeight;
            pi.canRotate = StackSort_CanRotate(si.item);

            if (gd)
            {
                std::map<GameData*, int>::iterator it = exactIdMap.find(gd);
                if (it == exactIdMap.end())
                {
                    exactIdMap[gd] = nextExactId;
                    pi.exactId     = nextExactId++;
                }
                else
                {
                    pi.exactId = it->second;
                }
            }
            else
            {
                pi.exactId = -1;
            }

            // Custom groups (tier 2) are not wired yet — config-file loader
            // is a follow-on. -1 keeps the tier disabled in PairWeight.
            pi.customGroupId = -1;

            // gameDataType: generic ITEM (enum 4) is too broad to cluster on
            // — treat it as no-data so the tier skips.
            if (gd)
            {
                int t           = (int)gd->type;
                pi.gameDataType = (t == (int)ITEM) ? -1 : t;
            }
            else
            {
                pi.gameDataType = -1;
            }

            // itemFunction: ITEM_NO_FUNCTION (enum 0) means "no declared role"
            // — skip the tier rather than cluster every unclassified item.
            int fn          = (int)si.item->itemFunction;
            pi.itemFunction = (fn == 0) ? -1 : fn;

            pi.flagsMask = 0;
            if (gd)
            {
                BdataMap::const_iterator fc = gd->bdata.find("food crop");
                if (fc != gd->bdata.end() && fc->second) pi.flagsMask |= 1;
                BdataMap::const_iterator ti = gd->bdata.find("trade item");
                if (ti != gd->bdata.end() && ti->second) pi.flagsMask |= 2;
            }

            packItems.push_back(pi);
        }

        // The snapshot being replaced may still have a sweep in flight, queued
        // by another slot; its results would be dropped by generation anyway.
        AbortJobs(NULL, section, NULL);
        ResultCache::Insert(section, gridW, gridH, dim, packItems, identities);

        LogDebug("[StackSort] Cache miss: section=" + IntToStr((int)(intptr_t)section) + " '" + section->name + "'" +
                 " " + IntToStr(gridW) + "x" + IntToStr(gridH) + " " + IntToStr((int)packItems.size()) + " items");

        t.sectionKeys[t.numSections]         = section;
        t.sectionInv[t.numSections]          = inv;
        t.sectionNeedsCompute[t.numSections] = true;
        ++t.numSections;
        *outNeedsCompute = true;
    }
}

void SortWorker::SnapshotSections(int trackIdx, InventoryGUI* gui, bool* outNeedsCompute)
{
    GUITracking& t  = s_guiTracking[trackIdx];
    t.numSections   = 0;
    t.numTrackedInv = 0;

    Inventory* bodyInv = gui->_NV_getInventory();
    if (bodyInv)
    {
        ScanInventory(trackIdx, bodyInv, outNeedsCompute);
        t.trackedInv[t.numTrackedInv++] = bodyInv;
    }

    ContainerItem* backpack = gui->getBackpack();
    t.trackedBackpack       = backpack ? backpack->inventory : NULL;
    if (t.trackedBackpack)
    {
        ScanInventory(trackIdx, t.trackedBackpack, outNeedsCompute);
        if (t.numTrackedInv < 2) t.trackedInv[t.numTrackedInv++] = t.trackedBackpack;
    }
}

void SortWorker::EnqueueJob(int trackIdx)
{
    GUITracking& t = s_guiTracking[trackIdx];
    if (t.numSections == 0) return;

    // Every precomputed-result query reports a miss without mutation
    // tracking, so a sweep would be pure waste. Entries still exist (Insert
    // ran) for the revert capture.
    if (!s_mutationTrackingAvailable)
    {
        for (int i = 0; i < t.numSections; ++i)
            t.sectionNeedsCompute[i] = false;
        return;
    }

    int numEnqueued       = 0;
    bool sweepTableFull   = false;
    Packer::TargetDim dim = GetDim();

    // Only sections that missed the cache in the preceding scan get a sweep:
    // re-running a hit would overwrite its refined results with first-pass
    // ones. Entries are resolved before s_queueLock because Find takes s_lock
    // and s_queueLock is a leaf lock (see ResultCache.h). Their gridW/gridH/
    // generation are written only by the main thread, so reading them here
    // needs no resultLock.
    CachedSection* entries[MAX_SECTIONS];
    for (int i = 0; i < t.numSections; ++i)
    {
        entries[i]               = t.sectionNeedsCompute[i] ? ResultCache::Find(t.sectionKeys[i]) : NULL;
        t.sectionNeedsCompute[i] = false;
    }

    bool allocFailed = false;
    {
        ScopedLock guard(s_queueLock);
        for (int i = 0; i < t.numSections; ++i)
        {
            CachedSection* cs = entries[i];
            if (!cs) continue;

            int maxSlots = (dim == Packer::TARGET_H) ? (cs->gridH - 1) : (cs->gridW - 1);
            if (maxSlots < 1) continue;

            // Split [1..maxSlots] into one chunk per worker so first-pass
            // runs in parallel. Warm-start chain is chunk-local.
            int chunkCount = s_numWorkers;
            if (chunkCount > maxSlots) chunkCount = maxSlots;
            int baseSize  = maxSlots / chunkCount;
            int remainder = maxSlots % chunkCount;

            // Everything that can throw runs before the sweep opens: a chunk
            // counted into a sweep but never queued would hold it open forever.
            Job* chunk[MAX_WORKERS];
            int made = 0;
            try
            {
                s_jobQueue.reserve(s_jobQueue.size() + chunkCount);
                for (; made < chunkCount; ++made)
                    chunk[made] = new Job();
            }
            catch (...)
            {
                for (int k = 0; k < made; ++k)
                    delete chunk[k];
                allocFailed = true;
                break;
            }

            int sweepId   = 0;
            int sweepSlot = OpenSweepLocked(cs, dim, chunkCount, sweepId);
            if (sweepSlot < 0) sweepTableFull = true;

            int start = 1;
            for (int c = 0; c < chunkCount; ++c)
            {
                int sz  = baseSize + (c < remainder ? 1 : 0);
                int end = start + sz - 1;

                Job* job                = chunk[c];
                job->sourceGUI          = t.gui;
                job->section.key        = t.sectionKeys[i];
                job->section.generation = cs->generation;
                job->abortFlag          = 0;
                job->dim                = dim;
                job->target             = 0;
                job->targetStart        = start;
                job->targetEnd          = end;
                job->priority           = 0;
                job->sweepSlot          = sweepSlot;
                job->sweepId            = sweepId;

                s_jobQueue.push_back(job);
                ++numEnqueued;

                start = end + 1;
            }
        }
    }

    if (sweepTableFull) LogError("[StackSort] All sweep slots busy; this snapshot gets no sweep line");

    // Released before reporting a failure so jobs already queued get workers.
    bool semaphoreOk = numEnqueued == 0 || ReleaseSemaphore(s_workSemaphore, numEnqueued, NULL);

    // The caller's barrier arms a re-snapshot; sections already queued are
    // re-inserted then, which drops their jobs by generation.
    if (allocFailed) throw std::bad_alloc();

    if (!semaphoreOk)
        LogError("[StackSort] Work semaphore rejected " + IntToStr(numEnqueued) + " releases; jobs may stall");

    // Gated as a block: the summary walks the cache, which is pure waste below
    // LOG_LEVEL_VERBOSE.
    if (GetLogLevel() >= LOG_LEVEL_VERBOSE)
    {
        std::string msg = "[StackSort] Enqueued " + IntToStr(numEnqueued) + " job";
        if (numEnqueued > 1) msg += "s";
        msg += " (";
        bool first = true;
        for (int i = 0; i < t.numSections; ++i)
        {
            const CachedSection* cs = entries[i];
            if (!cs) continue;
            if (!first) msg += ", ";
            msg += IntToStr(cs->gridW) + "x" + IntToStr(cs->gridH) + "/" + IntToStr(cs->itemCount) + " items";
            first = false;
        }
        msg += ")";
        LogVerbose(msg);
    }
}

// Fixed capacity so recording under s_queueLock never allocates. One GUI's
// jobs span at most its sections plus those a backpack change or re-snapshot
// left behind; a key past the cap only loses its re-arm, never its abort.
struct SortWorker::AbortedSections
{
    SectionRef refs[MAX_GUIS * MAX_SECTIONS];
    int count;

    AbortedSections() : count(0) {}

    // Generations only grow, so the newest per key is the one that can still
    // be the entry's current snapshot.
    void Add(const SectionRef& r)
    {
        for (int i = 0; i < count; ++i)
        {
            if (refs[i].key != r.key) continue;
            if (r.generation > refs[i].generation) refs[i].generation = r.generation;
            return;
        }
        if (count < MAX_GUIS * MAX_SECTIONS) refs[count++] = r;
    }
};

// Main thread. Never throws: close and reclaim paths rely on it completing.
// Only the sweep log lines are best-effort.
void SortWorker::AbortJobs(InventoryGUI* gui, InventorySection* section, AbortedSections* outAborted)
{
    if (!gui && !section) return;

    std::vector<SweepStats> finished;
    ReserveSweepLog(finished);

    // Sections are marked after s_queueLock is released: marking takes s_lock
    // and resultLock, and s_queueLock is a leaf lock.
    AbortedSections refinementAborted;

    {
        ScopedLock guard(s_queueLock);

        // Abort queued jobs
        for (int i = (int)s_jobQueue.size() - 1; i >= 0; --i)
        {
            Job* dropped = s_jobQueue[i];
            if (gui && dropped->sourceGUI != gui) continue;
            if (section && dropped->section.key != section) continue;

            InterlockedExchange(&dropped->abortFlag, 1);
            if (outAborted) outAborted->Add(dropped->section);
            refinementAborted.Add(dropped->section);

            SweepStats done;
            if (CloseIfDoneLocked(dropped->sweepSlot, dropped->sweepId, 1, done)) KeepSweepLog(finished, done);

            delete dropped;
            s_jobQueue.erase(s_jobQueue.begin() + i);
        }

        // Abort active jobs — held under s_queueLock so the worker can't
        // delete the job between our load and dereference (see WorkerProc).
        for (int w = 0; w < s_numWorkers; ++w)
        {
            Job* active = (Job*)s_activeJobs[w];
            if (!active) continue;
            if (gui && active->sourceGUI != gui) continue;
            if (section && active->section.key != section) continue;

            InterlockedExchange(&active->abortFlag, 1);
            if (outAborted) outAborted->Add(active->section);
            refinementAborted.Add(active->section);
        }
    }

    // First-pass completion alone does not make an entry reusable: a dropped
    // refinement job leaves unrefined slots that nothing would re-enqueue.
    // First-pass jobs are marked too: an active chunk can be past its last
    // publish (entry complete) but not yet past EnqueueRefinementBatch's
    // in-lock abort check, so its candidates are lost with no refinement job
    // ever queued. On an entry a dropped chunk left incomplete the mark is
    // redundant; on a chunk that had no candidates it costs one re-sweep.
    // A newer generation means the snapshot was replaced and re-swept anyway.
    for (int a = 0; a < refinementAborted.count; ++a)
    {
        CachedSection* cs = ResultCache::Find(refinementAborted.refs[a].key);
        if (!cs || !cs->resultLockInit) continue;
        ScopedLock rl(cs->resultLock);
        if (cs->generation == refinementAborted.refs[a].generation) cs->refineAborted = 1;
    }

    try
    {
        for (size_t i = 0; i < finished.size(); ++i)
            LogSweep(finished[i]);

        // Main thread: flushed now so the sweep lines keep their order relative
        // to the direct log calls that follow.
        FlushWorkerLog();
    }
    catch (...)
    {
    }
}

void SortWorker::AbortJobsForGUI(InventoryGUI* gui, AbortedSections* outAborted)
{
    AbortJobs(gui, NULL, outAborted);
}

void SortWorker::RearmSlotsSharing(int exceptIdx, const AbortedSections& aborted)
{
    DWORD expired = GetTickCount() - (DWORD)StackSortSettingsStore::Get().mutationDebounceMs;

    for (int a = 0; a < aborted.count; ++a)
    {
        // Find takes s_lock, then resultLock: no lock is held here. The entry
        // cannot be erased under us, since eviction runs on this thread too.
        CachedSection* cs = ResultCache::Find(aborted.refs[a].key);
        if (!cs || !cs->resultLockInit) continue;

        // An active job flagged above can no longer publish (its in-lock check
        // sees abortFlag), so completeness read here is final for that
        // generation. A newer generation means another snapshot replaced it
        // and that snapshot's own slot enqueued its sweep.
        bool orphaned;
        {
            ScopedLock rl(cs->resultLock);
            orphaned = cs->generation == aborted.refs[a].generation &&
                       (cs->numSlotsDone != (int)cs->results.size() || cs->refineAborted);
        }
        if (!orphaned) continue;

        for (int i = 0; i < MAX_GUIS; ++i)
        {
            if (i == exceptIdx) continue;
            GUITracking& t = s_guiTracking[i];
            if (!t.gui) continue;

            for (int j = 0; j < t.numSections; ++j)
            {
                if (t.sectionKeys[j] != aborted.refs[a].key) continue;
                // A pending re-snapshot keeps its own debounce.
                if (!t.mutationPending)
                {
                    t.mutationPending  = true;
                    t.lastMutationTick = expired;
                }
                break;
            }
        }
    }
}

DWORD WINAPI SortWorker::WorkerProc(LPVOID param)
{
    int workerIdx = (int)(intptr_t)param;

    while (s_shutdown == 0)
    {
        WaitForSingleObject(s_workSemaphore, INFINITE);
        if (s_shutdown != 0) break;

        Job* job = NULL;
        {
            ScopedLock guard(s_queueLock);
            if (!s_jobQueue.empty())
            {
                // Priority-aware selection: lowest priority value wins, FIFO within same priority
                int bestIdx = 0;
                for (size_t i = 1; i < s_jobQueue.size(); ++i)
                {
                    if (s_jobQueue[i]->priority < s_jobQueue[bestIdx]->priority) bestIdx = (int)i;
                }
                job = s_jobQueue[bestIdx];
                s_jobQueue.erase(s_jobQueue.begin() + bestIdx);
                // Publish to s_activeJobs under the same lock so eviction's
                // IsSectionActive check always sees either the queue or the
                // active slot — never a window where the job is untracked.
                s_activeJobs[workerIdx] = job;
            }
        }

        if (!job) continue;

        // Exception barrier: an escaping exception (realistically only
        // std::bad_alloc) would std::terminate the game. /EHsc unwinds C++
        // exceptions only, so every ScopedLock inside ExecuteJob releases its
        // lock (resultLock included) before we get here; SEH faults are
        // deliberately not caught. The entry may hold a half-assigned slot, so
        // it is invalidated while it still belongs to this job's generation.
        bool failed = false;
        try
        {
            ExecuteJob(job, workerIdx);
        }
        catch (...)
        {
            failed = true;
        }
        if (failed)
        {
            InterlockedExchange(&job->abortFlag, 1);
            ResultCache::InvalidateIfGeneration(job->section.key, job->section.generation);
            if (InterlockedExchange(&s_workerExceptionLogged, 1) == 0)
            {
                try
                {
                    WorkerLogError("[StackSort] Worker job threw (out of memory?); job dropped, sorting falls back");
                }
                catch (...)
                {
                }
            }
        }

        // Close and log the sweep BEFORE releasing the active slot: LogSweep
        // takes cs->resultLock, and only s_activeJobs keeps IsSectionActive
        // true, which is what stops EvictIfNeeded from erasing the
        // CachedSection (destroying that very lock) underneath us.
        // CloseIfDoneLocked cannot throw, so the sweep is closed even if the
        // log line then fails to allocate.
        try
        {
            // A job dropped by a generation check (another snapshot or an
            // Invalidate replaced its entry) counts as aborted too: the sweep's
            // digest would otherwise describe a snapshot it did not compute.
            // Read before s_queueLock, which is a leaf (Find takes s_lock), so
            // a bump can land after this read; LogSweep re-checks under
            // resultLock before it trusts the digest.
            bool dropped = job->abortFlag != 0;
            if (!dropped)
            {
                CachedSection* cs = ResultCache::Find(job->section.key);
                dropped           = !cs || cs->generation != job->section.generation;
            }

            SweepStats finished;
            bool sweepDone = false;
            {
                ScopedLock guard(s_queueLock);
                sweepDone = CloseIfDoneLocked(job->sweepSlot, job->sweepId, dropped ? 1 : 0, finished);
            }

            // Never log while holding a lock. LogSweep only queues the line.
            if (sweepDone) LogSweep(finished);
        }
        catch (...)
        {
        }

        // Null the pointer under s_queueLock so AbortJobs can't
        // dereference a freed job between our NULL and delete.
        {
            ScopedLock guard(s_queueLock);
            s_activeJobs[workerIdx] = NULL;
        }
        delete job;
    }

    return 0;
}

bool SortWorker::EnqueueRefinementBatch(const Job* parent, CachedSection* cs, Job** jobs, int count)
{
    if (count <= 0) return false;

    bool queued = false;
    {
        ScopedLock guard(s_queueLock);
        // Re-checked under s_queueLock: an abort that ran after the caller's
        // check purged the queue before these jobs existed, so queuing them
        // now would leave un-aborted orphans on a stale snapshot. A generation
        // bump from Insert is not serialized by this lock; a job that slips
        // past is dropped by ExecuteJob's in-lock generation check. cs is
        // alive: parent is still in s_activeJobs, which blocks eviction.
        bool roomReserved = false;
        try
        {
            // Reserved before the sweep counts the jobs, so no push_back below
            // can throw and leave counted-but-unqueued jobs holding the sweep
            // open forever.
            s_jobQueue.reserve(s_jobQueue.size() + count);
            roomReserved = true;
        }
        catch (...)
        {
        }
        if (roomReserved && parent->abortFlag == 0 && cs->generation == parent->section.generation)
        {
            // The parent first-pass job has not been decremented yet, so the
            // sweep cannot close between these jobs being counted and queued.
            AddRefineLocked(jobs[0]->sweepSlot, jobs[0]->sweepId, count);
            for (int i = 0; i < count; ++i)
                s_jobQueue.push_back(jobs[i]);
            queued = true;
        }
    }

    if (!queued)
    {
        for (int i = 0; i < count; ++i)
            delete jobs[i];

        // A failed reserve drops the candidates with no abort to mark the
        // entry, which would then be reused unrefined. On an abort AbortJobs
        // has marked it already (parent was active); a newer generation is
        // re-swept anyway. Taken after s_queueLock is released (leaf lock);
        // only ever raises the flag, so a stale write cannot validate state.
        ScopedLock rl(cs->resultLock);
        if (cs->generation == parent->section.generation) cs->refineAborted = 1;
        return false;
    }

    if (!ReleaseSemaphore(s_workSemaphore, count, NULL))
        WorkerLogError("[StackSort] Work semaphore rejected " + IntToStr(count) +
                       " refinement releases; jobs may stall");
    return true;
}

void SortWorker::Init()
{
    const StackSortSettings& cfg = StackSortSettingsStore::Get();

    // Clamped here rather than trusting the setting's declared range: this is
    // the bound on s_threads / s_activeJobs, and the range lives in another file.
    s_numWorkers = cfg.numWorkers;
    if (s_numWorkers > MAX_WORKERS) s_numWorkers = MAX_WORKERS;
    if (s_numWorkers < 1) s_numWorkers = 1;
    InterlockedExchange(&s_currentDim, (LONG)(cfg.defaultDim == 1 ? Packer::TARGET_W : Packer::TARGET_H));

    QueryPerformanceFrequency(&s_qpcFreq);
    for (int i = 0; i < MAX_GUIS * MAX_SECTIONS; ++i)
        s_sweeps[i].active = false;

    InitializeCriticalSection(&s_queueLock);
    InitializeCriticalSection(&s_logLock);
    s_logQueue.clear();
    InterlockedExchange(&s_logReady, 1);
    s_workSemaphore = CreateSemaphore(NULL, 0, MAX_QUEUED_JOBS, NULL);
    s_shutdown      = 0;

    for (int i = 0; i < s_numWorkers; ++i)
    {
        s_activeJobs[i] = NULL;
        s_threads[i]    = CreateThread(NULL, 0, WorkerProc, (LPVOID)(intptr_t)i, 0, NULL);
        if (!s_threads[i]) LogError("[StackSort] Failed to create worker thread " + IntToStr(i));
    }
    LogInfo("[StackSort] " + IntToStr(s_numWorkers) + " worker threads started");
}

void SortWorker::Shutdown()
{
    InterlockedExchange(&s_shutdown, 1);
    if (s_workSemaphore) ReleaseSemaphore(s_workSemaphore, s_numWorkers, NULL);

    for (int i = 0; i < s_numWorkers; ++i)
    {
        if (s_threads[i])
        {
            WaitForSingleObject(s_threads[i], 2000);
            CloseHandle(s_threads[i]);
            s_threads[i] = NULL;
        }
    }

    if (s_workSemaphore)
    {
        CloseHandle(s_workSemaphore);
        s_workSemaphore = NULL;
    }

    // Clean up any remaining queued jobs
    {
        ScopedLock guard(s_queueLock);
        for (size_t i = 0; i < s_jobQueue.size(); ++i)
            delete s_jobQueue[i];
        s_jobQueue.clear();
    }

    // Workers have stopped: this picks up their last lines.
    FlushWorkerLog();
    InterlockedExchange(&s_logReady, 0);

    DeleteCriticalSection(&s_queueLock);
    DeleteCriticalSection(&s_logLock);
}

void SortWorker::OnWorldReset()
{
    // Workers hold plain snapshots and cache entries. Abort before invalidating;
    // retain cache node storage because an active worker may still hold a node.
    {
        ScopedLock queue(s_queueLock);
        for (size_t i = 0; i < s_jobQueue.size(); ++i) delete s_jobQueue[i];
        s_jobQueue.clear();
        for (int w = 0; w < s_numWorkers; ++w)
        {
            Job* active = (Job*)s_activeJobs[w];
            if (active) InterlockedExchange(&active->abortFlag, 1);
        }
        for (int i = 0; i < MAX_GUIS * MAX_SECTIONS; ++i) s_sweeps[i].active = false;
    }
    ResultCache::InvalidateAll();
    for (int i = 0; i < MAX_GUIS; ++i) FreeTracking(i);
}

// Appends the section of every queued or active job. Collected under
// s_queueLock alone, before EvictIfNeeded takes s_lock, so the two locks never
// nest (see ResultCache.h).
static void CollectJobSections(std::vector<InventorySection*>& out)
{
    ScopedLock guard(s_queueLock);
    for (size_t i = 0; i < s_jobQueue.size(); ++i)
        out.push_back(s_jobQueue[i]->section.key);
    for (int w = 0; w < s_numWorkers; ++w)
    {
        Job* j = (Job*)s_activeJobs[w];
        if (j) out.push_back(j->section.key);
    }
}

// Drops the revert capture of slot idx's sections (only those scanned from
// onlyInv when it is non-NULL) unless another live slot lists the section. A
// capture lives for one open: once no live GUI shows the section, nothing
// routes its mutations here, so a later open must recapture rather than revert
// to a layout an NPC, a save load (section address reuse) or another GUI may
// have replaced since. Reads keys and stamps only, never a gui pointer.
// Main thread only. Never throws.
static void ClearUnsharedCaptures(int idx, Inventory* onlyInv)
{
    const GUITracking& t = s_guiTracking[idx];
    DWORD now            = GetTickCount();
    for (int j = 0; j < t.numSections; ++j)
    {
        if (onlyInv && t.sectionInv[j] != onlyInv) continue;

        InventorySection* key = t.sectionKeys[j];
        bool shared           = false;
        for (int o = 0; o < MAX_GUIS && !shared; ++o)
        {
            const GUITracking& other = s_guiTracking[o];
            if (o == idx || !other.gui || !IsSlotLive(other, now)) continue;
            for (int k = 0; k < other.numSections; ++k)
            {
                if (other.sectionKeys[k] == key)
                {
                    shared = true;
                    break;
                }
            }
        }
        if (!shared) ResultCache::ClearOriginalPositions(key);
    }
}

void SortWorker::OnInventoryOpened(InventoryGUI* gui)
{
    int existing = FindTracking(gui);
    if (existing >= 0)
    {
        StampSlot(s_guiTracking[existing]);
        return;
    }

    // Exception barrier: called from _NV_update, so nothing may unwind into
    // the game's frame.
    int idx = -1;
    try
    {
        idx = AllocTracking(gui);
        if (idx < 0) return;

        StackSort_InitRotateAPI();
        bool needsCompute = false;
        SnapshotSections(idx, gui, &needsCompute);

        // A fresh slot is a fresh open (StackSortUI's state starts over too),
        // so its first click must capture the layout as it is now.
        ClearUnsharedCaptures(idx, NULL);

        if (s_guiTracking[idx].numSections == 0)
            return; // no eligible sections (keep tracking to prevent per-frame retry)

        LogInfo("[StackSort] Inventory opened (cache: " + IntToStr(ResultCache::Size()) + " entries)");

        // Evict least recently used entries if needed. Protected: every open
        // GUI's sections, plus every section a queued or active job still uses.
        std::vector<InventorySection*> refs;
        for (int i = 0; i < MAX_GUIS; ++i)
        {
            if (!s_guiTracking[i].gui) continue;
            for (int j = 0; j < s_guiTracking[i].numSections; ++j)
                refs.push_back(s_guiTracking[i].sectionKeys[j]);
        }
        CollectJobSections(refs);
        std::sort(refs.begin(), refs.end());
        refs.erase(std::unique(refs.begin(), refs.end()), refs.end());
        ResultCache::EvictIfNeeded(refs.empty() ? NULL : &refs[0], (int)refs.size(), MAX_CACHED_SECTIONS);

        if (needsCompute) EnqueueJob(idx);
    }
    catch (...)
    {
        ReportMainThreadException("Inventory open");
        if (idx >= 0) MarkTrackingMutated(idx);
    }
}

// Never throws: called from hook_show and the stale-context reset.
void SortWorker::OnInventoryClosed(InventoryGUI* gui)
{
    int idx = FindTracking(gui);
    if (idx < 0) return;

    AbortedSections aborted;
    AbortJobsForGUI(gui, &aborted);
    ClearUnsharedCaptures(idx, NULL);

    s_guiTracking[idx].mutationPending = false;
    FreeTracking(idx);
    RearmSlotsSharing(idx, aborted);

    try
    {
        LogInfo("[StackSort] Inventory closed (cache: " + IntToStr(ResultCache::Size()) + " entries)");
    }
    catch (...)
    {
    }
}

// Does not dereference t.gui: also safe for a slot whose GUI may be dead.
// Never throws (AbortJobsForGUI and Invalidate don't), so the exception
// barriers use it as their recovery: it leaves a possibly half-rebuilt slot
// with its jobs aborted, its listed entries invalidated and a debounced
// re-snapshot armed.
void SortWorker::MarkTrackingMutated(int idx)
{
    GUITracking& t = s_guiTracking[idx];

    // Abort first so in-flight jobs stop spending budget. Invalidate then
    // bumps generation under each entry's resultLock, which is what stops a
    // job already past its abort check from publishing or re-validating.
    AbortJobsForGUI(t.gui);

    for (int i = 0; i < t.numSections; ++i)
        ResultCache::Invalidate(t.sectionKeys[i]);

    // Arm debounced re-snapshot + re-enqueue
    t.mutationPending  = true;
    t.lastMutationTick = GetTickCount();
}

// Narrow variant for a hand edit of one tracked inventory: sections of the
// other inventory keep their entries and jobs. The re-snapshot is armed even
// when no listed section belongs to inv, since an inventory that had no
// eligible section may have gained its first one.
void SortWorker::MarkInventoryMutated(int idx, Inventory* inv)
{
    GUITracking& t = s_guiTracking[idx];

    for (int i = 0; i < t.numSections; ++i)
    {
        if (t.sectionInv[i] != inv) continue;
        // Abort before Invalidate, as in MarkTrackingMutated.
        AbortJobs(NULL, t.sectionKeys[i], NULL);
        ResultCache::Invalidate(t.sectionKeys[i]);
    }

    t.mutationPending  = true;
    t.lastMutationTick = GetTickCount();
}

void SortWorker::OnMutation(InventoryGUI* gui)
{
    int idx = FindTracking(gui);
    if (idx < 0) return;
    MarkTrackingMutated(idx);
}

void SortWorker::OnInventoryMutated(Inventory* inv)
{
    if (!inv) return;

    // Only inventories some GUI tracks: calling into an untracked inventory
    // from its callback (possibly mid-teardown) is not safe, and reopening an
    // untracked section re-verifies identities anyway (IsReusable).
    for (int i = 0; i < MAX_GUIS; ++i)
    {
        GUITracking& t = s_guiTracking[i];
        if (!t.gui) continue;

        bool tracks = false;
        for (int ti = 0; ti < t.numTrackedInv; ++ti)
        {
            if (t.trackedInv[ti] == inv) tracks = true;
        }
        if (!tracks) continue;

        // Exception barrier for the add/remove item callbacks. Everything
        // below is no-throw today; the catch keeps a future allocation here
        // from unwinding into the game's callback frame.
        try
        {
            // A hand edit makes the revert capture describe a layout the
            // player has since replaced; the next first click recaptures.
            for (int j = 0; j < t.numSections; ++j)
            {
                if (t.sectionInv[j] == inv) ResultCache::ClearOriginalPositions(t.sectionKeys[j]);
            }

            MarkInventoryMutated(i, inv);
        }
        catch (...)
        {
            ReportMainThreadException("Inventory mutation");
            MarkTrackingMutated(i);
        }
    }
}

void SortWorker::OnTrackedGUIUpdate(InventoryGUI* gui)
{
    int idx = FindTracking(gui);
    if (idx < 0) return;

    GUITracking& t = s_guiTracking[idx];
    if ((DWORD)(s_pollCount - t.lastSeenPoll) <= (DWORD)(s_pollCount - s_frameStartPoll))
    {
        s_frameStartPoll       = s_pollCount;
        s_resnapshotsThisFrame = 0;
    }
    StampSlot(t);

    // A backpack equipped, swapped or dropped while the GUI stays open fires
    // no callback on the backpack inventories themselves, so without this
    // trackedInv keeps routing the old backpack's mutations here and ignores
    // the new one's. Not held to the frame budget: until it runs, mutations
    // are routed by the wrong inventory.
    ContainerItem* backpack = gui->getBackpack();
    Inventory* current      = backpack ? backpack->inventory : NULL;
    if (current != t.trackedBackpack)
    {
        ++s_resnapshotsThisFrame;

        // Exception barrier: called from _NV_update.
        try
        {
            LogInfo("[StackSort] Backpack changed, re-snapshotting");

            // In place rather than through OnInventoryClosed, which would drop
            // the UI state and the cycling target. Sections whose sweep
            // finished are reused by the scan; aborted, incomplete ones are
            // re-snapshotted. Captures do not survive the move in either
            // direction: the old backpack's mutations stop reaching this slot,
            // and the new one's layout may have changed while no GUI tracked it.
            //
            // Re-armed only after this slot re-snapshots: sections it still
            // lists were re-inserted (generation moved) or reused complete,
            // so only those it dropped can leave another slot's view orphaned.
            AbortedSections aborted;
            AbortJobsForGUI(gui, &aborted);
            if (t.trackedBackpack) ClearUnsharedCaptures(idx, t.trackedBackpack);
            bool needsCompute = false;
            SnapshotSections(idx, gui, &needsCompute);
            if (current) ClearUnsharedCaptures(idx, current);
            t.mutationPending = false; // this snapshot is current
            if (needsCompute) EnqueueJob(idx);
            RearmSlotsSharing(idx, aborted);
        }
        catch (...)
        {
            ReportMainThreadException("Backpack re-snapshot");
            MarkTrackingMutated(idx);
        }
        return;
    }

    // The debounced re-snapshot armed by MarkTrackingMutated, MarkSectionStale
    // or ToggleDim runs here, never from a loop over all slots: only inside
    // its own update is a GUI provably alive. A GUI destroyed without
    // show(false) therefore keeps its re-snapshot armed, undone, until the
    // slot is reclaimed. The budget spreads a burst (16 GUIs mutated at once,
    // or a dim toggle) over frames, since ScanInventory walks every item and
    // EnqueueJob takes s_queueLock.
    if (!t.mutationPending) return;
    if (s_resnapshotsThisFrame >= StackSortSettingsStore::Get().resnapshotsPerFrame) return;
    DWORD elapsed = GetTickCount() - t.lastMutationTick;
    if (elapsed < (DWORD)StackSortSettingsStore::Get().mutationDebounceMs) return;

    ++s_resnapshotsThisFrame;

    // SnapshotSections rather than a bare rescan: it rebuilds trackedInv and
    // trackedBackpack too, which mutation routing depends on.
    try
    {
        bool needsCompute = false;
        SnapshotSections(idx, gui, &needsCompute);
        if (needsCompute) EnqueueJob(idx);
        t.mutationPending = false;
    }
    catch (...)
    {
        // Re-armed with a fresh tick, so a persistent failure retries once
        // per debounce window rather than every frame.
        ReportMainThreadException("Re-snapshot");
        MarkTrackingMutated(idx);
    }
}

// Never throws.
void SortWorker::ReclaimStaleTracking()
{
    DWORD now = GetTickCount();

    for (int i = 0; i < MAX_GUIS; ++i)
    {
        GUITracking& t = s_guiTracking[i];
        if (!t.gui) continue;
        if (TickAge(now, t.lastSeenTick) <= TRACKING_RECLAIM_MS) continue;
        if ((DWORD)(s_pollCount - t.lastSeenPoll) <= TRACKING_RECLAIM_POLLS) continue;

        // The GUI was destroyed without show(false), or stopped updating for
        // long enough that a fresh slot costs nothing. Neither call
        // dereferences t.gui.
        AbortedSections aborted;
        AbortJobsForGUI(t.gui, &aborted);
        ClearUnsharedCaptures(i, NULL);
        FreeTracking(i);
        RearmSlotsSharing(i, aborted);
        try
        {
            LogInfo("[StackSort] Reclaimed tracking slot " + IntToStr(i) + " (GUI stopped updating)");
        }
        catch (...)
        {
        }
    }
}

void SortWorker::PollWorkerStart()
{
    ++s_pollCount;

    // Exception barrier: called from _NV_update. Only the log flush can throw.
    try
    {
        FlushWorkerLog();
    }
    catch (...)
    {
        ReportMainThreadException("Worker log flush");
    }
    ReclaimStaleTracking();
}

void SortWorker::SetMutationTrackingAvailable(bool available)
{
    s_mutationTrackingAvailable = available;
}

bool SortWorker::IsMutationTrackingAvailable()
{
    return s_mutationTrackingAvailable;
}

// Aborts by section, not by GUI, so the slot's other sections keep their
// jobs and results. Abort before Invalidate, as MarkTrackingMutated does.
void SortWorker::MarkSectionStale(InventorySection* section)
{
    AbortJobs(NULL, section, NULL);
    ResultCache::Invalidate(section);

    DWORD now = GetTickCount();
    for (int i = 0; i < MAX_GUIS; ++i)
    {
        GUITracking& t = s_guiTracking[i];
        if (!t.gui) continue;
        for (int j = 0; j < t.numSections; ++j)
        {
            if (t.sectionKeys[j] != section) continue;
            t.mutationPending  = true;
            t.lastMutationTick = now;
            break;
        }
    }
}

void SortWorker::OnRotationAPIAvailable()
{
    // Capability changed from exact orientation to optional rotation. Abort
    // old plain-data jobs and reject all old generations, including closed
    // cached sections. No native GUI/item is dereferenced here. Preserve undo.
    const DWORD expired=GetTickCount()-(DWORD)StackSortSettingsStore::Get().mutationDebounceMs;
    for(int i=0;i<MAX_GUIS;++i)
    {
        GUITracking& t=s_guiTracking[i];
        if(!t.gui) continue;
        AbortJobsForGUI(t.gui);
        t.mutationPending=true;t.lastMutationTick=expired;
    }
    ResultCache::InvalidateForRotation();
}

bool SortWorker::GetResult(InventorySection* section, int target, Packer::Result& outResult)
{
    if (!s_mutationTrackingAvailable) return false;

    CachedSection* cs = ResultCache::Find(section);
    if (!cs) return false;

    ScopedLock rl(cs->resultLock);
    if (cs->valid == 0) return false;
    if (target < 1 || target > ComputedLimitLocked(cs)) return false;
    if (!cs->slotDone[target - 1]) return false;

    outResult = cs->results[target - 1];
    return true;
}

// Caller holds cs->resultLock. Slot index (target - 1) of the best-scoring
// published allPlaced result whose lerSide reaches target, or -1.
static int FindBestSlotLocked(const CachedSection* cs, int target)
{
    if (cs->valid == 0) return -1;

    int computed = ComputedLimitLocked(cs);
    if (computed == 0) return -1;

    int best            = -1;
    long long bestScore = 0;
    int scanLimit       = (target < computed) ? target : computed;
    for (int t = 1; t <= scanLimit; ++t)
    {
        if (!cs->slotDone[t - 1]) continue;
        const Packer::Result& r = cs->results[t - 1];
        if (!r.allPlaced) continue;
        int lerSide = (cs->dim == Packer::TARGET_H) ? r.lerHeight : r.lerWidth;
        if (lerSide < target) continue;
        if (best < 0 || r.score > bestScore)
        {
            bestScore = r.score;
            best      = t - 1;
        }
    }
    return best;
}

bool SortWorker::GetBestResult(InventorySection* section, int target, Packer::Result& outResult, int* outSourceTarget)
{
    if (!s_mutationTrackingAvailable) return false;

    CachedSection* cs = ResultCache::Find(section);
    if (!cs) return false;

    int best = -1;
    {
        ScopedLock rl(cs->resultLock);
        best = FindBestSlotLocked(cs, target);
        if (best >= 0) outResult = cs->results[best];
    }

    if (outSourceTarget) *outSourceTarget = best + 1;
    return best >= 0;
}

bool SortWorker::GetApplySnapshot(InventorySection* section, int target, ApplySnapshot& out)
{
    if (!s_mutationTrackingAvailable) return false;

    CachedSection* cs = ResultCache::Find(section);
    if (!cs) return false;

    // Read before the lock: it walks live game items. applyLayout removes and
    // re-adds every returned pointer, so each must be an item this section
    // holds right now, with the dims the result was packed for (up to
    // orientation when KenshiRotate can reconcile it). Cheap at N <= 255.
    std::vector<ItemIdentity> live;
    ReadLiveIdentities(section, live);
    std::sort(live.begin(), live.end());

    // One hold for result, packItems and itemPtrs: only Insert replaces the
    // latter two, and it clears results[] under the same lock, so a result
    // found here was packed from exactly this packItems.
    int best   = -1;
    bool match = false;
    {
        ScopedLock rl(cs->resultLock);
        best = FindBestSlotLocked(cs, target);
        if (best >= 0)
        {
            match = cs->sortedIdentity == live;
            if (match)
            {
                out.result       = cs->results[best];
                out.sourceTarget = best + 1;
                out.packItems    = cs->packItems;
                out.itemPtrs     = cs->itemPtrs;
            }
        }
    }

    if (best < 0) return false;
    if (!match)
    {
        LogInfo("[StackSort] Cached snapshot of '" + section->name +
                "' no longer matches its items; re-snapshotting, sorting synchronously");
        MarkSectionStale(section);
    }
    return match;
}

bool SortWorker::HasContext(InventoryGUI* gui)
{
    return FindTracking(gui) >= 0;
}

bool SortWorker::IsTrackedInventory(Inventory* inv)
{
    for (int i = 0; i < MAX_GUIS; ++i)
    {
        const GUITracking& t = s_guiTracking[i];
        if (!t.gui) continue;
        for (int ti = 0; ti < t.numTrackedInv; ++ti)
        {
            if (t.trackedInv[ti] == inv) return true;
        }
    }
    return false;
}

bool SortWorker::ContextMatchesInventory(InventoryGUI* gui)
{
    int idx = FindTracking(gui);
    if (idx < 0) return false;

    Inventory* currentInv = gui->_NV_getInventory();
    if (!currentInv) return false;

    const GUITracking& t = s_guiTracking[idx];
    for (int ti = 0; ti < t.numTrackedInv; ++ti)
    {
        if (t.trackedInv[ti] == currentInv) return true;
    }
    return false;
}

InventoryGUI* SortWorker::FindContextGUIForInventory(Inventory* inv)
{
    for (int i = 0; i < MAX_GUIS; ++i)
    {
        const GUITracking& t = s_guiTracking[i];
        if (!t.gui) continue;
        for (int ti = 0; ti < t.numTrackedInv; ++ti)
        {
            if (t.trackedInv[ti] == inv) return t.gui;
        }
    }
    return NULL;
}

bool SortWorker::IsApplying()
{
    return s_applying;
}

void SortWorker::SetApplying(bool v)
{
    s_applying = v;
}

bool SortWorker::IsRunning()
{
    for (int i = 0; i < s_numWorkers; ++i)
    {
        if (s_activeJobs[i] != NULL) return true;
    }
    return false;
}

bool SortWorker::IsSectionActive(InventorySection* section)
{
    ScopedLock guard(s_queueLock);

    for (size_t i = 0; i < s_jobQueue.size(); ++i)
    {
        if (s_jobQueue[i]->section.key == section) return true;
    }
    for (int w = 0; w < s_numWorkers; ++w)
    {
        Job* j = (Job*)s_activeJobs[w];
        if (j && j->section.key == section) return true;
    }
    return false;
}

Packer::TargetDim SortWorker::GetDim()
{
    return (Packer::TargetDim)s_currentDim;
}

void SortWorker::ToggleDim()
{
    Packer::TargetDim newDim;
    std::vector<SweepStats> finished;
    ReserveSweepLog(finished);
    {
        ScopedLock guard(s_queueLock);

        newDim = (GetDim() == Packer::TARGET_H) ? Packer::TARGET_W : Packer::TARGET_H;
        InterlockedExchange(&s_currentDim, (LONG)newDim);

        // Abort all queued jobs
        for (int i = (int)s_jobQueue.size() - 1; i >= 0; --i)
        {
            Job* dropped = s_jobQueue[i];
            InterlockedExchange(&dropped->abortFlag, 1);

            SweepStats done;
            if (CloseIfDoneLocked(dropped->sweepSlot, dropped->sweepId, 1, done)) KeepSweepLog(finished, done);

            delete dropped;
            s_jobQueue.erase(s_jobQueue.begin() + i);
        }

        // Abort all active jobs (worker will notice abortFlag and skip writes)
        for (int w = 0; w < s_numWorkers; ++w)
        {
            Job* active = (Job*)s_activeJobs[w];
            if (active) InterlockedExchange(&active->abortFlag, 1);
        }
    }

    // Aborted sweeps first, so the log reads "abort, then the new sweep".
    // Best-effort: ToggleDim runs from a MyGUI click, so nothing may escape.
    try
    {
        for (size_t i = 0; i < finished.size(); ++i)
            LogSweep(finished[i]);
        FlushWorkerLog();
    }
    catch (...)
    {
    }

    // Invalidate every open GUI's sections and arm an already-expired
    // debounce: each slot re-snapshots under the new dim from its own next
    // _NV_update (OnTrackedGUIUpdate), one frame later. Not inline: a click
    // proves no other GUI alive, and one destroyed without show(false) would
    // be dereferenced here. Never throws.
    DWORD expired = GetTickCount() - (DWORD)StackSortSettingsStore::Get().mutationDebounceMs;
    for (int i = 0; i < MAX_GUIS; ++i)
    {
        GUITracking& t = s_guiTracking[i];
        if (!t.gui) continue;
        for (int j = 0; j < t.numSections; ++j)
            ResultCache::Invalidate(t.sectionKeys[j]);
        t.mutationPending  = true;
        t.lastMutationTick = expired;
    }

    try
    {
        LogInfo(std::string("[StackSort] Target dim toggled to ") + (newDim == Packer::TARGET_H ? "H" : "W"));
    }
    catch (...)
    {
    }
}

int SortWorker::GetMaxTarget(InventorySection* section)
{
    CachedSection* cs = ResultCache::Find(section);
    if (!cs) return 0;

    // Snapshot from before a dim toggle, awaiting its owner's re-snapshot:
    // its slots and LER sides are measured along the other axis. 0 makes the
    // caller fall back to the grid side under the active dim. cs->dim is
    // written by the main thread only.
    if (cs->dim != GetDim()) return 0;

    int maxSlots   = (cs->dim == Packer::TARGET_H) ? (cs->gridH - 1) : (cs->gridW - 1);
    int maxLerSide = 0;
    {
        ScopedLock rl(cs->resultLock);
        int computed = ComputedLimitLocked(cs);
        if (computed == 0) return maxSlots;

        for (int t = 1; t <= computed; ++t)
        {
            if (!cs->slotDone[t - 1]) continue;
            const Packer::Result& r = cs->results[t - 1];
            if (!r.allPlaced) continue;
            int lerSide = (cs->dim == Packer::TARGET_H) ? r.lerHeight : r.lerWidth;
            if (lerSide > maxLerSide) maxLerSide = lerSide;
        }
    }

    return maxLerSide > 0 ? maxLerSide : maxSlots;
}
