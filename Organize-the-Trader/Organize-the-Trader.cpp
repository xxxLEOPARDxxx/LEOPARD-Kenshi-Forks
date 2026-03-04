#include <Debug.h>

#include <core/Functions.h>
#include <kenshi/Globals.h>
#include <kenshi/InputHandler.h>
#include <kenshi/Kenshi.h>
#include <kenshi/PlayerInterface.h>

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

#include <cstddef>
#include <sstream>
#include <string>

namespace
{
const char* kPluginName = "Organize-the-Trader";
const char* kControlsContainerName = "OTT_TraderControlsContainer";
const char* kSearchEditName = "OTT_SearchEdit";
const char* kCategoryComboName = "OTT_CategoryCombo";
const char* kSortComboName = "OTT_SortCombo";
const char* kToggleHotkeyHint = "Ctrl+Shift+F8";

typedef void (*PlayerInterfaceUpdateUTFn)(PlayerInterface*);
PlayerInterfaceUpdateUTFn g_updateUTOrig = 0;

bool g_prevToggleHotkeyDown = false;
bool g_controlsWereInjected = false;
std::size_t g_selectedCategoryIndex = 0;
std::size_t g_selectedSortIndex = 0;

bool IsSupportedVersion(KenshiLib::BinaryVersion versionInfo)
{
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    return platform != KenshiLib::BinaryVersion::UNKNOWN
        && (version == "1.0.65" || version == "1.0.68");
}

void LogInfoLine(const std::string& message)
{
    std::stringstream line;
    line << kPluginName << " INFO: " << message;
    DebugLog(line.str().c_str());
}

void LogWarnLine(const std::string& message)
{
    std::stringstream line;
    line << kPluginName << " WARN: " << message;
    DebugLog(line.str().c_str());
}

void LogErrorLine(const std::string& message)
{
    std::stringstream line;
    line << kPluginName << " ERROR: " << message;
    ErrorLog(line.str().c_str());
}

std::string SafeWidgetName(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "<null>";
    }

    const std::string& name = widget->getName();
    if (name.empty())
    {
        return "<unnamed>";
    }
    return name;
}

std::string BuildParentChainForLog(MyGUI::Widget* widget)
{
    std::stringstream out;
    std::size_t depth = 0;
    while (widget != 0 && depth < 12)
    {
        if (depth > 0)
        {
            out << " <- ";
        }
        out << SafeWidgetName(widget);
        widget = widget->getParent();
        ++depth;
    }

    if (widget != 0)
    {
        out << " <- ...";
    }

    return out.str();
}

MyGUI::Widget* FindWidgetByName(const char* widgetName)
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return 0;
    }
    return gui->findWidgetT(widgetName, false);
}

MyGUI::Widget* FindControlsContainer()
{
    return FindWidgetByName(kControlsContainerName);
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

bool IsDescendantOf(MyGUI::Widget* widget, MyGUI::Widget* ancestor)
{
    if (widget == 0 || ancestor == 0)
    {
        return false;
    }

    MyGUI::Widget* current = widget;
    while (current != 0)
    {
        if (current == ancestor)
        {
            return true;
        }
        current = current->getParent();
    }

    return false;
}

MyGUI::Widget* FindWidgetInParentByName(MyGUI::Widget* parent, const char* widgetName)
{
    if (parent == 0 || widgetName == 0)
    {
        return 0;
    }

    MyGUI::Widget* candidate = FindWidgetByName(widgetName);
    if (candidate == 0)
    {
        return 0;
    }

    if (!IsDescendantOf(candidate, parent))
    {
        return 0;
    }

    return candidate;
}

bool IsLikelyTraderWindow(MyGUI::Widget* parent)
{
    if (parent == 0)
    {
        return false;
    }

    const char* traderMarkers[] =
    {
        "CharacterSelectionItemBox",
        "ConfirmButton",
        "CancelButton"
    };

    for (std::size_t index = 0; index < sizeof(traderMarkers) / sizeof(traderMarkers[0]); ++index)
    {
        if (FindWidgetInParentByName(parent, traderMarkers[index]) != 0)
        {
            return true;
        }
    }

    return false;
}

MyGUI::Widget* FindBestWindowAnchor(MyGUI::Widget* fromWidget)
{
    MyGUI::Widget* current = fromWidget;
    MyGUI::Widget* rootMost = 0;
    MyGUI::Widget* windowAncestor = 0;

    while (current != 0)
    {
        rootMost = current;
        if (current->castType<MyGUI::Window>(false) != 0)
        {
            windowAncestor = current;
        }
        current = current->getParent();
    }

    if (windowAncestor != 0)
    {
        return windowAncestor;
    }
    return rootMost;
}

MyGUI::Widget* ResolveInjectionParent(MyGUI::Widget* anchor)
{
    if (anchor == 0)
    {
        return 0;
    }

    MyGUI::Window* window = anchor->castType<MyGUI::Window>(false);
    if (window != 0)
    {
        MyGUI::Widget* client = window->getClientWidget();
        if (client != 0)
        {
            return client;
        }
    }

    return anchor;
}

void DestroyControlsIfPresent()
{
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0)
    {
        g_controlsWereInjected = false;
        return;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui != 0)
    {
        gui->destroyWidget(controlsContainer);
        LogInfoLine("controls container destroyed");
    }
    g_controlsWereInjected = false;
}

