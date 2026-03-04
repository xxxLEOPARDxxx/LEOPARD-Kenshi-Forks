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

#include <cctype>
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
bool g_controlsEnabled = true;
bool g_loggedNoVisibleTraderTarget = false;
bool g_loggedRejectedTraderCandidate = false;
std::size_t g_selectedCategoryIndex = 0;
std::size_t g_selectedSortIndex = 0;

const char* kCategoryOptions[] =
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

const char* kSortOptions[] =
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

MyGUI::Widget* FindBestWindowAnchor(MyGUI::Widget* fromWidget);
MyGUI::Widget* ResolveInjectionParent(MyGUI::Widget* anchor);
bool TryResolveHoveredTarget(MyGUI::Widget** outAnchor, MyGUI::Widget** outParent, bool logFailures);

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

bool NameMatchesToken(const std::string& name, const char* token)
{
    if (token == 0 || *token == '\0' || name.empty())
    {
        return false;
    }

    const std::string tokenValue(token);
    if (name == tokenValue)
    {
        return true;
    }

    if (name.size() <= tokenValue.size() + 1)
    {
        return false;
    }

    const std::size_t offset = name.size() - tokenValue.size();
    if (name[offset - 1] != '_')
    {
        return false;
    }

    return name.compare(offset, tokenValue.size(), tokenValue) == 0;
}

std::string UpperAsciiCopy(const std::string& value)
{
    std::string upper = value;
    for (std::size_t index = 0; index < upper.size(); ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(upper[index]);
        upper[index] = static_cast<char>(std::toupper(ch));
    }
    return upper;
}

bool ContainsAsciiCaseInsensitive(const std::string& haystack, const char* needle)
{
    if (needle == 0 || *needle == '\0')
    {
        return false;
    }

    const std::string haystackUpper = UpperAsciiCopy(haystack);
    const std::string needleUpper = UpperAsciiCopy(std::string(needle));
    return haystackUpper.find(needleUpper) != std::string::npos;
}

MyGUI::Widget* FindNamedDescendantByTokenRecursive(MyGUI::Widget* root, const char* token, bool requireVisible)
{
    if (root == 0 || token == 0)
    {
        return 0;
    }

    if ((!requireVisible || root->getInheritedVisible()) && NameMatchesToken(root->getName(), token))
    {
        return root;
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        MyGUI::Widget* child = root->getChildAt(childIndex);
        MyGUI::Widget* found = FindNamedDescendantByTokenRecursive(child, token, requireVisible);
        if (found != 0)
        {
            return found;
        }
    }

    return 0;
}

std::string TruncateForLog(const std::string& value, std::size_t maxLength)
{
    if (value.size() <= maxLength)
    {
        return value;
    }

    return value.substr(0, maxLength) + "...";
}

std::string WidgetTypeForLog(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "null";
    }
    if (widget->castType<MyGUI::Window>(false) != 0)
    {
        return "Window";
    }
    if (widget->castType<MyGUI::Button>(false) != 0)
    {
        return "Button";
    }
    if (widget->castType<MyGUI::EditBox>(false) != 0)
    {
        return "EditBox";
    }
    if (widget->castType<MyGUI::ComboBox>(false) != 0)
    {
        return "ComboBox";
    }
    if (widget->castType<MyGUI::TextBox>(false) != 0)
    {
        return "TextBox";
    }

    return "Widget";
}

std::string WidgetCaptionForLog(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "";
    }

    MyGUI::Button* button = widget->castType<MyGUI::Button>(false);
    if (button != 0)
    {
        return button->getCaption().asUTF8();
    }

    MyGUI::TextBox* textBox = widget->castType<MyGUI::TextBox>(false);
    if (textBox != 0)
    {
        return textBox->getCaption().asUTF8();
    }

    return "";
}

