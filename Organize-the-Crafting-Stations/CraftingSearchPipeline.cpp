#include "CraftingSearchPipeline.h"

#include "CraftingCore.h"
#include "CraftingInventoryBinding.h"
#include "CraftingSearchText.h"
#include "CraftingSearchUi.h"
#include "CraftingWindowDetection.h"

#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

#define g_searchFilterDirty (TraderState().search.g_searchFilterDirty)
#define g_loggedMissingBackpackForSearch (TraderState().search.g_loggedMissingBackpackForSearch)
#define g_loggedMissingSearchableItemText (TraderState().search.g_loggedMissingSearchableItemText)
#define g_searchQueryRaw (TraderState().search.g_searchQueryRaw)
#define g_searchQueryNormalized (TraderState().search.g_searchQueryNormalized)
#define g_lastZeroMatchQueryLogged (TraderState().search.g_lastZeroMatchQueryLogged)
#define g_lastObservedTraderEntriesStateSignature (TraderState().search.g_lastObservedTraderEntriesStateSignature)
#define g_lastZeroMatchGuardSignature (TraderState().search.g_lastZeroMatchGuardSignature)
#define g_lastSearchSampleQueryLogged (TraderState().search.g_lastSearchSampleQueryLogged)

#define g_loggedInventoryBindingFailure (TraderState().binding.g_loggedInventoryBindingFailure)
#define g_loggedInventoryBindingDiagnostics (TraderState().binding.g_loggedInventoryBindingDiagnostics)
#define g_lastPanelBindingRefusedSignature (TraderState().binding.g_lastPanelBindingRefusedSignature)

namespace
{
const char* kCraftingOriginalCoordUserStringKey = "OTT_CraftingOrigCoord";

struct CraftingRecipeWidget
{
    MyGUI::Widget* widget;
    MyGUI::IntCoord originalCoord;
    std::string normalizedText;
};

struct CraftingRecipeRowGroup
{
    int originalTop;
    std::vector<CraftingRecipeWidget> widgets;
    std::string normalizedText;
};

std::string SerializeCraftingCoord(const MyGUI::IntCoord& coord)
{
    std::stringstream line;
    line << coord.left << "," << coord.top << "," << coord.width << "," << coord.height;
    return line.str();
}

bool TryParseCraftingCoord(const std::string& text, MyGUI::IntCoord* outCoord)
{
    if (outCoord == 0)
    {
        return false;
    }

    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
    char comma1 = '\0';
    char comma2 = '\0';
    char comma3 = '\0';
    std::stringstream input(text);
    if (!(input >> left >> comma1 >> top >> comma2 >> width >> comma3 >> height))
    {
        return false;
    }

    if (comma1 != ',' || comma2 != ',' || comma3 != ',')
    {
        return false;
    }

    *outCoord = MyGUI::IntCoord(left, top, width, height);
    return true;
}

bool TryGetStoredCraftingOriginalCoord(MyGUI::Widget* widget, MyGUI::IntCoord* outCoord)
{
    if (widget == 0 || outCoord == 0)
    {
        return false;
    }

    const MyGUI::MapString& userStrings = widget->getUserStrings();
    const MyGUI::MapString::const_iterator it = userStrings.find(kCraftingOriginalCoordUserStringKey);
    if (it == userStrings.end())
    {
        return false;
    }

    return TryParseCraftingCoord(it->second, outCoord);
}

MyGUI::IntCoord RememberCraftingOriginalCoord(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return MyGUI::IntCoord();
    }

    MyGUI::IntCoord storedCoord;
    if (TryGetStoredCraftingOriginalCoord(widget, &storedCoord))
    {
        return storedCoord;
    }

    const MyGUI::IntCoord currentCoord = widget->getCoord();
    widget->setUserString(kCraftingOriginalCoordUserStringKey, SerializeCraftingCoord(currentCoord));
    return currentCoord;
}

std::string NormalizeCraftingRecipeCaption(const std::string& rawCaption)
{
    if (rawCaption.empty())
    {
        return "";
    }

    std::size_t begin = 0;
    while (begin < rawCaption.size() && std::isspace(static_cast<unsigned char>(rawCaption[begin])) != 0)
    {
        ++begin;
    }

    std::string trimmed = rawCaption.substr(begin);
    while (trimmed.size() >= 7 && trimmed[0] == '#')
    {
        bool hasRgbPrefix = true;
        for (std::size_t index = 1; index < 7; ++index)
        {
            if (std::isxdigit(static_cast<unsigned char>(trimmed[index])) == 0)
            {
                hasRgbPrefix = false;
                break;
            }
        }

        if (!hasRgbPrefix)
        {
            break;
        }

        trimmed = trimmed.substr(7);
    }

    return NormalizeSearchText(trimmed);
}

