#include "TraderSearchUi.h"

#include "TraderCore.h"
#include "TraderDiagnostics.h"
#include "TraderInventoryBinding.h"
#include "TraderSearchInputBehavior.h"
#include "TraderSearchPipeline.h"
#include "TraderSearchText.h"
#include "TraderWindowDetection.h"

#include <kenshi/Globals.h>
#include <kenshi/InputHandler.h>

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_ComboBox.h>
#include <mygui/MyGUI_EditBox.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>
#include <ois/OISKeyboard.h>

#include <Windows.h>

#include <sstream>

namespace
{
const char* kControlsContainerName = "OTT_TraderControlsContainer";
const char* kSortControlsContainerName = "OTT_TraderSortControlsContainer";
const char* kSearchEditName = "OTT_SearchEdit";
const char* kSearchPlaceholderName = "OTT_SearchPlaceholder";
const char* kSearchClearButtonName = "OTT_SearchClearButton";
const char* kSearchDragHandleName = "OTT_SearchDragHandle";
const char* kSearchCountTextName = "OTT_SearchCountText";
const char* kSortDragHandleName = "OTT_SortDragHandle";
const char* kSortModeComboName = "OTT_SortModeCombo";
const char* kSortDirectionButtonName = "OTT_SortDirectionButton";
const int kSearchCountGap = 2;
const int kPanelOuterPadding = 8;
const int kPanelGap = 8;
const int kPanelHandleWidth = 28;
const int kPanelHandleGap = 6;

struct PendingSearchEditShortcut
{
    PendingSearchEditShortcut()
        : active(false)
        , keyValue(0)
    {
    }

