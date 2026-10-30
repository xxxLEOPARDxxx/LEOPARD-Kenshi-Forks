#pragma warning(push)
#pragma warning(disable : 4091)
#include <kenshi/Inventory.h>
#include <kenshi/gui/InventoryGUI.h>
#pragma warning(pop)

#include <algorithm>
#include <vector>
#include <sstream>

#include "StackSortUI.h"
#include "Packer.h"
#include "ResultCache.h"
#include "SortWorker.h"
#include "StackSort.h"
#include "StackSortSettings.h"
#include "SectionEligibility.h"
#include "MergeDriver.h"

#include <stdexcept>
#include "Log.h"
#include "NativeMergeAdapter.h"

struct CallbackGuard
{
    InventorySection* section;
    RootObject* saved;

    CallbackGuard(InventorySection* s) : section(s), saved(s->callbackObject)
    {
        s->callbackObject = NULL;
    }

    ~CallbackGuard()
    {
        section->callbackObject = saved;
    }
};

// Never nested: the inner destructor would clear the flag while the outer
// scope is still mutating, letting its callbacks reach the mutation hooks.
struct ApplyGuard
{
    ApplyGuard()
    {
        SortWorker::SetApplying(true);
    }
    ~ApplyGuard()
    {
        SortWorker::SetApplying(false);
    }
};

namespace
{

// The cells _addItem can address. It indexes the content grid as (x, y) and
// throws Array2d<Item>::BoundsViolation past it. The grid is normally exactly
// width x height; the min only guards a mismatch.
int PlaceableWidth(const InventorySection* section)
{
    int w = section->width;
    if ((int)section->content.nRows < w) w = (int)section->content.nRows;
    return w;
}

int PlaceableHeight(const InventorySection* section)
{
    int h = section->height;
    if ((int)section->content.nCols < h) h = (int)section->content.nCols;
    return h;
}

void CollectLiveItems(InventorySection* section, std::vector<Item*>& out)
{
    out.clear();
    const Ogre::vector<InventorySection::SectionItem>::type& items = section->getItems();
    out.reserve(items.size());
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (items[i].item) out.push_back(items[i].item);
    }
}

// Same items 1:1. A pointer listed twice on either side is a mismatch.
// Sorts both in place and allocates nothing (std::sort is in-place), so the
// apply's mutation window can call it on pre-reserved scratch.
bool SameItemSetSorting(std::vector<Item*>& a, std::vector<Item*>& b)
{
    if (a.size() != b.size()) return false;
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    if (std::adjacent_find(a.begin(), a.end()) != a.end()) return false;
    return a == b;
}

bool SameItemSet(std::vector<Item*> a, std::vector<Item*> b)
{
    return SameItemSetSorting(a, b);
}

// Row-major occupancy of the placeable grid; 1 = taken.
struct Occupancy
{
    int w;
    int h;
    std::vector<unsigned char> cells;

    Occupancy(int gw, int gh) : w(gw > 0 ? gw : 0), h(gh > 0 ? gh : 0), cells((size_t)w * (size_t)h, 0) {}

    void Clear()
    {
        std::fill(cells.begin(), cells.end(), (unsigned char)0);
    }

    bool IsFree(int x, int y, int iw, int ih) const
    {
        if (iw < 1 || ih < 1 || x < 0 || y < 0 || x + iw > w || y + ih > h) return false;
        for (int dy = 0; dy < ih; ++dy)
        {
            for (int dx = 0; dx < iw; ++dx)
            {
                if (cells[(y + dy) * w + x + dx]) return false;
            }
        }
        return true;
    }

    // Clipped to the grid, so a footprint that overruns it still blocks the
    // cells it does cover.
    void Stamp(int x, int y, int iw, int ih)
    {
        for (int dy = 0; dy < ih; ++dy)
        {
            int cy = y + dy;
            if (cy < 0 || cy >= h) continue;
            for (int dx = 0; dx < iw; ++dx)
            {
                int cx = x + dx;
                if (cx < 0 || cx >= w) continue;
                cells[cy * w + cx] = 1;
            }
        }
    }

    // Row-major first fit, the order InventorySection::addItem scans in.
    bool FindFree(int iw, int ih, int& outX, int& outY) const
    {
        for (int y = 0; y + ih <= h; ++y)
        {
            for (int x = 0; x + iw <= w; ++x)
            {
                if (IsFree(x, y, iw, ih))
                {
                    outX = x;
                    outY = y;
                    return true;
                }
            }
        }
        return false;
    }
};

// Stamps every item the section holds. False when a footprint leaves the
// placeable grid or overlaps another.
bool StampSection(InventorySection* section, Occupancy& occ)
{
    bool ok                                                        = true;
    const Ogre::vector<InventorySection::SectionItem>::type& items = section->getItems();
    for (size_t i = 0; i < items.size(); ++i)
    {
        const InventorySection::SectionItem& si = items[i];
        if (!si.item) continue;
        if (!occ.IsFree(si.x, si.y, si.w, si.h)) ok = false;
        occ.Stamp(si.x, si.y, si.w, si.h);
    }
    return ok;
}

