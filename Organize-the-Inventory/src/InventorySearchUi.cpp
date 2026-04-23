#include "InventorySearchUi.h"

#include "InventoryConfig.h"
#include "InventoryCore.h"
#include "InventoryDiagnostics.h"
#include "InventoryPerformanceTelemetry.h"
#include "InventorySearchFilterScheduler.h"
#include "InventorySearchInputBehavior.h"
#include "InventorySearchPipeline.h"
#include "InventorySearchTargetCache.h"
#include "InventoryWindowDetection.h"

#include <kenshi/Globals.h>
#include <kenshi/InputHandler.h>

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_EditBox.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>

#include <ois/OISKeyboard.h>

#include <Windows.h>

#include <sstream>

namespace
{
const char* kControlsContainerName = "OTI_InventoryControlsContainer";
const char* kSearchEditName = "OTI_SearchEdit";
const char* kSearchPlaceholderName = "OTI_SearchPlaceholder";
const char* kSearchClearButtonName = "OTI_SearchClearButton";
const char* kSearchDragHandleName = "OTI_SearchDragHandle";
const char* kSearchCountTextName = "OTI_SearchCountText";
const int kPanelOuterPadding = 8;
const int kSearchCountGap = 2;
const int kSearchCountWidthMin = 56;
const int kPanelHandleWidth = 28;
const int kPanelHandleGap = 6;
const int kPanelEdgeInset = 0;
const int kRightMargin = 0;
const int kTopMargin = 0;

enum SearchFocusHotkeyKind
{
    SearchFocusHotkeyKind_None,
    SearchFocusHotkeyKind_Slash,
    SearchFocusHotkeyKind_CtrlF,
};

struct PendingSearchEditShortcut
{
    PendingSearchEditShortcut()
        : active(false)
        , keyValue(0)
    {
    }

    bool active;
    int keyValue;
    InventorySearchInputBehavior::EditResult editResult;
};

bool g_loggedNoVisibleInventoryTarget = false;
bool g_prevDiagnosticsHotkeyDown = false;
bool g_prevSearchSlashHotkeyDown = false;
bool g_prevSearchCtrlFHotkeyDown = false;
bool g_searchContainerDragging = false;
bool g_searchContainerUsesCreatureLayout = false;
bool g_pendingSlashFocusTextSuppression = false;
bool g_suppressNextSearchEditChangeEvent = false;
bool g_haveSearchEditSnapshot = false;
int g_searchContainerDragLastMouseX = 0;
int g_searchContainerDragLastMouseY = 0;
int g_searchContainerDragStartLeft = 0;
int g_searchContainerDragStartTop = 0;
PendingSearchEditShortcut g_pendingSearchEditShortcut;
InventorySearchInputBehavior::Snapshot g_searchEditSnapshot;
std::string g_pendingSlashFocusBaseQuery;

struct SearchContainerMetrics
{
    SearchContainerMetrics()
        : reserveCount(false)
        , countWidth(0)
        , containerWidth(0)
        , containerHeight(0)
        , clearButtonWidth(0)
        , searchRowTop(0)
        , searchInputLeft(0)
        , searchAreaWidth(0)
    {
    }

    bool reserveCount;
    int countWidth;
    int containerWidth;
    int containerHeight;
    int clearButtonWidth;
    int searchRowTop;
    int searchInputLeft;
    int searchAreaWidth;
};

MyGUI::Widget* FindControlsContainer()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return 0;
    }

    return gui->findWidgetT(kControlsContainerName, false);
}

MyGUI::EditBox* FindSearchEditBox()
{
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0)
    {
        return 0;
    }

    MyGUI::Widget* found =
        FindNamedDescendantRecursive(controlsContainer, kSearchEditName, false);
    return found == 0 ? 0 : found->castType<MyGUI::EditBox>(false);
}

MyGUI::TextBox* FindSearchPlaceholderTextBox()
{
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0)
    {
        return 0;
    }

    MyGUI::Widget* found =
        FindNamedDescendantRecursive(controlsContainer, kSearchPlaceholderName, false);
    return found == 0 ? 0 : found->castType<MyGUI::TextBox>(false);
}

MyGUI::Button* FindSearchClearButton()
{
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0)
    {
        return 0;
    }

    MyGUI::Widget* found =
        FindNamedDescendantRecursive(controlsContainer, kSearchClearButtonName, false);
    return found == 0 ? 0 : found->castType<MyGUI::Button>(false);
}

MyGUI::TextBox* FindSearchCountTextBox()
{
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0)
    {
        return 0;
    }

    MyGUI::Widget* found =
        FindNamedDescendantRecursive(controlsContainer, kSearchCountTextName, false);
    return found == 0 ? 0 : found->castType<MyGUI::TextBox>(false);
}

void DestroyWidgetDirect(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui != 0)
    {
        gui->destroyWidget(widget);
    }
}

bool TryGetControlsVisibleParent(
    MyGUI::Widget* controlsContainer,
    MyGUI::Widget*& parent)
{
    parent = 0;
    if (controlsContainer == 0)
    {
        return false;
    }

    __try
    {
        parent = controlsContainer->getParent();
        return parent != 0 && parent->getInheritedVisible();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        parent = 0;
        return false;
    }
}

bool ControlsParentIsSafelyVisible(MyGUI::Widget* controlsContainer)
{
    if (controlsContainer == 0)
    {
        return true;
    }

    MyGUI::Widget* parent = 0;
    return TryGetControlsVisibleParent(controlsContainer, parent);
}