void CollectCraftingRecipeWidgetsRecursive(
    MyGUI::Widget* widget,
    std::size_t depth,
    std::size_t maxDepth,
    std::vector<CraftingRecipeWidget>* outWidgets)
{
    if (widget == 0 || outWidgets == 0 || depth > maxDepth)
    {
        return;
    }

    if (depth > 0)
    {
        MyGUI::IntCoord storedCoord;
        const bool hadStoredCoord = TryGetStoredCraftingOriginalCoord(widget, &storedCoord);
        if (widget->getInheritedVisible() || hadStoredCoord)
        {
            CraftingRecipeWidget entry;
            entry.widget = widget;
            entry.originalCoord = hadStoredCoord ? storedCoord : RememberCraftingOriginalCoord(widget);
            if (widget->castType<MyGUI::TextBox>(false) != 0)
            {
                entry.normalizedText = NormalizeCraftingRecipeCaption(WidgetCaptionForLog(widget));
            }
            outWidgets->push_back(entry);
        }
    }

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        CollectCraftingRecipeWidgetsRecursive(
            widget->getChildAt(childIndex),
            depth + 1,
            maxDepth,
            outWidgets);
    }
}

bool GroupContainsWidget(const CraftingRecipeRowGroup& group, MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return false;
    }

    for (std::size_t index = 0; index < group.widgets.size(); ++index)
    {
        if (group.widgets[index].widget == widget)
        {
            return true;
        }
    }
    return false;
}

int ResolveCraftingRowIndex(const std::vector<int>& rowTops, int centerY)
{
    if (rowTops.empty())
    {
        return -1;
    }

    for (std::size_t index = 0; index + 1 < rowTops.size(); ++index)
    {
        const int boundary = (rowTops[index] + rowTops[index + 1]) / 2;
        if (centerY < boundary)
        {
            return static_cast<int>(index);
        }
    }

    return static_cast<int>(rowTops.size() - 1);
}

bool BuildCraftingRecipeRowGroups(
    MyGUI::Widget* entriesRoot,
    std::vector<CraftingRecipeRowGroup>* outGroups,
    std::size_t* outTrackedWidgetCount)
{
    if (outGroups == 0)
    {
        return false;
    }

    outGroups->clear();
    if (outTrackedWidgetCount != 0)
    {
        *outTrackedWidgetCount = 0;
    }

    if (entriesRoot == 0)
    {
        return false;
    }

    std::vector<CraftingRecipeWidget> widgets;
    CollectCraftingRecipeWidgetsRecursive(entriesRoot, 0, 6, &widgets);
    if (outTrackedWidgetCount != 0)
    {
        *outTrackedWidgetCount = widgets.size();
    }

    std::vector<int> rowTops;
    int rowHeight = 0;
    for (std::size_t index = 0; index < widgets.size(); ++index)
    {
        const CraftingRecipeWidget& widget = widgets[index];
        if (widget.normalizedText.empty())
        {
            continue;
        }

        rowTops.push_back(widget.originalCoord.top);
        if (widget.originalCoord.height > rowHeight)
        {
            rowHeight = widget.originalCoord.height;
        }
    }
    if (rowTops.empty())
    {
        return false;
    }

    std::sort(rowTops.begin(), rowTops.end());
    rowTops.erase(std::unique(rowTops.begin(), rowTops.end()), rowTops.end());

    int rowPitch = rowHeight > 0 ? rowHeight : 34;
    for (std::size_t index = 1; index < rowTops.size(); ++index)
    {
        const int delta = rowTops[index] - rowTops[index - 1];
        if (delta > 0 && (delta < rowPitch || rowPitch == rowHeight))
        {
            rowPitch = delta;
        }
    }
    if (rowPitch < rowHeight)
    {
        rowPitch = rowHeight;
    }
    const int maxTrackedHeight = rowPitch * 2;

    struct CraftingRecipeRowGroupLess
    {
        bool operator()(const CraftingRecipeRowGroup& left, const CraftingRecipeRowGroup& right) const
        {
            return left.originalTop < right.originalTop;
        }
    };

    for (std::size_t index = 0; index < rowTops.size(); ++index)
    {
        CraftingRecipeRowGroup group;
        group.originalTop = rowTops[index];
        outGroups->push_back(group);
    }

    for (std::size_t index = 0; index < widgets.size(); ++index)
    {
        const CraftingRecipeWidget& widget = widgets[index];
        const MyGUI::IntCoord coord = widget.originalCoord;
        if (coord.height <= 0 || coord.height > maxTrackedHeight)
        {
            continue;
        }

        const int centerY = coord.top + coord.height / 2;
        const int rowIndex = ResolveCraftingRowIndex(rowTops, centerY);
        if (rowIndex < 0 || static_cast<std::size_t>(rowIndex) >= outGroups->size())
        {
            continue;
        }

        CraftingRecipeRowGroup& group = (*outGroups)[static_cast<std::size_t>(rowIndex)];
        group.widgets.push_back(widget);
        if (widget.normalizedText.size() > group.normalizedText.size())
        {
            group.normalizedText = widget.normalizedText;
        }
    }

    for (std::size_t groupIndex = 0; groupIndex < outGroups->size(); ++groupIndex)
    {
        CraftingRecipeRowGroup& group = (*outGroups)[groupIndex];
        std::vector<CraftingRecipeWidget> topLevelWidgets;
        topLevelWidgets.reserve(group.widgets.size());
        for (std::size_t widgetIndex = 0; widgetIndex < group.widgets.size(); ++widgetIndex)
        {
            const CraftingRecipeWidget& candidate = group.widgets[widgetIndex];
            bool hasTrackedAncestor = false;
            MyGUI::Widget* current = candidate.widget == 0 ? 0 : candidate.widget->getParent();
            while (current != 0 && current != entriesRoot)
            {
                if (GroupContainsWidget(group, current))
                {
                    hasTrackedAncestor = true;
                    break;
                }
                current = current->getParent();
            }

            if (!hasTrackedAncestor)
            {
                topLevelWidgets.push_back(candidate);
            }
        }
        group.widgets.swap(topLevelWidgets);
    }

    std::sort(outGroups->begin(), outGroups->end(), CraftingRecipeRowGroupLess());
    return !outGroups->empty();
}

