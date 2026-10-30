#pragma once

#include <Windows.h>
#include <vector>

#include "Packer.h"

class InventorySection;
class Item;

struct ScopedLock
{
    CRITICAL_SECTION& cs;
    ScopedLock(CRITICAL_SECTION& c) : cs(c)
    {
        EnterCriticalSection(&cs);
    }
    ~ScopedLock()
    {
        LeaveCriticalSection(&cs);
    }

  private:
    ScopedLock(const ScopedLock&);
    ScopedLock& operator=(const ScopedLock&);
};

// One snapshot item, as values read from the live Item at snapshot time. The
// handle fields are copied out of RootObjectBase::handle so this header pulls
// in no Kenshi types. The pointer alone is not an identity: after a save load
// the allocator can hand a dead item's address to a new item, and the handle's
// serial is what tells the two apart. Compared only against values read from
// live items (section->getItems()); a cached item pointer is never
// dereferenced.
struct ItemIdentity
{
    Item* item;
    unsigned int index;
    unsigned int serial;
    unsigned int container;
    unsigned int containerSerial;
    int type;
    // itemWidth / itemHeight, or (min, max) of the two when KenshiRotate is
    // loaded (see ReadLiveIdentities in SortWorker.cpp).
    int w;
    int h;

    bool operator==(const ItemIdentity& o) const
    {
        return item == o.item && index == o.index && serial == o.serial && container == o.container &&
               containerSerial == o.containerSerial && type == o.type && w == o.w && h == o.h;
    }
    bool operator!=(const ItemIdentity& o) const
    {
        return !(*this == o);
    }
    bool operator<(const ItemIdentity& o) const
    {
        if (item != o.item) return item < o.item;
        if (index != o.index) return index < o.index;
        if (serial != o.serial) return serial < o.serial;
        if (container != o.container) return container < o.container;
        if (containerSerial != o.containerSerial) return containerSerial < o.containerSerial;
        if (type != o.type) return type < o.type;
        if (w != o.w) return w < o.w;
        return h < o.h;
    }
};

// One item's cell at revert-capture time. identity is read the way snapshots
// read it (SortWorker::ReadItemIdentity), so a different item at a reused
// address does not match at revert; w/h are the exact itemWidth/itemHeight,
// orientation included, so an item rotated since (KenshiRotate) does not
// either. Unmatched live items are placed in free cells instead.
struct OriginalPlacement
{
    ItemIdentity identity;
    int x, y, w, h;
};

// Keyed by InventorySection* (stable per-character while the world lives).
// Persists across GUI close/reopen. Nothing flushes it on a save load, so a
// key can come back as a different section at a reused address: reuse and
// apply both re-verify the snapshot's item identities against the live
// section instead of trusting the key.
//
// Lock order: s_lock (ResultCache.cpp, map structure) -> resultLock (per
// entry). s_queueLock (SortWorker.cpp, job queue) is a leaf: nothing takes
// s_lock or a resultLock while holding it, and it is never taken while either
// of those is held. s_logLock (SortWorker.cpp, worker log queue) is a second
// leaf: taken alone, only around a vector push or swap, never while any other
// lock is held, and no lock is acquired and no Log function runs while it is
// held.
//
// Publication invariant: a worker writes results[], bestOrders[], slotDone[],
// numSlotsDone, computedUpTo and valid only while holding resultLock, and only
// after re-checking inside that lock that its job is not aborted, that
// generation still equals the job's generation, and that the slot is in range.
// Every main-thread change to what an entry describes (Insert, Invalidate;
// ToggleDim goes through both) bumps generation under the same resultLock, so
// a stale job can neither publish into nor re-validate the new state. The one
// worker-side bump, InvalidateIfGeneration after a job threw, follows the
// same rule and only fires while the entry is still at that job's generation.
// Main-thread readers of those fields take resultLock as well.

struct CachedSection
{
    CachedSection()
        : section(NULL), gridW(0), gridH(0), dim(Packer::TARGET_H), itemCount(0), computedUpTo(0), numSlotsDone(0),
          refineAborted(0), valid(0), generation(0), lastUsedTick(0), hasOriginalPositions(false), resultLockInit(false)
    {
        memset(&resultLock, 0, sizeof(resultLock));
    }

    InventorySection* section; // cache key (game pointer, stable per-character)
    int gridW;
    int gridH;
    Packer::TargetDim dim; // target dim this cache was built for
    int itemCount;         // snapshot item count (for reopen validation)
    std::vector<Packer::Item> packItems;
    std::vector<Item*> itemPtrs;
    // The snapshot's item identities, sorted. Written by Insert under
    // resultLock; main thread only. Reuse and apply require the live section to
    // produce the same sorted list.
    std::vector<ItemIdentity> sortedIdentity;
    std::vector<Packer::Result> results;                // indexed by H-1
    std::vector<std::vector<Packer::Item> > bestOrders; // parallel to results[], warm-start seeds
    // Parallel to results[]: 1 once that slot was published for the current
    // generation. Unpublished slots hold default-constructed Results that
    // readers must skip.
    std::vector<unsigned char> slotDone;
    // Highest target published. A running max across parallel chunks, NOT a
    // prefix: slots below it may still be unpublished (see slotDone).
    volatile LONG computedUpTo;
    int numSlotsDone; // set slotDone entries; == results.size() means complete
    // Set under resultLock when refinement of the current generation may have
    // been lost: AbortJobs dropped or flagged one of its jobs (main thread), or
    // EnqueueRefinementBatch could not queue a batch (worker, generation
    // re-checked in the lock). The entry may look complete (numSlotsDone) yet
    // hold unrefined slots, so it must not be reused and a sharing slot must
    // re-sweep it. Only Insert clears it; setting it can only cost a re-sweep.
    int refineAborted;
    volatile LONG valid;      // 1 after this generation's first publish
    volatile LONG generation; // bumped by Insert/Invalidate; workers drop mismatches
    DWORD lastUsedTick;       // Insert or cache-hit tick (LRU eviction); under s_lock
    // Main thread only (UI capture/revert, ClearOriginalPositions); workers
    // never touch these two, so they need no resultLock. Lifetime is one GUI
    // open: SortWorker clears them on a hand edit of a tracked inventory, and
    // on open, close, reclaim and backpack change unless another live GUI
    // still shows the section; the UI clears them once a revert ran (or found
    // the capture unusable). Insert alone never clears an existing capture.
    std::vector<OriginalPlacement> originalPositions; // pre-sort item positions
    bool hasOriginalPositions;                        // true once original positions captured (first click)
    CRITICAL_SECTION resultLock;                      // see publication invariant above
    bool resultLockInit;                              // true once resultLock is initialized

