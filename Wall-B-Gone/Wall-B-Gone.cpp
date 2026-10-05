// ModConfigMenu: подписи настроек идут через Tr - перевод в locale/<язык>.
#define KLOC_DOMAIN "wall_b_gone"
#include <Localization.h>

#include <Debug.h>

#include <core/Functions.h>
#include "emc/mod_hub_client.h"

#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/InputHandler.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/Building/Building.h>
#include <kenshi/Character.h>
#include <kenshi/Faction.h>
#include <kenshi/Inventory.h>
#include <kenshi/GameData.h>
#include <kenshi/Kenshi.h>
#include <kenshi/gui/TitleScreen.h>
#include <kenshi/util/hand.h>
#include <kenshi/util/lektor.h>
#include <ois/OISKeyboard.h>
#include <mygui/MyGUI_TabControl.h>
#include <mygui/MyGUI_TabItem.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_ScrollView.h>
#include <string>
#include <sstream>
#include <cstring>
#include <cctype>
#include <Windows.h>
#include <fstream>
#include <stdint.h>
#include <vector>

// Track last failed dismantle to prevent rapid retries causing game corruption
static DWORD g_lastFailedDismantleTime = 0;
static const DWORD DISMANTLE_COOLDOWN_MS = 1000;

static const OIS::KeyCode kDefaultHotkey = OIS::KC_X;
static const bool kDefaultHotkeyRequireCtrl = false;
static const bool kDefaultHotkeyRequireShift = false;
static const bool kDefaultHotkeyRequireAlt = false;
static const uint32_t kHotkeyModifierCtrlMask = 1u << 0;
static const uint32_t kHotkeyModifierShiftMask = 1u << 1;
static const uint32_t kHotkeyModifierAltMask = 1u << 2;

// --- Hotkey state (edge detect) ---
static OIS::KeyCode g_hotkeyPrimary = kDefaultHotkey;
static OIS::KeyCode g_pendingHotkeyPrimary = kDefaultHotkey;
static bool g_hotkeyRequireCtrl = kDefaultHotkeyRequireCtrl;
static bool g_hotkeyRequireShift = kDefaultHotkeyRequireShift;
static bool g_hotkeyRequireAlt = kDefaultHotkeyRequireAlt;
static bool g_prevHotkeyDown = false;
static bool g_loggedRuntimeHotkeyFallback = false;

enum HotkeyCaptureState
{
    HotkeyCapture_Idle = 0,
    HotkeyCapture_AwaitKey = 1
};

static HotkeyCaptureState g_hotkeyCaptureState = HotkeyCapture_Idle;

enum HotkeyValidationResult
{
    HotkeyValidation_Ok = 0,
    HotkeyValidation_BlockedSystem = 1,
    HotkeyValidation_BlockedReserved = 2,
    HotkeyValidation_Invalid = 3
};

// Runtime toggle persisted to JSON and exposed in Plugins menu.
static bool g_modEnabled = true;
static bool g_sleepingBagDismantleEnabled = true;
// Разбирать любую выделенную постройку игрока, а не только стены и мебель.
static bool g_dismantleAnyOwnBuilding = false;
// К ней: постройку с вещами не пропускать, а сперва выложить вещи на землю.
static bool g_dismantleDropItems = false;
static bool g_debugLogging = false;
static std::string g_settingsPath;
static const char* kWallBGoneTabName = "Wall-B-Gone";
static const char* kWallBGonePanelName = "wall_b_gone_options";
static const int kWallBGonePanelLineId = 0x574247;
static const char* kHubNamespaceId = "emkej.qol";
static const char* kHubNamespaceDisplayName = "Emkej QoL";
static const char* kHubModId = "wall_b_gone";
static const char* kHubModDisplayName = "Wall-B-Gone";
static const char* kHubSettingEnabledId = "enabled";
static const char* kHubSettingSleepingBagEnabledId = "sleeping_bag_dismantle_enabled";
static const char* kHubSettingAnyOwnBuildingId = "dismantle_any_own_building";
static const char* kHubSettingDropItemsId = "dismantle_drop_items";
static const char* kHubSettingHotkeyId = "dismantle_hotkey";
static const char* kHubSettingHotkeyRequireCtrlId = "dismantle_hotkey_require_ctrl";
static const char* kHubSettingHotkeyRequireShiftId = "dismantle_hotkey_require_shift";
static const char* kHubSettingHotkeyRequireAltId = "dismantle_hotkey_require_alt";
static const char* kHubActionResetHotkeyId = "reset_hotkey_default";
static const int32_t kHubAttachFailureModeNone = 0;
static const int32_t kHubAttachFailureModeStartupOnly = 1;
static const int32_t kHubAttachFailureModeAlways = 2;
static const int32_t kHubRegisterModeNormal = 0;
static const int32_t kHubRegisterModeFail = 1;
static const std::string kHotkeyNativeLabel = "Hotkey";
static std::string g_hotkeyNativeBinding = "X";
static bool g_nativeHotkeyBindingActive = false;
static emc::ModHubClient g_modHubClient;
static int32_t g_modHubAttachFailureMode = kHubAttachFailureModeNone;
static int32_t g_modHubRegisterMode = kHubRegisterModeNormal;

