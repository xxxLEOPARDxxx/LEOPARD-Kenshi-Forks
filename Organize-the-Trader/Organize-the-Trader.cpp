#include <Debug.h>

#include <core/Functions.h>
#include <kenshi/GameData.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/InputHandler.h>
#include <kenshi/Inventory.h>
#include <kenshi/Item.h>
#include <kenshi/Kenshi.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/RootObject.h>
#include <kenshi/Character.h>
#include <kenshi/Dialogue.h>
#include <kenshi/Building.h>

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_ComboBox.h>
#include <mygui/MyGUI_EditBox.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_ImageBox.h>
#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>
#include <ois/OISKeyboard.h>

#include <Windows.h>

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// Minimal ownership interface stub to avoid including Platoon.h
// (it conflicts with Building.h enum declarations in this SDK drop).
class Ownerships
{
public:
    void getHomeFurnitureOfType(lektor<Building*>& out, BuildingFunction type);
    void getBuildingsWithFunction(lektor<Building*>& out, BuildingFunction bf);
};

class InventoryGUI;
class InventoryIcon;
class InventoryLayout;
class InventorySectionGUI
{
public:
    MyGUI::Widget* _widget;
    Ogre::vector<InventoryIcon*>::type _icons;
};

#include "TraderCore.h"
#include "TraderInventoryBinding.h"
#include "TraderSearchUi.h"
#include "TraderSearchText.h"
#include "TraderWindowDetection.h"

namespace
{
const char* kToggleHotkeyHint = "Ctrl+Shift+F8";
const char* kDiagnosticsHotkeyHint = "Ctrl+Shift+F9";

#define g_updateUTOrig (TraderState().hook.g_updateUTOrig)
#define g_inventoryRefreshGuiOrig (TraderState().hook.g_inventoryRefreshGuiOrig)
#define g_characterCreateInventoryLayoutOrig (TraderState().hook.g_characterCreateInventoryLayoutOrig)
#define g_rootObjectCreateInventoryLayoutOrig (TraderState().hook.g_rootObjectCreateInventoryLayoutOrig)
#define g_inventoryLayoutCreateGUIOrig (TraderState().hook.g_inventoryLayoutCreateGUIOrig)
#define g_inventoryLayoutCreateGUIHookInstalled (TraderState().hook.g_inventoryLayoutCreateGUIHookInstalled)
#define g_inventoryLayoutCreateGUIHookAttempted (TraderState().hook.g_inventoryLayoutCreateGUIHookAttempted)
#define g_expectedInventoryLayoutCreateGUIAddress (TraderState().hook.g_expectedInventoryLayoutCreateGUIAddress)
#define g_inventoryLayoutCreateGUIEarlyAttempted (TraderState().hook.g_inventoryLayoutCreateGUIEarlyAttempted)
#define g_inventoryLayoutCreateGUIHookCallCount (TraderState().hook.g_inventoryLayoutCreateGUIHookCallCount)
#define g_inventoryLayoutCreateInventoryLayoutLogCount (TraderState().hook.g_inventoryLayoutCreateInventoryLayoutLogCount)
#define g_lastInventoryLayoutReturnSignature (TraderState().hook.g_lastInventoryLayoutReturnSignature)

#define g_updateTickCounter (TraderState().core.g_updateTickCounter)
#define g_controlsEnabled (TraderState().core.g_controlsEnabled)
#define g_showSearchEntryCount (TraderState().core.g_showSearchEntryCount)
#define g_showSearchQuantityCount (TraderState().core.g_showSearchQuantityCount)
#define g_debugLogging (TraderState().core.g_debugLogging)
#define g_debugSearchLogging (TraderState().core.g_debugSearchLogging)
#define g_debugBindingLogging (TraderState().core.g_debugBindingLogging)
#define g_searchInputConfiguredWidth (TraderState().core.g_searchInputConfiguredWidth)
#define g_searchInputConfiguredHeight (TraderState().core.g_searchInputConfiguredHeight)

#define g_loggedNoVisibleTraderTarget (TraderState().windowDetection.g_loggedNoVisibleTraderTarget)

#define g_searchFilterDirty (TraderState().search.g_searchFilterDirty)
#define g_loggedMissingBackpackForSearch (TraderState().search.g_loggedMissingBackpackForSearch)
#define g_loggedMissingSearchableItemText (TraderState().search.g_loggedMissingSearchableItemText)
#define g_loggedNumericOnlyQueryIgnored (TraderState().search.g_loggedNumericOnlyQueryIgnored)
#define g_searchQueryRaw (TraderState().search.g_searchQueryRaw)
#define g_searchQueryNormalized (TraderState().search.g_searchQueryNormalized)
#define g_pendingSlashFocusBaseQuery (TraderState().search.g_pendingSlashFocusBaseQuery)
#define g_activeTraderTargetId (TraderState().search.g_activeTraderTargetId)
#define g_lastZeroMatchQueryLogged (TraderState().search.g_lastZeroMatchQueryLogged)
#define g_lastInventoryKeysetSelectionSignature (TraderState().search.g_lastInventoryKeysetSelectionSignature)
#define g_lastInventoryKeysetLowCoverageSignature (TraderState().search.g_lastInventoryKeysetLowCoverageSignature)
#define g_lastObservedTraderEntriesStateSignature (TraderState().search.g_lastObservedTraderEntriesStateSignature)
#define g_lastZeroMatchGuardSignature (TraderState().search.g_lastZeroMatchGuardSignature)
#define g_lastCoverageFallbackDecisionSignature (TraderState().search.g_lastCoverageFallbackDecisionSignature)
#define g_lastSearchSampleQueryLogged (TraderState().search.g_lastSearchSampleQueryLogged)
#define g_lockedKeysetTraderParent (TraderState().search.g_lockedKeysetTraderParent)
#define g_lockedKeysetStage (TraderState().search.g_lockedKeysetStage)
#define g_lockedKeysetSourceId (TraderState().search.g_lockedKeysetSourceId)
#define g_lockedKeysetSourcePreview (TraderState().search.g_lockedKeysetSourcePreview)
#define g_lockedKeysetExpectedCount (TraderState().search.g_lockedKeysetExpectedCount)
#define g_lastKeysetLockSignature (TraderState().search.g_lastKeysetLockSignature)
#define g_lastSearchVisibleEntryCount (TraderState().search.g_lastSearchVisibleEntryCount)
#define g_lastSearchTotalEntryCount (TraderState().search.g_lastSearchTotalEntryCount)

#define g_prevToggleHotkeyDown (TraderState().searchUi.g_prevToggleHotkeyDown)
#define g_prevDiagnosticsHotkeyDown (TraderState().searchUi.g_prevDiagnosticsHotkeyDown)
#define g_prevSearchSlashHotkeyDown (TraderState().searchUi.g_prevSearchSlashHotkeyDown)
#define g_prevSearchCtrlFHotkeyDown (TraderState().searchUi.g_prevSearchCtrlFHotkeyDown)
#define g_controlsWereInjected (TraderState().searchUi.g_controlsWereInjected)
#define g_suppressNextSearchEditChangeEvent (TraderState().searchUi.g_suppressNextSearchEditChangeEvent)
#define g_pendingSlashFocusTextSuppression (TraderState().searchUi.g_pendingSlashFocusTextSuppression)
#define g_focusSearchEditOnNextInjection (TraderState().searchUi.g_focusSearchEditOnNextInjection)
#define g_searchContainerDragging (TraderState().searchUi.g_searchContainerDragging)
#define g_searchContainerPositionCustomized (TraderState().searchUi.g_searchContainerPositionCustomized)
#define g_searchContainerDragLastMouseX (TraderState().searchUi.g_searchContainerDragLastMouseX)
#define g_searchContainerDragLastMouseY (TraderState().searchUi.g_searchContainerDragLastMouseY)
#define g_searchContainerStoredLeft (TraderState().searchUi.g_searchContainerStoredLeft)
#define g_searchContainerStoredTop (TraderState().searchUi.g_searchContainerStoredTop)

#define g_loggedInventoryBindingFailure (TraderState().binding.g_loggedInventoryBindingFailure)
#define g_loggedInventoryBindingDiagnostics (TraderState().binding.g_loggedInventoryBindingDiagnostics)
#define g_cachedHoveredWidgetInventory (TraderState().binding.g_cachedHoveredWidgetInventory)
#define g_cachedHoveredWidgetInventorySignature (TraderState().binding.g_cachedHoveredWidgetInventorySignature)
#define g_lastSectionWidgetBindingSignature (TraderState().binding.g_lastSectionWidgetBindingSignature)
#define g_sectionWidgetInventoryLinks (TraderState().binding.g_sectionWidgetInventoryLinks)
#define g_lastInventoryGuiBindingSignature (TraderState().binding.g_lastInventoryGuiBindingSignature)
#define g_inventoryGuiInventoryLinks (TraderState().binding.g_inventoryGuiInventoryLinks)
#define g_inventoryGuiBackPointerOffsets (TraderState().binding.g_inventoryGuiBackPointerOffsets)
#define g_lastInventoryGuiBackPointerLearningSignature (TraderState().binding.g_lastInventoryGuiBackPointerLearningSignature)
#define g_lastInventoryGuiBackPointerResolutionSignature (TraderState().binding.g_lastInventoryGuiBackPointerResolutionSignature)
#define g_lastInventoryGuiBackPointerResolutionFailureSignature (TraderState().binding.g_lastInventoryGuiBackPointerResolutionFailureSignature)
#define g_traderPanelInventoryBindings (TraderState().binding.g_traderPanelInventoryBindings)
#define g_lastPanelBindingSignature (TraderState().binding.g_lastPanelBindingSignature)
#define g_lastPanelBindingRefusedSignature (TraderState().binding.g_lastPanelBindingRefusedSignature)
#define g_lastPanelBindingProbeSignature (TraderState().binding.g_lastPanelBindingProbeSignature)
#define g_recentRefreshedInventories (TraderState().binding.g_recentRefreshedInventories)
#define g_lastRefreshInventorySummarySignature (TraderState().binding.g_lastRefreshInventorySummarySignature)

void DestroyControlsIfPresent();

bool IsSupportedVersion(KenshiLib::BinaryVersion versionInfo)
{
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    return platform != KenshiLib::BinaryVersion::UNKNOWN
        && (version == "1.0.65" || version == "1.0.68");
}


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
        if (root == 0)
        {
            continue;
        }

