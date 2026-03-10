#include <Debug.h>

#include <core/Functions.h>
#include "emc/mod_hub_client.h"

#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/InputHandler.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/Building.h>
#include <kenshi/Character.h>
#include <kenshi/GameData.h>
#include <kenshi/Kenshi.h>
#include <kenshi/TitleScreen.h>
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

static void InitNativeStringObj(NativeStringObj* strObj, const std::string& value)
{
    if (!strObj)
    {
        return;
    }

    for (int i = 0; i < 16; ++i)
    {
        strObj->data[i] = 0;
    }

    size_t len = value.size();
    if (len > 15)
    {
        len = 15;
    }

    for (size_t i = 0; i < len; ++i)
    {
        strObj->data[i] = value[i];
    }

    strObj->size = static_cast<uint64_t>(len);
    strObj->capacity = 15;
}

static std::string TrimAscii(const std::string& value)
{
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
    {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }

    return value.substr(start, end - start);
}

static std::string ToUpperAscii(const std::string& value)
{
    std::string upper = value;
    for (size_t i = 0; i < upper.size(); ++i)
    {
        upper[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(upper[i])));
    }
    return upper;
}

static bool TryExtractJsonString(const std::string& body, const char* key, std::string* valueOut)
{
    if (!valueOut || !key)
    {
        return false;
    }

    const std::string quotedKey = std::string("\"") + key + "\"";
    const size_t keyPos = body.find(quotedKey);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    const size_t colonPos = body.find(':', keyPos + quotedKey.size());
    if (colonPos == std::string::npos)
    {
        return false;
    }

    const size_t firstQuote = body.find('"', colonPos + 1);
    if (firstQuote == std::string::npos)
    {
        return false;
    }

    const size_t secondQuote = body.find('"', firstQuote + 1);
    if (secondQuote == std::string::npos || secondQuote <= firstQuote + 1)
    {
        return false;
    }

    *valueOut = body.substr(firstQuote + 1, secondQuote - firstQuote - 1);
    return true;
}

static bool TryParseKeyCode(const std::string& keyName, OIS::KeyCode* outKey)
{
    if (!outKey)
    {
        return false;
    }

    const std::string normalized = ToUpperAscii(TrimAscii(keyName));
    if (normalized.empty())
    {
        return false;
    }

    for (size_t i = 0; i < kHotkeyNameMapCount; ++i)
    {
        if (normalized == kHotkeyNameMap[i].name)
        {
            *outKey = kHotkeyNameMap[i].keyCode;
            return true;
        }
    }

    return false;
}

static const char* KeyCodeToName(OIS::KeyCode keyCode)
{
    for (size_t i = 0; i < kHotkeyNameMapCount; ++i)
    {
        if (kHotkeyNameMap[i].keyCode == keyCode)
        {
            return kHotkeyNameMap[i].name;
        }
    }

    return "UNKNOWN";
}

static bool IsCtrlKeyCode(OIS::KeyCode keyCode)
{
    return keyCode == OIS::KC_LCONTROL || keyCode == OIS::KC_RCONTROL;
}

static bool IsShiftKeyCode(OIS::KeyCode keyCode)
{
    return keyCode == OIS::KC_LSHIFT || keyCode == OIS::KC_RSHIFT;
}

static bool IsAltKeyCode(OIS::KeyCode keyCode)
{
    return keyCode == OIS::KC_LMENU || keyCode == OIS::KC_RMENU;
}

static bool IsModifierDown(OIS::Keyboard* keyboard, OIS::KeyCode keyCode)
{
    if (keyboard == 0)
    {
        return false;
    }

    if (IsCtrlKeyCode(keyCode))
    {
        return keyboard->isKeyDown(OIS::KC_LCONTROL) || keyboard->isKeyDown(OIS::KC_RCONTROL);
    }

    if (IsShiftKeyCode(keyCode))
    {
        return keyboard->isKeyDown(OIS::KC_LSHIFT) || keyboard->isKeyDown(OIS::KC_RSHIFT);
    }

    if (IsAltKeyCode(keyCode))
    {
        return keyboard->isKeyDown(OIS::KC_LMENU) || keyboard->isKeyDown(OIS::KC_RMENU);
    }

    return false;
}

static std::string FormatHotkeyBinding(
    OIS::KeyCode keyCode,
    bool requireCtrl,
    bool requireShift,
    bool requireAlt)
{
    std::stringstream ss;
    bool wrotePrefix = false;

    if (requireCtrl && !IsCtrlKeyCode(keyCode))
    {
        ss << "CTRL";
        wrotePrefix = true;
    }
    if (requireShift && !IsShiftKeyCode(keyCode))
    {
        if (wrotePrefix)
        {
            ss << "+";
        }
        ss << "SHIFT";
        wrotePrefix = true;
    }
    if (requireAlt && !IsAltKeyCode(keyCode))
    {
        if (wrotePrefix)
        {
            ss << "+";
        }
        ss << "ALT";
        wrotePrefix = true;
    }

    if (wrotePrefix)
    {
        ss << "+";
    }
    ss << KeyCodeToName(keyCode);
    return ss.str();
}

static bool IsSupportedKeyCode(OIS::KeyCode keyCode)
{
    for (size_t i = 0; i < kHotkeyNameMapCount; ++i)
    {
        if (kHotkeyNameMap[i].keyCode == keyCode)
        {
            return true;
        }
    }

    return false;
}

static void SyncNativeBindingFromHotkey()
{
    g_hotkeyNativeBinding = KeyCodeToName(g_hotkeyPrimary);
}

static HotkeyValidationResult ValidateHotkey(OIS::KeyCode keyCode, std::string* reason)
{
    if (!IsSupportedKeyCode(keyCode))
    {
        if (reason)
        {
            *reason = "unsupported key";
        }
        return HotkeyValidation_Invalid;
    }

    if (keyCode == OIS::KC_UNASSIGNED)
    {
        if (reason)
        {
            *reason = "key is unassigned";
        }
        return HotkeyValidation_Invalid;
    }

    if (keyCode == OIS::KC_ESCAPE || keyCode == OIS::KC_LWIN || keyCode == OIS::KC_RWIN || keyCode == OIS::KC_SYSRQ)
    {
        if (reason)
        {
            *reason = "system-reserved key";
        }
        return HotkeyValidation_BlockedSystem;
    }

    if (keyCode == OIS::KC_APPS)
    {
        if (reason)
        {
            *reason = "reserved key";
        }
        return HotkeyValidation_BlockedReserved;
    }

    if (reason)
    {
        reason->clear();
    }
    return HotkeyValidation_Ok;
}

