#include <Debug.h>

#include <core/Functions.h>
#include "emc/mod_hub_client.h"

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
    DWORD armTimestampMs;
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
static const DWORD kPauseDebounceMsMin = 0;
static const DWORD kPauseDebounceMsMax = 600000;
static const DWORD kModHubAttachRetryIntervalMs = 2000;
static const DWORD kModHubAttachRetryMaxAttempts = 30;

static const char* kHubNamespaceId = "emkej.qol";
static const char* kHubNamespaceDisplayName = "Emkej QoL";
static const char* kHubModId = "auto_pause_on_load";
static const char* kHubModDisplayName = "Auto Pause on Load";

static PluginConfig g_config = { true, 2000, false, true, true, true, true };
static RuntimeState g_state = { false, false, false, 0, 0, 0, false, false, false, false, false, false, false };
static emc::ModHubClient g_modHubClient;

static std::string g_settingsPath;
static bool g_hasSaveLoadHook = false;
static bool g_configNeedsWriteBack = false;
static bool g_modHubAttachRetryActive = false;
static DWORD g_modHubAttachRetryAttempts = 0;
static DWORD g_modHubAttachRetryLastAttemptMs = 0;

static void (*PlayerInterface_updateUT_orig)(PlayerInterface*) = 0;
static void (*SaveManager_loadByInfo_orig)(SaveManager*, const SaveInfo&, bool) = 0;
static void (*SaveManager_loadByName_orig)(SaveManager*, const std::string&) = 0;

static bool DebounceWindowElapsed(DWORD nowMs, DWORD lastEventMs, DWORD minGapMs);
static void DisarmPauseAfterLoad();
static void ResetUiTracking();
static void TickModHubAttachRetry();

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
    g_state.armTimestampMs = 99;
    g_state.loggedWorldUnavailable = true;
    DisarmPauseAfterLoad();
    const bool disarmedOk =
        !g_state.loadInProgress
        && !g_state.pauseArmed
        && !g_state.loadSignalSeenAfterArm
        && g_state.armTimestampMs == 0
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
    if (!foundConfigFile)
    {
        DebugLog("Auto-Pause-on-Load INFO: mod-config.json not found; using defaults");
    }

    if (g_config.debugLogTransitions)
    {
        std::stringstream info;
        info << "Auto-Pause-on-Load INFO: loaded config enabled=" << (g_config.enabled ? "true" : "false")
             << " pause_debounce_ms=" << g_config.pauseDebounceMs
             << " debug_log_transitions=" << (g_config.debugLogTransitions ? "true" : "false")
             << " pause_on_trade=" << (g_config.pauseOnTrade ? "true" : "false")
             << " resume_after_trade=" << (g_config.resumeAfterTrade ? "true" : "false")
             << " pause_on_inventory_open=" << (g_config.pauseOnInventoryOpen ? "true" : "false")
             << " resume_after_inventory_close=" << (g_config.resumeAfterInventoryClose ? "true" : "false");
        DebugLog(info.str().c_str());
    }
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

static void WriteHubErrorText(char* err_buf, uint32_t err_buf_size, const char* text)
{
    if (!err_buf || err_buf_size == 0u)
    {
        return;
    }

    if (!text)
    {
        err_buf[0] = '\0';
        return;
    }

    const size_t copyLen = static_cast<size_t>(err_buf_size - 1u);
    std::strncpy(err_buf, text, copyLen);
    err_buf[copyLen] = '\0';
}