        if (!root->getInheritedVisible())
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
        LogWarnLine("could not resolve trader inventory-backed name keys; search is using widget-only metadata");
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

void ResetSearchQueryForTraderSwitch(const char* reason)
{
    const bool hadQuery = !g_searchQueryRaw.empty() || !g_searchQueryNormalized.empty();

    g_searchQueryRaw.clear();
    g_searchQueryNormalized.clear();
    g_pendingSlashFocusBaseQuery.clear();
    g_pendingSlashFocusTextSuppression = false;
    g_suppressNextSearchEditChangeEvent = false;
    g_loggedNumericOnlyQueryIgnored = false;
    g_lastSearchSampleQueryLogged.clear();
    g_lastZeroMatchQueryLogged.clear();
    ResetObservedTraderEntriesState();

    if (hadQuery)
    {
        std::stringstream line;
        line << "search query reset"
             << " reason=" << (reason == 0 ? "<unknown>" : reason);
        LogDebugLine(line.str());
    }
}


bool TryStripSingleSlashShortcutInsertion(
    const std::string& currentText,
    const std::string& baseText,
    std::string* outRestoredText)
{
    if (outRestoredText == 0 || currentText.size() != baseText.size() + 1)
    {
        return false;
    }

    for (std::size_t index = 0; index < currentText.size(); ++index)
    {
        if (currentText[index] != '/')
        {
            continue;
        }

        const std::string restored =
            currentText.substr(0, index) + currentText.substr(index + 1);
        if (restored == baseText)
        {
            *outRestoredText = restored;
            return true;
        }
    }

    return false;
}

void SetSearchQueryAndRefresh(
    MyGUI::EditBox* searchEdit,
    const std::string& rawText,
    const char* reason,
    bool focusAfterSet)
{
    if (searchEdit == 0)
    {
        return;
    }

    g_searchQueryRaw = rawText;
    g_searchQueryNormalized = NormalizeSearchText(g_searchQueryRaw);
    g_loggedNumericOnlyQueryIgnored = false;
    g_lastSearchSampleQueryLogged.clear();
    g_lastZeroMatchQueryLogged.clear();
    g_pendingSlashFocusBaseQuery.clear();
    g_pendingSlashFocusTextSuppression = false;

    const std::string currentOnlyText = searchEdit->getOnlyText().asUTF8();
    if (currentOnlyText != rawText)
    {
        g_suppressNextSearchEditChangeEvent = true;
        searchEdit->setOnlyText(rawText);
    }

    if (focusAfterSet)
    {
        FocusSearchEdit(searchEdit, reason);
    }

    std::stringstream line;
    line << "search ui action"
         << " reason=" << (reason == 0 ? "<unknown>" : reason)
         << " raw=\"" << TruncateForLog(g_searchQueryRaw, 64) << "\""
         << " normalized=\"" << TruncateForLog(g_searchQueryNormalized, 64) << "\"";
    LogSearchDebugLine(line.str());

    MarkSearchFilterDirty(reason);
    ApplySearchFilterFromControls(false, ShouldLogSearchDebug());
    UpdateSearchUiState();
}

void OnSearchClearButtonClicked(MyGUI::Widget*)
{
    MyGUI::EditBox* searchEdit = FindSearchEditBox();
    if (searchEdit == 0)
    {
        return;
    }

    SetSearchQueryAndRefresh(searchEdit, "", "clear_button", true);
}

void OnSearchPlaceholderClicked(MyGUI::Widget*)
{
    MyGUI::EditBox* searchEdit = FindSearchEditBox();
    if (searchEdit == 0)
    {
        return;
    }

    FocusSearchEdit(searchEdit, "placeholder_click");
}

void OnSearchEditKeyFocusChanged(MyGUI::Widget*, MyGUI::Widget*)
{
    UpdateSearchUiState();
}

void OnSearchTextChanged(MyGUI::EditBox* sender)
{
    if (sender == 0)
    {
        return;
    }

    const std::string captionText = sender->getCaption().asUTF8();
    const std::string onlyText = sender->getOnlyText().asUTF8();
    if (g_suppressNextSearchEditChangeEvent && onlyText == g_searchQueryRaw)
    {
        g_suppressNextSearchEditChangeEvent = false;
        return;
    }
    g_suppressNextSearchEditChangeEvent = false;

    if (g_pendingSlashFocusTextSuppression)
    {
        std::string restoredText;
        if (TryStripSingleSlashShortcutInsertion(
                onlyText,
                g_pendingSlashFocusBaseQuery,
                &restoredText))
        {
            g_pendingSlashFocusTextSuppression = false;
            g_suppressNextSearchEditChangeEvent = true;
            sender->setOnlyText(restoredText);
            return;
        }

        g_pendingSlashFocusTextSuppression = false;
    }

    g_searchQueryRaw = onlyText;
    g_searchQueryNormalized = NormalizeSearchText(g_searchQueryRaw);
    g_loggedNumericOnlyQueryIgnored = false;

    std::stringstream line;
    line << "search input changed"
         << " raw=\"" << TruncateForLog(g_searchQueryRaw, 64) << "\""
         << " normalized=\"" << TruncateForLog(g_searchQueryNormalized, 64) << "\""
         << " raw_len=" << g_searchQueryRaw.size()
         << " caption_len=" << captionText.size()
         << " only_len=" << onlyText.size();
    LogSearchDebugLine(line.str());

    MarkSearchFilterDirty("text_changed");
    ApplySearchFilterFromControls(false, ShouldLogSearchDebug());
    UpdateSearchUiState();
}

void DestroyControlsIfPresent()
{
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0)
    {
        g_searchContainerDragging = false;
        g_controlsWereInjected = false;
        g_searchFilterDirty = false;
        ResetObservedTraderEntriesState();
        g_pendingSlashFocusBaseQuery.clear();
        g_pendingSlashFocusTextSuppression = false;
        g_suppressNextSearchEditChangeEvent = false;
        g_cachedHoveredWidgetInventory = 0;
        g_cachedHoveredWidgetInventorySignature.clear();
        ClearLockedKeysetSource();
        return;
    }

