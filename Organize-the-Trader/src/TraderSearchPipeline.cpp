#include "TraderSearchPipeline.h"

#include "TraderCore.h"
#include "TraderInventoryBinding.h"
#include "TraderSearchText.h"
#include "TraderSearchUi.h"
#include "TraderWindowDetection.h"

#include <kenshi/Inventory.h>
#include <kenshi/Item.h>

#include <mygui/MyGUI_Widget.h>

#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>
#include <vector>

#define g_searchFilterDirty (TraderState().search.g_searchFilterDirty)
#define g_loggedMissingBackpackForSearch (TraderState().search.g_loggedMissingBackpackForSearch)
#define g_loggedMissingSearchableItemText (TraderState().search.g_loggedMissingSearchableItemText)
#define g_searchQueryRaw (TraderState().search.g_searchQueryRaw)
#define g_searchQueryNormalized (TraderState().search.g_searchQueryNormalized)
#define g_sortMode (TraderState().search.g_sortMode)
#define g_sortDirection (TraderState().search.g_sortDirection)
#define g_lastZeroMatchQueryLogged (TraderState().search.g_lastZeroMatchQueryLogged)
#define g_lastObservedTraderEntriesStateSignature (TraderState().search.g_lastObservedTraderEntriesStateSignature)
#define g_lastZeroMatchGuardSignature (TraderState().search.g_lastZeroMatchGuardSignature)
#define g_lastSearchSampleQueryLogged (TraderState().search.g_lastSearchSampleQueryLogged)
#define g_lastSortInvestigationSignature (TraderState().search.g_lastSortInvestigationSignature)
#define g_expectedSortedInventoryLayoutSignature (TraderState().search.g_expectedSortedInventoryLayoutSignature)
#define g_sortedEntriesRoot (TraderState().search.g_sortedEntriesRoot)
#define g_entryBaseCoords (TraderState().search.g_entryBaseCoords)
#define g_sortedInventory (TraderState().search.g_sortedInventory)
#define g_sortedInventoryBasePositions (TraderState().search.g_sortedInventoryBasePositions)

#define g_loggedInventoryBindingFailure (TraderState().binding.g_loggedInventoryBindingFailure)
#define g_loggedInventoryBindingDiagnostics (TraderState().binding.g_loggedInventoryBindingDiagnostics)
#define g_lastPanelBindingRefusedSignature (TraderState().binding.g_lastPanelBindingRefusedSignature)

namespace
{
struct OrderedEntry
{
    MyGUI::Widget* widget;
    MyGUI::IntCoord coord;
    int quantity;
    int widthCells;
    int heightCells;
};

struct SortedGridMetrics
{
    SortedGridMetrics()
        : cellWidth(0)
        , cellHeight(0)
        , originLeft(0)
        , originTop(0)
        , columns(0)
        , rows(0)
    {
    }

