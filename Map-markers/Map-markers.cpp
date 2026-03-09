#include <Debug.h>

#ifndef BOOST_ALL_NO_LIB
#define BOOST_ALL_NO_LIB
#endif

#ifndef BOOST_ERROR_CODE_HEADER_ONLY
#define BOOST_ERROR_CODE_HEADER_ONLY
#endif

#include <core/Functions.h>
#include <kenshi/InputHandler.h>
#include <kenshi/Kenshi.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/SaveFileSystem.h>
#include <kenshi/SaveManager.h>

#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_ImageBox.h>
#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>

#include <ois/OISKeyboard.h>

#include <Windows.h>

#include <cctype>
#include <sstream>
#include <string>

namespace
{
const char* kPluginName = "Map-markers";
const OIS::KeyCode kProbeSnapshotHotkey = OIS::KC_F7;
const OIS::KeyCode kProbeLiveHotkey = OIS::KC_F8;
const char* kTestMarkerWidgetName = "MapMarkers_TestMarker";
const int kTestMarkerSize = 18;
const int kMinimumMapImageSize = 200;

void (*PlayerInterface_updateUT_orig)(PlayerInterface*) = 0;
void (*InputHandler_keyDownEvent_orig)(InputHandler*, OIS::KeyCode) = 0;

bool g_probeLive = false;
DWORD g_lastVisibleRootsScanTick = 0;
DWORD g_lastHoverLogTick = 0;
std::string g_lastVisibleRootsSignature;
std::string g_lastHoveredSignature;
std::string g_lastSaveIdentity;
std::string g_lastTestMarkerSignature;

bool IsSupportedVersion(KenshiLib::BinaryVersion versionInfo)
{
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    return platform != KenshiLib::BinaryVersion::UNKNOWN
        && (version == "1.0.65" || version == "1.0.68");
}

std::string ToLowerAscii(const std::string& value)
{
    std::string lowered(value);
    for (std::string::size_type index = 0; index < lowered.size(); ++index)
    {
        lowered[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[index])));
    }
    return lowered;
}

bool ContainsAsciiCaseInsensitive(const std::string& haystack, const char* needle)
{
    if (needle == 0 || *needle == '\0')
    {
        return false;
    }

    const std::string needleLower = ToLowerAscii(needle);
    return ToLowerAscii(haystack).find(needleLower) != std::string::npos;
}

int ClampInt(int value, int minimum, int maximum)
{
    if (value < minimum)
    {
        return minimum;
    }

    if (value > maximum)
    {
        return maximum;
    }

    return value;
}

void LogProbeLine(const std::string& message)
{
    std::stringstream line;
    line << kPluginName << " PROBE: " << message;
    DebugLog(line.str().c_str());
}

std::string SafeWidgetName(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "<null>";
    }

    const std::string& name = widget->getName();
    return name.empty() ? "<unnamed>" : name;
}

std::string SafeWidgetType(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "<null>";
    }
    return widget->getTypeName();
}

std::string SafeWindowCaption(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "";
    }

    MyGUI::Window* window = widget->castType<MyGUI::Window>(false);
    return window == 0 ? "" : window->getCaption().asUTF8();
}

std::string BuildWidgetDescriptor(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "<null>";
    }

    const MyGUI::IntCoord coord = widget->getCoord();
    std::stringstream line;
    line << SafeWidgetType(widget)
         << ":" << SafeWidgetName(widget)
         << "@(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";

    const std::string caption = SafeWindowCaption(widget);
    if (!caption.empty())
    {
        line << " caption=\"" << caption << "\"";
    }

    const bool nameHasMap = ContainsAsciiCaseInsensitive(SafeWidgetName(widget), "map");
    const bool typeHasMap = ContainsAsciiCaseInsensitive(SafeWidgetType(widget), "map");
    const bool captionHasMap = ContainsAsciiCaseInsensitive(caption, "map");
    if (nameHasMap || typeHasMap || captionHasMap)
    {
        line << " map_token=true";
    }

    return line.str();
}

std::string BuildWidgetChainForLog(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "<none>";
    }

    std::stringstream line;
    std::size_t depth = 0;
    for (MyGUI::Widget* current = widget; current != 0 && depth < 8; current = current->getParent(), ++depth)
    {
        if (depth != 0)
        {
            line << " <- ";
        }
        line << BuildWidgetDescriptor(current);
    }
    return line.str();
}

bool WidgetNameContains(MyGUI::Widget* widget, const char* token)
{
    return widget != 0 && ContainsAsciiCaseInsensitive(SafeWidgetName(widget), token);
}

bool WidgetChainHasMapIdentity(MyGUI::Widget* widget)
{
    for (MyGUI::Widget* current = widget; current != 0; current = current->getParent())
    {
        if (WidgetNameContains(current, "mapscrollview")
            || WidgetNameContains(current, "maptab")
            || WidgetNameContains(current, "tabsmain")
            || ContainsAsciiCaseInsensitive(SafeWindowCaption(current), "map"))
        {
            return true;
        }
    }

    return false;
}