void OnControlsAnchorCoordChanged(MyGUI::Widget* sender)
{
    if (sender == 0)
    {
        return;
    }

    const MyGUI::IntCoord coord = sender->getCoord();
    std::stringstream line;
    line << "anchor moved/resized name=" << SafeWidgetName(sender)
         << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";
    LogInfoLine(line.str());
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
        "lbBuyersMoney"
    };

    for (std::size_t index = 0; index < sizeof(moneyWidgetPriority) / sizeof(moneyWidgetPriority[0]); ++index)
    {
        MyGUI::Widget* moneyWidget = FindWidgetInParentByName(parent, moneyWidgetPriority[index]);
        if (moneyWidget == 0)
        {
            continue;
        }

        int top = RelativeBottomInParent(parent, moneyWidget) + 8;
        if (top < defaultTop)
        {
            top = defaultTop;
        }
        return top;
    }

    return defaultTop;
}

void OnSearchTextChanged(MyGUI::EditBox*)
{
    LogInfoLine("search text changed (phase 2 scaffold)");
}

void OnCategoryChanged(MyGUI::ComboBox*, std::size_t index)
{
    g_selectedCategoryIndex = index;

    std::stringstream line;
    line << "category changed to index=" << index << " (phase 2 scaffold)";
    LogInfoLine(line.str());
}

void OnSortChanged(MyGUI::ComboBox*, std::size_t index)
{
    g_selectedSortIndex = index;

    std::stringstream line;
    line << "sort changed to index=" << index << " (phase 2 scaffold)";
    LogInfoLine(line.str());
}

void PopulateCategoryOptions(MyGUI::ComboBox* categoryCombo)
{
    if (categoryCombo == 0)
    {
        return;
    }

    const char* categoryOptions[] =
    {
        "All",
        "Weapons",
        "Armor",
        "Clothing",
        "Food",
        "Materials",
        "Robotics",
        "Misc"
    };

    for (std::size_t index = 0; index < sizeof(categoryOptions) / sizeof(categoryOptions[0]); ++index)
    {
        categoryCombo->addItem(categoryOptions[index]);
    }
    categoryCombo->setIndexSelected(g_selectedCategoryIndex);
}

void PopulateSortOptions(MyGUI::ComboBox* sortCombo)
{
    if (sortCombo == 0)
    {
        return;
    }

    const char* sortOptions[] =
    {
        "Default",
        "Price Ascending",
        "Price Descending",
        "Name A-Z",
        "Name Z-A",
        "Weight Ascending",
        "Value/kg Descending",
        "Value/kg Ascending"
    };

    for (std::size_t index = 0; index < sizeof(sortOptions) / sizeof(sortOptions[0]); ++index)
    {
        sortCombo->addItem(sortOptions[index]);
    }
    sortCombo->setIndexSelected(g_selectedSortIndex);
}

