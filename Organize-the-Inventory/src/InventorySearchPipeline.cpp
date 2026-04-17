#include "InventorySearchPipeline.h"

#include "InventoryBackpackBinding.h"
#include "InventoryBinding.h"
#include "InventoryCore.h"
#include "InventorySearchText.h"
#include "InventorySearchUi.h"
#include "InventoryWindowDetection.h"

#include <kenshi/Item.h>

#include <mygui/MyGUI_Widget.h>

#include <Windows.h>

#include <cctype>
#include <sstream>
#include <vector>

namespace
{
struct ParsedSearchQuery
{
    ParsedSearchQuery()
        : blueprintOnly(false)
    {
    }

    bool blueprintOnly;
    std::string normalizedQuery;
};

struct InventorySearchEntry
{
    InventorySearchEntry()
        : widget(0)
        , item(0)
        , quantity(0)
    {
    }

    MyGUI::Widget* widget;
    Item* item;
    int quantity;
};

MyGUI::Widget* g_lastFilteredParent = 0;
std::vector<MyGUI::Widget*> g_lastFilteredEntryWidgets;
std::string g_lastFilterSummarySignature;
std::string g_lastInvestigateEnterSignature;
std::string g_lastInvestigateEmptyScanSignature;
std::string g_lastInvestigateBoundScanSignature;

bool TrySetWidgetVisibleSafe(MyGUI::Widget* widget, bool visible)
{
    if (widget == 0)
    {
        return false;
    }

    __try
    {
        widget->setVisible(visible);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void RestoreTrackedEntryVisibility()
{
    for (std::size_t index = 0; index < g_lastFilteredEntryWidgets.size(); ++index)
    {
        TrySetWidgetVisibleSafe(g_lastFilteredEntryWidgets[index], true);
    }

    g_lastFilteredEntryWidgets.clear();
    g_lastFilteredParent = 0;
}

bool ContainsWidgetPointer(
    const std::vector<MyGUI::Widget*>& widgets,
    MyGUI::Widget* candidate)
{
    if (candidate == 0)
    {
        return true;
    }

    for (std::size_t index = 0; index < widgets.size(); ++index)
    {
        if (widgets[index] == candidate)
        {
            return true;
        }
    }

    return false;
}

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
        parsed.normalizedQuery = NormalizeInventorySearchText(trimmedQuery.substr(2));
        return parsed;
    }

    parsed.normalizedQuery = NormalizeInventorySearchText(trimmedQuery);
    return parsed;
}

bool SearchTextMatchesBlueprintFilter(const std::string& searchableTextNormalized)
{
    return searchableTextNormalized.find("blueprint") != std::string::npos;
}

bool ShouldShowAnySearchCountMetric()
{
    return InventoryState().g_showSearchEntryCount || InventoryState().g_showSearchQuantityCount;
}

std::string BuildSearchCountCaption(
    std::size_t visibleEntryCount,
    std::size_t totalEntryCount,
    std::size_t visibleQuantity)
{
    std::stringstream line;
    bool wroteMetric = false;

    if (InventoryState().g_showSearchEntryCount)
    {
        line << visibleEntryCount << " / " << totalEntryCount;
        wroteMetric = true;
    }

    if (InventoryState().g_showSearchQuantityCount)
    {
        if (wroteMetric)
        {
            line << " | ";
        }
        line << "qty " << visibleQuantity;
    }

    return line.str();
}

MyGUI::Widget* ResolveTopmostItemEntryWidget(
    MyGUI::Widget* widget,
    MyGUI::Widget* scopeRoot)
{
    if (widget == 0 || scopeRoot == 0)
    {
        return 0;
    }

    Item* item = ResolveInventoryWidgetItemPointer(widget);
    if (item == 0)
    {
        return 0;
    }

    MyGUI::Widget* entryWidget = widget;
    MyGUI::Widget* current = widget->getParent();
    while (current != 0)
    {
        if (ResolveInventoryWidgetItemPointer(current) != item)
        {
            break;
        }

        entryWidget = current;
        if (current == scopeRoot)
        {
            break;
        }

        current = current->getParent();
    }

    return entryWidget;
}

MyGUI::Widget* ResolveFilterVisibilityWidget(
    MyGUI::Widget* widget,
    MyGUI::Widget* scopeRoot)
{
    if (widget == 0)
    {
        return 0;
    }

    MyGUI::Widget* resolved = ResolveTopmostItemEntryWidget(widget, scopeRoot);
    return resolved != 0 ? resolved : widget;
}

Item* ResolveSearchEntryItemPointerRecursive(
    MyGUI::Widget* widget,
    std::size_t depth,
    std::size_t maxDepth)
{
    if (widget == 0 || depth > maxDepth)
    {
        return 0;
    }

    Item* item = ResolveInventoryWidgetItemPointer(widget);
    if (item != 0)
    {
        return item;
    }

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        item = ResolveSearchEntryItemPointerRecursive(
            widget->getChildAt(childIndex),
            depth + 1,
            maxDepth);
        if (item != 0)
        {
            return item;
        }
    }