static void WallBGoneDebugLog(const char* message)
{
    if (g_debugLogging)
    {
        DebugLog(message);
    }
}

struct WallBGoneRuntimeStateV1
{
    int32_t enabled;
    int32_t sleeping_bag_dismantle_enabled;
    int32_t hotkey_keycode;
    uint32_t hotkey_modifiers;
};

static bool SaveConfigState();
static const emc::ModHubClientTableRegistrationV1* GetModHubTableRegistration();
static const char* KeyCodeToName(OIS::KeyCode keyCode);
static std::string FormatHotkeyBinding(
    OIS::KeyCode keyCode,
    bool requireCtrl,
    bool requireShift,
    bool requireAlt);
static void RefreshHotkeyUiWidgets();
static void SyncNativeBindingFromHotkey();

static MyGUI::Button* g_hotkeyRebindButton = 0;
static MyGUI::Button* g_hotkeyResetButton = 0;
static MyGUI::TextBox* g_hotkeyLabelWidget = 0;
static bool g_captureInputSuppressed = false;
static bool g_capturePrevControlEnabled = true;

static void BeginHotkeyCapture()
{
    g_hotkeyCaptureState = HotkeyCapture_AwaitKey;
    g_captureInputSuppressed = false;
    if (key)
    {
        g_capturePrevControlEnabled = key->controlEnabled;
        key->controlEnabled = false;
        g_captureInputSuppressed = true;
    }
}

static void EndHotkeyCapture()
{
    if (g_captureInputSuppressed && key)
    {
        key->controlEnabled = g_capturePrevControlEnabled;
    }
    g_captureInputSuppressed = false;
    g_hotkeyCaptureState = HotkeyCapture_Idle;
}

static void RefreshHotkeyUiWidgets()
{
    if (g_hotkeyRebindButton)
    {
        std::stringstream caption;
        if (g_hotkeyCaptureState == HotkeyCapture_AwaitKey)
        {
            caption << "Press primary key...";
        }
        else
        {
            caption << FormatHotkeyBinding(
                g_hotkeyPrimary,
                g_hotkeyRequireCtrl,
                g_hotkeyRequireShift,
                g_hotkeyRequireAlt);
        }
        g_hotkeyRebindButton->setCaption(caption.str());
    }

    if (g_hotkeyResetButton)
    {
        std::stringstream caption;
        caption << "Reset ("
            << FormatHotkeyBinding(
                kDefaultHotkey,
                kDefaultHotkeyRequireCtrl,
                kDefaultHotkeyRequireShift,
                kDefaultHotkeyRequireAlt)
            << ")";
        g_hotkeyResetButton->setCaption(caption.str());
    }

}

static void OnRebindHotkeyButtonClicked(MyGUI::Widget*)
{
    BeginHotkeyCapture();
    RefreshHotkeyUiWidgets();
}

static void OnResetHotkeyButtonClicked(MyGUI::Widget*)
{
    EndHotkeyCapture();
    g_pendingHotkeyPrimary = kDefaultHotkey;
    g_hotkeyPrimary = kDefaultHotkey;
    g_hotkeyRequireCtrl = kDefaultHotkeyRequireCtrl;
    g_hotkeyRequireShift = kDefaultHotkeyRequireShift;
    g_hotkeyRequireAlt = kDefaultHotkeyRequireAlt;
    SyncNativeBindingFromHotkey();
    SaveConfigState();
    RefreshHotkeyUiWidgets();
}