struct PlacedCell
{
    Item* item;
    int x;
    int y;
};

bool PlacedCellLess(const PlacedCell& a, const PlacedCell& b)
{
    if (a.item != b.item) return a.item < b.item;
    if (a.x != b.x) return a.x < b.x;
    return a.y < b.y;
}

void CollectPlacedCells(InventorySection* section, std::vector<PlacedCell>& out)
{
    out.clear();
    const Ogre::vector<InventorySection::SectionItem>::type& items = section->getItems();
    out.reserve(items.size());
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (!items[i].item) continue;
        PlacedCell c = {items[i].item, items[i].x, items[i].y};
        out.push_back(c);
    }
}

// _addItem returns nothing, so this is the only evidence a layout landed: the
// section holds exactly the expected items, each once and at its planned cell,
// inside the placeable grid with no two footprints overlapping. expected lists
// distinct items. live and occ are caller-owned scratch (pre-allocated so
// this allocates nothing in the apply's mutation window); expected is sorted
// in place.
bool LayoutLanded(InventorySection* section, std::vector<PlacedCell>& expected, std::vector<PlacedCell>& live,
                  Occupancy& occ)
{
    occ.Clear();
    if (!StampSection(section, occ)) return false;

    CollectPlacedCells(section, live);
    if (live.size() != expected.size()) return false;

    std::sort(live.begin(), live.end(), PlacedCellLess);
    std::sort(expected.begin(), expected.end(), PlacedCellLess);
    for (size_t i = 0; i < live.size(); ++i)
    {
        if (live[i].item != expected[i].item || live[i].x != expected[i].x || live[i].y != expected[i].y) return false;
    }
    return true;
}

// Under the caller's ApplyGuard + CallbackGuard, after a layout did not land
// as planned. Vanilla autoArrange repositions what the section holds; the
// items applyLayout removed but never homed are then put into free cells with
// _addItem. Never addItem: it is gated (enabled, limited slot) and may merge
// stacks, so it can orphan or destroy the item.
// The unhomed set is fixed BEFORE autoArrange runs: those items were all in
// the section when the apply began and removeItem does not destroy, so each
// pointer is alive and owned by no container. An item that autoArrange itself
// drops, merges or moves elsewhere may be destroyed or held by someone else,
// so it must never be re-added here; the caller's item-set check reports it.
// An unhomed item with no free cell left goes to outOwnerless, still removed
// and owned by nothing; the caller must hand it back (HandBackToInventory).
// unhomed and occ are caller-owned pre-allocated scratch; outOwnerless must
// have capacity for items.size(). outArranged is set just before autoArrange
// runs: from then on unhomed is complete and is the only set the caller may
// still re-home (see applyLayout's handler).
void RecoverLayout(InventorySection* section, const std::vector<Item*>& items, std::vector<Item*>& outOwnerless,
                   std::vector<Item*>& unhomed, Occupancy& occ, bool& outArranged)
{
    unhomed.clear();
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (!section->hasItem(items[i])) unhomed.push_back(items[i]);
    }

    outArranged = true;
    section->_NV_autoArrange();

    occ.Clear();
    StampSection(section, occ);
    for (size_t i = 0; i < unhomed.size(); ++i)
    {
        Item* item = unhomed[i];

        int x = 0, y = 0;
        if (occ.FindFree(item->itemWidth, item->itemHeight, x, y))
        {
            section->_NV__addItem(item, x, y);
            occ.Stamp(x, y, item->itemWidth, item->itemHeight);
        }
        else
        {
            outOwnerless.push_back(item);
        }
    }
}

// Last resort for items RecoverLayout could not fit back: removed from the
// section and held by no container, they would vanish from the game (never
// saved, never freed). Inventory::addItem homes each like a pickup: any
// section that takes it, possibly merged into a stack (which destroys the
// passed Item, so it is not read after the call), else dropped on the ground
// (dropOnFail), never destroyed (destroyOnFail false).
// Must run after the apply's ApplyGuard and CallbackGuard are released: the
// item may land in any section of the inventory, so its add callback has to
// reach the owner and the mutation hooks like any other inventory change.
// Each item is handled under its own catch: a throw (a log line failing to
// allocate, or the engine's addItem) must not abandon the items after it.
// Never throws.
void HandBackToInventory(InventorySection* section, const std::vector<Item*>& items)
{
    Inventory* inv = section->parentInventory;
    for (size_t i = 0; i < items.size(); ++i)
    {
        try
        {
            if (!inv)
            {
                LogError("[StackSort] No free cell left for an item of '" + section->name +
                         "' after a failed apply and the section has no inventory to return it to");
                continue;
            }
            Item* item = items[i];
            if (inv->addItem(item, item->quantity, true, false))
            {
                LogError("[StackSort] No free cell left for an item of '" + section->name +
                         "' after a failed apply; returned it to the inventory");
            }
            else
            {
                LogError("[StackSort] No free cell left for an item of '" + section->name +
                         "' after a failed apply and the inventory refused it; dropped if its owner can drop items");
            }
        }
        catch (...)
        {
        }
    }
}