static EMC_Result ApplyHubConfigUpdate(
    PluginConfig* config,
    const PluginConfig& updated,
    char* err_buf,
    uint32_t err_buf_size)
{
    if (!config || config != &g_config)
    {
        WriteHubErrorText(err_buf, err_buf_size, "invalid_config_target");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const PluginConfig previous = *config;
    *config = updated;
    if (!SaveConfigState())
    {
        *config = previous;
        WriteHubErrorText(err_buf, err_buf_size, "save_config_failed");
        return EMC_ERR_INTERNAL;
    }

    g_configNeedsWriteBack = false;
    if (!config->enabled)
    {
        DisarmPauseAfterLoad();
        ResetUiTracking();
    }

    return EMC_OK;
}

typedef bool PluginConfig::*PluginConfigBoolField;

static EMC_Result GetBoolHubSettingValue(void* user_data, int32_t* out_value, PluginConfigBoolField field)
{
    if (!user_data || !out_value)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const PluginConfig* config = static_cast<const PluginConfig*>(user_data);
    *out_value = (config->*field) ? 1 : 0;
    return EMC_OK;
}

static EMC_Result SetBoolHubSettingValue(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size,
    PluginConfigBoolField field)
{
    if (!user_data)
    {
        WriteHubErrorText(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    PluginConfig* config = static_cast<PluginConfig*>(user_data);
    PluginConfig updated = *config;
    updated.*field = value != 0;
    return ApplyHubConfigUpdate(config, updated, err_buf, err_buf_size);
}

static EMC_Result __cdecl GetEnabledSetting(void* user_data, int32_t* out_value)
{
    return GetBoolHubSettingValue(user_data, out_value, &PluginConfig::enabled);
}

static EMC_Result __cdecl SetEnabledSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return SetBoolHubSettingValue(user_data, value, err_buf, err_buf_size, &PluginConfig::enabled);
}

static EMC_Result __cdecl GetPauseDebounceMsSetting(void* user_data, int32_t* out_value)
{
    if (!user_data || !out_value)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const PluginConfig* config = static_cast<const PluginConfig*>(user_data);
    *out_value = static_cast<int32_t>(config->pauseDebounceMs);
    return EMC_OK;
}

static EMC_Result __cdecl SetPauseDebounceMsSetting(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size)
{
    if (!user_data)
    {
        WriteHubErrorText(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value < static_cast<int32_t>(kPauseDebounceMsMin) || value > static_cast<int32_t>(kPauseDebounceMsMax))
    {
        WriteHubErrorText(err_buf, err_buf_size, "pause_debounce_ms_out_of_range");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    PluginConfig* config = static_cast<PluginConfig*>(user_data);
    PluginConfig updated = *config;
    updated.pauseDebounceMs = static_cast<DWORD>(value);
    return ApplyHubConfigUpdate(config, updated, err_buf, err_buf_size);
}

static EMC_Result __cdecl GetDebugLogTransitionsSetting(void* user_data, int32_t* out_value)
{
    return GetBoolHubSettingValue(user_data, out_value, &PluginConfig::debugLogTransitions);
}

static EMC_Result __cdecl SetDebugLogTransitionsSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return SetBoolHubSettingValue(
        user_data,
        value,
        err_buf,
        err_buf_size,
        &PluginConfig::debugLogTransitions);
}

static EMC_Result __cdecl GetPauseOnTradeSetting(void* user_data, int32_t* out_value)
{
    return GetBoolHubSettingValue(user_data, out_value, &PluginConfig::pauseOnTrade);
}

static EMC_Result __cdecl SetPauseOnTradeSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return SetBoolHubSettingValue(user_data, value, err_buf, err_buf_size, &PluginConfig::pauseOnTrade);
}

static EMC_Result __cdecl GetResumeAfterTradeSetting(void* user_data, int32_t* out_value)
{
    return GetBoolHubSettingValue(user_data, out_value, &PluginConfig::resumeAfterTrade);
}

static EMC_Result __cdecl SetResumeAfterTradeSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return SetBoolHubSettingValue(user_data, value, err_buf, err_buf_size, &PluginConfig::resumeAfterTrade);
}

static EMC_Result __cdecl GetPauseOnInventoryOpenSetting(void* user_data, int32_t* out_value)
{
    return GetBoolHubSettingValue(user_data, out_value, &PluginConfig::pauseOnInventoryOpen);
}

static EMC_Result __cdecl SetPauseOnInventoryOpenSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return SetBoolHubSettingValue(
        user_data,
        value,
        err_buf,
        err_buf_size,
        &PluginConfig::pauseOnInventoryOpen);
}