    const MyGUI::IntCoord coord = controlsContainer->getCoord();
    g_searchContainerStoredLeft = coord.left;
    g_searchContainerStoredTop = coord.top;
    g_searchContainerPositionCustomized = true;
    g_searchContainerDragging = false;

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui != 0)
    {
        gui->destroyWidget(controlsContainer);
        LogDebugLine("controls container destroyed");
    }

    g_controlsWereInjected = false;
    g_searchFilterDirty = false;
    ResetObservedTraderEntriesState();
    g_pendingSlashFocusBaseQuery.clear();
    g_pendingSlashFocusTextSuppression = false;
    g_suppressNextSearchEditChangeEvent = false;
    g_cachedHoveredWidgetInventory = 0;
    g_cachedHoveredWidgetInventorySignature.clear();
    ClearLockedKeysetSource();
    ClearInventoryGuiInventoryLinks();
    ClearTraderPanelInventoryBindings();
}


bool TryInjectControlsToTarget(MyGUI::Widget* anchor, MyGUI::Widget* parent, const char* sourceTag)
{
    if (anchor == 0 || parent == 0)
    {
        LogErrorLine("could not resolve anchor/parent widget for controls injection");
        return false;
    }

    std::string candidateReason;
    const int candidateScore = ComputeTraderWindowCandidateScore(parent, &candidateReason);
    const bool acceptedTarget = (sourceTag != 0 && std::string(sourceTag) == "hover-direct")
        || candidateScore > 0;

    if (!acceptedTarget)
    {
        std::stringstream line;
        line << "rejecting injection target reason=not_likely_trader_window"
             << " source=" << (sourceTag == 0 ? "<unknown>" : sourceTag)
             << " anchor=" << SafeWidgetName(anchor)
             << " parent=" << SafeWidgetName(parent)
             << " candidate_score=" << candidateScore
             << " candidate_reason=\"" << TruncateForLog(candidateReason, 120) << "\""
             << " has_trader_structure=" << (HasTraderStructure(parent) ? "true" : "false");
        if (ShouldLogDebug())
        {
            LogWarnLine(line.str());
        }
        return false;
    }

    const std::string nextTraderTargetId = BuildTraderTargetIdentity(anchor, parent);
    if (!nextTraderTargetId.empty() && !g_activeTraderTargetId.empty() && nextTraderTargetId != g_activeTraderTargetId)
    {
        ResetSearchQueryForTraderSwitch("target_changed");
    }
    g_activeTraderTargetId = nextTraderTargetId;
    g_focusSearchEditOnNextInjection = true;

    MyGUI::Widget* controlsParent = parent;
    int topOverride = -1;

    MyGUI::Window* owningWindow = FindOwningWindow(parent);
    if (owningWindow != 0 && parent != owningWindow)
    {
        controlsParent = owningWindow;
        topOverride = 12;
    }

    DestroyControlsIfPresent();
    SearchUiCallbacks callbacks;
    callbacks.onSearchTextChanged = &OnSearchTextChanged;
    callbacks.onSearchEditFocusChanged = &OnSearchEditKeyFocusChanged;
    callbacks.onSearchPlaceholderClicked = &OnSearchPlaceholderClicked;
    callbacks.onSearchClearButtonClicked = &OnSearchClearButtonClicked;
    if (!BuildControlsScaffold(controlsParent, topOverride, callbacks))
    {
        LogErrorLine("failed to build phase 2 controls scaffold");
        return false;
    }

    g_controlsWereInjected = true;
    MarkSearchFilterDirty("controls_injected");
    if (ApplySearchFilterToTraderParent(parent, false, ShouldLogSearchDebug()))
    {
        g_searchFilterDirty = false;
    }

    std::stringstream line;
    line << "controls scaffold injected"
         << " source=" << (sourceTag == 0 ? "<unknown>" : sourceTag)
         << " anchor=" << SafeWidgetName(anchor)
         << " parent=" << SafeWidgetName(parent);
    LogDebugLine(line.str());
    return true;
}