bool IsVirtualKeyDown(int virtualKey)
{
    return virtualKey > 0 && (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

bool IsSlashCharacterChordDown(bool shiftDown, bool ctrlDown, bool altDown)
{
    const HKL keyboardLayout = GetKeyboardLayout(0);
    const SHORT slashMapping = VkKeyScanExA('/', keyboardLayout);
    if (slashMapping != -1)
    {
        const int virtualKey = LOBYTE(static_cast<WORD>(slashMapping));
        const BYTE modifierMask = HIBYTE(static_cast<WORD>(slashMapping));
        const bool shiftRequired = (modifierMask & 1U) != 0;
        const bool ctrlRequired = (modifierMask & 2U) != 0;
        const bool altRequired = (modifierMask & 4U) != 0;

        if (shiftDown == shiftRequired
            && ctrlDown == ctrlRequired
            && altDown == altRequired
            && IsVirtualKeyDown(virtualKey))
        {
            return true;
        }
    }

    const bool oemSlashDown = IsVirtualKeyDown(VK_OEM_2);
    const bool numpadSlashDown = IsVirtualKeyDown(VK_DIVIDE);
    return !ctrlDown && !altDown && ((!shiftDown && oemSlashDown) || numpadSlashDown);
}

bool IsSearchEditFocused(MyGUI::EditBox* searchEdit)
{
    if (searchEdit == 0)
    {
        return false;
    }

    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    return input != 0 && input->getKeyFocusWidget() == searchEdit;
}

void FocusSearchEdit(MyGUI::EditBox* searchEdit, const char* reason)
{
    if (searchEdit == 0)
    {
        return;
    }

    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    if (input == 0)
    {
        LogWarnLine("search focus skipped: MyGUI InputManager unavailable");
        return;
    }

    input->setKeyFocusWidget(searchEdit);

    std::stringstream line;
    line << "inventory search edit focused"
         << " reason=" << (reason == 0 ? "<unknown>" : reason);
    LogDebugLine(line.str());
}

void BlurSearchEdit(MyGUI::EditBox* searchEdit, const char* reason)
{
    if (searchEdit == 0)
    {
        return;
    }

    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    if (input == 0)
    {
        LogWarnLine("inventory search blur skipped: MyGUI InputManager unavailable");
        return;
    }

    if (input->getKeyFocusWidget() != searchEdit)
    {
        return;
    }

    input->setKeyFocusWidget(0);

    std::stringstream line;
    line << "inventory search edit blurred"
         << " reason=" << (reason == 0 ? "<unknown>" : reason);
    LogDebugLine(line.str());
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

bool IsInterestingSearchEditMyGuiKey(MyGUI::KeyCode keyCode)
{
    const int value = keyCode.getValue();
    return value == MyGUI::KeyCode::LeftControl
        || value == MyGUI::KeyCode::RightControl
        || value == MyGUI::KeyCode::ArrowLeft
        || value == MyGUI::KeyCode::ArrowRight
        || value == MyGUI::KeyCode::Backspace
        || value == MyGUI::KeyCode::Tab;
}

void ResetPendingSearchEditShortcut()
{
    g_pendingSearchEditShortcut = PendingSearchEditShortcut();
}

void ResetSearchEditSnapshot()
{
    g_haveSearchEditSnapshot = false;
    g_searchEditSnapshot = InventorySearchInputBehavior::Snapshot();
}

InventorySearchInputBehavior::Text ToSearchInputText(const MyGUI::UString& text)
{
    InventorySearchInputBehavior::Text result;
    const std::size_t length = text.size();
    result.reserve(length);
    for (std::size_t index = 0; index < length; ++index)
    {
        result.push_back(static_cast<InventorySearchInputBehavior::Codepoint>(text[index]));
    }

    return result;
}

MyGUI::UString ToMyGuiText(const InventorySearchInputBehavior::Text& text)
{
    MyGUI::UString result;
    const std::size_t length = text.size();
    for (std::size_t index = 0; index < length; ++index)
    {
        result.push_back(static_cast<MyGUI::UString::unicode_char>(text[index]));
    }

    return result;
}

InventorySearchInputBehavior::Selection CaptureSearchEditSelection(
    MyGUI::EditBox* searchEdit,
    std::size_t textLength)
{
    if (searchEdit == 0 || !searchEdit->isTextSelection())
    {
        return InventorySearchInputBehavior::Selection();
    }

    const std::size_t selectionStart = searchEdit->getTextSelectionStart();
    if (selectionStart == MyGUI::ITEM_NONE)
    {
        return InventorySearchInputBehavior::Selection();
    }

    const std::size_t selectionLength = searchEdit->getTextSelectionLength();
    return InventorySearchInputBehavior::NormalizeSelection(
        InventorySearchInputBehavior::Selection(true, selectionStart, selectionLength),
        textLength);
}

InventorySearchInputBehavior::Snapshot BuildSearchInputSnapshot(
    const MyGUI::UString& text,
    std::size_t cursorPosition,
    const InventorySearchInputBehavior::Selection& selection)
{
    return InventorySearchInputBehavior::Snapshot(
        ToSearchInputText(text),
        cursorPosition,
        InventorySearchInputBehavior::NormalizeSelection(selection, text.size()));
}

InventorySearchInputBehavior::Snapshot CaptureSearchEditSnapshot(MyGUI::EditBox* searchEdit)
{
    if (searchEdit == 0)
    {
        return InventorySearchInputBehavior::Snapshot();
    }

    const MyGUI::UString text = searchEdit->getOnlyText();
    const std::size_t textLength = text.size();
    const std::size_t cursorPosition = InventorySearchInputBehavior::ClampCursor(
        searchEdit->getTextCursor(),
        textLength);
    return BuildSearchInputSnapshot(
        text,
        cursorPosition,
        CaptureSearchEditSelection(searchEdit, textLength));
}

InventorySearchInputBehavior::ShortcutKind ClassifySearchEditShortcut(MyGUI::KeyCode keyCode)
{
    const int keyValue = keyCode.getValue();
    if (keyValue == MyGUI::KeyCode::ArrowLeft)
    {
        return InventorySearchInputBehavior::ShortcutKind_CtrlLeft;
    }

    if (keyValue == MyGUI::KeyCode::ArrowRight)
    {
        return InventorySearchInputBehavior::ShortcutKind_CtrlRight;
    }

    if (keyValue == MyGUI::KeyCode::Backspace)
    {
        return InventorySearchInputBehavior::ShortcutKind_CtrlBackspace;
    }

    return InventorySearchInputBehavior::ShortcutKind_None;
}

void ApplySearchEditSelection(
    MyGUI::EditBox* searchEdit,
    std::size_t cursorPosition,
    const InventorySearchInputBehavior::Selection& selection)
{
    if (searchEdit == 0)
    {
        return;
    }

    const std::size_t textLength = searchEdit->getTextLength();
    const std::size_t clampedCursor =
        InventorySearchInputBehavior::ClampCursor(cursorPosition, textLength);
    const InventorySearchInputBehavior::Selection normalizedSelection =
        InventorySearchInputBehavior::NormalizeSelection(selection, textLength);
    if (normalizedSelection.active)
    {
        searchEdit->setTextSelection(
            normalizedSelection.start,
            normalizedSelection.start + normalizedSelection.length);
        return;
    }

    searchEdit->setTextCursor(clampedCursor);
    searchEdit->setTextSelection(clampedCursor, clampedCursor);
}

InventorySearchInputBehavior::Snapshot BuildScheduledSearchShortcutSnapshot(
    MyGUI::EditBox* searchEdit,
    InventorySearchInputBehavior::ShortcutKind shortcut)
{
    InventorySearchInputBehavior::Snapshot snapshot = CaptureSearchEditSnapshot(searchEdit);
    if (shortcut == InventorySearchInputBehavior::ShortcutKind_CtrlBackspace
        && g_haveSearchEditSnapshot)
    {
        snapshot.text = g_searchEditSnapshot.text;
        snapshot.cursor = InventorySearchInputBehavior::ClampCursor(
            g_searchEditSnapshot.cursor,
            snapshot.text.size());
        snapshot.selection = InventorySearchInputBehavior::NormalizeSelection(
            snapshot.selection,
            snapshot.text.size());
    }

    return snapshot;
}

void UpdateSearchUiState();

void RememberSearchEditSnapshotValue(
    const std::string& query,
    std::size_t cursorPosition,
    const InventorySearchInputBehavior::Selection& selection)
{
    g_haveSearchEditSnapshot = true;
    g_searchEditSnapshot = BuildSearchInputSnapshot(MyGUI::UString(query), cursorPosition, selection);
}

void RememberSearchEditSnapshot(MyGUI::EditBox* searchEdit)
{
    if (searchEdit == 0)
    {
        ResetSearchEditSnapshot();
        return;
    }

    g_haveSearchEditSnapshot = true;
    g_searchEditSnapshot = CaptureSearchEditSnapshot(searchEdit);
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

    InventoryState().g_searchQueryRaw = rawText;
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
    line << "inventory search ui action"
         << " reason=" << (reason == 0 ? "<unknown>" : reason)
         << " raw=\"" << InventoryState().g_searchQueryRaw << "\"";
    LogSearchDebugLine(line.str());

    UpdateSearchUiState();
    RememberSearchEditSnapshot(searchEdit);
}

void ApplySearchShortcutQueryAndCursor(
    MyGUI::EditBox* searchEdit,
    const InventorySearchInputBehavior::EditResult& editResult,
    const char* reason)
{
    if (searchEdit == 0 || !editResult.handled || !editResult.rewriteText)
    {
        return;
    }

    const std::string rawText = ToMyGuiText(editResult.text).asUTF8();
    SetSearchQueryAndRefresh(searchEdit, rawText, reason, false);
    ApplySearchEditSelection(searchEdit, editResult.cursor, editResult.selection);
    RememberSearchEditSnapshotValue(rawText, editResult.cursor, editResult.selection);
}

bool ScheduleSearchEditMyGuiShortcut(MyGUI::EditBox* searchEdit, MyGUI::KeyCode keyCode)
{
    if (searchEdit == 0)
    {
        return false;
    }

    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0 || !inputManager->isControlPressed())
    {
        return false;
    }

    const InventorySearchInputBehavior::ShortcutKind shortcut =
        ClassifySearchEditShortcut(keyCode);
    if (shortcut == InventorySearchInputBehavior::ShortcutKind_None)
    {
        return false;
    }

    ResetPendingSearchEditShortcut();
    g_pendingSearchEditShortcut.active = true;
    g_pendingSearchEditShortcut.keyValue = keyCode.getValue();
    g_pendingSearchEditShortcut.editResult = InventorySearchInputBehavior::ApplyShortcut(
        shortcut,
        BuildScheduledSearchShortcutSnapshot(searchEdit, shortcut));
    if (!g_pendingSearchEditShortcut.editResult.handled)
    {
        ResetPendingSearchEditShortcut();
        return false;
    }

    return true;
}

void ApplyPendingSearchEditShortcut(MyGUI::EditBox* searchEdit, MyGUI::KeyCode keyCode)
{
    if (!g_pendingSearchEditShortcut.active
        || g_pendingSearchEditShortcut.keyValue != keyCode.getValue())
    {
        return;
    }

    const PendingSearchEditShortcut pending = g_pendingSearchEditShortcut;
    ResetPendingSearchEditShortcut();

    if (searchEdit == 0 || !pending.editResult.handled)
    {
        return;
    }

    if (pending.editResult.rewriteText)
    {
        ApplySearchShortcutQueryAndCursor(searchEdit, pending.editResult, "ctrl_shortcut");
        return;
    }

    ApplySearchEditSelection(searchEdit, pending.editResult.cursor, pending.editResult.selection);
    RememberSearchEditSnapshot(searchEdit);
}

void OnSearchEditKeyPressed(MyGUI::Widget* sender, MyGUI::KeyCode keyCode, MyGUI::Char character)
{
    (void)character;

    MyGUI::EditBox* searchEdit = sender == 0 ? 0 : sender->castType<MyGUI::EditBox>(false);
    if (searchEdit == 0 || !IsInterestingSearchEditMyGuiKey(keyCode))
    {
        return;
    }

    if (keyCode.getValue() != MyGUI::KeyCode::Tab)
    {
        ScheduleSearchEditMyGuiShortcut(searchEdit, keyCode);
        return;
    }

    BlurSearchEdit(searchEdit, "tab_key");
    ResetPendingSearchEditShortcut();
    RememberSearchEditSnapshot(searchEdit);
    UpdateSearchUiState();
}

void OnSearchEditKeyReleased(MyGUI::Widget* sender, MyGUI::KeyCode keyCode)
{
    if (sender == 0)
    {
        ResetPendingSearchEditShortcut();
        ResetSearchEditSnapshot();
        return;
    }

    MyGUI::EditBox* searchEdit = sender->castType<MyGUI::EditBox>(false);
    if (searchEdit == 0)
    {
        ResetPendingSearchEditShortcut();
        return;
    }

    if (IsInterestingSearchEditMyGuiKey(keyCode))
    {
        ApplyPendingSearchEditShortcut(searchEdit, keyCode);
    }

    RememberSearchEditSnapshot(searchEdit);
}

SearchFocusHotkeyKind DetectSearchFocusHotkeyPressedEdge(MyGUI::EditBox* searchEdit)
{
    if (searchEdit == 0 || key == 0 || key->keyboard == 0)
    {
        g_prevSearchSlashHotkeyDown = false;
        g_prevSearchCtrlFHotkeyDown = false;
        return SearchFocusHotkeyKind_None;
    }

    const bool searchFocused = IsSearchEditFocused(searchEdit);
    const bool ctrlDown = key->keyboard->isKeyDown(OIS::KC_LCONTROL)
        || key->keyboard->isKeyDown(OIS::KC_RCONTROL);
    const bool altDown = key->keyboard->isKeyDown(OIS::KC_LMENU)
        || key->keyboard->isKeyDown(OIS::KC_RMENU);
    const bool shiftDown = key->keyboard->isKeyDown(OIS::KC_LSHIFT)
        || key->keyboard->isKeyDown(OIS::KC_RSHIFT);
    const bool fDown = key->keyboard->isKeyDown(OIS::KC_F);

    const bool slashChordDown =
        !searchFocused && IsSlashCharacterChordDown(shiftDown, ctrlDown, altDown);
    const bool ctrlFChordDown = !searchFocused && ctrlDown && fDown;

    const bool slashPressedEdge = slashChordDown && !g_prevSearchSlashHotkeyDown;
    const bool ctrlFPressedEdge = ctrlFChordDown && !g_prevSearchCtrlFHotkeyDown;

    g_prevSearchSlashHotkeyDown = slashChordDown;
    g_prevSearchCtrlFHotkeyDown = ctrlFChordDown;

    if (slashPressedEdge)
    {
        return SearchFocusHotkeyKind_Slash;
    }
    if (ctrlFPressedEdge)
    {
        return SearchFocusHotkeyKind_CtrlF;
    }

    return SearchFocusHotkeyKind_None;
}

bool TryGetCurrentMousePosition(int* xOut, int* yOut)
{
    if (xOut == 0 || yOut == 0)
    {
        return false;
    }

    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        return false;
    }

    const MyGUI::IntPoint mouse = inputManager->getMousePosition();
    *xOut = mouse.left;
    *yOut = mouse.top;
    return true;
}

bool ParentLooksLikePlayerInventorySearchTarget(MyGUI::Widget* parent)
{
    if (parent == 0)
    {
        return false;
    }

    return FindWidgetInParentByToken(parent, "CharacterSelectionItemBox") != 0
        || (FindWidgetInParentByToken(parent, "Inventory") != 0
            && FindWidgetInParentByToken(parent, "Equipment") != 0);
}

bool ParentLooksLikeCreatureSearchTarget(MyGUI::Widget* parent)
{
    if (parent == 0 || ParentLooksLikePlayerInventorySearchTarget(parent))
    {
        return false;
    }

    const bool hasCreatureBackpackToken =
        FindWidgetInParentByToken(parent, "backpack_attach") != 0
        || FindWidgetInParentByToken(parent, "lbBackpack") != 0;
    if (!hasCreatureBackpackToken)
    {
        return false;
    }

    const MyGUI::IntCoord coord = parent->getAbsoluteCoord();
    return coord.width >= 200
        && coord.width <= 700
        && coord.height >= 120
        && coord.height <= 380;
}

void SetSearchContainerUsesCreatureLayout(bool usesCreatureLayout)
{
    g_searchContainerUsesCreatureLayout = usesCreatureLayout;
}

int CurrentSearchBarConfiguredWidth()
{
    return g_searchContainerUsesCreatureLayout
        ? InventoryState().g_creatureSearchBarConfiguredWidth
        : InventoryState().g_searchBarConfiguredWidth;
}

int CurrentSearchInputConfiguredWidth()
{
    return g_searchContainerUsesCreatureLayout
        ? InventoryState().g_creatureSearchInputConfiguredWidth
        : InventoryState().g_searchInputConfiguredWidth;
}

int CurrentSearchInputConfiguredHeight()
{
    return g_searchContainerUsesCreatureLayout
        ? InventoryState().g_creatureSearchInputConfiguredHeight
        : InventoryState().g_searchInputConfiguredHeight;
}

bool CurrentSearchInputPositionCustomized()
{
    return g_searchContainerUsesCreatureLayout
        ? InventoryState().g_creatureSearchInputPositionCustomized
        : InventoryState().g_searchInputPositionCustomized;
}

int CurrentSearchInputStoredLeft()
{
    return g_searchContainerUsesCreatureLayout
        ? InventoryState().g_creatureSearchInputStoredLeft
        : InventoryState().g_searchInputStoredLeft;
}

int CurrentSearchInputStoredTop()
{
    return g_searchContainerUsesCreatureLayout
        ? InventoryState().g_creatureSearchInputStoredTop
        : InventoryState().g_searchInputStoredTop;
}

void StoreCurrentSearchInputPosition(int left, int top)
{
    if (g_searchContainerUsesCreatureLayout)
    {
        InventoryState().g_creatureSearchInputStoredLeft = left;
        InventoryState().g_creatureSearchInputStoredTop = top;
        InventoryState().g_creatureSearchInputPositionCustomized = true;
        return;
    }

    InventoryState().g_searchInputStoredLeft = left;
    InventoryState().g_searchInputStoredTop = top;
    InventoryState().g_searchInputPositionCustomized = true;
}

SearchContainerMetrics BuildSearchContainerMetrics()
{
    SearchContainerMetrics metrics;
    metrics.reserveCount = InventoryState().g_showSearchEntryCount
        || InventoryState().g_showSearchQuantityCount;
    metrics.searchRowTop = kPanelOuterPadding;
    metrics.searchInputLeft = kPanelOuterPadding + kPanelHandleWidth + kPanelHandleGap;
    metrics.searchAreaWidth = CurrentSearchInputConfiguredWidth();
    metrics.clearButtonWidth =
        CurrentSearchInputConfiguredHeight() > 32
            ? 32
            : CurrentSearchInputConfiguredHeight();
    metrics.containerHeight =
        CurrentSearchInputConfiguredHeight() + (kPanelOuterPadding * 2);

    const int baseContainerWidth =
        kPanelOuterPadding
        + kPanelHandleWidth
        + kPanelHandleGap
        + metrics.searchAreaWidth
        + kPanelOuterPadding;
    if (!metrics.reserveCount)
    {
        metrics.containerWidth = baseContainerWidth;
        return metrics;
    }

    const int minimumContainerWidth = baseContainerWidth + kSearchCountGap + kSearchCountWidthMin;
    metrics.containerWidth = CurrentSearchBarConfiguredWidth();
    if (metrics.containerWidth < minimumContainerWidth)
    {
        metrics.containerWidth = minimumContainerWidth;
    }

    metrics.countWidth = metrics.containerWidth - baseContainerWidth - kSearchCountGap;
    return metrics;
}

MyGUI::IntCoord ClampPanelCoord(MyGUI::Widget* parent, const MyGUI::IntCoord& requested)
{
    if (parent == 0)
    {
        return requested;
    }

    MyGUI::IntCoord coord = requested;
    const int maxLeft = parent->getWidth() - coord.width - kPanelEdgeInset;
    const int maxTop = parent->getHeight() - coord.height - kPanelEdgeInset;
    if (coord.left > maxLeft)
    {
        coord.left = maxLeft;
    }
    if (coord.top > maxTop)
    {
        coord.top = maxTop;
    }
    if (coord.left < kPanelEdgeInset)
    {
        coord.left = kPanelEdgeInset;
    }
    if (coord.top < kPanelEdgeInset)
    {
        coord.top = kPanelEdgeInset;
    }
    return coord;
}

void RememberSearchContainerPosition(MyGUI::Widget* container)
{
    if (container == 0)
    {
        return;
    }

    const MyGUI::IntCoord coord = container->getCoord();
    StoreCurrentSearchInputPosition(coord.left, coord.top);
}

void PersistSearchContainerPosition()
{
    const InventoryConfigSnapshot config = CaptureInventoryConfigSnapshot();
    SaveInventoryConfigSnapshot(config);
}

void MoveSearchContainerByDelta(int deltaX, int deltaY)
{
    if (deltaX == 0 && deltaY == 0)
    {
        return;
    }

    MyGUI::Widget* container = FindControlsContainer();
    if (container == 0)
    {
        return;
    }

    MyGUI::Widget* parent = container->getParent();
    const MyGUI::IntCoord current = container->getCoord();
    const MyGUI::IntCoord moved = ClampPanelCoord(
        parent,
        MyGUI::IntCoord(current.left + deltaX, current.top + deltaY, current.width, current.height));
    if (moved.left == current.left && moved.top == current.top)
    {
        return;
    }

    container->setCoord(moved);
    RememberSearchContainerPosition(container);
}

void FinalizeSearchContainerDrag(const char* source)
{
    if (!g_searchContainerDragging)
    {
        return;
    }

    g_searchContainerDragging = false;
    MyGUI::Widget* container = FindControlsContainer();
    if (container == 0)
    {
        return;
    }

    RememberSearchContainerPosition(container);

    std::stringstream line;
    const MyGUI::IntCoord coord = container->getCoord();
    const bool positionChanged =
        coord.left != g_searchContainerDragStartLeft || coord.top != g_searchContainerDragStartTop;
    if (positionChanged)
    {
        PersistSearchContainerPosition();
    }

    line << "inventory search container drag finalized"
         << " source=" << (source == 0 ? "<unknown>" : source)
         << " moved=" << (positionChanged ? "true" : "false")
         << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";
    LogDebugLine(line.str());
}

void OnSearchDragHandleMousePressed(MyGUI::Widget*, int left, int top, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left)
    {
        return;
    }

    MyGUI::Widget* container = FindControlsContainer();
    if (container != 0)
    {
        const MyGUI::IntCoord coord = container->getCoord();
        g_searchContainerDragStartLeft = coord.left;
        g_searchContainerDragStartTop = coord.top;
    }
    else
    {
        g_searchContainerDragStartLeft = 0;
        g_searchContainerDragStartTop = 0;
    }

    g_searchContainerDragging = true;
    if (!TryGetCurrentMousePosition(&g_searchContainerDragLastMouseX, &g_searchContainerDragLastMouseY))
    {
        g_searchContainerDragLastMouseX = left;
        g_searchContainerDragLastMouseY = top;
    }
}

