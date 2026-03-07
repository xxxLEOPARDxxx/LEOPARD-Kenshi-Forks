#include "TraderSearchUi.h"

#include "TraderCore.h"
#include "TraderSearchText.h"
#include "TraderWindowDetection.h"

#include <kenshi/Globals.h>
#include <kenshi/InputHandler.h>

#include <mygui/MyGUI_Button.h>
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
const char* kSearchEditName = "OTT_SearchEdit";
const char* kSearchPlaceholderName = "OTT_SearchPlaceholder";
const char* kSearchClearButtonName = "OTT_SearchClearButton";
const char* kSearchDragHandleName = "OTT_SearchDragHandle";
const char* kSearchCountTextName = "OTT_SearchCountText";

#define g_showSearchEntryCount (TraderState().core.g_showSearchEntryCount)
#define g_showSearchQuantityCount (TraderState().core.g_showSearchQuantityCount)
#define g_searchInputConfiguredWidth (TraderState().core.g_searchInputConfiguredWidth)
#define g_searchInputConfiguredHeight (TraderState().core.g_searchInputConfiguredHeight)

#define g_searchQueryRaw (TraderState().search.g_searchQueryRaw)
#define g_searchQueryNormalized (TraderState().search.g_searchQueryNormalized)
#define g_lastSearchVisibleEntryCount (TraderState().search.g_lastSearchVisibleEntryCount)
#define g_lastSearchTotalEntryCount (TraderState().search.g_lastSearchTotalEntryCount)

#define g_prevSearchSlashHotkeyDown (TraderState().searchUi.g_prevSearchSlashHotkeyDown)
#define g_prevSearchCtrlFHotkeyDown (TraderState().searchUi.g_prevSearchCtrlFHotkeyDown)
#define g_focusSearchEditOnNextInjection (TraderState().searchUi.g_focusSearchEditOnNextInjection)
#define g_searchContainerDragging (TraderState().searchUi.g_searchContainerDragging)
#define g_searchContainerPositionCustomized (TraderState().searchUi.g_searchContainerPositionCustomized)
#define g_searchContainerDragLastMouseX (TraderState().searchUi.g_searchContainerDragLastMouseX)
#define g_searchContainerDragLastMouseY (TraderState().searchUi.g_searchContainerDragLastMouseY)
#define g_searchContainerStoredLeft (TraderState().searchUi.g_searchContainerStoredLeft)
#define g_searchContainerStoredTop (TraderState().searchUi.g_searchContainerStoredTop)

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

MyGUI::IntCoord ClampSearchContainerCoord(MyGUI::Widget* parent, const MyGUI::IntCoord& inputCoord)
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
    const MyGUI::IntCoord moved = ClampSearchContainerCoord(
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
    line << "search container drag finalized"
         << " source=" << (source == 0 ? "<unknown>" : source)
         << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";
    LogDebugLine(line.str());
}

void OnSearchDragHandleMousePressed(MyGUI::Widget*, int left, int top, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left)
    {
        return;
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

    const bool hasQuery = !g_searchQueryRaw.empty();
    const bool focused = IsSearchEditFocused(searchEdit);
    const bool noVisibleResults = SearchHasNoVisibleResults();

    if (clearButton != 0)
    {
        const MyGUI::IntCoord clearCoord = clearButton->getCoord();
        const int clearButtonWidth = clearCoord.width;
        const int clearGap = 4;
        const int fullRight = clearCoord.left + clearCoord.width;
        const int maxEditWidth = fullRight - searchEdit->getLeft();
        int desiredEditWidth = maxEditWidth;
        if (hasQuery)
        {
            desiredEditWidth -= clearButtonWidth + clearGap;
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

        clearButton->setVisible(hasQuery);
        clearButton->setEnabled(hasQuery);
        clearButton->setAlpha(focused ? 1.0f : 0.86f);
        clearButton->setColour(noVisibleResults
            ? MyGUI::Colour(1.0f, 0.78f, 0.78f, 1.0f)
            : MyGUI::Colour::White);
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

    const int outerPadding = 8;
    const int rowHeight = g_searchInputConfiguredHeight;
    const int handleWidth = 28;
    const int handleGap = 6;
    const int preferredCountWidth = ResolvePreferredSearchCountTextWidth();
    const int preferredCountGap = preferredCountWidth > 0 ? 6 : 0;
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

    const int containerHeight = rowHeight + (outerPadding * 2);

    const int rightMargin = 16;
    int left = parent->getWidth() - containerWidth - rightMargin;
    if (left < 8)
    {
        left = 8;
    }

    int top = topOverride >= 0 ? topOverride : ResolveControlsTop(parent);
    const int maxTop = parent->getHeight() - containerHeight - 8;
    if (top > maxTop)
    {
        top = maxTop;
    }
    if (top < 8)
    {
        top = 8;
    }

    MyGUI::IntCoord containerCoord(left, top, containerWidth, containerHeight);
    if (g_searchContainerPositionCustomized)
    {
        containerCoord.left = g_searchContainerStoredLeft;
        containerCoord.top = g_searchContainerStoredTop;
    }
    containerCoord = ClampSearchContainerCoord(parent, containerCoord);

    if (ShouldLogDebug())
    {
        const MyGUI::IntCoord parentCoord = parent->getCoord();
        std::stringstream line;
        line << "building controls scaffold"
             << " parent=" << SafeWidgetName(parent)
             << " parent_coord=(" << parentCoord.left << "," << parentCoord.top << ","
             << parentCoord.width << "," << parentCoord.height << ")"
             << " container_coord=(" << containerCoord.left << "," << containerCoord.top << ","
             << containerCoord.width << "," << containerCoord.height << ")"
             << " customized_position=" << (g_searchContainerPositionCustomized ? "true" : "false")
             << " search_only=true";
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
    const int searchInputAvailableWidth = containerWidth - searchInputLeft - outerPadding;
    int countWidth = ResolveSearchCountTextWidth(searchInputAvailableWidth);
    int countGap = countWidth > 0 ? 6 : 0;
    int searchAreaWidth = searchInputAvailableWidth - countWidth - countGap;
    if (searchAreaWidth < 120)
    {
        searchAreaWidth = 120;
    }
    int clearButtonWidth = rowHeight;
    if (clearButtonWidth > 26)
    {
        clearButtonWidth = 26;
    }
    if (clearButtonWidth < 18)
    {
        clearButtonWidth = 18;
    }

    MyGUI::Button* dragHandle = container->createWidget<MyGUI::Button>(
        "Kenshi_Button1",
        MyGUI::IntCoord(outerPadding, outerPadding, handleWidth, rowHeight),
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
            MyGUI::IntCoord(containerWidth - outerPadding - countWidth, outerPadding, countWidth, rowHeight),
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
        MyGUI::IntCoord(searchInputLeft, outerPadding, searchAreaWidth, rowHeight),
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

    MyGUI::TextBox* placeholder = container->createWidget<MyGUI::TextBox>(
        "Kenshi_TextboxStandardText",
        MyGUI::IntCoord(searchInputLeft + 10, outerPadding + 1, searchAreaWidth - 16, rowHeight),
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
            outerPadding,
            clearButtonWidth,
            rowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSearchClearButtonName);
    if (clearButton == 0)
    {
        LogErrorLine("failed to create search clear button");
        DestroyWidgetDirect(container);
        return false;
    }
    clearButton->setCaption("X");
    clearButton->eventMouseButtonClick += MyGUI::newDelegate(callbacks.onSearchClearButtonClicked);

    FocusSearchEditIfRequested(searchEdit, "controls_built");
    UpdateSearchUiState();

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