    int cellWidth;
    int cellHeight;
    int originLeft;
    int originTop;
    int columns;
    int rows;
};

struct InventoryLayoutSignatureItem
{
    Item* item;
    std::string sectionName;
    int leftCell;
    int topCell;
    int widthCells;
    int heightCells;
    int quantity;
};

struct InventoryLayoutSignatureItemLess
{
    bool operator()(
        const InventoryLayoutSignatureItem& left,
        const InventoryLayoutSignatureItem& right) const
    {
        return left.item < right.item;
    }
};

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

const int kSortedShelfGapRows = 1;

bool ApplySortedInventoryTargetCells(
    Inventory* inventory,
    const std::vector<Item*>& orderedItems,
    const std::vector<MyGUI::IntCoord>& targetCoords,
    const SortedGridMetrics& gridMetrics,
    std::string* outFailureReason);

void ResetBaseEntryCoords()
{
    g_sortedEntriesRoot = 0;
    g_entryBaseCoords.clear();
}

MyGUI::IntCoord BuildBaseCoord(const TraderEntryBaseCoord& baseCoord)
{
    return MyGUI::IntCoord(
        baseCoord.left,
        baseCoord.top,
        baseCoord.width,
        baseCoord.height);
}

bool TryFindBaseCoord(MyGUI::Widget* widget, MyGUI::IntCoord* outCoord)
{
    if (widget == 0 || outCoord == 0)
    {
        return false;
    }

    for (std::size_t index = 0; index < g_entryBaseCoords.size(); ++index)
    {
        const TraderEntryBaseCoord& baseCoord = g_entryBaseCoords[index];
        if (baseCoord.widget != widget)
        {
            continue;
        }

        *outCoord = BuildBaseCoord(baseCoord);
        return true;
    }

    return false;
}

void CaptureBaseEntryCoords(MyGUI::Widget* entriesRoot, const std::vector<OrderedEntry>& orderedEntries)
{
    g_sortedEntriesRoot = entriesRoot;
    g_entryBaseCoords.clear();
    g_entryBaseCoords.reserve(orderedEntries.size());

    for (std::size_t index = 0; index < orderedEntries.size(); ++index)
    {
        TraderEntryBaseCoord baseCoord;
        baseCoord.widget = orderedEntries[index].widget;
        baseCoord.left = orderedEntries[index].coord.left;
        baseCoord.top = orderedEntries[index].coord.top;
        baseCoord.width = orderedEntries[index].coord.width;
        baseCoord.height = orderedEntries[index].coord.height;
        g_entryBaseCoords.push_back(baseCoord);
    }
}

void EnsureBaseEntryCoords(MyGUI::Widget* entriesRoot, std::vector<OrderedEntry>* orderedEntries)
{
    if (entriesRoot == 0 || orderedEntries == 0)
    {
        ResetBaseEntryCoords();
        return;
    }

    bool shouldCapture = g_sortedEntriesRoot != entriesRoot
        || g_entryBaseCoords.size() != orderedEntries->size();
    if (!shouldCapture)
    {
        for (std::size_t index = 0; index < orderedEntries->size(); ++index)
        {
            MyGUI::IntCoord baseCoord;
            if (!TryFindBaseCoord((*orderedEntries)[index].widget, &baseCoord))
            {
                shouldCapture = true;
                break;
            }
            (*orderedEntries)[index].coord = baseCoord;
        }
    }

    if (shouldCapture)
    {
        std::sort(orderedEntries->begin(), orderedEntries->end(), OrderedEntryCoordLess());
        CaptureBaseEntryCoords(entriesRoot, *orderedEntries);
    }
    else
    {
        std::sort(orderedEntries->begin(), orderedEntries->end(), OrderedEntryCoordLess());
    }
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

void CollectUniqueAxisCoords(
    const std::vector<OrderedEntry>& orderedEntries,
    bool horizontal,
    std::vector<int>* outCoords)
{
    if (outCoords == 0)
    {
        return;
    }

    outCoords->clear();
    outCoords->reserve(orderedEntries.size());
    for (std::size_t index = 0; index < orderedEntries.size(); ++index)
    {
        outCoords->push_back(horizontal ? orderedEntries[index].coord.left : orderedEntries[index].coord.top);
    }

    std::sort(outCoords->begin(), outCoords->end());
    outCoords->erase(
        std::unique(outCoords->begin(), outCoords->end()),
        outCoords->end());
}

int ResolveGridAxisStep(
    const std::vector<OrderedEntry>& orderedEntries,
    bool horizontal,
    int rootExtent)
{
    int step = 0;
    for (std::size_t index = 0; index < orderedEntries.size(); ++index)
    {
        const OrderedEntry& entry = orderedEntries[index];
        const int spanPixels = horizontal ? entry.coord.width : entry.coord.height;
        const int spanCells = horizontal ? entry.widthCells : entry.heightCells;
        if (spanPixels > 0 && spanCells > 0 && spanPixels >= spanCells)
        {
            AccumulatePositiveGcd(spanPixels / spanCells, &step);
        }
    }

    if (step > 0)
    {
        return step;
    }

    for (std::size_t index = 0; index < orderedEntries.size(); ++index)
    {
        const OrderedEntry& entry = orderedEntries[index];
        AccumulatePositiveGcd(horizontal ? entry.coord.width : entry.coord.height, &step);
    }

    std::vector<int> coords;
    CollectUniqueAxisCoords(orderedEntries, horizontal, &coords);
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

int ResolveGridAxisOrigin(const std::vector<OrderedEntry>& orderedEntries, bool horizontal, int step)
{
    if (orderedEntries.empty())
    {
        return 0;
    }

    const int firstCoord = horizontal ? orderedEntries[0].coord.left : orderedEntries[0].coord.top;
    if (step <= 0)
    {
        return firstCoord;
    }

    return PositiveMod(firstCoord, step);
}

int ResolveEntrySpanCells(int spanPixels, int explicitCells, int cellPixels)
{
    if (explicitCells > 0)
    {
        return explicitCells;
    }

    if (cellPixels <= 0 || spanPixels <= 0)
    {
        return 1;
    }

    return (spanPixels + cellPixels - 1) / cellPixels;
}

std::string FormatCoordForInvestigation(const MyGUI::IntCoord& coord)
{
    std::stringstream line;
    line << "("
         << coord.left << ","
         << coord.top << ","
         << coord.width << ","
         << coord.height << ")";
    return line.str();
}

std::string FormatItemPosForInvestigation(const Item* item)
{
    if (item == 0)
    {
        return "(-,-)";
    }

    std::stringstream line;
    line << "(" << item->inventoryPos.x << "," << item->inventoryPos.y << ")";
    return line.str();
}

std::string BuildInventoryLayoutSignature(Inventory* inventory)
{
    if (inventory == 0 || !IsInventoryPointerValidSafe(inventory))
    {
        return "";
    }

    const lektor<Item*>& allItems = inventory->getAllItems();
    if (!allItems.valid())
    {
        return "";
    }

    std::vector<InventoryLayoutSignatureItem> items;
    items.reserve(allItems.size());
    for (lektor<Item*>::const_iterator iter = allItems.begin(); iter != allItems.end(); ++iter)
    {
        Item* item = *iter;
        if (item == 0)
        {
            continue;
        }

        InventoryLayoutSignatureItem signatureItem;
        signatureItem.item = item;
        signatureItem.sectionName = item->inventorySection;
        signatureItem.leftCell = item->inventoryPos.x;
        signatureItem.topCell = item->inventoryPos.y;
        signatureItem.widthCells = item->itemWidth > 0 ? item->itemWidth : 1;
        signatureItem.heightCells = item->itemHeight > 0 ? item->itemHeight : 1;
        signatureItem.quantity = item->quantity;
        items.push_back(signatureItem);
    }

    std::sort(items.begin(), items.end(), InventoryLayoutSignatureItemLess());

    std::stringstream signature;
    signature << inventory << "|" << items.size();
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        const InventoryLayoutSignatureItem& item = items[index];
        signature << "|" << item.item
                  << ":" << item.sectionName
                  << ":" << item.leftCell << "," << item.topCell
                  << ":" << item.widthCells << "x" << item.heightCells
                  << ":" << item.quantity;
    }
    return signature.str();
}

bool TryResolveSortMetricForItem(Item* item, TraderSortMode mode, double* outMetric)
{
    if (outMetric == 0)
    {
        return false;
    }

    *outMetric = 0.0;
    if (item == 0)
    {
        return false;
    }

    switch (mode)
    {
    case TraderSortMode_UnitPrice:
        *outMetric = item->getValueSingle(false);
        return true;
    case TraderSortMode_StackValue:
        *outMetric = item->getValueAll(false);
        return true;
    case TraderSortMode_UnitWeight:
        *outMetric = item->getItemWeightSingle();
        return true;
    case TraderSortMode_StackWeight:
        *outMetric = item->getItemWeight();
        return true;
    case TraderSortMode_ValuePerWeight:
    {
        const double value = item->getValueAll(false);
        const double weight = item->getItemWeight();
        *outMetric = weight > 0.0
            ? (value / weight)
            : (value > 0.0 ? std::numeric_limits<double>::max() : 0.0);
        return true;
    }
    default:
        return false;
    }
}

bool SortModeUsesItemNamePrimary(TraderSortMode mode)
{
    return mode == TraderSortMode_Name;
}

std::string FormatSortMetricForLog(double value)
{
    if (value == std::numeric_limits<double>::max())
    {
        return "max";
    }

    std::stringstream line;
    line.setf(std::ios::fixed, std::ios::floatfield);
    line.precision(3);
    line << value;
    return line.str();
}

void LogInvestigateSortLine(const std::string& message)
{
    LogInfoLine(std::string("[investigate][sort] ") + message);
}

bool CoordsOverlap(const MyGUI::IntCoord& left, const MyGUI::IntCoord& right)
{
    return left.left < right.left + right.width
        && left.left + left.width > right.left
        && left.top < right.top + right.height
        && left.top + left.height > right.top;
}

bool CanPlaceEntryFootprint(
    const std::vector<char>& occupancy,
    int columns,
    int rows,
    int leftCell,
    int topCell,
    int widthCells,
    int heightCells)
{
    if (columns <= 0 || rows <= 0 || widthCells <= 0 || heightCells <= 0)
    {
        return false;
    }

    if (leftCell < 0
        || topCell < 0
        || leftCell + widthCells > columns
        || topCell + heightCells > rows)
    {
        return false;
    }

    for (int y = topCell; y < topCell + heightCells; ++y)
    {
        for (int x = leftCell; x < leftCell + widthCells; ++x)
        {
            const int index = (y * columns) + x;
            if (index < 0 || index >= static_cast<int>(occupancy.size()) || occupancy[index] != 0)
            {
                return false;
            }
        }
    }

    return true;
}

void FillEntryFootprint(
    std::vector<char>* occupancy,
    int columns,
    int leftCell,
    int topCell,
    int widthCells,
    int heightCells)
{
    if (occupancy == 0 || columns <= 0)
    {
        return;
    }

    for (int y = topCell; y < topCell + heightCells; ++y)
    {
        for (int x = leftCell; x < leftCell + widthCells; ++x)
        {
            const int index = (y * columns) + x;
            if (index < 0 || index >= static_cast<int>(occupancy->size()))
            {
                continue;
            }

            (*occupancy)[index] = 1;
        }
    }
}

bool TryResolveSortedGridMetrics(
    MyGUI::Widget* entriesRoot,
    const std::vector<OrderedEntry>& orderedEntries,
    SortedGridMetrics* outGridMetrics,
    std::string* outFailureReason)
{
    if (entriesRoot == 0 || outGridMetrics == 0)
    {
        if (outFailureReason != 0)
        {
            *outFailureReason = "missing_entries_root";
        }
        return false;
    }
    if (orderedEntries.empty())
    {
        if (outFailureReason != 0)
        {
            *outFailureReason = "empty_ordered_entries";
        }
        return false;
    }

    const int cellWidth =
        ResolveGridAxisStep(orderedEntries, true, entriesRoot->getWidth());
    const int cellHeight =
        ResolveGridAxisStep(orderedEntries, false, entriesRoot->getHeight());
    if (cellWidth <= 0 || cellHeight <= 0)
    {
        if (outFailureReason != 0)
        {
            *outFailureReason = "grid_step_unresolved";
        }
        return false;
    }

    const int originLeft = ResolveGridAxisOrigin(orderedEntries, true, cellWidth);
    const int originTop = ResolveGridAxisOrigin(orderedEntries, false, cellHeight);

    int columns = 0;
    int rows = 0;
    if (entriesRoot->getWidth() > originLeft)
    {
        columns = (entriesRoot->getWidth() - originLeft) / cellWidth;
    }
    if (entriesRoot->getHeight() > originTop)
    {
        rows = (entriesRoot->getHeight() - originTop) / cellHeight;
    }

    int maxRight = originLeft;
    int maxBottom = originTop;
    for (std::size_t index = 0; index < orderedEntries.size(); ++index)
    {
        const OrderedEntry& entry = orderedEntries[index];
        if (entry.coord.left + entry.coord.width > maxRight)
        {
            maxRight = entry.coord.left + entry.coord.width;
        }
        if (entry.coord.top + entry.coord.height > maxBottom)
        {
            maxBottom = entry.coord.top + entry.coord.height;
        }
    }

    const int minColumns = (maxRight - originLeft + cellWidth - 1) / cellWidth;
    const int minRows = (maxBottom - originTop + cellHeight - 1) / cellHeight;
    if (columns < minColumns)
    {
        columns = minColumns;
    }
    if (rows < minRows)
    {
        rows = minRows;
    }
    if (columns <= 0 || rows <= 0)
    {
        if (outFailureReason != 0)
        {
            *outFailureReason = "grid_dimensions_invalid";
        }
        return false;
    }

    outGridMetrics->cellWidth = cellWidth;
    outGridMetrics->cellHeight = cellHeight;
    outGridMetrics->originLeft = originLeft;
    outGridMetrics->originTop = originTop;
    outGridMetrics->columns = columns;
    outGridMetrics->rows = rows;
    if (outFailureReason != 0)
    {
        outFailureReason->clear();
    }
    return true;
}

bool TryBuildSortedTargetCoords(
    MyGUI::Widget* entriesRoot,
    const std::vector<OrderedEntry>& orderedEntries,
    const std::vector<std::size_t>& displayOrder,
    std::vector<MyGUI::IntCoord>* outTargetCoords,
    SortedGridMetrics* outGridMetrics,
    std::string* outFailureReason)
{
    if (entriesRoot == 0 || outTargetCoords == 0)
    {
        if (outFailureReason != 0)
        {
            *outFailureReason = "missing_entries_root";
        }
        return false;
    }

    outTargetCoords->clear();
    outTargetCoords->reserve(orderedEntries.size());
    for (std::size_t index = 0; index < orderedEntries.size(); ++index)
    {
        outTargetCoords->push_back(orderedEntries[index].coord);
    }

    if (orderedEntries.size() != displayOrder.size() || orderedEntries.empty())
    {
        if (outFailureReason != 0)
        {
            *outFailureReason = orderedEntries.empty() ? "empty_ordered_entries" : "display_order_mismatch";
        }
        return false;
    }

    SortedGridMetrics gridMetrics;
    if (!TryResolveSortedGridMetrics(entriesRoot, orderedEntries, &gridMetrics, outFailureReason))
    {
        return false;
    }
    const int cellWidth = gridMetrics.cellWidth;
    const int cellHeight = gridMetrics.cellHeight;
    const int originLeft = gridMetrics.originLeft;
    const int originTop = gridMetrics.originTop;
    const int columns = gridMetrics.columns;
    const int rows = gridMetrics.rows;
    if (outGridMetrics != 0)
    {
        *outGridMetrics = gridMetrics;
    }

    std::vector<char> occupancy(columns * rows, 0);
    int shelfTopCell = 0;
    int shelfLeftCell = 0;
    int shelfHeightCells = 0;
    bool shelfHasEntries = false;
    for (std::size_t orderIndex = 0; orderIndex < displayOrder.size(); ++orderIndex)
    {
        const std::size_t sourceIndex = displayOrder[orderIndex];
        if (sourceIndex >= orderedEntries.size())
        {
            continue;
        }

        const OrderedEntry& entry = orderedEntries[sourceIndex];
        if (entry.widget == 0)
        {
            continue;
        }

        const int widthCells =
            ResolveEntrySpanCells(entry.coord.width, entry.widthCells, cellWidth);
        const int heightCells =
            ResolveEntrySpanCells(entry.coord.height, entry.heightCells, cellHeight);
        if (widthCells <= 0
            || heightCells <= 0
            || widthCells > columns
            || heightCells > rows)
        {
            if (outFailureReason != 0)
            {
                std::stringstream line;
                line << "entry_span_invalid:index=" << sourceIndex
                     << ",width_cells=" << widthCells
                     << ",height_cells=" << heightCells
                     << ",columns=" << columns
                     << ",rows=" << rows;
                *outFailureReason = line.str();
            }
            return false;
        }

        if (shelfHasEntries && shelfLeftCell + widthCells > columns)
        {
            shelfTopCell += shelfHeightCells + kSortedShelfGapRows;
            shelfLeftCell = 0;
            shelfHeightCells = 0;
            shelfHasEntries = false;
        }

        if (shelfTopCell + heightCells > rows)
        {
            if (outFailureReason != 0)
            {
                std::stringstream line;
                line << "shelf_overflow:index=" << sourceIndex
                     << ",width_cells=" << widthCells
                     << ",height_cells=" << heightCells
                     << ",top_cell=" << shelfTopCell
                     << ",rows=" << rows;
                *outFailureReason = line.str();
            }
            return false;
        }

        if (!CanPlaceEntryFootprint(
                occupancy,
                columns,
                rows,
                shelfLeftCell,
                shelfTopCell,
                widthCells,
                heightCells))
        {
            if (outFailureReason != 0)
            {
                std::stringstream line;
                line << "shelf_placement_blocked:index=" << sourceIndex
                     << ",left_cell=" << shelfLeftCell
                     << ",top_cell=" << shelfTopCell
                     << ",width_cells=" << widthCells
                     << ",height_cells=" << heightCells;
                *outFailureReason = line.str();
            }
            return false;
        }

        FillEntryFootprint(
            &occupancy,
            columns,
            shelfLeftCell,
            shelfTopCell,
            widthCells,
            heightCells);
        (*outTargetCoords)[sourceIndex] = MyGUI::IntCoord(
            originLeft + (shelfLeftCell * cellWidth),
            originTop + (shelfTopCell * cellHeight),
            entry.coord.width,
            entry.coord.height);

        shelfLeftCell += widthCells;
        if (heightCells > shelfHeightCells)
        {
            shelfHeightCells = heightCells;
        }
        shelfHasEntries = true;
    }

    if (outFailureReason != 0)
    {
        outFailureReason->clear();
    }
    return true;
}

void ApplyEntryTargetCoords(
    const std::vector<OrderedEntry>& orderedEntries,
    const std::vector<MyGUI::IntCoord>& targetCoords)
{
    if (orderedEntries.size() != targetCoords.size())
    {
        return;
    }

    for (std::size_t index = 0; index < orderedEntries.size(); ++index)
    {
        MyGUI::Widget* widget = orderedEntries[index].widget;
        if (widget == 0)
        {
            continue;
        }

        const MyGUI::IntCoord currentCoord = widget->getCoord();
        const MyGUI::IntCoord targetCoord = targetCoords[index];
        if (currentCoord.left == targetCoord.left
            && currentCoord.top == targetCoord.top
            && currentCoord.width == targetCoord.width
            && currentCoord.height == targetCoord.height)
        {
            continue;
        }

        widget->setCoord(targetCoord);
    }
}

void ResetSortedInventoryLayoutState()
{
    g_sortedInventory = 0;
    g_sortedInventoryBasePositions.clear();
}

bool InventoryContainsItemPointer(Inventory* inventory, Item* item)
{
    if (inventory == 0 || item == 0)
    {
        return false;
    }

    const lektor<Item*>& allItems = inventory->getAllItems();
    if (!allItems.valid())
    {
        return false;
    }

    for (lektor<Item*>::const_iterator iter = allItems.begin(); iter != allItems.end(); ++iter)
    {
        if (*iter == item)
        {
            return true;
        }
    }

    return false;
}

bool CaptureSortedInventoryBasePositions(Inventory* inventory)
{
    if (inventory == 0 || !IsInventoryPointerValidSafe(inventory))
    {
        ResetSortedInventoryLayoutState();
        return false;
    }

    if (g_sortedInventory == inventory && !g_sortedInventoryBasePositions.empty())
    {
        return true;
    }

    ResetSortedInventoryLayoutState();

    const lektor<Item*>& allItems = inventory->getAllItems();
    if (!allItems.valid() || allItems.size() == 0)
    {
        return false;
    }

    g_sortedInventory = inventory;
    g_sortedInventoryBasePositions.reserve(allItems.size());
    for (lektor<Item*>::const_iterator iter = allItems.begin(); iter != allItems.end(); ++iter)
    {
        Item* item = *iter;
        if (item == 0)
        {
            continue;
        }

        TraderSortedInventoryItemBasePosition position;
        position.item = item;
        position.leftCell = item->inventoryPos.x;
        position.topCell = item->inventoryPos.y;
        position.sectionName = item->inventorySection;
        g_sortedInventoryBasePositions.push_back(position);
    }

    return !g_sortedInventoryBasePositions.empty();
}

bool RestoreSortedInventoryLayoutInternal()
{
    if (g_sortedInventory == 0
        || !IsInventoryPointerValidSafe(g_sortedInventory)
        || g_sortedInventoryBasePositions.empty()
        || !g_sortedInventory->isVisible())
    {
        ResetSortedInventoryLayoutState();
        return false;
    }

    std::vector<Item*> orderedItems;
    std::vector<MyGUI::IntCoord> targetCoords;
    orderedItems.reserve(g_sortedInventoryBasePositions.size());
    targetCoords.reserve(g_sortedInventoryBasePositions.size());

    for (std::size_t index = 0; index < g_sortedInventoryBasePositions.size(); ++index)
    {
        const TraderSortedInventoryItemBasePosition& position = g_sortedInventoryBasePositions[index];
        if (position.item == 0 || !InventoryContainsItemPointer(g_sortedInventory, position.item))
        {
            continue;
        }

        orderedItems.push_back(position.item);
        targetCoords.push_back(MyGUI::IntCoord(position.leftCell, position.topCell, 1, 1));
    }

    SortedGridMetrics restoreGridMetrics;
    restoreGridMetrics.cellWidth = 1;
    restoreGridMetrics.cellHeight = 1;
    restoreGridMetrics.originLeft = 0;
    restoreGridMetrics.originTop = 0;

    std::string failureReason;
    const bool changed = ApplySortedInventoryTargetCells(
        g_sortedInventory,
        orderedItems,
        targetCoords,
        restoreGridMetrics,
        &failureReason);

    ResetSortedInventoryLayoutState();
    return changed && failureReason.empty();
}

Item* FindInventoryItemByTopLeftCell(
    Inventory* inventory,
    int leftCell,
    int topCell,
    int widthCells,
    int heightCells,
    const std::vector<Item*>& usedItems)
{
    if (inventory == 0)
    {
        return 0;
    }

    Item* cachedFallback = 0;
    if (g_sortedInventory == inventory && !g_sortedInventoryBasePositions.empty())
    {
        for (std::size_t index = 0; index < g_sortedInventoryBasePositions.size(); ++index)
        {
            const TraderSortedInventoryItemBasePosition& position = g_sortedInventoryBasePositions[index];
            Item* item = position.item;
            if (item == 0
                || !InventoryContainsItemPointer(inventory, item)
                || std::find(usedItems.begin(), usedItems.end(), item) != usedItems.end())
            {
                continue;
            }

            if (position.leftCell != leftCell || position.topCell != topCell)
            {
                continue;
            }

            const int itemWidth = item->itemWidth > 0 ? item->itemWidth : 1;
            const int itemHeight = item->itemHeight > 0 ? item->itemHeight : 1;
            if (itemWidth == widthCells && itemHeight == heightCells)
            {
                return item;
            }
            if (cachedFallback == 0)
            {
                cachedFallback = item;
            }
        }
    }

    const lektor<Item*>& allItems = inventory->getAllItems();
    if (!allItems.valid())
    {
        return cachedFallback;
    }

    Item* fallback = 0;
    for (lektor<Item*>::const_iterator iter = allItems.begin(); iter != allItems.end(); ++iter)
    {
        Item* item = *iter;
        if (item == 0)
        {
            continue;
        }
        if (std::find(usedItems.begin(), usedItems.end(), item) != usedItems.end())
        {
            continue;
        }
        if (item->inventoryPos.x != leftCell || item->inventoryPos.y != topCell)
        {
            continue;
        }

        const int itemWidth = item->itemWidth > 0 ? item->itemWidth : 1;
        const int itemHeight = item->itemHeight > 0 ? item->itemHeight : 1;
        if (itemWidth == widthCells && itemHeight == heightCells)
        {
            return item;
        }
        if (fallback == 0)
        {
            fallback = item;
        }
    }

    return fallback == 0 ? cachedFallback : fallback;
}

bool TryResolveOrderedEntryItemsFromInventory(
    Inventory* inventory,
    const std::vector<OrderedEntry>& orderedEntries,
    const SortedGridMetrics& gridMetrics,
    std::vector<Item*>* outItems,
    std::string* outFailureReason)
{
    if (inventory == 0 || outItems == 0)
    {
        if (outFailureReason != 0)
        {
            *outFailureReason = "inventory_or_output_missing";
        }
        return false;
    }
    if (gridMetrics.cellWidth <= 0 || gridMetrics.cellHeight <= 0)
    {
        if (outFailureReason != 0)
        {
            *outFailureReason = "grid_metrics_invalid";
        }
        return false;
    }

    outItems->clear();
    outItems->reserve(orderedEntries.size());
    std::vector<Item*> usedItems;
    usedItems.reserve(orderedEntries.size());

    for (std::size_t index = 0; index < orderedEntries.size(); ++index)
    {
        const OrderedEntry& entry = orderedEntries[index];
        if (entry.quantity <= 0)
        {
            outItems->push_back(0);
            continue;
        }

        const int leftOffset = entry.coord.left - gridMetrics.originLeft;
        const int topOffset = entry.coord.top - gridMetrics.originTop;
        if (leftOffset < 0
            || topOffset < 0
            || PositiveMod(leftOffset, gridMetrics.cellWidth) != 0
            || PositiveMod(topOffset, gridMetrics.cellHeight) != 0)
        {
            if (outFailureReason != 0)
            {
                std::stringstream line;
                line << "entry_not_grid_aligned:index=" << index
                     << ",coord=" << entry.coord.left << "," << entry.coord.top
                     << ",origin=" << gridMetrics.originLeft << "," << gridMetrics.originTop
                     << ",cell=" << gridMetrics.cellWidth << "," << gridMetrics.cellHeight;
                *outFailureReason = line.str();
            }
            return false;
        }

        const int leftCell = leftOffset / gridMetrics.cellWidth;
        const int topCell = topOffset / gridMetrics.cellHeight;
        const int widthCells =
            ResolveEntrySpanCells(entry.coord.width, entry.widthCells, gridMetrics.cellWidth);
        const int heightCells =
            ResolveEntrySpanCells(entry.coord.height, entry.heightCells, gridMetrics.cellHeight);

        Item* item = FindInventoryItemByTopLeftCell(
            inventory,
            leftCell,
            topCell,
            widthCells,
            heightCells,
            usedItems);
        if (item == 0)
        {
            if (outFailureReason != 0)
            {
                std::stringstream line;
                line << "inventory_item_not_found:index=" << index
                     << ",cell=" << leftCell << "," << topCell
                     << ",size=" << widthCells << "," << heightCells;
                *outFailureReason = line.str();
            }
            return false;
        }

        usedItems.push_back(item);
        outItems->push_back(item);
    }

    if (outFailureReason != 0)
    {
        outFailureReason->clear();
    }
    return true;
}

bool ApplySortedInventoryTargetCells(
    Inventory* inventory,
    const std::vector<Item*>& orderedItems,
    const std::vector<MyGUI::IntCoord>& targetCoords,
    const SortedGridMetrics& gridMetrics,
    std::string* outFailureReason)
{
    if (inventory == 0
        || orderedItems.size() != targetCoords.size()
        || gridMetrics.cellWidth <= 0
        || gridMetrics.cellHeight <= 0)
    {
        if (outFailureReason != 0)
        {
            *outFailureReason = "apply_inputs_invalid";
        }
        return false;
    }

    struct InventoryTargetCell
    {
        Item* item;
        InventorySection* section;
        int targetCellX;
        int targetCellY;
    };

    std::vector<InventoryTargetCell> targets;
    targets.reserve(orderedItems.size());

    bool changed = false;
    for (std::size_t index = 0; index < orderedItems.size(); ++index)
    {
        Item* item = orderedItems[index];
        if (item == 0)
        {
            continue;
        }

        const int leftOffset = targetCoords[index].left - gridMetrics.originLeft;
        const int topOffset = targetCoords[index].top - gridMetrics.originTop;
        if (leftOffset < 0
            || topOffset < 0
            || PositiveMod(leftOffset, gridMetrics.cellWidth) != 0
            || PositiveMod(topOffset, gridMetrics.cellHeight) != 0)
        {
            if (outFailureReason != 0)
            {
                std::stringstream line;
                line << "target_not_grid_aligned:index=" << index
                     << ",target=" << targetCoords[index].left << "," << targetCoords[index].top;
                *outFailureReason = line.str();
            }
            return false;
        }

        const std::string targetSectionName =
            item->inventorySection.empty() ? "backpack_content" : item->inventorySection;
        InventorySection* targetSection = inventory->getSection(targetSectionName);
        if (targetSection == 0)
        {
            if (outFailureReason != 0)
            {
                std::stringstream line;
                line << "target_section_missing:index=" << index
                     << ",section=" << targetSectionName;
                *outFailureReason = line.str();
            }
            return false;
        }

        const int targetCellX = leftOffset / gridMetrics.cellWidth;
        const int targetCellY = topOffset / gridMetrics.cellHeight;
        InventoryTargetCell target;
        target.item = item;
        target.section = targetSection;
        target.targetCellX = targetCellX;
        target.targetCellY = targetCellY;
        targets.push_back(target);

        if (item->inventoryPos.x != targetCellX
            || item->inventoryPos.y != targetCellY
            || item->inventorySection != targetSectionName)
        {
            changed = true;
        }
    }

    if (!changed)
    {
        if (outFailureReason != 0)
        {
            outFailureReason->clear();
        }
        return false;
    }

    for (std::size_t index = 0; index < targets.size(); ++index)
    {
        Item* item = targets[index].item;
        if (item == 0)
        {
            continue;
        }

        InventorySection* currentSection =
            item->inventorySection.empty() ? 0 : inventory->getSection(item->inventorySection);
        if (currentSection == 0)
        {
            currentSection = targets[index].section;
        }

        if (currentSection == 0 || !currentSection->removeItem(item))
        {
            if (outFailureReason != 0)
            {
                std::stringstream line;
                line << "remove_failed:index=" << index
                     << ",section=" << (currentSection == 0 ? "<null>" : currentSection->name)
                     << ",item=" << ResolveCanonicalItemName(item);
                *outFailureReason = line.str();
            }
            return false;
        }
    }

    struct TargetTopLeftLess
    {
        bool operator()(const InventoryTargetCell& left, const InventoryTargetCell& right) const
        {
            if (left.targetCellY != right.targetCellY)
            {
                return left.targetCellY < right.targetCellY;
            }
            if (left.targetCellX != right.targetCellX)
            {
                return left.targetCellX < right.targetCellX;
            }
            return left.item < right.item;
        }
    };
    std::sort(targets.begin(), targets.end(), TargetTopLeftLess());

    for (std::size_t index = 0; index < targets.size(); ++index)
    {
        InventoryTargetCell& target = targets[index];
        if (target.item == 0 || target.section == 0)
        {
            continue;
        }

        if (!target.section->canItemGoHere(target.item, target.targetCellX, target.targetCellY))
        {
            if (outFailureReason != 0)
            {
                std::stringstream line;
                line << "target_blocked:index=" << index
                     << ",section=" << target.section->name
                     << ",cell=" << target.targetCellX << "," << target.targetCellY
                     << ",item=" << ResolveCanonicalItemName(target.item);
                *outFailureReason = line.str();
            }
            return false;
        }

        target.section->_addItem(target.item, target.targetCellX, target.targetCellY);
    }

    if (outFailureReason != 0)
    {
        outFailureReason->clear();
    }
    return true;
}
}

void RestoreSortedInventoryLayoutIfNeeded()
{
    RestoreSortedInventoryLayoutInternal();
}

void MarkSearchFilterDirty(const char* reason)
{
    g_searchFilterDirty = true;

    if (!ShouldLogSearchDebug())
    {
        return;
    }

    std::stringstream line;
    line << "search refresh requested"
         << " reason=" << (reason == 0 ? "<unknown>" : reason);
    LogSearchDebugLine(line.str());
}

std::string BuildObservedTraderEntriesStateSignature()
{
    MyGUI::Widget* traderParent = ResolveTraderParentFromControlsContainer();
    if (traderParent == 0)
    {
        return "";
    }

    MyGUI::Widget* backpackContent = ResolveBestBackpackContentWidget(traderParent, false, false);
    if (backpackContent == 0)
    {
        backpackContent = FindWidgetInParentByToken(traderParent, "backpack_content");
    }
    if (backpackContent == 0)
    {
        return "";
    }

    MyGUI::Widget* entriesRoot = ResolveInventoryEntriesRoot(backpackContent);
    if (entriesRoot == 0)
    {
        return "";
    }

    const std::size_t childCount = entriesRoot->getChildCount();
    std::size_t occupiedCount = 0;
    std::size_t totalQuantity = 0;
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        int quantity = 0;
        if (TryResolveItemQuantityFromWidget(entriesRoot->getChildAt(childIndex), &quantity)
            && quantity > 0)
        {
            ++occupiedCount;
            totalQuantity += static_cast<std::size_t>(quantity);
        }
    }