static void CreateFallbackKeybindControls(MyGUI::Widget* parentWidget)
{
    if (!parentWidget)
    {
        ErrorLog("Wall-B-Gone: fallback keybind parent widget is null");
        return;
    }

    const int panelW = parentWidget->getWidth();
    const int y = 326;
    const int labelX = 43;
    const int rebindW = 360;
    const int resetW = 120;
    const int rightPadding = 40;
    const int gap = 12;
    int rebindX = panelW - (rebindW + resetW + gap + rightPadding);
    if (rebindX < 280)
    {
        rebindX = 280;
    }
    const int resetX = rebindX + rebindW + gap;

    g_hotkeyLabelWidget = parentWidget->createWidget<MyGUI::TextBox>(
        "Kenshi_TextboxStandardText",
        MyGUI::IntCoord(labelX, y + 6, 280, 26),
        MyGUI::Align::Default);

    if (g_hotkeyLabelWidget)
    {
        g_hotkeyLabelWidget->setVisible(true);
        g_hotkeyLabelWidget->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        g_hotkeyLabelWidget->setCaption("Dismantle Hotkey");
    }

    // Align with Kenshi settings look: value button + reset button on the right side.
    g_hotkeyRebindButton = parentWidget->createWidget<MyGUI::Button>(
        "Kenshi_Button1",
        MyGUI::IntCoord(rebindX, y, rebindW, 40),
        MyGUI::Align::Default);

    if (g_hotkeyRebindButton)
    {
        g_hotkeyRebindButton->eventMouseButtonClick += MyGUI::newDelegate(&OnRebindHotkeyButtonClicked);
        g_hotkeyRebindButton->setVisible(true);
        g_hotkeyRebindButton->setEnabled(true);
        g_hotkeyRebindButton->setNeedToolTip(true);
        g_hotkeyRebindButton->setUserString("ToolTip", "Click then press a primary key. Use the modifier toggles above to build combos.");
    }

    g_hotkeyResetButton = parentWidget->createWidget<MyGUI::Button>(
        "Kenshi_Button1",
        MyGUI::IntCoord(resetX, y, resetW, 40),
        MyGUI::Align::Default);

    if (g_hotkeyResetButton)
    {
        g_hotkeyResetButton->eventMouseButtonClick += MyGUI::newDelegate(&OnResetHotkeyButtonClicked);
        g_hotkeyResetButton->setVisible(true);
        g_hotkeyResetButton->setEnabled(true);
        g_hotkeyResetButton->setNeedToolTip(true);
        g_hotkeyResetButton->setUserString("ToolTip", "Reset the dismantle hotkey and modifier requirements to defaults.");
    }

    RefreshHotkeyUiWidgets();
}

class ToolTip;
class ForgottenGUI;
class DatapanelGUI;
class DataPanelLine;
class DataPanelLine_CheckBox;
class DataPanelLine_KeyConfig;

class OptionsWindow : public GUIWindow, public wraps::BaseLayout
{
public:
    char _0xd0;
    lektor<std::string> _0xd8;
    int _0xf0;
    DataPanelLine_KeyConfig* _0xf8;
    DatapanelGUI* datapanel;
    MyGUI::TabControl* optionsTab;
    bool _0x110;
    ToolTip* tooltip;
    bool _0x120;
};

class DatapanelGUI : public GUIWindow
{
public:
    virtual ~DatapanelGUI() {}
    virtual void vfunc0x68() {}
    virtual void vfunc0x70() {}
    virtual void vfunc0x78() {}
    virtual void vfunc0x80() {}
    virtual void vfunc0x88() {}
    virtual void vfunc0x90() {}
    virtual void vfunc0x98() {}
    virtual void vfunc0xa0() {}
    virtual void vfunc0xa8() {}
    virtual void vfunc0xb0() {}
    virtual void vfunc0xb8() {}
    virtual void vfunc0xc0(int) {}
    virtual void vfunc0xc8() {}
    virtual void vfunc0xd0() {}
    virtual void vfunc0xd8() {}
    virtual void vfunc0xe0(float) {}
    virtual void vfunc0xe8() {}
    virtual void vfunc0xf0() {}
};