bool TryInjectControlsToHoveredWindowDirect()
{
    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        LogWarnLine("manual attach failed: MyGUI InputManager unavailable");
        return false;
    }

    MyGUI::Widget* hovered = inputManager->getMouseFocusWidget();
    if (hovered == 0)
    {
        LogWarnLine("manual attach failed: no mouse-focused widget; hover target window and press hotkey again");
        return false;
    }

    MyGUI::Widget* anchor = FindBestWindowAnchor(hovered);
    MyGUI::Widget* parent = ResolveInjectionParent(anchor);
    if (anchor == 0 || parent == 0)
    {
        std::stringstream line;
        line << "manual attach failed: anchor/parent unresolved hovered_chain=" << BuildParentChainForLog(hovered);
        LogWarnLine(line.str());
        return false;
    }

    std::stringstream line;
    line << "manual attach using hovered window"
         << " anchor=" << SafeWidgetName(anchor)
         << " parent=" << SafeWidgetName(parent)
         << " parent_coord=(" << parent->getCoord().left << "," << parent->getCoord().top << ","
         << parent->getCoord().width << "," << parent->getCoord().height << ")"
         << " hovered_chain=" << BuildParentChainForLog(hovered);
    LogDebugLine(line.str());

    if (ShouldLogDebug())
    {
        DumpHoveredAttachDiagnostics(hovered, anchor, parent);
    }

    return TryInjectControlsToTarget(anchor, parent, "hover-direct");
}

void EnsureControlsInjectedIfEnabled()
{
    if (!g_controlsEnabled)
    {
        return;
    }

    if (FindControlsContainer() != 0)
    {
        return;
    }

    MyGUI::Widget* anchor = 0;
    MyGUI::Widget* parent = 0;
    if (!TryResolveVisibleTraderTarget(&anchor, &parent))
    {
        if (TryResolveHoveredTarget(&anchor, &parent, false))
        {
            g_loggedNoVisibleTraderTarget = false;
            if (!TryInjectControlsToTarget(anchor, parent, "hover-auto"))
            {
                LogWarnLine("hover auto controls scaffold injection failed");
            }
            return;
        }

        if (!g_loggedNoVisibleTraderTarget)
        {
            if (ShouldLogDebug())
            {
                LogDebugLine("controls enabled but no visible trader target found yet");
                DumpTraderTargetProbe();
                DumpVisibleWindowCandidateDiagnostics();
            }
            g_loggedNoVisibleTraderTarget = true;
        }
        return;
    }
    g_loggedNoVisibleTraderTarget = false;
    if (!TryInjectControlsToTarget(anchor, parent, "auto"))
    {
        LogWarnLine("auto controls scaffold injection failed");
    }
}

bool IsToggleHotkeyPressedEdge()
{
    if (key == 0 || key->keyboard == 0)
    {
        g_prevToggleHotkeyDown = false;
        return false;
    }

    const bool ctrlDown = key->keyboard->isKeyDown(OIS::KC_LCONTROL)
        || key->keyboard->isKeyDown(OIS::KC_RCONTROL);
    const bool shiftDown = key->keyboard->isKeyDown(OIS::KC_LSHIFT)
        || key->keyboard->isKeyDown(OIS::KC_RSHIFT);
    const bool f8Down = key->keyboard->isKeyDown(OIS::KC_F8);

    const bool chordDown = ctrlDown && shiftDown && f8Down;
    const bool pressedEdge = chordDown && !g_prevToggleHotkeyDown;
    g_prevToggleHotkeyDown = chordDown;
    return pressedEdge;
}

bool IsDiagnosticsHotkeyPressedEdge()
{
    if (key == 0 || key->keyboard == 0)
    {
        g_prevDiagnosticsHotkeyDown = false;
        return false;
    }

    const bool ctrlDown = key->keyboard->isKeyDown(OIS::KC_LCONTROL)
        || key->keyboard->isKeyDown(OIS::KC_RCONTROL);
    const bool shiftDown = key->keyboard->isKeyDown(OIS::KC_LSHIFT)
        || key->keyboard->isKeyDown(OIS::KC_RSHIFT);
    const bool f9Down = key->keyboard->isKeyDown(OIS::KC_F9);

    const bool chordDown = ctrlDown && shiftDown && f9Down;
    const bool pressedEdge = chordDown && !g_prevDiagnosticsHotkeyDown;
    g_prevDiagnosticsHotkeyDown = chordDown;
    return pressedEdge;
}