    // Do not copy after resultLockInit — CRITICAL_SECTION has self-referencing pointers.
};

class ResultCache
{
  public:
    static void Init();
    static void Shutdown();
    static void InvalidateAll(); // retain entries until workers no longer hold them

    // Insert or update a cache entry with a fresh snapshot.
    // Pre-allocates results[] sized for the given dim:
    //   TARGET_H -> gridH-1 slots (targets 1..gridH-1)
    //   TARGET_W -> gridW-1 slots (targets 1..gridW-1)
    // Resets publication state and bumps generation under resultLock.
    // identities is parallel to packItems (identities[i] is packItems[i]'s
    // live item); itemPtrs and sortedIdentity are derived from it.
    // Main thread only. Returns the new generation number.
    static LONG Insert(InventorySection* section, int gridW, int gridH, Packer::TargetDim dim,
                       const std::vector<Packer::Item>& packItems, const std::vector<ItemIdentity>& identities);

    // Lookup by section pointer. Returns NULL if not found.
    // The returned pointer is stable (std::map iterators are not
    // invalidated by insert/erase of other keys). Caller must ensure
    // the entry is not erased while using the pointer (abort protocol).
    static CachedSection* Find(InventorySection* section);

    // Set valid=0 and bump generation under resultLock, so jobs still in
    // flight for the old snapshot can neither publish nor re-validate it.
    // Does not remove the entry. Main thread only.
    static void Invalidate(InventorySection* section);

    // Worker-safe variant for a job that failed mid-publish: invalidates only
    // while the entry still holds the job's generation, so a newer snapshot
    // the main thread inserted meanwhile keeps its results. Any thread; the
    // caller must keep the entry alive (an active job does, see EvictIfNeeded).
    static void InvalidateIfGeneration(InventorySection* section, LONG generation);

    // Drop the right-click revert capture so the next first click recaptures
    // the current hand layout; also drops the section's sorted-without-capture
    // mark, entry or not. Main thread only. Never throws.
    static void ClearOriginalPositions(InventorySection* section);

    // Sorted-without-capture marks: sections StackSort rearranged while they
    // held no revert capture (no entry yet, or the apply itself dropped the
    // capture). Their screen layout is StackSort's, so the capture must not be
    // taken until something hands the layout back to the player: every event
    // that drops a capture (ClearOriginalPositions: hand edit, GUI open/close,
    // revert) drops the mark too. Kept outside the entries because the
    // section may have none. Keyed by pointer, never dereferenced; a stale
    // mark on a reused address can only withhold one capture. Main thread
    // only. Mark never throws (an allocation failure leaves it unmarked).
    static void MarkSortedWithoutCapture(InventorySection* section);
    static bool IsSortedWithoutCapture(InventorySection* section);

    // Remove entry from the map. Caller must ensure no worker is
    // actively writing to this entry (use abort + generation check).
    static void Remove(InventorySection* section);

    // Check if a cached entry can be reused on inventory reopen.
    // liveSorted is the sorted identity list read from the live section; it
    // must equal the snapshot's 1:1 (same items, same handles, same dims as
    // ItemIdentity records them), so
    // a count-preserving swap made while no GUI tracked the section, or a new
    // section at a freed address, is a miss. Grid dims and cached dim must
    // match too. Cache entries sized for a different dim fail, and so do
    // entries whose sweep did not publish every slot (aborted on close, or
    // still in flight): nothing would ever fill the missing slots otherwise.
    // A true return counts as a use for LRU eviction. Main thread only.
    static bool IsReusable(InventorySection* section, const std::vector<ItemIdentity>& liveSorted, int gridW, int gridH,
                           Packer::TargetDim dim);

    // Evict least recently used unreferenced entries when cache exceeds
    // maxEntries. referencedKeys must hold every section in use by an open
    // GUI AND every section with a queued or active job (collected under
    // s_queueLock before this call -- taking s_queueLock here, under s_lock,
    // would invert the lock order). Main thread only: a section with no job
    // at collection time can only gain one through EnqueueJob on the main
    // thread, since refinement jobs are spawned by an active parent job for
    // the same section.
    static void EvictIfNeeded(InventorySection* const* referencedKeys, int referencedCount, int maxEntries);

    // Return current number of cached entries (for logging).
    static int Size();
};