bool BuildControlsScaffold(MyGUI::Widget* parent)
{
    if (parent == 0)
    {
        return false;
    }

    int containerWidth = parent->getWidth() - 24;
    if (containerWidth > 560)
    {
        containerWidth = 560;
    }
    if (containerWidth < 320)
    {
        containerWidth = parent->getWidth() - 8;
    }
    if (containerWidth < 240)
    {
        containerWidth = 240;
    }

    const bool compactLayout = containerWidth < 430;
    const int containerHeight = compactLayout ? 126 : 92;

    const int rightMargin = 16;
    int left = parent->getWidth() - containerWidth - rightMargin;
    if (left < 8)
    {
        left = 8;
    }

    int top = ResolveControlsTop(parent);
    const int maxTop = parent->getHeight() - containerHeight - 8;
    if (top > maxTop)
    {
        top = maxTop;
    }
    if (top < 8)
    {
        top = 8;
    }

    MyGUI::Widget* container = parent->createWidget<MyGUI::Widget>(
        "Kenshi_GenericTextBoxFlatSkin",
        MyGUI::IntCoord(left, top, containerWidth, containerHeight),
        MyGUI::Align::Right | MyGUI::Align::Top,
        kControlsContainerName);
    if (container == 0)
    {
        LogErrorLine("failed to create controls container");
        return false;
    }

    const int outerPadding = 8;
    const int rowHeight = 26;
    const int rowGap = 8;
    const int labelWidth = 58;
    const int searchInputLeft = outerPadding + labelWidth + 6;
    int searchInputWidth = containerWidth - searchInputLeft - outerPadding;
    if (searchInputWidth < 120)
    {
        searchInputWidth = 120;
    }

    MyGUI::TextBox* searchLabel = container->createWidget<MyGUI::TextBox>(
        "Kenshi_TextboxStandardText",
        MyGUI::IntCoord(outerPadding, outerPadding + 3, labelWidth, rowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top);
    if (searchLabel != 0)
    {
        searchLabel->setCaption("Search:");
    }

    MyGUI::EditBox* searchEdit = container->createWidget<MyGUI::EditBox>(
        "Kenshi_EditBox",
        MyGUI::IntCoord(searchInputLeft, outerPadding, searchInputWidth, rowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSearchEditName);
    if (searchEdit == 0)
    {
        LogErrorLine("failed to create search edit box");
        DestroyControlsIfPresent();
        return false;
    }
    searchEdit->setOnlyText("");
    searchEdit->eventEditTextChange += MyGUI::newDelegate(&OnSearchTextChanged);

    const int row2Y = outerPadding + rowHeight + rowGap;
    const int row3Y = row2Y + rowHeight + rowGap;
    const int comboLabelWidth = 68;

    MyGUI::ComboBox* categoryCombo = 0;
    MyGUI::ComboBox* sortCombo = 0;

    if (!compactLayout)
    {
        const int sortLabelWidth = 40;
        const int gap = 6;
        int availableForCombos = containerWidth - (outerPadding * 2) - comboLabelWidth - sortLabelWidth - (gap * 3);
        if (availableForCombos < 240)
        {
            availableForCombos = 240;
        }
        int categoryComboWidth = availableForCombos / 2;
        int sortComboWidth = availableForCombos - categoryComboWidth;

        if (categoryComboWidth < 120)
        {
            categoryComboWidth = 120;
        }
        if (sortComboWidth < 120)
        {
            sortComboWidth = 120;
        }

        const int categoryLabelX = outerPadding;
        const int categoryComboX = categoryLabelX + comboLabelWidth + gap;
        const int sortLabelX = categoryComboX + categoryComboWidth + gap;
        const int sortComboX = sortLabelX + sortLabelWidth + gap;
        int adjustedSortComboWidth = containerWidth - outerPadding - sortComboX;
        if (adjustedSortComboWidth < 120)
        {
            adjustedSortComboWidth = 120;
        }

        MyGUI::TextBox* categoryLabel = container->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(categoryLabelX, row2Y + 3, comboLabelWidth, rowHeight),
            MyGUI::Align::Left | MyGUI::Align::Top);
        if (categoryLabel != 0)
        {
            categoryLabel->setCaption("Category:");
        }

        categoryCombo = container->createWidget<MyGUI::ComboBox>(
            "Kenshi_ComboBox",
            MyGUI::IntCoord(categoryComboX, row2Y, categoryComboWidth, rowHeight),
            MyGUI::Align::Left | MyGUI::Align::Top,
            kCategoryComboName);

        MyGUI::TextBox* sortLabel = container->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(sortLabelX, row2Y + 3, sortLabelWidth, rowHeight),
            MyGUI::Align::Left | MyGUI::Align::Top);
        if (sortLabel != 0)
        {
            sortLabel->setCaption("Sort:");
        }

        sortCombo = container->createWidget<MyGUI::ComboBox>(
            "Kenshi_ComboBox",
            MyGUI::IntCoord(sortComboX, row2Y, adjustedSortComboWidth, rowHeight),
            MyGUI::Align::Left | MyGUI::Align::Top,
            kSortComboName);
    }
    else
    {
        MyGUI::TextBox* categoryLabel = container->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(outerPadding, row2Y + 3, comboLabelWidth, rowHeight),
            MyGUI::Align::Left | MyGUI::Align::Top);
        if (categoryLabel != 0)
        {
            categoryLabel->setCaption("Category:");
        }

        int compactComboWidth = containerWidth - (outerPadding * 2) - comboLabelWidth - 6;
        if (compactComboWidth < 120)
        {
            compactComboWidth = 120;
        }

        categoryCombo = container->createWidget<MyGUI::ComboBox>(
            "Kenshi_ComboBox",
            MyGUI::IntCoord(outerPadding + comboLabelWidth + 6, row2Y, compactComboWidth, rowHeight),
            MyGUI::Align::Left | MyGUI::Align::Top,
            kCategoryComboName);

        MyGUI::TextBox* sortLabel = container->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(outerPadding, row3Y + 3, comboLabelWidth, rowHeight),
            MyGUI::Align::Left | MyGUI::Align::Top);
        if (sortLabel != 0)
        {
            sortLabel->setCaption("Sort:");
        }

        sortCombo = container->createWidget<MyGUI::ComboBox>(
            "Kenshi_ComboBox",
            MyGUI::IntCoord(outerPadding + comboLabelWidth + 6, row3Y, compactComboWidth, rowHeight),
            MyGUI::Align::Left | MyGUI::Align::Top,
            kSortComboName);
    }

    if (categoryCombo == 0 || sortCombo == 0)
    {
        LogErrorLine("failed to create category/sort combo boxes");
        DestroyControlsIfPresent();
        return false;
    }

    PopulateCategoryOptions(categoryCombo);
    PopulateSortOptions(sortCombo);

    categoryCombo->eventComboChangePosition += MyGUI::newDelegate(&OnCategoryChanged);
    sortCombo->eventComboChangePosition += MyGUI::newDelegate(&OnSortChanged);

    return true;
}

