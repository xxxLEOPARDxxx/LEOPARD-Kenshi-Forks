#include <algorithm>
#include <map>
#include <set>
#include <climits>

#include "ResultCache.h"

static std::map<InventorySection*, CachedSection> s_cache;
static CRITICAL_SECTION s_lock;

// Main thread only, so not under s_lock (see MarkSortedWithoutCapture).
static std::set<InventorySection*> s_sortedWithoutCapture;

// Bounds the marks of sections that never get an entry (and so are never
// cleared by a capture drop). Clearing them all can only permit one wrong
// capture per section, which a revert then restores as StackSort's layout.
static const size_t MAX_UNCAPTURED_MARKS = 512;

// Caller holds cs.resultLock. Wraps past LONG_MAX to 1 so 0 keeps meaning
// "never inserted" (Insert relies on that for hasOriginalPositions).
static void BumpGenerationLocked(CachedSection& cs)
{
    LONG newGen = cs.generation == LONG_MAX ? 1 : cs.generation + 1;
    if (newGen <= 0) newGen = 1;
    InterlockedExchange(&cs.generation, newGen);
}

void ResultCache::Init()
{
    InitializeCriticalSection(&s_lock);
}

void ResultCache::Shutdown()
{
    ScopedLock guard(s_lock);
    for (std::map<InventorySection*, CachedSection>::iterator it = s_cache.begin(); it != s_cache.end(); ++it)
    {
        if (it->second.resultLockInit) DeleteCriticalSection(&it->second.resultLock);
    }
    s_cache.clear();
    s_sortedWithoutCapture.clear();
}

static void InvalidateAllImpl(bool clearOriginalPositions,bool canonicalizeUndo)
{
    ScopedLock guard(s_lock);
    for (std::map<InventorySection*, CachedSection>::iterator it = s_cache.begin(); it != s_cache.end(); ++it)
    {
        CachedSection& cs = it->second;
        ScopedLock entry(cs.resultLock);
        BumpGenerationLocked(cs);
        InterlockedExchange(&cs.valid, 0);
        if(clearOriginalPositions)
        {
            cs.hasOriginalPositions = false;
            cs.originalPositions.clear();
        }
        else if(canonicalizeUndo)
        {
            // ReadItemIdentity becomes orientation-free after late binding.
            // Migrate only its saved plain-data comparison dimensions. The
            // exact OriginalPlacement w/h must remain unchanged for undo.
            for(size_t i=0;i<cs.originalPositions.size();++i)
            {
                ItemIdentity& identity=cs.originalPositions[i].identity;
                if(identity.w>identity.h) std::swap(identity.w,identity.h);
            }
        }
    }
    if(clearOriginalPositions) s_sortedWithoutCapture.clear();
}

void ResultCache::InvalidateAll() {InvalidateAllImpl(true,false);}
void ResultCache::InvalidateForRotation() {InvalidateAllImpl(false,true);}

LONG ResultCache::Insert(InventorySection* section, int gridW, int gridH, Packer::TargetDim dim,
                         const std::vector<Packer::Item>& packItems, const std::vector<ItemIdentity>& identities)
{
    // Built before any lock: allocation and sorting need no shared state.
    std::vector<Item*> itemPtrs(identities.size());
    for (size_t i = 0; i < identities.size(); ++i)
        itemPtrs[i] = identities[i].item;
    std::vector<ItemIdentity> sorted(identities);
    std::sort(sorted.begin(), sorted.end());

    ScopedLock guard(s_lock);

    CachedSection& cs = s_cache[section];
    if (!cs.resultLockInit)
    {
        InitializeCriticalSection(&cs.resultLock);
        cs.resultLockInit = true;
    }

    // New entries start without originals; existing entries keep theirs
    // across re-inserts (a debounced re-snapshot right after ConsolidateStacks
    // must not wipe the capture that follows it). SortWorker owns clearing.
    if (cs.generation == 0) cs.hasOriginalPositions = false;

    // One resultLock block for the snapshot (workers copy packItems/dims under
    // the same lock, so no torn read), the slot reset and the generation bump:
    // a worker that re-checks generation under this lock either publishes
    // before the resize or sees the new generation and drops its result.
    // Bumping after release would let it write the old snapshot's result into
    // (or past the end of) the resized vectors.
    int maxSlots = (dim == Packer::TARGET_H) ? (gridH - 1) : (gridW - 1);
    LONG newGen  = 0;
    {
        ScopedLock rl(cs.resultLock);
        cs.section   = section;
        cs.gridW     = gridW;
        cs.gridH     = gridH;
        cs.dim       = dim;
        cs.itemCount = (int)identities.size();
        cs.packItems = packItems;
        cs.itemPtrs.swap(itemPtrs);
        cs.sortedIdentity.swap(sorted);
        cs.lastUsedTick = GetTickCount();
        cs.results.clear();
        cs.bestOrders.clear();
        cs.slotDone.clear();
        if (maxSlots > 0)
        {
            cs.results.resize(maxSlots);
            cs.bestOrders.resize(maxSlots);
            cs.slotDone.resize(maxSlots, 0);
        }
        cs.numSlotsDone  = 0;
        cs.refineAborted = 0;
        InterlockedExchange(&cs.computedUpTo, 0);
        InterlockedExchange(&cs.valid, 0); // not valid until a worker publishes
        BumpGenerationLocked(cs);
        newGen = cs.generation;
    }

    return newGen;
}