// StackSort's own guarded mutations hide their add/remove callbacks from the
// mutation hooks, so an item set change they cause must be reported directly.
// The log line is best-effort so a failed allocation cannot skip the
// invalidation.
void ReportItemSetChanged(InventorySection* section)
{
    try
    {
        LogError("[StackSort] Items of '" + section->name + "' changed during a rearrange; re-snapshotting");
    }
    catch (...)
    {
    }
    SortWorker::MarkSectionStale(section);
    ResultCache::ClearOriginalPositions(section);
}

// Vanilla arrange as StackSort's own fallback. Guarded like applyLayout: it
// only repositions the items the section already holds, so letting its
// callbacks reach the mutation hooks would invalidate and recompute every
// section of the GUI. The caller must refresh the GUI, since the guard keeps
// the owner's callbacks from doing it.
void GuardedAutoArrange(InventorySection* section)
{
    std::vector<Item*> before;
    CollectLiveItems(section, before);

    // Reserved before the guard and compared in place, so no allocation
    // after autoArrange can skip the item-set report.
    std::vector<Item*> after;
    after.reserve(before.size() + 1);
    {
        ApplyGuard applyGuard;
        CallbackGuard guard(section);
        section->_NV_autoArrange();
        CollectLiveItems(section, after);
    }

    if (!SameItemSetSorting(before, after)) ReportItemSetChanged(section);
}

// After ApplySort changed section. FirstClickInit captured every section that
// had an entry before this click's apply, so a section without a capture now
// either had no entry (sync fallback) or lost its capture inside the apply
// (ReportItemSetChanged). Either way its layout is StackSort's, and the next
// click must not capture it as the player's. Main thread only.
void MarkIfUncaptured(InventorySection* section)
{
    CachedSection* cs = ResultCache::Find(section);
    if (!cs || !cs->hasOriginalPositions) ResultCache::MarkSortedWithoutCapture(section);
}

// The re-check right before a mutation: itemPtrs must be exactly the
// section's live items (1:1), and each placement's footprint must be its
// item's live dims, or their swap when applyLayout can rotate the item.
// Pointers are dereferenced only after the set match proves each is live in
// this section.
bool LiveItemsMatch(InventorySection* section, const std::vector<Item*>& itemPtrs,
                    const std::vector<Packer::Placement>& placements)
{
    std::vector<Item*> live;
    CollectLiveItems(section, live);
    if (!SameItemSet(live, itemPtrs)) return false;
    if (placements.size() != itemPtrs.size()) return false;

    const bool canReconcile = StackSort_RotateAvailable();
    for (size_t i = 0; i < placements.size(); ++i)
    {
        const Packer::Placement& p = placements[i];
        if (p.id < 0 || (size_t)p.id >= itemPtrs.size()) return false;

        const Item* item = itemPtrs[p.id];
        bool exact       = item->itemWidth == p.w && item->itemHeight == p.h;
        bool swapped     = item->itemWidth == p.h && item->itemHeight == p.w;
        if (!exact && !(swapped && canReconcile)) return false;
    }
    return true;
}

Packer::Placement MakePlacement(int id, int x, int y, int w, int h)
{
    Packer::Placement p;
    p.id      = id;
    p.exactId = -1;
    p.x       = x;
    p.y       = y;
    p.w       = w;
    p.h       = h;
    p.rotated = false;
    return p;
}

bool LargerFootprint(const Item* a, const Item* b)
{
    return a->itemWidth * a->itemHeight > b->itemWidth * b->itemHeight;
}

} // namespace

static std::vector<unsigned char> snapshotOccupancyGrid(InventorySection* section)
{
    int gridW = section->width;
    int gridH = section->height;
    std::vector<unsigned char> grid(gridW * gridH, 0);

    const Ogre::vector<InventorySection::SectionItem>::type& items = section->getItems();
    for (size_t i = 0; i < items.size(); ++i)
    {
        const InventorySection::SectionItem& si = items[i];
        for (int dy = 0; dy < si.h; ++dy)
        {
            for (int dx = 0; dx < si.w; ++dx)
            {
                int cx = si.x + dx;
                int cy = si.y + dy;
                if (cx >= 0 && cx < gridW && cy >= 0 && cy < gridH) grid[cy * gridW + cx] = 1;
            }
        }
    }

    return grid;
}

