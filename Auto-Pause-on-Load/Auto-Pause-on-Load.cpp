#include <Debug.h>

#include <core/Functions.h>

#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/Kenshi.h>
#include <kenshi/Character.h>
#include <kenshi/Dialogue.h>
#include <kenshi/Inventory.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/SaveManager.h>
#include <Windows.h>

#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

struct PluginConfig
{
    bool enabled;
    DWORD pauseDebounceMs;
    bool debugLogTransitions;
    bool pauseOnTrade;
    bool resumeAfterTrade;
    bool pauseOnInventoryOpen;
    bool resumeAfterInventoryClose;
};

struct RuntimeState
{
    bool loadInProgress;
    bool pauseArmed;
    bool loadSignalSeenAfterArm;
    bool saveLoadIntentPending;
    DWORD armTimestampMs;
    DWORD saveLoadIntentTimestampMs;
    DWORD lastPauseMs;
    DWORD lastTickAliveLogMs;
    bool loggedWorldUnavailable;
    bool tradeActive;
    bool inventoryActive;
    bool tradePausedByPlugin;
    bool inventoryPausedByPlugin;
    bool tradeWasPausedBeforeStart;
    bool inventoryWasPausedBeforeStart;
};

static const char* kPluginName = "Auto-Pause-on-Load";
static const char* kConfigFileName = "mod-config.json";
static const DWORD kTickAliveIntervalMs = 5000;
static const DWORD kNoSignalDisarmMs = 1500;
static const DWORD kArmedTimeoutMs = 60000;
static const DWORD kSaveLoadIntentTimeoutMs = 5000;
static const DWORD kPauseDebounceMsMin = 0;
static const DWORD kPauseDebounceMsMax = 600000;
static PluginConfig g_config = { true, 2000, false, true, true, true, true };
static RuntimeState g_state = { false, false, false, false, 0, 0, 0, 0, false, false, false, false, false, false, false };

static std::string g_settingsPath;
static bool g_configNeedsWriteBack = false;
static bool g_hasObservedLoadSignal = false;
static bool g_lastObservedLoadSignal = false;
static bool g_hasObservedSaveManagerSignal = false;
static int g_lastObservedSaveManagerSignal = 0;

static void (*PlayerInterface_updateUT_orig)(PlayerInterface*) = 0;
static bool g_hooksInstalled = false;
static bool g_updateUTHookVerified = false;
static DWORD g_updateUTHookVerifyTimeMs = 0;

static void PlayerInterface_updateUT_hook(PlayerInterface* thisptr);

static bool IsSupportedVersion(unsigned int platform, const std::string& version);
static bool ResolveSupportedRuntimeNoSeh(unsigned int* out_platform, std::string* out_version);
static bool ResolveSupportedRuntime(unsigned int* out_platform, std::string* out_version);
static bool DebounceWindowElapsed(DWORD nowMs, DWORD lastEventMs, DWORD minGapMs);
static void DisarmPauseAfterLoad();
static void ResetUiTracking();
static const char* DescribeSaveManagerSignal(int signal);
static bool QuerySaveManagerState(
    bool* loadRequestedOut,
    int* signalOut,
    int* delayOut,
    std::string* currentGameOut,
    std::string* requestedNameOut);
static void LogLoadInvestigation(
    const char* stage,
    bool isLoadingSave,
    int saveManagerSignal,
    int saveManagerDelay,
    const std::string* currentGame,
    const std::string* requestedName);

struct ConfigParseDiagnostics
{
    bool foundEnabled;
    bool invalidEnabled;
    bool foundPauseDebounceMs;
    bool invalidPauseDebounceMs;
    bool clampedPauseDebounceMs;
    bool foundDebugLogTransitions;
    bool invalidDebugLogTransitions;
    bool foundPauseOnTrade;
    bool invalidPauseOnTrade;
    bool foundResumeAfterTrade;
    bool invalidResumeAfterTrade;
    bool foundPauseOnInventoryOpen;
    bool invalidPauseOnInventoryOpen;
    bool foundResumeAfterInventoryClose;
    bool invalidResumeAfterInventoryClose;
    bool syntaxError;
    size_t syntaxErrorOffset;
};

static void ResetConfigParseDiagnostics(ConfigParseDiagnostics* diagnostics)
{
    if (!diagnostics)
    {
        return;
    }

    diagnostics->foundEnabled = false;
    diagnostics->invalidEnabled = false;
    diagnostics->foundPauseDebounceMs = false;
    diagnostics->invalidPauseDebounceMs = false;
    diagnostics->clampedPauseDebounceMs = false;
    diagnostics->foundDebugLogTransitions = false;
    diagnostics->invalidDebugLogTransitions = false;
    diagnostics->foundPauseOnTrade = false;
    diagnostics->invalidPauseOnTrade = false;
    diagnostics->foundResumeAfterTrade = false;
    diagnostics->invalidResumeAfterTrade = false;
    diagnostics->foundPauseOnInventoryOpen = false;
    diagnostics->invalidPauseOnInventoryOpen = false;
    diagnostics->foundResumeAfterInventoryClose = false;
    diagnostics->invalidResumeAfterInventoryClose = false;
    diagnostics->syntaxError = false;
    diagnostics->syntaxErrorOffset = 0;
}

static bool IsSupportedVersion(unsigned int platform, const std::string& version)
{
    return platform != KenshiLib::BinaryVersion::UNKNOWN
        && (version == "1.0.65" || version == "1.0.68");
}

static bool ResolveSupportedRuntimeNoSeh(unsigned int* out_platform, std::string* out_version)
{
    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();
    if (!IsSupportedVersion(platform, version))
    {
        return false;
    }

    if (out_platform)
    {
        *out_platform = platform;
    }
    if (out_version)
    {
        *out_version = version;
    }
    return true;
}

static bool ResolveSupportedRuntime(unsigned int* out_platform, std::string* out_version)
{
#ifdef _DEBUG
    // Debug deployments run under RE_Kenshi.exe here; use the local Steam 1.0.65 test runtime
    // instead of crossing CRT boundaries through KenshiLib's version helper.
    if (out_platform)
    {
        *out_platform = KenshiLib::BinaryVersion::STEAM;
    }
    if (out_version)
    {
        *out_version = "1.0.65";
    }
    return true;
#else
    __try
    {
        return ResolveSupportedRuntimeNoSeh(out_platform, out_version);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ErrorLog("Auto-Pause-on-Load ERROR: GetKenshiVersion() faulted during startup");
        return false;
    }
#endif
}