class DataPanelLine : public Ogre::GeneralAllocatedObject
{
public:
    virtual ~DataPanelLine() {}
    virtual void setVisible(bool) {}
    virtual void setEnabled(bool) {}
    virtual void chenge() {}
    virtual void input(float&) {}
    virtual void setTooltip(const std::string&, ToolTip*) {}
};

class DataPanelLine_CheckBox : public DataPanelLine
{
public:
    bool* valuePtr;
};

typedef DatapanelGUI* (*FnCreateDatapanel)(ForgottenGUI*, const std::string&, MyGUI::Widget*, bool);
typedef DataPanelLine* (*FnCreateHeaderLine)(DatapanelGUI*, const std::string&, const std::string&, int, bool, bool);
typedef DataPanelLine_CheckBox* (*FnCreateCheckboxLine)(DatapanelGUI*, const std::string&, bool&, int);
typedef DataPanelLine* (*FnCreateKeyConfigLine)(DatapanelGUI*, const char*, bool, int, float, float, const char*);
typedef DataPanelLine* (*FnCreateKeyConfigLineWrapper)(
    DatapanelGUI*,
    const void*,
    const void*,
    int);
typedef void (*FnOptionsInit)(OptionsWindow*);
typedef void (*FnOptionsSave)(OptionsWindow*);

static FnCreateDatapanel g_fnCreateDatapanel = 0;
static FnCreateHeaderLine g_fnCreateHeaderLine = 0;
static FnCreateCheckboxLine g_fnCreateCheckboxLine = 0;
static FnCreateKeyConfigLine g_fnCreateKeyConfigLine = 0;
static FnCreateKeyConfigLineWrapper g_fnCreateKeyConfigLineWrapper = 0;
static FnOptionsInit g_fnOptionsInit = 0;
static FnOptionsSave g_fnOptionsSave = 0;
static FnOptionsInit g_fnOptionsInitOrig = 0;
static FnOptionsSave g_fnOptionsSaveOrig = 0;
static ForgottenGUI* g_ptrKenshiGUI = 0;

struct NativeStringObj
{
    char data[16];
    uint64_t size;
    uint64_t capacity;
};

struct KeyNameEntry
{
    const char* name;
    OIS::KeyCode keyCode;
};