MyGUI::Widget* ResolveVisibleCraftingSearchEntriesRoot(MyGUI::Widget* craftingParent, const char** outToken)
{
    if (outToken != 0)
    {
        *outToken = 0;
    }

    if (craftingParent == 0)
    {
        return 0;
    }

    const char* preferredTokens[] =
    {
        "QueueItemsPanel",
        "BlueprintsAvailablePanel",
        "CraftingStationsList",
        "ResearchQueuePanel"
    };

    MyGUI::Widget* fallback = 0;
    const char* fallbackToken = 0;
    for (std::size_t index = 0; index < sizeof(preferredTokens) / sizeof(preferredTokens[0]); ++index)
    {
        MyGUI::Widget* widget = FindWidgetInParentByToken(craftingParent, preferredTokens[index]);
        if (widget == 0)
        {
            continue;
        }

        if (fallback == 0)
        {
            fallback = widget;
            fallbackToken = preferredTokens[index];
        }

        if (!widget->getInheritedVisible())
        {
            continue;
        }

        if (outToken != 0)
        {
            *outToken = preferredTokens[index];
        }
        return widget;
    }

    if (outToken != 0)
    {
        *outToken = fallbackToken;
    }
    return fallback;
}

bool ApplySearchFilterToCraftingParent(MyGUI::Widget* craftingParent, bool forceShowAll, bool logSummary)
{
    if (craftingParent == 0)
    {
        return false;
    }

    const char* entriesRootToken = 0;
    MyGUI::Widget* entriesRoot = ResolveVisibleCraftingSearchEntriesRoot(craftingParent, &entriesRootToken);
    if (entriesRoot == 0)
    {
        UpdateSearchCountText(0, 0, 0);
        return false;
    }

    std::vector<CraftingRecipeRowGroup> rowGroups;
    std::size_t trackedWidgetCount = 0;
    if (!BuildCraftingRecipeRowGroups(entriesRoot, &rowGroups, &trackedWidgetCount))
    {
        UpdateSearchCountText(0, 0, 0);
        return false;
    }

    const std::string query = forceShowAll ? std::string() : g_searchQueryNormalized;
    std::size_t totalCount = 0;
    std::size_t visibleCount = 0;
    std::vector<int> visibleSlotTops;
    visibleSlotTops.reserve(rowGroups.size());
    for (std::size_t groupIndex = 0; groupIndex < rowGroups.size(); ++groupIndex)
    {
        visibleSlotTops.push_back(rowGroups[groupIndex].originalTop);
    }

    std::size_t nextVisibleSlot = 0;
    for (std::size_t groupIndex = 0; groupIndex < rowGroups.size(); ++groupIndex)
    {
        CraftingRecipeRowGroup& group = rowGroups[groupIndex];
        const bool hasSearchableText = !group.normalizedText.empty();
        if (hasSearchableText)
        {
            ++totalCount;
        }

        const bool shouldBeVisible =
            query.empty() || (!group.normalizedText.empty() && group.normalizedText.find(query) != std::string::npos);
        int targetTop = group.originalTop;
        if (shouldBeVisible && nextVisibleSlot < visibleSlotTops.size())
        {
            targetTop = visibleSlotTops[nextVisibleSlot];
            ++nextVisibleSlot;
        }

        const int topDelta = targetTop - group.originalTop;
        for (std::size_t widgetIndex = 0; widgetIndex < group.widgets.size(); ++widgetIndex)
        {
            CraftingRecipeWidget& widget = group.widgets[widgetIndex];
            if (widget.widget != 0)
            {
                MyGUI::IntCoord coord = widget.originalCoord;
                coord.top += topDelta;
                widget.widget->setCoord(coord);
                widget.widget->setVisible(shouldBeVisible);
            }
        }

        if (shouldBeVisible && hasSearchableText)
        {
            ++visibleCount;
        }
    }

    if (logSummary && ShouldLogSearchDebug())
    {
        std::stringstream line;
        line << "crafting search filter applied"
             << " query=\"" << (forceShowAll ? "" : g_searchQueryRaw) << "\""
             << " normalized=\"" << (forceShowAll ? "" : query) << "\""
             << " visible=" << visibleCount
             << " total=" << totalCount
             << " row_groups=" << rowGroups.size()
             << " tracked_widgets=" << trackedWidgetCount
             << " entries_root_token=" << (entriesRootToken == 0 ? "<null>" : entriesRootToken)
             << " entries_root=" << SafeWidgetName(entriesRoot);
        LogInfoLine(line.str());
    }

    UpdateSearchCountText(visibleCount, totalCount, 0);
    return true;
}
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
}