static const char* DescribeSaveManagerSignal(int signal)
{
    switch (signal)
    {
    case 0:
        return "NONE";
    case SaveManager::SAVEGAME:
        return "SAVEGAME";
    case SaveManager::LOADGAME:
        return "LOADGAME";
    case SaveManager::IMPORTGAME:
        return "IMPORTGAME";
    case SaveManager::NEWGAME:
        return "NEWGAME";
    default:
        return "UNKNOWN";
    }
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

static void SkipJsonWhitespace(const std::string& text, size_t* pos)
{
    if (!pos)
    {
        return;
    }

    while (*pos < text.size() && std::isspace(static_cast<unsigned char>(text[*pos])) != 0)
    {
        ++(*pos);
    }
}

static bool IsJsonLiteralTerminator(char c)
{
    return std::isspace(static_cast<unsigned char>(c)) != 0 || c == ',' || c == '}' || c == ']';
}

static void SkipUtf8Bom(const std::string& text, size_t* pos)
{
    if (!pos || *pos != 0 || text.size() < 3)
    {
        return;
    }

    const unsigned char b0 = static_cast<unsigned char>(text[0]);
    const unsigned char b1 = static_cast<unsigned char>(text[1]);
    const unsigned char b2 = static_cast<unsigned char>(text[2]);
    if (b0 == 0xEF && b1 == 0xBB && b2 == 0xBF)
    {
        *pos = 3;
    }
}

static bool RecordConfigSyntaxError(ConfigParseDiagnostics* diagnostics, size_t offset)
{
    if (diagnostics)
    {
        diagnostics->syntaxError = true;
        diagnostics->syntaxErrorOffset = offset;
    }
    return false;
}

static bool ParseJsonStringToken(const std::string& text, size_t* pos, std::string* valueOut)
{
    if (!pos || !valueOut)
    {
        return false;
    }

    SkipJsonWhitespace(text, pos);
    if (*pos >= text.size() || text[*pos] != '"')
    {
        return false;
    }

    ++(*pos);
    valueOut->clear();

    while (*pos < text.size())
    {
        const char c = text[*pos];
        if (c == '"')
        {
            ++(*pos);
            return true;
        }

        if (c == '\\')
        {
            ++(*pos);
            if (*pos >= text.size())
            {
                return false;
            }
            valueOut->push_back(text[*pos]);
            ++(*pos);
            continue;
        }

        valueOut->push_back(c);
        ++(*pos);
    }

    return false;
}

static bool ParseJsonBoolValue(const std::string& text, size_t* pos, bool* valueOut)
{
    if (!pos || !valueOut)
    {
        return false;
    }

    SkipJsonWhitespace(text, pos);

    if (*pos + 4 <= text.size() && text.compare(*pos, 4, "true") == 0)
    {
        const size_t end = *pos + 4;
        if (end == text.size() || IsJsonLiteralTerminator(text[end]))
        {
            *valueOut = true;
            *pos = end;
            return true;
        }
    }

    if (*pos + 5 <= text.size() && text.compare(*pos, 5, "false") == 0)
    {
        const size_t end = *pos + 5;
        if (end == text.size() || IsJsonLiteralTerminator(text[end]))
        {
            *valueOut = false;
            *pos = end;
            return true;
        }
    }

    return false;
}

static bool ParseJsonUnsignedValue(const std::string& text, size_t* pos, DWORD* valueOut, bool* clampedOut)
{
    if (!pos || !valueOut)
    {
        return false;
    }

    SkipJsonWhitespace(text, pos);
    size_t cursor = *pos;
    while (cursor < text.size() && std::isdigit(static_cast<unsigned char>(text[cursor])) != 0)
    {
        ++cursor;
    }

    if (cursor == *pos)
    {
        return false;
    }

    if (cursor < text.size() && !IsJsonLiteralTerminator(text[cursor]))
    {
        return false;
    }

    const std::string numberText = text.substr(*pos, cursor - *pos);
    unsigned long parsed = 0;
    try
    {
        parsed = std::stoul(numberText);
    }
    catch (...)
    {
        return false;
    }

    bool clamped = false;
    if (parsed > 600000UL)
    {
        parsed = 600000UL;
        clamped = true;
    }

    *valueOut = static_cast<DWORD>(parsed);
    if (clampedOut)
    {
        *clampedOut = clamped;
    }
    *pos = cursor;
    return true;
}

static bool SkipJsonValue(const std::string& text, size_t* pos);

static bool SkipJsonObject(const std::string& text, size_t* pos)
{
    if (!pos || *pos >= text.size() || text[*pos] != '{')
    {
        return false;
    }

    ++(*pos);
    SkipJsonWhitespace(text, pos);
    if (*pos < text.size() && text[*pos] == '}')
    {
        ++(*pos);
        return true;
    }

    while (*pos < text.size())
    {
        std::string ignoredKey;
        if (!ParseJsonStringToken(text, pos, &ignoredKey))
        {
            return false;
        }

        SkipJsonWhitespace(text, pos);
        if (*pos >= text.size() || text[*pos] != ':')
        {
            return false;
        }

        ++(*pos);
        if (!SkipJsonValue(text, pos))
        {
            return false;
        }

        SkipJsonWhitespace(text, pos);
        if (*pos >= text.size())
        {
            return false;
        }

        if (text[*pos] == ',')
        {
            ++(*pos);
            continue;
        }

        if (text[*pos] == '}')
        {
            ++(*pos);
            return true;
        }

        return false;
    }

    return false;
}

static bool SkipJsonArray(const std::string& text, size_t* pos)
{
    if (!pos || *pos >= text.size() || text[*pos] != '[')
    {
        return false;
    }

    ++(*pos);
    SkipJsonWhitespace(text, pos);
    if (*pos < text.size() && text[*pos] == ']')
    {
        ++(*pos);
        return true;
    }

    while (*pos < text.size())
    {
        if (!SkipJsonValue(text, pos))
        {
            return false;
        }

        SkipJsonWhitespace(text, pos);
        if (*pos >= text.size())
        {
            return false;
        }

        if (text[*pos] == ',')
        {
            ++(*pos);
            continue;
        }

        if (text[*pos] == ']')
        {
            ++(*pos);
            return true;
        }

        return false;
    }

    return false;
}

static bool SkipJsonValue(const std::string& text, size_t* pos)
{
    if (!pos)
    {
        return false;
    }

    SkipJsonWhitespace(text, pos);
    if (*pos >= text.size())
    {
        return false;
    }

    const char c = text[*pos];
    if (c == '"')
    {
        std::string ignored;
        return ParseJsonStringToken(text, pos, &ignored);
    }

    if (c == '{')
    {
        return SkipJsonObject(text, pos);
    }

    if (c == '[')
    {
        return SkipJsonArray(text, pos);
    }

    if (c == '-' || std::isdigit(static_cast<unsigned char>(c)) != 0)
    {
        size_t cursor = *pos;
        if (text[cursor] == '-')
        {
            ++cursor;
        }

        bool sawDigit = false;
        while (cursor < text.size() && std::isdigit(static_cast<unsigned char>(text[cursor])) != 0)
        {
            sawDigit = true;
            ++cursor;
        }

        if (!sawDigit)
        {
            return false;
        }

        if (cursor < text.size() && text[cursor] == '.')
        {
            ++cursor;
            bool sawFractionDigit = false;
            while (cursor < text.size() && std::isdigit(static_cast<unsigned char>(text[cursor])) != 0)
            {
                sawFractionDigit = true;
                ++cursor;
            }
            if (!sawFractionDigit)
            {
                return false;
            }
        }

        if (cursor < text.size() && (text[cursor] == 'e' || text[cursor] == 'E'))
        {
            ++cursor;
            if (cursor < text.size() && (text[cursor] == '+' || text[cursor] == '-'))
            {
                ++cursor;
            }

            bool sawExponentDigit = false;
            while (cursor < text.size() && std::isdigit(static_cast<unsigned char>(text[cursor])) != 0)
            {
                sawExponentDigit = true;
                ++cursor;
            }
            if (!sawExponentDigit)
            {
                return false;
            }
        }

        *pos = cursor;
        return true;
    }

    if (*pos + 4 <= text.size() && text.compare(*pos, 4, "true") == 0)
    {
        *pos += 4;
        return true;
    }

    if (*pos + 5 <= text.size() && text.compare(*pos, 5, "false") == 0)
    {
        *pos += 5;
        return true;
    }

    if (*pos + 4 <= text.size() && text.compare(*pos, 4, "null") == 0)
    {
        *pos += 4;
        return true;
    }

    return false;
}

static bool ParseConfigJson(const std::string& body, PluginConfig* configOut, ConfigParseDiagnostics* diagnostics)
{
    if (!configOut || !diagnostics)
    {
        return false;
    }

    size_t pos = 0;
    SkipUtf8Bom(body, &pos);
    SkipJsonWhitespace(body, &pos);
    if (pos >= body.size() || body[pos] != '{')
    {
        return RecordConfigSyntaxError(diagnostics, pos);
    }

    ++pos;
    SkipJsonWhitespace(body, &pos);
    if (pos < body.size() && body[pos] == '}')
    {
        ++pos;
        SkipJsonWhitespace(body, &pos);
        if (pos == body.size())
        {
            return true;
        }
        return RecordConfigSyntaxError(diagnostics, pos);
    }

    while (pos < body.size())
    {
        std::string key;
        if (!ParseJsonStringToken(body, &pos, &key))
        {
            return RecordConfigSyntaxError(diagnostics, pos);
        }

        SkipJsonWhitespace(body, &pos);
        if (pos >= body.size() || body[pos] != ':')
        {
            return RecordConfigSyntaxError(diagnostics, pos);
        }
        ++pos;

        if (key == "enabled")
        {
            bool parsedBool = false;
            size_t valuePos = pos;
            if (ParseJsonBoolValue(body, &valuePos, &parsedBool))
            {
                diagnostics->foundEnabled = true;
                configOut->enabled = parsedBool;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidEnabled = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "pause_debounce_ms")
        {
            DWORD parsedUnsigned = 0;
            bool clamped = false;
            size_t valuePos = pos;
            if (ParseJsonUnsignedValue(body, &valuePos, &parsedUnsigned, &clamped))
            {
                diagnostics->foundPauseDebounceMs = true;
                diagnostics->clampedPauseDebounceMs = diagnostics->clampedPauseDebounceMs || clamped;
                configOut->pauseDebounceMs = parsedUnsigned;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidPauseDebounceMs = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "debug_log_transitions")
        {
            bool parsedBool = false;
            size_t valuePos = pos;
            if (ParseJsonBoolValue(body, &valuePos, &parsedBool))
            {
                diagnostics->foundDebugLogTransitions = true;
                configOut->debugLogTransitions = parsedBool;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidDebugLogTransitions = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "pause_on_trade")
        {
            bool parsedBool = false;
            size_t valuePos = pos;
            if (ParseJsonBoolValue(body, &valuePos, &parsedBool))
            {
                diagnostics->foundPauseOnTrade = true;
                configOut->pauseOnTrade = parsedBool;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidPauseOnTrade = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "resume_after_trade")
        {
            bool parsedBool = false;
            size_t valuePos = pos;
            if (ParseJsonBoolValue(body, &valuePos, &parsedBool))
            {
                diagnostics->foundResumeAfterTrade = true;
                configOut->resumeAfterTrade = parsedBool;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidResumeAfterTrade = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "pause_on_inventory_open")
        {
            bool parsedBool = false;
            size_t valuePos = pos;
            if (ParseJsonBoolValue(body, &valuePos, &parsedBool))
            {
                diagnostics->foundPauseOnInventoryOpen = true;
                configOut->pauseOnInventoryOpen = parsedBool;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidPauseOnInventoryOpen = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "resume_after_inventory_close")
        {
            bool parsedBool = false;
            size_t valuePos = pos;
            if (ParseJsonBoolValue(body, &valuePos, &parsedBool))
            {
                diagnostics->foundResumeAfterInventoryClose = true;
                configOut->resumeAfterInventoryClose = parsedBool;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidResumeAfterInventoryClose = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else
        {
            if (!SkipJsonValue(body, &pos))
            {
                return RecordConfigSyntaxError(diagnostics, pos);
            }
        }

        SkipJsonWhitespace(body, &pos);
        if (pos >= body.size())
        {
            return RecordConfigSyntaxError(diagnostics, pos);
        }

        if (body[pos] == ',')
        {
            ++pos;
            SkipJsonWhitespace(body, &pos);
            continue;
        }

        if (body[pos] == '}')
        {
            ++pos;
            break;
        }

        return RecordConfigSyntaxError(diagnostics, pos);
    }

    SkipJsonWhitespace(body, &pos);
    if (pos != body.size())
    {
        return RecordConfigSyntaxError(diagnostics, pos);
    }

    return true;
}

static bool RunInternalSelfChecks()
{
    // Keep this intentionally small: sanity-check parser and state helpers.
    PluginConfig parsedConfig = { true, 2000, false, false, false, false, false };
    ConfigParseDiagnostics diagnostics;
    ResetConfigParseDiagnostics(&diagnostics);

    if (!ParseConfigJson(
            "{\"enabled\":false,\"pause_debounce_ms\":1234,\"debug_log_transitions\":true,\"pause_on_trade\":true,\"resume_after_trade\":true,\"pause_on_inventory_open\":true,\"resume_after_inventory_close\":true}",
            &parsedConfig,
            &diagnostics))
    {
        return false;
    }
    if (parsedConfig.enabled
        || parsedConfig.pauseDebounceMs != 1234
        || !parsedConfig.debugLogTransitions
        || !parsedConfig.pauseOnTrade
        || !parsedConfig.resumeAfterTrade
        || !parsedConfig.pauseOnInventoryOpen
        || !parsedConfig.resumeAfterInventoryClose)
    {
        return false;
    }

    const std::string bomJson = std::string("\xEF\xBB\xBF") + "{\"enabled\":true,\"pause_debounce_ms\":2000,\"debug_log_transitions\":false,\"pause_on_trade\":false,\"resume_after_trade\":false,\"pause_on_inventory_open\":false,\"resume_after_inventory_close\":false}";
    parsedConfig.enabled = false;
    parsedConfig.pauseDebounceMs = 1;
    parsedConfig.debugLogTransitions = true;
    parsedConfig.pauseOnTrade = true;
    parsedConfig.resumeAfterTrade = true;
    parsedConfig.pauseOnInventoryOpen = true;
    parsedConfig.resumeAfterInventoryClose = true;
    ResetConfigParseDiagnostics(&diagnostics);
    if (!ParseConfigJson(bomJson, &parsedConfig, &diagnostics))
    {
        return false;
    }
    if (!parsedConfig.enabled
        || parsedConfig.pauseDebounceMs != 2000
        || parsedConfig.debugLogTransitions
        || parsedConfig.pauseOnTrade
        || parsedConfig.resumeAfterTrade
        || parsedConfig.pauseOnInventoryOpen
        || parsedConfig.resumeAfterInventoryClose)
    {
        return false;
    }

    parsedConfig.enabled = true;
    ResetConfigParseDiagnostics(&diagnostics);
    if (!ParseConfigJson("{\"enabled\":\"nope\"}", &parsedConfig, &diagnostics))
    {
        return false;
    }
    if (!diagnostics.invalidEnabled)
    {
        return false;
    }

    if (!DebounceWindowElapsed(100, 0, 50) || DebounceWindowElapsed(20, 0, 50))
    {
        return false;
    }

    const RuntimeState savedState = g_state;
    g_state.loadInProgress = true;
    g_state.pauseArmed = true;
    g_state.loadSignalSeenAfterArm = true;
    g_state.saveLoadIntentPending = true;
    g_state.armTimestampMs = 99;
    g_state.saveLoadIntentTimestampMs = 123;
    g_state.loggedWorldUnavailable = true;
    DisarmPauseAfterLoad();
    const bool disarmedOk =
        !g_state.loadInProgress
        && !g_state.pauseArmed
        && !g_state.loadSignalSeenAfterArm
        && !g_state.saveLoadIntentPending
        && g_state.armTimestampMs == 0
        && g_state.saveLoadIntentTimestampMs == 0
        && !g_state.loggedWorldUnavailable;
    g_state = savedState;
    return disarmedOk;
}

static bool ReadConfigFromFile(
    const std::string& configPath,
    PluginConfig* configOut,
    bool* foundFileOut,
    bool* needsWriteBackOut)
{
    if (!configOut)
    {
        return false;
    }

    if (foundFileOut)
    {
        *foundFileOut = false;
    }
    if (needsWriteBackOut)
    {
        *needsWriteBackOut = false;
    }

    std::ifstream in(configPath.c_str(), std::ios::in | std::ios::binary);
    if (!in)
    {
        if (needsWriteBackOut)
        {
            *needsWriteBackOut = true;
        }
        return true;
    }

    if (foundFileOut)
    {
        *foundFileOut = true;
    }

    const std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ConfigParseDiagnostics diagnostics;
    ResetConfigParseDiagnostics(&diagnostics);
    if (!ParseConfigJson(body, configOut, &diagnostics))
    {
        std::stringstream error;
        error << kPluginName << " ERROR: mod-config.json parse error near byte offset " << diagnostics.syntaxErrorOffset;
        ErrorLog(error.str().c_str());
        return false;
    }

    bool needsWriteBack = false;
    if (!diagnostics.foundEnabled || diagnostics.invalidEnabled)
    {
        needsWriteBack = true;
        ErrorLog("Auto-Pause-on-Load WARN: invalid/missing key \"enabled\"; using default");
    }
    if (!diagnostics.foundPauseDebounceMs || diagnostics.invalidPauseDebounceMs)
    {
        needsWriteBack = true;
        ErrorLog("Auto-Pause-on-Load WARN: invalid/missing key \"pause_debounce_ms\"; using default");
    }
    if (diagnostics.clampedPauseDebounceMs)
    {
        needsWriteBack = true;
        ErrorLog("Auto-Pause-on-Load WARN: \"pause_debounce_ms\" exceeded max; clamped to 600000");
    }
    if (!diagnostics.foundDebugLogTransitions || diagnostics.invalidDebugLogTransitions)
    {
        needsWriteBack = true;
        ErrorLog("Auto-Pause-on-Load WARN: invalid/missing key \"debug_log_transitions\"; using default");
    }
    if (!diagnostics.foundPauseOnTrade || diagnostics.invalidPauseOnTrade)
    {
        needsWriteBack = true;
        ErrorLog("Auto-Pause-on-Load WARN: invalid/missing key \"pause_on_trade\"; using default");
    }
    if (!diagnostics.foundResumeAfterTrade || diagnostics.invalidResumeAfterTrade)
    {
        needsWriteBack = true;
        ErrorLog("Auto-Pause-on-Load WARN: invalid/missing key \"resume_after_trade\"; using default");
    }
    if (!diagnostics.foundPauseOnInventoryOpen || diagnostics.invalidPauseOnInventoryOpen)
    {
        needsWriteBack = true;
        ErrorLog("Auto-Pause-on-Load WARN: invalid/missing key \"pause_on_inventory_open\"; using default");
    }
    if (!diagnostics.foundResumeAfterInventoryClose || diagnostics.invalidResumeAfterInventoryClose)
    {
        needsWriteBack = true;
        ErrorLog("Auto-Pause-on-Load WARN: invalid/missing key \"resume_after_inventory_close\"; using default");
    }
    if (needsWriteBackOut)
    {
        *needsWriteBackOut = needsWriteBack;
    }
    return true;
}

static bool SaveConfigToFile(const std::string& configPath, const PluginConfig& config)
{
    std::ofstream out(configPath.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out)
    {
        return false;
    }

    out << "{\n";
    out << "  \"enabled\": " << (config.enabled ? "true" : "false") << ",\n";
    out << "  \"pause_debounce_ms\": " << config.pauseDebounceMs << ",\n";
    out << "  \"debug_log_transitions\": " << (config.debugLogTransitions ? "true" : "false") << ",\n";
    out << "  \"pause_on_trade\": " << (config.pauseOnTrade ? "true" : "false") << ",\n";
    out << "  \"resume_after_trade\": " << (config.resumeAfterTrade ? "true" : "false") << ",\n";
    out << "  \"pause_on_inventory_open\": " << (config.pauseOnInventoryOpen ? "true" : "false") << ",\n";
    out << "  \"resume_after_inventory_close\": " << (config.resumeAfterInventoryClose ? "true" : "false") << "\n";
    out << "}\n";

    return true;
}

static void LoadConfigState()
{
    g_configNeedsWriteBack = false;
    g_config.enabled = true;
    g_config.pauseDebounceMs = 2000;
    g_config.debugLogTransitions = false;
    g_config.pauseOnTrade = true;
    g_config.resumeAfterTrade = true;
    g_config.pauseOnInventoryOpen = true;
    g_config.resumeAfterInventoryClose = true;

    if (g_settingsPath.empty())
    {
        return;
    }

    bool foundConfigFile = false;
    bool needsWriteBack = false;
    if (!ReadConfigFromFile(g_settingsPath, &g_config, &foundConfigFile, &needsWriteBack))
    {
        ErrorLog("Auto-Pause-on-Load ERROR: failed to read mod-config.json; using defaults and rewriting file");
        g_configNeedsWriteBack = true;
        return;
    }

    g_configNeedsWriteBack = (!foundConfigFile) || needsWriteBack;
}

static bool SaveConfigState()
{
    if (g_settingsPath.empty())
    {
        ErrorLog("Auto-Pause-on-Load ERROR: settings path is empty; cannot save mod-config.json");
        return false;
    }

    if (!SaveConfigToFile(g_settingsPath, g_config))
    {
        ErrorLog("Auto-Pause-on-Load ERROR: failed to save mod-config.json");
        return false;
    }

    return true;
}

// Keep Mod Hub registration in the existing single-TU layout so startup hook resolution
// runs in a similar compilation environment to the working multi-file mods.
#include "AutoPauseModHub.inl"

static bool DebounceWindowElapsed(DWORD nowMs, DWORD lastEventMs, DWORD minGapMs)
{
    const DWORD elapsed = nowMs - lastEventMs;
    return elapsed >= minGapMs;
}

static void DisarmPauseAfterLoad()
{
    g_state.pauseArmed = false;
    g_state.loadInProgress = false;
    g_state.loadSignalSeenAfterArm = false;
    g_state.saveLoadIntentPending = false;
    g_state.armTimestampMs = 0;
    g_state.saveLoadIntentTimestampMs = 0;
    g_state.loggedWorldUnavailable = false;
}

static void ArmPauseAfterLoad(const char* source)
{
    g_state.pauseArmed = true;
    g_state.loadInProgress = false;
    g_state.loadSignalSeenAfterArm = false;
    g_state.armTimestampMs = GetTickCount();
    g_state.loggedWorldUnavailable = false;

    if (g_config.debugLogTransitions)
    {
        std::stringstream logline;
        logline << "Auto-Pause-on-Load DEBUG: armed from " << source;
        DebugLog(logline.str().c_str());
    }
}

static bool QuerySaveLoadSignal(bool* isLoadingOut)
{
    if (!isLoadingOut)
    {
        return false;
    }

    *isLoadingOut = false;
    if (!ou)
    {
        if (!g_updateUTHookVerified)
        {
            return false;
        }

        if (!g_state.loggedWorldUnavailable)
        {
            ErrorLog("Auto-Pause-on-Load WARN: game world unavailable while waiting for save-load completion");
            g_state.loggedWorldUnavailable = true;
        }
        return false;
    }

    g_state.loggedWorldUnavailable = false;
    __try
    {
        *isLoadingOut = ou->isLoadingFromASaveGame();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ErrorLog("Auto-Pause-on-Load WARN: exception while querying save-load state");
        return false;
    }
}

static bool ForcePauseState(bool paused)
{
    if (!ou)
    {
        if (!g_state.loggedWorldUnavailable)
        {
            ErrorLog("Auto-Pause-on-Load WARN: game world unavailable; cannot change pause state");
            g_state.loggedWorldUnavailable = true;
        }
        return false;
    }

    g_state.loggedWorldUnavailable = false;
    __try
    {
        ou->userPause(paused);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ErrorLog("Auto-Pause-on-Load WARN: exception while changing pause state");
        return false;
    }
}

static bool ForcePauseTrue()
{
    return ForcePauseState(true);
}

static bool ForcePauseFalse()
{
    return ForcePauseState(false);
}

static bool QueryGamePaused(bool* isPausedOut)
{
    if (!isPausedOut)
    {
        return false;
    }

    *isPausedOut = false;
    if (!ou)
    {
        if (!g_state.loggedWorldUnavailable)
        {
            ErrorLog("Auto-Pause-on-Load WARN: game world unavailable; cannot query pause state");
            g_state.loggedWorldUnavailable = true;
        }
        return false;
    }

    g_state.loggedWorldUnavailable = false;
    __try
    {
        *isPausedOut = ou->isPaused();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ErrorLog("Auto-Pause-on-Load WARN: exception while querying pause state");
        return false;
    }
}

static bool QuerySelectedCharacter(Character** selectedOut)
{
    if (!selectedOut)
    {
        return false;
    }

    *selectedOut = 0;
    if (!ou || !ou->player)
    {
        return true;
    }

    __try
    {
        const hand selectedHandle = ou->player->selectedCharacter;
        if (selectedHandle.isNull() || selectedHandle.type != CHARACTER)
        {
            return true;
        }

        *selectedOut = selectedHandle.getCharacter();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (g_config.debugLogTransitions)
        {
            ErrorLog("Auto-Pause-on-Load WARN: exception while resolving selected character");
        }
        *selectedOut = 0;
        return true;
    }
}

static bool TryResolveCharacterInventoryVisible(Character* character, bool* visibleOut)
{
    if (!visibleOut)
    {
        return false;
    }

    *visibleOut = false;
    if (!character)
    {
        return true;
    }

    __try
    {
        if (character->inventory && character->inventory->isVisible())
        {
            *visibleOut = true;
            return true;
        }

        *visibleOut = character->isInventoryVisible();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (g_config.debugLogTransitions)
        {
            ErrorLog("Auto-Pause-on-Load WARN: exception while querying inventory visibility");
        }
        return false;
    }
}

static void AccumulateCharacterUiSignals(
    Character* character,
    bool* anyTradeActiveOut,
    bool* anyInventoryVisibleOut)
{
    if (!character || !anyTradeActiveOut || !anyInventoryVisibleOut)
    {
        return;
    }

    bool characterInventoryVisible = false;
    if (TryResolveCharacterInventoryVisible(character, &characterInventoryVisible) && characterInventoryVisible)
    {
        *anyInventoryVisibleOut = true;
    }

    __try
    {
        Dialogue* dialog = character->dialogue;
        if (!dialog)
        {
            return;
        }

        Character* target = dialog->getConversationTarget().getCharacter();
        if (!target)
        {
            return;
        }

        bool targetInventoryVisible = false;
        if (!TryResolveCharacterInventoryVisible(target, &targetInventoryVisible))
        {
            targetInventoryVisible = false;
        }

        const bool dialogActive = !dialog->conversationHasEndedPrettyMuch();
        const bool playerEngaged = character->_isEngagedWithAPlayer;
        const bool targetEngaged = target->_isEngagedWithAPlayer;
        if (dialogActive
            && (playerEngaged || targetEngaged)
            && (characterInventoryVisible || targetInventoryVisible))
        {
            *anyTradeActiveOut = true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (g_config.debugLogTransitions)
        {
            ErrorLog("Auto-Pause-on-Load WARN: exception while evaluating dialogue UI signals");
        }
    }
}

static bool QueryTradeAndInventoryStates(bool* tradeActiveOut, bool* inventoryActiveOut)
{
    if (!tradeActiveOut || !inventoryActiveOut)
    {
        return false;
    }

    *tradeActiveOut = false;
    *inventoryActiveOut = false;

    Character* selected = 0;
    if (!QuerySelectedCharacter(&selected))
    {
        return false;
    }

    bool anyTradeActive = false;
    bool anyInventoryVisible = false;
    if (selected)
    {
        AccumulateCharacterUiSignals(selected, &anyTradeActive, &anyInventoryVisible);
    }

    if (!ou || !ou->player)
    {
        *tradeActiveOut = anyTradeActive;
        *inventoryActiveOut = anyInventoryVisible;
        return true;
    }

    __try
    {
        const lektor<Character*>& allPlayerCharacters = ou->player->getAllPlayerCharacters();
        for (auto iter = allPlayerCharacters.begin(); iter != allPlayerCharacters.end(); ++iter)
        {
            Character* candidate = *iter;
            if (!candidate || candidate == selected)
            {
                continue;
            }

            AccumulateCharacterUiSignals(candidate, &anyTradeActive, &anyInventoryVisible);
            if (anyTradeActive && anyInventoryVisible)
            {
                break;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (g_config.debugLogTransitions)
        {
            ErrorLog("Auto-Pause-on-Load WARN: exception while scanning player characters for UI signals");
        }
    }

    *tradeActiveOut = anyTradeActive;
    *inventoryActiveOut = anyInventoryVisible;
    return true;
}

static void ResetUiTracking()
{
    g_state.tradeActive = false;
    g_state.inventoryActive = false;
    g_state.tradePausedByPlugin = false;
    g_state.inventoryPausedByPlugin = false;
    g_state.tradeWasPausedBeforeStart = false;
    g_state.inventoryWasPausedBeforeStart = false;
}

static void TryPauseOnUiOpen(
    DWORD nowMs,
    const char* reason,
    bool* pausedByPluginOut,
    bool* wasPausedBeforeStartOut)
{
    if (!pausedByPluginOut || !wasPausedBeforeStartOut)
    {
        return;
    }

    *pausedByPluginOut = false;
    *wasPausedBeforeStartOut = false;

    bool wasPausedBeforeStart = false;
    if (QueryGamePaused(&wasPausedBeforeStart))
    {
        *wasPausedBeforeStartOut = wasPausedBeforeStart;
        if (wasPausedBeforeStart)
        {
            return;
        }
    }

    if (ForcePauseTrue())
    {
        g_state.lastPauseMs = nowMs;
        *pausedByPluginOut = true;
        std::stringstream info;
        info << "Auto-Pause-on-Load INFO: paused=true source=" << reason;
        DebugLog(info.str().c_str());
    }
}

static bool IsUiPauseFeatureActive()
{
    return (g_state.tradeActive && g_config.pauseOnTrade)
        || (g_state.inventoryActive && g_config.pauseOnInventoryOpen);
}

static void TryResumeAfterUiClose(const char* reason)
{
    bool isPaused = false;
    if (!QueryGamePaused(&isPaused) || !isPaused)
    {
        return;
    }

    if (IsUiPauseFeatureActive())
    {
        return;
    }

    if (ForcePauseFalse())
    {
        std::stringstream info;
        info << "Auto-Pause-on-Load INFO: resumed=true source=" << reason;
        DebugLog(info.str().c_str());
    }
}

static void TryPauseAndDisarm(DWORD nowMs, const char* reason)
{
    if (!DebounceWindowElapsed(nowMs, g_state.lastPauseMs, g_config.pauseDebounceMs))
    {
        if (g_config.debugLogTransitions)
        {
            DebugLog("Auto-Pause-on-Load DEBUG: pause skipped (debounce)");
        }
        DisarmPauseAfterLoad();
        return;
    }

    if (ForcePauseTrue())
    {
        g_state.lastPauseMs = nowMs;
        if (g_config.debugLogTransitions)
        {
            std::stringstream info;
            info << "Auto-Pause-on-Load INFO: paused_after_load=true source=" << reason;
            DebugLog(info.str().c_str());
        }
    }

    DisarmPauseAfterLoad();
}

static void TickPauseOnTradeAndInventory()
{
    if (!g_config.enabled)
    {
        ResetUiTracking();
        return;
    }

    bool tradeActiveNow = false;
    bool inventoryActiveNow = false;
    if (!QueryTradeAndInventoryStates(&tradeActiveNow, &inventoryActiveNow))
    {
        return;
    }

    const bool tradeOpened = (!g_state.tradeActive && tradeActiveNow);
    const bool tradeClosed = (g_state.tradeActive && !tradeActiveNow);
    const bool inventoryOpened = (!g_state.inventoryActive && inventoryActiveNow);
    const bool inventoryClosed = (g_state.inventoryActive && !inventoryActiveNow);

    const DWORD nowMs = GetTickCount();

    if (tradeOpened)
    {
        g_state.tradePausedByPlugin = false;
        g_state.tradeWasPausedBeforeStart = false;
        if (g_config.pauseOnTrade)
        {
            TryPauseOnUiOpen(
                nowMs,
                "trade_open",
                &g_state.tradePausedByPlugin,
                &g_state.tradeWasPausedBeforeStart);
        }
    }

    if (inventoryOpened)
    {
        g_state.inventoryPausedByPlugin = false;
        g_state.inventoryWasPausedBeforeStart = false;
        if (g_config.pauseOnInventoryOpen)
        {
            TryPauseOnUiOpen(
                nowMs,
                "inventory_open",
                &g_state.inventoryPausedByPlugin,
                &g_state.inventoryWasPausedBeforeStart);
        }
    }

    g_state.tradeActive = tradeActiveNow;
    g_state.inventoryActive = inventoryActiveNow;

    if (tradeClosed)
    {
        const bool shouldResume =
            g_config.pauseOnTrade
            && g_config.resumeAfterTrade
            && g_state.tradePausedByPlugin
            && !g_state.tradeWasPausedBeforeStart;
        g_state.tradePausedByPlugin = false;
        g_state.tradeWasPausedBeforeStart = false;
        if (shouldResume)
        {
            TryResumeAfterUiClose("trade_closed");
        }
    }

    if (inventoryClosed)
    {
        const bool shouldResume =
            g_config.pauseOnInventoryOpen
            && g_config.resumeAfterInventoryClose
            && g_state.inventoryPausedByPlugin
            && !g_state.inventoryWasPausedBeforeStart;
        g_state.inventoryPausedByPlugin = false;
        g_state.inventoryWasPausedBeforeStart = false;
        if (shouldResume)
        {
            TryResumeAfterUiClose("inventory_closed");
        }
    }
}

static void TickPauseOnLoad()
{
    if (!g_config.enabled)
    {
        g_hasObservedLoadSignal = false;
        g_lastObservedLoadSignal = false;
        g_hasObservedSaveManagerSignal = false;
        g_lastObservedSaveManagerSignal = 0;
        DisarmPauseAfterLoad();
        return;
    }

    const DWORD nowMs = GetTickCount();

    if (g_config.debugLogTransitions)
    {
        if (g_state.lastTickAliveLogMs == 0 || DebounceWindowElapsed(nowMs, g_state.lastTickAliveLogMs, kTickAliveIntervalMs))
        {
            DebugLog("Auto-Pause-on-Load DEBUG: tick alive");
            g_state.lastTickAliveLogMs = nowMs;
        }
    }

    if (g_state.pauseArmed
        && !g_state.loadInProgress
        && g_state.armTimestampMs != 0
        && DebounceWindowElapsed(nowMs, g_state.armTimestampMs, kArmedTimeoutMs))
    {
        ErrorLog("Auto-Pause-on-Load WARN: armed pause timed out before load completion");
        DisarmPauseAfterLoad();
        return;
    }

    bool isLoadingSave = false;
    if (!QuerySaveLoadSignal(&isLoadingSave))
    {
        return;
    }

    std::string currentGame;
    std::string requestedName;
    bool loadRequested = false;
    int saveManagerSignal = 0;
    int saveManagerDelay = 0;
    std::string* currentGameLog = g_config.debugLogTransitions ? &currentGame : 0;
    std::string* requestedNameLog = g_config.debugLogTransitions ? &requestedName : 0;
    if (!QuerySaveManagerState(
            &loadRequested,
            &saveManagerSignal,
            &saveManagerDelay,
            currentGameLog,
            requestedNameLog))
    {
        return;
    }

    if (!g_hasObservedSaveManagerSignal || g_lastObservedSaveManagerSignal != saveManagerSignal)
    {
        LogLoadInvestigation(
            "save_manager_signal",
            isLoadingSave,
            saveManagerSignal,
            saveManagerDelay,
            currentGameLog,
            requestedNameLog);
        g_hasObservedSaveManagerSignal = true;
        g_lastObservedSaveManagerSignal = saveManagerSignal;
    }

    if (loadRequested)
    {
        const bool wasPending = g_state.saveLoadIntentPending;
        g_state.saveLoadIntentPending = true;
        g_state.saveLoadIntentTimestampMs = nowMs;
        if (!wasPending)
        {
            LogLoadInvestigation(
                "load_intent_seen",
                isLoadingSave,
                saveManagerSignal,
                saveManagerDelay,
                currentGameLog,
                requestedNameLog);
        }
    }
    else if (g_state.saveLoadIntentPending
        && !g_state.pauseArmed
        && g_state.saveLoadIntentTimestampMs != 0
        && DebounceWindowElapsed(nowMs, g_state.saveLoadIntentTimestampMs, kSaveLoadIntentTimeoutMs))
    {
        LogLoadInvestigation(
            "load_intent_expired",
            isLoadingSave,
            saveManagerSignal,
            saveManagerDelay,
            currentGameLog,
            requestedNameLog);
        g_state.saveLoadIntentPending = false;
        g_state.saveLoadIntentTimestampMs = 0;
    }

    bool loadStarted = false;
    bool loadFinished = false;
    bool bootstrapLoadIntent = false;
    if (!g_hasObservedLoadSignal)
    {
        g_hasObservedLoadSignal = true;
        loadStarted = isLoadingSave;
        bootstrapLoadIntent = isLoadingSave && !g_updateUTHookVerified;
    }
    else
    {
        loadStarted = (!g_lastObservedLoadSignal && isLoadingSave);
        loadFinished = (g_lastObservedLoadSignal && !isLoadingSave);
    }
    g_lastObservedLoadSignal = isLoadingSave;

    if (loadStarted)
    {
        LogLoadInvestigation(
            "load_flag_started",
            isLoadingSave,
            saveManagerSignal,
            saveManagerDelay,
            currentGameLog,
            requestedNameLog);

        if (g_state.saveLoadIntentPending)
        {
            ArmPauseAfterLoad("save_manager_signal");
            LogLoadInvestigation(
                "load_arm_accepted",
                isLoadingSave,
                saveManagerSignal,
                saveManagerDelay,
                currentGameLog,
                requestedNameLog);
        }
        else if (bootstrapLoadIntent)
        {
            ArmPauseAfterLoad("startup_load_bootstrap");
            LogLoadInvestigation(
                "load_arm_bootstrap",
                isLoadingSave,
                saveManagerSignal,
                saveManagerDelay,
                currentGameLog,
                requestedNameLog);
        }
        else
        {
            LogLoadInvestigation(
                "load_ignored_no_intent",
                isLoadingSave,
                saveManagerSignal,
                saveManagerDelay,
                currentGameLog,
                requestedNameLog);
        }
    }

    if (isLoadingSave
        && g_state.saveLoadIntentPending
        && !g_state.pauseArmed
        && !g_state.loadInProgress)
    {
        ArmPauseAfterLoad("save_manager_signal_late");
        LogLoadInvestigation(
            "load_arm_late",
            isLoadingSave,
            saveManagerSignal,
            saveManagerDelay,
            currentGameLog,
            requestedNameLog);
    }

    if (isLoadingSave)
    {
        if (g_state.pauseArmed)
        {
            if (!g_state.loadInProgress && g_config.debugLogTransitions)
            {
                DebugLog("Auto-Pause-on-Load DEBUG: load started");
            }
            g_state.loadInProgress = true;
            g_state.loadSignalSeenAfterArm = true;
        }
        return;
    }

    if (loadFinished)
    {
        LogLoadInvestigation(
            "load_flag_finished",
            isLoadingSave,
            saveManagerSignal,
            saveManagerDelay,
            currentGameLog,
            requestedNameLog);
    }

    if (!g_state.pauseArmed)
    {
        return;
    }

    if (g_state.loadInProgress)
    {
        if (g_config.debugLogTransitions)
        {
            DebugLog("Auto-Pause-on-Load DEBUG: load finished");
        }
        TryPauseAndDisarm(nowMs, "load_transition");
        return;
    }

    // Disarm if no load signal arrives shortly after arming. This avoids
    // false-positive pauses when a load call fails or is cancelled early.
    if (!g_state.loadSignalSeenAfterArm
        && g_state.armTimestampMs != 0
        && DebounceWindowElapsed(nowMs, g_state.armTimestampMs, kNoSignalDisarmMs))
    {
        if (g_config.debugLogTransitions)
        {
            DebugLog("Auto-Pause-on-Load DEBUG: no load signal observed; disarming");
        }
        DisarmPauseAfterLoad();
    }
}

static bool QuerySaveManagerState(
    bool* loadRequestedOut,
    int* signalOut,
    int* delayOut,
    std::string* currentGameOut,
    std::string* requestedNameOut)
{
    if (!loadRequestedOut || !signalOut || !delayOut)
    {
        return false;
    }

    *loadRequestedOut = false;
    *signalOut = 0;
    *delayOut = 0;
    if (currentGameOut)
    {
        currentGameOut->clear();
    }
    if (requestedNameOut)
    {
        requestedNameOut->clear();
    }

    __try
    {
        SaveManager* saveManager = SaveManager::getSingleton();
        if (!saveManager)
        {
            return true;
        }

        *signalOut = saveManager->signal;
        *delayOut = saveManager->delay;
        *loadRequestedOut = (saveManager->signal == SaveManager::LOADGAME);
        if (currentGameOut)
        {
            *currentGameOut = saveManager->currentGame;
        }
        if (requestedNameOut)
        {
            *requestedNameOut = saveManager->name;
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (g_config.debugLogTransitions)
        {
            ErrorLog("Auto-Pause-on-Load WARN: exception while querying SaveManager state");
        }
        return false;
    }
}

static void LogLoadInvestigation(
    const char* stage,
    bool isLoadingSave,
    int saveManagerSignal,
    int saveManagerDelay,
    const std::string* currentGame,
    const std::string* requestedName)
{
    if (!g_config.debugLogTransitions)
    {
        return;
    }

    std::stringstream line;
    line << "Auto-Pause-on-Load INFO: [investigate][load]"
         << " stage=" << (stage ? stage : "unknown")
         << " is_loading=" << (isLoadingSave ? "true" : "false")
         << " signal=" << DescribeSaveManagerSignal(saveManagerSignal)
         << " delay=" << saveManagerDelay
         << " intent_pending=" << (g_state.saveLoadIntentPending ? "true" : "false")
         << " pause_armed=" << (g_state.pauseArmed ? "true" : "false")
         << " load_in_progress=" << (g_state.loadInProgress ? "true" : "false");
    if (currentGame && !currentGame->empty())
    {
        line << " current_game=\"" << *currentGame << "\"";
    }
    if (requestedName && !requestedName->empty())
    {
        line << " requested_name=\"" << *requestedName << "\"";
    }
    DebugLog(line.str().c_str());
}

static bool TryInstallUpdateUTHook()
{
    if (PlayerInterface_updateUT_orig != 0)
    {
        g_hooksInstalled = true;
        return true;
    }

    if (KenshiLib::SUCCESS == KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
            PlayerInterface_updateUT_hook,
            &PlayerInterface_updateUT_orig))
    {
        g_hooksInstalled = true;
        return true;
    }

    ErrorLog("Auto-Pause-on-Load WARN: failed to install PlayerInterface::updateUT hook");
    return false;
}

static void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    PlayerInterface_updateUT_orig(thisptr);

    if (!g_updateUTHookVerified)
    {
        if (g_updateUTHookVerifyTimeMs == 0)
        {
            g_updateUTHookVerifyTimeMs = GetTickCount();
        }
        else if (GetTickCount() - g_updateUTHookVerifyTimeMs > 1000)
        {
            g_updateUTHookVerified = true;
            if (g_config.debugLogTransitions)
            {
                DebugLog("Auto-Pause-on-Load INFO: updateUT hook verified alive");
            }
        }
    }

    TickPauseOnLoad();
    if (g_updateUTHookVerified)
    {
        TickPauseOnTradeAndInventory();
    }
    else
    {
        ResetUiTracking();
    }
}

__declspec(dllexport) void startPlugin()
{
    unsigned int platform = KenshiLib::BinaryVersion::UNKNOWN;
    std::string version;
    if (!ResolveSupportedRuntime(&platform, &version))
    {
        ErrorLog("Auto-Pause-on-Load: unsupported Kenshi version/platform");
        return;
    }

    LoadConfigState();
    if (g_configNeedsWriteBack)
    {
        if (!SaveConfigState())
        {
            ErrorLog("Auto-Pause-on-Load WARN: failed to persist normalized mod-config.json");
        }
    }

    if (!RunInternalSelfChecks())
    {
        ErrorLog("Auto-Pause-on-Load ERROR: internal self-check failed");
        return;
    }

    if (!TryInstallUpdateUTHook())
    {
        ErrorLog("Auto-Pause-on-Load WARN: failed to install updateUT host at startup; pause runtime will remain inactive");
    }

    g_hasObservedLoadSignal = false;
    g_lastObservedLoadSignal = false;
    g_hasObservedSaveManagerSignal = false;
    g_lastObservedSaveManagerSignal = 0;
    DisarmPauseAfterLoad();

    ConfigureModHubClient();
    StartModHubClient();

    std::stringstream info;
    info << "Auto-Pause-on-Load INFO: initialized enabled=" << (g_config.enabled ? "true" : "false")
         << " debug_log_transitions=" << (g_config.debugLogTransitions ? "true" : "false")
         << " hooks_installed=" << (g_hooksInstalled ? "true" : "false")
         << " hub_ui=" << (g_modHubClient.UseHubUi() ? "true" : "false")
         << " config_needs_writeback=" << (g_configNeedsWriteBack ? "true" : "false");
    if (g_config.debugLogTransitions)
    {
        info << " pause_debounce_ms=" << g_config.pauseDebounceMs
             << " pause_on_trade=" << (g_config.pauseOnTrade ? "true" : "false")
             << " resume_after_trade=" << (g_config.resumeAfterTrade ? "true" : "false")
             << " pause_on_inventory_open=" << (g_config.pauseOnInventoryOpen ? "true" : "false")
             << " resume_after_inventory_close=" << (g_config.resumeAfterInventoryClose ? "true" : "false");
    }
    DebugLog(info.str().c_str());
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        char dllPath[_MAX_PATH] = { 0 };
        if (GetModuleFileNameA(hModule, dllPath, _MAX_PATH) > 0)
        {
            std::string fullPath = TrimAscii(std::string(dllPath));
            size_t sep = fullPath.find_last_of("\\/");
            if (sep != std::string::npos)
            {
                const std::string myDirectory = fullPath.substr(0, sep);
                g_settingsPath = myDirectory + "\\" + kConfigFileName;
            }
        }

    }
    return TRUE;
}