static bool InitPluginMenuFunctions(unsigned int platform, const std::string& version, uintptr_t baseAddr)
{
    if (platform == 1)
    {
        if (version == "1.0.65")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(baseAddr + 0x003F0120);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(baseAddr + 0x003EC950);
            g_fnCreateHeaderLine = reinterpret_cast<FnCreateHeaderLine>(baseAddr + 0x006FDE20);
            g_fnCreateCheckboxLine = reinterpret_cast<FnCreateCheckboxLine>(baseAddr + 0x006FE210);
            g_fnCreateKeyConfigLine = reinterpret_cast<FnCreateKeyConfigLine>(baseAddr + 0x006FF450);
            g_fnCreateKeyConfigLineWrapper = reinterpret_cast<FnCreateKeyConfigLineWrapper>(baseAddr + 0x006FF190);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(baseAddr + 0x0073F4B0);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(baseAddr + 0x02132750);
            return true;
        }
        if (version == "1.0.68")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(baseAddr + 0x003F0260);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(baseAddr + 0x003ECA90);
            g_fnCreateHeaderLine = reinterpret_cast<FnCreateHeaderLine>(baseAddr + 0x006FE9F0);
            g_fnCreateCheckboxLine = reinterpret_cast<FnCreateCheckboxLine>(baseAddr + 0x006FEDE0);
            g_fnCreateKeyConfigLine = reinterpret_cast<FnCreateKeyConfigLine>(baseAddr + 0x00700020);
            g_fnCreateKeyConfigLineWrapper = reinterpret_cast<FnCreateKeyConfigLineWrapper>(baseAddr + 0x006FFD60);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(baseAddr + 0x0073FFE0);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(baseAddr + 0x021337B0);
            return true;
        }
    }
    else if (platform == 0)
    {
        if (version == "1.0.65")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(baseAddr + 0x003EFD40);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(baseAddr + 0x003EC570);
            g_fnCreateHeaderLine = reinterpret_cast<FnCreateHeaderLine>(baseAddr + 0x006FD780);
            g_fnCreateCheckboxLine = reinterpret_cast<FnCreateCheckboxLine>(baseAddr + 0x006FDB70);
            g_fnCreateKeyConfigLine = reinterpret_cast<FnCreateKeyConfigLine>(baseAddr + 0x006FEDB0);
            g_fnCreateKeyConfigLineWrapper = reinterpret_cast<FnCreateKeyConfigLineWrapper>(baseAddr + 0x006FEA30);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(baseAddr + 0x0073EE10);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(baseAddr + 0x021306C0);
            return true;
        }
        if (version == "1.0.68")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(baseAddr + 0x003EFC00);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(baseAddr + 0x003EC430);
            g_fnCreateHeaderLine = reinterpret_cast<FnCreateHeaderLine>(baseAddr + 0x006FE390);
            g_fnCreateCheckboxLine = reinterpret_cast<FnCreateCheckboxLine>(baseAddr + 0x006FE780);
            g_fnCreateKeyConfigLine = reinterpret_cast<FnCreateKeyConfigLine>(baseAddr + 0x006FF170);
            g_fnCreateKeyConfigLineWrapper = reinterpret_cast<FnCreateKeyConfigLineWrapper>(baseAddr + 0x006FEEA0);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(baseAddr + 0x0073F980);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(baseAddr + 0x021326E0);
            return true;
        }
    }

    return false;
}

static bool ReadEnabledFromBody(const std::string& body, bool* foundValue)
{
    const size_t keyPos = body.find("\"enabled\"");
    if (keyPos == std::string::npos)
    {
        return true;
    }

    const size_t truePos = body.find("true", keyPos);
    const size_t falsePos = body.find("false", keyPos);

    *foundValue = true;
    if (falsePos != std::string::npos && (truePos == std::string::npos || falsePos < truePos))
    {
        return false;
    }

    return true;
}

static bool ReadBoolKeyFromBody(const std::string& body, const char* keyName, bool defaultValue)
{
    if (!keyName || keyName[0] == '\0')
    {
        return defaultValue;
    }

    const std::string quotedKey = std::string("\"") + keyName + "\"";
    const size_t keyPos = body.find(quotedKey);
    if (keyPos == std::string::npos)
    {
        return defaultValue;
    }

    const size_t truePos = body.find("true", keyPos + quotedKey.size());
    const size_t falsePos = body.find("false", keyPos + quotedKey.size());
    if (falsePos != std::string::npos && (truePos == std::string::npos || falsePos < truePos))
    {
        return false;
    }

    return true;
}

static bool ReadSleepingBagDismantleEnabledFromBody(const std::string& body, bool* foundValue)
{
    const size_t keyPos = body.find("\"sleepingBagDismantleEnabled\"");
    if (keyPos == std::string::npos)
    {
        return true;
    }

    const size_t truePos = body.find("true", keyPos);
    const size_t falsePos = body.find("false", keyPos);

    if (foundValue)
    {
        *foundValue = true;
    }

    if (falsePos != std::string::npos && (truePos == std::string::npos || falsePos < truePos))
    {
        return false;
    }

    return true;
}

static bool ReadHotkeyRequireCtrlFromBody(const std::string& body)
{
    return ReadBoolKeyFromBody(body, "hotkeyRequireCtrl", kDefaultHotkeyRequireCtrl);
}

static bool ReadHotkeyRequireShiftFromBody(const std::string& body)
{
    return ReadBoolKeyFromBody(body, "hotkeyRequireShift", kDefaultHotkeyRequireShift);
}

static bool ReadHotkeyRequireAltFromBody(const std::string& body)
{
    return ReadBoolKeyFromBody(body, "hotkeyRequireAlt", kDefaultHotkeyRequireAlt);
}

static bool ReadHotkeyFromBody(const std::string& body, OIS::KeyCode* hotkeyOut)
{
    if (!hotkeyOut)
    {
        return false;
    }

    std::string hotkeyText;
    if (!TryExtractJsonString(body, "hotkey", &hotkeyText))
    {
        *hotkeyOut = kDefaultHotkey;
        return true;
    }

    OIS::KeyCode parsedKey = OIS::KC_UNASSIGNED;
    if (!TryParseKeyCode(hotkeyText, &parsedKey))
    {
        std::stringstream warning;
        warning << "Wall-B-Gone WARN: failed to parse hotkey '" << hotkeyText << "' from config; using default '"
            << KeyCodeToName(kDefaultHotkey) << "'";
        DebugLog(warning.str().c_str());
        *hotkeyOut = kDefaultHotkey;
        return true;
    }

    std::string validationReason;
    const HotkeyValidationResult validationResult = ValidateHotkey(parsedKey, &validationReason);
    if (validationResult != HotkeyValidation_Ok)
    {
        std::stringstream warning;
        warning << "Wall-B-Gone WARN: config hotkey '" << KeyCodeToName(parsedKey) << "' is blocked (" << validationReason
            << "); using default '" << KeyCodeToName(kDefaultHotkey) << "'";
        DebugLog(warning.str().c_str());
        *hotkeyOut = kDefaultHotkey;
        return true;
    }

    *hotkeyOut = parsedKey;
    return true;
}

