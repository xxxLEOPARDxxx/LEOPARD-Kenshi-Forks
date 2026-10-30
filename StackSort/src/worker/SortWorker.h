#pragma once

#include <Windows.h>
#include <string>
#include <vector>

#include "Packer.h"

class Inventory;
class InventoryGUI;
class InventorySection;
class Item;
struct CachedSection;
struct ItemIdentity;

class SortWorker
{
  public:
    static void Init();
    static void Shutdown();
    static void OnWorldReset(); // cancel by opaque keys, no native dereferences

    // Inventory opened: allocate GUI tracking, check cache for reusable
    // results, snapshot new sections into cache, enqueue job. Drops revert
    // captures no other live GUI shares, so the first click recaptures.
    // Exception barrier: nothing thrown escapes (a failure arms a re-snapshot).
    static void OnInventoryOpened(InventoryGUI* gui);

    // Inventory closed: abort queued/active jobs for this GUI, drop revert
    // captures no other live GUI shares, free tracking. Cache entries are NOT
    // removed (persist for reuse on reopen). Never throws.
    static void OnInventoryClosed(InventoryGUI* gui);

    // Item added/removed in a tracked inventory: invalidate cache entries,
    // abort jobs, arm debounced re-snapshot + re-enqueue.
    static void OnMutation(InventoryGUI* gui);

    // Section add/remove callback for inv, outside an apply. For EVERY
    // tracking slot whose tracked inventories include inv (an inventory can be
    // tracked by two GUIs at once, e.g. a dropped backpack open as a container
    // while its old owner's GUI still lists it): drops the revert capture of
    // the slot's sections scanned from inv, then does what OnMutation does.
    // Untracked inventories are ignored; their entries are re-verified on
    // reopen. Never calls into inv. Main thread only. Exception barrier for
    // the item callbacks: nothing thrown escapes.
    static void OnInventoryMutated(Inventory* inv);

    // Called from _NV_update every frame for a visible, tracked, top-level,
    // non-trader GUI (gui is thisptr, so provably alive). Stamps the slot as
    // alive (tick and poll count; see IsSlotLive in SortWorker.cpp),
    // re-snapshots in place when the GUI's equipped backpack inventory differs
    // from the one tracking recorded, and otherwise runs this slot's debounced
    // re-snapshot + re-enqueue once due, within the per-frame budget. A slot's
    // gui is dereferenced only from that GUI's own update (here and
    // OnInventoryOpened). Main thread only. Exception barrier: a failure arms
    // a re-snapshot.
    static void OnTrackedGUIUpdate(InventoryGUI* gui);

    // Called from _NV_update each frame, after OnTrackedGUIUpdate. Advances
    // the poll count, flushes the worker log, and reclaims slots whose GUI
    // stopped updating without show(false). Never dereferences a slot's gui.
    // Exception barrier.
    static void PollWorkerStart();

    // False when a mutation callback hook failed to install. Nothing would
    // then invalidate cached snapshots, so every precomputed-result query
    // reports a miss and the apply path takes the synchronous fallback.
    // Set from startPlugin before any worker exists; read on the main thread.
    static void SetMutationTrackingAvailable(bool available);
    // Without mutation tracking a revert capture is never dropped on a hand
    // edit either, so the UI takes no capture and reverts nothing.
    static bool IsMutationTrackingAvailable();

    // Copy precomputed result for a specific section + target into outResult.
    // Reads the active dim from s_currentDim. Returns true if found.
    static bool GetResult(InventorySection* section, int target, Packer::Result& outResult);

    // Scan computed results for this section and copy the one with the best
    // score whose lerSide >= target into outResult (lerSide is lerHeight in
    // H-mode, lerWidth in W-mode). If outSourceTarget is non-NULL, writes
    // which target value produced the winning result.
    // Returns true if a valid result was found.
    static bool GetBestResult(InventorySection* section, int target, Packer::Result& outResult,
                              int* outSourceTarget = NULL);

    // Everything an apply needs from one cache entry, copied in a single
    // resultLock hold: packItems is exactly the vector the worker packed
    // result from (same generation), and itemPtrs[i] is packItems[i]'s item.
    struct ApplySnapshot
    {
        Packer::Result result;
        int sourceTarget;
        std::vector<Packer::Item> packItems;
        std::vector<Item*> itemPtrs;
    };

    // GetBestResult plus the snapshot that result belongs to. section must be
    // live (taken from the inventory being sorted): when a result qualifies,
    // the snapshot's item identities are re-checked against
    // section->getItems() first. On a mismatch the section is marked stale
    // (MarkSectionStale) and false is returned so the caller takes the
    // synchronous fallback. False without marking when no result qualifies.
    // out is unspecified on false. Main thread only.
    static bool GetApplySnapshot(InventorySection* section, int target, ApplySnapshot& out);

    // Drop section's results and arm a re-snapshot for every slot that lists
    // it. For a snapshot found stale at apply time, and for StackSort's own
    // mutations whose add/remove callbacks ApplyGuard hides when they turn
    // out to have changed the item set. Aborts the section's queued and
    // active jobs (whatever GUI queued them), so the sweep closes as aborted
    // instead of logging a digest over a replaced snapshot. Does not touch
    // the revert capture. Main thread only. Never throws.
    static void MarkSectionStale(InventorySection* section);

    // One live item's identity exactly as snapshots record it (dims
    // orientation-free when KenshiRotate is loaded). Main thread only.
    static void ReadItemIdentity(Item* item, ItemIdentity& out);