    return 0;
}

Item* ResolveSearchEntryItemPointer(MyGUI::Widget* widget)
{
    return ResolveSearchEntryItemPointerRecursive(widget, 0, 5);
}

void CollectVisibleEntryWidgetsRecursive(
    MyGUI::Widget* widget,
    MyGUI::Widget* scopeRoot,
    std::vector<MyGUI::Widget*>* outWidgets)
{
    if (widget == 0 || scopeRoot == 0 || outWidgets == 0)
    {
        return;
    }

    if (!widget->getInheritedVisible())
    {
        return;
    }

    Item* item = ResolveInventoryWidgetItemPointer(widget);
    if (item != 0)
    {
        MyGUI::Widget* entryWidget = ResolveTopmostItemEntryWidget(widget, scopeRoot);
        if (!ContainsWidgetPointer(*outWidgets, entryWidget))
        {
            outWidgets->push_back(entryWidget);
        }
        return;
    }

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        CollectVisibleEntryWidgetsRecursive(
            widget->getChildAt(childIndex),
            scopeRoot,
            outWidgets);
    }
}

int ResolveEntryQuantity(MyGUI::Widget* widget, Item* item)
{
    int quantity = 0;
    if (TryResolveInventoryItemQuantityFromWidget(widget, &quantity) && quantity > 0)
    {
        return quantity;
    }

    if (item != 0 && item->quantity > 0)
    {
        return item->quantity;
    }

    return item != 0 ? 1 : 0;
}

void AddSearchEntryUnique(
    std::vector<InventorySearchEntry>* entries,
    MyGUI::Widget* widget,
    Item* item,
    int quantity)
{
    if (entries == 0 || widget == 0)
    {
        return;
    }

    for (std::size_t index = 0; index < entries->size(); ++index)
    {
        const InventorySearchEntry& existingEntry = (*entries)[index];
        if (existingEntry.widget == widget)
        {
            InventorySearchEntry& existingMutable = (*entries)[index];
            if (existingMutable.item == 0 && item != 0)
            {
                existingMutable.item = item;
            }
            if (quantity > existingMutable.quantity)
            {
                existingMutable.quantity = quantity;
            }
            return;
        }

        if (item != 0 && existingEntry.item == item)
        {
            return;
        }
    }

    InventorySearchEntry entry;
    entry.widget = widget;
    entry.item = item;
    entry.quantity = quantity;
    entries->push_back(entry);
}

void CollectSearchEntriesFromItemWidgets(
    MyGUI::Widget* inventoryParent,
    std::vector<InventorySearchEntry>* outEntries)
{
    if (inventoryParent == 0 || outEntries == 0)
    {
        return;
    }

    std::vector<MyGUI::Widget*> entryWidgets;
    CollectVisibleEntryWidgetsRecursive(inventoryParent, inventoryParent, &entryWidgets);
    for (std::size_t index = 0; index < entryWidgets.size(); ++index)
    {
        MyGUI::Widget* entryWidget = entryWidgets[index];
        if (entryWidget == 0)
        {
            continue;
        }

        Item* item = ResolveSearchEntryItemPointer(entryWidget);
        AddSearchEntryUnique(
            outEntries,
            entryWidget,
            item,
            ResolveEntryQuantity(entryWidget, item));
    }
}