static bool ReadConfigFromFile(
    const std::string& configPath,
    bool* enabledOut,
    bool* sleepingBagDismantleEnabledOut,
    bool* hotkeyRequireCtrlOut,
    bool* hotkeyRequireShiftOut,
    bool* hotkeyRequireAltOut,
    OIS::KeyCode* hotkeyOut)
{
    if (!enabledOut
        || !sleepingBagDismantleEnabledOut
        || !hotkeyRequireCtrlOut
        || !hotkeyRequireShiftOut
        || !hotkeyRequireAltOut
        || !hotkeyOut)
    {
        return false;
    }

    std::ifstream in(configPath.c_str(), std::ios::in | std::ios::binary);
    if (!in)
    {
        return true;
    }

    const std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    bool foundEnabled = false;
    *enabledOut = ReadEnabledFromBody(body, &foundEnabled);
    bool foundSleepingBagDismantleEnabled = false;
    *sleepingBagDismantleEnabledOut = ReadSleepingBagDismantleEnabledFromBody(body, &foundSleepingBagDismantleEnabled);
    *hotkeyRequireCtrlOut = ReadHotkeyRequireCtrlFromBody(body);
    *hotkeyRequireShiftOut = ReadHotkeyRequireShiftFromBody(body);
    *hotkeyRequireAltOut = ReadHotkeyRequireAltFromBody(body);
    return ReadHotkeyFromBody(body, hotkeyOut);
}

static bool SaveConfigToFile(
    const std::string& configPath,
    bool enabled,
    bool sleepingBagDismantleEnabled,
    bool hotkeyRequireCtrl,
    bool hotkeyRequireShift,
    bool hotkeyRequireAlt,
    OIS::KeyCode hotkey)
{
    std::string validationReason;
    const HotkeyValidationResult validationResult = ValidateHotkey(hotkey, &validationReason);
    if (validationResult != HotkeyValidation_Ok)
    {
        std::stringstream error;
        error << "Wall-B-Gone ERROR: refusing to save invalid hotkey '" << KeyCodeToName(hotkey)
            << "' (" << validationReason << ")";
        ErrorLog(error.str().c_str());
        return false;
    }

    std::ofstream out(configPath.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out)
    {
        return false;
    }

    out << "{\n";
    out << "  \"enabled\": " << (enabled ? "true" : "false") << ",\n";
    out << "  \"sleepingBagDismantleEnabled\": " << (sleepingBagDismantleEnabled ? "true" : "false") << ",\n";
    out << "  \"hotkeyRequireCtrl\": " << (hotkeyRequireCtrl ? "true" : "false") << ",\n";
    out << "  \"hotkeyRequireShift\": " << (hotkeyRequireShift ? "true" : "false") << ",\n";
    out << "  \"hotkeyRequireAlt\": " << (hotkeyRequireAlt ? "true" : "false") << ",\n";
    out << "  \"hotkey\": \"" << KeyCodeToName(hotkey) << "\"\n";
    out << "}\n";

    return true;
}

static void LoadConfigState()
{
    g_modEnabled = true;
    g_sleepingBagDismantleEnabled = true;
    g_hotkeyPrimary = kDefaultHotkey;
    g_pendingHotkeyPrimary = kDefaultHotkey;
    g_hotkeyRequireCtrl = kDefaultHotkeyRequireCtrl;
    g_hotkeyRequireShift = kDefaultHotkeyRequireShift;
    g_hotkeyRequireAlt = kDefaultHotkeyRequireAlt;

    if (g_settingsPath.empty())
    {
        return;
    }

    bool loadedEnabled = true;
    bool loadedSleepingBagDismantleEnabled = true;
    bool loadedHotkeyRequireCtrl = kDefaultHotkeyRequireCtrl;
    bool loadedHotkeyRequireShift = kDefaultHotkeyRequireShift;
    bool loadedHotkeyRequireAlt = kDefaultHotkeyRequireAlt;
    OIS::KeyCode loadedHotkey = kDefaultHotkey;
    if (!ReadConfigFromFile(
        g_settingsPath,
        &loadedEnabled,
        &loadedSleepingBagDismantleEnabled,
        &loadedHotkeyRequireCtrl,
        &loadedHotkeyRequireShift,
        &loadedHotkeyRequireAlt,
        &loadedHotkey))
    {
        ErrorLog("Wall-B-Gone ERROR: failed to read mod-config.json; using defaults");
        return;
    }

    g_modEnabled = loadedEnabled;
    g_sleepingBagDismantleEnabled = loadedSleepingBagDismantleEnabled;
    g_hotkeyPrimary = loadedHotkey;
    g_pendingHotkeyPrimary = loadedHotkey;
    g_hotkeyRequireCtrl = loadedHotkeyRequireCtrl;
    g_hotkeyRequireShift = loadedHotkeyRequireShift;
    g_hotkeyRequireAlt = loadedHotkeyRequireAlt;
    SyncNativeBindingFromHotkey();

    std::stringstream info;
    info << "Wall-B-Gone INFO: loaded config enabled=" << (g_modEnabled ? "true" : "false")
        << " sleepingBagDismantleEnabled=" << (g_sleepingBagDismantleEnabled ? "true" : "false")
        << " hotkey=" << FormatHotkeyBinding(
            g_hotkeyPrimary,
            g_hotkeyRequireCtrl,
            g_hotkeyRequireShift,
            g_hotkeyRequireAlt);
    DebugLog(info.str().c_str());
}

static bool SaveConfigState()
{
    if (g_settingsPath.empty())
    {
        ErrorLog("Wall-B-Gone: settings path is empty; cannot save mod-config.json");
        return false;
    }

    if (!SaveConfigToFile(
        g_settingsPath,
        g_modEnabled,
        g_sleepingBagDismantleEnabled,
        g_hotkeyRequireCtrl,
        g_hotkeyRequireShift,
        g_hotkeyRequireAlt,
        g_hotkeyPrimary))
    {
        ErrorLog("Wall-B-Gone: failed to save mod-config.json");
        return false;
    }

    std::stringstream info;
    info << "Wall-B-Gone INFO: saved config enabled=" << (g_modEnabled ? "true" : "false")
        << " sleepingBagDismantleEnabled=" << (g_sleepingBagDismantleEnabled ? "true" : "false")
        << " hotkey=" << FormatHotkeyBinding(
            g_hotkeyPrimary,
            g_hotkeyRequireCtrl,
            g_hotkeyRequireShift,
            g_hotkeyRequireAlt);
    DebugLog(info.str().c_str());

    return true;
}

static void EnsureRuntimeHotkeyValid()
{
    if (IsSupportedKeyCode(g_hotkeyPrimary))
    {
        g_loggedRuntimeHotkeyFallback = false;
        return;
    }

    g_hotkeyPrimary = kDefaultHotkey;
    g_pendingHotkeyPrimary = kDefaultHotkey;
    SyncNativeBindingFromHotkey();

    if (!g_loggedRuntimeHotkeyFallback)
    {
        ErrorLog("Wall-B-Gone WARN: runtime hotkey unsupported; falling back to default");
        g_loggedRuntimeHotkeyFallback = true;
    }
}

static void WriteRuntimeApiError(char* err_buf, uint32_t err_buf_size, const char* text)
{
    if (!err_buf || err_buf_size == 0u || !text)
    {
        return;
    }

    const size_t copyLen = static_cast<size_t>(err_buf_size - 1u);
    std::strncpy(err_buf, text, copyLen);
    err_buf[copyLen] = '\0';
}

static bool IsHubUserDataValid(void* user_data)
{
    return user_data == &g_modHubClient;
}

