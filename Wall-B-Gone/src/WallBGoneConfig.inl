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

static bool TryParseHotkeyBinding(
    const std::string& bindingText,
    OIS::KeyCode* outKey,
    bool* outRequireCtrl,
    bool* outRequireShift,
    bool* outRequireAlt)
{
    if (!outKey)
    {
        return false;
    }

    const std::string normalized = TrimAscii(bindingText);
    if (normalized.empty())
    {
        return false;
    }

    bool requireCtrl = false;
    bool requireShift = false;
    bool requireAlt = false;
    bool foundKey = false;
    OIS::KeyCode parsedKey = OIS::KC_UNASSIGNED;

    size_t tokenStart = 0;
    while (tokenStart <= normalized.size())
    {
        const size_t tokenEnd = normalized.find('+', tokenStart);
        const std::string token = ToUpperAscii(TrimAscii(
            normalized.substr(tokenStart, tokenEnd == std::string::npos ? std::string::npos : tokenEnd - tokenStart)));

        if (token.empty())
        {
            return false;
        }

        if (token == "CTRL" || token == "CONTROL" || token == "LCONTROL" || token == "RCONTROL")
        {
            requireCtrl = true;
        }
        else if (token == "SHIFT" || token == "LSHIFT" || token == "RSHIFT")
        {
            requireShift = true;
        }
        else if (token == "ALT" || token == "LMENU" || token == "RMENU" || token == "MENU")
        {
            requireAlt = true;
        }
        else
        {
            if (foundKey)
            {
                return false;
            }

            if (!TryParseKeyCode(token, &parsedKey))
            {
                return false;
            }

            foundKey = true;
        }

        if (tokenEnd == std::string::npos)
        {
            break;
        }

        tokenStart = tokenEnd + 1u;
    }

    if (!foundKey)
    {
        return false;
    }

    *outKey = parsedKey;
    if (outRequireCtrl)
    {
        *outRequireCtrl = requireCtrl;
    }
    if (outRequireShift)
    {
        *outRequireShift = requireShift;
    }
    if (outRequireAlt)
    {
        *outRequireAlt = requireAlt;
    }

    return true;
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
    g_hotkeyNativeBinding = FormatHotkeyBinding(
        g_hotkeyPrimary,
        g_hotkeyRequireCtrl,
        g_hotkeyRequireShift,
        g_hotkeyRequireAlt);
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
    if (!TryParseHotkeyBinding(hotkeyText, &parsedKey, 0, 0, 0))
    {
        std::stringstream warning;
        warning << "Wall-B-Gone WARN: failed to parse hotkey '" << hotkeyText << "' from config; using default '"
            << KeyCodeToName(kDefaultHotkey) << "'";
        WallBGoneDebugLog(warning.str().c_str());
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
        WallBGoneDebugLog(warning.str().c_str());
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
    bool* anyOwnBuildingOut,
    bool* debugLoggingOut,
    bool* hotkeyRequireCtrlOut,
    bool* hotkeyRequireShiftOut,
    bool* hotkeyRequireAltOut,
    OIS::KeyCode* hotkeyOut)
{
    if (!enabledOut
        || !sleepingBagDismantleEnabledOut
        || !anyOwnBuildingOut
        || !debugLoggingOut
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
    *anyOwnBuildingOut = ReadBoolKeyFromBody(body, "dismantleAnyOwnBuilding", false);
    *debugLoggingOut = ReadBoolKeyFromBody(body, "debugLogging", false);
    *hotkeyRequireCtrlOut = ReadHotkeyRequireCtrlFromBody(body);
    *hotkeyRequireShiftOut = ReadHotkeyRequireShiftFromBody(body);
    *hotkeyRequireAltOut = ReadHotkeyRequireAltFromBody(body);
    return ReadHotkeyFromBody(body, hotkeyOut);
}

static bool SaveConfigToFile(
    const std::string& configPath,
    bool enabled,
    bool sleepingBagDismantleEnabled,
    bool anyOwnBuilding,
    bool debugLogging,
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
    out << "  \"dismantleAnyOwnBuilding\": " << (anyOwnBuilding ? "true" : "false") << ",\n";
    out << "  \"dismantleDropItems\": " << (g_dismantleDropItems ? "true" : "false") << ",\n";
    out << "  \"debugLogging\": " << (debugLogging ? "true" : "false") << ",\n";
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
    g_dismantleAnyOwnBuilding = false;
    g_dismantleDropItems = false;
    g_debugLogging = false;
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
    bool loadedAnyOwnBuilding = false;
    bool loadedDebugLogging = false;
    bool loadedHotkeyRequireCtrl = kDefaultHotkeyRequireCtrl;
    bool loadedHotkeyRequireShift = kDefaultHotkeyRequireShift;
    bool loadedHotkeyRequireAlt = kDefaultHotkeyRequireAlt;
    OIS::KeyCode loadedHotkey = kDefaultHotkey;
    if (!ReadConfigFromFile(
        g_settingsPath,
        &loadedEnabled,
        &loadedSleepingBagDismantleEnabled,
        &loadedAnyOwnBuilding,
        &loadedDebugLogging,
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
    g_dismantleAnyOwnBuilding = loadedAnyOwnBuilding;
    {
        // Отдельным чтением, чтобы не менять подпись ReadConfigFromFile.
        std::ifstream in(g_settingsPath.c_str(), std::ios::in | std::ios::binary);
        if (in)
        {
            const std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            g_dismantleDropItems = ReadBoolKeyFromBody(body, "dismantleDropItems", false);
        }
    }
    g_debugLogging = loadedDebugLogging;
    g_hotkeyPrimary = loadedHotkey;
    g_pendingHotkeyPrimary = loadedHotkey;
    g_hotkeyRequireCtrl = loadedHotkeyRequireCtrl;
    g_hotkeyRequireShift = loadedHotkeyRequireShift;
    g_hotkeyRequireAlt = loadedHotkeyRequireAlt;
    SyncNativeBindingFromHotkey();

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
        g_dismantleAnyOwnBuilding,
        g_debugLogging,
        g_hotkeyRequireCtrl,
        g_hotkeyRequireShift,
        g_hotkeyRequireAlt,
        g_hotkeyPrimary))
    {
        ErrorLog("Wall-B-Gone: failed to save mod-config.json");
        return false;
    }

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
        WallBGoneDebugLog("Wall-B-Gone WARN: runtime hotkey unsupported; falling back to default");
        g_loggedRuntimeHotkeyFallback = true;
    }
}