// Moves itemPtrs, exactly the section's live items, to placements (placement
// ids index itemPtrs). The caller validated the layout and ran LiveItemsMatch
// immediately before, with nothing in between. No item is left ownerless: a
// refused removeItem rolls back before anything is placed, an item whose
// rotation fails goes to a free cell afterwards, a layout that did not land
// as planned is recovered with vanilla autoArrange, and an item that still
// has no cell goes back through the inventory (HandBackToInventory). Returns
// false after a rollback (refused removeItem, or an exception between the
// first removeItem and the last _addItem); the section then holds every item
// it held before, though not necessarily at its old cells.
//
// Everything the mutation window needs is allocated before the first
// removeItem, and the window's own handler allocates nothing: an exception
// there (bad_alloc included) would otherwise leave removed items in no
// container, and the hook barriers would let the game carry on and save that.
// Allocations inside the engine's removeItem/_addItem cannot be reserved, so
// the handler re-homes whatever the section no longer holds.
static bool applyLayout(InventorySection* section, const std::vector<Packer::Placement>& placements,
                        const std::vector<Item*>& itemPtrs)
{
    bool itemSetChanged = false;
    bool threw          = false;
    bool recovered      = false;
    bool arranged       = false;
    size_t rotateFailed = 0;

    const size_t n = itemPtrs.size();
    std::vector<Item*> ownerless;
    ownerless.reserve(n);
    std::vector<PlacedCell> before;
    std::vector<unsigned char> rotationFailed(placements.size(), 0);
    std::vector<PlacedCell> expected;
    expected.reserve(placements.size());
    std::vector<Item*> deferred;
    deferred.reserve(placements.size());
    std::vector<PlacedCell> live;
    live.reserve(n + 1);
    std::vector<Item*> unhomed;
    unhomed.reserve(n);
    std::vector<Item*> now;
    now.reserve(n + 1);
    std::vector<Item*> sortedPtrs(itemPtrs);
    Occupancy occ(PlaceableWidth(section), PlaceableHeight(section));
    {
        ApplyGuard applyGuard;
        CallbackGuard guard(section);

        CollectPlacedCells(section, before);

        try
        {
            // Iterate snapshot backward (live vector mutates during removal)
            for (int i = (int)n - 1; i >= 0; --i)
            {
                if (section->removeItem(itemPtrs[i])) continue;

                // Contradicts the live check the caller just ran. Each item
                // already removed goes back to its own cells, which nothing else
                // holds since nothing has been placed yet.
                for (size_t j = (size_t)i + 1; j < n; ++j)
                {
                    for (size_t k = 0; k < before.size(); ++k)
                    {
                        if (before[k].item != itemPtrs[j]) continue;
                        section->_NV__addItem(itemPtrs[j], before[k].x, before[k].y);
                        break;
                    }
                }
                section->recalculateTotalWeight();
                LogError("[StackSort] removeItem refused an item of '" + section->name + "'; layout not applied");
                return false;
            }

            // Rotation pass: ensure each item's dims match what the packer expects.
            // Compare current dims against p.w/p.h — handles both initial rotation
            // and un-rotating items changed by a previous sort.
            // All SetRotated calls happen before any placement (plan doc requirement).
            if (StackSort_RotateAvailable())
            {
                for (size_t i = 0; i < placements.size(); ++i)
                {
                    const Packer::Placement& p = placements[i];
                    Item* item                 = itemPtrs[p.id];

                    if (item->itemWidth == p.w && item->itemHeight == p.h)
                        continue; // dims already match — no rotation needed

                    bool currentlyRotated = StackSort_IsRotated(item);
                    if (!StackSort_SetRotated(item, !currentlyRotated))
                    {
                        rotationFailed[i] = 1;
                        ++rotateFailed;
                    }
                }
            }

            for (size_t i = 0; i < placements.size(); ++i)
            {
                const Packer::Placement& p = placements[i];
                Item* item                 = itemPtrs[p.id];
                if (rotationFailed[i])
                {
                    deferred.push_back(item);
                    continue;
                }
                section->_NV__addItem(item, p.x, p.y);
                PlacedCell c = {item, p.x, p.y};
                expected.push_back(c);
            }

            // At its unrotated dims the item no longer fits its planned
            // footprint, so it takes whatever cells the layout left free.
            bool allHomed = true;
            if (!deferred.empty())
            {
                occ.Clear();
                StampSection(section, occ);
                for (size_t i = 0; i < deferred.size(); ++i)
                {
                    Item* item = deferred[i];
                    int x = 0, y = 0;
                    if (!occ.FindFree(item->itemWidth, item->itemHeight, x, y))
                    {
                        allHomed = false; // RecoverLayout re-homes it below
                        continue;
                    }
                    section->_NV__addItem(item, x, y);
                    occ.Stamp(x, y, item->itemWidth, item->itemHeight);
                    PlacedCell c = {item, x, y};
                    expected.push_back(c);
                }
            }

            if (!allHomed || !LayoutLanded(section, expected, live, occ))
            {
                recovered = true;
                RecoverLayout(section, itemPtrs, ownerless, unhomed, occ, arranged);

                // Scratch only: autoArrange cannot add items, so now stays
                // within its reserve and the comparison allocates nothing.
                CollectLiveItems(section, now);
                itemSetChanged = !SameItemSetSorting(now, sortedPtrs);
            }

            section->recalculateTotalWeight();
        }
        catch (...)
        {
            // Allocates nothing (ownerless is pre-reserved to n). An item the
            // section no longer holds is owned by no container, so it goes to
            // a free cell, never to its old cell: a placed item may sit there.
            // Once RecoverLayout has run autoArrange, an item missing from the
            // section may have been merged away (freed) or moved elsewhere by
            // it, so only its unhomed set (complete before autoArrange) is
            // still ours to re-home, minus what it already put in ownerless.
            threw = true;
            try
            {
                const std::vector<Item*>& candidates = arranged ? unhomed : itemPtrs;
                const size_t alreadyOwnerless        = ownerless.size();
                occ.Clear();
                StampSection(section, occ);
                for (size_t i = 0; i < candidates.size(); ++i)
                {
                    Item* item = candidates[i];
                    if (section->hasItem(item)) continue;

                    bool listed = false;
                    for (size_t k = 0; k < alreadyOwnerless && !listed; ++k)
                        listed = ownerless[k] == item;
                    if (listed) continue;

                    int x = 0, y = 0;
                    if (occ.FindFree(item->itemWidth, item->itemHeight, x, y))
                    {
                        section->_NV__addItem(item, x, y);
                        occ.Stamp(x, y, item->itemWidth, item->itemHeight);
                    }
                    else
                    {
                        ownerless.push_back(item);
                    }
                }
                section->recalculateTotalWeight();
            }
            catch (...)
            {
            }
        }
    }

    // Log lines are best-effort here: a throw from one must not skip the
    // hand-back of ownerless items or the re-snapshot report.
    if (threw)
    {
        try
        {
            LogError("[StackSort] Exception while rearranging '" + section->name +
                     "'; layout abandoned, items restored");
        }
        catch (...)
        {
        }
        HandBackToInventory(section, ownerless);
        ReportItemSetChanged(section);
        return false;
    }

    try
    {
        if (rotateFailed > 0)
            LogInfo("[StackSort] SetRotated failed for " + IntToStr((int)rotateFailed) +
                    " item(s), placed in free cells");
        if (recovered)
            LogError("[StackSort] Layout for '" + section->name +
                     "' did not land as planned; fell back to vanilla arrange");
    }
    catch (...)
    {
    }

    // Before the report, so it re-snapshots whatever the hand-back changed.
    HandBackToInventory(section, ownerless);
    if (itemSetChanged) ReportItemSetChanged(section);
    return true;
}

