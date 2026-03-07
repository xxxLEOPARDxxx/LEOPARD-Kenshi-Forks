#include "TraderDiagnostics.h"

#include "TraderCore.h"
#include "TraderInventoryBinding.h"
#include "TraderSearchPipeline.h"
#include "TraderSearchText.h"
#include "TraderSearchUi.h"
#include "TraderWindowDetection.h"

#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Widget.h>

#include <algorithm>
#include <sstream>
#include <vector>

namespace
{
void DumpVisibleRootWidgetsForDiagnostics()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        LogWarnLine("GUI singleton unavailable while dumping root widgets");
        return;
    }

    std::size_t index = 0;
    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        const MyGUI::IntCoord coord = root->getCoord();
        std::stringstream line;
        line << "root[" << index << "] name=" << SafeWidgetName(root)
             << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";
        LogInfoLine(line.str());
        ++index;
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

    LogSearchSampleForQuery(entriesRoot, state.search.g_searchQueryNormalized, 12);

    LogInventoryBindingDiagnostics(orderedEntries.size());
}