void LogWidgetDiagnosticNode(const char* tag, MyGUI::Widget* widget, std::size_t depth)
{
    if (widget == 0)
    {
        std::stringstream line;
        line << tag << " depth=" << depth << " widget=<null>";
        LogInfoLine(line.str());
        return;
    }

    const MyGUI::IntCoord coord = widget->getCoord();
    std::stringstream line;
    line << tag
         << " depth=" << depth
         << " type=" << WidgetTypeForLog(widget)
         << " name=" << SafeWidgetName(widget)
         << " visible=" << (widget->getInheritedVisible() ? "true" : "false")
         << " children=" << widget->getChildCount()
         << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";

    const std::string caption = TruncateForLog(WidgetCaptionForLog(widget), 48);
    if (!caption.empty())
    {
        line << " caption=\"" << caption << "\"";
    }

    LogInfoLine(line.str());
}

void DumpAncestorDiagnostics(const char* label, MyGUI::Widget* widget)
{
    std::stringstream header;
    header << "diagnostic " << label << " ancestor-chain";
    LogInfoLine(header.str());

    std::size_t depth = 0;
    while (widget != 0 && depth < 12)
    {
        LogWidgetDiagnosticNode(label, widget, depth);
        widget = widget->getParent();
        ++depth;
    }

    if (widget != 0)
    {
        std::stringstream line;
        line << label << " depth=" << depth << " ...";
        LogInfoLine(line.str());
    }
}

void DumpNamedDescendantDiagnosticsRecursive(
    MyGUI::Widget* widget,
    std::size_t depth,
    std::size_t maxDepth,
    std::size_t* loggedCount,
    std::size_t maxEntries)
{
    if (widget == 0 || loggedCount == 0 || *loggedCount >= maxEntries || depth > maxDepth)
    {
        return;
    }

    const bool shouldLog = !widget->getName().empty() || !WidgetCaptionForLog(widget).empty();
    if (shouldLog)
    {
        LogWidgetDiagnosticNode("subtree", widget, depth);
        ++(*loggedCount);
        if (*loggedCount >= maxEntries)
        {
            return;
        }
    }

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        DumpNamedDescendantDiagnosticsRecursive(
            widget->getChildAt(childIndex),
            depth + 1,
            maxDepth,
            loggedCount,
            maxEntries);
        if (*loggedCount >= maxEntries)
        {
            return;
        }
    }
}

void DumpWidgetSubtreeDiagnostics(const char* label, MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return;
    }

    std::stringstream header;
    header << "diagnostic " << label << " subtree-begin";
    LogInfoLine(header.str());
    LogWidgetDiagnosticNode(label, widget, 0);

    std::size_t loggedCount = 0;
    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        DumpNamedDescendantDiagnosticsRecursive(
            widget->getChildAt(childIndex),
            1,
            3,
            &loggedCount,
            36);
        if (loggedCount >= 36)
        {
            break;
        }
    }

    if (loggedCount >= 36)
    {
        std::stringstream line;
        line << "diagnostic " << label << " subtree-truncated=true";
        LogInfoLine(line.str());
    }

    std::stringstream footer;
    footer << "diagnostic " << label << " subtree-end";
    LogInfoLine(footer.str());
}

void DumpHoveredAttachDiagnostics(MyGUI::Widget* hovered, MyGUI::Widget* anchor, MyGUI::Widget* parent)
{
    LogInfoLine("diagnostic hovered-attach begin");
    DumpAncestorDiagnostics("hovered", hovered);

    if (anchor != 0 && anchor != hovered)
    {
        DumpAncestorDiagnostics("anchor", anchor);
    }

    if (parent != 0 && parent != anchor)
    {
        DumpAncestorDiagnostics("parent", parent);
    }

    DumpWidgetSubtreeDiagnostics("anchor", anchor);
    if (parent != 0 && parent != anchor)
    {
        DumpWidgetSubtreeDiagnostics("parent", parent);
    }

    LogInfoLine("diagnostic hovered-attach end");
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

MyGUI::Widget* FindNamedDescendantRecursive(MyGUI::Widget* root, const char* widgetName, bool requireVisible)
{
    if (root == 0 || widgetName == 0)
    {
        return 0;
    }

    if ((!requireVisible || root->getInheritedVisible()) && root->getName() == widgetName)
    {
        return root;
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        MyGUI::Widget* child = root->getChildAt(childIndex);
        MyGUI::Widget* found = FindNamedDescendantRecursive(child, widgetName, requireVisible);
        if (found != 0)
        {
            return found;
        }
    }

    return 0;
}

MyGUI::Widget* FindFirstVisibleWidgetByName(const char* widgetName)
{
    if (widgetName == 0)
    {
        return 0;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return 0;
    }

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        MyGUI::Widget* found = FindNamedDescendantRecursive(root, widgetName, true);
        if (found != 0)
        {
            return found;
        }
    }

    return 0;
}