static EMC_Result __cdecl GetResumeAfterInventoryCloseSetting(void* user_data, int32_t* out_value)
{
    return GetBoolHubSettingValue(user_data, out_value, &PluginConfig::resumeAfterInventoryClose);
}

static EMC_Result __cdecl SetResumeAfterInventoryCloseSetting(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size)
{
    return SetBoolHubSettingValue(
        user_data,
        value,
        err_buf,
        err_buf_size,
        &PluginConfig::resumeAfterInventoryClose);
}

static void LogModHubFallback(const char* reason)
{
    std::stringstream line;
    line << kPluginName
         << " WARN: event=mod_hub_fallback"
         << " reason=" << (reason ? reason : "unknown")
         << " result=" << g_modHubClient.LastAttemptFailureResult()
         << " use_hub_ui=0";
    ErrorLog(line.str().c_str());
}

static void LogModHubRetryEvent(const char* eventName, emc::ModHubClient::AttemptResult result)
{
    std::stringstream line;
    line << kPluginName
         << " INFO: event=" << (eventName ? eventName : "mod_hub_retry")
         << " attempt=" << g_modHubAttachRetryAttempts
         << " result_enum=" << static_cast<int32_t>(result)
         << " result=" << g_modHubClient.LastAttemptFailureResult()
         << " use_hub_ui=" << (g_modHubClient.UseHubUi() ? 1 : 0);
    DebugLog(line.str().c_str());
}

static bool ShouldLogModHubRetryEvent(emc::ModHubClient::AttemptResult result)
{
    if (g_config.debugLogTransitions)
    {
        return true;
    }

    if (g_modHubAttachRetryAttempts <= 1)
    {
        return true;
    }

    return result != emc::ModHubClient::ATTACH_FAILED;
}

static const EMC_ModDescriptorV1 kModHubDescriptor = {
    kHubNamespaceId,
    kHubNamespaceDisplayName,
    kHubModId,
    kHubModDisplayName,
    &g_config };

static const EMC_BoolSettingDefV1 kHubEnabledSetting = {
    "enabled",
    "Enabled",
    "Enable all Auto-Pause-on-Load behavior",
    &g_config,
    &GetEnabledSetting,
    &SetEnabledSetting };

static const EMC_IntSettingDefV1 kHubPauseDebounceMsSetting = {
    "pause_debounce_ms",
    "Pause debounce (ms)",
    "Minimum interval between automatic pause events",
    &g_config,
    static_cast<int32_t>(kPauseDebounceMsMin),
    static_cast<int32_t>(kPauseDebounceMsMax),
    100,
    &GetPauseDebounceMsSetting,
    &SetPauseDebounceMsSetting };

static const EMC_BoolSettingDefV1 kHubDebugLogTransitionsSetting = {
    "debug_log_transitions",
    "Debug log transitions",
    "Emit verbose transition logs for load and UI pause flows",
    &g_config,
    &GetDebugLogTransitionsSetting,
    &SetDebugLogTransitionsSetting };

static const EMC_BoolSettingDefV1 kHubPauseOnTradeSetting = {
    "pause_on_trade",
    "Pause on trade",
    "Pause the game when trade UI opens",
    &g_config,
    &GetPauseOnTradeSetting,
    &SetPauseOnTradeSetting };

static const EMC_BoolSettingDefV1 kHubResumeAfterTradeSetting = {
    "resume_after_trade",
    "Resume after trade",
    "Resume when trade UI closes if this mod paused the game",
    &g_config,
    &GetResumeAfterTradeSetting,
    &SetResumeAfterTradeSetting };

static const EMC_BoolSettingDefV1 kHubPauseOnInventoryOpenSetting = {
    "pause_on_inventory_open",
    "Pause on inventory open",
    "Pause the game when inventory opens",
    &g_config,
    &GetPauseOnInventoryOpenSetting,
    &SetPauseOnInventoryOpenSetting };