void CollectSearchEntriesFromEntriesRootChildren(
    MyGUI::Widget* entriesRoot,
    std::vector<InventorySearchEntry>* outEntries)
{
    if (entriesRoot == 0 || outEntries == 0)
    {
        return;
    }

    const std::size_t childCount = entriesRoot->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        MyGUI::Widget* child = entriesRoot->getChildAt(childIndex);
        if (child == 0 || !child->getInheritedVisible())
        {
            continue;
        }

        Item* item = ResolveSearchEntryItemPointer(child);
        AddSearchEntryUnique(
            outEntries,
            child,
            item,
            ResolveEntryQuantity(child, item));
    }
}

void MergeBoundSearchEntriesForRoot(
    MyGUI::Widget* inventoryRoot,
    std::vector<InventorySearchEntry>* outEntries)
{
    if (inventoryRoot == 0 || outEntries == 0)
    {
        return;
    }

    std::vector<InventoryBoundEntry> boundEntries;
    std::string ignoredReason;
    if (!CollectBoundInventoryEntriesForRoot(inventoryRoot, &boundEntries, &ignoredReason))
    {
        return;
    }

    for (std::size_t index = 0; index < boundEntries.size(); ++index)
    {
        const InventoryBoundEntry& boundEntry = boundEntries[index];
        MyGUI::Widget* visibilityWidget =
            ResolveFilterVisibilityWidget(boundEntry.widget, inventoryRoot);
        Item* item = boundEntry.item != 0
            ? boundEntry.item
            : ResolveSearchEntryItemPointer(visibilityWidget);
        AddSearchEntryUnique(
            outEntries,
            visibilityWidget,
            item,
            ResolveEntryQuantity(visibilityWidget, item));
    }
}

void CollectSearchEntriesFromBackpackPanels(
    MyGUI::Widget* inventoryParent,
    std::vector<InventorySearchEntry>* outEntries)
{
    if (inventoryParent == 0 || outEntries == 0)
    {
        return;
    }

    std::vector<MyGUI::Widget*> backpackContents;
    CollectVisibleWidgetsByToken("backpack_content", &backpackContents);
    for (std::size_t index = 0; index < backpackContents.size(); ++index)
    {
        MyGUI::Widget* backpackContent = backpackContents[index];
        if (backpackContent == 0 || !backpackContent->getInheritedVisible())
        {
            continue;
        }

        MyGUI::Widget* entriesRoot = ResolveInventoryEntriesRoot(backpackContent);
        if (entriesRoot == 0)
        {
            entriesRoot = backpackContent;
        }

        std::vector<InventoryBoundEntry> backpackBoundEntries;
        std::string ignoredBackpackReason;
        if (CollectBoundBackpackEntriesForContent(
                backpackContent,
                &backpackBoundEntries,
                &ignoredBackpackReason))
        {
            for (std::size_t boundIndex = 0; boundIndex < backpackBoundEntries.size(); ++boundIndex)
            {
                const InventoryBoundEntry& boundEntry = backpackBoundEntries[boundIndex];
                AddSearchEntryUnique(
                    outEntries,
                    boundEntry.widget,
                    boundEntry.item,
                    boundEntry.quantity);
            }
        }
        else
        {
            const std::size_t entryCountBeforeChildren = outEntries->size();
            CollectSearchEntriesFromEntriesRootChildren(entriesRoot, outEntries);

            if (outEntries->size() == entryCountBeforeChildren)
            {
                std::vector<MyGUI::Widget*> entryWidgets;
                CollectLikelyInventoryEntryWidgets(entriesRoot, &entryWidgets);
                for (std::size_t entryIndex = 0; entryIndex < entryWidgets.size(); ++entryIndex)
                {
                    // Backpack probes often find inner visuals first; hide the outer item widget instead.
                    MyGUI::Widget* entryWidget =
                        ResolveFilterVisibilityWidget(entryWidgets[entryIndex], entriesRoot);
                    if (entryWidget == 0)
                    {
                        continue;
                    }

                    Item* item = ResolveSearchEntryItemPointer(entryWidget);
                    AddSearchEntryUnique(
                        outEntries,
                        entryWidget,
                        item,
                        ResolveEntryQuantity(entryWidget, item));
                }
            }
        }

        MergeBoundSearchEntriesForRoot(backpackContent, outEntries);
        if (entriesRoot != backpackContent)
        {
            MergeBoundSearchEntriesForRoot(entriesRoot, outEntries);
        }
    }
}