static const KeyNameEntry kHotkeyNameMap[] =
{
    { "A", OIS::KC_A }, { "B", OIS::KC_B }, { "C", OIS::KC_C }, { "D", OIS::KC_D }, { "E", OIS::KC_E },
    { "F", OIS::KC_F }, { "G", OIS::KC_G }, { "H", OIS::KC_H }, { "I", OIS::KC_I }, { "J", OIS::KC_J },
    { "K", OIS::KC_K }, { "L", OIS::KC_L }, { "M", OIS::KC_M }, { "N", OIS::KC_N }, { "O", OIS::KC_O },
    { "P", OIS::KC_P }, { "Q", OIS::KC_Q }, { "R", OIS::KC_R }, { "S", OIS::KC_S }, { "T", OIS::KC_T },
    { "U", OIS::KC_U }, { "V", OIS::KC_V }, { "W", OIS::KC_W }, { "X", OIS::KC_X }, { "Y", OIS::KC_Y },
    { "Z", OIS::KC_Z },
    { "0", OIS::KC_0 }, { "1", OIS::KC_1 }, { "2", OIS::KC_2 }, { "3", OIS::KC_3 }, { "4", OIS::KC_4 },
    { "5", OIS::KC_5 }, { "6", OIS::KC_6 }, { "7", OIS::KC_7 }, { "8", OIS::KC_8 }, { "9", OIS::KC_9 },
    { "SPACE", OIS::KC_SPACE }, { "TAB", OIS::KC_TAB }, { "RETURN", OIS::KC_RETURN }, { "ENTER", OIS::KC_RETURN },
    { "BACK", OIS::KC_BACK }, { "INSERT", OIS::KC_INSERT }, { "DELETE", OIS::KC_DELETE },
    { "HOME", OIS::KC_HOME }, { "END", OIS::KC_END }, { "PGUP", OIS::KC_PGUP }, { "PGDOWN", OIS::KC_PGDOWN },
    { "PAGEUP", OIS::KC_PGUP }, { "PAGEDOWN", OIS::KC_PGDOWN },
    { "F1", OIS::KC_F1 }, { "F2", OIS::KC_F2 }, { "F3", OIS::KC_F3 }, { "F4", OIS::KC_F4 },
    { "F5", OIS::KC_F5 }, { "F6", OIS::KC_F6 }, { "F7", OIS::KC_F7 }, { "F8", OIS::KC_F8 },
    { "F9", OIS::KC_F9 }, { "F10", OIS::KC_F10 }, { "F11", OIS::KC_F11 }, { "F12", OIS::KC_F12 },
    { "MINUS", OIS::KC_MINUS }, { "EQUALS", OIS::KC_EQUALS }, { "LBRACKET", OIS::KC_LBRACKET },
    { "RBRACKET", OIS::KC_RBRACKET }, { "BACKSLASH", OIS::KC_BACKSLASH }, { "SEMICOLON", OIS::KC_SEMICOLON },
    { "APOSTROPHE", OIS::KC_APOSTROPHE }, { "GRAVE", OIS::KC_GRAVE }, { "COMMA", OIS::KC_COMMA },
    { "PERIOD", OIS::KC_PERIOD }, { "SLASH", OIS::KC_SLASH },
    { "LSHIFT", OIS::KC_LSHIFT }, { "RSHIFT", OIS::KC_RSHIFT }, { "LCONTROL", OIS::KC_LCONTROL },
    { "RCONTROL", OIS::KC_RCONTROL }, { "CTRL", OIS::KC_LCONTROL }, { "LALT", OIS::KC_LMENU },
    { "RALT", OIS::KC_RMENU }, { "ALT", OIS::KC_LMENU },
    { "ESCAPE", OIS::KC_ESCAPE }, { "LWIN", OIS::KC_LWIN }, { "RWIN", OIS::KC_RWIN },
    { "ESC", OIS::KC_ESCAPE },
    { "SYSRQ", OIS::KC_SYSRQ }, { "APPS", OIS::KC_APPS }
};

static const size_t kHotkeyNameMapCount = sizeof(kHotkeyNameMap) / sizeof(kHotkeyNameMap[0]);

#include "src/WallBGoneConfig.inl"

#include "src/WallBGoneModHub.inl"

#include "src/WallBGoneUiRuntime.inl"

__declspec(dllexport) void startPlugin()
{
    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    if (platform == KenshiLib::BinaryVersion::UNKNOWN || (version != "1.0.65" && version != "1.0.68"))
    {
        ErrorLog("Wall-B-Gone: unsupported Kenshi version/platform");
        return;
    }

    const uintptr_t baseAddr = reinterpret_cast<uintptr_t>(GetModuleHandleA(0));
    if (!InitPluginMenuFunctions(platform, version, baseAddr))
    {
        ErrorLog("Wall-B-Gone: failed to initialize plugin menu function pointers");
        return;
    }

    LoadConfigState();
    SaveConfigState();

    ConfigureModHubClient();
    StartModHubClient();

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(g_fnOptionsInit, OptionsWindowInitHook, &g_fnOptionsInitOrig))
    {
        ErrorLog("Wall-B-Gone: Could not hook options init");
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(g_fnOptionsSave, OptionsWindowSaveHook, &g_fnOptionsSaveOrig))
    {
        ErrorLog("Wall-B-Gone: Could not hook options save");
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        &PlayerInterface_updateUT_orig))
    {
        ErrorLog("Wall-B-Gone: Could not hook PlayerInterface::updateUT");
        return;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&InputHandler::keyDownEvent),
        InputHandler_keyDownEvent_hook,
        &InputHandler_keyDownEvent_orig))
    {
        ErrorLog("Wall-B-Gone: Could not hook InputHandler::keyDownEvent");
        return;
    }

}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        char dllPath[_MAX_PATH] = { 0 };
        if (GetModuleFileNameA(hModule, dllPath, _MAX_PATH) > 0)
        {
            std::string fullPath(dllPath);
            size_t sep = fullPath.find_last_of("\\/");
            if (sep != std::string::npos)
            {
                const std::string myDirectory = fullPath.substr(0, sep);
                g_settingsPath = myDirectory + "\\mod-config.json";
            }
        }
    }
    return TRUE;
}