void LogRecentRefreshedInventorySummary(std::size_t expectedEntryCount)
{
    PruneRecentlyRefreshedInventories();
    if (g_recentRefreshedInventories.empty())
    {
        LogWarnLine("recent refresh inventory summary empty");
        return;
    }

    std::vector<RefreshedInventoryLink> sorted = g_recentRefreshedInventories;
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

    std::size_t limit = sorted.size() < 14 ? sorted.size() : 14;
    for (std::size_t index = 0; index < limit; ++index)
    {
        const RefreshedInventoryLink& link = sorted[index];
        const unsigned long long ageTicks =
            g_updateTickCounter >= link.lastSeenTick
                ? g_updateTickCounter - link.lastSeenTick
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

    g_lastInventoryKeysetSelectionSignature.clear();
    g_lastInventoryKeysetLowCoverageSignature.clear();
    g_lastCoverageFallbackDecisionSignature.clear();

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

    LogSearchSampleForQuery(entriesRoot, g_searchQueryNormalized, 12);

    LogInventoryBindingDiagnostics(orderedEntries.size());
}

void TickPhase2ControlsScaffold()
{
    TickSearchContainerDrag();

    if (IsDiagnosticsHotkeyPressedEdge())
    {
        DumpOnDemandTraderDiagnosticsSnapshot();
    }

    if (IsToggleHotkeyPressedEdge())
    {
        if (g_controlsEnabled)
        {
            ApplySearchFilterFromControls(true, false);
            g_controlsEnabled = false;
            DestroyControlsIfPresent();
            g_loggedNoVisibleTraderTarget = false;
            LogDebugLine("controls toggled OFF");
            return;
        }

        g_controlsEnabled = true;
        LogDebugLine("controls toggled ON");

        g_loggedNoVisibleTraderTarget = false;
        EnsureControlsInjectedIfEnabled();
        if (FindControlsContainer() == 0)
        {
            if (!TryInjectControlsToHoveredWindowDirect())
            {
                LogWarnLine("manual attach did not inject controls");
            }
        }
    }

    if (!g_controlsEnabled)
    {
        return;
    }

    EnsureControlsInjectedIfEnabled();
    bool handledSearchShortcut = false;
    MyGUI::EditBox* searchEdit = FindSearchEditBox();
    if (searchEdit != 0)
    {
        const SearchFocusHotkeyKind hotkeyKind =
            DetectSearchFocusHotkeyPressedEdge(searchEdit);
        if (hotkeyKind != SearchFocusHotkeyKind_None)
        {
            if (hotkeyKind == SearchFocusHotkeyKind_Slash)
            {
                g_pendingSlashFocusBaseQuery = g_searchQueryRaw;
                g_pendingSlashFocusTextSuppression = true;
            }
            else
            {
                g_pendingSlashFocusBaseQuery.clear();
                g_pendingSlashFocusTextSuppression = false;
            }
            FocusSearchEdit(searchEdit, "focus_hotkey");
            handledSearchShortcut = true;
        }
    }
    else
    {
        g_prevSearchSlashHotkeyDown = false;
        g_prevSearchCtrlFHotkeyDown = false;
        g_pendingSlashFocusBaseQuery.clear();
        g_pendingSlashFocusTextSuppression = false;
    }

    if (FindControlsContainer() != 0)
    {
        ObserveTraderEntriesStateForRefresh();
    }

    if (FindControlsContainer() != 0 && !handledSearchShortcut && g_searchFilterDirty)
    {
        ApplySearchFilterFromControls(false, ShouldLogSearchDebug());
    }

    if (g_controlsWereInjected && FindControlsContainer() == 0)
    {
        g_controlsWereInjected = false;
        g_cachedHoveredWidgetInventory = 0;
        g_cachedHoveredWidgetInventorySignature.clear();
        ClearLockedKeysetSource();
        ClearInventoryGuiInventoryLinks();
        ClearTraderPanelInventoryBindings();
        LogDebugLine("controls container no longer present (window likely closed/destroyed); hover target window and press Ctrl+Shift+F8 to attach again");
    }
}

void InventoryLayoutCreateGUI_hook(
    void* self,
    InventoryGUI* invGui,
    Ogre::map<std::string, InventorySectionGUI*>::type& sectionGuis,
    Inventory* inv)
{
    if (g_inventoryLayoutCreateGUIOrig != 0)
    {
        g_inventoryLayoutCreateGUIOrig(self, invGui, sectionGuis, inv);
    }

    if (inv == 0)
    {
        return;
    }

    if (FindControlsContainer() != 0)
    {
        MarkSearchFilterDirty("inventory_layout_create_gui");
    }

    ++g_inventoryLayoutCreateGUIHookCallCount;
    if (ShouldLogBindingDebug() && g_inventoryLayoutCreateGUIHookCallCount <= 8)
    {
        RootObject* owner = inv->getOwner();
        if (owner == 0)
        {
            owner = inv->getCallbackObject();
        }

        std::vector<std::string> keys;
        std::string keyPreview;
        if (TryExtractSearchKeysFromInventory(inv, &keys) && !keys.empty())
        {
            keyPreview = BuildKeyPreviewForLog(keys, 8);
        }

        MyGUI::Widget* firstSectionWidget = 0;
        std::string firstSectionName;
        for (Ogre::map<std::string, InventorySectionGUI*>::type::const_iterator it = sectionGuis.begin();
             it != sectionGuis.end();
             ++it)
        {
            InventorySectionGUI* sectionGui = it->second;
            if (sectionGui == 0 || sectionGui->_widget == 0)
            {
                continue;
            }

            firstSectionWidget = sectionGui->_widget;
            firstSectionName = it->first;
            break;
        }

        std::stringstream line;
        line << "createGUI hook call"
             << " index=" << g_inventoryLayoutCreateGUIHookCallCount
             << " self=" << self
             << " inv_gui=" << invGui
             << " owner=" << RootObjectDisplayNameForLog(owner)
             << " inv_items=" << InventoryItemCountForLog(inv)
             << " sections_total=" << sectionGuis.size()
             << " first_section=" << firstSectionName
             << " first_section_widget=" << firstSectionWidget;
        if (!keyPreview.empty())
        {
            line << " key_preview=\"" << TruncateForLog(keyPreview, 180) << "\"";
        }
        LogInfoLine(line.str());
    }

    RegisterInventoryGuiInventoryLink(invGui, inv);

    std::size_t boundSections = 0;
    std::stringstream sectionPreview;
    for (Ogre::map<std::string, InventorySectionGUI*>::type::const_iterator it = sectionGuis.begin();
         it != sectionGuis.end();
         ++it)
    {
        InventorySectionGUI* sectionGui = it->second;
        MyGUI::Widget* sectionWidget = sectionGui == 0 ? 0 : sectionGui->_widget;
        if (sectionWidget == 0)
        {
            continue;
        }

        RegisterSectionWidgetInventoryLink(sectionWidget, inv, it->first);
        if (boundSections < 6)
        {
            if (boundSections > 0)
            {
                sectionPreview << " | ";
            }
            sectionPreview << it->first << ":" << SafeWidgetName(sectionWidget);
        }

        ++boundSections;
    }

    if (boundSections == 0)
    {
        return;
    }

    std::stringstream signature;
    signature << inv
              << "|sections=" << boundSections
              << "|" << sectionPreview.str();
    if (signature.str() != g_lastSectionWidgetBindingSignature)
    {
        RootObject* owner = inv->getOwner();
        if (owner == 0)
        {
            owner = inv->getCallbackObject();
        }

        std::stringstream line;
        line << "inventory layout section binding"
             << " owner=" << RootObjectDisplayNameForLog(owner)
             << " inv_items=" << InventoryItemCountForLog(inv)
             << " sections=" << boundSections
             << " preview=\"" << TruncateForLog(sectionPreview.str(), 220) << "\"";
        LogBindingDebugLine(line.str());
        g_lastSectionWidgetBindingSignature = signature.str();
    }
}

void Inventory_refreshGui_hook(Inventory* self)
{
    if (g_inventoryRefreshGuiOrig != 0)
    {
        g_inventoryRefreshGuiOrig(self);
    }

    RegisterRecentlyRefreshedInventory(self);
}

std::uintptr_t ResolveInventoryLayoutCreateGUIHookAddress(KenshiLib::BinaryVersion versionInfo)
{
    const std::string version = versionInfo.GetVersion();
    if (version != "1.0.65" && version != "1.0.68")
    {
        return 0;
    }

    const std::uintptr_t baseAddress = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(0));
    if (baseAddress == 0)
    {
        return 0;
    }

    const bool hasNonZeroPlatform = versionInfo.GetPlatform() != 0;
    if (hasNonZeroPlatform)
    {
        if (version == "1.0.65")
        {
            // 1.0.65 Steam: slot 37 (0x14F530) resolves into the backpack attach branch,
            // but the inventory-item population path lives at slot 1 / RVA 0x14EEA0.
            return baseAddress + 0x0014EEA0;
        }
        return baseAddress + 0x0014F570;
    }

    if (version == "1.0.65")
    {
        return baseAddress + 0x0014F450;
    }
    return baseAddress + 0x0014F470;
}