    static Packer::TargetDim GetDim();
    // Flip H<->W, abort all jobs, invalidate every open GUI's entries and arm
    // an immediate re-snapshot for each; every GUI re-snapshots and
    // re-enqueues under the new dim from its own next OnTrackedGUIUpdate.
    // Never dereferences a slot's gui. Never throws: called from a click.
    static void ToggleDim();

    enum LogKind
    {
        LOG_KIND_ERROR,
        LOG_KIND_INFO,
        LOG_KIND_VERBOSE,
        LOG_KIND_DEBUG
    };

    // Any thread. KenshiLib's DebugLog/ErrorLog append to a global stringstream
    // and ofstream with no lock while RE_Kenshi reads that stringstream every
    // frame on the main thread, so a worker must never call them (directly or
    // through Log.h). Queues one finished line instead; the main thread emits
    // it from FlushWorkerLog. Prefer the WorkerLog* macros below: they skip
    // string construction below the active level. The queue lock is a leaf
    // (see ResultCache.h). Lines queued while no worker pool exists (before
    // Init, after Shutdown) are dropped.
    static void QueueLog(LogKind kind, const std::string& line);

    // Main thread only. Emits every queued line through the normal Log
    // functions, outside the queue lock.
    static void FlushWorkerLog();

    static bool HasContext(InventoryGUI* gui);
    static bool IsTrackedInventory(Inventory* inv);
    static bool ContextMatchesInventory(InventoryGUI* gui);
    static InventoryGUI* FindContextGUIForInventory(Inventory* inv);
    static bool IsApplying();
    static void SetApplying(bool v);
    static bool IsRunning();
    // Max reachable target value for the active dim at this section.
    static int GetMaxTarget(InventorySection* section);
    // True if any queued or active job targets this section.
    static bool IsSectionActive(InventorySection* section);

    struct SectionRef
    {
        InventorySection* key;
        LONG generation;
    };

    struct Job
    {
        SectionRef section;
        InventoryGUI* sourceGUI;
        volatile LONG abortFlag;
        Packer::TargetDim dim;
        int target;      // 0 = first-pass chunk, >0 = refine this target
        int targetStart; // first-pass chunk start (inclusive), unused for refinement
        int targetEnd;   // first-pass chunk end (inclusive), 0 = unused/refinement
        int priority;    // 0 = first-pass, 1 = refinement
        int sweepSlot;   // index into the sweep table, -1 = untracked
        int sweepId;     // guards against a slot reused by a later sweep
    };

  private:
    static DWORD WINAPI WorkerProc(LPVOID param);
    static void ExecuteJob(Job* job, int workerIdx);

    static void SnapshotSections(int trackIdx, InventoryGUI* gui, bool* outNeedsCompute);
    static void ScanInventory(int trackIdx, Inventory* inv, bool* outNeedsCompute);

    static int FindTracking(InventoryGUI* gui);
    static int AllocTracking(InventoryGUI* gui);
    static void FreeTracking(int idx);
    static void MarkTrackingMutated(int idx);
    static void MarkInventoryMutated(int idx, Inventory* inv);
    static void ReclaimStaleTracking();

    static void EnqueueJob(int trackIdx);

    // Sections whose jobs an abort dropped, each at the newest generation
    // seen. Defined in SortWorker.cpp.
    struct AbortedSections;

    // Main thread. Never throws. Aborts every queued and active job that
    // matches: by sourceGUI when gui is non-NULL, by section key when section
    // is non-NULL (both NULL matches nothing). Sweeps the abort closes are
    // logged with their aborted counts. outAborted, when non-NULL, receives
    // the sections whose jobs were dropped.
    static void AbortJobs(InventoryGUI* gui, InventorySection* section, AbortedSections* outAborted);
    static void AbortJobsForGUI(InventoryGUI* gui, AbortedSections* outAborted = NULL);
    // Main thread. Never throws. Never dereferences a slot's gui. For each
    // aborted section whose entry is still at the aborted generation and
    // incomplete (no other snapshot replaced it), arms an already-expired
    // re-snapshot on every other live slot that lists it: nothing else would
    // finish that sweep, since only the slot that inserted a snapshot
    // enqueues it.
    static void RearmSlotsSharing(int exceptIdx, const AbortedSections& aborted);
    // Worker thread. Takes ownership of jobs[]: queues them, or deletes them
    // and returns false when parent was aborted or cs moved to a newer
    // generation (checked under s_queueLock, which AbortJobs and
    // ToggleDim also hold while setting abort flags). A batch dropped while
    // the parent still holds cs's generation marks cs->refineAborted.
    static bool EnqueueRefinementBatch(const Job* parent, CachedSection* cs, Job** jobs, int count);
};

// Worker-thread counterparts of LogError/LogInfo/LogVerbose/LogDebug. Include
// Log.h before expanding them (GetLogLevel).
#define WorkerLogError(msg) SortWorker::QueueLog(SortWorker::LOG_KIND_ERROR, msg)

#define WorkerLogInfo(msg)                                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        if (GetLogLevel() >= LOG_LEVEL_INFO) SortWorker::QueueLog(SortWorker::LOG_KIND_INFO, msg);                     \
    } while (0)

#define WorkerLogVerbose(msg)                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        if (GetLogLevel() >= LOG_LEVEL_VERBOSE) SortWorker::QueueLog(SortWorker::LOG_KIND_VERBOSE, msg);               \
    } while (0)

#ifdef STACKSORT_VERBOSE
#define WorkerLogDebug(msg) SortWorker::QueueLog(SortWorker::LOG_KIND_DEBUG, msg)
#else
#define WorkerLogDebug(msg) ((void)0)
#endif