static EMC_Result HubGetBoolSetting(void* user_data, bool value, int32_t* out_value)
{
    if (!IsHubUserDataValid(user_data) || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = value ? 1 : 0;
    return EMC_OK;
}

static EMC_Result HubSetBoolSetting(void* user_data, int32_t value, bool* target, char* err_buf, uint32_t err_buf_size)
{
    if (!IsHubUserDataValid(user_data) || target == 0)
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value != 0 && value != 1)
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "invalid_bool");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const bool previous_value = *target;
    *target = (value != 0);
    if (!SaveConfigState())
    {
        *target = previous_value;
        WriteRuntimeApiError(err_buf, err_buf_size, "persist_failed");
        return EMC_ERR_INTERNAL;
    }

    return EMC_OK;
}

static EMC_Result __cdecl HubGetEnabledSetting(void* user_data, int32_t* out_value)
{
    return HubGetBoolSetting(user_data, g_modEnabled, out_value);
}

static EMC_Result __cdecl HubSetEnabledSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return HubSetBoolSetting(user_data, value, &g_modEnabled, err_buf, err_buf_size);
}

static EMC_Result __cdecl HubGetSleepingBagEnabledSetting(void* user_data, int32_t* out_value)
{
    return HubGetBoolSetting(user_data, g_sleepingBagDismantleEnabled, out_value);
}

static EMC_Result __cdecl HubSetSleepingBagEnabledSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return HubSetBoolSetting(user_data, value, &g_sleepingBagDismantleEnabled, err_buf, err_buf_size);
}

static EMC_Result __cdecl HubGetHotkeyRequireCtrlSetting(void* user_data, int32_t* out_value)
{
    return HubGetBoolSetting(user_data, g_hotkeyRequireCtrl, out_value);
}

static EMC_Result __cdecl HubSetHotkeyRequireCtrlSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    const EMC_Result result = HubSetBoolSetting(user_data, value, &g_hotkeyRequireCtrl, err_buf, err_buf_size);
    if (result == EMC_OK)
    {
        RefreshHotkeyUiWidgets();
    }
    return result;
}

static EMC_Result __cdecl HubGetHotkeyRequireShiftSetting(void* user_data, int32_t* out_value)
{
    return HubGetBoolSetting(user_data, g_hotkeyRequireShift, out_value);
}

static EMC_Result __cdecl HubSetHotkeyRequireShiftSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    const EMC_Result result = HubSetBoolSetting(user_data, value, &g_hotkeyRequireShift, err_buf, err_buf_size);
    if (result == EMC_OK)
    {
        RefreshHotkeyUiWidgets();
    }
    return result;
}

static EMC_Result __cdecl HubGetHotkeyRequireAltSetting(void* user_data, int32_t* out_value)
{
    return HubGetBoolSetting(user_data, g_hotkeyRequireAlt, out_value);
}

static EMC_Result __cdecl HubSetHotkeyRequireAltSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    const EMC_Result result = HubSetBoolSetting(user_data, value, &g_hotkeyRequireAlt, err_buf, err_buf_size);
    if (result == EMC_OK)
    {
        RefreshHotkeyUiWidgets();
    }
    return result;
}

static EMC_Result __cdecl HubGetDismantleHotkeySetting(void* user_data, EMC_KeybindValueV1* out_value)
{
    if (!IsHubUserDataValid(user_data) || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EnsureRuntimeHotkeyValid();
    out_value->keycode = static_cast<int32_t>(g_hotkeyPrimary);
    out_value->modifiers = 0u;
    return EMC_OK;
}

static EMC_Result __cdecl HubSetDismantleHotkeySetting(
    void* user_data,
    EMC_KeybindValueV1 value,
    char* err_buf,
    uint32_t err_buf_size)
{
    if (!IsHubUserDataValid(user_data))
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value.modifiers != 0u)
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "use_modifier_toggles");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const OIS::KeyCode requestedHotkey = static_cast<OIS::KeyCode>(value.keycode);
    std::string validationReason;
    if (ValidateHotkey(requestedHotkey, &validationReason) != HotkeyValidation_Ok)
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "invalid_hotkey");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const OIS::KeyCode previous_hotkey = g_hotkeyPrimary;
    g_hotkeyPrimary = requestedHotkey;
    g_pendingHotkeyPrimary = requestedHotkey;
    SyncNativeBindingFromHotkey();
    RefreshHotkeyUiWidgets();

    if (!SaveConfigState())
    {
        g_hotkeyPrimary = previous_hotkey;
        g_pendingHotkeyPrimary = previous_hotkey;
        SyncNativeBindingFromHotkey();
        RefreshHotkeyUiWidgets();
        WriteRuntimeApiError(err_buf, err_buf_size, "persist_failed");
        return EMC_ERR_INTERNAL;
    }

    return EMC_OK;
}

static EMC_Result __cdecl HubResetHotkeyDefaultAction(void* user_data, char* err_buf, uint32_t err_buf_size)
{
    if (!IsHubUserDataValid(user_data))
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const OIS::KeyCode previous_hotkey = g_hotkeyPrimary;
    const bool previous_hotkey_require_ctrl = g_hotkeyRequireCtrl;
    const bool previous_hotkey_require_shift = g_hotkeyRequireShift;
    const bool previous_hotkey_require_alt = g_hotkeyRequireAlt;
    g_hotkeyPrimary = kDefaultHotkey;
    g_pendingHotkeyPrimary = kDefaultHotkey;
    g_hotkeyRequireCtrl = kDefaultHotkeyRequireCtrl;
    g_hotkeyRequireShift = kDefaultHotkeyRequireShift;
    g_hotkeyRequireAlt = kDefaultHotkeyRequireAlt;
    SyncNativeBindingFromHotkey();
    RefreshHotkeyUiWidgets();

    if (!SaveConfigState())
    {
        g_hotkeyPrimary = previous_hotkey;
        g_pendingHotkeyPrimary = previous_hotkey;
        g_hotkeyRequireCtrl = previous_hotkey_require_ctrl;
        g_hotkeyRequireShift = previous_hotkey_require_shift;
        g_hotkeyRequireAlt = previous_hotkey_require_alt;
        SyncNativeBindingFromHotkey();
        RefreshHotkeyUiWidgets();
        WriteRuntimeApiError(err_buf, err_buf_size, "persist_failed");
        return EMC_ERR_INTERNAL;
    }

    return EMC_OK;
}