void LogInvestigateBoundScanIfNeeded(
    MyGUI::Widget* inventoryParent,
    const ParsedSearchQuery& parsedQuery,
    const std::string& reason,
    std::size_t entryCount)
{
    if (!ShouldLogSearchDebug() || inventoryParent == 0)
    {
        g_lastInvestigateBoundScanSignature.clear();
        return;
    }

    std::stringstream signature;
    signature << inventoryParent
              << "|" << parsedQuery.blueprintOnly
              << "|" << parsedQuery.normalizedQuery
              << "|" << entryCount
              << "|" << reason;
    if (signature.str() == g_lastInvestigateBoundScanSignature)
    {
        return;
    }
    g_lastInvestigateBoundScanSignature = signature.str();

    std::stringstream line;
    line << "[investigate][inventory-search] bound_scan"
         << " root=" << inventoryParent
         << " name=" << inventoryParent->getName()
         << " query=\"" << parsedQuery.normalizedQuery << "\""
         << " blueprint_only=" << (parsedQuery.blueprintOnly ? "true" : "false")
         << " entries=" << entryCount
         << " reason=\"" << reason << "\"";
    LogSearchDebugLine(line.str());
}

struct InventoryScanProbe
{
    InventoryScanProbe()
        : visitedCount(0)
        , visibleCount(0)
        , itemPointerCount(0)
        , namedCount(0)
    {
    }

    std::size_t visitedCount;
    std::size_t visibleCount;
    std::size_t itemPointerCount;
    std::size_t namedCount;
    std::string firstNamedWidget;
    std::string firstItemWidget;
};

void ProbeInventoryWidgetTreeRecursive(MyGUI::Widget* widget, InventoryScanProbe* probe)
{
    if (widget == 0 || probe == 0)
    {
        return;
    }

    ++probe->visitedCount;
    if (widget->getInheritedVisible())
    {
        ++probe->visibleCount;
    }

    const std::string widgetName = widget->getName();
    if (!widgetName.empty())
    {
        ++probe->namedCount;
        if (probe->firstNamedWidget.empty())
        {
            probe->firstNamedWidget = widgetName;
        }
    }

    if (ResolveInventoryWidgetItemPointer(widget) != 0)
    {
        ++probe->itemPointerCount;
        if (probe->firstItemWidget.empty())
        {
            probe->firstItemWidget = widgetName.empty() ? "<unnamed>" : widgetName;
        }
    }

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        ProbeInventoryWidgetTreeRecursive(widget->getChildAt(childIndex), probe);
    }
}

void LogInvestigateEntryIfNeeded(
    MyGUI::Widget* inventoryParent,
    bool forceShowAll,
    const ParsedSearchQuery& parsedQuery)
{
    if (!ShouldLogSearchDebug() || inventoryParent == 0)
    {
        g_lastInvestigateEnterSignature.clear();
        return;
    }

    std::stringstream signature;
    signature << inventoryParent
              << "|" << forceShowAll
              << "|" << parsedQuery.blueprintOnly
              << "|" << parsedQuery.normalizedQuery;
    if (signature.str() == g_lastInvestigateEnterSignature)
    {
        return;
    }
    g_lastInvestigateEnterSignature = signature.str();

    const MyGUI::IntCoord coord = inventoryParent->getCoord();
    std::stringstream line;
    line << "[investigate][inventory-search] enter"
         << " root=" << inventoryParent
         << " name=" << inventoryParent->getName()
         << " force_show_all=" << (forceShowAll ? "true" : "false")
         << " blueprint_only=" << (parsedQuery.blueprintOnly ? "true" : "false")
         << " query=\"" << parsedQuery.normalizedQuery << "\""
         << " child_count=" << inventoryParent->getChildCount()
         << " visible=" << (inventoryParent->getInheritedVisible() ? "true" : "false")
         << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height
         << ")";
    LogSearchDebugLine(line.str());
}