void StackSortUI::ApplySort(InventoryGUI* gui, int target, int& outAppliedLerSide, int& outSourceTarget)
{
    outAppliedLerSide = 0;
    outSourceTarget   = 0;

    Inventory* inv = gui->_NV_getInventory();
    if (!inv) return;

    Packer::TargetDim dim       = SortWorker::GetDim();
    const char* dimTag          = (dim == Packer::TARGET_H) ? "H" : "W";
    bool anySorted              = false;
    int maxSourceTargetThisCall = 0;

    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);

    lektor<InventorySection*>& sections = inv->getAllSections();
    // Reserved up front so the push_backs below cannot throw mid-apply.
    std::vector<InventorySection*> markedThisCall;
    markedThisCall.reserve(sections.size());
    for (lektor<InventorySection*>::iterator it = sections.begin(); it != sections.end(); ++it)
    {
        InventorySection* section = *it;

        if (!StackSort_EligibleSection(section)) continue;
        if (!section->enabled) continue;

        const Ogre::vector<InventorySection::SectionItem>::type& sectionItems = section->getItems();
        if (sectionItems.empty()) continue;

        int gridW      = section->width;
        int gridH      = section->height;
        int placeW     = PlaceableWidth(section);
        int placeH     = PlaceableHeight(section);
        int sMaxTarget = ((dim == Packer::TARGET_H) ? gridH : gridW) - 1;

        int clamped = target;
        if (clamped > sMaxTarget) clamped = sMaxTarget;
        if (clamped < 1) clamped = 1;

        // Compute before-sort LER for comparison
        std::vector<unsigned char> beforeGrid = snapshotOccupancyGrid(section);
        int beforeLerArea = 0, beforeLerW = 0, beforeLerH = 0, beforeLerX = 0, beforeLerY = 0;
        Packer::Ler::ComputeLER(beforeGrid, gridW, gridH, beforeLerArea, beforeLerW, beforeLerH, beforeLerX,
                                beforeLerY);

        // Try best precomputed result across all target values. The snapshot
        // carries the exact items the worker packed it from; the layout is
        // proven against those, then against the live section right before
        // the mutation (it can predate a resize, or a change no hook saw).
        SortWorker::ApplySnapshot snap;
        if (SortWorker::GetApplySnapshot(section, clamped, snap) && snap.result.allPlaced)
        {
            const Packer::Result& precomputed = snap.result;
            int sourceTarget                  = snap.sourceTarget;

            if (Packer::Grid::ValidateLayout(placeW, placeH, snap.packItems, precomputed.placements) &&
                LiveItemsMatch(section, snap.itemPtrs, precomputed.placements))
            {
                std::string tStr = std::string("[") + dimTag + "] target=" + IntToStr(clamped);
                if (sourceTarget != clamped) tStr += " (via target=" + IntToStr(sourceTarget) + ")";

                LogInfo("[StackSort] Applying " + tStr + " for '" + section->name + "'" + " (" +
                        IntToStr((int)snap.itemPtrs.size()) + " items)" + ", LER " + IntToStr(precomputed.lerWidth) +
                        "x" + IntToStr(precomputed.lerHeight) + "=" + IntToStr(precomputed.lerArea) + " at (" +
                        IntToStr(precomputed.lerX) + "," + IntToStr(precomputed.lerY) + ")" + " (was " +
                        IntToStr(beforeLerW) + "x" + IntToStr(beforeLerH) + "=" + IntToStr(beforeLerArea) + ")");

                bool applied = applyLayout(section, precomputed.placements, snap.itemPtrs);
                markedThisCall.push_back(section);
                MarkIfUncaptured(section);
                if (!applied)
                {
                    SortWorker::MarkSectionStale(section);
                    continue;
                }
                int lerSide = (dim == Packer::TARGET_H) ? precomputed.lerHeight : precomputed.lerWidth;
                if (lerSide > outAppliedLerSide) outAppliedLerSide = lerSide;
                if (sourceTarget > maxSourceTargetThisCall) maxSourceTargetThisCall = sourceTarget;
                anySorted = true;
                continue;
            }

            LogInfo("[StackSort] Precomputed layout for '" + section->name +
                    "' does not fit its live items; re-snapshotting, sorting synchronously");
            SortWorker::MarkSectionStale(section);
        }

        // Synchronous fallback
        std::vector<Packer::Item> packItems;
        std::vector<Item*> itemPtrs;

        for (size_t i = 0; i < sectionItems.size(); ++i)
        {
            const InventorySection::SectionItem& si = sectionItems[i];
            if (!si.item) continue;

            Packer::Item pi;
            pi.id        = (int)packItems.size();
            pi.w         = si.item->itemWidth;
            pi.h         = si.item->itemHeight;
            pi.canRotate = StackSort_CanRotate(si.item);
            // Sync fallback has no tier data; -1 on every tier disables grouping.
            pi.exactId       = -1;
            pi.customGroupId = -1;
            pi.gameDataType  = -1;
            pi.itemFunction  = -1;
            pi.flagsMask     = 0;
            packItems.push_back(pi);
            itemPtrs.push_back(si.item);
        }

        if (packItems.empty()) continue;

        LogInfo(std::string("[StackSort] Sync fallback [") + dimTag + "] target=" + IntToStr(clamped) + " for '" +
                section->name + "'" + " (" + IntToStr(gridW) + "x" + IntToStr(gridH) + ", " +
                IntToStr((int)packItems.size()) + " items)");

        // Tight-budget PackAnnealed so the sync path honors pre-reservation
        // for target>1. Main-thread cost: ~tens of ms on typical inventories.
        Packer::SearchParams syncParams = Packer::SearchParams::defaults();
        syncParams.numRestarts          = StackSortSettingsStore::Get().syncRestarts;
        syncParams.itersPerRestart      = StackSortSettingsStore::Get().syncIters;

        QueryPerformanceCounter(&t0);
        Packer::GridSpec dims;
        dims.gridW            = gridW;
        dims.gridH            = gridH;
        dims.target           = clamped;
        Packer::Result result = Packer::Search::PackAnnealed(dims, packItems, dim, NULL, NULL, NULL, 0, &syncParams);
        QueryPerformanceCounter(&t1);

        double ms = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart * 1000.0;

        if (!result.allPlaced)
        {
            LogInfo("[StackSort] Not all items placed (" + IntToStr((int)result.placements.size()) + "/" +
                    IntToStr((int)packItems.size()) + "), falling back to vanilla");
            GuardedAutoArrange(section);
            markedThisCall.push_back(section);
            MarkIfUncaptured(section);
            anySorted = true;
            continue;
        }

        if (!Packer::Grid::ValidateLayout(placeW, placeH, packItems, result.placements))
        {
            LogError("[StackSort] Placement validation failed, vanilla fallback");
            GuardedAutoArrange(section);
            markedThisCall.push_back(section);
            MarkIfUncaptured(section);
            anySorted = true;
            continue;
        }

        if (!LiveItemsMatch(section, itemPtrs, result.placements))
        {
            LogError("[StackSort] Items of '" + section->name + "' changed during the sort; left as is");
            continue;
        }

        std::stringstream logMsg;
        logMsg << "[StackSort] Packed " << packItems.size() << " items in " << std::fixed;
        logMsg.precision(2);
        logMsg << ms << "ms"
               << ", LER " << result.lerWidth << "x" << result.lerHeight << "=" << result.lerArea << " (was "
               << beforeLerW << "x" << beforeLerH << "=" << beforeLerArea << ")"
               << ", score " << result.score;
        LogDebug(logMsg.str());

        bool applied = applyLayout(section, result.placements, itemPtrs);
        markedThisCall.push_back(section);
        MarkIfUncaptured(section);
        if (!applied) continue;
        int lerSide = (dim == Packer::TARGET_H) ? result.lerHeight : result.lerWidth;
        if (lerSide > outAppliedLerSide) outAppliedLerSide = lerSide;
        // Sync fallback has no source slot — leave outSourceTarget at the
        // current target so the W-mode skip is a no-op.
        if (clamped > maxSourceTargetThisCall) maxSourceTargetThisCall = clamped;
        anySorted = true;
    }

    // HandBackToInventory runs with the guards released, so its add callback
    // drops the capture and mark of every section of this inventory, including
    // siblings already sorted earlier in this loop. Re-mark them all.
    for (size_t i = 0; i < markedThisCall.size(); ++i)
        MarkIfUncaptured(markedThisCall[i]);

    if (anySorted)
    {
        StackSortUI::ForceRefreshAllSections(gui);
        StackSort_RefreshVisuals(gui);
    }

    outSourceTarget = maxSourceTargetThisCall;
}