static const emc::ModHubClientTableRegistrationV1* GetModHubTableRegistration()
{
    static const EMC_ModDescriptorV1 kModDescriptor = {
        kHubNamespaceId,
        kHubNamespaceDisplayName,
        kHubModId,
        kHubModDisplayName,
        &g_modHubClient };

    static const EMC_BoolSettingDefV1 kEnabledSettingDef = {
        kHubSettingEnabledId,
        "Enabled",
        "Enable Wall-B-Gone features",
        &g_modHubClient,
        &HubGetEnabledSetting,
        &HubSetEnabledSetting };

    static const EMC_BoolSettingDefV1 kSleepingBagEnabledSettingDef = {
        kHubSettingSleepingBagEnabledId,
        "Allow sleeping bag dismantle",
        "Allow dismantle behavior for sleeping bags",
        &g_modHubClient,
        &HubGetSleepingBagEnabledSetting,
        &HubSetSleepingBagEnabledSetting };

    static const EMC_KeybindSettingDefV1 kHotkeySettingDef = {
        kHubSettingHotkeyId,
        "Dismantle hotkey",
        "Primary key used to dismantle the selected wall or sleeping bag. Use the modifier toggles below for combos.",
        &g_modHubClient,
        &HubGetDismantleHotkeySetting,
        &HubSetDismantleHotkeySetting };

    static const EMC_BoolSettingDefV1 kHotkeyRequireCtrlSettingDef = {
        kHubSettingHotkeyRequireCtrlId,
        "Require Ctrl",
        "Require Ctrl to be held with the dismantle hotkey",
        &g_modHubClient,
        &HubGetHotkeyRequireCtrlSetting,
        &HubSetHotkeyRequireCtrlSetting };

    static const EMC_BoolSettingDefV1 kHotkeyRequireShiftSettingDef = {
        kHubSettingHotkeyRequireShiftId,
        "Require Shift",
        "Require Shift to be held with the dismantle hotkey",
        &g_modHubClient,
        &HubGetHotkeyRequireShiftSetting,
        &HubSetHotkeyRequireShiftSetting };

    static const EMC_BoolSettingDefV1 kHotkeyRequireAltSettingDef = {
        kHubSettingHotkeyRequireAltId,
        "Require Alt",
        "Require Alt to be held with the dismantle hotkey",
        &g_modHubClient,
        &HubGetHotkeyRequireAltSetting,
        &HubSetHotkeyRequireAltSetting };

    static const EMC_ActionRowDefV1 kResetHotkeyActionDef = {
        kHubActionResetHotkeyId,
        "Reset hotkey default",
        "Reset dismantle hotkey and modifier requirements to defaults",
        &g_modHubClient,
        EMC_ACTION_FORCE_REFRESH,
        &HubResetHotkeyDefaultAction };

    static const emc::ModHubClientSettingRowV1 kRows[] = {
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kEnabledSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kSleepingBagEnabledSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND, &kHotkeySettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kHotkeyRequireCtrlSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kHotkeyRequireShiftSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kHotkeyRequireAltSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION, &kResetHotkeyActionDef }
    };

    static const emc::ModHubClientTableRegistrationV1 kRegistration = {
        &kModDescriptor,
        kRows,
        static_cast<uint32_t>(sizeof(kRows) / sizeof(kRows[0])) };

    return &kRegistration;
}

static bool ShouldForceHubAttachFailure(bool is_retry)
{
    if (g_modHubAttachFailureMode == kHubAttachFailureModeAlways)
    {
        return true;
    }

    return g_modHubAttachFailureMode == kHubAttachFailureModeStartupOnly && !is_retry;
}

static bool __cdecl ShouldForceHubAttachFailureForClient(void* user_data, bool is_retry, EMC_Result* out_result)
{
    (void)user_data;
    if (!ShouldForceHubAttachFailure(is_retry))
    {
        return false;
    }

    if (out_result != 0)
    {
        *out_result = EMC_ERR_INTERNAL;
    }

    return true;
}

static EMC_Result __cdecl RegisterHubSettingsForClient(const EMC_HubApiV1* api, void* user_data)
{
    (void)user_data;
    if (g_modHubRegisterMode == kHubRegisterModeFail)
    {
        return EMC_ERR_INTERNAL;
    }

    return emc::RegisterSettingsTableV1(api, GetModHubTableRegistration());
}

static void ConfigureModHubClient()
{
    emc::ModHubClient::Config config;
    config.register_fn = &RegisterHubSettingsForClient;
    config.register_user_data = 0;
    config.should_force_attach_failure_fn = &ShouldForceHubAttachFailureForClient;
    config.attach_failure_user_data = 0;
    g_modHubClient.SetConfig(config);
}

static void StartModHubClient()
{
    const emc::ModHubClient::AttemptResult result = g_modHubClient.OnStartup();
    if (result == emc::ModHubClient::ATTACH_SUCCESS)
    {
        DebugLog("Wall-B-Gone INFO: event=mod_hub_attached use_hub_ui=1");
        return;
    }

    if (result == emc::ModHubClient::ATTACH_FAILED)
    {
        ErrorLog("Wall-B-Gone WARN: event=mod_hub_fallback reason=get_api_failed use_hub_ui=0");
    }
    else if (result == emc::ModHubClient::REGISTRATION_FAILED)
    {
        ErrorLog("Wall-B-Gone WARN: event=mod_hub_fallback reason=register_mod_or_setting_failed use_hub_ui=0");
    }
    else
    {
        ErrorLog("Wall-B-Gone WARN: event=mod_hub_fallback reason=invalid_client_configuration use_hub_ui=0");
    }
}

static void OnOptionsWindowInitForModHub()
{
    g_modHubClient.OnOptionsWindowInit();
}

static bool ShouldUseHubUiFromModHub()
{
    return g_modHubClient.UseHubUi();
}

extern "C" __declspec(dllexport) void __cdecl WallBGone_Test_ModHub_SetAttachFailureMode(int32_t mode)
{
    if (mode < kHubAttachFailureModeNone || mode > kHubAttachFailureModeAlways)
    {
        mode = kHubAttachFailureModeNone;
    }

    g_modHubAttachFailureMode = mode;
    ConfigureModHubClient();
}

extern "C" __declspec(dllexport) void __cdecl WallBGone_Test_ModHub_SetRegisterMode(int32_t mode)
{
    if (mode < kHubRegisterModeNormal || mode > kHubRegisterModeFail)
    {
        mode = kHubRegisterModeNormal;
    }

    g_modHubRegisterMode = mode;
    ConfigureModHubClient();
}

extern "C" __declspec(dllexport) void __cdecl WallBGone_Test_ModHub_ResetClientState()
{
    g_modEnabled = true;
    g_sleepingBagDismantleEnabled = true;
    g_hotkeyPrimary = kDefaultHotkey;
    g_pendingHotkeyPrimary = kDefaultHotkey;
    SyncNativeBindingFromHotkey();
    RefreshHotkeyUiWidgets();

    g_modHubAttachFailureMode = kHubAttachFailureModeNone;
    g_modHubRegisterMode = kHubRegisterModeNormal;
    g_modHubClient.Reset();
    ConfigureModHubClient();
}

extern "C" __declspec(dllexport) void __cdecl WallBGone_Test_ModHub_RunStartupAttach()
{
    g_modHubClient.Reset();
    ConfigureModHubClient();
    StartModHubClient();
}

extern "C" __declspec(dllexport) void __cdecl WallBGone_Test_ModHub_OnOptionsWindowInit()
{
    OnOptionsWindowInitForModHub();
}

extern "C" __declspec(dllexport) int32_t __cdecl WallBGone_Test_ModHub_UseHubUi()
{
    return g_modHubClient.UseHubUi() ? 1 : 0;
}

extern "C" __declspec(dllexport) int32_t __cdecl WallBGone_Test_ModHub_IsAttachRetryPending()
{
    return g_modHubClient.IsAttachRetryPending() ? 1 : 0;
}