MyGUI::Widget* FindFirstVisibleWidgetByToken(const char* token)
{
    if (token == 0)
    {
        return 0;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return 0;
    }

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        MyGUI::Widget* found = FindNamedDescendantByTokenRecursive(root, token, true);
        if (found != 0)
        {
            return found;
        }
    }

    return 0;
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

MyGUI::Widget* FindWidgetInParentByToken(MyGUI::Widget* parent, const char* token)
{
    if (parent == 0 || token == 0)
    {
        return 0;
    }
    return FindNamedDescendantByTokenRecursive(parent, token, false);
}

MyGUI::Window* FindOwningWindow(MyGUI::Widget* widget)
{
    while (widget != 0)
    {
        MyGUI::Window* window = widget->castType<MyGUI::Window>(false);
        if (window != 0)
        {
            return window;
        }
        widget = widget->getParent();
    }

    return 0;
}

bool HasTraderInventoryMarkers(MyGUI::Widget* parent)
{
    if (parent == 0)
    {
        return false;
    }

    const bool hasArrangeButton = FindWidgetInParentByToken(parent, "ArrangeButton") != 0;
    const bool hasScrollView = FindWidgetInParentByToken(parent, "scrollview_backpack_content") != 0;
    const bool hasBackpack = FindWidgetInParentByToken(parent, "backpack_content") != 0;

    return hasArrangeButton && hasScrollView && hasBackpack;
}

bool IsLikelyTraderWindow(MyGUI::Widget* parent)
{
    if (!HasTraderInventoryMarkers(parent))
    {
        return false;
    }

    MyGUI::Window* window = FindOwningWindow(parent);
    if (window == 0)
    {
        return false;
    }

    return ContainsAsciiCaseInsensitive(window->getCaption().asUTF8(), "TRADER");
}

bool HasTraderStructure(MyGUI::Widget* parent)
{
    return HasTraderInventoryMarkers(parent);
}

void DumpTraderTargetProbe()
{
    const char* probeTokens[] =
    {
        "scrollview_backpack_content",
        "backpack_content",
        "ArrangeButton",
        "datapanel",
        "TradePanel",
        "CharacterSelectionItemBox",
        "MoneyAmountTextBox",
        "TotalMoneyBuyer"
    };

    for (std::size_t index = 0; index < sizeof(probeTokens) / sizeof(probeTokens[0]); ++index)
    {
        const char* token = probeTokens[index];
        const bool exactAny = FindWidgetByName(token) != 0;
        const bool exactVisible = FindFirstVisibleWidgetByName(token) != 0;
        const bool tokenVisible = FindFirstVisibleWidgetByToken(token) != 0;

        std::stringstream line;
        line << "probe token=" << token
             << " exact_any=" << (exactAny ? "true" : "false")
             << " exact_visible=" << (exactVisible ? "true" : "false")
             << " token_visible=" << (tokenVisible ? "true" : "false");
        LogInfoLine(line.str());
    }
}

void DumpVisibleWindowCandidateDiagnostics()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        LogWarnLine("GUI singleton unavailable while dumping window candidates");
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

        MyGUI::Window* window = root->castType<MyGUI::Window>(false);
        if (window == 0)
        {
            continue;
        }

        MyGUI::Widget* parent = ResolveInjectionParent(root);
        if (parent == 0)
        {
            continue;
        }

        const bool hasMarkers = HasTraderStructure(parent);
        const bool captionHasTrader = ContainsAsciiCaseInsensitive(window->getCaption().asUTF8(), "TRADER");
        if (!hasMarkers && !captionHasTrader)
        {
            continue;
        }

        const bool likelyTrader = IsLikelyTraderWindow(parent);
        const MyGUI::IntCoord coord = root->getCoord();
        std::stringstream line;
        line << "window-candidate[" << index << "]"
             << " name=" << SafeWidgetName(root)
             << " caption=\"" << TruncateForLog(window->getCaption().asUTF8(), 60) << "\""
             << " has_markers=" << (hasMarkers ? "true" : "false")
             << " caption_has_trader=" << (captionHasTrader ? "true" : "false")
             << " likely_trader=" << (likelyTrader ? "true" : "false")
             << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";
        LogInfoLine(line.str());
        ++index;
    }

    if (index == 0)
    {
        LogInfoLine("window-candidate scan found no likely trader windows");
    }
}

