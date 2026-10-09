#include "vs_keybind.h"

#include <kenshi/Globals.h>
#include <kenshi/InputHandler.h>
#include <ois/OISKeyboard.h>

#include <cctype>
#include <sstream>

namespace vs_keybind
{
namespace
{
struct KeyNameEntry
{
    const char* name;
    OIS::KeyCode keyCode;
};

const KeyNameEntry kKeyNameMap[] = {
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
    { "LSHIFT", OIS::KC_LSHIFT }, { "RSHIFT", OIS::KC_RSHIFT }, { "SHIFT", OIS::KC_LSHIFT },
    { "LCONTROL", OIS::KC_LCONTROL }, { "RCONTROL", OIS::KC_RCONTROL }, { "CTRL", OIS::KC_LCONTROL },
    { "LALT", OIS::KC_LMENU }, { "RALT", OIS::KC_RMENU }, { "ALT", OIS::KC_LMENU },
    { "ESCAPE", OIS::KC_ESCAPE }, { "ESC", OIS::KC_ESCAPE }, { "LWIN", OIS::KC_LWIN }, { "RWIN", OIS::KC_RWIN },
    { "SYSRQ", OIS::KC_SYSRQ }, { "APPS", OIS::KC_APPS }
};

const size_t kKeyNameMapCount = sizeof(kKeyNameMap) / sizeof(kKeyNameMap[0]);

bool IsCtrlKeyCode(int32_t keyCode)
{
    return keyCode == static_cast<int32_t>(OIS::KC_LCONTROL)
        || keyCode == static_cast<int32_t>(OIS::KC_RCONTROL);
}

bool IsShiftKeyCode(int32_t keyCode)
{
    return keyCode == static_cast<int32_t>(OIS::KC_LSHIFT)
        || keyCode == static_cast<int32_t>(OIS::KC_RSHIFT);
}

bool IsAltKeyCode(int32_t keyCode)
{
    return keyCode == static_cast<int32_t>(OIS::KC_LMENU)
        || keyCode == static_cast<int32_t>(OIS::KC_RMENU);
}

std::string TrimAscii(const std::string& value)
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

std::string ToUpperAscii(const std::string& value)
{
    std::string upper = value;
    for (size_t i = 0; i < upper.size(); ++i)
    {
        upper[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(upper[i])));
    }
    return upper;
}

bool IsSupportedKeyCode(int32_t keyCode)
{
    if (keyCode == kKeyCodeUnbound)
    {
        return true;
    }

    for (size_t i = 0; i < kKeyNameMapCount; ++i)
    {
        if (keyCode == static_cast<int32_t>(kKeyNameMap[i].keyCode))
        {
            return true;
        }
    }

    return false;
}

bool IsModifierDown(OIS::Keyboard* keyboard, int32_t keyCode)
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

bool IsKeyDown(OIS::Keyboard* keyboard, int32_t keyCode)
{
    if (keyboard == 0)
    {
        return false;
    }

    if (keyCode == kKeyCodeUnbound)
    {
        return true;
    }

    if (IsCtrlKeyCode(keyCode) || IsShiftKeyCode(keyCode) || IsAltKeyCode(keyCode))
    {
        return IsModifierDown(keyboard, keyCode);
    }

    return keyboard->isKeyDown(static_cast<OIS::KeyCode>(keyCode));
}
} // namespace

const int32_t kKeyCodeUnbound = -1;
const int32_t kDefaultHighlightKeyCode = static_cast<int32_t>(OIS::KC_LMENU);

bool TryParseKeyCode(const std::string& rawValue, int32_t* keyCodeOut)
{
    if (keyCodeOut == 0)
    {
        return false;
    }

    const std::string normalized = ToUpperAscii(TrimAscii(rawValue));
    if (normalized.empty())
    {
        return false;
    }

    if (normalized == "UNBOUND" || normalized == "NONE")
    {
        *keyCodeOut = kKeyCodeUnbound;
        return true;
    }

    for (size_t i = 0; i < kKeyNameMapCount; ++i)
    {
        if (normalized == kKeyNameMap[i].name)
        {
            *keyCodeOut = static_cast<int32_t>(kKeyNameMap[i].keyCode);
            return true;
        }
    }

    return false;
}

