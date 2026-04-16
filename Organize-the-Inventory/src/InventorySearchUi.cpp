#include "InventorySearchUi.h"

#include "InventoryConfig.h"
#include "InventoryCore.h"
#include "InventoryDiagnostics.h"
#include "InventorySearchPipeline.h"
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
const int kSearchCountWidth = 72;
const int kPanelHandleWidth = 28;
const int kPanelHandleGap = 6;
const int kRightMargin = 16;
const int kTopMargin = 8;

bool g_loggedNoVisibleInventoryTarget = false;
bool g_prevDiagnosticsHotkeyDown = false;
bool g_searchContainerDragging = false;
int g_searchContainerDragLastMouseX = 0;
int g_searchContainerDragLastMouseY = 0;
int g_searchContainerDragStartLeft = 0;
int g_searchContainerDragStartTop = 0;

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

MyGUI::IntCoord ClampPanelCoord(MyGUI::Widget* parent, const MyGUI::IntCoord& requested)
{
    if (parent == 0)
    {
        return requested;
    }

    MyGUI::IntCoord coord = requested;
    const int maxLeft = parent->getWidth() - coord.width - 8;
    const int maxTop = parent->getHeight() - coord.height - 8;
    if (coord.left > maxLeft)
    {
        coord.left = maxLeft;
    }
    if (coord.top > maxTop)
    {
        coord.top = maxTop;
    }
    if (coord.left < 8)
    {
        coord.left = 8;
    }
    if (coord.top < 8)
    {
        coord.top = 8;
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
    InventoryState().g_searchInputStoredLeft = coord.left;
    InventoryState().g_searchInputStoredTop = coord.top;
    InventoryState().g_searchInputPositionCustomized = true;
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
    const bool reserveCount = InventoryState().g_showSearchEntryCount
        || InventoryState().g_showSearchQuantityCount;
    const int searchCountWidth = reserveCount ? kSearchCountWidth : 0;
    const int searchCountGap = reserveCount ? kSearchCountGap : 0;
    const int width =
        kPanelOuterPadding
        + kPanelHandleWidth
        + kPanelHandleGap
        + InventoryState().g_searchInputConfiguredWidth
        + kPanelOuterPadding
        + searchCountWidth
        + searchCountGap;
    const int height = InventoryState().g_searchInputConfiguredHeight + (kPanelOuterPadding * 2);

    MyGUI::IntCoord coord(
        parent->getWidth() - width - kRightMargin,
        kTopMargin,
        width,
        height);
    if (InventoryState().g_searchInputPositionCustomized)
    {
        coord.left = InventoryState().g_searchInputStoredLeft;
        coord.top = InventoryState().g_searchInputStoredTop;
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

    InventoryState().g_searchQueryRaw.clear();
    searchEdit->setOnlyText("");
    FocusSearchEdit(searchEdit, "clear_button");
    UpdateSearchUiState();
}

void OnSearchEditFocusChanged(MyGUI::Widget*, MyGUI::Widget*)
{
    UpdateSearchUiState();
}

void OnSearchTextChanged(MyGUI::EditBox* sender)
{
    if (sender == 0)
    {
        return;
    }

    InventoryState().g_searchQueryRaw = sender->getOnlyText().asUTF8();

    std::stringstream line;
    line << "inventory search input changed"
         << " raw=\"" << InventoryState().g_searchQueryRaw << "\""
         << " raw_len=" << InventoryState().g_searchQueryRaw.size();
    LogSearchDebugLine(line.str());

    UpdateSearchUiState();
}

void DestroyControlsIfPresent(bool clearQuery)
{
    g_searchContainerDragging = false;

    MyGUI::Widget* controlsContainer = FindControlsContainer();
    ClearInventorySearchFilterState();

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

    const bool reserveCount = InventoryState().g_showSearchEntryCount
        || InventoryState().g_showSearchQuantityCount;
    const int searchCountWidth = reserveCount ? kSearchCountWidth : 0;
    const int searchCountGap = reserveCount ? kSearchCountGap : 0;
    const int containerWidth =
        kPanelOuterPadding
        + kPanelHandleWidth
        + kPanelHandleGap
        + InventoryState().g_searchInputConfiguredWidth
        + kPanelOuterPadding
        + searchCountWidth
        + searchCountGap;
    const int containerHeight =
        InventoryState().g_searchInputConfiguredHeight + (kPanelOuterPadding * 2);
    const int clearButtonWidth =
        InventoryState().g_searchInputConfiguredHeight > 32
            ? 32
            : InventoryState().g_searchInputConfiguredHeight;
    const int searchRowTop = kPanelOuterPadding;
    const int searchInputLeft = kPanelOuterPadding + kPanelHandleWidth + kPanelHandleGap;
    const int searchAreaWidth = InventoryState().g_searchInputConfiguredWidth;

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
            searchRowTop,
            kPanelHandleWidth,
            InventoryState().g_searchInputConfiguredHeight),
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

    if (reserveCount)
    {
        MyGUI::TextBox* countText = container->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(
                containerWidth - kPanelOuterPadding - searchCountWidth,
                searchRowTop,
                searchCountWidth,
                InventoryState().g_searchInputConfiguredHeight),
            MyGUI::Align::Left | MyGUI::Align::Top,
            kSearchCountTextName);
        if (countText == 0)
        {
            DestroyWidgetDirect(container);
            LogErrorLine("failed to create inventory count text");
            return false;
        }
        countText->setTextAlign(MyGUI::Align::Right | MyGUI::Align::VCenter);
        countText->setCaption("");
        countText->setVisible(false);
    }

    MyGUI::EditBox* searchEdit = container->createWidget<MyGUI::EditBox>(
        "Kenshi_EditBox",
        MyGUI::IntCoord(
            searchInputLeft,
            searchRowTop,
            searchAreaWidth,
            InventoryState().g_searchInputConfiguredHeight),
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

    MyGUI::TextBox* placeholder = container->createWidget<MyGUI::TextBox>(
        "Kenshi_TextboxStandardText",
        MyGUI::IntCoord(
            searchInputLeft + 10,
            searchRowTop + 1,
            searchAreaWidth - 16,
            InventoryState().g_searchInputConfiguredHeight),
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
            searchInputLeft + searchAreaWidth - clearButtonWidth,
            searchRowTop,
            clearButtonWidth,
            clearButtonWidth),
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
    if (InventoryState().g_autoFocusSearchInput)
    {
        FocusSearchEdit(searchEdit, "auto_focus");
    }
    return true;
}

void RefreshAttachedControlsPositionIfNeeded(MyGUI::Widget* parent)
{
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0 || parent == 0 || InventoryState().g_searchInputPositionCustomized)
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
    LogInfoLine(line.str());
    return true;
}
}