MyGUI::Widget* FindDescendantByName(MyGUI::Widget* root, const char* name)
{
    if (root == 0 || name == 0 || *name == '\0')
    {
        return 0;
    }

    if (SafeWidgetName(root) == name)
    {
        return root;
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t index = 0; index < childCount; ++index)
    {
        if (MyGUI::Widget* found = FindDescendantByName(root->getChildAt(index), name))
        {
            return found;
        }
    }

    return 0;
}

MyGUI::ImageBox* FindMapImageRecursive(MyGUI::Widget* root)
{
    if (root == 0)
    {
        return 0;
    }

    MyGUI::ImageBox* imageBox = root->castType<MyGUI::ImageBox>(false);
    if (imageBox != 0
        && WidgetNameContains(imageBox, "mapimage")
        && WidgetChainHasMapIdentity(imageBox))
    {
        return imageBox;
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t index = 0; index < childCount; ++index)
    {
        if (MyGUI::ImageBox* found = FindMapImageRecursive(root->getChildAt(index)))
        {
            return found;
        }
    }

    return 0;
}

MyGUI::ImageBox* FindActiveMapImage()
{
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

        if (MyGUI::ImageBox* found = FindMapImageRecursive(root))
        {
            return found;
        }
    }

    return 0;
}

void EnsureTestMarkerAttached()
{
    MyGUI::ImageBox* mapImage = FindActiveMapImage();
    if (mapImage == 0 || !mapImage->getInheritedVisible())
    {
        g_lastTestMarkerSignature.clear();
        return;
    }

    const MyGUI::IntCoord imageCoord = mapImage->getCoord();
    if (imageCoord.width < kMinimumMapImageSize || imageCoord.height < kMinimumMapImageSize)
    {
        return;
    }

    MyGUI::Widget* marker = FindDescendantByName(mapImage, kTestMarkerWidgetName);
    const bool createdMarker = marker == 0;
    if (createdMarker)
    {
        marker = mapImage->createWidgetT(
            "Button",
            "Kenshi_Button1",
            MyGUI::IntCoord(0, 0, kTestMarkerSize, kTestMarkerSize),
            MyGUI::Align::Default,
            kTestMarkerWidgetName);

        marker->setNeedMouseFocus(false);
        marker->setAlpha(0.95f);
        marker->setColour(MyGUI::Colour(1.0f, 0.35f, 0.25f, 1.0f));
    }

    const int maxLeft = imageCoord.width > kTestMarkerSize ? imageCoord.width - kTestMarkerSize : 0;
    const int maxTop = imageCoord.height > kTestMarkerSize ? imageCoord.height - kTestMarkerSize : 0;
    const int markerLeft = ClampInt((imageCoord.width * 37) / 100 - (kTestMarkerSize / 2), 0, maxLeft);
    const int markerTop = ClampInt((imageCoord.height * 41) / 100 - (kTestMarkerSize / 2), 0, maxTop);
    marker->setCoord(markerLeft, markerTop, kTestMarkerSize, kTestMarkerSize);

    std::stringstream signature;
    signature << SafeWidgetName(mapImage)
              << "|" << imageCoord.width << "x" << imageCoord.height
              << "|" << markerLeft << "," << markerTop;
    const std::string signatureString = signature.str();
    if (signatureString != g_lastTestMarkerSignature)
    {
        g_lastTestMarkerSignature = signatureString;

        std::stringstream line;
        line << "test_marker "
             << (createdMarker ? "attached" : "updated")
             << " parent=" << BuildWidgetDescriptor(mapImage)
             << " local_coord=(" << markerLeft << "," << markerTop << "," << kTestMarkerSize << "," << kTestMarkerSize << ")";
        LogProbeLine(line.str());
    }
}

std::string BuildVisibleRootsSignature()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return "<no_gui>";
    }

    std::stringstream signature;
    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        signature << BuildWidgetDescriptor(root)
                  << " child_count=" << root->getChildCount()
                  << "\n";
    }

    return signature.str();
}

void LogVisibleRootsSnapshot(const char* reason)
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        LogProbeLine("visible_roots unavailable: MyGUI::Gui singleton missing");
        return;
    }

    std::size_t rootCount = 0;
    std::stringstream header;
    header << "visible_roots reason=" << (reason == 0 ? "<unknown>" : reason);
    LogProbeLine(header.str());

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        std::stringstream line;
        line << "visible_root[" << rootCount << "] "
             << BuildWidgetDescriptor(root)
             << " child_count=" << root->getChildCount();
        LogProbeLine(line.str());
        ++rootCount;
    }

    if (rootCount == 0)
    {
        LogProbeLine("visible_root scan found no visible MyGUI roots");
    }
}

