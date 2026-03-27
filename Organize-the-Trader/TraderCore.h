#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace MyGUI
{
class Widget;
}

class PlayerInterface;
class Inventory;
class InventorySection;
class Character;
class RootObject;
class InventoryItemBase;
class Item;
class InventoryLayout;
class InventoryGUI;
class InventorySectionGUI;

typedef void (*PlayerInterfaceUpdateUTFn)(PlayerInterface*);
typedef InventoryLayout* (*CharacterCreateInventoryLayoutFn)(Character*);
typedef InventoryLayout* (*RootObjectCreateInventoryLayoutFn)(RootObject*);
typedef void (*InventoryLayoutCreateGUIFn)(
    void*,
    InventoryGUI*,
    Ogre::map<std::string, InventorySectionGUI*>::type&,
    Inventory*);
typedef void (*PlayerInterfacePickupItemFn)(PlayerInterface*, Item*);
typedef bool (*InventoryTransferMouseItemFn)(Inventory*, Item*);
typedef bool (*InventorySectionRemoveItemFn)(InventorySection*, Item*);
typedef void (*InventorySectionAddItemFn)(InventorySection*, Item*, int, int);
typedef Item* (*InventoryRemoveItemDontDestroyFn)(Inventory*, Item*, int, bool);
typedef bool (*InventoryTakeItemEntireStackFn)(Inventory*, Item*);
typedef void (*InventorySectionAddItemCallbackFn)(Inventory*, Item*);
typedef void (*InventorySectionUpdateItemCallbackFn)(Inventory*, Item*, int);
typedef void (*InventorySectionRemoveItemCallbackFn)(Inventory*, Item*);
typedef void (*InventoryAddToListFn)(Inventory*, Item*);
typedef void (*InventoryRemoveFromListFn)(Inventory*, Item*, bool);
typedef void (*ItemResetAfterCopyFn)(Item*);
typedef void (*InventoryItemAddQuantityFn)(InventoryItemBase*, int&, Item*, InventorySection*);

static const int kSearchInputConfiguredWidthMin = 120;
static const int kSearchInputConfiguredWidthMax = 720;
static const int kSearchInputConfiguredHeightMin = 22;
static const int kSearchInputConfiguredHeightMax = 48;
static const int kDefaultSearchInputConfiguredWidth = 372;
static const int kDefaultSearchInputConfiguredHeight = 26;

enum TraderSortMode
{
    TraderSortMode_None = 0,
    TraderSortMode_PriceAscending,
    TraderSortMode_PriceDescending,
};

struct TraderConfigSnapshot
{
    TraderConfigSnapshot()
        : enabled(true)
        , showSearchEntryCount(true)
        , showSearchQuantityCount(true)
        , showSearchClearButton(true)
        , debugLogging(false)
        , debugSearchLogging(false)
        , debugBindingLogging(false)
        , searchInputWidth(kDefaultSearchInputConfiguredWidth)
        , searchInputHeight(kDefaultSearchInputConfiguredHeight)
        , searchInputPositionCustomized(false)
        , searchInputLeft(0)
        , searchInputTop(0)
    {
    }

    bool enabled;
    bool showSearchEntryCount;
    bool showSearchQuantityCount;
    bool showSearchClearButton;
    bool debugLogging;
    bool debugSearchLogging;
    bool debugBindingLogging;
    int searchInputWidth;
    int searchInputHeight;
    bool searchInputPositionCustomized;
    int searchInputLeft;
    int searchInputTop;
};

struct SectionWidgetInventoryLink
{
    MyGUI::Widget* sectionWidget;
    Inventory* inventory;
    std::string sectionName;
    std::string widgetName;
    std::size_t itemCount;
    unsigned long long lastSeenTick;
};

struct InventoryGuiInventoryLink
{
    InventoryGUI* inventoryGui;
    Inventory* inventory;
    std::string ownerName;
    std::size_t itemCount;
    unsigned long long lastSeenTick;
};

enum InventoryGuiBackPointerKind
{
    InventoryGuiBackPointerKind_DirectInventory,
    InventoryGuiBackPointerKind_OwnerObject,
    InventoryGuiBackPointerKind_CallbackObject,
};

