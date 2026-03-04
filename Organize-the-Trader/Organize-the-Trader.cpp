#include <Debug.h>

#include <core/Functions.h>
#include <kenshi/Globals.h>
#include <kenshi/InputHandler.h>
#include <kenshi/Kenshi.h>
#include <kenshi/PlayerInterface.h>

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_InputManager.h>
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
const char* kSpikeWidgetName = "OTT_TraderWindowSpikeWidget";
const char* kSpikeHotkeyHint = "Ctrl+Shift+F8";

typedef void (*PlayerInterfaceUpdateUTFn)(PlayerInterface*);
PlayerInterfaceUpdateUTFn g_updateUTOrig = 0;

bool g_prevSpikeHotkeyDown = false;
bool g_spikeWasInjected = false;

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

MyGUI::Widget* FindSpikeWidget()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return 0;
    }
    return gui->findWidgetT(kSpikeWidgetName, false);
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

void DestroySpikeWidgetIfPresent()
{
    MyGUI::Widget* spikeWidget = FindSpikeWidget();
    if (spikeWidget == 0)
    {
        g_spikeWasInjected = false;
        return;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui != 0)
    {
        gui->destroyWidget(spikeWidget);
        LogInfoLine("spike widget destroyed");
    }
    g_spikeWasInjected = false;
}

void OnSpikeAnchorCoordChanged(MyGUI::Widget* sender)
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

bool TryInjectSpikeWidgetToHoveredWindow()
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

    DestroySpikeWidgetIfPresent();

    const int spikeWidth = 240;
    const int spikeHeight = 34;
    const int topMargin = 16;
    const int rightMargin = 16;
    int left = parent->getWidth() - spikeWidth - rightMargin;
    if (left < 16)
    {
        left = 16;
    }

    MyGUI::Button* spikeButton = parent->createWidget<MyGUI::Button>(
        "Kenshi_Button1",
        MyGUI::IntCoord(left, topMargin, spikeWidth, spikeHeight),
        MyGUI::Align::Right | MyGUI::Align::Top,
        kSpikeWidgetName);

    if (spikeButton == 0)
    {
        LogErrorLine("failed to create spike widget");
        return false;
    }

    spikeButton->setCaption("OTT Spike: Anchored");
    spikeButton->setEnabled(false);
    spikeButton->setNeedToolTip(true);
    spikeButton->setUserString("ToolTip", "Feasibility spike widget. Should move with parent window.");

    anchor->eventChangeCoord += MyGUI::newDelegate(&OnSpikeAnchorCoordChanged);
    g_spikeWasInjected = true;

    std::stringstream line;
    line << "spike injected. hovered_chain=" << BuildParentChainForLog(hovered)
         << " anchor=" << SafeWidgetName(anchor)
         << " parent=" << SafeWidgetName(parent);
    LogInfoLine(line.str());
    return true;
}

bool IsSpikeHotkeyPressedEdge()
{
    if (key == 0 || key->keyboard == 0)
    {
        g_prevSpikeHotkeyDown = false;
        return false;
    }

    const bool ctrlDown = key->keyboard->isKeyDown(OIS::KC_LCONTROL)
        || key->keyboard->isKeyDown(OIS::KC_RCONTROL);
    const bool shiftDown = key->keyboard->isKeyDown(OIS::KC_LSHIFT)
        || key->keyboard->isKeyDown(OIS::KC_RSHIFT);
    const bool f8Down = key->keyboard->isKeyDown(OIS::KC_F8);

    const bool chordDown = ctrlDown && shiftDown && f8Down;
    const bool pressedEdge = chordDown && !g_prevSpikeHotkeyDown;
    g_prevSpikeHotkeyDown = chordDown;
    return pressedEdge;
}

void TickSpikeFeasibility()
{
    if (IsSpikeHotkeyPressedEdge())
    {
        MyGUI::Widget* existing = FindSpikeWidget();
        if (existing != 0)
        {
            DestroySpikeWidgetIfPresent();
            return;
        }

        if (!TryInjectSpikeWidgetToHoveredWindow())
        {
            LogWarnLine("spike injection failed");
        }
    }

    if (g_spikeWasInjected && FindSpikeWidget() == 0)
    {
        g_spikeWasInjected = false;
        LogInfoLine("spike widget no longer present (window likely closed/destroyed)");
    }
}

void PlayerInterface_updateUT_hook(PlayerInterface* self)
{
    if (g_updateUTOrig != 0)
    {
        g_updateUTOrig(self);
    }

    TickSpikeFeasibility();
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
    info << "feasibility spike active. Hover trader window and press " << kSpikeHotkeyHint
         << " to toggle anchor test widget.";
    LogInfoLine(info.str());
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