extern "C" __declspec(dllexport) int32_t __cdecl WallBGone_Test_ModHub_HasAttachRetryAttempted()
{
    return g_modHubClient.HasAttachRetryAttempted() ? 1 : 0;
}

extern "C" __declspec(dllexport) int __cdecl WallBGone_GetRuntimeStateV1(WallBGoneRuntimeStateV1* out_state)
{
    if (!out_state)
    {
        return 1;
    }

    EnsureRuntimeHotkeyValid();
    out_state->enabled = g_modEnabled ? 1 : 0;
    out_state->sleeping_bag_dismantle_enabled = g_sleepingBagDismantleEnabled ? 1 : 0;
    out_state->hotkey_keycode = static_cast<int32_t>(g_hotkeyPrimary);
    out_state->hotkey_modifiers = 0u;
    if (g_hotkeyRequireCtrl)
    {
        out_state->hotkey_modifiers |= kHotkeyModifierCtrlMask;
    }
    if (g_hotkeyRequireShift)
    {
        out_state->hotkey_modifiers |= kHotkeyModifierShiftMask;
    }
    if (g_hotkeyRequireAlt)
    {
        out_state->hotkey_modifiers |= kHotkeyModifierAltMask;
    }
    return 0;
}

extern "C" __declspec(dllexport) int __cdecl WallBGone_SetRuntimeStateV1(
    const WallBGoneRuntimeStateV1* state,
    char* err_buf,
    uint32_t err_buf_size)
{
    if (!state)
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "missing_state");
        return 1;
    }

    if ((state->enabled != 0 && state->enabled != 1)
        || (state->sleeping_bag_dismantle_enabled != 0 && state->sleeping_bag_dismantle_enabled != 1))
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "invalid_bool");
        return 1;
    }

    const uint32_t supportedModifierMask = kHotkeyModifierCtrlMask | kHotkeyModifierShiftMask | kHotkeyModifierAltMask;
    if ((state->hotkey_modifiers & ~supportedModifierMask) != 0u)
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "invalid_modifiers");
        return 1;
    }

    const OIS::KeyCode requestedHotkey = static_cast<OIS::KeyCode>(state->hotkey_keycode);
    std::string validationReason;
    if (ValidateHotkey(requestedHotkey, &validationReason) != HotkeyValidation_Ok)
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "invalid_hotkey");
        return 1;
    }

    g_modEnabled = state->enabled != 0;
    g_sleepingBagDismantleEnabled = state->sleeping_bag_dismantle_enabled != 0;
    g_hotkeyPrimary = requestedHotkey;
    g_pendingHotkeyPrimary = requestedHotkey;
    g_hotkeyRequireCtrl = (state->hotkey_modifiers & kHotkeyModifierCtrlMask) != 0u;
    g_hotkeyRequireShift = (state->hotkey_modifiers & kHotkeyModifierShiftMask) != 0u;
    g_hotkeyRequireAlt = (state->hotkey_modifiers & kHotkeyModifierAltMask) != 0u;
    SyncNativeBindingFromHotkey();
    RefreshHotkeyUiWidgets();

    if (!SaveConfigState())
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "persist_failed");
        return 2;
    }

    return 0;
}

static DataPanelLine* TryCreateNativeKeybindRow(
    DatapanelGUI* panel,
    int tabID,
    std::string* bindingPtr)
{
    if (!g_fnCreateKeyConfigLine || !panel || !bindingPtr)
    {
        return 0;
    }

    const int candidateTabIds[3] = { tabID, 6, 1 };
    NativeStringObj nativeLabel;
    NativeStringObj nativeBinding;
    InitNativeStringObj(&nativeLabel, kHotkeyNativeLabel);
    InitNativeStringObj(&nativeBinding, *bindingPtr);
    for (int i = 0; i < 3; ++i)
    {
        const int nativeTabId = candidateTabIds[i];
        DataPanelLine* keyLine = 0;

        if (g_fnCreateKeyConfigLineWrapper)
        {
            __try
            {
                keyLine = g_fnCreateKeyConfigLineWrapper(
                    panel,
                    &nativeLabel,
                    &nativeBinding,
                    nativeTabId);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                keyLine = 0;
            }
            if (keyLine)
            {
                return keyLine;
            }
        }

        // Keep native/legacy probes disabled here: they have incompatible signatures
        // and crash immediately; wrapper path is the closest verified game call shape.
    }

    return 0;
}

static void CreateHotkeyModifierOptionLines(DatapanelGUI* panel, int tabID, ToolTip* tooltip)
{
    if (panel == 0 || g_fnCreateCheckboxLine == 0)
    {
        return;
    }

    DataPanelLine_CheckBox* requireCtrlLine = g_fnCreateCheckboxLine(panel, "   Require Ctrl", g_hotkeyRequireCtrl, tabID);
    if (requireCtrlLine && tooltip)
    {
        requireCtrlLine->setTooltip("Require Ctrl to be held with the dismantle hotkey.", tooltip);
    }

    DataPanelLine_CheckBox* requireShiftLine = g_fnCreateCheckboxLine(panel, "   Require Shift", g_hotkeyRequireShift, tabID);
    if (requireShiftLine && tooltip)
    {
        requireShiftLine->setTooltip("Require Shift to be held with the dismantle hotkey.", tooltip);
    }

    DataPanelLine_CheckBox* requireAltLine = g_fnCreateCheckboxLine(panel, "   Require Alt", g_hotkeyRequireAlt, tabID);
    if (requireAltLine && tooltip)
    {
        requireAltLine->setTooltip("Require Alt to be held with the dismantle hotkey.", tooltip);
    }
}