    TraderPanelInventoryBinding panelBinding;
    if (TryResolveAndCacheTraderPanelInventoryBinding(
            traderParent,
            entriesRoot,
            occupiedCount,
            0,
            &panelBinding,
            0)
        && panelBinding.inventory != 0)
    {
        const std::string inventoryLayoutSignature =
            BuildInventoryLayoutSignature(panelBinding.inventory);
        if (!inventoryLayoutSignature.empty())
        {
            return inventoryLayoutSignature;
        }
    }

    std::stringstream signature;
    signature << traderParent
              << "|" << backpackContent
              << "|" << entriesRoot
              << "|" << childCount
              << "|" << occupiedCount
              << "|" << totalQuantity;
    return signature.str();
}

void ResetObservedTraderEntriesState()
{
    g_lastObservedTraderEntriesStateSignature.clear();
    g_expectedSortedInventoryLayoutSignature.clear();
}

void ObserveTraderEntriesStateForRefresh()
{
    const std::string currentSignature = BuildObservedTraderEntriesStateSignature();
    if (currentSignature.empty())
    {
        ResetBaseEntryCoords();
        ResetObservedTraderEntriesState();
        return;
    }

    if (g_lastObservedTraderEntriesStateSignature.empty())
    {
        g_lastObservedTraderEntriesStateSignature = currentSignature;
        return;
    }

    if (currentSignature != g_lastObservedTraderEntriesStateSignature)
    {
        const bool expectedSortLayoutChange =
            g_sortMode != TraderSortMode_None
            && !g_expectedSortedInventoryLayoutSignature.empty()
            && currentSignature == g_expectedSortedInventoryLayoutSignature;
        g_lastObservedTraderEntriesStateSignature = currentSignature;
        if (expectedSortLayoutChange)
        {
            return;
        }
        ResetBaseEntryCoords();
        ResetSortedInventoryLayoutState();
        g_expectedSortedInventoryLayoutSignature.clear();
        MarkSearchFilterDirty("entries_root_state_changed");
        return;
    }

    g_lastObservedTraderEntriesStateSignature = currentSignature;
}

