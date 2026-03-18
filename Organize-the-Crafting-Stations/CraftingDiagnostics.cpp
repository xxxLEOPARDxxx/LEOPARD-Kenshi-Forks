#include "CraftingDiagnostics.h"

#include "CraftingCore.h"
#include "CraftingInventoryBinding.h"
#include "CraftingSearchPipeline.h"
#include "CraftingSearchText.h"
#include "CraftingSearchUi.h"
#include "CraftingWindowDetection.h"

#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>

#include <algorithm>
#include <sstream>
#include <vector>

namespace
{
struct CraftingRowScanCandidate
{
    MyGUI::Widget* widget;
    std::size_t depth;
    int score;
    int quantity;
    std::string type;
    std::string caption;
    std::string nameHint;
    std::string searchText;
    std::string rawProbe;
};

void LogCraftingProbeWidgetSummary(const char* token, MyGUI::Widget* widget)
{
    std::stringstream line;
    line << "manual diagnostics crafting widget"
         << " token=" << (token == 0 ? "<null>" : token)
         << " found=" << (widget != 0 ? "true" : "false");

    if (widget != 0)
    {
        const MyGUI::IntCoord coord = widget->getCoord();
        line << " name=" << SafeWidgetName(widget)
             << " type=" << WidgetTypeForLog(widget)
             << " visible=" << (widget->getInheritedVisible() ? "true" : "false")
             << " children=" << widget->getChildCount()
             << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";

        const std::string caption = TruncateForLog(WidgetCaptionForLog(widget), 60);
        if (!caption.empty())
        {
            line << " caption=\"" << caption << "\"";
        }
    }

    LogWarnLine(line.str());
}

int ComputeCraftingRowScanCandidateScore(
    MyGUI::Widget* widget,
    std::size_t depth,
    std::string* outCaption,
    std::string* outNameHint,
    std::string* outSearchText,
    std::string* outRawProbe,
    int* outQuantity)
{
    if (outCaption != 0)
    {
        outCaption->clear();
    }
    if (outNameHint != 0)
    {
        outNameHint->clear();
    }
    if (outSearchText != 0)
    {
        outSearchText->clear();
    }
    if (outRawProbe != 0)
    {
        outRawProbe->clear();
    }
    if (outQuantity != 0)
    {
        *outQuantity = 0;
    }

    if (widget == 0 || !widget->getInheritedVisible())
    {
        return 0;
    }

    const std::string caption = NormalizeSearchText(WidgetCaptionForLog(widget));
    const std::string nameHint = NormalizeSearchText(ResolveItemNameHintRecursive(widget, 0, 5));
    const std::string searchText = NormalizeSearchText(BuildItemSearchText(widget));
    const std::string rawProbe = BuildItemRawProbe(widget);
    int quantity = 0;
    TryResolveItemQuantityFromWidget(widget, &quantity);

    int score = 0;
    if (!caption.empty())
    {
        score += 2600;
    }
    if (!nameHint.empty())
    {
        score += 1800;
    }
    if (!searchText.empty())
    {
        score += 1200;
    }
    if (quantity > 0)
    {
        score += 900;
    }

    const std::string type = WidgetTypeForLog(widget);
    if (type == "Button")
    {
        score += 700;
    }
    else if (type == "TextBox")
    {
        score += 250;
    }

    const std::size_t childCount = widget->getChildCount();
    if (childCount > 0 && childCount <= 8)
    {
        score += static_cast<int>(childCount) * 90;
    }
    if (depth >= 1 && depth <= 4)
    {
        score += 220;
    }
    if (ContainsAsciiCaseInsensitive(SafeWidgetName(widget), "queue")
        || ContainsAsciiCaseInsensitive(SafeWidgetName(widget), "item")
        || ContainsAsciiCaseInsensitive(SafeWidgetName(widget), "craft"))
    {
        score += 180;
    }

    if (score <= 0)
    {
        return 0;
    }

    if (outCaption != 0)
    {
        *outCaption = caption;
    }
    if (outNameHint != 0)
    {
        *outNameHint = nameHint;
    }
    if (outSearchText != 0)
    {
        *outSearchText = searchText;
    }
    if (outRawProbe != 0)
    {
        *outRawProbe = rawProbe;
    }
    if (outQuantity != 0)
    {
        *outQuantity = quantity;
    }

    return score;
}

void CollectCraftingRowScanCandidatesRecursive(
    MyGUI::Widget* widget,
    std::size_t depth,
    std::size_t maxDepth,
    std::vector<CraftingRowScanCandidate>* outCandidates)
{
    if (widget == 0 || outCandidates == 0 || depth > maxDepth)
    {
        return;
    }

    if (depth > 0)
    {
        CraftingRowScanCandidate candidate;
        candidate.widget = widget;
        candidate.depth = depth;
        candidate.score = ComputeCraftingRowScanCandidateScore(
            widget,
            depth,
            &candidate.caption,
            &candidate.nameHint,
            &candidate.searchText,
            &candidate.rawProbe,
            &candidate.quantity);
        candidate.type = WidgetTypeForLog(widget);

        if (candidate.score >= 1200)
        {
            outCandidates->push_back(candidate);
        }
    }

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        CollectCraftingRowScanCandidatesRecursive(
            widget->getChildAt(childIndex),
            depth + 1,
            maxDepth,
            outCandidates);
    }
}