struct InventoryGuiBackPointerOffset
{
    std::size_t offset;
    InventoryGuiBackPointerKind kind;
    std::size_t hits;
};

struct TraderPanelInventoryBinding
{
    MyGUI::Widget* traderParent;
    MyGUI::Widget* entriesRoot;
    Inventory* inventory;
    std::string stage;
    std::string source;
    std::size_t expectedEntryCount;
    std::size_t nonEmptyKeyCount;
    unsigned long long lastSeenTick;
};

struct TraderEntryBaseCoord
{
    MyGUI::Widget* widget;
    int left;
    int top;
    int width;
    int height;
};

struct TraderSortedInventoryItemBasePosition
{
    TraderSortedInventoryItemBasePosition()
        : item(0)
        , leftCell(0)
        , topCell(0)
    {
    }

    Item* item;
    int leftCell;
    int topCell;
    std::string sectionName;
};

struct RefreshedInventoryLink
{
    Inventory* inventory;
    std::size_t itemCount;
    bool visible;
    bool ownerTrader;
    bool ownerSelected;
    std::string ownerName;
    unsigned long long lastSeenTick;
};

struct CoreState
{
    CoreState()
        : g_updateTickCounter(0)
        , g_controlsEnabled(true)
        , g_showSearchEntryCount(true)
        , g_showSearchQuantityCount(true)
        , g_showSearchClearButton(true)
        , g_debugLogging(false)
        , g_debugSearchLogging(false)
        , g_debugBindingLogging(false)
        , g_searchInputConfiguredWidth(372)
        , g_searchInputConfiguredHeight(26)
    {
    }

    unsigned long long g_updateTickCounter;
    bool g_controlsEnabled;
    bool g_showSearchEntryCount;
    bool g_showSearchQuantityCount;
    bool g_showSearchClearButton;
    bool g_debugLogging;
    bool g_debugSearchLogging;
    bool g_debugBindingLogging;
    int g_searchInputConfiguredWidth;
    int g_searchInputConfiguredHeight;
};

struct WindowDetectionState
{
    WindowDetectionState()
        : g_loggedNoVisibleTraderTarget(false)
    {
    }

    bool g_loggedNoVisibleTraderTarget;
    std::string g_lastBackpackResolutionSignature;
};

struct SearchState
{
    SearchState()
        : g_searchFilterDirty(false)
        , g_loggedMissingBackpackForSearch(false)
        , g_loggedMissingSearchableItemText(false)
        , g_loggedNumericOnlyQueryIgnored(false)
        , g_sortMode(TraderSortMode_None)
        , g_lockedKeysetTraderParent(0)
        , g_sortedEntriesRoot(0)
        , g_sortedInventory(0)
        , g_lockedKeysetExpectedCount(0)
        , g_lastSearchVisibleEntryCount(0)
        , g_lastSearchTotalEntryCount(0)
    {
    }

    bool g_searchFilterDirty;
    bool g_loggedMissingBackpackForSearch;
    bool g_loggedMissingSearchableItemText;
    bool g_loggedNumericOnlyQueryIgnored;
    TraderSortMode g_sortMode;
    std::string g_searchQueryRaw;
    std::string g_searchQueryNormalized;
    std::string g_pendingSlashFocusBaseQuery;
    std::string g_activeTraderTargetId;
    std::string g_lastZeroMatchQueryLogged;
    std::string g_lastInventoryKeysetSelectionSignature;
    std::string g_lastInventoryKeysetLowCoverageSignature;
    std::string g_lastObservedTraderEntriesStateSignature;
    std::string g_lastZeroMatchGuardSignature;
    std::string g_lastCoverageFallbackDecisionSignature;
    std::string g_lastSearchSampleQueryLogged;
    std::string g_lastSortInvestigationSignature;
    MyGUI::Widget* g_lockedKeysetTraderParent;
    MyGUI::Widget* g_sortedEntriesRoot;
    std::string g_lockedKeysetStage;
    std::string g_lockedKeysetSourceId;
    std::string g_lockedKeysetSourcePreview;
    std::vector<TraderEntryBaseCoord> g_entryBaseCoords;
    Inventory* g_sortedInventory;
    std::vector<TraderSortedInventoryItemBasePosition> g_sortedInventoryBasePositions;
    std::size_t g_lockedKeysetExpectedCount;
    std::string g_lastKeysetLockSignature;
    std::size_t g_lastSearchVisibleEntryCount;
    std::size_t g_lastSearchTotalEntryCount;
};

