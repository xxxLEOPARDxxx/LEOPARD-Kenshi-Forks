#include "TraderDiagnostics.h"

#include "TraderCore.h"
#include "TraderInventoryBinding.h"
#include "TraderSearchPipeline.h"
#include "TraderSearchText.h"
#include "TraderSearchUi.h"
#include "TraderWindowDetection.h"

#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/Inventory.h>
#include <kenshi/Item.h>
#include <kenshi/PlayerInterface.h>

#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_Widget.h>

#include <algorithm>
#include <sstream>
#include <vector>

namespace
{
struct HoverProbeCandidate
{
    std::size_t index;
    MyGUI::Widget* widget;
    MyGUI::IntCoord currentCoord;
    MyGUI::IntCoord baseCoord;
    bool hasBaseCoord;
    bool hitCurrent;
    bool hitBase;
    bool hoveredMatches;
    int currentDistance;
    int baseDistance;
    int quantity;
    std::string label;
};

bool TryFindDiagnosticBaseCoord(MyGUI::Widget* widget, MyGUI::IntCoord* outCoord)
{
    if (widget == 0 || outCoord == 0)
    {
        return false;
    }

    SearchState& search = TraderState().search;
    for (std::size_t index = 0; index < search.g_entryBaseCoords.size(); ++index)
    {
        const TraderEntryBaseCoord& baseCoord = search.g_entryBaseCoords[index];
        if (baseCoord.widget != widget)
        {
            continue;
        }

        *outCoord = MyGUI::IntCoord(
            baseCoord.left,
            baseCoord.top,
            baseCoord.width,
            baseCoord.height);
        return true;
    }

    return false;
}

int PositiveMod(int value, int divisor)
{
    if (divisor <= 0)
    {
        return 0;
    }

    int result = value % divisor;
    if (result < 0)
    {
        result += divisor;
    }
    return result;
}

int ComputePositiveGcd(int left, int right)
{
    if (left < 0)
    {
        left = -left;
    }
    if (right < 0)
    {
        right = -right;
    }

    if (left == 0)
    {
        return right;
    }
    if (right == 0)
    {
        return left;
    }

    while (right != 0)
    {
        const int next = left % right;
        left = right;
        right = next;
    }
    return left;
}

void AccumulatePositiveGcd(int value, int* accumulator)
{
    if (accumulator == 0)
    {
        return;
    }

    if (value < 0)
    {
        value = -value;
    }
    if (value <= 0)
    {
        return;
    }

    if (*accumulator <= 0)
    {
        *accumulator = value;
        return;
    }

    *accumulator = ComputePositiveGcd(*accumulator, value);
}

bool CoordContainsPoint(const MyGUI::IntCoord& coord, const MyGUI::IntPoint& point)
{
    return point.left >= coord.left
        && point.top >= coord.top
        && point.left < coord.left + coord.width
        && point.top < coord.top + coord.height;
}

int DistanceFromPointToCoord(const MyGUI::IntCoord& coord, const MyGUI::IntPoint& point)
{
    int dx = 0;
    if (point.left < coord.left)
    {
        dx = coord.left - point.left;
    }
    else if (point.left >= coord.left + coord.width)
    {
        dx = point.left - (coord.left + coord.width - 1);
    }

    int dy = 0;
    if (point.top < coord.top)
    {
        dy = coord.top - point.top;
    }
    else if (point.top >= coord.top + coord.height)
    {
        dy = point.top - (coord.top + coord.height - 1);
    }

    return dx + dy;
}

bool IsSameOrDescendant(MyGUI::Widget* candidate, MyGUI::Widget* ancestor)
{
    if (candidate == 0 || ancestor == 0)
    {
        return false;
    }

    MyGUI::Widget* current = candidate;
    for (std::size_t depth = 0; current != 0 && depth < 24; ++depth)
    {
        if (current == ancestor)
        {
            return true;
        }
        current = current->getParent();
    }

    return false;
}

void CollectUniqueProbeAxisCoords(
    const std::vector<HoverProbeCandidate>& candidates,
    bool useBaseCoords,
    bool horizontal,
    std::vector<int>* outCoords)
{
    if (outCoords == 0)
    {
        return;
    }

    outCoords->clear();
    outCoords->reserve(candidates.size());
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        const MyGUI::IntCoord& coord = useBaseCoords
            ? candidates[index].baseCoord
            : candidates[index].currentCoord;
        outCoords->push_back(horizontal ? coord.left : coord.top);
    }