void LogInvestigateEmptyScanIfNeeded(
    MyGUI::Widget* inventoryParent,
    const ParsedSearchQuery& parsedQuery)
{
    if (!ShouldLogSearchDebug() || inventoryParent == 0)
    {
        g_lastInvestigateEmptyScanSignature.clear();
        return;
    }

    InventoryScanProbe probe;
    ProbeInventoryWidgetTreeRecursive(inventoryParent, &probe);

    std::stringstream signature;
    signature << inventoryParent
              << "|" << parsedQuery.blueprintOnly
              << "|" << parsedQuery.normalizedQuery
              << "|" << probe.visitedCount
              << "|" << probe.visibleCount
              << "|" << probe.itemPointerCount
              << "|" << probe.namedCount;
    if (signature.str() == g_lastInvestigateEmptyScanSignature)
    {
        return;
    }
    g_lastInvestigateEmptyScanSignature = signature.str();

    std::stringstream line;
    line << "[investigate][inventory-search] empty_scan"
         << " root=" << inventoryParent
         << " name=" << inventoryParent->getName()
         << " query=\"" << parsedQuery.normalizedQuery << "\""
         << " blueprint_only=" << (parsedQuery.blueprintOnly ? "true" : "false")
         << " visited=" << probe.visitedCount
         << " visible_nodes=" << probe.visibleCount
         << " named_nodes=" << probe.namedCount
         << " item_pointer_hits=" << probe.itemPointerCount
         << " first_named=" << (probe.firstNamedWidget.empty() ? "<none>" : probe.firstNamedWidget)
         << " first_item_widget=" << (probe.firstItemWidget.empty() ? "<none>" : probe.firstItemWidget);
    LogSearchDebugLine(line.str());
}

void LogFilterSummaryIfChanged(
    MyGUI::Widget* inventoryParent,
    const ParsedSearchQuery& parsedQuery,
    std::size_t totalEntryCount,
    std::size_t visibleEntryCount,
    std::size_t visibleQuantity)
{
    if (!ShouldLogSearchDebug())
    {
        g_lastFilterSummarySignature.clear();
        return;
    }

    std::stringstream signature;
    signature << inventoryParent
              << "|" << parsedQuery.blueprintOnly
              << "|" << parsedQuery.normalizedQuery
              << "|" << totalEntryCount
              << "|" << visibleEntryCount
              << "|" << visibleQuantity;
    if (signature.str() == g_lastFilterSummarySignature)
    {
        return;
    }

    g_lastFilterSummarySignature = signature.str();

    std::stringstream line;
    line << "inventory search filter applied"
         << " parent=" << inventoryParent
         << " query=\"" << parsedQuery.normalizedQuery << "\""
         << " blueprint_only=" << (parsedQuery.blueprintOnly ? "true" : "false")
         << " total_entries=" << totalEntryCount
         << " visible_entries=" << visibleEntryCount
         << " visible_quantity=" << visibleQuantity;
    LogSearchDebugLine(line.str());
}
}