bool IsAddressInMainModuleTextSection(std::uintptr_t address)
{
    const std::uintptr_t baseAddress = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(0));
    if (baseAddress == 0 || address == 0)
    {
        return false;
    }

    const IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(baseAddress);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return false;
    }

    const IMAGE_NT_HEADERS64* ntHeaders =
        reinterpret_cast<const IMAGE_NT_HEADERS64*>(baseAddress + static_cast<std::uintptr_t>(dosHeader->e_lfanew));
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
    {
        return false;
    }

    const IMAGE_SECTION_HEADER* sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
    for (unsigned int index = 0; index < ntHeaders->FileHeader.NumberOfSections; ++index)
    {
        const IMAGE_SECTION_HEADER& section = sectionHeader[index];
        if (std::memcmp(section.Name, ".text", 5) != 0)
        {
            continue;
        }

        const std::uintptr_t sectionBegin = baseAddress + static_cast<std::uintptr_t>(section.VirtualAddress);
        const std::uintptr_t sectionSpan =
            section.Misc.VirtualSize > 0
                ? static_cast<std::uintptr_t>(section.Misc.VirtualSize)
                : static_cast<std::uintptr_t>(section.SizeOfRawData);
        const std::uintptr_t sectionEnd = sectionBegin + sectionSpan;
        return address >= sectionBegin && address < sectionEnd;
    }

    return false;
}

std::string FormatAbsoluteAddressForLog(std::uintptr_t address)
{
    if (address == 0)
    {
        return "0x0";
    }

    std::stringstream out;
    out << "0x" << std::hex << std::uppercase << address;

    const std::uintptr_t baseAddress = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(0));
    if (baseAddress != 0 && address >= baseAddress)
    {
        out << " (rva=0x" << std::hex << std::uppercase << (address - baseAddress) << ")";
    }
    return out.str();
}

void TryInstallInventoryLayoutCreateGUIHookEarly()
{
    if (g_inventoryLayoutCreateGUIHookInstalled || g_inventoryLayoutCreateGUIOrig != 0)
    {
        return;
    }

    if (g_inventoryLayoutCreateGUIEarlyAttempted)
    {
        return;
    }
    g_inventoryLayoutCreateGUIEarlyAttempted = true;

    if (g_expectedInventoryLayoutCreateGUIAddress == 0
        || !IsAddressInMainModuleTextSection(g_expectedInventoryLayoutCreateGUIAddress))
    {
        std::stringstream line;
        line << "early createGUI hook skipped: expected target invalid "
             << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress);
        LogWarnLine(line.str());
        return;
    }

    g_inventoryLayoutCreateGUIHookAttempted = true;
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            g_expectedInventoryLayoutCreateGUIAddress,
            InventoryLayoutCreateGUI_hook,
            &g_inventoryLayoutCreateGUIOrig))
    {
        std::stringstream line;
        line << "early createGUI hook install failed at "
             << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress)
             << " (will rely on deferred install)";
        LogWarnLine(line.str());
        return;
    }

    g_inventoryLayoutCreateGUIHookInstalled = true;
    std::stringstream line;
    line << "early createGUI hook installed at "
         << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress);
    LogInfoLine(line.str());
}

std::uintptr_t PointerDistance(std::uintptr_t left, std::uintptr_t right)
{
    return left >= right ? (left - right) : (right - left);
}

bool TryResolveRelativeJumpTarget(std::uintptr_t address, std::uintptr_t* outTarget)
{
    if (outTarget == 0 || address == 0 || !IsAddressInMainModuleTextSection(address))
    {
        return false;
    }

    const unsigned char* code = reinterpret_cast<const unsigned char*>(address);
    if (code == 0 || code[0] != 0xE9)
    {
        return false;
    }

    const std::int32_t rel = *reinterpret_cast<const std::int32_t*>(code + 1);
    const std::uintptr_t target =
        address + static_cast<std::uintptr_t>(5)
        + static_cast<std::uintptr_t>(static_cast<std::intptr_t>(rel));
    if (!IsAddressInMainModuleTextSection(target))
    {
        return false;
    }

    *outTarget = target;
    return true;
}

std::uintptr_t ResolveExecutableThunkTarget(std::uintptr_t address, int maxHops)
{
    std::uintptr_t resolved = address;
    for (int hop = 0; hop < maxHops; ++hop)
    {
        std::uintptr_t nextTarget = 0;
        if (!TryResolveRelativeJumpTarget(resolved, &nextTarget) || nextTarget == resolved)
        {
            break;
        }
        resolved = nextTarget;
    }
    return resolved;
}