    bool active;
    int keyValue;
    TraderSearchInputBehavior::EditResult editResult;
};

PendingSearchEditShortcut g_pendingSearchEditShortcut;
bool g_haveSearchEditSnapshot = false;
TraderSearchInputBehavior::Snapshot g_searchEditSnapshot;

#define g_showSearchEntryCount (TraderState().core.g_showSearchEntryCount)
#define g_showSearchQuantityCount (TraderState().core.g_showSearchQuantityCount)
#define g_showSearchClearButton (TraderState().core.g_showSearchClearButton)
#define g_searchInputConfiguredWidth (TraderState().core.g_searchInputConfiguredWidth)
#define g_searchInputConfiguredHeight (TraderState().core.g_searchInputConfiguredHeight)
#define g_sortPanelConfiguredWidth (TraderState().core.g_sortPanelConfiguredWidth)
#define g_sortPanelConfiguredHeight (TraderState().core.g_sortPanelConfiguredHeight)

#define g_searchQueryRaw (TraderState().search.g_searchQueryRaw)
#define g_searchQueryNormalized (TraderState().search.g_searchQueryNormalized)
#define g_loggedNumericOnlyQueryIgnored (TraderState().search.g_loggedNumericOnlyQueryIgnored)
#define g_sortMode (TraderState().search.g_sortMode)
#define g_sortDirection (TraderState().search.g_sortDirection)
#define g_activeTraderTargetId (TraderState().search.g_activeTraderTargetId)
#define g_lastZeroMatchQueryLogged (TraderState().search.g_lastZeroMatchQueryLogged)
#define g_lastSearchSampleQueryLogged (TraderState().search.g_lastSearchSampleQueryLogged)
#define g_lastSearchVisibleEntryCount (TraderState().search.g_lastSearchVisibleEntryCount)
#define g_lastSearchTotalEntryCount (TraderState().search.g_lastSearchTotalEntryCount)
#define g_sortedEntriesRoot (TraderState().search.g_sortedEntriesRoot)
#define g_entryBaseCoords (TraderState().search.g_entryBaseCoords)

#define g_controlsEnabled (TraderState().core.g_controlsEnabled)

#define g_loggedNoVisibleTraderTarget (TraderState().windowDetection.g_loggedNoVisibleTraderTarget)

#define g_searchFilterDirty (TraderState().search.g_searchFilterDirty)

#define g_prevSearchSlashHotkeyDown (TraderState().searchUi.g_prevSearchSlashHotkeyDown)
#define g_prevToggleHotkeyDown (TraderState().searchUi.g_prevToggleHotkeyDown)
#define g_prevDiagnosticsHotkeyDown (TraderState().searchUi.g_prevDiagnosticsHotkeyDown)
#define g_prevSearchCtrlFHotkeyDown (TraderState().searchUi.g_prevSearchCtrlFHotkeyDown)
#define g_controlsWereInjected (TraderState().searchUi.g_controlsWereInjected)
#define g_suppressNextSearchEditChangeEvent (TraderState().searchUi.g_suppressNextSearchEditChangeEvent)
#define g_pendingSlashFocusBaseQuery (TraderState().search.g_pendingSlashFocusBaseQuery)
#define g_pendingSlashFocusTextSuppression (TraderState().searchUi.g_pendingSlashFocusTextSuppression)
#define g_focusSearchEditOnNextInjection (TraderState().searchUi.g_focusSearchEditOnNextInjection)
#define g_searchContainerDragging (TraderState().searchUi.g_searchContainerDragging)
#define g_searchContainerPositionCustomized (TraderState().searchUi.g_searchContainerPositionCustomized)
#define g_searchContainerDragLastMouseX (TraderState().searchUi.g_searchContainerDragLastMouseX)
#define g_searchContainerDragLastMouseY (TraderState().searchUi.g_searchContainerDragLastMouseY)
#define g_searchContainerDragStartLeft (TraderState().searchUi.g_searchContainerDragStartLeft)
#define g_searchContainerDragStartTop (TraderState().searchUi.g_searchContainerDragStartTop)
#define g_searchContainerStoredLeft (TraderState().searchUi.g_searchContainerStoredLeft)
#define g_searchContainerStoredTop (TraderState().searchUi.g_searchContainerStoredTop)
#define g_sortContainerDragging (TraderState().searchUi.g_sortContainerDragging)
#define g_sortContainerPositionCustomized (TraderState().searchUi.g_sortContainerPositionCustomized)
#define g_sortContainerDragLastMouseX (TraderState().searchUi.g_sortContainerDragLastMouseX)
#define g_sortContainerDragLastMouseY (TraderState().searchUi.g_sortContainerDragLastMouseY)
#define g_sortContainerDragStartLeft (TraderState().searchUi.g_sortContainerDragStartLeft)
#define g_sortContainerDragStartTop (TraderState().searchUi.g_sortContainerDragStartTop)
#define g_sortContainerStoredLeft (TraderState().searchUi.g_sortContainerStoredLeft)
#define g_sortContainerStoredTop (TraderState().searchUi.g_sortContainerStoredTop)

#define g_cachedHoveredWidgetInventory (TraderState().binding.g_cachedHoveredWidgetInventory)
#define g_cachedHoveredWidgetInventorySignature (TraderState().binding.g_cachedHoveredWidgetInventorySignature)

bool IsInterestingSearchEditMyGuiKey(MyGUI::KeyCode keyCode)
{
    const int value = keyCode.getValue();
    return value == MyGUI::KeyCode::LeftControl
        || value == MyGUI::KeyCode::RightControl
        || value == MyGUI::KeyCode::ArrowLeft
        || value == MyGUI::KeyCode::ArrowRight
        || value == MyGUI::KeyCode::Backspace;
}

void ResetPendingSearchEditShortcut()
{
    g_pendingSearchEditShortcut = PendingSearchEditShortcut();
}

void ResetSearchEditSnapshot()
{
    g_haveSearchEditSnapshot = false;
    g_searchEditSnapshot = TraderSearchInputBehavior::Snapshot();
}

TraderSearchInputBehavior::Text ToSearchInputText(const MyGUI::UString& text)
{
    TraderSearchInputBehavior::Text result;
    const std::size_t length = text.size();
    result.reserve(length);
    for (std::size_t i = 0; i < length; ++i)
    {
        result.push_back(static_cast<TraderSearchInputBehavior::Codepoint>(text[i]));
    }
    return result;
}

MyGUI::UString ToMyGuiText(const TraderSearchInputBehavior::Text& text)
{
    MyGUI::UString result;
    const std::size_t length = text.size();
    for (std::size_t i = 0; i < length; ++i)
    {
        result.push_back(static_cast<MyGUI::UString::unicode_char>(text[i]));
    }
    return result;
}

TraderSearchInputBehavior::Selection CaptureSearchEditSelection(
    MyGUI::EditBox* searchEdit,
    std::size_t textLength)
{
    if (searchEdit == 0 || !searchEdit->isTextSelection())
    {
        return TraderSearchInputBehavior::Selection();
    }

    const std::size_t selectionStart = searchEdit->getTextSelectionStart();
    if (selectionStart == MyGUI::ITEM_NONE)
    {
        return TraderSearchInputBehavior::Selection();
    }

    const std::size_t selectionLength = searchEdit->getTextSelectionLength();
    return TraderSearchInputBehavior::NormalizeSelection(
        TraderSearchInputBehavior::Selection(true, selectionStart, selectionLength),
        textLength);
}

TraderSearchInputBehavior::Snapshot BuildSearchInputSnapshot(
    const MyGUI::UString& text,
    std::size_t cursorPosition,
    const TraderSearchInputBehavior::Selection& selection)
{
    return TraderSearchInputBehavior::Snapshot(
        ToSearchInputText(text),
        cursorPosition,
        TraderSearchInputBehavior::NormalizeSelection(selection, text.size()));
}

TraderSearchInputBehavior::Snapshot CaptureSearchEditSnapshot(MyGUI::EditBox* searchEdit)
{
    if (searchEdit == 0)
    {
        return TraderSearchInputBehavior::Snapshot();
    }

    const MyGUI::UString text = searchEdit->getOnlyText();
    const std::size_t textLength = text.size();
    const std::size_t cursorPosition = TraderSearchInputBehavior::ClampCursor(
        searchEdit->getTextCursor(),
        textLength);
    return BuildSearchInputSnapshot(
        text,
        cursorPosition,
        CaptureSearchEditSelection(searchEdit, textLength));
}

TraderSearchInputBehavior::ShortcutKind ClassifySearchEditShortcut(MyGUI::KeyCode keyCode)
{
    const int keyValue = keyCode.getValue();
    if (keyValue == MyGUI::KeyCode::ArrowLeft)
    {
        return TraderSearchInputBehavior::ShortcutKind_CtrlLeft;
    }

    if (keyValue == MyGUI::KeyCode::ArrowRight)
    {
        return TraderSearchInputBehavior::ShortcutKind_CtrlRight;
    }

    if (keyValue == MyGUI::KeyCode::Backspace)
    {
        return TraderSearchInputBehavior::ShortcutKind_CtrlBackspace;
    }

    return TraderSearchInputBehavior::ShortcutKind_None;
}

void ApplySearchEditSelection(
    MyGUI::EditBox* searchEdit,
    std::size_t cursorPosition,
    const TraderSearchInputBehavior::Selection& selection)
{
    if (searchEdit == 0)
    {
        return;
    }

    const std::size_t textLength = searchEdit->getTextLength();
    const std::size_t clampedCursor = TraderSearchInputBehavior::ClampCursor(cursorPosition, textLength);
    const TraderSearchInputBehavior::Selection normalizedSelection =
        TraderSearchInputBehavior::NormalizeSelection(selection, textLength);
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

TraderSearchInputBehavior::Snapshot BuildScheduledSearchShortcutSnapshot(
    MyGUI::EditBox* searchEdit,
    TraderSearchInputBehavior::ShortcutKind shortcut)
{
    TraderSearchInputBehavior::Snapshot snapshot = CaptureSearchEditSnapshot(searchEdit);
    if (shortcut == TraderSearchInputBehavior::ShortcutKind_CtrlBackspace && g_haveSearchEditSnapshot)
    {
        snapshot.text = g_searchEditSnapshot.text;
        snapshot.cursor = TraderSearchInputBehavior::ClampCursor(
            g_searchEditSnapshot.cursor,
            snapshot.text.size());
        snapshot.selection = TraderSearchInputBehavior::NormalizeSelection(
            snapshot.selection,
            snapshot.text.size());
    }

    return snapshot;
}

bool ShouldShowAnySearchCountMetric()
{
    return g_showSearchEntryCount || g_showSearchQuantityCount;
}

bool SearchHasNoVisibleResults()
{
    return !g_searchQueryNormalized.empty()
        && g_lastSearchTotalEntryCount > 0
        && g_lastSearchVisibleEntryCount == 0;
}

int ResolvePreferredSearchCountTextWidth()
{
    if (!ShouldShowAnySearchCountMetric())
    {
        return 0;
    }

    if (g_showSearchEntryCount && g_showSearchQuantityCount)
    {
        return 132;
    }

    return 72;
}

int ResolveSearchCountTextWidth(int availableWidth)
{
    if (!ShouldShowAnySearchCountMetric())
    {
        return 0;
    }

    int desiredWidth = ResolvePreferredSearchCountTextWidth();

    const int minSearchInputWidth = 120;
    const int minCountWidth = 44;
    const int maxAllowedWidth = availableWidth - minSearchInputWidth;
    if (maxAllowedWidth < minCountWidth)
    {
        return 0;
    }
    if (desiredWidth > maxAllowedWidth)
    {
        desiredWidth = maxAllowedWidth;
    }

    return desiredWidth;
}

std::string BuildSearchCountCaption(
    std::size_t visibleEntryCount,
    std::size_t totalEntryCount,
    std::size_t visibleQuantity)
{
    std::stringstream line;
    bool wroteMetric = false;

    if (g_showSearchEntryCount)
    {
        line << visibleEntryCount << " / " << totalEntryCount;
        wroteMetric = true;
    }

    if (g_showSearchQuantityCount)
    {
        if (wroteMetric)
        {
            line << " | ";
        }
        line << "qty " << visibleQuantity;
    }

    return line.str();
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

MyGUI::IntCoord ClampPanelCoord(MyGUI::Widget* parent, const MyGUI::IntCoord& inputCoord)
{
    if (parent == 0)
    {
        return inputCoord;
    }

    int left = inputCoord.left;
    int top = inputCoord.top;
    const int width = inputCoord.width;
    const int height = inputCoord.height;

    if (left < 8)
    {
        left = 8;
    }
    if (top < 8)
    {
        top = 8;
    }

    int maxLeft = parent->getWidth() - width - 8;
    int maxTop = parent->getHeight() - height - 8;
    if (maxLeft < 8)
    {
        maxLeft = 8;
    }
    if (maxTop < 8)
    {
        maxTop = 8;
    }

    if (left > maxLeft)
    {
        left = maxLeft;
    }
    if (top > maxTop)
    {
        top = maxTop;
    }

    return MyGUI::IntCoord(left, top, width, height);
}

MyGUI::Widget* FindSortControlsContainer()
{
    return FindWidgetByName(kSortControlsContainerName);
}

void RememberSearchContainerPosition(MyGUI::Widget* container)
{
    if (container == 0)
    {
        return;
    }

    const MyGUI::IntCoord coord = container->getCoord();
    g_searchContainerStoredLeft = coord.left;
    g_searchContainerStoredTop = coord.top;
    g_searchContainerPositionCustomized = true;
}

void RememberSortContainerPosition(MyGUI::Widget* container)
{
    if (container == 0)
    {
        return;
    }

    const MyGUI::IntCoord coord = container->getCoord();
    g_sortContainerStoredLeft = coord.left;
    g_sortContainerStoredTop = coord.top;
    g_sortContainerPositionCustomized = true;
}

void RememberSearchEditSnapshotValue(
    const std::string& query,
    std::size_t cursorPosition,
    const TraderSearchInputBehavior::Selection& selection)
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

void PersistSearchContainerPosition()
{
    const TraderConfigSnapshot config = CaptureTraderConfigSnapshot();
    SaveTraderConfigSnapshot(config);
}

void PersistSortContainerPosition()
{
    const TraderConfigSnapshot config = CaptureTraderConfigSnapshot();
    SaveTraderConfigSnapshot(config);
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

void MoveSortContainerByDelta(int deltaX, int deltaY)
{
    if (deltaX == 0 && deltaY == 0)
    {
        return;
    }

    MyGUI::Widget* container = FindSortControlsContainer();
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
    RememberSortContainerPosition(container);
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

    line << "search container drag finalized"
         << " source=" << (source == 0 ? "<unknown>" : source)
         << " moved=" << (positionChanged ? "true" : "false")
         << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";
    LogDebugLine(line.str());
}

void FinalizeSortContainerDrag(const char* source)
{
    if (!g_sortContainerDragging)
    {
        return;
    }

    g_sortContainerDragging = false;
    MyGUI::Widget* container = FindSortControlsContainer();
    if (container == 0)
    {
        return;
    }

    RememberSortContainerPosition(container);

    std::stringstream line;
    const MyGUI::IntCoord coord = container->getCoord();
    const bool positionChanged =
        coord.left != g_sortContainerDragStartLeft || coord.top != g_sortContainerDragStartTop;
    if (positionChanged)
    {
        PersistSortContainerPosition();
    }

    line << "sort container drag finalized"
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

void OnSortDragHandleMousePressed(MyGUI::Widget*, int left, int top, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left)
    {
        return;
    }

    MyGUI::Widget* container = FindSortControlsContainer();
    if (container != 0)
    {
        const MyGUI::IntCoord coord = container->getCoord();
        g_sortContainerDragStartLeft = coord.left;
        g_sortContainerDragStartTop = coord.top;
    }
    else
    {
        g_sortContainerDragStartLeft = 0;
        g_sortContainerDragStartTop = 0;
    }

    g_sortContainerDragging = true;
    if (!TryGetCurrentMousePosition(&g_sortContainerDragLastMouseX, &g_sortContainerDragLastMouseY))
    {
        g_sortContainerDragLastMouseX = left;
        g_sortContainerDragLastMouseY = top;
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

void OnSortDragHandleMouseDrag(MyGUI::Widget*, int left, int top, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left || !g_sortContainerDragging)
    {
        return;
    }

    int mouseX = left;
    int mouseY = top;
    TryGetCurrentMousePosition(&mouseX, &mouseY);

    const int deltaX = mouseX - g_sortContainerDragLastMouseX;
    const int deltaY = mouseY - g_sortContainerDragLastMouseY;
    if (deltaX == 0 && deltaY == 0)
    {
        return;
    }

    MoveSortContainerByDelta(deltaX, deltaY);
    g_sortContainerDragLastMouseX = mouseX;
    g_sortContainerDragLastMouseY = mouseY;
}

void OnSearchDragHandleMouseMove(MyGUI::Widget*, int left, int top)
{
    if (!g_searchContainerDragging)
    {
        return;
    }

    OnSearchDragHandleMouseDrag(0, left, top, MyGUI::MouseButton::Left);
}

void OnSortDragHandleMouseMove(MyGUI::Widget*, int left, int top)
{
    if (!g_sortContainerDragging)
    {
        return;
    }

    OnSortDragHandleMouseDrag(0, left, top, MyGUI::MouseButton::Left);
}

void OnSearchDragHandleMouseReleased(MyGUI::Widget*, int, int, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left)
    {
        return;
    }

    FinalizeSearchContainerDrag("drag_release");
}

void OnSortDragHandleMouseReleased(MyGUI::Widget*, int, int, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left)
    {
        return;
    }

    FinalizeSortContainerDrag("drag_release");
}

int RelativeBottomInParent(MyGUI::Widget* parent, MyGUI::Widget* child)
{
    if (parent == 0 || child == 0)
    {
        return 0;
    }

    const MyGUI::IntCoord parentAbsolute = parent->getAbsoluteCoord();
    const MyGUI::IntCoord childAbsolute = child->getAbsoluteCoord();
    return (childAbsolute.top - parentAbsolute.top) + childAbsolute.height;
}

int ResolveControlsTop(MyGUI::Widget* parent)
{
    const int defaultTop = 16;
    if (parent == 0)
    {
        return defaultTop;
    }

    const char* moneyWidgetPriority[] =
    {
        "MoneyAmountTextBox",
        "MoneyAmountText",
        "TotalMoneyBuyer",
        "lbTotalMoney",
        "MoneyLabelText",
        "lbBuyersMoney",
        "datapanel"
    };

    for (std::size_t index = 0; index < sizeof(moneyWidgetPriority) / sizeof(moneyWidgetPriority[0]); ++index)
    {
        MyGUI::Widget* moneyWidget = FindWidgetInParentByToken(parent, moneyWidgetPriority[index]);
        if (moneyWidget == 0)
        {
            continue;
        }

        int top = RelativeBottomInParent(parent, moneyWidget) + 8;
        if (top < defaultTop)
        {
            top = defaultTop;
        }

        std::stringstream line;
        line << "controls top resolved from widget=" << moneyWidgetPriority[index]
             << " top=" << top;
        LogDebugLine(line.str());
        return top;
    }

    LogDebugLine("controls top fallback to default (no money widget found)");
    return defaultTop;
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
}

void OnSearchEditKeyPressed(MyGUI::Widget* sender, MyGUI::KeyCode keyCode, MyGUI::Char character);
void OnSearchEditKeyReleased(MyGUI::Widget* sender, MyGUI::KeyCode keyCode);
void OnSortModeComboAccepted(MyGUI::ComboBox* sender, std::size_t index);
void OnSortDirectionButtonClicked(MyGUI::Widget* sender);
void EnsureControlsInjectedIfEnabled();

MyGUI::Widget* FindControlsContainer()
{
    return FindWidgetByName(kControlsContainerName);
}

MyGUI::EditBox* FindSearchEditBox()
{
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0)
    {
        return 0;
    }

    MyGUI::Widget* found = FindNamedDescendantRecursive(controlsContainer, kSearchEditName, false);
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

    MyGUI::Widget* found = FindNamedDescendantRecursive(controlsContainer, kSearchCountTextName, false);
    return found == 0 ? 0 : found->castType<MyGUI::TextBox>(false);
}

MyGUI::ComboBox* FindSortModeComboBox()
{
    MyGUI::Widget* sortContainer = FindSortControlsContainer();
    if (sortContainer == 0)
    {
        return 0;
    }

    MyGUI::Widget* found = FindNamedDescendantRecursive(sortContainer, kSortModeComboName, false);
    return found == 0 ? 0 : found->castType<MyGUI::ComboBox>(false);
}

MyGUI::Button* FindSortDirectionButton()
{
    MyGUI::Widget* sortContainer = FindSortControlsContainer();
    if (sortContainer == 0)
    {
        return 0;
    }

    MyGUI::Widget* found =
        FindNamedDescendantRecursive(sortContainer, kSortDirectionButtonName, false);
    return found == 0 ? 0 : found->castType<MyGUI::Button>(false);
}

MyGUI::Widget* ResolveTraderParentFromControlsContainer()
{
    return ::ResolveTraderParentFromControlsContainer(FindControlsContainer());
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
    line << "search edit focused"
         << " reason=" << (reason == 0 ? "<unknown>" : reason);
    LogDebugLine(line.str());
}

void FocusSearchEditIfRequested(MyGUI::EditBox* searchEdit, const char* reason)
{
    if (!g_focusSearchEditOnNextInjection || searchEdit == 0)
    {
        return;
    }

    g_focusSearchEditOnNextInjection = false;
    FocusSearchEdit(searchEdit, reason);
}

bool IsSearchEditFocused(MyGUI::EditBox* searchEdit)
{
    if (searchEdit == 0)
    {
        return false;
    }

    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    if (input == 0)
    {
        return false;
    }

    return input->getKeyFocusWidget() == searchEdit;
}

std::size_t ResolveSortModeComboIndex(TraderSortMode mode)
{
    switch (mode)
    {
    case TraderSortMode_Price:
        return 1;
    default:
        return 0;
    }
}

void UpdateSearchUiState()
{
    MyGUI::EditBox* searchEdit = FindSearchEditBox();
    if (searchEdit == 0)
    {
        return;
    }

    MyGUI::Button* clearButton = FindSearchClearButton();
    MyGUI::TextBox* placeholder = FindSearchPlaceholderTextBox();
    MyGUI::TextBox* countText = FindSearchCountTextBox();
    MyGUI::ComboBox* sortModeCombo = FindSortModeComboBox();
    MyGUI::Button* sortDirectionButton = FindSortDirectionButton();

    const bool hasQuery = !g_searchQueryRaw.empty();
    const bool focused = IsSearchEditFocused(searchEdit);
    const bool noVisibleResults = SearchHasNoVisibleResults();
    const bool sortActive = g_sortMode != TraderSortMode_None;

    if (clearButton != 0)
    {
        MyGUI::Widget* container = searchEdit->getParent();
        const MyGUI::IntCoord clearCoord = clearButton->getCoord();
        const int clearButtonWidth = clearCoord.width;
        const int clearGap = 4;
        const bool showClearButton = g_showSearchClearButton;
        const bool clearButtonHasAction = hasQuery;
        int availableRight = 0;
        if (countText != 0 && ShouldShowAnySearchCountMetric())
        {
            availableRight = countText->getLeft() - kSearchCountGap;
        }
        else if (container != 0)
        {
            availableRight = container->getWidth() - 8;
        }
        else
        {
            availableRight = clearCoord.left + clearCoord.width;
        }

        int clearLeft = availableRight - clearButtonWidth;
        if (clearLeft < searchEdit->getLeft())
        {
            clearLeft = searchEdit->getLeft();
        }

        int desiredEditWidth = availableRight - searchEdit->getLeft();
        if (showClearButton)
        {
            desiredEditWidth = clearLeft - clearGap - searchEdit->getLeft();
        }
        if (desiredEditWidth < 80)
        {
            desiredEditWidth = 80;
        }

        const MyGUI::IntCoord editCoord = searchEdit->getCoord();
        if (editCoord.width != desiredEditWidth)
        {
            searchEdit->setCoord(editCoord.left, editCoord.top, desiredEditWidth, editCoord.height);
        }

        if (clearCoord.left != clearLeft)
        {
            clearButton->setCoord(clearLeft, clearCoord.top, clearCoord.width, clearCoord.height);
        }

        clearButton->setVisible(showClearButton);
        clearButton->setAlpha(
            clearButtonHasAction
                ? (focused ? 1.0f : 0.86f)
                : (focused ? 0.72f : 0.6f));
        clearButton->setColour(
            clearButtonHasAction
                ? (noVisibleResults
                    ? MyGUI::Colour(1.0f, 0.78f, 0.78f, 1.0f)
                    : MyGUI::Colour::White)
                : MyGUI::Colour(0.7f, 0.7f, 0.7f, 1.0f));
    }

    if (placeholder != 0)
    {
        const bool showPlaceholder = !hasQuery && !focused;
        if (clearButton != 0)
        {
            const MyGUI::IntCoord editCoord = searchEdit->getCoord();
            const int placeholderLeft = editCoord.left + 10;
            const int placeholderTop = editCoord.top + 1;
            int placeholderWidth = editCoord.width - 16;
            if (placeholderWidth < 40)
            {
                placeholderWidth = 40;
            }

            const MyGUI::IntCoord placeholderCoord = placeholder->getCoord();
            if (placeholderCoord.left != placeholderLeft
                || placeholderCoord.top != placeholderTop
                || placeholderCoord.width != placeholderWidth
                || placeholderCoord.height != editCoord.height)
            {
                placeholder->setCoord(
                    placeholderLeft,
                    placeholderTop,
                    placeholderWidth,
                    editCoord.height);
            }
        }
        placeholder->setVisible(showPlaceholder);
        placeholder->setTextColour(
            focused
                ? MyGUI::Colour(0.82f, 0.82f, 0.82f, 1.0f)
                : MyGUI::Colour(0.63f, 0.63f, 0.63f, 1.0f));
    }

    searchEdit->setAlpha(focused ? 1.0f : 0.92f);
    searchEdit->setColour(
        noVisibleResults
            ? MyGUI::Colour(1.0f, 0.9f, 0.9f, 1.0f)
            : (focused
                ? MyGUI::Colour::White
                : MyGUI::Colour(0.93f, 0.93f, 0.93f, 1.0f)));

    if (countText != 0)
    {
        countText->setTextColour(
            noVisibleResults
                ? MyGUI::Colour(1.0f, 0.42f, 0.42f, 1.0f)
                : MyGUI::Colour(0.83f, 0.83f, 0.83f, 1.0f));
        countText->setAlpha(focused ? 1.0f : 0.92f);
    }

    if (sortModeCombo != 0)
    {
        const std::size_t desiredIndex = ResolveSortModeComboIndex(g_sortMode);
        if (sortModeCombo->getIndexSelected() != desiredIndex)
        {
            sortModeCombo->setIndexSelected(desiredIndex);
        }
        sortModeCombo->setAlpha(sortActive ? 1.0f : 0.92f);
        sortModeCombo->setColour(
            sortActive
                ? MyGUI::Colour::White
                : MyGUI::Colour(0.88f, 0.88f, 0.88f, 1.0f));
    }

    if (sortDirectionButton != 0)
    {
        sortDirectionButton->setCaption(
            g_sortDirection == TraderSortDirection_Descending ? "v" : "^");
        sortDirectionButton->setAlpha(sortActive ? 1.0f : 0.84f);
        sortDirectionButton->setColour(
            sortActive
                ? MyGUI::Colour::White
                : MyGUI::Colour(0.82f, 0.82f, 0.82f, 1.0f));
    }
}

void UpdateSearchCountText(
    std::size_t visibleEntryCount,
    std::size_t totalEntryCount,
    std::size_t visibleQuantity)
{
    g_lastSearchVisibleEntryCount = visibleEntryCount;
    g_lastSearchTotalEntryCount = totalEntryCount;
    MyGUI::TextBox* countText = FindSearchCountTextBox();
    if (countText == 0)
    {
        UpdateSearchUiState();
        return;
    }

    countText->setVisible(ShouldShowAnySearchCountMetric());
    countText->setCaption(BuildSearchCountCaption(visibleEntryCount, totalEntryCount, visibleQuantity));
    UpdateSearchUiState();
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

void TickSortContainerDrag()
{
    if (!g_sortContainerDragging)
    {
        return;
    }

    int mouseX = 0;
    int mouseY = 0;
    if (TryGetCurrentMousePosition(&mouseX, &mouseY))
    {
        const int deltaX = mouseX - g_sortContainerDragLastMouseX;
        const int deltaY = mouseY - g_sortContainerDragLastMouseY;
        MoveSortContainerByDelta(deltaX, deltaY);
        g_sortContainerDragLastMouseX = mouseX;
        g_sortContainerDragLastMouseY = mouseY;
    }

    if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)
    {
        FinalizeSortContainerDrag("drag_release_poll");
    }
}

bool AreControlsScaffoldPresent()
{
    return FindControlsContainer() != 0 && FindSortControlsContainer() != 0;
}

bool BuildControlsScaffold(
    MyGUI::Widget* parent,
    int topOverride,
    const SearchUiCallbacks& callbacks)
{
    if (parent == 0)
    {
        return false;
    }
    if (callbacks.onSearchTextChanged == 0
        || callbacks.onSearchEditFocusChanged == 0
        || callbacks.onSearchPlaceholderClicked == 0
        || callbacks.onSearchClearButtonClicked == 0)
    {
        LogErrorLine("failed to build controls scaffold: missing search UI callbacks");
        return false;
    }

    const int outerPadding = kPanelOuterPadding;
    const int handleWidth = kPanelHandleWidth;
    const int handleGap = kPanelHandleGap;
    const int searchRowHeight = g_searchInputConfiguredHeight;
    const int sortRowHeight = g_sortPanelConfiguredHeight;
    const int searchRowTop = outerPadding;
    const int preferredCountWidth = ResolvePreferredSearchCountTextWidth();
    const int preferredCountGap = preferredCountWidth > 0 ? kSearchCountGap : 0;
    const int searchInputLeft = outerPadding + handleWidth + handleGap;
    const int desiredContainerWidth =
        searchInputLeft
        + g_searchInputConfiguredWidth
        + outerPadding
        + preferredCountWidth
        + preferredCountGap;
    const int maxContainerWidth =
        parent->getWidth() > 8 ? parent->getWidth() - 8 : parent->getWidth();
    int containerWidth = desiredContainerWidth;
    if (maxContainerWidth > 0 && containerWidth > maxContainerWidth)
    {
        containerWidth = maxContainerWidth;
    }
    if (containerWidth < 240)
    {
        containerWidth = 240;
    }
    if (maxContainerWidth > 0 && containerWidth > maxContainerWidth)
    {
        containerWidth = maxContainerWidth;
    }

    const int searchContainerHeight = searchRowHeight + (outerPadding * 2);
    int sortContainerWidth = g_sortPanelConfiguredWidth;
    if (sortContainerWidth < kSortPanelConfiguredWidthMin)
    {
        sortContainerWidth = kSortPanelConfiguredWidthMin;
    }
    if (maxContainerWidth > 0 && sortContainerWidth > maxContainerWidth)
    {
        sortContainerWidth = maxContainerWidth;
    }
    const int sortContainerHeight = sortRowHeight + (outerPadding * 2);

    const int rightMargin = 16;
    int left = parent->getWidth() - containerWidth - rightMargin;
    if (left < 8)
    {
        left = 8;
    }

    int top = topOverride >= 0 ? topOverride : ResolveControlsTop(parent);
    const int scaffoldHeight =
        searchContainerHeight
        + (g_sortContainerPositionCustomized ? 0 : (kPanelGap + sortContainerHeight));
    const int maxTop = parent->getHeight() - scaffoldHeight - 8;
    if (top > maxTop)
    {
        top = maxTop;
    }
    if (top < 8)
    {
        top = 8;
    }

    MyGUI::IntCoord containerCoord(left, top, containerWidth, searchContainerHeight);
    if (g_searchContainerPositionCustomized)
    {
        containerCoord.left = g_searchContainerStoredLeft;
        containerCoord.top = g_searchContainerStoredTop;
    }
    containerCoord = ClampPanelCoord(parent, containerCoord);

    MyGUI::IntCoord sortContainerCoord(
        containerCoord.left,
        containerCoord.top + containerCoord.height + kPanelGap,
        sortContainerWidth,
        sortContainerHeight);
    if (g_sortContainerPositionCustomized)
    {
        sortContainerCoord.left = g_sortContainerStoredLeft;
        sortContainerCoord.top = g_sortContainerStoredTop;
    }
    sortContainerCoord = ClampPanelCoord(parent, sortContainerCoord);

    if (ShouldLogDebug())
    {
        const MyGUI::IntCoord parentCoord = parent->getCoord();
        std::stringstream line;
        line << "building controls scaffold"
             << " parent=" << SafeWidgetName(parent)
             << " parent_coord=(" << parentCoord.left << "," << parentCoord.top << ","
             << parentCoord.width << "," << parentCoord.height << ")"
             << " search_coord=(" << containerCoord.left << "," << containerCoord.top << ","
             << containerCoord.width << "," << containerCoord.height << ")"
             << " sort_coord=(" << sortContainerCoord.left << "," << sortContainerCoord.top << ","
             << sortContainerCoord.width << "," << sortContainerCoord.height << ")"
             << " search_customized=" << (g_searchContainerPositionCustomized ? "true" : "false")
             << " sort_customized=" << (g_sortContainerPositionCustomized ? "true" : "false");
        LogDebugLine(line.str());
    }

    MyGUI::Widget* container = parent->createWidget<MyGUI::Widget>(
        "Kenshi_GenericTextBoxFlatSkin",
        containerCoord,
        MyGUI::Align::Right | MyGUI::Align::Top,
        kControlsContainerName);
    if (container == 0)
    {
        LogErrorLine("failed to create controls container");
        return false;
    }

    MyGUI::Widget* sortContainer = 0;
    const int searchInputAvailableWidth = containerWidth - searchInputLeft - outerPadding;
    int countWidth = ResolveSearchCountTextWidth(searchInputAvailableWidth);
    int countGap = countWidth > 0 ? kSearchCountGap : 0;
    int searchAreaWidth = searchInputAvailableWidth - countWidth - countGap;
    if (searchAreaWidth < 120)
    {
        searchAreaWidth = 120;
    }
    int clearButtonWidth = searchRowHeight;
    if (clearButtonWidth > 32)
    {
        clearButtonWidth = 32;
    }
    if (clearButtonWidth < 18)
    {
        clearButtonWidth = 18;
    }
    const int clearButtonTop = searchRowTop + ((searchRowHeight - clearButtonWidth) / 2);

    MyGUI::Button* dragHandle = container->createWidget<MyGUI::Button>(
        "Kenshi_Button1",
        MyGUI::IntCoord(outerPadding, outerPadding, handleWidth, searchRowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSearchDragHandleName);
    if (dragHandle == 0)
    {
        LogErrorLine("failed to create search drag handle");
        DestroyWidgetDirect(container);
        return false;
    }
    dragHandle->setCaption("::");
    dragHandle->setNeedMouseFocus(true);
    dragHandle->eventMouseButtonPressed += MyGUI::newDelegate(&OnSearchDragHandleMousePressed);
    dragHandle->eventMouseMove += MyGUI::newDelegate(&OnSearchDragHandleMouseMove);
    dragHandle->eventMouseDrag += MyGUI::newDelegate(&OnSearchDragHandleMouseDrag);
    dragHandle->eventMouseButtonReleased += MyGUI::newDelegate(&OnSearchDragHandleMouseReleased);

    if (countWidth > 0)
    {
        MyGUI::TextBox* countText = container->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(
                containerWidth - outerPadding - countWidth,
                searchRowTop,
                countWidth,
                searchRowHeight),
            MyGUI::Align::Left | MyGUI::Align::Top,
            kSearchCountTextName);
        if (countText == 0)
        {
            LogErrorLine("failed to create search count text");
            DestroyWidgetDirect(container);
            return false;
        }
        countText->setTextAlign(MyGUI::Align::Right | MyGUI::Align::VCenter);
        UpdateSearchCountText(0, 0, 0);
    }

    MyGUI::EditBox* searchEdit = container->createWidget<MyGUI::EditBox>(
        "Kenshi_EditBox",
        MyGUI::IntCoord(searchInputLeft, searchRowTop, searchAreaWidth, searchRowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSearchEditName);
    if (searchEdit == 0)
    {
        LogErrorLine("failed to create search edit box");
        DestroyWidgetDirect(container);
        return false;
    }
    searchEdit->setOnlyText(g_searchQueryRaw);
    searchEdit->eventEditTextChange += MyGUI::newDelegate(callbacks.onSearchTextChanged);
    searchEdit->eventKeySetFocus += MyGUI::newDelegate(callbacks.onSearchEditFocusChanged);
    searchEdit->eventKeyLostFocus += MyGUI::newDelegate(callbacks.onSearchEditFocusChanged);
    searchEdit->eventKeyButtonPressed += MyGUI::newDelegate(&OnSearchEditKeyPressed);
    searchEdit->eventKeyButtonReleased += MyGUI::newDelegate(&OnSearchEditKeyReleased);

    MyGUI::TextBox* placeholder = container->createWidget<MyGUI::TextBox>(
        "Kenshi_TextboxStandardText",
        MyGUI::IntCoord(
            searchInputLeft + 10,
            searchRowTop + 1,
            searchAreaWidth - 16,
            searchRowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSearchPlaceholderName);
    if (placeholder == 0)
    {
        LogErrorLine("failed to create search placeholder");
        DestroyWidgetDirect(container);
        return false;
    }
    placeholder->setCaption("Search items...");
    placeholder->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
    placeholder->setNeedMouseFocus(true);
    placeholder->eventMouseButtonClick += MyGUI::newDelegate(callbacks.onSearchPlaceholderClicked);

    MyGUI::Button* clearButton = container->createWidget<MyGUI::Button>(
        "Kenshi_Button1",
        MyGUI::IntCoord(
            searchInputLeft + searchAreaWidth - clearButtonWidth,
            clearButtonTop,
            clearButtonWidth,
            clearButtonWidth),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSearchClearButtonName);
    if (clearButton == 0)
    {
        LogErrorLine("failed to create search clear button");
        DestroyWidgetDirect(container);
        return false;
    }
    clearButton->setCaption("x");
    clearButton->eventMouseButtonClick += MyGUI::newDelegate(callbacks.onSearchClearButtonClicked);

    sortContainer = parent->createWidget<MyGUI::Widget>(
        "Kenshi_GenericTextBoxFlatSkin",
        sortContainerCoord,
        MyGUI::Align::Right | MyGUI::Align::Top,
        kSortControlsContainerName);
    if (sortContainer == 0)
    {
        LogErrorLine("failed to create sort controls container");
        DestroyWidgetDirect(container);
        return false;
    }

    MyGUI::Button* sortDragHandle = sortContainer->createWidget<MyGUI::Button>(
        "Kenshi_Button1",
        MyGUI::IntCoord(outerPadding, outerPadding, handleWidth, sortRowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSortDragHandleName);
    if (sortDragHandle == 0)
    {
        LogErrorLine("failed to create sort drag handle");
        DestroyWidgetDirect(sortContainer);
        DestroyWidgetDirect(container);
        return false;
    }
    sortDragHandle->setCaption("::");
    sortDragHandle->setNeedMouseFocus(true);
    sortDragHandle->eventMouseButtonPressed += MyGUI::newDelegate(&OnSortDragHandleMousePressed);
    sortDragHandle->eventMouseMove += MyGUI::newDelegate(&OnSortDragHandleMouseMove);
    sortDragHandle->eventMouseDrag += MyGUI::newDelegate(&OnSortDragHandleMouseDrag);
    sortDragHandle->eventMouseButtonReleased += MyGUI::newDelegate(&OnSortDragHandleMouseReleased);

    const int sortPanelLeft = outerPadding + handleWidth + handleGap;
    const int sortPanelWidth = sortContainerWidth - sortPanelLeft - outerPadding;
    const int sortPanelInnerPadding = 6;
    const int sortLabelWidth = 32;
    const int sortControlGap = 4;
    int sortDirectionButtonWidth = sortRowHeight;
    if (sortDirectionButtonWidth > 32)
    {
        sortDirectionButtonWidth = 32;
    }
    if (sortDirectionButtonWidth < 22)
    {
        sortDirectionButtonWidth = 22;
    }
    int sortComboWidth =
        sortPanelWidth
        - (sortPanelInnerPadding * 2)
        - sortLabelWidth
        - (sortControlGap * 2)
        - sortDirectionButtonWidth;
    if (sortComboWidth < 80)
    {
        sortComboWidth = 80;
    }
    const int sortComboLeft = sortPanelInnerPadding + sortLabelWidth + sortControlGap;
    const int sortDirectionButtonLeft = sortComboLeft + sortComboWidth + sortControlGap;

    MyGUI::TextBox* sortLabel = sortContainer->createWidget<MyGUI::TextBox>(
        "Kenshi_TextboxStandardText",
        MyGUI::IntCoord(sortPanelLeft + sortPanelInnerPadding, outerPadding, sortLabelWidth, sortRowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        "OTT_SortLabel");
    if (sortLabel == 0)
    {
        LogErrorLine("failed to create sort label");
        DestroyWidgetDirect(sortContainer);
        DestroyWidgetDirect(container);
        return false;
    }
    sortLabel->setCaption("Sort");
    sortLabel->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);

    MyGUI::ComboBox* sortModeCombo = sortContainer->createWidget<MyGUI::ComboBox>(
        "Kenshi_ComboBox",
        MyGUI::IntCoord(sortPanelLeft + sortComboLeft, outerPadding, sortComboWidth, sortRowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSortModeComboName);
    if (sortModeCombo == 0)
    {
        LogErrorLine("failed to create sort mode combo");
        DestroyWidgetDirect(sortContainer);
        DestroyWidgetDirect(container);
        return false;
    }
    sortModeCombo->setComboModeDrop(true);
    sortModeCombo->setSmoothShow(false);
    sortModeCombo->addItem("Default");
    sortModeCombo->addItem("Price");
    sortModeCombo->setIndexSelected(ResolveSortModeComboIndex(g_sortMode));
    sortModeCombo->eventComboAccept += MyGUI::newDelegate(&OnSortModeComboAccepted);

    MyGUI::Button* sortDirectionButton = sortContainer->createWidget<MyGUI::Button>(
        "Kenshi_Button1",
        MyGUI::IntCoord(
            sortPanelLeft + sortDirectionButtonLeft,
            outerPadding,
            sortDirectionButtonWidth,
            sortRowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSortDirectionButtonName);
    if (sortDirectionButton == 0)
    {
        LogErrorLine("failed to create sort direction button");
        DestroyWidgetDirect(sortContainer);
        DestroyWidgetDirect(container);
        return false;
    }
    sortDirectionButton->setCaption(
        g_sortDirection == TraderSortDirection_Descending ? "v" : "^");
    sortDirectionButton->eventMouseButtonClick +=
        MyGUI::newDelegate(&OnSortDirectionButtonClicked);

    FocusSearchEditIfRequested(searchEdit, "controls_built");
    UpdateSearchUiState();
    RememberSearchEditSnapshot(searchEdit);

    return true;
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

void ResetSearchQueryForTraderSwitch(const char* reason)
{
    const bool hadQuery = !g_searchQueryRaw.empty() || !g_searchQueryNormalized.empty();

    RestoreSortedInventoryLayoutIfNeeded();

    g_searchQueryRaw.clear();
    g_searchQueryNormalized.clear();
    g_pendingSlashFocusBaseQuery.clear();
    g_pendingSlashFocusTextSuppression = false;
    g_suppressNextSearchEditChangeEvent = false;
    g_loggedNumericOnlyQueryIgnored = false;
    g_lastSearchSampleQueryLogged.clear();
    g_lastZeroMatchQueryLogged.clear();
    g_sortedEntriesRoot = 0;
    g_entryBaseCoords.clear();
    TraderState().search.g_lastSortInvestigationSignature.clear();
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
    RememberSearchEditSnapshot(searchEdit);
}

void ApplySearchShortcutQueryAndCursor(
    MyGUI::EditBox* searchEdit,
    const TraderSearchInputBehavior::EditResult& editResult,
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

    const TraderSearchInputBehavior::ShortcutKind shortcut = ClassifySearchEditShortcut(keyCode);
    if (shortcut == TraderSearchInputBehavior::ShortcutKind_None)
    {
        return false;
    }

    ResetPendingSearchEditShortcut();
    g_pendingSearchEditShortcut.active = true;
    g_pendingSearchEditShortcut.keyValue = keyCode.getValue();
    g_pendingSearchEditShortcut.editResult = TraderSearchInputBehavior::ApplyShortcut(
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
    if (!g_pendingSearchEditShortcut.active || g_pendingSearchEditShortcut.keyValue != keyCode.getValue())
    {
        return;
    }

    const PendingSearchEditShortcut pending = g_pendingSearchEditShortcut;
    ResetPendingSearchEditShortcut();

    if (searchEdit == 0)
    {
        return;
    }

    if (!pending.editResult.handled)
    {
        return;
    }

    if (pending.editResult.rewriteText)
    {
        ApplySearchShortcutQueryAndCursor(
            searchEdit,
            pending.editResult,
            "ctrl_shortcut");
        return;
    }

    ApplySearchEditSelection(searchEdit, pending.editResult.cursor, pending.editResult.selection);
    RememberSearchEditSnapshot(searchEdit);
}

void OnSearchClearButtonClicked(MyGUI::Widget*)
{
    MyGUI::EditBox* searchEdit = FindSearchEditBox();
    if (searchEdit == 0)
    {
        return;
    }

    if (g_searchQueryRaw.empty())
    {
        FocusSearchEdit(searchEdit, "clear_button_empty");
        return;
    }

    SetSearchQueryAndRefresh(searchEdit, "", "clear_button", true);
}

TraderSortMode ResolveSortModeFromComboIndex(std::size_t index)
{
    return index == 1 ? TraderSortMode_Price : TraderSortMode_None;
}

void SetSortStateAndRefresh(
    TraderSortMode requestedMode,
    TraderSortDirection requestedDirection,
    const char* reason)
{
    const TraderSortMode previousMode = g_sortMode;
    const TraderSortDirection previousDirection = g_sortDirection;
    if (previousMode == requestedMode && previousDirection == requestedDirection)
    {
        UpdateSearchUiState();
        return;
    }

    g_sortMode = requestedMode;
    g_sortDirection = requestedDirection;

    std::stringstream line;
    line << "search ui action"
         << " reason=" << (reason == 0 ? "<unknown>" : reason)
         << " sort_mode=" << TraderSortStateLabel(g_sortMode, g_sortDirection);
    LogSearchDebugLine(line.str());

    if (previousMode != TraderSortMode_None || requestedMode != TraderSortMode_None)
    {
        MarkSearchFilterDirty(reason);
        ApplySearchFilterFromControls(false, ShouldLogSearchDebug());
    }
    UpdateSearchUiState();
}

void OnSortModeComboAccepted(MyGUI::ComboBox*, std::size_t index)
{
    const TraderSortMode requestedMode = ResolveSortModeFromComboIndex(index);
    SetSortStateAndRefresh(
        requestedMode,
        g_sortDirection,
        requestedMode == TraderSortMode_None ? "sort_mode_default" : "sort_mode_price");
}

void OnSortDirectionButtonClicked(MyGUI::Widget*)
{
    const TraderSortDirection nextDirection = ToggleTraderSortDirection(g_sortDirection);
    SetSortStateAndRefresh(
        g_sortMode,
        nextDirection,
        nextDirection == TraderSortDirection_Descending
            ? "sort_direction_desc"
            : "sort_direction_asc");
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
    ResetPendingSearchEditShortcut();
    UpdateSearchUiState();
}

void OnSearchEditKeyPressed(MyGUI::Widget* sender, MyGUI::KeyCode keyCode, MyGUI::Char character)
{
    (void)character;

    if (sender == 0 || !IsInterestingSearchEditMyGuiKey(keyCode))
    {
        return;
    }

    ScheduleSearchEditMyGuiShortcut(sender->castType<MyGUI::EditBox>(false), keyCode);
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
    RememberSearchEditSnapshot(sender);
}

void DestroyControlsIfPresent()
{
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    MyGUI::Widget* sortControlsContainer = FindSortControlsContainer();
    if (controlsContainer == 0 && sortControlsContainer == 0)
    {
        RestoreSortedInventoryLayoutIfNeeded();
        ResetPendingSearchEditShortcut();
        ResetSearchEditSnapshot();
        g_searchContainerDragging = false;
        g_sortContainerDragging = false;
        g_searchContainerDragStartLeft = 0;
        g_searchContainerDragStartTop = 0;
        g_sortContainerDragStartLeft = 0;
        g_sortContainerDragStartTop = 0;
        g_controlsWereInjected = false;
        g_searchFilterDirty = false;
        g_sortedEntriesRoot = 0;
        g_entryBaseCoords.clear();
        TraderState().search.g_lastSortInvestigationSignature.clear();
        ResetObservedTraderEntriesState();
        g_pendingSlashFocusBaseQuery.clear();
        g_pendingSlashFocusTextSuppression = false;
        g_suppressNextSearchEditChangeEvent = false;
        g_cachedHoveredWidgetInventory = 0;
        g_cachedHoveredWidgetInventorySignature.clear();
        ClearLockedKeysetSource();
        return;
    }

    if (controlsContainer != 0 && g_searchContainerPositionCustomized)
    {
        const MyGUI::IntCoord coord = controlsContainer->getCoord();
        g_searchContainerStoredLeft = coord.left;
        g_searchContainerStoredTop = coord.top;
    }
    if (sortControlsContainer != 0 && g_sortContainerPositionCustomized)
    {
        const MyGUI::IntCoord coord = sortControlsContainer->getCoord();
        g_sortContainerStoredLeft = coord.left;
        g_sortContainerStoredTop = coord.top;
    }
    g_searchContainerDragging = false;
    g_sortContainerDragging = false;
    g_searchContainerDragStartLeft = 0;
    g_searchContainerDragStartTop = 0;
    g_sortContainerDragStartLeft = 0;
    g_sortContainerDragStartTop = 0;

    RestoreSortedInventoryLayoutIfNeeded();

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui != 0)
    {
        if (sortControlsContainer != 0)
        {
            gui->destroyWidget(sortControlsContainer);
            LogDebugLine("sort controls container destroyed");
        }
        if (controlsContainer != 0)
        {
            gui->destroyWidget(controlsContainer);
            LogDebugLine("controls container destroyed");
        }
    }

    g_controlsWereInjected = false;
    ResetPendingSearchEditShortcut();
    ResetSearchEditSnapshot();
    g_searchFilterDirty = false;
    g_sortedEntriesRoot = 0;
    g_entryBaseCoords.clear();
    TraderState().search.g_lastSortInvestigationSignature.clear();
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

void ApplyRuntimeSearchUiConfig()
{
    DestroyControlsIfPresent();
    if (!g_controlsEnabled)
    {
        return;
    }

    g_loggedNoVisibleTraderTarget = false;
    EnsureControlsInjectedIfEnabled();
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

    if (AreControlsScaffoldPresent())
    {
        return;
    }

    if (FindControlsContainer() != 0 || FindSortControlsContainer() != 0)
    {
        DestroyControlsIfPresent();
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

void TickPhase2ControlsScaffold()
{
    TickSearchContainerDrag();
    TickSortContainerDrag();

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
        if (!AreControlsScaffoldPresent())
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

    if (AreControlsScaffoldPresent())
    {
        ObserveTraderEntriesStateForRefresh();
    }

    if (AreControlsScaffoldPresent() && !handledSearchShortcut && g_searchFilterDirty)
    {
        ApplySearchFilterFromControls(false, ShouldLogSearchDebug());
    }

    if (g_controlsWereInjected && !AreControlsScaffoldPresent())
    {
        RestoreSortedInventoryLayoutIfNeeded();
        g_controlsWereInjected = false;
        g_cachedHoveredWidgetInventory = 0;
        g_cachedHoveredWidgetInventorySignature.clear();
        ClearLockedKeysetSource();
        ClearInventoryGuiInventoryLinks();
        ClearTraderPanelInventoryBindings();
        LogDebugLine("controls container no longer present (window likely closed/destroyed); hover target window and press Ctrl+Shift+F8 to attach again");
    }
}