    std::sort(outCoords->begin(), outCoords->end());
    outCoords->erase(
        std::unique(outCoords->begin(), outCoords->end()),
        outCoords->end());
}

int ResolveProbeGridAxisStep(
    const std::vector<HoverProbeCandidate>& candidates,
    bool useBaseCoords,
    bool horizontal,
    int rootExtent)
{
    int step = 0;
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        const MyGUI::IntCoord& coord = useBaseCoords
            ? candidates[index].baseCoord
            : candidates[index].currentCoord;
        AccumulatePositiveGcd(horizontal ? coord.width : coord.height, &step);
    }

    std::vector<int> coords;
    CollectUniqueProbeAxisCoords(candidates, useBaseCoords, horizontal, &coords);
    for (std::size_t index = 1; index < coords.size(); ++index)
    {
        AccumulatePositiveGcd(coords[index] - coords[index - 1], &step);
    }

    if (step <= 0 && rootExtent > 0)
    {
        step = rootExtent;
    }
    return step;
}

int ResolveProbeGridAxisOrigin(
    const std::vector<HoverProbeCandidate>& candidates,
    bool useBaseCoords,
    bool horizontal,
    int step)
{
    bool found = false;
    int minCoord = 0;
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        const MyGUI::IntCoord& coord = useBaseCoords
            ? candidates[index].baseCoord
            : candidates[index].currentCoord;
        const int value = horizontal ? coord.left : coord.top;
        if (!found || value < minCoord)
        {
            minCoord = value;
            found = true;
        }
    }

    if (!found)
    {
        return 0;
    }
    if (step <= 0)
    {
        return minCoord;
    }

    return PositiveMod(minCoord, step);
}

bool TryResolveHoverProbeGridCell(
    MyGUI::Widget* entriesRoot,
    const std::vector<HoverProbeCandidate>& candidates,
    const MyGUI::IntPoint& mouseRelative,
    bool useBaseCoords,
    int* outCellX,
    int* outCellY,
    int* outCellWidth,
    int* outCellHeight,
    int* outOriginLeft,
    int* outOriginTop)
{
    if (entriesRoot == 0 || candidates.empty())
    {
        return false;
    }

    const int cellWidth =
        ResolveProbeGridAxisStep(candidates, useBaseCoords, true, entriesRoot->getWidth());
    const int cellHeight =
        ResolveProbeGridAxisStep(candidates, useBaseCoords, false, entriesRoot->getHeight());
    if (cellWidth <= 0 || cellHeight <= 0)
    {
        return false;
    }

    const int originLeft =
        ResolveProbeGridAxisOrigin(candidates, useBaseCoords, true, cellWidth);
    const int originTop =
        ResolveProbeGridAxisOrigin(candidates, useBaseCoords, false, cellHeight);
    if (mouseRelative.left < originLeft || mouseRelative.top < originTop)
    {
        return false;
    }

    if (outCellX != 0)
    {
        *outCellX = (mouseRelative.left - originLeft) / cellWidth;
    }
    if (outCellY != 0)
    {
        *outCellY = (mouseRelative.top - originTop) / cellHeight;
    }
    if (outCellWidth != 0)
    {
        *outCellWidth = cellWidth;
    }
    if (outCellHeight != 0)
    {
        *outCellHeight = cellHeight;
    }
    if (outOriginLeft != 0)
    {
        *outOriginLeft = originLeft;
    }
    if (outOriginTop != 0)
    {
        *outOriginTop = originTop;
    }

    return true;
}

std::string ResolveDiagnosticEntryLabel(
    std::size_t entryIndex,
    MyGUI::Widget* widget,
    const std::vector<std::string>& inventoryNameKeys)
{
    if (entryIndex < inventoryNameKeys.size() && !inventoryNameKeys[entryIndex].empty())
    {
        return inventoryNameKeys[entryIndex];
    }

    const std::string widgetSearchText = NormalizeSearchText(BuildItemSearchText(widget));
    if (!widgetSearchText.empty())
    {
        return widgetSearchText;
    }

    return SafeWidgetName(widget);
}