static const EMC_BoolSettingDefV1 kHubResumeAfterInventoryCloseSetting = {
    "resume_after_inventory_close",
    "Resume after inventory close",
    "Resume when inventory closes if this mod paused the game",
    &g_config,
    &GetResumeAfterInventoryCloseSetting,
    &SetResumeAfterInventoryCloseSetting };

static const emc::ModHubClientSettingRowV1 kModHubSettingRows[] = {
    { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kHubEnabledSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &kHubPauseDebounceMsSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kHubDebugLogTransitionsSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kHubPauseOnTradeSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kHubResumeAfterTradeSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kHubPauseOnInventoryOpenSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kHubResumeAfterInventoryCloseSetting }
};

static const emc::ModHubClientTableRegistrationV1 kModHubTableRegistration = {
    &kModHubDescriptor,
    kModHubSettingRows,
    static_cast<uint32_t>(sizeof(kModHubSettingRows) / sizeof(kModHubSettingRows[0])) };

static void ConfigureModHubClient()
{
    emc::ModHubClient::Config hubConfig;
    hubConfig.table_registration = &kModHubTableRegistration;
    g_modHubClient.SetConfig(hubConfig);
}

static void StartModHubClient()
{
    g_modHubAttachRetryActive = false;
    g_modHubAttachRetryAttempts = 0;
    g_modHubAttachRetryLastAttemptMs = GetTickCount();

    const emc::ModHubClient::AttemptResult result = g_modHubClient.OnStartup();
    if (result == emc::ModHubClient::ATTACH_SUCCESS)
    {
        DebugLog("Auto-Pause-on-Load INFO: event=mod_hub_attached use_hub_ui=1");
        return;
    }

    if (result == emc::ModHubClient::ATTACH_FAILED)
    {
        LogModHubFallback("get_api_failed");
        g_modHubAttachRetryActive = true;
        return;
    }

    if (result == emc::ModHubClient::REGISTRATION_FAILED)
    {
        LogModHubFallback("register_mod_or_setting_failed");
        return;
    }

    LogModHubFallback("invalid_client_configuration");
}

static void TickModHubAttachRetry()
{
    if (!g_modHubAttachRetryActive || g_modHubClient.UseHubUi())
    {
        return;
    }

    if (g_modHubAttachRetryAttempts >= kModHubAttachRetryMaxAttempts)
    {
        g_modHubAttachRetryActive = false;
        ErrorLog("Auto-Pause-on-Load WARN: event=mod_hub_retry_stopped reason=max_attempts_reached");
        return;
    }

    const DWORD nowMs = GetTickCount();
    if (!DebounceWindowElapsed(nowMs, g_modHubAttachRetryLastAttemptMs, kModHubAttachRetryIntervalMs))
    {
        return;
    }

    ++g_modHubAttachRetryAttempts;
    g_modHubAttachRetryLastAttemptMs = nowMs;

    const emc::ModHubClient::AttemptResult result = g_modHubClient.OnStartup();
    if (ShouldLogModHubRetryEvent(result))
    {
        LogModHubRetryEvent("mod_hub_retry_attempt", result);
    }
    if (result == emc::ModHubClient::ATTACH_SUCCESS)
    {
        g_modHubAttachRetryActive = false;
        DebugLog("Auto-Pause-on-Load INFO: event=mod_hub_retry_success");
        return;
    }

    if (result == emc::ModHubClient::REGISTRATION_FAILED)
    {
        g_modHubAttachRetryActive = false;
        LogModHubFallback("register_mod_or_setting_failed");
        return;
    }
}

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
    g_state.armTimestampMs = 0;
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
        std::stringstream info;
        info << "Auto-Pause-on-Load INFO: paused_after_load=true source=" << reason;
        DebugLog(info.str().c_str());
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

    if (!g_hasSaveLoadHook || !g_state.pauseArmed)
    {
        return;
    }

    if (g_state.armTimestampMs != 0 && DebounceWindowElapsed(nowMs, g_state.armTimestampMs, kArmedTimeoutMs))
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

    if (isLoadingSave)
    {
        if (!g_state.loadInProgress && g_config.debugLogTransitions)
        {
            DebugLog("Auto-Pause-on-Load DEBUG: load started");
        }
        g_state.loadInProgress = true;
        g_state.loadSignalSeenAfterArm = true;
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

static void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    PlayerInterface_updateUT_orig(thisptr);
    TickModHubAttachRetry();
    TickPauseOnLoad();
    TickPauseOnTradeAndInventory();
}