void OnSearchDragHandleMouseDrag(MyGUI::Widget*, int left, int top, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left || !g_searchContainerDragging)
    {
        return;
    }

    int mouseX = left;
    int mouseY = top;
    TryGetCurrentMousePosition(&mouseX, &mouseY);

    const int deltaX = mouseX - g_searchContainerDragLastMouseX;
    const int deltaY = mouseY - g_searchContainerDragLastMouseY;
    if (deltaX == 0 && deltaY == 0)
    {
        return;
    }

    MoveSearchContainerByDelta(deltaX, deltaY);
    g_searchContainerDragLastMouseX = mouseX;
    g_searchContainerDragLastMouseY = mouseY;
}

void OnSearchDragHandleMouseMove(MyGUI::Widget*, int left, int top)
{
    if (!g_searchContainerDragging)
    {
        return;
    }

    OnSearchDragHandleMouseDrag(0, left, top, MyGUI::MouseButton::Left);
}

void OnSearchDragHandleMouseReleased(MyGUI::Widget*, int, int, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left)
    {
        return;
    }

    FinalizeSearchContainerDrag("drag_release");
}

void TickSearchContainerDrag()
{
    if (!g_searchContainerDragging)
    {
        return;
    }

    int mouseX = 0;
    int mouseY = 0;
    if (TryGetCurrentMousePosition(&mouseX, &mouseY))
    {
        const int deltaX = mouseX - g_searchContainerDragLastMouseX;
        const int deltaY = mouseY - g_searchContainerDragLastMouseY;
        MoveSearchContainerByDelta(deltaX, deltaY);
        g_searchContainerDragLastMouseX = mouseX;
        g_searchContainerDragLastMouseY = mouseY;
    }

    if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)
    {
        FinalizeSearchContainerDrag("drag_release_poll");
    }
}