void DumpCraftingRowScan(const char* rootToken, MyGUI::Widget* root)
{
    std::stringstream summary;
    summary << "craft_search.row_scan"
            << " root_token=" << (rootToken == 0 ? "<null>" : rootToken)
            << " found=" << (root != 0 ? "true" : "false");

    if (root == 0)
    {
        LogWarnLine(summary.str());
        return;
    }

    const MyGUI::IntCoord coord = root->getCoord();
    summary << " name=" << SafeWidgetName(root)
            << " type=" << WidgetTypeForLog(root)
            << " visible=" << (root->getInheritedVisible() ? "true" : "false")
            << " children=" << root->getChildCount()
            << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";
    LogWarnLine(summary.str());

    std::vector<CraftingRowScanCandidate> candidates;
    CollectCraftingRowScanCandidatesRecursive(root, 0, 6, &candidates);

    struct CraftingRowScanCandidateLess
    {
        bool operator()(const CraftingRowScanCandidate& left, const CraftingRowScanCandidate& right) const
        {
            if (left.score != right.score)
            {
                return left.score > right.score;
            }
            if (left.depth != right.depth)
            {
                return left.depth < right.depth;
            }

            const MyGUI::IntCoord leftCoord = left.widget == 0 ? MyGUI::IntCoord() : left.widget->getCoord();
            const MyGUI::IntCoord rightCoord = right.widget == 0 ? MyGUI::IntCoord() : right.widget->getCoord();
            if (leftCoord.top != rightCoord.top)
            {
                return leftCoord.top < rightCoord.top;
            }
            if (leftCoord.left != rightCoord.left)
            {
                return leftCoord.left < rightCoord.left;
            }
            return left.widget < right.widget;
        }
    };
    std::sort(candidates.begin(), candidates.end(), CraftingRowScanCandidateLess());

    const std::size_t limit = candidates.size() < 18 ? candidates.size() : 18;
    for (std::size_t index = 0; index < limit; ++index)
    {
        const CraftingRowScanCandidate& candidate = candidates[index];
        const MyGUI::IntCoord candidateCoord =
            candidate.widget == 0 ? MyGUI::IntCoord() : candidate.widget->getCoord();

        std::stringstream line;
        line << "craft_search.row_scan"
             << " root_token=" << (rootToken == 0 ? "<null>" : rootToken)
             << " idx=" << index
             << " depth=" << candidate.depth
             << " score=" << candidate.score
             << " type=" << candidate.type
             << " name=" << SafeWidgetName(candidate.widget)
             << " children=" << (candidate.widget == 0 ? 0 : candidate.widget->getChildCount())
             << " coord=(" << candidateCoord.left << "," << candidateCoord.top << ","
             << candidateCoord.width << "," << candidateCoord.height << ")"
             << " quantity=" << candidate.quantity;
        if (!candidate.caption.empty())
        {
            line << " caption=\"" << TruncateForLog(candidate.caption, 72) << "\"";
        }
        if (!candidate.nameHint.empty())
        {
            line << " hint=\"" << TruncateForLog(candidate.nameHint, 72) << "\"";
        }
        if (!candidate.searchText.empty())
        {
            line << " text=\"" << TruncateForLog(candidate.searchText, 120) << "\"";
        }
        if (!candidate.rawProbe.empty())
        {
            line << " raw_probe=\"" << TruncateForLog(candidate.rawProbe, 180) << "\"";
        }
        LogWarnLine(line.str());
    }

    if (candidates.empty())
    {
        std::stringstream line;
        line << "craft_search.text_extract"
             << " root_token=" << (rootToken == 0 ? "<null>" : rootToken)
             << " candidates=0";
        LogWarnLine(line.str());
        return;
    }

    const std::size_t textLimit = candidates.size() < 8 ? candidates.size() : 8;
    for (std::size_t index = 0; index < textLimit; ++index)
    {
        const CraftingRowScanCandidate& candidate = candidates[index];
        std::stringstream line;
        line << "craft_search.text_extract"
             << " root_token=" << (rootToken == 0 ? "<null>" : rootToken)
             << " idx=" << index
             << " name=" << SafeWidgetName(candidate.widget)
             << " hint=\"" << TruncateForLog(candidate.nameHint, 72) << "\""
             << " text=\"" << TruncateForLog(candidate.searchText, 120) << "\"";
        LogWarnLine(line.str());
    }
}