static void SaveManager_loadByInfo_hook(SaveManager* thisptr, const SaveInfo& saveInfo, bool resetPos)
{
    ArmPauseAfterLoad("SaveManager::load(saveInfo,resetPos)");
    if (SaveManager_loadByInfo_orig)
    {
        SaveManager_loadByInfo_orig(thisptr, saveInfo, resetPos);
    }
}

static void SaveManager_loadByName_hook(SaveManager* thisptr, const std::string& saveName)
{
    ArmPauseAfterLoad("SaveManager::load(name)");
    if (SaveManager_loadByName_orig)
    {
        SaveManager_loadByName_orig(thisptr, saveName);
    }
}

__declspec(dllexport) void startPlugin()
{
    DebugLog("Auto-Pause-on-Load: startPlugin()");

    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    if (platform == KenshiLib::BinaryVersion::UNKNOWN || (version != "1.0.65" && version != "1.0.68"))
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

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        &PlayerInterface_updateUT_orig))
    {
        ErrorLog("Auto-Pause-on-Load: Could not hook PlayerInterface::updateUT");
        return;
    }

    g_hasSaveLoadHook = false;
    if (KenshiLib::SUCCESS == KenshiLib::AddHook(
        KenshiLib::GetRealAddress(static_cast<void (SaveManager::*)(const SaveInfo&, bool)>(&SaveManager::load)),
        SaveManager_loadByInfo_hook,
        &SaveManager_loadByInfo_orig))
    {
        g_hasSaveLoadHook = true;
    }
    else
    {
        ErrorLog("Auto-Pause-on-Load: Could not hook SaveManager::load(SaveInfo,bool)");
    }

    if (KenshiLib::SUCCESS == KenshiLib::AddHook(
        KenshiLib::GetRealAddress(static_cast<void (SaveManager::*)(const std::string&)>(&SaveManager::load)),
        SaveManager_loadByName_hook,
        &SaveManager_loadByName_orig))
    {
        g_hasSaveLoadHook = true;
    }
    else
    {
        ErrorLog("Auto-Pause-on-Load: Could not hook SaveManager::load(std::string)");
    }

    if (!g_hasSaveLoadHook)
    {
        ErrorLog("Auto-Pause-on-Load: no SaveManager load hooks active; feature disabled");
    }

    ConfigureModHubClient();
    StartModHubClient();

    std::stringstream info;
    info << "Auto-Pause-on-Load INFO: initialized (enabled=" << (g_config.enabled ? "true" : "false");
    if (g_config.debugLogTransitions)
    {
        info << ", pause_debounce_ms=" << g_config.pauseDebounceMs
             << ", pause_on_trade=" << (g_config.pauseOnTrade ? "true" : "false")
             << ", resume_after_trade=" << (g_config.resumeAfterTrade ? "true" : "false")
             << ", pause_on_inventory_open=" << (g_config.pauseOnInventoryOpen ? "true" : "false")
             << ", resume_after_inventory_close=" << (g_config.resumeAfterInventoryClose ? "true" : "false");
    }
    info << ", save_load_hooks=" << (g_hasSaveLoadHook ? "true" : "false")
         << ", hub_ui=" << (g_modHubClient.UseHubUi() ? "true" : "false") << ")";
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