static void OptionsWindowInitHook(OptionsWindow* self)
{
    if (g_fnOptionsInitOrig)
    {
        g_fnOptionsInitOrig(self);
    }

    if (!self || !self->optionsTab || !g_ptrKenshiGUI || !g_fnCreateDatapanel || !g_fnCreateCheckboxLine)
    {
        return;
    }

    OnOptionsWindowInitForModHub();
    if (ShouldUseHubUiFromModHub())
    {
        return;
    }

    if (self->optionsTab->findItemWith(kWallBGoneTabName))
    {
        return;
    }

    MyGUI::TabItem* pluginOptionTab = self->optionsTab->addItem(kWallBGoneTabName);
    if (!pluginOptionTab)
    {
        ErrorLog("Wall-B-Gone: failed to add Wall-B-Gone tab item");
        return;
    }

    DatapanelGUI* pluginOptionPanel = g_fnCreateDatapanel(g_ptrKenshiGUI, kWallBGonePanelName, pluginOptionTab, false);
    if (!pluginOptionPanel)
    {
        ErrorLog("Wall-B-Gone: failed to create wall_b_gone_options datapanel");
        return;
    }

    const int tabID = kWallBGonePanelLineId;
    pluginOptionPanel->vfunc0xc0(tabID);
    pluginOptionPanel->vfunc0xe0(25.0f);

    if (g_fnCreateHeaderLine)
    {
        g_fnCreateHeaderLine(pluginOptionPanel, "[Wall-B-Gone Settings]", "", tabID, false, true);
    }

    DataPanelLine_CheckBox* toggleLine = g_fnCreateCheckboxLine(pluginOptionPanel, "   Enable Wall-B-Gone", g_modEnabled, tabID);
    if (toggleLine && self->tooltip)
    {
        toggleLine->setTooltip("Enable or disable Wall-B-Gone hotkey behavior.", self->tooltip);
    }

    DataPanelLine_CheckBox* sleepingBagToggleLine = g_fnCreateCheckboxLine(
        pluginOptionPanel,
        "   Enable sleeping bag dismantle",
        g_sleepingBagDismantleEnabled,
        tabID);
    if (sleepingBagToggleLine && self->tooltip)
    {
        sleepingBagToggleLine->setTooltip(
            "Allow hotkey dismantle for sleeping bags and compatible medical variants. Occupied bags are always protected.",
            self->tooltip);
    }

    CreateHotkeyModifierOptionLines(pluginOptionPanel, tabID, self->tooltip);

    g_hotkeyRebindButton = 0;
    g_hotkeyResetButton = 0;
    g_hotkeyLabelWidget = 0;
    g_nativeHotkeyBindingActive = false;

    MyGUI::Widget* panelWidget = pluginOptionPanel->getWidget();

    if (g_fnCreateKeyConfigLine)
    {
        SyncNativeBindingFromHotkey();
        DataPanelLine* keyLine = TryCreateNativeKeybindRow(
            pluginOptionPanel,
            tabID,
            &g_hotkeyNativeBinding);

        if (keyLine)
        {
            g_nativeHotkeyBindingActive = true;
            if (self->tooltip)
            {
                keyLine->setTooltip(
                    "Click and press the primary key for Wall-B-Gone. Use the Require Ctrl/Shift/Alt toggles above to build combos.",
                    self->tooltip);
            }
        }
        else if (!keyLine)
        {
            ErrorLog("Wall-B-Gone: failed to create native keybind row");
            CreateFallbackKeybindControls(panelWidget);
        }
    }
    else
    {
        CreateFallbackKeybindControls(panelWidget);
    }

    pluginOptionTab->setVisible(false);
    self->optionsTab->setItemData(pluginOptionTab, pluginOptionPanel);
}

static void OptionsWindowSaveHook(OptionsWindow* self)
{
    if (g_fnOptionsSaveOrig)
    {
        g_fnOptionsSaveOrig(self);
    }

    if (g_nativeHotkeyBindingActive)
    {
        OIS::KeyCode parsedKey = OIS::KC_UNASSIGNED;
        if (TryParseKeyCode(g_hotkeyNativeBinding, &parsedKey))
        {
            std::string reason;
            if (ValidateHotkey(parsedKey, &reason) == HotkeyValidation_Ok)
            {
                g_hotkeyPrimary = parsedKey;
                g_pendingHotkeyPrimary = parsedKey;
                SyncNativeBindingFromHotkey();
            }
            else
            {
                ErrorLog("Wall-B-Gone: native keybind value rejected by validation; keeping previous key");
                SyncNativeBindingFromHotkey();
            }
        }
        else
        {
            ErrorLog("Wall-B-Gone: native keybind value parse failed; keeping previous key");
            SyncNativeBindingFromHotkey();
        }
    }
    else
    {
        // Fallback capture path updates g_hotkeyPrimary directly.
        g_pendingHotkeyPrimary = g_hotkeyPrimary;
        SyncNativeBindingFromHotkey();
    }

    SaveConfigState();
}