bool TryInjectControlsToHoveredTraderWindow()
{
    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        LogWarnLine("MyGUI InputManager unavailable");
        return false;
    }

    MyGUI::Widget* hovered = inputManager->getMouseFocusWidget();
    if (hovered == 0)
    {
        LogWarnLine("no mouse-focused widget; hover trader window and press hotkey again");
        DumpVisibleRootWidgetsForDiagnostics();
        return false;
    }

    MyGUI::Widget* anchor = FindBestWindowAnchor(hovered);
    MyGUI::Widget* parent = ResolveInjectionParent(anchor);
    if (anchor == 0 || parent == 0)
    {
        LogErrorLine("could not resolve anchor/parent widget for spike injection");
        return false;
    }

    if (!IsLikelyTraderWindow(parent))
    {
        LogWarnLine("hovered window does not look like trader UI; injection skipped");
        return false;
    }

    DestroyControlsIfPresent();
    if (!BuildControlsScaffold(parent))
    {
        LogErrorLine("failed to build phase 2 controls scaffold");
        return false;
    }

    anchor->eventChangeCoord += MyGUI::newDelegate(&OnControlsAnchorCoordChanged);
    g_controlsWereInjected = true;

    std::stringstream line;
    line << "controls scaffold injected. hovered_chain=" << BuildParentChainForLog(hovered)
         << " anchor=" << SafeWidgetName(anchor)
         << " parent=" << SafeWidgetName(parent);
    LogInfoLine(line.str());
    return true;
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

void TickPhase2ControlsScaffold()
{
    if (IsToggleHotkeyPressedEdge())
    {
        MyGUI::Widget* existing = FindControlsContainer();
        if (existing != 0)
        {
            DestroyControlsIfPresent();
            return;
        }

        if (!TryInjectControlsToHoveredTraderWindow())
        {
            LogWarnLine("controls scaffold injection failed");
        }
    }

    if (g_controlsWereInjected && FindControlsContainer() == 0)
    {
        g_controlsWereInjected = false;
        LogInfoLine("controls container no longer present (window likely closed/destroyed)");
    }
}

void PlayerInterface_updateUT_hook(PlayerInterface* self)
{
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

    const KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    if (!IsSupportedVersion(versionInfo))
    {
        LogErrorLine("unsupported Kenshi version/platform");
        return;
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
    info << "phase 2 controls scaffold active. Hover trader window and press " << kToggleHotkeyHint
         << " to toggle search/filter/sort controls.";
    LogInfoLine(info.str());
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