CachedSection* ResultCache::Find(InventorySection* section)
{
    ScopedLock guard(s_lock);
    std::map<InventorySection*, CachedSection>::iterator it = s_cache.find(section);
    return (it != s_cache.end()) ? &it->second : NULL;
}

void ResultCache::Invalidate(InventorySection* section)
{
    ScopedLock guard(s_lock);
    std::map<InventorySection*, CachedSection>::iterator it = s_cache.find(section);
    if (it == s_cache.end() || !it->second.resultLockInit) return;

    // Under resultLock so a worker between its in-lock check and its publish
    // finishes first; the generation bump then stops any later publish from
    // re-setting valid=1 on the pre-mutation snapshot.
    CachedSection& cs = it->second;
    ScopedLock rl(cs.resultLock);
    InterlockedExchange(&cs.valid, 0);
    BumpGenerationLocked(cs);
}

void ResultCache::InvalidateIfGeneration(InventorySection* section, LONG generation)
{
    ScopedLock guard(s_lock);
    std::map<InventorySection*, CachedSection>::iterator it = s_cache.find(section);
    if (it == s_cache.end() || !it->second.resultLockInit) return;

    CachedSection& cs = it->second;
    ScopedLock rl(cs.resultLock);
    if (cs.generation != generation) return;
    InterlockedExchange(&cs.valid, 0);
    BumpGenerationLocked(cs);
}

void ResultCache::ClearOriginalPositions(InventorySection* section)
{
    s_sortedWithoutCapture.erase(section);

    // s_lock only guards the map lookup; the two fields are main-thread only.
    ScopedLock guard(s_lock);
    std::map<InventorySection*, CachedSection>::iterator it = s_cache.find(section);
    if (it == s_cache.end()) return;

    it->second.originalPositions.clear();
    it->second.hasOriginalPositions = false;
}

void ResultCache::MarkSortedWithoutCapture(InventorySection* section)
{
    try
    {
        if (s_sortedWithoutCapture.size() >= MAX_UNCAPTURED_MARKS) s_sortedWithoutCapture.clear();
        s_sortedWithoutCapture.insert(section);
    }
    catch (...)
    {
    }
}

bool ResultCache::IsSortedWithoutCapture(InventorySection* section)
{
    return s_sortedWithoutCapture.count(section) != 0;
}

void ResultCache::Remove(InventorySection* section)
{
    ScopedLock guard(s_lock);
    std::map<InventorySection*, CachedSection>::iterator it = s_cache.find(section);
    if (it != s_cache.end())
    {
        if (it->second.resultLockInit) DeleteCriticalSection(&it->second.resultLock);
        s_cache.erase(it);
    }
}

bool ResultCache::IsReusable(InventorySection* section, const std::vector<ItemIdentity>& liveSorted, int gridW,
                             int gridH, Packer::TargetDim dim)
{
    ScopedLock guard(s_lock);
    std::map<InventorySection*, CachedSection>::iterator it = s_cache.find(section);
    if (it == s_cache.end()) return false;

    CachedSection& cs = it->second;
    if (!cs.resultLockInit) return false;

    bool reusable;
    {
        ScopedLock rl(cs.resultLock);
        reusable = cs.valid != 0 && cs.dim == dim && cs.gridW == gridW && cs.gridH == gridH && !cs.results.empty() &&
                   cs.numSlotsDone == (int)cs.results.size() && !cs.refineAborted && cs.sortedIdentity == liveSorted;
    }
    if (reusable) cs.lastUsedTick = GetTickCount();
    return reusable;
}

void ResultCache::EvictIfNeeded(InventorySection* const* referencedKeys, int referencedCount, int maxEntries)
{
    ScopedLock guard(s_lock);

    // Ages rather than raw ticks so the GetTickCount wrap (49.7 days) cannot
    // make the newest entry look like the oldest.
    DWORD now = GetTickCount();

    while ((int)s_cache.size() > maxEntries)
    {
        std::map<InventorySection*, CachedSection>::iterator oldest = s_cache.end();
        DWORD oldestAge                                             = 0;

        for (std::map<InventorySection*, CachedSection>::iterator it = s_cache.begin(); it != s_cache.end(); ++it)
        {
            // Referenced keys cover open GUIs and sections with queued or
            // active jobs: erasing one of the latter would delete its
            // resultLock out from under a worker.
            bool referenced = false;
            for (int i = 0; i < referencedCount; ++i)
            {
                if (referencedKeys[i] == it->first)
                {
                    referenced = true;
                    break;
                }
            }
            if (referenced) continue;

            DWORD age = now - it->second.lastUsedTick;
            if (oldest == s_cache.end() || age > oldestAge)
            {
                oldestAge = age;
                oldest    = it;
            }
        }

        if (oldest == s_cache.end()) break; // all entries are referenced, can't evict

        // TryEnter stays manual — conditional acquire not compatible with ScopedLock
        if (oldest->second.resultLockInit)
        {
            if (!TryEnterCriticalSection(&oldest->second.resultLock)) break; // worker still holds lock, retry next time
            LeaveCriticalSection(&oldest->second.resultLock);
            DeleteCriticalSection(&oldest->second.resultLock);
        }
        s_cache.erase(oldest);
    }
}

int ResultCache::Size()
{
    ScopedLock guard(s_lock);
    return (int)s_cache.size();
}