bool ApplyInventorySearchFilterToParent(MyGUI::Widget* inventoryParent, bool forceShowAll)
{
    if (inventoryParent == 0)
    {
        ClearInventorySearchFilterState();
        return false;
    }

    ParsedSearchQuery parsedQuery = forceShowAll
        ? ParsedSearchQuery()
        : ParseSearchQuery(InventoryState().g_searchQueryRaw);
    LogInvestigateEntryIfNeeded(inventoryParent, forceShowAll, parsedQuery);

    RestoreTrackedEntryVisibility();

    std::vector<InventorySearchEntry> entries;
    CollectSearchEntriesFromItemWidgets(inventoryParent, &entries);

    std::vector<InventoryBoundEntry> boundEntries;
    std::string boundReason;
    if (CollectBoundInventoryEntriesForRoot(inventoryParent, &boundEntries, &boundReason))
    {
        const std::size_t entryCountBeforeBoundMerge = entries.size();
        for (std::size_t index = 0; index < boundEntries.size(); ++index)
        {
            const InventoryBoundEntry& boundEntry = boundEntries[index];
            MyGUI::Widget* visibilityWidget =
                ResolveFilterVisibilityWidget(boundEntry.widget, inventoryParent);
            Item* item = boundEntry.item != 0
                ? boundEntry.item
                : ResolveSearchEntryItemPointer(visibilityWidget);
            AddSearchEntryUnique(
                &entries,
                visibilityWidget,
                item,
                ResolveEntryQuantity(visibilityWidget, item));
        }

        if (entries.size() != entryCountBeforeBoundMerge || entryCountBeforeBoundMerge == 0)
        {
            LogInvestigateBoundScanIfNeeded(
                inventoryParent,
                parsedQuery,
                boundReason,
                entries.size());
        }
    }

    CollectSearchEntriesFromBackpackPanels(inventoryParent, &entries);

    if (entries.empty())
    {
        LogInvestigateEmptyScanIfNeeded(inventoryParent, parsedQuery);
        SetInventorySearchCountDisplay("", false);
        g_lastFilterSummarySignature.clear();
        return false;
    }

    const bool hasActiveFilter =
        parsedQuery.blueprintOnly || !parsedQuery.normalizedQuery.empty();

    std::size_t totalEntryCount = 0;
    std::size_t visibleEntryCount = 0;
    std::size_t visibleQuantity = 0;
    g_lastFilteredEntryWidgets.clear();
    g_lastFilteredEntryWidgets.reserve(entries.size());

    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        const InventorySearchEntry& entry = entries[index];
        MyGUI::Widget* entryWidget = entry.widget;
        if (entryWidget == 0)
        {
            continue;
        }

        ++totalEntryCount;

        bool visible = true;
        if (hasActiveFilter)
        {
            const std::string normalizedSearchText =
                NormalizeInventorySearchText(
                    BuildInventoryItemSearchTextFromResolvedItem(entryWidget, entry.item));
            visible = InventorySearchTextMatchesQuery(
                normalizedSearchText,
                parsedQuery.normalizedQuery);
            if (visible && parsedQuery.blueprintOnly)
            {
                visible = SearchTextMatchesBlueprintFilter(normalizedSearchText);
            }
        }

        TrySetWidgetVisibleSafe(entryWidget, visible);
        g_lastFilteredEntryWidgets.push_back(entryWidget);

        if (visible)
        {
            ++visibleEntryCount;
            if (entry.quantity > 0)
            {
                visibleQuantity += static_cast<std::size_t>(entry.quantity);
            }
        }
    }

    g_lastFilteredParent = inventoryParent;

    if (ShouldShowAnySearchCountMetric())
    {
        SetInventorySearchCountDisplay(
            BuildSearchCountCaption(visibleEntryCount, totalEntryCount, visibleQuantity),
            true);
    }
    else
    {
        SetInventorySearchCountDisplay("", false);
    }

    LogFilterSummaryIfChanged(
        inventoryParent,
        parsedQuery,
        totalEntryCount,
        visibleEntryCount,
        visibleQuantity);
    return true;
}

void ClearInventorySearchFilterState()
{
    RestoreTrackedEntryVisibility();
    g_lastFilterSummarySignature.clear();
    g_lastInvestigateEnterSignature.clear();
    g_lastInvestigateEmptyScanSignature.clear();
    g_lastInvestigateBoundScanSignature.clear();
    SetInventorySearchCountDisplay("", false);
}