bool TryResolveVisibleTraderTarget(MyGUI::Widget** outAnchor, MyGUI::Widget** outParent)
{
    if (outAnchor != 0)
    {
        *outAnchor = 0;
    }
    if (outParent != 0)
    {
        *outParent = 0;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return false;
    }

    MyGUI::Widget* bestAnchor = 0;
    MyGUI::Widget* bestParent = 0;
    int bestArea = -1;

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        MyGUI::Window* window = root->castType<MyGUI::Window>(false);
        if (window == 0)
        {
            continue;
        }

        MyGUI::Widget* parent = ResolveInjectionParent(root);
        if (parent == 0)
        {
            continue;
        }

        if (!IsLikelyTraderWindow(parent))
        {
            continue;
        }

        const MyGUI::IntCoord coord = root->getCoord();
        const int area = coord.width * coord.height;
        if (bestAnchor == 0 || area > bestArea)
        {
            bestAnchor = root;
            bestParent = parent;
            bestArea = area;
        }
    }

    if (bestAnchor == 0 || bestParent == 0)
    {
        return false;
    }

    if (outAnchor != 0)
    {
        *outAnchor = bestAnchor;
    }
    if (outParent != 0)
    {
        *outParent = bestParent;
    }

    g_loggedRejectedTraderCandidate = false;
    MyGUI::Window* window = bestAnchor->castType<MyGUI::Window>(false);
    std::stringstream line;
    line << "resolved trader target via window-scan"
         << " anchor=" << SafeWidgetName(bestAnchor)
         << " parent=" << SafeWidgetName(bestParent)
         << " caption=\"" << (window == 0 ? "" : TruncateForLog(window->getCaption().asUTF8(), 60)) << "\"";
    LogInfoLine(line.str());
    return true;
}

bool TryResolveHoveredTarget(MyGUI::Widget** outAnchor, MyGUI::Widget** outParent, bool logFailures)
{
    if (outAnchor != 0)
    {
        *outAnchor = 0;
    }
    if (outParent != 0)
    {
        *outParent = 0;
    }

    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        if (logFailures)
        {
            LogWarnLine("hover attach failed: MyGUI InputManager unavailable");
        }
        return false;
    }

    MyGUI::Widget* hovered = inputManager->getMouseFocusWidget();
    if (hovered == 0)
    {
        if (logFailures)
        {
            LogWarnLine("hover attach failed: no mouse-focused widget");
        }
        return false;
    }

    MyGUI::Widget* anchor = FindBestWindowAnchor(hovered);
    MyGUI::Widget* parent = ResolveInjectionParent(anchor);
    if (anchor == 0 || parent == 0)
    {
        if (logFailures)
        {
            std::stringstream line;
            line << "hover attach failed: anchor/parent unresolved hovered_chain=" << BuildParentChainForLog(hovered);
            LogWarnLine(line.str());
        }
        return false;
    }

    if (!IsLikelyTraderWindow(parent))
    {
        if (logFailures)
        {
            std::stringstream line;
            line << "hover attach rejected"
                 << " anchor=" << SafeWidgetName(anchor)
                 << " parent=" << SafeWidgetName(parent)
                 << " parent_coord=(" << parent->getCoord().left << "," << parent->getCoord().top << ","
                 << parent->getCoord().width << "," << parent->getCoord().height << ")"
                 << " hovered_chain=" << BuildParentChainForLog(hovered);
            LogWarnLine(line.str());
        }
        return false;
    }

    if (outAnchor != 0)
    {
        *outAnchor = anchor;
    }
    if (outParent != 0)
    {
        *outParent = parent;
    }

    std::stringstream line;
    line << "resolved trader target via hover attach"
         << " anchor=" << SafeWidgetName(anchor)
         << " parent=" << SafeWidgetName(parent)
         << " parent_coord=(" << parent->getCoord().left << "," << parent->getCoord().top << ","
         << parent->getCoord().width << "," << parent->getCoord().height << ")"
         << " hovered_chain=" << BuildParentChainForLog(hovered);
    LogInfoLine(line.str());
    return true;
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
        LogInfoLine(line.str());
        return top;
    }

    LogInfoLine("controls top fallback to default (no money widget found)");
    return defaultTop;
}