bool SearchTextMatchesQuery(const std::string& searchableTextNormalized, const std::string& normalizedQuery)
{
    if (normalizedQuery.empty())
    {
        return true;
    }
    if (searchableTextNormalized.empty())
    {
        return true;
    }

    return searchableTextNormalized.find(normalizedQuery) != std::string::npos;
}

struct ParsedSearchQuery
{
    ParsedSearchQuery()
        : blueprintOnly(false)
    {
    }

    bool blueprintOnly;
    std::string normalizedQuery;
};

std::string TrimLeadingAsciiWhitespace(const std::string& value)
{
    std::size_t index = 0;
    while (index < value.size() && std::isspace(static_cast<unsigned char>(value[index])) != 0)
    {
        ++index;
    }

    return value.substr(index);
}

ParsedSearchQuery ParseSearchQuery(const std::string& rawQuery)
{
    ParsedSearchQuery parsed;
    const std::string trimmedQuery = TrimLeadingAsciiWhitespace(rawQuery);
    if (trimmedQuery.size() >= 2
        && (trimmedQuery[0] == 'b' || trimmedQuery[0] == 'B')
        && trimmedQuery[1] == ':')
    {
        parsed.blueprintOnly = true;
        parsed.normalizedQuery = NormalizeSearchText(trimmedQuery.substr(2));
        return parsed;
    }

    parsed.normalizedQuery = NormalizeSearchText(trimmedQuery);
    return parsed;
}

std::string BuildSearchQueryLogKey(const ParsedSearchQuery& query)
{
    if (query.blueprintOnly)
    {
        return std::string("b:") + query.normalizedQuery;
    }

    return query.normalizedQuery;
}

bool SearchTextMatchesBlueprintFilter(const std::string& searchableTextNormalized)
{
    return searchableTextNormalized.find("blueprint") != std::string::npos;
}

void LogSearchSampleForQuery(MyGUI::Widget* entriesRoot, const std::string& normalizedQuery, std::size_t maxItems)
{
#if !defined(OTT_ENABLE_VERBOSE_DIAGNOSTICS)
    (void)entriesRoot;
    (void)normalizedQuery;
    (void)maxItems;
    return;
#else
    if (!ShouldLogSearchDebug())
    {
        return;
    }

    if (entriesRoot == 0 || maxItems == 0)
    {
        return;
    }

    std::stringstream header;
    header << "search debug sample begin"
           << " query=\"" << normalizedQuery << "\""
           << " entries_root=" << SafeWidgetName(entriesRoot)
           << " child_count=" << entriesRoot->getChildCount()
           << " max_items=" << maxItems;
    LogInfoLine(header.str());

    const std::size_t childCount = entriesRoot->getChildCount();
    const std::size_t limit = childCount < maxItems ? childCount : maxItems;
    for (std::size_t childIndex = 0; childIndex < limit; ++childIndex)
    {
        MyGUI::Widget* child = entriesRoot->getChildAt(childIndex);
        if (child == 0)
        {
            continue;
        }

        const std::string searchRaw = BuildItemSearchText(child);
        const std::string searchNormalized = NormalizeSearchText(searchRaw);
        const bool matches = SearchTextMatchesQuery(searchNormalized, normalizedQuery);
        const std::string itemNameHint = ResolveItemNameHintRecursive(child, 0, 5);
        const std::string rawProbe = BuildItemRawProbe(child);

        std::stringstream line;
        line << "search sample idx=" << childIndex
             << " name=" << SafeWidgetName(child)
             << " caption=\"" << TruncateForLog(WidgetCaptionForLog(child), 48) << "\""
             << " item_hint=\"" << TruncateForLog(itemNameHint, 64) << "\""
             << " children=" << child->getChildCount()
             << " raw_len=" << searchRaw.size()
             << " normalized_len=" << searchNormalized.size()
             << " match=" << (matches ? "true" : "false")
             << " text=\"" << TruncateForLog(searchNormalized, 180) << "\""
             << " raw_probe=\"" << TruncateForLog(rawProbe, 220) << "\"";
        LogInfoLine(line.str());
    }

    LogInfoLine("search debug sample end");
#endif
}