MyGUI::IntCoord BuildSearchContainerCoord(MyGUI::Widget* parent)
{
    const SearchContainerMetrics metrics = BuildSearchContainerMetrics();

    MyGUI::IntCoord coord(
        parent->getWidth() - metrics.containerWidth - kRightMargin,
        kTopMargin,
        metrics.containerWidth,
        metrics.containerHeight);
    if (CurrentSearchInputPositionCustomized())
    {
        coord.left = CurrentSearchInputStoredLeft();
        coord.top = CurrentSearchInputStoredTop();
    }
    return ClampPanelCoord(parent, coord);
}

void UpdateSearchUiState()
{
    MyGUI::EditBox* searchEdit = FindSearchEditBox();
    MyGUI::TextBox* placeholder = FindSearchPlaceholderTextBox();
    MyGUI::Button* clearButton = FindSearchClearButton();

    const bool hasQuery = !InventoryState().g_searchQueryRaw.empty();
    const bool searchFocused = IsSearchEditFocused(searchEdit);
    if (placeholder != 0)
    {
        placeholder->setVisible(!hasQuery && !searchFocused);
    }
    if (clearButton != 0)
    {
        clearButton->setVisible(InventoryState().g_showSearchClearButton && hasQuery);
    }
}

void OnSearchPlaceholderClicked(MyGUI::Widget*)
{
    FocusSearchEdit(FindSearchEditBox(), "placeholder_click");
    UpdateSearchUiState();
}