bool ValidatePrimaryKeyCode(int32_t keyCode, std::string* reasonOut)
{
    if (!IsSupportedKeyCode(keyCode))
    {
        if (reasonOut != 0)
        {
            *reasonOut = "unsupported key";
        }
        return false;
    }

    if (keyCode == kKeyCodeUnbound)
    {
        if (reasonOut != 0)
        {
            reasonOut->clear();
        }
        return true;
    }

    if (keyCode == static_cast<int32_t>(OIS::KC_ESCAPE)
        || keyCode == static_cast<int32_t>(OIS::KC_LWIN)
        || keyCode == static_cast<int32_t>(OIS::KC_RWIN)
        || keyCode == static_cast<int32_t>(OIS::KC_SYSRQ))
    {
        if (reasonOut != 0)
        {
            *reasonOut = "system-reserved key";
        }
        return false;
    }

    if (keyCode == static_cast<int32_t>(OIS::KC_APPS))
    {
        if (reasonOut != 0)
        {
            *reasonOut = "reserved key";
        }
        return false;
    }

    if (reasonOut != 0)
    {
        reasonOut->clear();
    }
    return true;
}

std::string KeyCodeToConfigString(int32_t keyCode)
{
    if (keyCode == kKeyCodeUnbound)
    {
        return "UNBOUND";
    }

    if (IsCtrlKeyCode(keyCode))
    {
        return "CTRL";
    }

    if (IsShiftKeyCode(keyCode))
    {
        return "SHIFT";
    }

    if (IsAltKeyCode(keyCode))
    {
        return "ALT";
    }

    for (size_t i = 0; i < kKeyNameMapCount; ++i)
    {
        if (keyCode == static_cast<int32_t>(kKeyNameMap[i].keyCode))
        {
            return kKeyNameMap[i].name;
        }
    }

    return "UNKNOWN";
}

std::string FormatKeybind(const PluginConfig& config)
{
    if (config.highlightKeyCode == kKeyCodeUnbound)
    {
        return "UNBOUND";
    }

    std::stringstream ss;
    bool wrotePrefix = false;

    if (config.highlightKeyRequireCtrl && !IsCtrlKeyCode(config.highlightKeyCode))
    {
        ss << "CTRL";
        wrotePrefix = true;
    }
    if (config.highlightKeyRequireShift && !IsShiftKeyCode(config.highlightKeyCode))
    {
        if (wrotePrefix)
        {
            ss << "+";
        }
        ss << "SHIFT";
        wrotePrefix = true;
    }
    if (config.highlightKeyRequireAlt && !IsAltKeyCode(config.highlightKeyCode))
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
    ss << KeyCodeToConfigString(config.highlightKeyCode);
    return ss.str();
}

bool IsHighlightGateOpen(const PluginConfig& config)
{
    if (config.highlightKeyCode == kKeyCodeUnbound)
    {
        return true;
    }

    if (key == 0 || key->keyboard == 0)
    {
        return false;
    }

    OIS::Keyboard* keyboard = key->keyboard;
    if (!IsKeyDown(keyboard, config.highlightKeyCode))
    {
        return false;
    }

    if (config.highlightKeyRequireCtrl && !IsModifierDown(keyboard, static_cast<int32_t>(OIS::KC_LCONTROL)))
    {
        return false;
    }

    if (config.highlightKeyRequireShift && !IsModifierDown(keyboard, static_cast<int32_t>(OIS::KC_LSHIFT)))
    {
        return false;
    }

    if (config.highlightKeyRequireAlt && !IsModifierDown(keyboard, static_cast<int32_t>(OIS::KC_LMENU)))
    {
        return false;
    }

    return true;
}

} // namespace vs_keybind