void LogInventoryLayoutReturnDiagnostics(
    const char* sourceTag,
    RootObject* owner,
    InventoryLayout* layout)
{
    if (!ShouldLogVerboseBindingDiagnostics())
    {
        return;
    }

    if (layout == 0 || sourceTag == 0)
    {
        return;
    }

    void*** vtableHolder = reinterpret_cast<void***>(layout);
    if (vtableHolder == 0 || *vtableHolder == 0)
    {
        return;
    }

    struct LayoutSlotCandidate
    {
        int slot;
        std::uintptr_t entryAddress;
        std::uintptr_t resolvedAddress;
        std::uintptr_t deltaToExpected;
        bool exactExpectedMatch;
    };

    std::vector<LayoutSlotCandidate> candidates;
    candidates.reserve(48);

    for (int slot = 0; slot < 48; ++slot)
    {
        const std::uintptr_t entryAddress = reinterpret_cast<std::uintptr_t>((*vtableHolder)[slot]);
        if (entryAddress == 0 || !IsAddressInMainModuleTextSection(entryAddress))
        {
            continue;
        }

        LayoutSlotCandidate candidate;
        candidate.slot = slot;
        candidate.entryAddress = entryAddress;
        candidate.resolvedAddress = ResolveExecutableThunkTarget(entryAddress, 2);
        candidate.deltaToExpected = 0;
        candidate.exactExpectedMatch = false;

        if (g_expectedInventoryLayoutCreateGUIAddress != 0)
        {
            const std::uintptr_t entryDelta =
                PointerDistance(candidate.entryAddress, g_expectedInventoryLayoutCreateGUIAddress);
            const std::uintptr_t resolvedDelta =
                PointerDistance(candidate.resolvedAddress, g_expectedInventoryLayoutCreateGUIAddress);
            candidate.deltaToExpected = entryDelta < resolvedDelta ? entryDelta : resolvedDelta;
            candidate.exactExpectedMatch =
                candidate.entryAddress == g_expectedInventoryLayoutCreateGUIAddress
                || candidate.resolvedAddress == g_expectedInventoryLayoutCreateGUIAddress;
        }

        candidates.push_back(candidate);
    }

    if (candidates.empty())
    {
        return;
    }

    std::size_t nearestIndex = 0;
    bool exactMatchFound = false;
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        if (candidates[index].exactExpectedMatch)
        {
            nearestIndex = index;
            exactMatchFound = true;
            break;
        }

        if (index == 0 || candidates[index].deltaToExpected < candidates[nearestIndex].deltaToExpected)
        {
            nearestIndex = index;
        }
    }

    std::stringstream preview;
    const std::size_t maxPreview = candidates.size() < 8 ? candidates.size() : 8;
    for (std::size_t index = 0; index < maxPreview; ++index)
    {
        if (index > 0)
        {
            preview << " | ";
        }
        preview << "slot" << candidates[index].slot
                << "=" << FormatAbsoluteAddressForLog(candidates[index].entryAddress)
                << "->" << FormatAbsoluteAddressForLog(candidates[index].resolvedAddress);
    }

    const LayoutSlotCandidate& nearest = candidates[nearestIndex];
    std::stringstream signature;
    signature << sourceTag
              << "|" << owner
              << "|" << layout
              << "|" << reinterpret_cast<void*>(*vtableHolder)
              << "|" << exactMatchFound
              << "|" << nearest.slot
              << "|" << nearest.entryAddress
              << "|" << g_inventoryLayoutCreateGUIHookInstalled
              << "|" << g_inventoryLayoutCreateGUIHookCallCount;
    if (signature.str() == g_lastInventoryLayoutReturnSignature
        && g_inventoryLayoutCreateInventoryLayoutLogCount >= 16)
    {
        return;
    }

    ++g_inventoryLayoutCreateInventoryLayoutLogCount;
    g_lastInventoryLayoutReturnSignature = signature.str();

    std::stringstream line;
    line << "createInventoryLayout result"
         << " source=" << sourceTag
         << " owner=" << RootObjectDisplayNameForLog(owner)
         << " layout=" << layout
         << " vtable=" << reinterpret_cast<void*>(*vtableHolder)
         << " expected=" << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress)
         << " exact_expected_slot=" << (exactMatchFound ? "true" : "false")
         << " nearest_slot=" << nearest.slot
         << " nearest_entry=" << FormatAbsoluteAddressForLog(nearest.entryAddress)
         << " nearest_resolved=" << FormatAbsoluteAddressForLog(nearest.resolvedAddress);
    if (!exactMatchFound && g_expectedInventoryLayoutCreateGUIAddress != 0)
    {
        line << " nearest_delta=0x" << std::hex << std::uppercase << nearest.deltaToExpected << std::dec;
    }
    line << " hook_installed=" << (g_inventoryLayoutCreateGUIHookInstalled ? "true" : "false")
         << " hook_calls=" << g_inventoryLayoutCreateGUIHookCallCount
         << " preview=\"" << TruncateForLog(preview.str(), 320) << "\"";

    if (!exactMatchFound && g_expectedInventoryLayoutCreateGUIAddress != 0)
    {
        LogWarnLine(line.str());
        return;
    }

    LogInfoLine(line.str());
}

void TryInstallInventoryLayoutCreateGUIHookFromLayout(InventoryLayout* layout)
{
    if (layout == 0 || g_inventoryLayoutCreateGUIHookInstalled)
    {
        return;
    }

    void*** vtableHolder = reinterpret_cast<void***>(layout);
    if (vtableHolder == 0 || *vtableHolder == 0)
    {
        return;
    }

    struct CreateGUIHookCandidate
    {
        int slot;
        std::uintptr_t entryAddress;
        std::uintptr_t resolvedAddress;
        std::uintptr_t deltaToExpected;
        bool exactExpectedMatch;
    };

    const std::uintptr_t kExpectedAddressTolerance = 0x3000;
    std::vector<CreateGUIHookCandidate> candidates;
    candidates.reserve(48);

    for (int slot = 0; slot < 48; ++slot)
    {
        const std::uintptr_t candidateAddress = reinterpret_cast<std::uintptr_t>((*vtableHolder)[slot]);
        if (candidateAddress == 0 || !IsAddressInMainModuleTextSection(candidateAddress))
        {
            continue;
        }

        CreateGUIHookCandidate candidate;
        candidate.slot = slot;
        candidate.entryAddress = candidateAddress;
        candidate.resolvedAddress = ResolveExecutableThunkTarget(candidateAddress, 2);
        candidate.deltaToExpected = 0;
        candidate.exactExpectedMatch = false;

        if (g_expectedInventoryLayoutCreateGUIAddress != 0)
        {
            const std::uintptr_t entryDelta =
                PointerDistance(candidate.entryAddress, g_expectedInventoryLayoutCreateGUIAddress);
            const std::uintptr_t resolvedDelta =
                PointerDistance(candidate.resolvedAddress, g_expectedInventoryLayoutCreateGUIAddress);
            candidate.deltaToExpected = entryDelta < resolvedDelta ? entryDelta : resolvedDelta;
            candidate.exactExpectedMatch =
                candidate.entryAddress == g_expectedInventoryLayoutCreateGUIAddress
                || candidate.resolvedAddress == g_expectedInventoryLayoutCreateGUIAddress;
        }

        candidates.push_back(candidate);
    }

    if (candidates.empty())
    {
        if (!g_inventoryLayoutCreateGUIHookAttempted)
        {
            LogWarnLine("deferred inventory layout createGUI hook skipped: no valid vtable slot candidate");
            g_inventoryLayoutCreateGUIHookAttempted = true;
        }
        return;
    }

    std::size_t selectedIndex = 0;
    bool selected = false;
    bool selectedNearExpected = false;
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        if (!candidates[index].exactExpectedMatch)
        {
            continue;
        }
        selectedIndex = index;
        selected = true;
        break;
    }

    if (!selected && g_expectedInventoryLayoutCreateGUIAddress != 0)
    {
        std::size_t bestIndex = 0;
        std::uintptr_t bestDelta = static_cast<std::uintptr_t>(-1);
        for (std::size_t index = 0; index < candidates.size(); ++index)
        {
            if (candidates[index].deltaToExpected < bestDelta)
            {
                bestIndex = index;
                bestDelta = candidates[index].deltaToExpected;
            }
        }

        if (bestDelta <= kExpectedAddressTolerance)
        {
            selectedIndex = bestIndex;
            selected = true;
            selectedNearExpected = true;
        }
    }

    if (!selected && g_expectedInventoryLayoutCreateGUIAddress == 0)
    {
        selectedIndex = 0;
        selected = true;
    }

    if (!selected)
    {
        std::stringstream candidatesLine;
        candidatesLine << "deferred inventory layout createGUI hook skipped: no expected/nearby candidate"
                       << " expected=" << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress);
        const std::size_t maxPreview = candidates.size() < 8 ? candidates.size() : 8;
        for (std::size_t index = 0; index < maxPreview; ++index)
        {
            candidatesLine << " slot" << candidates[index].slot
                           << "=" << FormatAbsoluteAddressForLog(candidates[index].entryAddress)
                           << "->" << FormatAbsoluteAddressForLog(candidates[index].resolvedAddress);
        }
        LogWarnLine(candidatesLine.str());
        g_inventoryLayoutCreateGUIHookAttempted = true;
        return;
    }

    const CreateGUIHookCandidate& selectedCandidate = candidates[selectedIndex];
    g_inventoryLayoutCreateGUIHookAttempted = true;
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            selectedCandidate.entryAddress,
            InventoryLayoutCreateGUI_hook,
            &g_inventoryLayoutCreateGUIOrig))
    {
        std::stringstream line;
        line << "could not install deferred inventory layout createGUI hook"
             << " target=" << FormatAbsoluteAddressForLog(selectedCandidate.entryAddress)
             << " resolved=" << FormatAbsoluteAddressForLog(selectedCandidate.resolvedAddress)
             << " expected=" << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress)
             << " slot=" << selectedCandidate.slot;
        LogWarnLine(line.str());
        return;
    }

    g_inventoryLayoutCreateGUIHookInstalled = true;
    if (selectedNearExpected)
    {
        std::stringstream line;
        line << "deferred inventory layout createGUI hook installed from nearby candidate"
             << " target=" << FormatAbsoluteAddressForLog(selectedCandidate.entryAddress)
             << " resolved=" << FormatAbsoluteAddressForLog(selectedCandidate.resolvedAddress)
             << " expected=" << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress)
             << " delta=0x" << std::hex << std::uppercase << selectedCandidate.deltaToExpected
             << std::dec
             << " slot=" << selectedCandidate.slot;
        LogWarnLine(line.str());
    }
    else
    {
        std::stringstream line;
        line << "deferred inventory layout createGUI hook installed"
             << " target=" << FormatAbsoluteAddressForLog(selectedCandidate.entryAddress)
             << " resolved=" << FormatAbsoluteAddressForLog(selectedCandidate.resolvedAddress)
             << " expected=" << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress)
             << " slot=" << selectedCandidate.slot;
        LogInfoLine(line.str());
    }
}