void OnSearchClearButtonClicked(MyGUI::Widget*)
{
    MyGUI::EditBox* searchEdit = FindSearchEditBox();
    if (searchEdit == 0)
    {
        return;
    }

    if (InventoryState().g_searchQueryRaw.empty())
    {
        FocusSearchEdit(searchEdit, "clear_button_empty");
        RememberSearchEditSnapshot(searchEdit);
        return;
    }

    SetSearchQueryAndRefresh(searchEdit, "", "clear_button", true);
}

void OnSearchEditFocusChanged(MyGUI::Widget*, MyGUI::Widget*)
{
    ResetPendingSearchEditShortcut();
    UpdateSearchUiState();
}

void OnSearchTextChanged(MyGUI::EditBox* sender)
{
    if (sender == 0)
    {
        return;
    }

    const std::string onlyText = sender->getOnlyText().asUTF8();
    if (g_suppressNextSearchEditChangeEvent && onlyText == InventoryState().g_searchQueryRaw)
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
            InventoryState().g_searchQueryRaw = restoredText;
            sender->setOnlyText(restoredText);
            return;
        }

        g_pendingSlashFocusTextSuppression = false;
    }

    InventoryState().g_searchQueryRaw = onlyText;

    std::stringstream line;
    line << "inventory search input changed"
         << " raw=\"" << InventoryState().g_searchQueryRaw << "\""
         << " raw_len=" << InventoryState().g_searchQueryRaw.size();
    LogSearchDebugLine(line.str());

    UpdateSearchUiState();
    RememberSearchEditSnapshot(sender);
}