void TickInventorySearchUi()
{
    TickSearchContainerDrag();

    if (IsDiagnosticsHotkeyPressedEdge())
    {
        DumpOnDemandInventoryDiagnosticsSnapshot(FindControlsContainer());
    }

    if (!InventoryState().g_controlsEnabled)
    {
        DestroyControlsIfPresent(true);
        g_loggedNoVisibleInventoryTarget = false;
        return;
    }

    MyGUI::Widget* targetAnchor = 0;
    MyGUI::Widget* targetParent = 0;
    const bool hasVisibleTarget = TryResolveVisibleInventoryTarget(&targetAnchor, &targetParent);
    MyGUI::Widget* controlsContainer = FindControlsContainer();

    if (!hasVisibleTarget)
    {
        if (TryResolveHoveredInventoryTarget(&targetAnchor, &targetParent, false))
        {
            g_loggedNoVisibleInventoryTarget = false;
            if (!TryInjectControlsToTarget(targetParent, "hover_auto"))
            {
                return;
            }
            controlsContainer = FindControlsContainer();
        }
        else
        {
            if (controlsContainer != 0)
            {
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
    }

    g_loggedNoVisibleInventoryTarget = false;
    MyGUI::Widget* filterRoot = targetAnchor != 0 ? targetAnchor : targetParent;

    if (controlsContainer == 0)
    {
        if (!TryInjectControlsToTarget(targetParent, "auto"))
        {
            return;
        }
        controlsContainer = FindControlsContainer();
    }

    MyGUI::Widget* currentParent = controlsContainer->getParent();
    if (currentParent == 0 || !currentParent->getInheritedVisible())
    {
        if (!TryInjectControlsToTarget(targetParent, "reattach_missing_parent"))
        {
            return;
        }
        controlsContainer = FindControlsContainer();
        currentParent = controlsContainer == 0 ? 0 : controlsContainer->getParent();
    }

    if (currentParent != targetParent
        && InventoryState().g_followActiveInventory)
    {
        if (!TryInjectControlsToTarget(targetParent, "follow_active_inventory"))
        {
            return;
        }
        controlsContainer = FindControlsContainer();
        currentParent = controlsContainer == 0 ? 0 : controlsContainer->getParent();
    }

    if (currentParent == 0)
    {
        ClearInventorySearchFilterState();
        return;
    }

    RefreshAttachedControlsPositionIfNeeded(currentParent);
    UpdateSearchUiState();
    ApplyInventorySearchFilterToParent(filterRoot, false);
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