struct SearchUiState
{
    SearchUiState()
        : g_prevToggleHotkeyDown(false)
        , g_prevDiagnosticsHotkeyDown(false)
        , g_prevSearchSlashHotkeyDown(false)
        , g_prevSearchCtrlFHotkeyDown(false)
        , g_controlsWereInjected(false)
        , g_suppressNextSearchEditChangeEvent(false)
        , g_pendingSlashFocusTextSuppression(false)
        , g_focusSearchEditOnNextInjection(false)
        , g_searchContainerDragging(false)
        , g_searchContainerPositionCustomized(false)
        , g_searchContainerDragLastMouseX(0)
        , g_searchContainerDragLastMouseY(0)
        , g_searchContainerDragStartLeft(0)
        , g_searchContainerDragStartTop(0)
        , g_searchContainerStoredLeft(0)
        , g_searchContainerStoredTop(0)
    {
    }

    bool g_prevToggleHotkeyDown;
    bool g_prevDiagnosticsHotkeyDown;
    bool g_prevSearchSlashHotkeyDown;
    bool g_prevSearchCtrlFHotkeyDown;
    bool g_controlsWereInjected;
    bool g_suppressNextSearchEditChangeEvent;
    bool g_pendingSlashFocusTextSuppression;
    bool g_focusSearchEditOnNextInjection;
    bool g_searchContainerDragging;
    bool g_searchContainerPositionCustomized;
    int g_searchContainerDragLastMouseX;
    int g_searchContainerDragLastMouseY;
    int g_searchContainerDragStartLeft;
    int g_searchContainerDragStartTop;
    int g_searchContainerStoredLeft;
    int g_searchContainerStoredTop;
};

struct BindingState
{
    BindingState()
        : g_loggedInventoryBindingFailure(false)
        , g_loggedInventoryBindingDiagnostics(false)
        , g_loggedWidgetInventoryCandidatesMissing(false)
        , g_cachedHoveredWidgetInventory(0)
    {
    }

    bool g_loggedInventoryBindingFailure;
    bool g_loggedInventoryBindingDiagnostics;
    bool g_loggedWidgetInventoryCandidatesMissing;
    Inventory* g_cachedHoveredWidgetInventory;
    std::string g_cachedHoveredWidgetInventorySignature;
    std::string g_lastSectionWidgetBindingSignature;
    std::vector<SectionWidgetInventoryLink> g_sectionWidgetInventoryLinks;
    std::string g_lastInventoryGuiBindingSignature;
    std::vector<InventoryGuiInventoryLink> g_inventoryGuiInventoryLinks;
    std::vector<InventoryGuiBackPointerOffset> g_inventoryGuiBackPointerOffsets;
    std::string g_lastInventoryGuiBackPointerLearningSignature;
    std::string g_lastInventoryGuiBackPointerResolutionSignature;
    std::string g_lastInventoryGuiBackPointerResolutionFailureSignature;
    std::vector<TraderPanelInventoryBinding> g_traderPanelInventoryBindings;
    std::string g_lastPanelBindingSignature;
    std::string g_lastPanelBindingRefusedSignature;
    std::string g_lastPanelBindingProbeSignature;
    std::vector<RefreshedInventoryLink> g_recentRefreshedInventories;
};