void DestroyControlsIfPresent(bool clearQuery)
{
    g_searchContainerDragging = false;
    g_prevSearchSlashHotkeyDown = false;
    g_prevSearchCtrlFHotkeyDown = false;
    g_pendingSlashFocusTextSuppression = false;
    g_suppressNextSearchEditChangeEvent = false;
    g_pendingSlashFocusBaseQuery.clear();
    ResetPendingSearchEditShortcut();
    ResetSearchEditSnapshot();

    MyGUI::Widget* controlsContainer = FindControlsContainer();
    const bool restoreFilteredEntries =
        ControlsParentIsSafelyVisible(controlsContainer);
    ClearInventorySearchFilterRefreshState();
    if (restoreFilteredEntries)
    {
        ClearInventorySearchFilterState();
    }
    else
    {
        ClearInventorySearchFilterStateWithoutRestoringEntries();
    }

    if (controlsContainer != 0)
    {
        DestroyWidgetDirect(controlsContainer);
    }

    if (clearQuery)
    {
        InventoryState().g_searchQueryRaw.clear();
    }
}

bool IsDiagnosticsHotkeyPressedEdge()
{
    if (!ShouldEnableDebugProbes())
    {
        g_prevDiagnosticsHotkeyDown = false;
        return false;
    }

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

bool BuildControlsScaffold(MyGUI::Widget* parent)
{
    if (parent == 0)
    {
        return false;
    }

    const SearchContainerMetrics metrics = BuildSearchContainerMetrics();

    MyGUI::Widget* container = parent->createWidget<MyGUI::Widget>(
        "Kenshi_GenericTextBoxFlatSkin",
        BuildSearchContainerCoord(parent),
        MyGUI::Align::Right | MyGUI::Align::Top,
        kControlsContainerName);
    if (container == 0)
    {
        LogErrorLine("failed to create inventory controls container");
        return false;
    }

    MyGUI::Button* dragHandle = container->createWidget<MyGUI::Button>(
        "Kenshi_Button1",
        MyGUI::IntCoord(
            kPanelOuterPadding,
            metrics.searchRowTop,
            kPanelHandleWidth,
            CurrentSearchInputConfiguredHeight()),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSearchDragHandleName);
    if (dragHandle == 0)
    {
        DestroyWidgetDirect(container);
        LogErrorLine("failed to create inventory search drag handle");
        return false;
    }
    dragHandle->setCaption("::");
    dragHandle->setNeedMouseFocus(true);
    dragHandle->eventMouseButtonPressed += MyGUI::newDelegate(&OnSearchDragHandleMousePressed);
    dragHandle->eventMouseMove += MyGUI::newDelegate(&OnSearchDragHandleMouseMove);
    dragHandle->eventMouseDrag += MyGUI::newDelegate(&OnSearchDragHandleMouseDrag);
    dragHandle->eventMouseButtonReleased += MyGUI::newDelegate(&OnSearchDragHandleMouseReleased);

    if (metrics.reserveCount)
    {
        MyGUI::TextBox* countText = container->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(
                metrics.containerWidth - kPanelOuterPadding - metrics.countWidth,
                metrics.searchRowTop,
                metrics.countWidth,
                CurrentSearchInputConfiguredHeight()),
            MyGUI::Align::Left | MyGUI::Align::Top,
            kSearchCountTextName);
        if (countText == 0)
        {
            DestroyWidgetDirect(container);
            LogErrorLine("failed to create inventory count text");
            return false;
        }
        countText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        countText->setCaption("");
        countText->setVisible(false);
    }

    MyGUI::EditBox* searchEdit = container->createWidget<MyGUI::EditBox>(
        "Kenshi_EditBox",
        MyGUI::IntCoord(
            metrics.searchInputLeft,
            metrics.searchRowTop,
            metrics.searchAreaWidth,
            CurrentSearchInputConfiguredHeight()),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSearchEditName);
    if (searchEdit == 0)
    {
        DestroyWidgetDirect(container);
        LogErrorLine("failed to create inventory search edit box");
        return false;
    }
    searchEdit->setOnlyText(InventoryState().g_searchQueryRaw);
    searchEdit->eventEditTextChange += MyGUI::newDelegate(&OnSearchTextChanged);
    searchEdit->eventKeySetFocus += MyGUI::newDelegate(&OnSearchEditFocusChanged);
    searchEdit->eventKeyLostFocus += MyGUI::newDelegate(&OnSearchEditFocusChanged);
    searchEdit->eventKeyButtonPressed += MyGUI::newDelegate(&OnSearchEditKeyPressed);
    searchEdit->eventKeyButtonReleased += MyGUI::newDelegate(&OnSearchEditKeyReleased);

    MyGUI::TextBox* placeholder = container->createWidget<MyGUI::TextBox>(
        "Kenshi_TextboxStandardText",
        MyGUI::IntCoord(
            metrics.searchInputLeft + 10,
            metrics.searchRowTop + 1,
            metrics.searchAreaWidth - 16,
            CurrentSearchInputConfiguredHeight()),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSearchPlaceholderName);
    if (placeholder == 0)
    {
        DestroyWidgetDirect(container);
        LogErrorLine("failed to create inventory search placeholder");
        return false;
    }
    placeholder->setCaption("Search inventory...");
    placeholder->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
    placeholder->setNeedMouseFocus(true);
    placeholder->eventMouseButtonClick += MyGUI::newDelegate(&OnSearchPlaceholderClicked);

    MyGUI::Button* clearButton = container->createWidget<MyGUI::Button>(
        "Kenshi_Button1",
        MyGUI::IntCoord(
            metrics.searchInputLeft + metrics.searchAreaWidth - metrics.clearButtonWidth,
            metrics.searchRowTop,
            metrics.clearButtonWidth,
            metrics.clearButtonWidth),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSearchClearButtonName);
    if (clearButton == 0)
    {
        DestroyWidgetDirect(container);
        LogErrorLine("failed to create inventory search clear button");
        return false;
    }
    clearButton->setCaption("x");
    clearButton->setNeedMouseFocus(true);
    clearButton->eventMouseButtonClick += MyGUI::newDelegate(&OnSearchClearButtonClicked);

    UpdateSearchUiState();
    RememberSearchEditSnapshot(searchEdit);
    if (InventoryState().g_autoFocusSearchInput)
    {
        FocusSearchEdit(searchEdit, "auto_focus");
        RememberSearchEditSnapshot(searchEdit);
    }
    return true;
}