MyGUI::Widget* ResolveCraftingProbeRoot(MyGUI::Widget* craftingParent, const char** outToken)
{
    if (outToken != 0)
    {
        *outToken = 0;
    }

    if (craftingParent == 0)
    {
        return 0;
    }

    struct ProbeRootCandidate
    {
        const char* token;
        int baseScore;
    };

    const ProbeRootCandidate probeTokens[] =
    {
        { "CraftTab", 5200 },
        { "CraftingStationsList", 4200 },
        { "BlueprintsAvailablePanel", 3200 },
        { "ResearchQueuePanel", 2800 },
        { "CraftingPanel", 1800 }
    };

    MyGUI::Widget* bestWidget = 0;
    const char* bestToken = 0;
    int bestScore = -1000000;
    for (std::size_t index = 0; index < sizeof(probeTokens) / sizeof(probeTokens[0]); ++index)
    {
        MyGUI::Widget* widget = FindWidgetInParentByToken(craftingParent, probeTokens[index].token);
        if (widget == 0)
        {
            continue;
        }

        int score = probeTokens[index].baseScore;
        if (widget->getInheritedVisible())
        {
            score += 2600;
        }
        else
        {
            score -= 1200;
        }

        const std::size_t childCount = widget->getChildCount();
        score += static_cast<int>(childCount > 12 ? 12 : childCount) * 120;

        const std::string token(probeTokens[index].token);
        if ((token == "BlueprintsAvailablePanel" || token == "ResearchQueuePanel")
            && !widget->getInheritedVisible())
        {
            score -= 800;
        }

        if (bestWidget == 0 || score > bestScore)
        {
            bestWidget = widget;
            bestToken = probeTokens[index].token;
            bestScore = score;
        }
    }

    if (bestWidget != 0)
    {
        if (outToken != 0)
        {
            *outToken = bestToken;
        }
        return bestWidget;
    }

    return craftingParent;
}