InventoryLayout* Character_createInventoryLayout_hook(Character* self)
{
    if (g_characterCreateInventoryLayoutOrig == 0)
    {
        return 0;
    }

    TryInstallInventoryLayoutCreateGUIHookEarly();
    InventoryLayout* layout = g_characterCreateInventoryLayoutOrig(self);
    LogInventoryLayoutReturnDiagnostics("character", self, layout);
    TryInstallInventoryLayoutCreateGUIHookFromLayout(layout);
    return layout;
}

InventoryLayout* RootObject_createInventoryLayout_hook(RootObject* self)
{
    if (g_rootObjectCreateInventoryLayoutOrig == 0)
    {
        return 0;
    }

    TryInstallInventoryLayoutCreateGUIHookEarly();
    InventoryLayout* layout = g_rootObjectCreateInventoryLayoutOrig(self);
    LogInventoryLayoutReturnDiagnostics("root_object", self, layout);
    TryInstallInventoryLayoutCreateGUIHookFromLayout(layout);
    return layout;
}

void PlayerInterface_updateUT_hook(PlayerInterface* self)
{
    ++g_updateTickCounter;
    if ((g_updateTickCounter % 240ULL) == 0ULL)
    {
        PruneRecentlyRefreshedInventories();
        PruneSectionWidgetInventoryLinks();
        PruneInventoryGuiInventoryLinks();
        PruneTraderPanelInventoryBindings();
    }
    if (g_updateUTOrig != 0)
    {
        g_updateUTOrig(self);
    }

    TickPhase2ControlsScaffold();
}
}

__declspec(dllexport) void startPlugin()
{
    LogInfoLine("startPlugin()");

    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    if (!IsSupportedVersion(versionInfo))
    {
        LogErrorLine("unsupported Kenshi version/platform");
        return;
    }

    {
        std::stringstream line;
        line << "binary version detected"
             << " platform=" << versionInfo.GetPlatform()
             << " version=" << versionInfo.GetVersion();
        LogInfoLine(line.str());
    }

    LoadModConfig();

    g_expectedInventoryLayoutCreateGUIAddress =
        ResolveInventoryLayoutCreateGUIHookAddress(versionInfo);
    if (g_expectedInventoryLayoutCreateGUIAddress != 0)
    {
        std::stringstream line;
        line << "inventory layout createGUI expected target="
             << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress)
             << " (deferred install)";
        LogInfoLine(line.str());
    }
    else
    {
        LogWarnLine("inventory layout createGUI expected target unresolved; deferred hook disabled");
    }

    LogWarnLine("Inventory::refreshGui hook disabled (unsafe in current runtime)");

    const std::uintptr_t characterCreateLayoutAddress =
        KenshiLib::GetRealAddress(&Character::_NV_createInventoryLayout);
    if (characterCreateLayoutAddress == 0
        || KenshiLib::SUCCESS != KenshiLib::AddHook(
            characterCreateLayoutAddress,
            Character_createInventoryLayout_hook,
            &g_characterCreateInventoryLayoutOrig))
    {
        LogWarnLine("could not hook Character::_NV_createInventoryLayout; deferred createGUI binding may be unavailable");
    }
    else
    {
        std::stringstream line;
        line << "hooked Character::_NV_createInventoryLayout at "
             << FormatAbsoluteAddressForLog(characterCreateLayoutAddress);
        LogInfoLine(line.str());
    }

    const std::uintptr_t rootObjectCreateLayoutAddress =
        KenshiLib::GetRealAddress(&RootObject::_NV_createInventoryLayout);
    if (rootObjectCreateLayoutAddress == 0
        || rootObjectCreateLayoutAddress == characterCreateLayoutAddress
        || KenshiLib::SUCCESS != KenshiLib::AddHook(
            rootObjectCreateLayoutAddress,
            RootObject_createInventoryLayout_hook,
            &g_rootObjectCreateInventoryLayoutOrig))
    {
        LogWarnLine("could not hook RootObject::_NV_createInventoryLayout; deferred createGUI binding for building/shop layouts may be unavailable");
    }
    else
    {
        std::stringstream line;
        line << "hooked RootObject::_NV_createInventoryLayout at "
             << FormatAbsoluteAddressForLog(rootObjectCreateLayoutAddress);
        LogInfoLine(line.str());
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        &g_updateUTOrig))
    {
        LogErrorLine("could not hook PlayerInterface::updateUT");
        return;
    }

    std::stringstream info;
    info << "phase 2 controls scaffold active."
         << " Auto-attach is enabled for detected trader windows."
         << " Press " << kToggleHotkeyHint << " to hide/show."
         << " Press " << kDiagnosticsHotkeyHint << " for a diagnostics snapshot.";
    LogInfoLine(info.str());
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