bool AttachedControlsLayoutNeedsRebuild(MyGUI::Widget* parent)
{
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0 || parent == 0)
    {
        return false;
    }

    const SearchContainerMetrics metrics = BuildSearchContainerMetrics();
    const MyGUI::IntCoord expectedContainerCoord = BuildSearchContainerCoord(parent);
    const MyGUI::IntCoord currentContainerCoord = controlsContainer->getCoord();
    if (currentContainerCoord.width != expectedContainerCoord.width
        || currentContainerCoord.height != expectedContainerCoord.height)
    {
        return true;
    }

    MyGUI::EditBox* searchEdit = FindSearchEditBox();
    if (searchEdit == 0)
    {
        return true;
    }

    const MyGUI::IntCoord searchEditCoord = searchEdit->getCoord();
    if (searchEditCoord.left != metrics.searchInputLeft
        || searchEditCoord.top != metrics.searchRowTop
        || searchEditCoord.width != metrics.searchAreaWidth
        || searchEditCoord.height != CurrentSearchInputConfiguredHeight())
    {
        return true;
    }

    MyGUI::TextBox* countText = FindSearchCountTextBox();
    if (metrics.reserveCount)
    {
        if (countText == 0)
        {
            return true;
        }

        const MyGUI::IntCoord countCoord = countText->getCoord();
        if (countCoord.width != metrics.countWidth
            || countCoord.top != metrics.searchRowTop
            || countCoord.height != CurrentSearchInputConfiguredHeight())
        {
            return true;
        }
    }
    else if (countText != 0)
    {
        return true;
    }

    MyGUI::Button* clearButton = FindSearchClearButton();
    if (clearButton == 0)
    {
        return true;
    }

    const MyGUI::IntCoord clearButtonCoord = clearButton->getCoord();
    if (clearButtonCoord.width != metrics.clearButtonWidth
        || clearButtonCoord.height != metrics.clearButtonWidth
        || clearButtonCoord.top != metrics.searchRowTop)
    {
        return true;
    }

    return false;
}

void RefreshAttachedControlsPositionIfNeeded(MyGUI::Widget* parent)
{
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0 || parent == 0 || CurrentSearchInputPositionCustomized())
    {
        return;
    }

    const MyGUI::IntCoord coord = BuildSearchContainerCoord(parent);
    controlsContainer->setCoord(coord);
}

bool TryInjectControlsToTarget(MyGUI::Widget* parent, const char* reason)
{
    if (parent == 0)
    {
        return false;
    }

    DestroyControlsIfPresent(false);
    if (!BuildControlsScaffold(parent))
    {
        return false;
    }

    std::stringstream line;
    line << "inventory controls scaffold injected"
         << " parent=" << SafeWidgetName(parent)
         << " reason=" << (reason == 0 ? "<unknown>" : reason);
    LogDebugLine(line.str());
    return true;
}
}