bool StackSortUI::ConsolidateStacks(InventorySection* section)
{
    if (!StackSort_EligibleSection(section)) return false;
    StackMerge::Outcome outcome;
    {
        ApplyGuard applyGuard;
        CallbackGuard guard(section);
        NativeMergeAdapter native(section);
        outcome = StackMerge::Consolidate(native);
        section->recalculateTotalWeight();
    }
    if (outcome.changed) SortWorker::MarkSectionStale(section);
    if (outcome.fault) throw std::runtime_error("StackSort native consolidation fault");
    return outcome.changed;
}
// Called on first click (currentTarget==0), after consolidation but before
// ApplySort. Captures the human-arranged layout for right-click revert.
// Per-inventory scope: only captures sections of gui->_NV_getInventory().
void StackSortUI::CaptureOriginalPositions(InventoryGUI* gui)
{
    // Without the mutation hooks nothing drops a capture when the player
    // rearranges by hand, so it could describe any earlier layout.
    if (!SortWorker::IsMutationTrackingAvailable()) return;

    Inventory* inv = gui->_NV_getInventory();
    if (!inv) return;

    lektor<InventorySection*>& sections = inv->getAllSections();
    for (lektor<InventorySection*>::iterator it = sections.begin(); it != sections.end(); ++it)
    {
        InventorySection* section = *it;
        if (!StackSort_EligibleSection(section)) continue;
        if (!section->enabled) continue;
        if (section->getItems().empty()) continue;

        // On screen is StackSort's layout, not the player's (MarkIfUncaptured).
        if (ResultCache::IsSortedWithoutCapture(section)) continue;

        CachedSection* cs = ResultCache::Find(section);
        if (!cs || cs->hasOriginalPositions) continue;

        const Ogre::vector<InventorySection::SectionItem>::type& sectionItems = section->getItems();
        cs->originalPositions.clear();
        cs->originalPositions.reserve(sectionItems.size());
        for (size_t i = 0; i < sectionItems.size(); ++i)
        {
            const InventorySection::SectionItem& si = sectionItems[i];
            if (!si.item) continue;
            OriginalPlacement op;
            SortWorker::ReadItemIdentity(si.item, op.identity);
            op.x = si.x;
            op.y = si.y;
            op.w = si.item->itemWidth;
            op.h = si.item->itemHeight;
            cs->originalPositions.push_back(op);
        }
        cs->hasOriginalPositions = true;
    }
}