void LogSaveIdentityIfChanged(bool force)
{
    SaveManager* saveManager = SaveManager::getSingleton();
    SaveFileSystem* saveFileSystem = SaveFileSystem::getSingleton();

    const std::string currentGame = saveManager == 0 ? "" : saveManager->getCurrentGame();
    const std::string activeSave = saveFileSystem == 0 ? "" : saveFileSystem->getActiveSave();

    std::stringstream identity;
    identity << currentGame << "\n" << activeSave;
    const std::string identityString = identity.str();
    if (!force && identityString == g_lastSaveIdentity)
    {
        return;
    }

    g_lastSaveIdentity = identityString;
    if (!force && currentGame.empty() && activeSave.empty())
    {
        return;
    }

    std::stringstream line;
    line << "save_identity current_game=\"" << currentGame
         << "\" active_save=\"" << activeSave << "\"";
    LogProbeLine(line.str());
}

void LogHoveredWidgetState(bool force)
{
    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        if (force)
        {
            LogProbeLine("hover unavailable: MyGUI::InputManager singleton missing");
        }
        return;
    }

    const MyGUI::IntPoint mouse = inputManager->getMousePosition();
    MyGUI::Widget* hovered = inputManager->getMouseFocusWidget();

    std::stringstream signature;
    signature << mouse.left << "," << mouse.top << "\n" << BuildWidgetChainForLog(hovered);
    const std::string signatureString = signature.str();
    if (!force && signatureString == g_lastHoveredSignature)
    {
        return;
    }

    g_lastHoveredSignature = signatureString;

    std::stringstream line;
    line << "hover mouse=(" << mouse.left << "," << mouse.top << ")"
         << " focus_chain=" << BuildWidgetChainForLog(hovered);
    LogProbeLine(line.str());
}

void TriggerManualSnapshot(const char* reason)
{
    std::stringstream line;
    line << "snapshot reason=" << (reason == 0 ? "<unknown>" : reason);
    LogProbeLine(line.str());
    LogSaveIdentityIfChanged(true);
    LogHoveredWidgetState(true);
    LogVisibleRootsSnapshot(reason);
}

bool AreProbeModifiersPressed(const InputHandler* inputHandler)
{
    return inputHandler != 0
        && inputHandler->ctrl
        && inputHandler->alt
        && !inputHandler->shift;
}

void TickUiDiagnostics()
{
    EnsureTestMarkerAttached();
    LogSaveIdentityIfChanged(false);

    const DWORD now = GetTickCount();
    if (now - g_lastVisibleRootsScanTick >= 500)
    {
        g_lastVisibleRootsScanTick = now;
        const std::string signature = BuildVisibleRootsSignature();
        if (signature != g_lastVisibleRootsSignature)
        {
            g_lastVisibleRootsSignature = signature;
            LogVisibleRootsSnapshot("visible_root_change");
        }
    }

    if (g_probeLive && now - g_lastHoverLogTick >= 200)
    {
        g_lastHoverLogTick = now;
        LogHoveredWidgetState(false);
    }
}

void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    if (PlayerInterface_updateUT_orig)
    {
        PlayerInterface_updateUT_orig(thisptr);
    }

    TickUiDiagnostics();
}

void InputHandler_keyDownEvent_hook(InputHandler* thisptr, OIS::KeyCode keyCode)
{
    const bool modifiersPressed = AreProbeModifiersPressed(thisptr);

    if (modifiersPressed && keyCode == kProbeSnapshotHotkey)
    {
        TriggerManualSnapshot("manual_hotkey");
        return;
    }

    if (modifiersPressed && keyCode == kProbeLiveHotkey)
    {
        g_probeLive = !g_probeLive;

        std::stringstream line;
        line << "live_hover_probe=" << (g_probeLive ? "enabled" : "disabled");
        LogProbeLine(line.str());
        if (g_probeLive)
        {
            TriggerManualSnapshot("live_hover_enabled");
        }
        return;
    }

    if (InputHandler_keyDownEvent_orig)
    {
        InputHandler_keyDownEvent_orig(thisptr, keyCode);
    }
}
}

__declspec(dllexport) void startPlugin()
{
    DebugLog("Map-markers: startPlugin()");

    const KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    if (!IsSupportedVersion(versionInfo))
    {
        ErrorLog("Map-markers: unsupported Kenshi version/platform");
        return;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        &PlayerInterface_updateUT_orig))
    {
        ErrorLog("Map-markers: could not hook PlayerInterface::updateUT");
        return;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&InputHandler::keyDownEvent),
        InputHandler_keyDownEvent_hook,
        &InputHandler_keyDownEvent_orig))
    {
        ErrorLog("Map-markers: could not hook InputHandler::keyDownEvent");
        return;
    }

    std::stringstream info;
    info << kPluginName
         << " INFO: diagnostics hooks installed"
         << " snapshot_hotkey=CTRL+ALT+F7"
         << " live_hover_hotkey=CTRL+ALT+F8"
         << " test_marker=auto_attach";
    DebugLog(info.str().c_str());
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