std::string SafeHandToStringForHoverProbe(const hand& value)
{
    return value.toString();
}

Item* TryGetHandItemSafeForHoverProbe(const hand& value)
{
    __try
    {
        if (value.isValid())
        {
            return value.getItem();
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }

    return 0;
}

RootObject* TryGetHandRootSafeForHoverProbe(const hand& value)
{
    __try
    {
        if (value.isValid())
        {
            return value.getRootObject();
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }

    return 0;
}

Item* TryGetRootObjectItemSafeForHoverProbe(RootObject* root)
{
    if (root == 0)
    {
        return 0;
    }

    __try
    {
        return root->getHandle().getItem();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

Inventory* TryGetItemInventorySafeForHoverProbe(Item* item)
{
    if (item == 0)
    {
        return 0;
    }

    __try
    {
        return item->getInventory();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

std::string FormatRootSummaryForHoverProbe(RootObject* root)
{
    std::stringstream line;
    line << " root_ptr=" << root
         << " root=\"" << TruncateForLog(RootObjectDisplayNameForLog(root), 72) << "\"";

    Inventory* inventory = TryGetRootObjectInventorySafe(root);
    line << " root_inventory_ptr=" << inventory
         << " root_inventory_items=" << InventoryItemCountForLog(inventory);
    return line.str();
}

std::string FormatItemSummaryForHoverProbe(Item* item)
{
    if (item == 0)
    {
        return " item_ptr=0";
    }

    std::stringstream line;
    Inventory* inventory = TryGetItemInventorySafeForHoverProbe(item);
    RootObject* owner = 0;
    RootObject* callbackObject = 0;
    TryGetInventoryOwnerPointersSafe(inventory, &owner, &callbackObject);

    line << " item_ptr=" << item
         << " item_name=\"" << TruncateForLog(ResolveCanonicalItemName(item), 72) << "\""
         << " item_handle=\"" << TruncateForLog(SafeHandToStringForHoverProbe(item->getHandle()), 96) << "\""
         << " inv_pos=(" << item->inventoryPos.x << "," << item->inventoryPos.y << ")"
         << " size_cells=(" << item->itemWidth << "," << item->itemHeight << ")"
         << " section=\"" << TruncateForLog(item->inventorySection, 48) << "\""
         << " inventory_ptr=" << inventory
         << " inventory_items=" << InventoryItemCountForLog(inventory)
         << " inventory_owner=\"" << TruncateForLog(RootObjectDisplayNameForLog(owner), 72) << "\""
         << " inventory_callback=\"" << TruncateForLog(RootObjectDisplayNameForLog(callbackObject), 72) << "\"";
    return line.str();
}

bool DoesHoverProbeItemMatchCandidateLabel(Item* item, const HoverProbeCandidate* candidate)
{
    if (item == 0 || candidate == 0 || candidate->label.empty())
    {
        return false;
    }

    const std::string normalizedCandidate = NormalizeSearchText(candidate->label);
    const std::string normalizedItem = NormalizeSearchText(ResolveCanonicalItemName(item));
    return !normalizedCandidate.empty()
        && !normalizedItem.empty()
        && normalizedCandidate == normalizedItem;
}

const HoverProbeCandidate* FindBestHoverProbeCandidate(
    const std::vector<HoverProbeCandidate>& candidates,
    bool useBaseHit)
{
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        if ((useBaseHit && candidates[index].hitBase)
            || (!useBaseHit && candidates[index].hitCurrent))
        {
            return &candidates[index];
        }
    }

    return candidates.empty() ? 0 : &candidates[0];
}

Item* FindInventoryItemAtCellForHoverProbe(
    Inventory* inventory,
    int cellX,
    int cellY,
    int* outMatchCount)
{
    if (outMatchCount != 0)
    {
        *outMatchCount = 0;
    }

    if (inventory == 0)
    {
        return 0;
    }

    __try
    {
        Item* matched = 0;
        const lektor<Item*>& allItems = inventory->getAllItems();
        for (lektor<Item*>::const_iterator iter = allItems.begin(); iter != allItems.end(); ++iter)
        {
            Item* item = *iter;
            if (item == 0)
            {
                continue;
            }

            const int itemLeft = item->inventoryPos.x;
            const int itemTop = item->inventoryPos.y;
            const int itemWidth = item->itemWidth > 0 ? item->itemWidth : 1;
            const int itemHeight = item->itemHeight > 0 ? item->itemHeight : 1;
            if (cellX < itemLeft
                || cellY < itemTop
                || cellX >= itemLeft + itemWidth
                || cellY >= itemTop + itemHeight)
            {
                continue;
            }

            matched = item;
            if (outMatchCount != 0)
            {
                ++(*outMatchCount);
            }
        }

        return matched;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

void LogHoverEngineState(
    const char* label,
    const hand* handle,
    RootObject* directRoot,
    const HoverProbeCandidate* bestCurrentCandidate,
    const HoverProbeCandidate* bestBaseCandidate,
    Inventory* boundInventory)
{
    RootObject* root = directRoot;
    Item* item = 0;
    std::string handleString = "<none>";
    if (handle != 0)
    {
        handleString = SafeHandToStringForHoverProbe(*handle);
        if (root == 0)
        {
            root = TryGetHandRootSafeForHoverProbe(*handle);
        }
        item = TryGetHandItemSafeForHoverProbe(*handle);
    }
    if (item == 0 && root != 0)
    {
        item = TryGetRootObjectItemSafeForHoverProbe(root);
    }

    const Inventory* itemInventory = TryGetItemInventorySafeForHoverProbe(item);
    std::stringstream line;
    line << "[investigate][sort-hover] engine_state"
         << " label=" << (label == 0 ? "<unknown>" : label)
         << " handle=\"" << TruncateForLog(handleString, 96) << "\""
         << FormatRootSummaryForHoverProbe(root)
         << " in_bound_inventory=" << ((boundInventory != 0 && itemInventory == boundInventory) ? "true" : "false")
         << " matches_current=" << (DoesHoverProbeItemMatchCandidateLabel(item, bestCurrentCandidate) ? "true" : "false")
         << " matches_base=" << (DoesHoverProbeItemMatchCandidateLabel(item, bestBaseCandidate) ? "true" : "false")
         << FormatItemSummaryForHoverProbe(item);
    LogWarnLine(line.str());
}

void LogSortHoverProbe(
    MyGUI::Widget* traderParent,
    MyGUI::Widget* entriesRoot,
    const std::vector<std::string>& inventoryNameKeys)
{
    if (traderParent == 0 || entriesRoot == 0)
    {
        return;
    }

    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        LogWarnLine("[investigate][sort-hover] input_manager_missing=true");
        return;
    }

    MyGUI::Widget* hovered = inputManager->getMouseFocusWidget();
    const MyGUI::IntPoint mouseAbsolute = inputManager->getMousePosition();
    const MyGUI::IntCoord entriesRootAbsolute = entriesRoot->getAbsoluteCoord();
    const MyGUI::IntPoint mouseRelative(
        mouseAbsolute.left - entriesRootAbsolute.left,
        mouseAbsolute.top - entriesRootAbsolute.top);

    std::stringstream header;
    header << "[investigate][sort-hover] snapshot"
           << " sort_mode="
           << TraderSortStateLabel(
               TraderState().search.g_sortMode,
               TraderState().search.g_sortDirection,
               TraderState().search.g_sortPriceMode)
           << " query=\"" << TraderState().search.g_searchQueryNormalized << "\""
           << " hovered=" << SafeWidgetName(hovered)
           << " hovered_chain=\"" << TruncateForLog(BuildParentChainForLog(hovered), 220) << "\""
           << " mouse_abs=(" << mouseAbsolute.left << "," << mouseAbsolute.top << ")"
           << " mouse_rel=(" << mouseRelative.left << "," << mouseRelative.top << ")"
           << " entries_root=" << SafeWidgetName(entriesRoot)
           << " entries_root_abs=(" << entriesRootAbsolute.left << "," << entriesRootAbsolute.top << ","
           << entriesRootAbsolute.width << "," << entriesRootAbsolute.height << ")"
           << " base_coord_cache_size=" << TraderState().search.g_entryBaseCoords.size();
    LogWarnLine(header.str());

    if (hovered != 0)
    {
        std::stringstream hoveredLine;
        hoveredLine << "[investigate][sort-hover] hovered_widget"
                    << " search_text=\"" << TruncateForLog(BuildItemSearchText(hovered), 120) << "\""
                    << " raw_probe=\"" << TruncateForLog(BuildItemRawProbe(hovered), 120) << "\"";
        LogWarnLine(hoveredLine.str());
    }

    std::vector<HoverProbeCandidate> candidates;
    const std::size_t childCount = entriesRoot->getChildCount();
    candidates.reserve(childCount);
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        MyGUI::Widget* child = entriesRoot->getChildAt(childIndex);
        if (child == 0)
        {
            continue;
        }

        HoverProbeCandidate candidate;
        candidate.index = childIndex;
        candidate.widget = child;
        candidate.currentCoord = child->getCoord();
        candidate.baseCoord = candidate.currentCoord;
        candidate.hasBaseCoord = TryFindDiagnosticBaseCoord(child, &candidate.baseCoord);
        candidate.hitCurrent = CoordContainsPoint(candidate.currentCoord, mouseRelative);
        candidate.hitBase = candidate.hasBaseCoord && CoordContainsPoint(candidate.baseCoord, mouseRelative);
        candidate.hoveredMatches = IsSameOrDescendant(hovered, child);
        candidate.currentDistance = DistanceFromPointToCoord(candidate.currentCoord, mouseRelative);
        candidate.baseDistance = candidate.hasBaseCoord
            ? DistanceFromPointToCoord(candidate.baseCoord, mouseRelative)
            : candidate.currentDistance;
        candidate.quantity = 0;
        TryResolveItemQuantityFromWidget(child, &candidate.quantity);
        candidate.label = ResolveDiagnosticEntryLabel(childIndex, child, inventoryNameKeys);
        candidates.push_back(candidate);
    }

    struct ProbeCandidateSorter
    {
        bool operator()(const HoverProbeCandidate& left, const HoverProbeCandidate& right) const
        {
            if (left.hoveredMatches != right.hoveredMatches)
            {
                return left.hoveredMatches;
            }
            if (left.hitCurrent != right.hitCurrent)
            {
                return left.hitCurrent;
            }
            if (left.hitBase != right.hitBase)
            {
                return left.hitBase;
            }
            if (left.currentDistance != right.currentDistance)
            {
                return left.currentDistance < right.currentDistance;
            }
            if (left.baseDistance != right.baseDistance)
            {
                return left.baseDistance < right.baseDistance;
            }
            return left.index < right.index;
        }
    };
    std::sort(candidates.begin(), candidates.end(), ProbeCandidateSorter());

    std::size_t hitCurrentCount = 0;
    std::size_t hitBaseCount = 0;
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        if (candidates[index].hitCurrent)
        {
            ++hitCurrentCount;
        }
        if (candidates[index].hitBase)
        {
            ++hitBaseCount;
        }
    }

    std::stringstream summary;
    summary << "[investigate][sort-hover] candidate_summary"
            << " hit_current=" << hitCurrentCount
            << " hit_base=" << hitBaseCount
            << " child_count=" << candidates.size();
    LogWarnLine(summary.str());

    const HoverProbeCandidate* bestCurrentCandidate =
        FindBestHoverProbeCandidate(candidates, false);
    const HoverProbeCandidate* bestBaseCandidate =
        FindBestHoverProbeCandidate(candidates, true);

    {
        std::stringstream line;
        line << "[investigate][sort-hover] hit_labels"
             << " current_index=" << (bestCurrentCandidate == 0 ? -1 : static_cast<int>(bestCurrentCandidate->index))
             << " current_label=\"" << TruncateForLog(bestCurrentCandidate == 0 ? "" : bestCurrentCandidate->label, 96) << "\""
             << " base_index=" << (bestBaseCandidate == 0 ? -1 : static_cast<int>(bestBaseCandidate->index))
             << " base_label=\"" << TruncateForLog(bestBaseCandidate == 0 ? "" : bestBaseCandidate->label, 96) << "\"";
        LogWarnLine(line.str());
    }

    TraderPanelInventoryBinding binding;
    Inventory* boundInventory = 0;
    bool hasBinding = TryGetTraderPanelInventoryBinding(
        traderParent,
        entriesRoot,
        candidates.size(),
        &binding);
    if (hasBinding && IsInventoryPointerValidSafe(binding.inventory))
    {
        boundInventory = binding.inventory;
    }

    {
        RootObject* owner = 0;
        RootObject* callbackObject = 0;
        TryGetInventoryOwnerPointersSafe(boundInventory, &owner, &callbackObject);

        std::stringstream line;
        line << "[investigate][sort-hover] panel_binding"
             << " resolved=" << (hasBinding ? "true" : "false")
             << " inventory_ptr=" << boundInventory
             << " items=" << InventoryItemCountForLog(boundInventory)
             << " owner=\"" << TruncateForLog(RootObjectDisplayNameForLog(owner), 72) << "\""
             << " callback=\"" << TruncateForLog(RootObjectDisplayNameForLog(callbackObject), 72) << "\""
             << " stage=\"" << TruncateForLog(hasBinding ? binding.stage : "", 48) << "\""
             << " source=\"" << TruncateForLog(hasBinding ? binding.source : "", 160) << "\"";
        LogWarnLine(line.str());
    }

    if (boundInventory != 0)
    {
        int baseCellX = -1;
        int baseCellY = -1;
        int cellWidth = 0;
        int cellHeight = 0;
        int originLeft = 0;
        int originTop = 0;
        const bool hasBaseCell = TryResolveHoverProbeGridCell(
            entriesRoot,
            candidates,
            mouseRelative,
            true,
            &baseCellX,
            &baseCellY,
            &cellWidth,
            &cellHeight,
            &originLeft,
            &originTop);

        int boundBaseMatchCount = 0;
        Item* boundBaseItem = hasBaseCell
            ? FindInventoryItemAtCellForHoverProbe(
                boundInventory,
                baseCellX,
                baseCellY,
                &boundBaseMatchCount)
            : 0;

        std::stringstream line;
        line << "[investigate][sort-hover] bound_slot"
             << " has_base_cell=" << (hasBaseCell ? "true" : "false")
             << " base_cell=(" << baseCellX << "," << baseCellY << ")"
             << " grid_cell=(" << cellWidth << "," << cellHeight << ")"
             << " grid_origin=(" << originLeft << "," << originTop << ")"
             << " match_count=" << boundBaseMatchCount
             << " matches_current=" << (DoesHoverProbeItemMatchCandidateLabel(boundBaseItem, bestCurrentCandidate) ? "true" : "false")
             << " matches_base=" << (DoesHoverProbeItemMatchCandidateLabel(boundBaseItem, bestBaseCandidate) ? "true" : "false")
             << FormatItemSummaryForHoverProbe(boundBaseItem);
        LogWarnLine(line.str());
    }

    if (ou != 0 && ou->player != 0)
    {
        LogHoverEngineState(
            "gui_display_object",
            &ou->guiDisplayObject,
            0,
            bestCurrentCandidate,
            bestBaseCandidate,
            boundInventory);
        LogHoverEngineState(
            "selected_object",
            &ou->player->selectedObject,
            0,
            bestCurrentCandidate,
            bestBaseCandidate,
            boundInventory);
        LogHoverEngineState(
            "mouse_target",
            0,
            ou->player->mouseRightTarget,
            bestCurrentCandidate,
            bestBaseCandidate,
            boundInventory);
    }

    const std::size_t limit = candidates.size() < 8 ? candidates.size() : 8;
    for (std::size_t index = 0; index < limit; ++index)
    {
        const HoverProbeCandidate& candidate = candidates[index];
        std::stringstream line;
        line << "[investigate][sort-hover] candidate"
             << " rank=" << index
             << " index=" << candidate.index
             << " widget=" << SafeWidgetName(candidate.widget)
             << " hovered_match=" << (candidate.hoveredMatches ? "true" : "false")
             << " hit_current=" << (candidate.hitCurrent ? "true" : "false")
             << " hit_base=" << (candidate.hitBase ? "true" : "false")
             << " dist_current=" << candidate.currentDistance
             << " dist_base=" << candidate.baseDistance
             << " qty=" << candidate.quantity
             << " current=(" << candidate.currentCoord.left << "," << candidate.currentCoord.top << ","
             << candidate.currentCoord.width << "," << candidate.currentCoord.height << ")"
             << " base=(" << candidate.baseCoord.left << "," << candidate.baseCoord.top << ","
             << candidate.baseCoord.width << "," << candidate.baseCoord.height << ")"
             << " label=\"" << TruncateForLog(candidate.label, 96) << "\"";
        LogWarnLine(line.str());
    }
}

void LogRecentRefreshedInventorySummary(std::size_t expectedEntryCount)
{
    TraderRuntimeState& state = TraderState();

    PruneRecentlyRefreshedInventories();
    if (state.binding.g_recentRefreshedInventories.empty())
    {
        LogWarnLine("recent refresh inventory summary empty");
        return;
    }

    std::vector<RefreshedInventoryLink> sorted = state.binding.g_recentRefreshedInventories;
    struct RecentInventorySorter
    {
        bool operator()(const RefreshedInventoryLink& left, const RefreshedInventoryLink& right) const
        {
            if (left.lastSeenTick != right.lastSeenTick)
            {
                return left.lastSeenTick > right.lastSeenTick;
            }
            if (left.ownerTrader != right.ownerTrader)
            {
                return left.ownerTrader;
            }
            if (left.visible != right.visible)
            {
                return left.visible;
            }
            return left.inventory < right.inventory;
        }
    };
    std::sort(sorted.begin(), sorted.end(), RecentInventorySorter());

    std::stringstream summary;
    summary << "recent refresh inventory summary"
            << " expected_entries=" << expectedEntryCount
            << " tracked=" << sorted.size();
    LogWarnLine(summary.str());

    const std::size_t limit = sorted.size() < 14 ? sorted.size() : 14;
    for (std::size_t index = 0; index < limit; ++index)
    {
        const RefreshedInventoryLink& link = sorted[index];
        const unsigned long long ageTicks =
            state.core.g_updateTickCounter >= link.lastSeenTick
                ? state.core.g_updateTickCounter - link.lastSeenTick
                : 0ULL;
        std::vector<std::string> keys;
        const bool hasKeys = TryExtractSearchKeysFromInventory(link.inventory, &keys);
        std::stringstream line;
        line << "recent_refresh[" << index << "]"
             << " ptr=" << link.inventory
             << " owner=" << link.ownerName
             << " owner_trader=" << (link.ownerTrader ? "true" : "false")
             << " owner_selected=" << (link.ownerSelected ? "true" : "false")
             << " visible=" << (link.visible ? "true" : "false")
             << " items=" << link.itemCount
             << " age_ticks=" << ageTicks
             << " key_count=" << (hasKeys ? keys.size() : 0);
        if (hasKeys && !keys.empty())
        {
            line << " key0=\"" << TruncateForLog(keys[0], 48) << "\"";
        }
        LogWarnLine(line.str());
    }
}
}

void DumpOnDemandTraderDiagnosticsSnapshot()
{
    if (!ShouldLogBindingDebug())
    {
        LogWarnLine("manual diagnostics snapshot skipped: debugBindingLogging=false");
        return;
    }

    MyGUI::Widget* traderParent = ResolveTraderParentFromControlsContainer();
    if (traderParent == 0)
    {
        MyGUI::Widget* anchor = 0;
        MyGUI::Widget* parent = 0;
        if (TryResolveVisibleTraderTarget(&anchor, &parent) && parent != 0)
        {
            traderParent = parent;
        }
    }

    if (traderParent == 0)
    {
        LogWarnLine("manual diagnostics: could not resolve trader parent");
        LogInventoryBindingDiagnostics(0);
        return;
    }

    MyGUI::Widget* backpackContent = ResolveBestBackpackContentWidget(traderParent, true);
    if (backpackContent == 0)
    {
        backpackContent = FindWidgetInParentByToken(traderParent, "backpack_content");
    }
    MyGUI::Widget* entriesRoot =
        backpackContent == 0 ? 0 : ResolveInventoryEntriesRoot(backpackContent);

    if (entriesRoot == 0)
    {
        std::stringstream line;
        line << "manual diagnostics: entries root missing"
             << " parent=" << SafeWidgetName(traderParent)
             << " backpack=" << SafeWidgetName(backpackContent);
        LogWarnLine(line.str());
        LogInventoryBindingDiagnostics(0);
        return;
    }

    struct OrderedEntry
    {
        MyGUI::Widget* widget;
        MyGUI::IntCoord coord;
        int quantity;
    };

    std::vector<OrderedEntry> orderedEntries;
    const std::size_t childCount = entriesRoot->getChildCount();
    orderedEntries.reserve(childCount);
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        MyGUI::Widget* child = entriesRoot->getChildAt(childIndex);
        if (child == 0)
        {
            continue;
        }

        OrderedEntry entry;
        entry.widget = child;
        entry.coord = child->getCoord();
        entry.quantity = 0;
        TryResolveItemQuantityFromWidget(child, &entry.quantity);
        orderedEntries.push_back(entry);
    }

    struct OrderedEntryCoordLess
    {
        bool operator()(const OrderedEntry& left, const OrderedEntry& right) const
        {
            if (left.coord.top != right.coord.top)
            {
                return left.coord.top < right.coord.top;
            }
            if (left.coord.left != right.coord.left)
            {
                return left.coord.left < right.coord.left;
            }
            return left.widget < right.widget;
        }
    };
    std::sort(orderedEntries.begin(), orderedEntries.end(), OrderedEntryCoordLess());

    std::vector<int> uiQuantities;
    uiQuantities.reserve(orderedEntries.size());
    std::size_t occupiedCount = 0;
    std::stringstream quantityPreview;
    for (std::size_t index = 0; index < orderedEntries.size(); ++index)
    {
        const int quantity = orderedEntries[index].quantity;
        uiQuantities.push_back(quantity);
        if (quantity > 0)
        {
            ++occupiedCount;
        }
        if (index < 20)
        {
            if (index > 0)
            {
                quantityPreview << ",";
            }
            quantityPreview << quantity;
        }
    }
    if (orderedEntries.size() > 20)
    {
        quantityPreview << ",...";
    }

    {
        std::stringstream line;
        line << "manual diagnostics snapshot"
             << " parent=" << SafeWidgetName(traderParent)
             << " entries_root=" << SafeWidgetName(entriesRoot)
             << " total_entries=" << orderedEntries.size()
             << " occupied_entries=" << occupiedCount
             << " quantities=[" << quantityPreview.str() << "]";
        LogWarnLine(line.str());
    }
    LogRecentRefreshedInventorySummary(orderedEntries.size());

    TraderRuntimeState& state = TraderState();
    state.search.g_lastInventoryKeysetSelectionSignature.clear();
    state.search.g_lastInventoryKeysetLowCoverageSignature.clear();
    state.search.g_lastCoverageFallbackDecisionSignature.clear();

    std::vector<std::string> inventoryNameKeys;
    std::vector<QuantityNameKey> inventoryQuantityNameKeys;
    std::string inventorySource;
    const bool resolved = TryResolveTraderInventoryNameKeys(
        traderParent,
        orderedEntries.size(),
        &uiQuantities,
        &inventoryNameKeys,
        &inventorySource,
        &inventoryQuantityNameKeys,
        false);

    std::stringstream result;
    result << "manual diagnostics keyset"
           << " resolved=" << (resolved ? "true" : "false")
           << " key_count=" << inventoryNameKeys.size()
           << " non_empty=" << CountNonEmptyKeys(inventoryNameKeys)
           << " quantity_key_count=" << inventoryQuantityNameKeys.size()
           << " source=\"" << TruncateForLog(inventorySource, 220) << "\"";
    LogWarnLine(result.str());

    if (resolved && !inventoryNameKeys.empty())
    {
        std::stringstream preview;
        preview << "manual diagnostics key preview " << BuildKeyPreviewForLog(inventoryNameKeys, 20);
        LogWarnLine(preview.str());
    }

    LogSortHoverProbe(traderParent, entriesRoot, inventoryNameKeys);

    LogSearchSampleForQuery(entriesRoot, state.search.g_searchQueryNormalized, 12);

    LogInventoryBindingDiagnostics(orderedEntries.size());
}