// Helper function to check internal buildings safely (SEH wrapper)
// Returns true if there are internal buildings or if an error occurs
static bool CheckInternalBuildingsSafely(Building* b)
{
    bool hasInternal = false;
    __try
    {
        if (b->getNumInternalBuildings() > 0)
        {
            hasInternal = true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DebugLog("Hotkey action: CRASH AVERTED in CheckInternalBuildingsSafely");
        hasInternal = true;
    }
    return hasInternal;
}

static bool UnsafeCheckMounted(Building* b)
{
    lektor<Building*> mountedBuildings;
    int numMounted = b->getMountedBuildings(&mountedBuildings);
    return (numMounted > 0);
}

static bool CheckMountedBuildingsSafely(Building* b)
{
    bool hasMounted = false;
    __try
    {
        if (UnsafeCheckMounted(b))
        {
            hasMounted = true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DebugLog("Hotkey action: CRASH AVERTED in CheckMountedBuildingsSafely");
        hasMounted = true;
    }
    return hasMounted;
}

static char ToLowerAsciiChar(char value)
{
    if (value >= 'A' && value <= 'Z')
    {
        return static_cast<char>(value + ('a' - 'A'));
    }

    return value;
}

static bool ContainsAsciiInsensitive(const std::string& haystack, const char* needle)
{
    if (!needle || needle[0] == '\0')
    {
        return false;
    }

    const size_t needleLen = std::strlen(needle);
    if (needleLen > haystack.size())
    {
        return false;
    }

    for (size_t start = 0; start + needleLen <= haystack.size(); ++start)
    {
        size_t i = 0;
        for (; i < needleLen; ++i)
        {
            if (ToLowerAsciiChar(haystack[start + i]) != ToLowerAsciiChar(needle[i]))
            {
                break;
            }
        }

        if (i == needleLen)
        {
            return true;
        }
    }

    return false;
}

static bool IsBaseSleepingBagText(const std::string& value)
{
    return ContainsAsciiInsensitive(value, "sleeping bag")
        || ContainsAsciiInsensitive(value, "sleeping-bag")
        || ContainsAsciiInsensitive(value, "sleeping_bag")
        || ContainsAsciiInsensitive(value, "sleepingbag")
        || ContainsAsciiInsensitive(value, "camp bed")
        || ContainsAsciiInsensitive(value, "camp_bed")
        || ContainsAsciiInsensitive(value, "campbed")
        || ContainsAsciiInsensitive(value, "bedroll");
}

static bool IsMedicalSleepingBagText(const std::string& value)
{
    const bool hasMedical = ContainsAsciiInsensitive(value, "medical");
    const bool hasSleepBagHint = ContainsAsciiInsensitive(value, "sleep")
        || ContainsAsciiInsensitive(value, "bag")
        || ContainsAsciiInsensitive(value, "bedroll")
        || ContainsAsciiInsensitive(value, "camp");

    if (hasMedical && hasSleepBagHint)
    {
        return true;
    }

    // Compatibility IDs used by some sleeping-bag mods.
    return ContainsAsciiInsensitive(value, "medicalbed")
        || ContainsAsciiInsensitive(value, "medical_bed")
        || ContainsAsciiInsensitive(value, "advancedmedicalbed")
        || ContainsAsciiInsensitive(value, "advanced_medical_bed");
}

static bool IsSleepingBagGameData(const GameData* data)
{
    if (!data)
    {
        return false;
    }

    const std::string& name = data->name;
    const std::string& stringId = data->stringID;

    return IsBaseSleepingBagText(name) || IsBaseSleepingBagText(stringId);
}

static bool IsOutsideFurnitureSafely(Building* b)
{
    bool isOutsideFurniture = false;
    __try
    {
        isOutsideFurniture = b->getIsOutsideFurniture();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DebugLog("Hotkey action: CRASH AVERTED in IsOutsideFurnitureSafely");
        isOutsideFurniture = false;
    }

    return isOutsideFurniture;
}

static bool IsSleepingBagBuilding(Building* b)
{
    if (!b)
    {
        return false;
    }

    if (b->getSpecialFunction() != BF_BED)
    {
        return false;
    }

    const GameData* data = b->getGameData();
    if (IsSleepingBagGameData(data))
    {
        return true;
    }

    if (!data)
    {
        return false;
    }

    const bool isMedicalSleepingBag = IsMedicalSleepingBagText(data->name) || IsMedicalSleepingBagText(data->stringID);
    if (!isMedicalSleepingBag)
    {
        return false;
    }

    // Keep medical-bed matching scoped to camp-style (outside) furniture.
    return IsOutsideFurnitureSafely(b);
}

static bool CheckSleepingBagOccupiedSafely(const hand& sleepingBagHandle)
{
    bool occupied = false;
    __try
    {
        if (!ou)
        {
            occupied = true;
        }
        else
        {
            const ogre_unordered_set<Character*>::type& characters = ou->getCharacterUpdateList();
            for (ogre_unordered_set<Character*>::type::const_iterator it = characters.begin(); it != characters.end(); ++it)
            {
                Character* character = *it;
                if (!character)
                {
                    continue;
                }

                if (character->inSomething != IN_BED)
                {
                    continue;
                }

                if (character->inWhat == sleepingBagHandle)
                {
                    occupied = true;
                    break;
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DebugLog("Hotkey action: CRASH AVERTED in CheckSleepingBagOccupiedSafely");
        occupied = true;
    }

    return occupied;
}

static bool PerformDismantleLogic(Building* b, const hand& sel)
{
    b->dropMats();
    ou->dynamicDestroyBuilding(sel);
    return true;
}

static bool SafelyDismantleTarget(Building* b, const hand& sel)
{
    bool success = false;
    __try
    {
        success = PerformDismantleLogic(b, sel);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DebugLog("Hotkey action: CRASH AVERTED during dismantle!");
        success = false;
    }
    return success;
}

static void HandleHotkeyAction()
{
    if (g_hotkeyCaptureState == HotkeyCapture_AwaitKey)
    {
        return;
    }

    if (!g_modEnabled)
        return;

    EnsureRuntimeHotkeyValid();

    if (!key || !ou || !ou->player)
        return;

    bool hotkeyDown = false;
    bool modifiersSatisfied = false;
    if (key->keyboard)
    {
        hotkeyDown = key->keyboard->isKeyDown(g_hotkeyPrimary);
        modifiersSatisfied = true;

        if (g_hotkeyRequireCtrl && !IsModifierDown(key->keyboard, OIS::KC_LCONTROL))
        {
            modifiersSatisfied = false;
        }

        if (g_hotkeyRequireShift && !IsModifierDown(key->keyboard, OIS::KC_LSHIFT))
        {
            modifiersSatisfied = false;
        }

        if (g_hotkeyRequireAlt && !IsModifierDown(key->keyboard, OIS::KC_LMENU))
        {
            modifiersSatisfied = false;
        }
    }

    const bool hotkeyActive = hotkeyDown && modifiersSatisfied;
    const bool pressedThisFrame = (hotkeyActive && !g_prevHotkeyDown);
    g_prevHotkeyDown = hotkeyActive;

    if (!pressedThisFrame)
        return;

    const hand& sel = ou->player->selectedObject;

    Building* b = sel.getBuilding();
    if (b)
    {
        const bool isWallTarget = (b->isAWall() != 0);
        const bool isSleepingBagTarget = (g_sleepingBagDismantleEnabled && IsSleepingBagBuilding(b));

        if (!isWallTarget && !isSleepingBagTarget)
        {
            return;
        }

        if (b->canDismantle())
        {
            if (isWallTarget)
            {
                bool hasAttachedBuildings = false;

                if (CheckInternalBuildingsSafely(b))
                {
                    hasAttachedBuildings = true;
                }

                if (!hasAttachedBuildings)
                {
                    if (CheckMountedBuildingsSafely(b))
                    {
                        hasAttachedBuildings = true;
                    }
                }

                if (hasAttachedBuildings)
                {
                    return;
                }
            }
            else
            {
                if (CheckSleepingBagOccupiedSafely(b->getHandle()))
                {
                    return;
                }
            }

            DWORD currentTime = GetTickCount();
            if (g_lastFailedDismantleTime != 0)
            {
                DWORD timeSinceLastFailure = currentTime - g_lastFailedDismantleTime;
                if (timeSinceLastFailure < DISMANTLE_COOLDOWN_MS)
                {
                    return;
                }
            }

            __try
            {
                Building::ConstructionState* buildState = b->getBuildState();

                if (buildState && !buildState->isComplete)
                {
                }
                else
                {
                    bool dismantleResult = SafelyDismantleTarget(b, sel);

                    if (!dismantleResult)
                    {
                        if (isWallTarget)
                        {
                            DebugLog("Hotkey action: Dismantle failed - wall may be connected to problematic structures");
                        }
                        else
                        {
                            DebugLog("Hotkey action: Dismantle failed - sleeping bag may be in an invalid state");
                        }

                        g_lastFailedDismantleTime = GetTickCount();
                    }
                    else
                    {
                        g_lastFailedDismantleTime = 0;
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                if (isWallTarget)
                {
                    DebugLog("Hotkey action: CRASH AVERTED in outer dismantle wrapper!");
                }
                else
                {
                    DebugLog("Hotkey action: CRASH AVERTED while dismantling sleeping bag");
                }

                g_lastFailedDismantleTime = GetTickCount();
            }
        }
        return;
    }

    RootObject* ro = sel.getRootObject();
    if (ro)
    {
        return;
    }
}

static void (*PlayerInterface_updateUT_orig)(PlayerInterface*) = 0;
static void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    PlayerInterface_updateUT_orig(thisptr);
    HandleHotkeyAction();
}

static void (*InputHandler_keyDownEvent_orig)(InputHandler*, OIS::KeyCode) = 0;
static void InputHandler_keyDownEvent_hook(InputHandler* thisptr, OIS::KeyCode keyCode)
{
    if (g_hotkeyCaptureState == HotkeyCapture_AwaitKey)
    {
        if (keyCode == OIS::KC_ESCAPE)
        {
            EndHotkeyCapture();
            RefreshHotkeyUiWidgets();
            return;
        }

        std::string validationReason;
        const HotkeyValidationResult validationResult = ValidateHotkey(keyCode, &validationReason);
        if (validationResult != HotkeyValidation_Ok)
        {
            EndHotkeyCapture();
            RefreshHotkeyUiWidgets();
            return;
        }

        g_pendingHotkeyPrimary = keyCode;
        g_hotkeyPrimary = keyCode;
        SyncNativeBindingFromHotkey();
        EndHotkeyCapture();
        RefreshHotkeyUiWidgets();
        SaveConfigState();
        return;
    }

    InputHandler_keyDownEvent_orig(thisptr, keyCode);
}

__declspec(dllexport) void startPlugin()
{
    DebugLog("Wall-B-Gone: startPlugin()");

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
