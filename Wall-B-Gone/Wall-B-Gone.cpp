#include <Debug.h>

#include <core/Functions.h>

#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/InputHandler.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/Building.h>
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
#include <cctype>
#include <Windows.h>
#include <fstream>
#include <stdint.h>

// Track last failed dismantle to prevent rapid retries causing game corruption
static DWORD g_lastFailedDismantleTime = 0;
static const DWORD DISMANTLE_COOLDOWN_MS = 1000;

static const OIS::KeyCode kDefaultHotkey = OIS::KC_X;

// --- Hotkey state (edge detect) ---
static OIS::KeyCode g_hotkeyPrimary = kDefaultHotkey;
static OIS::KeyCode g_pendingHotkeyPrimary = kDefaultHotkey;
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
static std::string g_settingsPath;
static const char* kWallBGoneTabName = "Wall-B-Gone";
static const char* kWallBGonePanelName = "wall_b_gone_options";
static const int kWallBGonePanelLineId = 0x574247;
static const std::string kHotkeyNativeLabel = "Hotkey";
static std::string g_hotkeyNativeBinding = "X";

static bool SaveConfigState();
static const char* KeyCodeToName(OIS::KeyCode keyCode);
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
            caption << "Press key...";
        }
        else
        {
            caption << KeyCodeToName(g_hotkeyPrimary);
        }
        g_hotkeyRebindButton->setCaption(caption.str());
    }

    if (g_hotkeyResetButton)
    {
        std::stringstream caption;
        caption << "Reset (" << KeyCodeToName(kDefaultHotkey) << ")";
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
    const int y = 146;
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
        g_hotkeyRebindButton->setUserString("ToolTip", "Click then press a key.");
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
        g_hotkeyResetButton->setUserString("ToolTip", "Reset to default hotkey.");
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

static bool ReadConfigFromFile(const std::string& configPath, bool* enabledOut, OIS::KeyCode* hotkeyOut)
{
    if (!enabledOut || !hotkeyOut)
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
    return ReadHotkeyFromBody(body, hotkeyOut);
}

static bool SaveConfigToFile(const std::string& configPath, bool enabled, OIS::KeyCode hotkey)
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
    out << "  \"hotkey\": \"" << KeyCodeToName(hotkey) << "\"\n";
    out << "}\n";

    return true;
}

static void LoadConfigState()
{
    g_modEnabled = true;
    g_hotkeyPrimary = kDefaultHotkey;
    g_pendingHotkeyPrimary = kDefaultHotkey;

    if (g_settingsPath.empty())
    {
        return;
    }

    bool loadedEnabled = true;
    OIS::KeyCode loadedHotkey = kDefaultHotkey;
    if (!ReadConfigFromFile(g_settingsPath, &loadedEnabled, &loadedHotkey))
    {
        ErrorLog("Wall-B-Gone ERROR: failed to read mod-config.json; using defaults");
        return;
    }

    g_modEnabled = loadedEnabled;
    g_hotkeyPrimary = loadedHotkey;
    g_pendingHotkeyPrimary = loadedHotkey;
    SyncNativeBindingFromHotkey();

    std::stringstream info;
    info << "Wall-B-Gone INFO: loaded config enabled=" << (g_modEnabled ? "true" : "false")
        << " hotkey=" << KeyCodeToName(g_hotkeyPrimary);
    DebugLog(info.str().c_str());
}

static bool SaveConfigState()
{
    if (g_settingsPath.empty())
    {
        ErrorLog("Wall-B-Gone: settings path is empty; cannot save mod-config.json");
        return false;
    }

    if (!SaveConfigToFile(g_settingsPath, g_modEnabled, g_hotkeyPrimary))
    {
        ErrorLog("Wall-B-Gone: failed to save mod-config.json");
        return false;
    }

    std::stringstream info;
    info << "Wall-B-Gone INFO: saved config enabled=" << (g_modEnabled ? "true" : "false")
        << " hotkey=" << KeyCodeToName(g_hotkeyPrimary);
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

    g_hotkeyRebindButton = 0;
    g_hotkeyResetButton = 0;
    g_hotkeyLabelWidget = 0;

    MyGUI::Widget* panelWidget = pluginOptionPanel->getWidget();

    if (g_fnCreateKeyConfigLine)
    {
        SyncNativeBindingFromHotkey();
        DataPanelLine* keyLine = TryCreateNativeKeybindRow(
            pluginOptionPanel,
            tabID,
            &g_hotkeyNativeBinding);

        if (keyLine && self->tooltip)
        {
            keyLine->setTooltip("Click and press a key to bind Wall-B-Gone dismantle hotkey.", self->tooltip);
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

static bool PerformDismantleLogic(Building* b, const hand& sel)
{
    b->dropMats();
    ou->dynamicDestroyBuilding(sel);
    return true;
}

static bool SafelyDismantleWall(Building* b, const hand& sel)
{
    bool success = false;
    __try
    {
        success = PerformDismantleLogic(b, sel);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DebugLog("Hotkey action: CRASH AVERTED during dismantle! Wall may be connected to problematic structures.");
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
    if (key->keyboard)
    {
        hotkeyDown = key->keyboard->isKeyDown(g_hotkeyPrimary);
    }

    const bool pressedThisFrame = (hotkeyDown && !g_prevHotkeyDown);
    g_prevHotkeyDown = hotkeyDown;

    if (!pressedThisFrame)
        return;

    const hand& sel = ou->player->selectedObject;

    Building* b = sel.getBuilding();
    if (b)
    {
        if (b->isAWall())
        {
            if (b->canDismantle())
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
                        bool dismantleResult = SafelyDismantleWall(b, sel);

                        if (!dismantleResult)
                        {
                            DebugLog("Hotkey action: Dismantle failed - wall may be connected to problematic structures");
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
                    DebugLog("Hotkey action: CRASH AVERTED in outer dismantle wrapper!");
                    g_lastFailedDismantleTime = GetTickCount();
                }
            }
            else
            {
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