void OnSearchTextChanged(MyGUI::EditBox*)
{
    LogInfoLine("search text changed (phase 2 scaffold)");
}

std::size_t NormalizeOptionIndex(std::size_t index, std::size_t optionCount)
{
    if (optionCount == 0)
    {
        return 0;
    }
    return index % optionCount;
}

const char* CategoryOptionAt(std::size_t index)
{
    const std::size_t count = sizeof(kCategoryOptions) / sizeof(kCategoryOptions[0]);
    return kCategoryOptions[NormalizeOptionIndex(index, count)];
}

const char* SortOptionAt(std::size_t index)
{
    const std::size_t count = sizeof(kSortOptions) / sizeof(kSortOptions[0]);
    return kSortOptions[NormalizeOptionIndex(index, count)];
}

void ApplyCategoryButtonCaption(MyGUI::Button* button)
{
    if (button == 0)
    {
        return;
    }

    const std::size_t count = sizeof(kCategoryOptions) / sizeof(kCategoryOptions[0]);
    g_selectedCategoryIndex = NormalizeOptionIndex(g_selectedCategoryIndex, count);
    button->setCaption(CategoryOptionAt(g_selectedCategoryIndex));
}

void ApplySortButtonCaption(MyGUI::Button* button)
{
    if (button == 0)
    {
        return;
    }

    const std::size_t count = sizeof(kSortOptions) / sizeof(kSortOptions[0]);
    g_selectedSortIndex = NormalizeOptionIndex(g_selectedSortIndex, count);
    button->setCaption(SortOptionAt(g_selectedSortIndex));
}

void OnCategoryButtonClicked(MyGUI::Widget* sender)
{
    const std::size_t count = sizeof(kCategoryOptions) / sizeof(kCategoryOptions[0]);
    g_selectedCategoryIndex = NormalizeOptionIndex(g_selectedCategoryIndex + 1, count);

    MyGUI::Button* button = sender == 0 ? 0 : sender->castType<MyGUI::Button>(false);
    if (button != 0)
    {
        ApplyCategoryButtonCaption(button);
    }

    std::stringstream line;
    line << "category changed to index=" << g_selectedCategoryIndex
         << " value=" << CategoryOptionAt(g_selectedCategoryIndex)
         << " (phase 2 scaffold)";
    LogInfoLine(line.str());
}

void OnSortButtonClicked(MyGUI::Widget* sender)
{
    const std::size_t count = sizeof(kSortOptions) / sizeof(kSortOptions[0]);
    g_selectedSortIndex = NormalizeOptionIndex(g_selectedSortIndex + 1, count);

    MyGUI::Button* button = sender == 0 ? 0 : sender->castType<MyGUI::Button>(false);
    if (button != 0)
    {
        ApplySortButtonCaption(button);
    }

    std::stringstream line;
    line << "sort changed to index=" << g_selectedSortIndex
         << " value=" << SortOptionAt(g_selectedSortIndex)
         << " (phase 2 scaffold)";
    LogInfoLine(line.str());
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

    {
        const MyGUI::IntCoord parentCoord = parent->getCoord();
        std::stringstream line;
        line << "building controls scaffold"
             << " parent=" << SafeWidgetName(parent)
             << " parent_coord=(" << parentCoord.left << "," << parentCoord.top << ","
             << parentCoord.width << "," << parentCoord.height << ")"
             << " container_coord=(" << left << "," << top << "," << containerWidth << "," << containerHeight << ")"
             << " compact_layout=" << (compactLayout ? "true" : "false");
        LogInfoLine(line.str());
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

    MyGUI::Button* categoryButton = 0;
    MyGUI::Button* sortButton = 0;

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

        categoryButton = container->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
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

        sortButton = container->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
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

        categoryButton = container->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
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

        sortButton = container->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(outerPadding + comboLabelWidth + 6, row3Y, compactComboWidth, rowHeight),
            MyGUI::Align::Left | MyGUI::Align::Top,
            kSortComboName);
    }

    if (categoryButton == 0 || sortButton == 0)
    {
        LogErrorLine("failed to create category/sort buttons");
        DestroyControlsIfPresent();
        return false;
    }

    ApplyCategoryButtonCaption(categoryButton);
    ApplySortButtonCaption(sortButton);
    categoryButton->eventMouseButtonClick += MyGUI::newDelegate(&OnCategoryButtonClicked);
    sortButton->eventMouseButtonClick += MyGUI::newDelegate(&OnSortButtonClicked);

    return true;
}