void ObserveTraderEntriesStateForRefresh()
{
    const std::string currentSignature = BuildObservedTraderEntriesStateSignature();
    if (currentSignature.empty())
    {
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
        g_lastObservedTraderEntriesStateSignature = currentSignature;
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
        UpdateSearchCountText(0, 0, 0);
        return false;
    }
    g_loggedMissingBackpackForSearch = false;

    MyGUI::Widget* entriesRoot = ResolveInventoryEntriesRoot(backpackContent);
    if (entriesRoot == 0)
    {
        UpdateSearchCountText(0, 0, 0);
        return false;
    }

    const std::string query = forceShowAll ? std::string() : g_searchQueryNormalized;
    std::size_t totalCount = 0;
    std::size_t visibleCount = 0;
    std::size_t missingSearchableTextCount = 0;
    std::size_t fallbackKeptVisibleCount = 0;
    std::size_t itemNameHintCount = 0;

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
    if (!query.empty())
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
        !query.empty() && expectedEntryCount >= 8 && widgetSearchableEntryCount == 0;

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

    if (!query.empty() && !hasPanelBinding)
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
        signature << query
                  << "|" << panelBindingStatus
                  << "|" << expectedEntryCount
                  << "|" << SafeWidgetName(traderParent);
        if (signature.str() != g_lastPanelBindingRefusedSignature)
        {
            std::stringstream line;
            line << "search refused: missing high-confidence panel inventory binding"
                 << " query=\"" << query << "\""
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
        if (g_lastSearchSampleQueryLogged != query)
        {
            LogSearchSampleForQuery(entriesRoot, query, 12);
            g_lastSearchSampleQueryLogged = query;
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
    std::string inventorySource;
    bool hasInventoryNameKeys = false;
    if (hasPanelBinding && panelBinding.inventory != 0)
    {
        hasInventoryNameKeys =
            TryExtractSearchKeysFromInventory(panelBinding.inventory, &inventoryNameKeys);
        TryExtractQuantityNameKeysFromInventory(panelBinding.inventory, &inventoryQuantityNameKeys);
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

    if (!query.empty() && (!hasInventoryNameKeys || panelBindingLowCoverage))
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
        signature << query
                  << "|" << refusalReason
                  << "|" << panelBindingNonEmptyKeyCount
                  << "|" << expectedEntryCount
                  << "|" << SafeWidgetName(traderParent);
        if (signature.str() != g_lastPanelBindingRefusedSignature)
        {
            std::stringstream line;
            line << "search refused: panel binding confidence too low"
                 << " query=\"" << query << "\""
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

    if (!hasInventoryNameKeys && !query.empty() && !g_loggedInventoryBindingFailure)
    {
        LogWarnLine("could not resolve inventory-backed name keys for the current crafting list; search is using widget-only metadata");
        g_loggedInventoryBindingFailure = true;
    }
    if (hasInventoryNameKeys)
    {
        g_loggedInventoryBindingFailure = false;
    }
    if (!hasInventoryNameKeys && !query.empty() && ShouldLogBindingDebug() && !g_loggedInventoryBindingDiagnostics)
    {
        LogInventoryBindingDiagnostics(expectedEntryCount);
        g_loggedInventoryBindingDiagnostics = true;
    }
    if (hasPanelBinding && hasInventoryNameKeys)
    {
        g_loggedInventoryBindingFailure = false;
    }
    if (query.empty())
    {
        g_loggedInventoryBindingDiagnostics = false;
    }

    struct EntryFilterState
    {
        MyGUI::Widget* widget;
        std::string searchableText;
        bool occupied;
        int quantity;
        bool lowCoverageQuantityMatched;
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

        if (!sequenceAlignedNameHint.empty())
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
        state.occupied = occupied;
        state.quantity = ordered.quantity;
        state.lowCoverageQuantityMatched = false;
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

        if (!query.empty())
        {
            if (lowCoverageQuantityAssistActive)
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
            signature << query
                      << "|" << inventorySource
                      << "|" << (inventoryKeyCoverageLow ? "1" : "0")
                      << "|" << (lowAlignmentConfidence ? "1" : "0")
                      << "|" << zeroMatchGuardRestoredCount;
            if (signature.str() != g_lastZeroMatchGuardSignature)
            {
                std::stringstream line;
                line << "search zero-match guard restored occupied entries="
                     << zeroMatchGuardRestoredCount
                     << " query=\"" << query << "\""
                     << " inventory_low_coverage=" << (inventoryKeyCoverageLow ? "true" : "false")
                     << " low_alignment_confidence=" << (lowAlignmentConfidence ? "true" : "false")
                     << " source=\"" << TruncateForLog(inventorySource, 120) << "\"";
                LogWarnLine(line.str());
                g_lastZeroMatchGuardSignature = signature.str();
            }
        }
    }

    if (!query.empty() && fallbackKeptVisibleCount > 0 && !g_loggedMissingSearchableItemText)
    {
        std::stringstream line;
        line << "search fallback: kept " << fallbackKeptVisibleCount
             << " entries visible because searchable text was missing";
        LogWarnLine(line.str());
        g_loggedMissingSearchableItemText = true;
    }
    if (query.empty())
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
        LogInfoLine(line.str());
    }

    if (!query.empty() && g_lastSearchSampleQueryLogged != query)
    {
        LogSearchSampleForQuery(entriesRoot, query, 12);
        g_lastSearchSampleQueryLogged = query;
    }

    if (!query.empty() && !hasAnySearchableText && g_lastZeroMatchQueryLogged != query)
    {
        if (ShouldLogVerboseSearchDiagnostics() && hasInventoryNameKeys)
        {
            std::stringstream previewLine;
            previewLine << "search zero-match key preview query=\"" << query << "\" "
                        << BuildKeyPreviewForLog(inventoryNameKeys, 16);
            LogWarnLine(previewLine.str());
        }
        LogSearchSampleForQuery(entriesRoot, query, 12);
        g_lastZeroMatchQueryLogged = query;
    }
    if (!query.empty() && visibleCount == 0 && g_lastZeroMatchQueryLogged != query)
    {
        if (ShouldLogVerboseSearchDiagnostics() && hasInventoryNameKeys)
        {
            std::stringstream previewLine;
            previewLine << "search zero-match key preview query=\"" << query << "\" "
                        << BuildKeyPreviewForLog(inventoryNameKeys, 16);
            LogWarnLine(previewLine.str());
        }
        LogSearchSampleForQuery(entriesRoot, query, 12);
        g_lastZeroMatchQueryLogged = query;
    }

    UpdateSearchCountText(visibleOccupiedCount, expectedEntryCount, visibleQuantity);
    return true;
}

void ApplySearchFilterFromControls(bool forceShowAll, bool logSummary)
{
    MyGUI::Widget* targetParent = ResolveTraderParentFromControlsContainer();
    if (targetParent == 0)
    {
        return;
    }

    if (FindWidgetInParentByToken(targetParent, "QueueItemsPanel") != 0)
    {
        if (ApplySearchFilterToCraftingParent(targetParent, forceShowAll, logSummary))
        {
            g_searchFilterDirty = false;
        }
        return;
    }

    if (ApplySearchFilterToTraderParent(targetParent, forceShowAll, logSummary))
    {
        g_searchFilterDirty = false;
    }
}