// Puts section back into its captured hand layout in one guarded apply that
// removes and re-places every live item: items matching their capture (same
// identity, same exact dims) return to their cells, every other item goes to
// a free cell around them, largest first. Everything is validated before the
// first removeItem; when the items added since cannot all fit, vanilla
// autoArrange runs instead so nothing leaves the section. Returns true when
// the section was changed.
static bool RevertSection(InventorySection* section, const std::vector<OriginalPlacement>& originals)
{
    const int placeW = PlaceableWidth(section);
    const int placeH = PlaceableHeight(section);

    std::vector<Packer::Placement> placements;
    std::vector<Item*> itemPtrs;
    std::vector<Item*> stragglers;
    std::vector<unsigned char> used(originals.size(), 0);

    const Ogre::vector<InventorySection::SectionItem>::type& items = section->getItems();
    for (size_t i = 0; i < items.size(); ++i)
    {
        Item* item = items[i].item;
        if (!item) continue;

        ItemIdentity live;
        SortWorker::ReadItemIdentity(item, live);

        int match = -1;
        for (size_t k = 0; k < originals.size(); ++k)
        {
            const OriginalPlacement& op = originals[k];
            if (used[k] || op.identity != live) continue;
            if (op.w != item->itemWidth || op.h != item->itemHeight) continue;
            match = (int)k;
            break;
        }
        if (match < 0)
        {
            stragglers.push_back(item);
            continue;
        }

        used[match]                 = 1;
        const OriginalPlacement& op = originals[match];
        placements.push_back(MakePlacement((int)itemPtrs.size(), op.x, op.y, op.w, op.h));
        itemPtrs.push_back(item);
    }

    if (placements.empty()) return false;

    // The originals at their live dims must still form a valid layout (the
    // section may have shrunk); nothing has been touched yet.
    if (!Packer::Grid::ValidatePlacements(placeW, placeH, placements))
    {
        LogInfo("[StackSort] Revert skipped for '" + section->name + "': its captured layout no longer fits");
        return false;
    }

    if (!stragglers.empty())
    {
        Occupancy occ(placeW, placeH);
        for (size_t i = 0; i < placements.size(); ++i)
            occ.Stamp(placements[i].x, placements[i].y, placements[i].w, placements[i].h);

        std::stable_sort(stragglers.begin(), stragglers.end(), LargerFootprint);
        for (size_t i = 0; i < stragglers.size(); ++i)
        {
            Item* item = stragglers[i];
            int x = 0, y = 0;
            if (!occ.FindFree(item->itemWidth, item->itemHeight, x, y))
            {
                LogInfo("[StackSort] Revert of '" + section->name +
                        "': no room for the items added since the sort; vanilla arrange instead");
                GuardedAutoArrange(section);
                return true;
            }
            occ.Stamp(x, y, item->itemWidth, item->itemHeight);
            placements.push_back(MakePlacement((int)itemPtrs.size(), x, y, item->itemWidth, item->itemHeight));
            itemPtrs.push_back(item);
        }
    }

    // Stragglers were fitted around the originals, so this only re-proves
    // the full plan before the first removeItem.
    if (!Packer::Grid::ValidatePlacements(placeW, placeH, placements) || !LiveItemsMatch(section, itemPtrs, placements))
    {
        LogError("[StackSort] Revert plan for '" + section->name + "' failed validation; left as is");
        return false;
    }

    return applyLayout(section, placements, itemPtrs);
}