void TickInventorySearchUi()
{
    InventorySearchTickPerfScope perfScope;
    InventorySearchTickPerfSample& perfSample = perfScope.Sample();

    TickSearchContainerDrag();

    if (IsDiagnosticsHotkeyPressedEdge())
    {
        DumpOnDemandInventoryDiagnosticsSnapshot(FindControlsContainer());
    }

    perfSample.controlsEnabled = InventoryState().g_controlsEnabled;
    if (!InventoryState().g_controlsEnabled)
    {
        ClearInventorySearchTargetCache();
        DestroyControlsIfPresent(true);
        g_loggedNoVisibleInventoryTarget = false;
        return;
    }

    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer != 0 && !ControlsParentIsSafelyVisible(controlsContainer))
    {
        ClearInventorySearchTargetCache();
        perfSample.hiddenControlsCacheCleared = true;
    }

    InventorySearchTargetResolution targetResolution =
        ResolveInventorySearchTarget(controlsContainer != 0, true);
    MyGUI::Widget* targetAnchor = targetResolution.anchor;
    MyGUI::Widget* targetParent = targetResolution.parent;
    perfSample.visibleTargetMicros = targetResolution.visibleScanMicros;
    perfSample.hoverTargetMicros = targetResolution.hoverScanMicros;
    perfSample.visibleTarget = targetResolution.visibleTarget;
    perfSample.hoverTarget = targetResolution.hoverTarget;
    perfSample.targetCacheHit = targetResolution.cacheHit;
    perfSample.targetCacheValidated = targetResolution.cacheValidated;
    perfSample.targetCacheInvalidated = targetResolution.cacheInvalidated;
    perfSample.targetCacheValidationMicros = targetResolution.cacheValidationMicros;
    perfSample.visibleScanAttempted = targetResolution.visibleScanAttempted;
    perfSample.visibleScanSkipped = targetResolution.visibleScanSkipped;
    perfSample.hoverScanAttempted = targetResolution.hoverScanAttempted;

    if (!targetResolution.hasTarget)
    {
        if (controlsContainer != 0)
        {
            ClearInventorySearchTargetCache();
            DestroyControlsIfPresent(true);
            LogDebugLine("inventory controls scaffold removed after inventory window closed");
        }
        else if (!g_loggedNoVisibleInventoryTarget)
        {
            LogDebugLine("inventory search scaffold waiting for a visible inventory target");
            if (ShouldLogDebug())
            {
                DumpInventoryTargetProbe();
                DumpVisibleInventoryWindowCandidateDiagnostics();
            }
        }

        g_loggedNoVisibleInventoryTarget = true;
        return;
    }

    g_loggedNoVisibleInventoryTarget = false;
    MyGUI::Widget* filterRoot = targetAnchor != 0 ? targetAnchor : targetParent;
    MyGUI::Widget* companionFilterRoot = 0;
    if (TryResolveCompanionBackpackFilterRootForTarget(
            targetAnchor,
            targetParent,
            &companionFilterRoot))
    {
        filterRoot = companionFilterRoot;
    }
    MyGUI::Widget* controlsTargetParent = targetParent;
    MyGUI::Widget* companionControlsParent = 0;
    if (TryResolveCompanionControlsParentForTarget(
            targetAnchor,
            targetParent,
            &companionControlsParent))
    {
        controlsTargetParent = companionControlsParent;
    }

    const bool usesCreatureSearchLayout = ParentLooksLikeCreatureSearchTarget(controlsTargetParent);
    SetSearchContainerUsesCreatureLayout(usesCreatureSearchLayout);
    if (usesCreatureSearchLayout && !InventoryState().g_creatureSearchEnabled)
    {
        if (controlsContainer != 0)
        {
            DestroyControlsIfPresent(false);
        }
        ClearInventorySearchFilterRefreshState();
        ClearInventorySearchFilterState();
        ClearInventorySearchTargetCache();
        return;
    }

    if (controlsContainer == 0)
    {
        if (!TryInjectControlsToTarget(controlsTargetParent, "auto"))
        {
            return;
        }
        return;
    }

    MyGUI::Widget* currentParent = 0;
    if (!TryGetControlsVisibleParent(controlsContainer, currentParent))
    {
        ClearInventorySearchFilterRefreshState();
        ClearInventorySearchFilterStateWithoutRestoringEntries();
        ClearInventorySearchTargetCache();
        return;
    }

    if (currentParent != controlsTargetParent
        && InventoryState().g_followActiveInventory)
    {
        if (!TryInjectControlsToTarget(controlsTargetParent, "follow_active_inventory"))
        {
            return;
        }
        return;
    }

    SetSearchContainerUsesCreatureLayout(ParentLooksLikeCreatureSearchTarget(currentParent));
    if (currentParent == 0)
    {
        ClearInventorySearchFilterRefreshState();
        ClearInventorySearchFilterState();
        return;
    }

    if (AttachedControlsLayoutNeedsRebuild(currentParent))
    {
        if (!TryInjectControlsToTarget(controlsTargetParent, "layout_config_changed"))
        {
            return;
        }
        return;
    }

    DumpInventoryBackpackCandidateDiagnosticsIfChanged(targetParent);
    RefreshAttachedControlsPositionIfNeeded(currentParent);
    UpdateSearchUiState();
    MyGUI::EditBox* searchEdit = FindSearchEditBox();
    if (searchEdit != 0)
    {
        const SearchFocusHotkeyKind hotkeyKind =
            DetectSearchFocusHotkeyPressedEdge(searchEdit);
        if (hotkeyKind != SearchFocusHotkeyKind_None)
        {
            if (hotkeyKind == SearchFocusHotkeyKind_Slash)
            {
                g_pendingSlashFocusBaseQuery = InventoryState().g_searchQueryRaw;
                g_pendingSlashFocusTextSuppression = true;
            }
            else
            {
                g_pendingSlashFocusBaseQuery.clear();
                g_pendingSlashFocusTextSuppression = false;
            }

            FocusSearchEdit(searchEdit, "focus_hotkey");
            UpdateSearchUiState();
        }
    }
    else
    {
        g_prevSearchSlashHotkeyDown = false;
        g_prevSearchCtrlFHotkeyDown = false;
        g_pendingSlashFocusTextSuppression = false;
        g_suppressNextSearchEditChangeEvent = false;
        g_pendingSlashFocusBaseQuery.clear();
    }
    InventorySearchFilterRefreshResult filterRefresh =
        ApplyInventorySearchFilterIfNeeded(filterRoot, false);
    perfSample.filterAttempted = filterRefresh.attempted;
    perfSample.filterApplied = filterRefresh.applied;
    perfSample.filterSkipped = filterRefresh.skipped;
}

void SetInventorySearchCountDisplay(const std::string& caption, bool visible)
{
    MyGUI::TextBox* countText = FindSearchCountTextBox();
    if (countText == 0)
    {
        return;
    }

    countText->setCaption(caption);
    countText->setVisible(visible && !caption.empty());
}