bool TryInjectControlsToTarget(MyGUI::Widget* anchor, MyGUI::Widget* parent, const char* sourceTag)
{
    if (anchor == 0 || parent == 0)
    {
        LogErrorLine("could not resolve anchor/parent widget for controls injection");
        return false;
    }

    const bool acceptedTarget = (sourceTag != 0 && std::string(sourceTag) == "hover-direct")
        || IsLikelyTraderWindow(parent);

    if (!acceptedTarget)
    {
        std::stringstream line;
        line << "rejecting injection target reason=not_likely_trader_window"
             << " source=" << (sourceTag == 0 ? "<unknown>" : sourceTag)
             << " anchor=" << SafeWidgetName(anchor)
             << " parent=" << SafeWidgetName(parent)
             << " has_trader_structure=" << (HasTraderStructure(parent) ? "true" : "false");
        LogWarnLine(line.str());
        return false;
    }

    DestroyControlsIfPresent();
    if (!BuildControlsScaffold(parent))
    {
        LogErrorLine("failed to build phase 2 controls scaffold");
        return false;
    }

    g_controlsWereInjected = true;

    std::stringstream line;
    line << "controls scaffold injected"
         << " source=" << (sourceTag == 0 ? "<unknown>" : sourceTag)
         << " anchor=" << SafeWidgetName(anchor)
         << " parent=" << SafeWidgetName(parent);
    LogInfoLine(line.str());
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
    LogInfoLine(line.str());

    DumpHoveredAttachDiagnostics(hovered, anchor, parent);

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
            g_loggedRejectedTraderCandidate = false;

            if (!TryInjectControlsToTarget(anchor, parent, "hover-auto"))
            {
                LogWarnLine("hover auto controls scaffold injection failed");
            }
            return;
        }

        if (!g_loggedNoVisibleTraderTarget)
        {
            LogInfoLine("controls enabled but no visible trader target found yet");
            DumpTraderTargetProbe();
            DumpVisibleWindowCandidateDiagnostics();
            g_loggedNoVisibleTraderTarget = true;
        }
        return;
    }
    g_loggedNoVisibleTraderTarget = false;
    g_loggedRejectedTraderCandidate = false;

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

void TickPhase2ControlsScaffold()
{
    if (IsToggleHotkeyPressedEdge())
    {
        if (g_controlsEnabled)
        {
            g_controlsEnabled = false;
            DestroyControlsIfPresent();
            g_loggedNoVisibleTraderTarget = false;
            g_loggedRejectedTraderCandidate = false;
            LogInfoLine("controls toggled OFF");
            return;
        }

        g_controlsEnabled = true;
        LogInfoLine("controls toggled ON");

        g_loggedNoVisibleTraderTarget = false;
        g_loggedRejectedTraderCandidate = false;

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

    if (g_controlsWereInjected && FindControlsContainer() == 0)
    {
        g_controlsWereInjected = false;
        LogInfoLine("controls container no longer present (window likely closed/destroyed); hover target window and press Ctrl+Shift+F8 to attach again");
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
    info << "phase 2 controls scaffold active."
         << " Auto-attach is enabled for detected trader windows."
         << " Press " << kToggleHotkeyHint << " to hide/show or manual-attach for diagnostics.";
    LogInfoLine(info.str());
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