bool ApplySearchFilterToTraderParent(MyGUI::Widget* traderParent, bool forceShowAll, bool logSummary)
{
    if (traderParent == 0)
    {
        return false;
    }

    MyGUI::Widget* backpackContent = ResolveBestBackpackContentWidget(traderParent, logSummary);
    if (backpackContent == 0)
    {
        backpackContent = FindWidgetInParentByToken(traderParent, "backpack_content");
    }
    if (backpackContent == 0)
    {
        if (!g_loggedMissingBackpackForSearch)
        {
            std::stringstream line;
            line << "search filter skipped: backpack_content not found parent=" << SafeWidgetName(traderParent);
            LogWarnLine(line.str());
            g_loggedMissingBackpackForSearch = true;
        }
        ResetBaseEntryCoords();
        UpdateSearchCountText(0, 0, 0);
        return false;
    }
    g_loggedMissingBackpackForSearch = false;

    MyGUI::Widget* entriesRoot = ResolveInventoryEntriesRoot(backpackContent);
    if (entriesRoot == 0)
    {
        ResetBaseEntryCoords();
        UpdateSearchCountText(0, 0, 0);
        return false;
    }

    const ParsedSearchQuery parsedQuery = forceShowAll
        ? ParsedSearchQuery()
        : ParseSearchQuery(g_searchQueryRaw);
    const std::string query = parsedQuery.normalizedQuery;
    const bool blueprintOnly = parsedQuery.blueprintOnly;
    const bool hasActiveFilter = blueprintOnly || !query.empty();
    const std::string queryLogKey = BuildSearchQueryLogKey(parsedQuery);
    std::size_t totalCount = 0;
    std::size_t visibleCount = 0;
    std::size_t missingSearchableTextCount = 0;
    std::size_t fallbackKeptVisibleCount = 0;
    std::size_t itemNameHintCount = 0;

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
        entry.widthCells = 0;
        entry.heightCells = 0;
        TryResolveItemQuantityFromWidget(child, &entry.quantity);
        Item* item = ResolveWidgetItemPointer(child);
        if (item != 0)
        {
            if (item->itemWidth > 0)
            {
                entry.widthCells = item->itemWidth;
            }
            if (item->itemHeight > 0)
            {
                entry.heightCells = item->itemHeight;
            }
        }
        orderedEntries.push_back(entry);
    }
    EnsureBaseEntryCoords(entriesRoot, &orderedEntries);

    const std::size_t orderedEntryCount = orderedEntries.size();
    std::vector<std::size_t> occupiedEntryIndices;
    occupiedEntryIndices.reserve(orderedEntryCount);
    std::vector<int> uiQuantities;
    uiQuantities.reserve(orderedEntryCount);
    for (std::size_t index = 0; index < orderedEntryCount; ++index)
    {
        if (orderedEntries[index].quantity <= 0)
        {
            continue;
        }

        occupiedEntryIndices.push_back(index);
        uiQuantities.push_back(orderedEntries[index].quantity);
    }
    const std::size_t expectedEntryCount = occupiedEntryIndices.size();
    std::size_t totalOccupiedQuantity = 0;
    for (std::size_t quantityIndex = 0; quantityIndex < uiQuantities.size(); ++quantityIndex)
    {
        if (uiQuantities[quantityIndex] > 0)
        {
            totalOccupiedQuantity += static_cast<std::size_t>(uiQuantities[quantityIndex]);
        }
    }
    std::size_t widgetSearchableEntryCount = 0;
    if (hasActiveFilter)
    {
        for (std::size_t index = 0; index < orderedEntryCount; ++index)
        {
            const std::string widgetSearchText =
                NormalizeSearchText(BuildItemSearchText(orderedEntries[index].widget));
            if (!widgetSearchText.empty())
            {
                ++widgetSearchableEntryCount;
            }
        }
    }
    const bool preferCoverageFallbackWhenWidgetOpaque =
        hasActiveFilter && expectedEntryCount >= 8 && widgetSearchableEntryCount == 0;

    TraderPanelInventoryBinding panelBinding;
    std::string panelBindingStatus;
    const bool hasPanelBinding = TryResolveAndCacheTraderPanelInventoryBinding(
        traderParent,
        entriesRoot,
        expectedEntryCount,
        &uiQuantities,
        &panelBinding,
        &panelBindingStatus);
    if (ShouldLogBindingDebug() && expectedEntryCount > 0)
    {
        LogPanelBindingProbeOnce(
            traderParent,
            entriesRoot,
            expectedEntryCount,
            hasPanelBinding ? std::string("resolved_") + panelBindingStatus : panelBindingStatus,
            hasPanelBinding ? &panelBinding : 0);
    }

    if (hasActiveFilter && !hasPanelBinding)
    {
        totalCount = orderedEntryCount;
        visibleCount = orderedEntryCount;
        for (std::size_t index = 0; index < orderedEntryCount; ++index)
        {
            MyGUI::Widget* child = orderedEntries[index].widget;
            if (child != 0)
            {
                child->setVisible(true);
            }
        }

        std::stringstream signature;
        signature << queryLogKey
                  << "|" << panelBindingStatus
                  << "|" << expectedEntryCount
                  << "|" << SafeWidgetName(traderParent);
        if (signature.str() != g_lastPanelBindingRefusedSignature)
        {
            std::stringstream line;
            line << "search refused: missing high-confidence panel inventory binding"
                 << " query=\"" << queryLogKey << "\""
                 << " reason=" << panelBindingStatus
                 << " expected_entries=" << expectedEntryCount
                 << " parent=" << SafeWidgetName(traderParent);
            if (ShouldLogBindingDebug())
            {
                LogWarnLine(line.str());
            }
            g_lastPanelBindingRefusedSignature = signature.str();
        }

        if (ShouldLogBindingDebug() && !g_loggedInventoryBindingDiagnostics)
        {
            LogInventoryBindingDiagnostics(expectedEntryCount);
            g_loggedInventoryBindingDiagnostics = true;
        }
        if (g_lastSearchSampleQueryLogged != queryLogKey)
        {
            LogSearchSampleForQuery(entriesRoot, query, 12);
            g_lastSearchSampleQueryLogged = queryLogKey;
        }
        g_loggedInventoryBindingFailure = true;

        if (logSummary && ShouldLogSearchDebug())
        {
            std::stringstream line;
            line << "search filter applied query=\"" << (forceShowAll ? "" : g_searchQueryRaw)
                 << "\" normalized=\"" << (forceShowAll ? "" : query)
                 << "\" visible=" << visibleCount
                 << " total=" << totalCount
                 << " panel_binding=false"
                 << " panel_binding_reason=" << panelBindingStatus
                 << " occupied_entries=" << expectedEntryCount
                 << " entries_root=" << SafeWidgetName(entriesRoot)
                 << " backpack_content=" << SafeWidgetName(backpackContent)
                 << " searchable_entries=0"
                 << " filtering_refused=true";
            LogInfoLine(line.str());
        }
        UpdateSearchCountText(expectedEntryCount, expectedEntryCount, totalOccupiedQuantity);
        return true;
    }

    std::vector<std::string> inventoryNameKeys;
    std::vector<QuantityNameKey> inventoryQuantityNameKeys;
    std::vector<Item*> orderedEntryItems;
    std::string inventorySource;
    bool hasInventoryNameKeys = false;
    bool hasOrderedEntryItems = false;
    if (hasPanelBinding && panelBinding.inventory != 0)
    {
        hasInventoryNameKeys =
            TryExtractSearchKeysFromInventory(panelBinding.inventory, &inventoryNameKeys);
        TryExtractQuantityNameKeysFromInventory(panelBinding.inventory, &inventoryQuantityNameKeys);
        SortedGridMetrics orderedEntryGridMetrics;
        std::string orderedEntryItemFailureReason;
        if (TryResolveSortedGridMetrics(
                entriesRoot,
                orderedEntries,
                &orderedEntryGridMetrics,
                &orderedEntryItemFailureReason))
        {
            hasOrderedEntryItems = TryResolveOrderedEntryItemsFromInventory(
                panelBinding.inventory,
                orderedEntries,
                orderedEntryGridMetrics,
                &orderedEntryItems,
                &orderedEntryItemFailureReason);
        }
        if (expectedEntryCount > 0 && inventoryNameKeys.size() > expectedEntryCount)
        {
            inventoryNameKeys.resize(expectedEntryCount);
        }
        if (expectedEntryCount > 0 && inventoryQuantityNameKeys.size() > expectedEntryCount)
        {
            inventoryQuantityNameKeys.resize(expectedEntryCount);
        }

        std::stringstream source;
        source << "panel_binding:"
               << panelBinding.stage
               << " " << panelBinding.source;
        inventorySource = source.str();
    }

    std::vector<std::string> alignedInventoryNameHints;
    const bool hasAlignedInventoryNameHints = hasInventoryNameKeys
        && BuildAlignedInventoryNameHintsByQuantity(
            uiQuantities,
            inventoryQuantityNameKeys,
            &alignedInventoryNameHints);
    const std::size_t panelBindingNonEmptyKeyCount = hasInventoryNameKeys
        ? CountNonEmptyKeys(inventoryNameKeys)
        : 0;
    const bool panelBindingLowCoverage =
        hasInventoryNameKeys
        && expectedEntryCount >= 8
        && panelBindingNonEmptyKeyCount * 2 < expectedEntryCount;

    if (hasActiveFilter && (!hasInventoryNameKeys || panelBindingLowCoverage))
    {
        totalCount = orderedEntryCount;
        visibleCount = orderedEntryCount;
        for (std::size_t index = 0; index < orderedEntryCount; ++index)
        {
            MyGUI::Widget* child = orderedEntries[index].widget;
            if (child != 0)
            {
                child->setVisible(true);
            }
        }

        const std::string refusalReason =
            !hasInventoryNameKeys ? "panel_binding_keys_missing" : "panel_binding_low_coverage";
        std::stringstream signature;
        signature << queryLogKey
                  << "|" << refusalReason
                  << "|" << panelBindingNonEmptyKeyCount
                  << "|" << expectedEntryCount
                  << "|" << SafeWidgetName(traderParent);
        if (signature.str() != g_lastPanelBindingRefusedSignature)
        {
            std::stringstream line;
            line << "search refused: panel binding confidence too low"
                 << " query=\"" << queryLogKey << "\""
                 << " reason=" << refusalReason
                 << " non_empty_keys=" << panelBindingNonEmptyKeyCount
                 << " expected_entries=" << expectedEntryCount
                 << " source=\"" << TruncateForLog(inventorySource, 160) << "\"";
            if (ShouldLogBindingDebug())
            {
                LogWarnLine(line.str());
            }
            g_lastPanelBindingRefusedSignature = signature.str();
        }

        if (ShouldLogBindingDebug() && !g_loggedInventoryBindingDiagnostics)
        {
            LogInventoryBindingDiagnostics(expectedEntryCount);
            g_loggedInventoryBindingDiagnostics = true;
        }
        g_loggedInventoryBindingFailure = true;

        if (logSummary && ShouldLogSearchDebug())
        {
            std::stringstream line;
            line << "search filter applied query=\"" << (forceShowAll ? "" : g_searchQueryRaw)
                 << "\" normalized=\"" << (forceShowAll ? "" : query)
                 << "\" visible=" << visibleCount
                 << " total=" << totalCount
                 << " panel_binding=true"
                 << " panel_binding_reason=" << refusalReason
                 << " occupied_entries=" << expectedEntryCount
                 << " inventory_non_empty_keys=" << panelBindingNonEmptyKeyCount
                 << " entries_root=" << SafeWidgetName(entriesRoot)
                 << " backpack_content=" << SafeWidgetName(backpackContent)
                 << " filtering_refused=true";
            LogInfoLine(line.str());
        }
        UpdateSearchCountText(expectedEntryCount, expectedEntryCount, totalOccupiedQuantity);
        return true;
    }

    if (!hasInventoryNameKeys && hasActiveFilter && !g_loggedInventoryBindingFailure)
    {
        LogWarnLine("could not resolve trader inventory-backed name keys; search is using widget-only metadata");
        g_loggedInventoryBindingFailure = true;
    }
    if (hasInventoryNameKeys)
    {
        g_loggedInventoryBindingFailure = false;
    }
    if (!hasInventoryNameKeys && hasActiveFilter && ShouldLogBindingDebug() && !g_loggedInventoryBindingDiagnostics)
    {
        LogInventoryBindingDiagnostics(expectedEntryCount);
        g_loggedInventoryBindingDiagnostics = true;
    }
    if (hasPanelBinding && hasInventoryNameKeys)
    {
        g_loggedInventoryBindingFailure = false;
    }
    if (!hasActiveFilter)
    {
        g_loggedInventoryBindingDiagnostics = false;
    }

    struct EntryFilterState
    {
        MyGUI::Widget* widget;
        std::string searchableText;
        std::string sortItemName;
        bool blueprint;
        bool occupied;
        int quantity;
        bool hasSortMetric;
        double sortMetric;
        bool lowCoverageQuantityMatched;
        std::size_t originalIndex;
    };
    std::vector<EntryFilterState> entries;
    entries.reserve(orderedEntryCount);
    std::size_t visibleOccupiedCount = 0;
    std::size_t visibleQuantity = 0;
    std::size_t searchableEntryCount = 0;
    std::size_t sequenceAlignedNameHintCount = 0;
    std::size_t quantityAlignedNameHintCount = 0;
    std::size_t quantityCandidateNameHintCount = 0;
    std::size_t inventoryKeyQueryMatches = 0;
    std::size_t zeroMatchGuardRestoredCount = 0;
    std::size_t sortMetricEntryCount = 0;
    const std::size_t inventoryNonEmptyKeyCount = hasInventoryNameKeys
        ? CountNonEmptyKeys(inventoryNameKeys)
        : 0;
    const bool inventoryKeyCoverageLow = hasInventoryNameKeys
        && expectedEntryCount >= 8
        && inventoryNonEmptyKeyCount * 2 < expectedEntryCount;
    std::size_t alignedInventoryNameHintCoverage = 0;
    if (hasAlignedInventoryNameHints)
    {
        alignedInventoryNameHintCoverage = CountNonEmptyKeys(alignedInventoryNameHints);
    }
    const bool strongAlignedHintCoverage = hasAlignedInventoryNameHints
        && (expectedEntryCount < 8 || alignedInventoryNameHintCoverage * 2 >= expectedEntryCount);
    const std::string inventorySourceLower = NormalizeSearchText(inventorySource);
    const bool inventorySourceLooksDirect =
        inventorySourceLower.find("widget") != std::string::npos
        || inventorySourceLower.find("selected item") != std::string::npos
        || inventorySourceLower.find("hovered") != std::string::npos;
    const bool inventorySourceTraderAnchored = IsTraderAnchoredCandidateSource(inventorySourceLower);
    const bool inventorySourceRisky = IsRiskyCoverageFallbackSource(inventorySourceLower);
    const bool allowSequenceAlignedHints = hasAlignedInventoryNameHints && !inventoryKeyCoverageLow;
    const bool opaqueIndexedHintsHaveConfidence =
        inventorySourceLooksDirect
        || (inventorySourceTraderAnchored
            && expectedEntryCount > 0
            && alignedInventoryNameHintCoverage * 2 >= expectedEntryCount)
        || (expectedEntryCount > 0
            && alignedInventoryNameHintCoverage * 3 >= expectedEntryCount * 2);
    const bool allowOpaqueIndexedHints =
        preferCoverageFallbackWhenWidgetOpaque
        && hasInventoryNameKeys
        && expectedEntryCount > 0
        && inventoryNonEmptyKeyCount * 2 >= expectedEntryCount
        && !inventorySourceRisky
        && opaqueIndexedHintsHaveConfidence;
    const bool allowIndexedNameHints =
        hasInventoryNameKeys
        && (!inventoryKeyCoverageLow || allowOpaqueIndexedHints)
        && (inventorySourceLooksDirect || strongAlignedHintCoverage || allowOpaqueIndexedHints);
    const bool allowQuantityCandidateHints = false;
    std::vector<int> lowCoverageMatchedQuantities;
    if (inventoryKeyCoverageLow && !query.empty() && !inventoryQuantityNameKeys.empty())
    {
        const std::size_t pairCount =
            inventoryNameKeys.size() < inventoryQuantityNameKeys.size()
                ? inventoryNameKeys.size()
                : inventoryQuantityNameKeys.size();
        for (std::size_t pairIndex = 0; pairIndex < pairCount; ++pairIndex)
        {
            if (inventoryNameKeys[pairIndex].find(query) == std::string::npos)
            {
                continue;
            }

            const int matchedQuantity = inventoryQuantityNameKeys[pairIndex].quantity;
            if (matchedQuantity <= 0)
            {
                continue;
            }

            bool alreadyAdded = false;
            for (std::size_t existingIndex = 0; existingIndex < lowCoverageMatchedQuantities.size(); ++existingIndex)
            {
                if (lowCoverageMatchedQuantities[existingIndex] == matchedQuantity)
                {
                    alreadyAdded = true;
                    break;
                }
            }
            if (!alreadyAdded)
            {
                lowCoverageMatchedQuantities.push_back(matchedQuantity);
            }
        }
    }
    const bool lowCoverageQuantityAssistActive =
        inventoryKeyCoverageLow && !query.empty() && !lowCoverageMatchedQuantities.empty();
    std::size_t lowCoverageQuantityMatchedEntryCount = 0;
    if (hasInventoryNameKeys && !query.empty())
    {
        for (std::size_t keyIndex = 0; keyIndex < inventoryNameKeys.size(); ++keyIndex)
        {
            if (inventoryNameKeys[keyIndex].find(query) != std::string::npos)
            {
                ++inventoryKeyQueryMatches;
            }
        }
    }

    for (std::size_t childIndex = 0; childIndex < orderedEntryCount; ++childIndex)
    {
        const OrderedEntry& ordered = orderedEntries[childIndex];
        MyGUI::Widget* child = ordered.widget;
        ++totalCount;
        const bool occupied = ordered.quantity > 0;
        std::string searchableText;

        std::string itemNameHint;
        std::size_t occupiedOrdinal = static_cast<std::size_t>(-1);
        if (occupied)
        {
            for (std::size_t occupiedIndex = 0; occupiedIndex < occupiedEntryIndices.size(); ++occupiedIndex)
            {
                if (occupiedEntryIndices[occupiedIndex] == childIndex)
                {
                    occupiedOrdinal = occupiedIndex;
                    break;
                }
            }
        }
        const std::string indexedNameHint =
            (allowIndexedNameHints
             && occupiedOrdinal != static_cast<std::size_t>(-1)
             && occupiedOrdinal < inventoryNameKeys.size())
            ? inventoryNameKeys[occupiedOrdinal]
            : "";
        const std::string sequenceAlignedNameHint =
            (allowSequenceAlignedHints
             && occupiedOrdinal != static_cast<std::size_t>(-1)
             && occupiedOrdinal < alignedInventoryNameHints.size())
            ? alignedInventoryNameHints[occupiedOrdinal]
            : "";
        Item* orderedEntryItem = 0;
        if (hasOrderedEntryItems && childIndex < orderedEntryItems.size())
        {
            orderedEntryItem = orderedEntryItems[childIndex];
        }
        const std::string resolvedItemSearchText =
            orderedEntryItem != 0
                ? NormalizeSearchText(BuildItemSearchSourceText(orderedEntryItem))
                : "";
        const std::string sortItemName =
            orderedEntryItem != 0
                ? NormalizeSearchText(
                    SortModeUsesItemNamePrimary(g_sortMode)
                        ? BuildItemSearchSourceText(orderedEntryItem)
                        : ResolveCanonicalItemName(orderedEntryItem))
                : "";
        double sortMetric = 0.0;
        bool hasSortMetric = false;
        if (orderedEntryItem != 0)
        {
            if (SortModeUsesItemNamePrimary(g_sortMode))
            {
                hasSortMetric = !sortItemName.empty();
            }
            else
            {
                hasSortMetric = TryResolveSortMetricForItem(orderedEntryItem, g_sortMode, &sortMetric);
            }
        }

        std::string quantityAlignedNameHint;
        if (!inventoryQuantityNameKeys.empty())
        {
            if (ordered.quantity > 0)
            {
                quantityAlignedNameHint = ResolveUniqueQuantityNameHint(inventoryQuantityNameKeys, ordered.quantity);
            }
        }

        std::string quantityCandidateNameHint;
        if (allowQuantityCandidateHints
            && quantityAlignedNameHint.empty()
            && !inventoryQuantityNameKeys.empty()
            && ordered.quantity > 0)
        {
            quantityCandidateNameHint = ResolveTopQuantityNameHints(
                inventoryQuantityNameKeys,
                ordered.quantity,
                4);
        }

        if (!resolvedItemSearchText.empty())
        {
            itemNameHint = resolvedItemSearchText;
        }
        else if (!sequenceAlignedNameHint.empty())
        {
            itemNameHint = sequenceAlignedNameHint;
            ++sequenceAlignedNameHintCount;
        }
        else if (!quantityAlignedNameHint.empty())
        {
            itemNameHint = quantityAlignedNameHint;
            ++quantityAlignedNameHintCount;
        }
        else if (!quantityCandidateNameHint.empty())
        {
            itemNameHint = quantityCandidateNameHint;
            ++quantityCandidateNameHintCount;
        }
        else if (!indexedNameHint.empty())
        {
            itemNameHint = indexedNameHint;
        }
        else
        {
            itemNameHint = NormalizeSearchText(ResolveItemNameHintRecursive(child, 0, 5));
        }
        if (!itemNameHint.empty() && !ContainsAsciiLetter(itemNameHint))
        {
            itemNameHint.clear();
        }

        if (!itemNameHint.empty())
        {
            ++itemNameHintCount;
        }
        AppendNormalizedSearchChunk(itemNameHint, &searchableText);

        const std::string widgetSearchText = NormalizeSearchText(BuildItemSearchText(child));
        AppendNormalizedSearchChunk(widgetSearchText, &searchableText);
        if (!searchableText.empty())
        {
            ++searchableEntryCount;
        }

        EntryFilterState state;
        state.widget = child;
        state.searchableText = searchableText;
        state.sortItemName = sortItemName;
        state.blueprint = SearchTextMatchesBlueprintFilter(searchableText);
        state.occupied = occupied;
        state.quantity = ordered.quantity;
        state.hasSortMetric = hasSortMetric;
        state.sortMetric = sortMetric;
        state.lowCoverageQuantityMatched = false;
        state.originalIndex = childIndex;
        if (hasSortMetric)
        {
            ++sortMetricEntryCount;
        }
        if (lowCoverageQuantityAssistActive && ordered.quantity > 0)
        {
            for (std::size_t quantityIndex = 0; quantityIndex < lowCoverageMatchedQuantities.size(); ++quantityIndex)
            {
                if (lowCoverageMatchedQuantities[quantityIndex] == ordered.quantity)
                {
                    state.lowCoverageQuantityMatched = true;
                    ++lowCoverageQuantityMatchedEntryCount;
                    break;
                }
            }
        }
        entries.push_back(state);
    }

    const bool hasAnySearchableText = searchableEntryCount > 0;
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        const EntryFilterState& entry = entries[index];
        bool shouldBeVisible = true;

        if (hasActiveFilter)
        {
            if (blueprintOnly && !entry.blueprint)
            {
                shouldBeVisible = false;
                if (entry.searchableText.empty() && entry.occupied)
                {
                    ++missingSearchableTextCount;
                }
            }
            else if (lowCoverageQuantityAssistActive)
            {
                if (!entry.searchableText.empty())
                {
                    shouldBeVisible =
                        entry.searchableText.find(query) != std::string::npos
                        || entry.lowCoverageQuantityMatched;
                }
                else
                {
                    shouldBeVisible = entry.lowCoverageQuantityMatched;
                    if (!entry.occupied)
                    {
                        shouldBeVisible = false;
                    }
                    else
                    {
                        ++missingSearchableTextCount;
                    }
                }
            }
            else if (entry.searchableText.empty())
            {
                ++missingSearchableTextCount;
                if (!entry.occupied)
                {
                    shouldBeVisible = false;
                }
                else
                {
                    shouldBeVisible = !hasAnySearchableText;
                    if (shouldBeVisible)
                    {
                        ++fallbackKeptVisibleCount;
                    }
                }
            }
            else
            {
                shouldBeVisible = entry.searchableText.find(query) != std::string::npos;
            }
        }

        entry.widget->setVisible(shouldBeVisible);
        if (shouldBeVisible)
        {
            ++visibleCount;
            if (entry.occupied)
            {
                ++visibleOccupiedCount;
                if (entry.quantity > 0)
                {
                    visibleQuantity += static_cast<std::size_t>(entry.quantity);
                }
            }
        }
    }

    const bool lowAlignmentConfidence =
        !query.empty()
        && expectedEntryCount >= 10
        && !inventorySourceLooksDirect
        && (sequenceAlignedNameHintCount + quantityAlignedNameHintCount) * 3 < expectedEntryCount;
    if (!query.empty()
        && visibleCount == 0
        && expectedEntryCount > 0
        && !allowOpaqueIndexedHints
        && (inventoryKeyCoverageLow || lowAlignmentConfidence))
    {
        for (std::size_t index = 0; index < entries.size(); ++index)
        {
            const EntryFilterState& entry = entries[index];
            if (!entry.occupied)
            {
                continue;
            }

            if (!entry.widget->getVisible())
            {
                entry.widget->setVisible(true);
                ++zeroMatchGuardRestoredCount;
            }
        }
        visibleCount = zeroMatchGuardRestoredCount;

        if (zeroMatchGuardRestoredCount > 0)
        {
            std::stringstream signature;
            signature << queryLogKey
                      << "|" << inventorySource
                      << "|" << (inventoryKeyCoverageLow ? "1" : "0")
                      << "|" << (lowAlignmentConfidence ? "1" : "0")
                      << "|" << zeroMatchGuardRestoredCount;
            if (signature.str() != g_lastZeroMatchGuardSignature)
            {
                std::stringstream line;
                line << "search zero-match guard restored occupied entries="
                     << zeroMatchGuardRestoredCount
                     << " query=\"" << queryLogKey << "\""
                     << " inventory_low_coverage=" << (inventoryKeyCoverageLow ? "true" : "false")
                     << " low_alignment_confidence=" << (lowAlignmentConfidence ? "true" : "false")
                     << " source=\"" << TruncateForLog(inventorySource, 120) << "\"";
                LogWarnLine(line.str());
                g_lastZeroMatchGuardSignature = signature.str();
            }
        }
    }

    std::vector<std::size_t> displayOrder;
    const bool shouldApplySearchLayout = hasActiveFilter && !entries.empty();
    const bool shouldApplySortLayout =
        g_sortMode != TraderSortMode_None
        && !entries.empty()
        && sortMetricEntryCount > 0;
    const bool shouldBuildPackedLayout = shouldApplySearchLayout || shouldApplySortLayout;
    if (shouldBuildPackedLayout)
    {
        displayOrder.reserve(entries.size());
        std::vector<std::size_t> visibleEntryIndices;
        std::vector<std::size_t> hiddenEntryIndices;
        visibleEntryIndices.reserve(entries.size());
        hiddenEntryIndices.reserve(entries.size());
        for (std::size_t index = 0; index < entries.size(); ++index)
        {
            if (entries[index].widget != 0 && entries[index].widget->getVisible())
            {
                visibleEntryIndices.push_back(index);
            }
            else
            {
                hiddenEntryIndices.push_back(index);
            }
        }

        struct SortMetricSorter
        {
            SortMetricSorter(
                const std::vector<EntryFilterState>* source,
                TraderSortMode mode,
                TraderSortDirection direction)
                : entries(source)
                , sortMode(mode)
                , sortDirection(direction)
            {
            }

            const std::vector<EntryFilterState>* entries;
            TraderSortMode sortMode;
            TraderSortDirection sortDirection;

            bool operator()(std::size_t leftIndex, std::size_t rightIndex) const
            {
                const EntryFilterState& left = (*entries)[leftIndex];
                const EntryFilterState& right = (*entries)[rightIndex];
                if (left.hasSortMetric != right.hasSortMetric)
                {
                    return left.hasSortMetric;
                }
                if (sortMode == TraderSortMode_Name)
                {
                    if (left.sortItemName != right.sortItemName)
                    {
                        if (sortDirection == TraderSortDirection_Descending)
                        {
                            return left.sortItemName > right.sortItemName;
                        }
                        return left.sortItemName < right.sortItemName;
                    }
                    return left.originalIndex < right.originalIndex;
                }
                if (left.hasSortMetric && right.hasSortMetric && left.sortMetric != right.sortMetric)
                {
                    if (sortDirection == TraderSortDirection_Descending)
                    {
                        return left.sortMetric > right.sortMetric;
                    }
                    return left.sortMetric < right.sortMetric;
                }
                if (left.sortItemName != right.sortItemName)
                {
                    return left.sortItemName < right.sortItemName;
                }
                return left.originalIndex < right.originalIndex;
            }
        };

        if (shouldApplySortLayout)
        {
            std::stable_sort(
                visibleEntryIndices.begin(),
                visibleEntryIndices.end(),
                SortMetricSorter(&entries, g_sortMode, g_sortDirection));
        }

        displayOrder.insert(
            displayOrder.end(),
            visibleEntryIndices.begin(),
            visibleEntryIndices.end());
        displayOrder.insert(
            displayOrder.end(),
            hiddenEntryIndices.begin(),
            hiddenEntryIndices.end());
    }

    std::vector<MyGUI::IntCoord> targetCoords;
    bool sortedLayoutApplied = false;
    SortedGridMetrics sortedGridMetrics;
    std::string sortLayoutFailureReason;
    if (shouldBuildPackedLayout)
    {
        sortedLayoutApplied = TryBuildSortedTargetCoords(
            entriesRoot,
            orderedEntries,
            displayOrder,
            &targetCoords,
            &sortedGridMetrics,
            &sortLayoutFailureReason);
        if (!sortedLayoutApplied)
        {
            targetCoords.clear();
            targetCoords.reserve(orderedEntries.size());
            for (std::size_t index = 0; index < orderedEntries.size(); ++index)
            {
                targetCoords.push_back(orderedEntries[index].coord);
            }
        }
    }
    else
    {
        targetCoords.reserve(orderedEntries.size());
        for (std::size_t index = 0; index < orderedEntries.size(); ++index)
        {
            targetCoords.push_back(orderedEntries[index].coord);
        }
    }

    if (shouldApplySearchLayout || g_sortMode == TraderSortMode_None)
    {
        ApplyEntryTargetCoords(orderedEntries, targetCoords);
    }

    std::string inventoryLayoutFailureReason;
    if (g_sortMode == TraderSortMode_None)
    {
        RestoreSortedInventoryLayoutInternal();
        g_expectedSortedInventoryLayoutSignature.clear();
    }
    else if (hasPanelBinding && panelBinding.inventory != 0)
    {
        g_expectedSortedInventoryLayoutSignature.clear();
        if (!sortedLayoutApplied)
        {
            inventoryLayoutFailureReason =
                sortLayoutFailureReason.empty() ? "sort_target_coords_unavailable" : sortLayoutFailureReason;
        }
        else if (!CaptureSortedInventoryBasePositions(panelBinding.inventory))
        {
            inventoryLayoutFailureReason = "capture_base_positions_failed";
        }
        else
        {
            std::vector<Item*> orderedItems;
            if (!TryResolveOrderedEntryItemsFromInventory(
                    panelBinding.inventory,
                    orderedEntries,
                    sortedGridMetrics,
                    &orderedItems,
                    &inventoryLayoutFailureReason))
            {
                if (inventoryLayoutFailureReason.empty())
                {
                    inventoryLayoutFailureReason = "ordered_items_unresolved";
                }
            }
            else
            {
                std::string applyFailureReason;
                ApplySortedInventoryTargetCells(
                    panelBinding.inventory,
                    orderedItems,
                    targetCoords,
                    sortedGridMetrics,
                    &applyFailureReason);
                inventoryLayoutFailureReason = applyFailureReason;
                if (inventoryLayoutFailureReason.empty())
                {
                    g_expectedSortedInventoryLayoutSignature =
                        BuildInventoryLayoutSignature(panelBinding.inventory);
                }
            }
        }
    }
    else
    {
        g_expectedSortedInventoryLayoutSignature.clear();
    }

    if (g_sortMode != TraderSortMode_None
        && !inventoryLayoutFailureReason.empty()
        && ShouldLogSearchDebug())
    {
        std::stringstream line;
        line << "search sort inventory apply skipped"
             << " mode=" << TraderSortStateLabel(g_sortMode, g_sortDirection)
             << " reason=" << inventoryLayoutFailureReason
             << " source=\"" << TruncateForLog(inventorySource, 160) << "\"";
        LogWarnLine(line.str());
    }

    if (ShouldLogSearchDebug())
    {
        if (g_sortMode == TraderSortMode_None)
        {
            g_lastSortInvestigationSignature.clear();
        }
        else
        {
            std::stringstream signature;
            signature << TraderSortStateLabel(g_sortMode, g_sortDirection)
                      << "|" << query
                      << "|" << SafeWidgetName(entriesRoot)
                      << "|" << shouldApplySortLayout
                      << "|" << sortedLayoutApplied
                      << "|" << sortLayoutFailureReason
                      << "|" << displayOrder.size()
                      << "|" << sortedGridMetrics.cellWidth
                      << "x" << sortedGridMetrics.cellHeight
                      << "|" << sortedGridMetrics.columns
                      << "x" << sortedGridMetrics.rows;
            for (std::size_t index = 0; index < entries.size(); ++index)
            {
                signature << "|" << index
                          << ":" << (entries[index].widget != 0 && entries[index].widget->getVisible() ? "1" : "0")
                          << ":" << FormatSortMetricForLog(entries[index].sortMetric)
                          << ":" << targetCoords[index].left
                          << "," << targetCoords[index].top;
            }

            if (signature.str() != g_lastSortInvestigationSignature)
            {
                std::size_t visibleEntryCount = 0;
                std::size_t hiddenEntryCount = 0;
                std::size_t movedEntryCount = 0;
                std::size_t logicalMismatchCount = 0;
                std::size_t overlapPairCount = 0;
                std::stringstream overlapPreview;
                std::stringstream orderPreview;
                const std::size_t orderPreviewLimit = displayOrder.size() < 16 ? displayOrder.size() : 16;
                for (std::size_t orderIndex = 0; orderIndex < orderPreviewLimit; ++orderIndex)
                {
                    const std::size_t sourceIndex = displayOrder[orderIndex];
                    if (orderIndex > 0)
                    {
                        orderPreview << ",";
                    }
                    orderPreview << sourceIndex;
                    if (sourceIndex < entries.size() && entries[sourceIndex].hasSortMetric)
                    {
                        orderPreview << ":" << FormatSortMetricForLog(entries[sourceIndex].sortMetric);
                    }
                }
                if (displayOrder.size() > orderPreviewLimit)
                {
                    orderPreview << ",...";
                }

                for (std::size_t leftIndex = 0; leftIndex < entries.size(); ++leftIndex)
                {
                    const EntryFilterState& leftEntry = entries[leftIndex];
                    if (leftEntry.widget == 0 || !leftEntry.widget->getVisible())
                    {
                        ++hiddenEntryCount;
                        continue;
                    }

                    ++visibleEntryCount;
                    if (leftEntry.widget->getCoord().left != orderedEntries[leftIndex].coord.left
                        || leftEntry.widget->getCoord().top != orderedEntries[leftIndex].coord.top)
                    {
                        ++movedEntryCount;
                    }

                    Item* leftItem = ResolveWidgetItemPointer(leftEntry.widget);
                    if (leftItem != 0 && sortedGridMetrics.cellWidth > 0 && sortedGridMetrics.cellHeight > 0)
                    {
                        const int targetCellX =
                            (targetCoords[leftIndex].left - sortedGridMetrics.originLeft) / sortedGridMetrics.cellWidth;
                        const int targetCellY =
                            (targetCoords[leftIndex].top - sortedGridMetrics.originTop) / sortedGridMetrics.cellHeight;
                        if (leftItem->inventoryPos.x != targetCellX || leftItem->inventoryPos.y != targetCellY)
                        {
                            ++logicalMismatchCount;
                        }
                    }

                    for (std::size_t rightIndex = leftIndex + 1; rightIndex < entries.size(); ++rightIndex)
                    {
                        const EntryFilterState& rightEntry = entries[rightIndex];
                        if (rightEntry.widget == 0 || !rightEntry.widget->getVisible())
                        {
                            continue;
                        }

                        if (!CoordsOverlap(targetCoords[leftIndex], targetCoords[rightIndex]))
                        {
                            continue;
                        }

                        if (overlapPairCount < 8)
                        {
                            if (overlapPairCount > 0)
                            {
                                overlapPreview << " ";
                            }
                            overlapPreview << leftIndex << "-" << rightIndex;
                        }
                        ++overlapPairCount;
                    }
                }

                {
                    std::stringstream line;
                    line << "snapshot"
                         << " mode="
                         << TraderSortStateLabel(g_sortMode, g_sortDirection)
                         << " query=\"" << queryLogKey << "\""
                         << " should_apply=" << (shouldApplySortLayout ? "true" : "false")
                         << " layout_applied=" << (sortedLayoutApplied ? "true" : "false")
                         << " failure=\"" << (sortLayoutFailureReason.empty() ? "<none>" : sortLayoutFailureReason) << "\""
                         << " total_entries=" << entries.size()
                         << " visible_entries=" << visibleEntryCount
                         << " hidden_entries=" << hiddenEntryCount
                         << " sort_metric_entries=" << sortMetricEntryCount
                         << " occupied_entries=" << expectedEntryCount
                         << " entries_root=" << SafeWidgetName(entriesRoot)
                         << " root_size=(" << entriesRoot->getWidth() << "," << entriesRoot->getHeight() << ")"
                         << " grid_cell=(" << sortedGridMetrics.cellWidth << "," << sortedGridMetrics.cellHeight << ")"
                         << " grid_origin=(" << sortedGridMetrics.originLeft << "," << sortedGridMetrics.originTop << ")"
                         << " grid_cells=(" << sortedGridMetrics.columns << "," << sortedGridMetrics.rows << ")"
                         << " moved_entries=" << movedEntryCount
                         << " logical_mismatches=" << logicalMismatchCount
                         << " overlap_pairs=" << overlapPairCount
                         << " order=[" << orderPreview.str() << "]";
                    if (!inventorySource.empty())
                    {
                        line << " source=\"" << TruncateForLog(inventorySource, 120) << "\"";
                    }
                    LogInvestigateSortLine(line.str());
                }

                if (overlapPairCount > 0)
                {
                    std::stringstream line;
                    line << "overlap_preview pairs=" << overlapPairCount
                         << " sample=" << overlapPreview.str();
                    LogInvestigateSortLine(line.str());
                }

                for (std::size_t index = 0; index < entries.size(); ++index)
                {
                    const EntryFilterState& entry = entries[index];
                    Item* item = ResolveWidgetItemPointer(entry.widget);
                    std::stringstream line;
                    line << "entry"
                         << " index=" << index
                         << " widget=" << SafeWidgetName(entry.widget)
                         << " widget_ptr=" << entry.widget
                         << " visible=" << (entry.widget != 0 && entry.widget->getVisible() ? "true" : "false")
                         << " qty=" << entry.quantity
                         << " occupied=" << (entry.occupied ? "true" : "false")
                         << " metric=" << FormatSortMetricForLog(entry.sortMetric)
                         << " has_metric=" << (entry.hasSortMetric ? "true" : "false")
                         << " base=" << FormatCoordForInvestigation(orderedEntries[index].coord)
                         << " target=" << FormatCoordForInvestigation(targetCoords[index])
                         << " current=" << (entry.widget == 0
                                ? "(null)"
                                : FormatCoordForInvestigation(entry.widget->getCoord()))
                         << " size_cells=("
                         << ResolveEntrySpanCells(
                                orderedEntries[index].coord.width,
                                orderedEntries[index].widthCells,
                                sortedGridMetrics.cellWidth <= 0 ? orderedEntries[index].coord.width : sortedGridMetrics.cellWidth)
                         << ","
                         << ResolveEntrySpanCells(
                                orderedEntries[index].coord.height,
                                orderedEntries[index].heightCells,
                                sortedGridMetrics.cellHeight <= 0 ? orderedEntries[index].coord.height : sortedGridMetrics.cellHeight)
                         << ")"
                         << " item_ptr=" << item
                         << " item_name=\"" << (item == 0 ? "" : TruncateForLog(ResolveCanonicalItemName(item), 72)) << "\""
                         << " inv_pos=" << FormatItemPosForInvestigation(item)
                         << " searchable=\"" << TruncateForLog(entry.searchableText, 96) << "\"";
                    LogInvestigateSortLine(line.str());
                }

                g_lastSortInvestigationSignature = signature.str();
            }
        }
    }

    if (hasActiveFilter && fallbackKeptVisibleCount > 0 && !g_loggedMissingSearchableItemText)
    {
        std::stringstream line;
        line << "search fallback: kept " << fallbackKeptVisibleCount
             << " entries visible because searchable text was missing";
        LogWarnLine(line.str());
        g_loggedMissingSearchableItemText = true;
    }
    if (!hasActiveFilter)
    {
        g_loggedMissingSearchableItemText = false;
        g_lastZeroMatchQueryLogged.clear();
        g_lastZeroMatchGuardSignature.clear();
        g_lastSearchSampleQueryLogged.clear();
        g_loggedInventoryBindingFailure = false;
        g_loggedInventoryBindingDiagnostics = false;
        g_lastPanelBindingRefusedSignature.clear();
    }

    if (logSummary && ShouldLogSearchDebug())
    {
        std::stringstream line;
        line << "search filter applied query=\"" << (forceShowAll ? "" : g_searchQueryRaw)
             << "\" normalized=\"" << (forceShowAll ? "" : query)
             << "\" blueprint_only=" << (blueprintOnly ? "true" : "false")
             << " sort_mode=\""
             << TraderSortStateLabel(g_sortMode, g_sortDirection)
             << "\" visible=" << visibleCount
             << " total=" << totalCount
             << " item_hints=" << itemNameHintCount
             << " sequence_aligned_hints=" << sequenceAlignedNameHintCount
             << " quantity_aligned_hints=" << quantityAlignedNameHintCount
             << " quantity_candidate_hints=" << quantityCandidateNameHintCount
             << " inventory_keys=" << (hasInventoryNameKeys ? inventoryNameKeys.size() : 0)
             << " inventory_non_empty_keys=" << inventoryNonEmptyKeyCount
             << " inventory_low_coverage=" << (inventoryKeyCoverageLow ? "true" : "false")
             << " occupied_entries=" << expectedEntryCount
             << " aligned_hint_coverage=" << alignedInventoryNameHintCoverage
             << "/" << expectedEntryCount
             << " allow_indexed_hints=" << (allowIndexedNameHints ? "true" : "false")
             << " allow_opaque_indexed_hints=" << (allowOpaqueIndexedHints ? "true" : "false")
             << " opaque_prefer_mode=" << (preferCoverageFallbackWhenWidgetOpaque ? "true" : "false")
             << " widget_searchable_pre_resolve=" << widgetSearchableEntryCount
             << " sort_metric_entries=" << sortMetricEntryCount
             << " low_coverage_quantity_assist=" << (lowCoverageQuantityAssistActive ? "true" : "false")
             << " low_coverage_quantity_matches=" << lowCoverageQuantityMatchedEntryCount
             << " inventory_key_query_matches=" << inventoryKeyQueryMatches
             << " zero_match_guard_restored=" << zeroMatchGuardRestoredCount
             << " entries_root=" << SafeWidgetName(entriesRoot)
             << " backpack_content=" << SafeWidgetName(backpackContent)
             << " missing_searchable=" << missingSearchableTextCount
             << " searchable_entries=" << searchableEntryCount;
        if (hasInventoryNameKeys)
        {
            line << " inventory_source=\"" << TruncateForLog(inventorySource, 96) << "\"";
        }
        line << " sort_layout_applied=" << (sortedLayoutApplied ? "true" : "false");
        LogInfoLine(line.str());
    }

    if (hasActiveFilter && g_lastSearchSampleQueryLogged != queryLogKey)
    {
        LogSearchSampleForQuery(entriesRoot, query, 12);
        g_lastSearchSampleQueryLogged = queryLogKey;
    }

    if (hasActiveFilter && !hasAnySearchableText && g_lastZeroMatchQueryLogged != queryLogKey)
    {
        if (ShouldLogVerboseSearchDiagnostics() && hasInventoryNameKeys)
        {
            std::stringstream previewLine;
            previewLine << "search zero-match key preview query=\"" << queryLogKey << "\" "
                        << BuildKeyPreviewForLog(inventoryNameKeys, 16);
            LogWarnLine(previewLine.str());
        }
        LogSearchSampleForQuery(entriesRoot, query, 12);
        g_lastZeroMatchQueryLogged = queryLogKey;
    }
    if (hasActiveFilter && visibleCount == 0 && g_lastZeroMatchQueryLogged != queryLogKey)
    {
        if (ShouldLogVerboseSearchDiagnostics() && hasInventoryNameKeys)
        {
            std::stringstream previewLine;
            previewLine << "search zero-match key preview query=\"" << queryLogKey << "\" "
                        << BuildKeyPreviewForLog(inventoryNameKeys, 16);
            LogWarnLine(previewLine.str());
        }
        LogSearchSampleForQuery(entriesRoot, query, 12);
        g_lastZeroMatchQueryLogged = queryLogKey;
    }

    UpdateSearchCountText(visibleOccupiedCount, expectedEntryCount, visibleQuantity);
    return true;
}

void ApplySearchFilterFromControls(bool forceShowAll, bool logSummary)
{
    MyGUI::Widget* traderParent = ResolveTraderParentFromControlsContainer();
    if (traderParent == 0)
    {
        return;
    }

    if (ApplySearchFilterToTraderParent(traderParent, forceShowAll, logSummary))
    {
        g_searchFilterDirty = false;
    }
}