// Revert all eligible sections under gui's inventory to their original
// pre-sort positions. Returns true if any sections were changed. A capture is
// consumed by the attempt whatever its outcome, so a capture that could not
// be applied is not retried and the next first click recaptures.
bool StackSortUI::RevertInventory(InventoryGUI* gui)
{
    // No capture is taken without mutation tracking (CaptureOriginalPositions);
    // this also keeps one from before the hooks failed from being applied.
    if (!SortWorker::IsMutationTrackingAvailable()) return false;

    Inventory* inv = gui->_NV_getInventory();
    if (!inv) return false;

    bool anySorted = false;

    lektor<InventorySection*>& sections = inv->getAllSections();
    for (lektor<InventorySection*>::iterator it = sections.begin(); it != sections.end(); ++it)
    {
        InventorySection* section = *it;
        if (!StackSort_EligibleSection(section)) continue;
        if (!section->enabled) continue;
        if (section->getItems().empty()) continue;

        CachedSection* cs = ResultCache::Find(section);
        if (!cs || !cs->hasOriginalPositions) continue;

        // Copied: ReportItemSetChanged inside the apply may clear the entry's
        // capture while this is still being read.
        std::vector<OriginalPlacement> originals(cs->originalPositions);

        if (RevertSection(section, originals)) anySorted = true;
        ResultCache::ClearOriginalPositions(section);
    }

    if (anySorted)
    {
        StackSortUI::ForceRefreshAllSections(gui);
        StackSort_RefreshVisuals(gui);
    }

    return anySorted;
}