void DumpOnDemandCraftingDiagnosticsSnapshot(MyGUI::Widget* craftingAnchor, MyGUI::Widget* craftingParent)
{
    MyGUI::Window* owningWindow = FindOwningWindow(craftingAnchor != 0 ? craftingAnchor : craftingParent);
    std::stringstream line;
    line << "manual diagnostics crafting target"
         << " anchor=" << SafeWidgetName(craftingAnchor)
         << " parent=" << SafeWidgetName(craftingParent)
         << " caption=\"" << (owningWindow == 0 ? "" : TruncateForLog(owningWindow->getCaption().asUTF8(), 80)) << "\"";
    LogWarnLine(line.str());

    const char* probeTokens[] =
    {
        "CraftTab",
        "CraftingPanel",
        "BlueprintsAvailablePanel",
        "ResearchQueuePanel",
        "QueueItemsPanel",
        "QueuePanel",
        "CraftQueue",
        "CraftingStationsList",
        "QueueButton",
        "QueueRepeat",
        "lbCraftingItems",
        "lbCraftingQueue",
        "lbCraftingStations",
        "lbCraftingDescription"
    };

    MyGUI::Widget* craftingPanel = 0;
    MyGUI::Widget* blueprintsPanel = 0;
    MyGUI::Widget* queuePanel = 0;
    MyGUI::Widget* queueItemsPanel = 0;
    MyGUI::Widget* craftQueue = 0;
    MyGUI::Widget* stationsList = 0;
    for (std::size_t index = 0; index < sizeof(probeTokens) / sizeof(probeTokens[0]); ++index)
    {
        MyGUI::Widget* widget = FindWidgetInParentByToken(craftingParent, probeTokens[index]);
        LogCraftingProbeWidgetSummary(probeTokens[index], widget);

        if (widget == 0)
        {
            continue;
        }

        if (std::string(probeTokens[index]) == "CraftingPanel")
        {
            craftingPanel = widget;
        }
        else if (std::string(probeTokens[index]) == "BlueprintsAvailablePanel")
        {
            blueprintsPanel = widget;
        }
        else if (std::string(probeTokens[index]) == "ResearchQueuePanel")
        {
            queuePanel = widget;
        }
        else if (std::string(probeTokens[index]) == "QueueItemsPanel")
        {
            queueItemsPanel = widget;
        }
        else if (std::string(probeTokens[index]) == "CraftQueue")
        {
            craftQueue = widget;
        }
        else if (std::string(probeTokens[index]) == "CraftingStationsList")
        {
            stationsList = widget;
        }
    }

    const char* probeRootToken = 0;
    MyGUI::Widget* probeRoot = ResolveCraftingProbeRoot(craftingParent, &probeRootToken);
    std::stringstream rootLine;
    rootLine << "manual diagnostics crafting probe_root"
             << " token=" << (probeRootToken == 0 ? "<parent>" : probeRootToken)
             << " name=" << SafeWidgetName(probeRoot);
    LogWarnLine(rootLine.str());

    DumpWidgetSubtreeDiagnostics("craft_search.widget_probe_target", probeRoot);
    if (craftingPanel != 0 && craftingPanel != probeRoot)
    {
        DumpWidgetSubtreeDiagnostics("craft_search.widget_probe_crafting_panel", craftingPanel);
    }
    if (blueprintsPanel != 0 && blueprintsPanel != probeRoot && blueprintsPanel != craftingPanel)
    {
        DumpWidgetSubtreeDiagnostics("craft_search.widget_probe_blueprints", blueprintsPanel);
    }
    if (queuePanel != 0 && queuePanel != probeRoot && queuePanel != craftingPanel && queuePanel != blueprintsPanel)
    {
        DumpWidgetSubtreeDiagnostics("craft_search.widget_probe_queue", queuePanel);
    }
    if (stationsList != 0 && stationsList != probeRoot && stationsList != craftingPanel)
    {
        DumpWidgetSubtreeDiagnostics("craft_search.widget_probe_stations", stationsList);
    }
    if (queueItemsPanel != 0 && queueItemsPanel != probeRoot && queueItemsPanel != stationsList)
    {
        DumpWidgetSubtreeDiagnostics("craft_search.widget_probe_items", queueItemsPanel);
    }
    if (craftQueue != 0 && craftQueue != probeRoot && craftQueue != queueItemsPanel && craftQueue != stationsList)
    {
        DumpWidgetSubtreeDiagnostics("craft_search.widget_probe_craft_queue", craftQueue);
    }

    DumpCraftingRowScan("QueueItemsPanel", queueItemsPanel);
    DumpCraftingRowScan("CraftQueue", craftQueue);
    DumpCraftingRowScan("CraftingStationsList", stationsList);
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

    DumpVisibleCraftingWindowCandidateDiagnostics();

    MyGUI::Widget* craftingAnchor = 0;
    MyGUI::Widget* craftingParent = 0;
    if (TryResolveVisibleCraftingTarget(&craftingAnchor, &craftingParent) && craftingParent != 0)
    {
        DumpOnDemandCraftingDiagnosticsSnapshot(craftingAnchor, craftingParent);
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