struct HookState
{
    HookState()
        : g_updateUTOrig(0)
        , g_characterCreateInventoryLayoutOrig(0)
        , g_rootObjectCreateInventoryLayoutOrig(0)
        , g_inventoryLayoutCreateGUIOrig(0)
        , g_playerInterfacePickupItemOrig(0)
        , g_inventoryTransferMouseItemOrig(0)
        , g_inventorySectionRemoveItemOrig(0)
        , g_inventorySectionAddItemOrig(0)
        , g_inventoryRemoveItemDontDestroyOrig(0)
        , g_inventoryTakeItemEntireStackOrig(0)
        , g_inventorySectionAddItemCallbackOrig(0)
        , g_inventorySectionUpdateItemCallbackOrig(0)
        , g_inventorySectionRemoveItemCallbackOrig(0)
        , g_inventoryAddToListOrig(0)
        , g_inventoryRemoveFromListOrig(0)
        , g_itemResetAfterCopyOrig(0)
        , g_inventoryItemAddQuantityOrig(0)
        , g_inventoryLayoutCreateGUIHookInstalled(false)
        , g_inventoryLayoutCreateGUIHookAttempted(false)
        , g_expectedInventoryLayoutCreateGUIAddress(0)
        , g_inventoryLayoutCreateGUIEarlyAttempted(false)
        , g_inventoryLayoutCreateGUIHookCallCount(0)
        , g_inventoryLayoutCreateInventoryLayoutLogCount(0)
        , g_inventoryMoveProbeSequence(0)
    {
    }

    PlayerInterfaceUpdateUTFn g_updateUTOrig;
    CharacterCreateInventoryLayoutFn g_characterCreateInventoryLayoutOrig;
    RootObjectCreateInventoryLayoutFn g_rootObjectCreateInventoryLayoutOrig;
    InventoryLayoutCreateGUIFn g_inventoryLayoutCreateGUIOrig;
    PlayerInterfacePickupItemFn g_playerInterfacePickupItemOrig;
    InventoryTransferMouseItemFn g_inventoryTransferMouseItemOrig;
    InventorySectionRemoveItemFn g_inventorySectionRemoveItemOrig;
    InventorySectionAddItemFn g_inventorySectionAddItemOrig;
    InventoryRemoveItemDontDestroyFn g_inventoryRemoveItemDontDestroyOrig;
    InventoryTakeItemEntireStackFn g_inventoryTakeItemEntireStackOrig;
    InventorySectionAddItemCallbackFn g_inventorySectionAddItemCallbackOrig;
    InventorySectionUpdateItemCallbackFn g_inventorySectionUpdateItemCallbackOrig;
    InventorySectionRemoveItemCallbackFn g_inventorySectionRemoveItemCallbackOrig;
    InventoryAddToListFn g_inventoryAddToListOrig;
    InventoryRemoveFromListFn g_inventoryRemoveFromListOrig;
    ItemResetAfterCopyFn g_itemResetAfterCopyOrig;
    InventoryItemAddQuantityFn g_inventoryItemAddQuantityOrig;
    bool g_inventoryLayoutCreateGUIHookInstalled;
    bool g_inventoryLayoutCreateGUIHookAttempted;
    std::uintptr_t g_expectedInventoryLayoutCreateGUIAddress;
    bool g_inventoryLayoutCreateGUIEarlyAttempted;
    std::size_t g_inventoryLayoutCreateGUIHookCallCount;
    std::size_t g_inventoryLayoutCreateInventoryLayoutLogCount;
    std::size_t g_inventoryMoveProbeSequence;
    std::string g_lastInventoryLayoutReturnSignature;
};

struct TraderRuntimeState
{
    CoreState core;
    WindowDetectionState windowDetection;
    SearchState search;
    SearchUiState searchUi;
    BindingState binding;
    HookState hook;
};

TraderRuntimeState& TraderState();

void LogInfoLine(const std::string& message);
void LogWarnLine(const std::string& message);
void LogErrorLine(const std::string& message);
bool ShouldCompileVerboseDiagnostics();
bool ShouldLogDebug();
bool ShouldLogSearchDebug();
bool ShouldLogBindingDebug();
bool ShouldLogVerboseSearchDiagnostics();
bool ShouldLogVerboseBindingDiagnostics();
void LogDebugLine(const std::string& message);
void LogSearchDebugLine(const std::string& message);
void LogBindingDebugLine(const std::string& message);
int ClampSearchInputConfiguredWidth(int value);
int ClampSearchInputConfiguredHeight(int value);
void NormalizeTraderConfigSnapshot(TraderConfigSnapshot* config);
TraderConfigSnapshot CaptureTraderConfigSnapshot();
void ApplyTraderConfigSnapshot(const TraderConfigSnapshot& config);
bool SaveTraderConfigSnapshot(const TraderConfigSnapshot& config);
void LoadModConfig();
