#include <Debug.h>

#include <core/Functions.h>

#include <kenshi/Character.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/Kenshi.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/SaveManager.h>
#include <kenshi/TitleScreen.h>
#include <kenshi/util/lektor.h>
#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_RenderManager.h>
#include <mygui/MyGUI_TabControl.h>
#include <mygui/MyGUI_TabItem.h>
#include <mygui/MyGUI_Widget.h>

#include <Windows.h>

#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

struct PluginConfig
{
    bool enabled;
    DWORD pauseDebounceMs;
    bool debugLogTransitions;
    bool enableDeleteAllJobsSelectedMemberAction;
    bool enableExperimentalSingleJobDelete;
    bool logSelectedMemberJobSnapshot;
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
};

static const char* kPluginName = "Job-B-Gone";
static const char* kConfigFileName = "mod-config.json";
static const DWORD kTickAliveIntervalMs = 5000;
static const DWORD kNoSignalDisarmMs = 1500;
static const DWORD kArmedTimeoutMs = 60000;
static const char* kPluginTabName = "Job-B-Gone";
static const char* kPluginPanelName = "job_b_gone_options";

static PluginConfig g_config = { true, 2000, false, true, false, true };
static RuntimeState g_state = { false, false, false, 0, 0, 0, false };

static std::string g_settingsPath;
static bool g_hasSaveLoadHook = false;
static bool g_configNeedsWriteBack = false;
static uintptr_t g_selectedMemberInfoWindowAddress = 0;

static void (*PlayerInterface_updateUT_orig)(PlayerInterface*) = 0;
static void (*SaveManager_loadByInfo_orig)(SaveManager*, const SaveInfo&, bool) = 0;
static void (*SaveManager_loadByName_orig)(SaveManager*, const std::string&) = 0;

class ToolTip;
class ForgottenGUI;
class DatapanelGUI;

class OptionsWindow : public GUIWindow, public wraps::BaseLayout
{
public:
    char _0xd0;
    lektor<std::string> _0xd8;
    int _0xf0;
    void* _0xf8;
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

typedef DatapanelGUI* (*FnCreateDatapanel)(ForgottenGUI*, const std::string&, MyGUI::Widget*, bool);
typedef void (*FnOptionsInit)(OptionsWindow*);

static FnCreateDatapanel g_fnCreateDatapanel = 0;
static FnOptionsInit g_fnOptionsInit = 0;
static FnOptionsInit g_fnOptionsInitOrig = 0;
static ForgottenGUI* g_ptrKenshiGUI = 0;

static MyGUI::Button* g_deleteAllJobsSelectedMemberButton = 0;
static MyGUI::Widget* g_deleteAllJobsSelectedMemberButtonParent = 0;
static PlayerInterface* g_lastPlayerInterface = 0;
static bool g_loggedSelectedMemberButtonCreateFailure = false;
static bool g_lastLoggedHasSelectedMemberForButton = false;
static bool g_lastLoggedPanelWidgetAvailable = false;
static bool g_lastLoggedButtonVisibleState = false;
static bool g_lastLoggedButtonExists = false;
static bool g_lastLoggedInfoPanelNull = false;
static DWORD g_lastInfoPanelNullLogMs = 0;
static bool g_buttonAttachedToGlobalLayer = false;
static bool g_pendingSelectedMemberUiRefresh = false;
static DWORD g_pendingSelectedMemberUiRefreshStartMs = 0;
static DWORD g_lastSelectedMemberUiRefreshAttemptMs = 0;
static int g_selectedMemberUiRefreshAttempts = 0;

static bool DebounceWindowElapsed(DWORD nowMs, DWORD lastEventMs, DWORD minGapMs);
static void DisarmPauseAfterLoad();
static bool InitPluginMenuFunctions(unsigned int platform, const std::string& version, uintptr_t baseAddr);
static void OptionsWindowInitHook(OptionsWindow* self);
static void EnsureSelectedMemberJobPanelButton(PlayerInterface* thisptr);
static Character* ResolveSelectedMember();
static bool TryResolveSelectedMemberForDebug();
static bool TryGetPermajobCount(Character* member, int* countOut);
static bool TryGetPermajobRow(Character* member, int slot, TaskType* taskTypeOut, std::string* nameOut, uintptr_t* taskDataPtrOut);
static void LogSelectedMemberJobSnapshot(Character* member, const char* phase);
static bool TryDeleteAllJobsForSelectedMember(int* beforeOut, int* afterOut, int* deletedOut, const char** resultOut);
static bool RefreshSelectedMemberUi(const char* source);
static void ArmSelectedMemberUiRefresh(const char* source);
static void TickSelectedMemberUiRefresh();
static void OnDeleteAllJobsSelectedMemberButtonClicked(MyGUI::Widget*);
static MyGUI::IntCoord GetFallbackButtonCoord();

struct ConfigParseDiagnostics
{
    bool foundEnabled;
    bool invalidEnabled;
    bool foundPauseDebounceMs;
    bool invalidPauseDebounceMs;
    bool clampedPauseDebounceMs;
    bool foundDebugLogTransitions;
    bool invalidDebugLogTransitions;
    bool foundEnableDeleteAllJobsSelectedMemberAction;
    bool invalidEnableDeleteAllJobsSelectedMemberAction;
    bool foundEnableExperimentalSingleJobDelete;
    bool invalidEnableExperimentalSingleJobDelete;
    bool foundLogSelectedMemberJobSnapshot;
    bool invalidLogSelectedMemberJobSnapshot;
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
    diagnostics->foundEnableDeleteAllJobsSelectedMemberAction = false;
    diagnostics->invalidEnableDeleteAllJobsSelectedMemberAction = false;
    diagnostics->foundEnableExperimentalSingleJobDelete = false;
    diagnostics->invalidEnableExperimentalSingleJobDelete = false;
    diagnostics->foundLogSelectedMemberJobSnapshot = false;
    diagnostics->invalidLogSelectedMemberJobSnapshot = false;
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
        else if (key == "enable_delete_all_jobs_selected_member_action")
        {
            bool parsedBool = false;
            size_t valuePos = pos;
            if (ParseJsonBoolValue(body, &valuePos, &parsedBool))
            {
                diagnostics->foundEnableDeleteAllJobsSelectedMemberAction = true;
                configOut->enableDeleteAllJobsSelectedMemberAction = parsedBool;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidEnableDeleteAllJobsSelectedMemberAction = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "enable_experimental_single_job_delete")
        {
            bool parsedBool = false;
            size_t valuePos = pos;
            if (ParseJsonBoolValue(body, &valuePos, &parsedBool))
            {
                diagnostics->foundEnableExperimentalSingleJobDelete = true;
                configOut->enableExperimentalSingleJobDelete = parsedBool;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidEnableExperimentalSingleJobDelete = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "log_selected_member_job_snapshot")
        {
            bool parsedBool = false;
            size_t valuePos = pos;
            if (ParseJsonBoolValue(body, &valuePos, &parsedBool))
            {
                diagnostics->foundLogSelectedMemberJobSnapshot = true;
                configOut->logSelectedMemberJobSnapshot = parsedBool;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidLogSelectedMemberJobSnapshot = true;
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
    PluginConfig parsedConfig = { true, 2000, false, true, false, true };
    ConfigParseDiagnostics diagnostics;
    ResetConfigParseDiagnostics(&diagnostics);

    if (!ParseConfigJson(
            "{\"enabled\":false,\"pause_debounce_ms\":1234,\"debug_log_transitions\":true,"
            "\"enable_delete_all_jobs_selected_member_action\":false,"
            "\"enable_experimental_single_job_delete\":true,"
            "\"log_selected_member_job_snapshot\":false}",
            &parsedConfig,
            &diagnostics))
    {
        return false;
    }
    if (parsedConfig.enabled
        || parsedConfig.pauseDebounceMs != 1234
        || !parsedConfig.debugLogTransitions
        || parsedConfig.enableDeleteAllJobsSelectedMemberAction
        || !parsedConfig.enableExperimentalSingleJobDelete
        || parsedConfig.logSelectedMemberJobSnapshot)
    {
        return false;
    }

    const std::string bomJson = std::string("\xEF\xBB\xBF")
        + "{\"enabled\":true,\"pause_debounce_ms\":2000,\"debug_log_transitions\":false,"
          "\"enable_delete_all_jobs_selected_member_action\":true,"
          "\"enable_experimental_single_job_delete\":false,"
          "\"log_selected_member_job_snapshot\":true}";
    parsedConfig.enabled = false;
    parsedConfig.pauseDebounceMs = 1;
    parsedConfig.debugLogTransitions = true;
    parsedConfig.enableDeleteAllJobsSelectedMemberAction = false;
    parsedConfig.enableExperimentalSingleJobDelete = true;
    parsedConfig.logSelectedMemberJobSnapshot = false;
    ResetConfigParseDiagnostics(&diagnostics);
    if (!ParseConfigJson(bomJson, &parsedConfig, &diagnostics))
    {
        return false;
    }
    if (!parsedConfig.enabled
        || parsedConfig.pauseDebounceMs != 2000
        || parsedConfig.debugLogTransitions
        || !parsedConfig.enableDeleteAllJobsSelectedMemberAction
        || parsedConfig.enableExperimentalSingleJobDelete
        || !parsedConfig.logSelectedMemberJobSnapshot)
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
        ErrorLog("Job-B-Gone WARN: invalid/missing key \"enabled\"; using default");
    }
    if (!diagnostics.foundPauseDebounceMs || diagnostics.invalidPauseDebounceMs)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: invalid/missing key \"pause_debounce_ms\"; using default");
    }
    if (diagnostics.clampedPauseDebounceMs)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: \"pause_debounce_ms\" exceeded max; clamped to 600000");
    }
    if (!diagnostics.foundDebugLogTransitions || diagnostics.invalidDebugLogTransitions)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: invalid/missing key \"debug_log_transitions\"; using default");
    }
    if (!diagnostics.foundEnableDeleteAllJobsSelectedMemberAction
        || diagnostics.invalidEnableDeleteAllJobsSelectedMemberAction)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: invalid/missing key \"enable_delete_all_jobs_selected_member_action\"; using default");
    }
    if (!diagnostics.foundEnableExperimentalSingleJobDelete
        || diagnostics.invalidEnableExperimentalSingleJobDelete)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: invalid/missing key \"enable_experimental_single_job_delete\"; using default");
    }
    if (!diagnostics.foundLogSelectedMemberJobSnapshot || diagnostics.invalidLogSelectedMemberJobSnapshot)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: invalid/missing key \"log_selected_member_job_snapshot\"; using default");
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
    out << "  \"enable_delete_all_jobs_selected_member_action\": "
        << (config.enableDeleteAllJobsSelectedMemberAction ? "true" : "false") << ",\n";
    out << "  \"enable_experimental_single_job_delete\": "
        << (config.enableExperimentalSingleJobDelete ? "true" : "false") << ",\n";
    out << "  \"log_selected_member_job_snapshot\": "
        << (config.logSelectedMemberJobSnapshot ? "true" : "false") << "\n";
    out << "}\n";

    return true;
}

static void LoadConfigState()
{
    g_configNeedsWriteBack = false;
    g_config.enabled = true;
    g_config.pauseDebounceMs = 2000;
    g_config.debugLogTransitions = false;
    g_config.enableDeleteAllJobsSelectedMemberAction = true;
    g_config.enableExperimentalSingleJobDelete = false;
    g_config.logSelectedMemberJobSnapshot = true;

    if (g_settingsPath.empty())
    {
        return;
    }

    bool foundConfigFile = false;
    bool needsWriteBack = false;
    if (!ReadConfigFromFile(g_settingsPath, &g_config, &foundConfigFile, &needsWriteBack))
    {
        ErrorLog("Job-B-Gone ERROR: failed to read mod-config.json; using defaults and rewriting file");
        g_configNeedsWriteBack = true;
        return;
    }

    g_configNeedsWriteBack = (!foundConfigFile) || needsWriteBack;
    if (!foundConfigFile)
    {
        DebugLog("Job-B-Gone INFO: mod-config.json not found; using defaults");
    }

    std::stringstream info;
    info << "Job-B-Gone INFO: loaded config enabled=" << (g_config.enabled ? "true" : "false")
         << " pause_debounce_ms=" << g_config.pauseDebounceMs
         << " debug_log_transitions=" << (g_config.debugLogTransitions ? "true" : "false")
         << " enable_delete_all_jobs_selected_member_action="
         << (g_config.enableDeleteAllJobsSelectedMemberAction ? "true" : "false")
         << " enable_experimental_single_job_delete="
         << (g_config.enableExperimentalSingleJobDelete ? "true" : "false")
         << " log_selected_member_job_snapshot="
         << (g_config.logSelectedMemberJobSnapshot ? "true" : "false");
    DebugLog(info.str().c_str());
}

static bool SaveConfigState()
{
    if (g_settingsPath.empty())
    {
        ErrorLog("Job-B-Gone ERROR: settings path is empty; cannot save mod-config.json");
        return false;
    }

    if (!SaveConfigToFile(g_settingsPath, g_config))
    {
        ErrorLog("Job-B-Gone ERROR: failed to save mod-config.json");
        return false;
    }

    return true;
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
        logline << "Job-B-Gone DEBUG: armed from " << source;
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
            ErrorLog("Job-B-Gone WARN: game world unavailable while waiting for save-load completion");
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
        ErrorLog("Job-B-Gone WARN: exception while querying save-load state");
        return false;
    }
}

static bool ForcePauseTrue()
{
    if (!ou)
    {
        if (!g_state.loggedWorldUnavailable)
        {
            ErrorLog("Job-B-Gone WARN: game world unavailable; cannot force pause");
            g_state.loggedWorldUnavailable = true;
        }
        return false;
    }

    g_state.loggedWorldUnavailable = false;
    __try
    {
        ou->userPause(true);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ErrorLog("Job-B-Gone WARN: exception while forcing paused state");
        return false;
    }
}

static void TryPauseAndDisarm(DWORD nowMs, const char* reason)
{
    if (!DebounceWindowElapsed(nowMs, g_state.lastPauseMs, g_config.pauseDebounceMs))
    {
        if (g_config.debugLogTransitions)
        {
            DebugLog("Job-B-Gone DEBUG: pause skipped (debounce)");
        }
        DisarmPauseAfterLoad();
        return;
    }

    if (ForcePauseTrue())
    {
        g_state.lastPauseMs = nowMs;
        std::stringstream info;
        info << "Job-B-Gone INFO: paused_after_load=true source=" << reason;
        DebugLog(info.str().c_str());
    }

    DisarmPauseAfterLoad();
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
            DebugLog("Job-B-Gone DEBUG: tick alive");
            g_state.lastTickAliveLogMs = nowMs;
        }
    }

    if (!g_hasSaveLoadHook || !g_state.pauseArmed)
    {
        return;
    }

    if (g_state.armTimestampMs != 0 && DebounceWindowElapsed(nowMs, g_state.armTimestampMs, kArmedTimeoutMs))
    {
        ErrorLog("Job-B-Gone WARN: armed pause timed out before load completion");
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
            DebugLog("Job-B-Gone DEBUG: load started");
        }
        g_state.loadInProgress = true;
        g_state.loadSignalSeenAfterArm = true;
        return;
    }

    if (g_state.loadInProgress)
    {
        if (g_config.debugLogTransitions)
        {
            DebugLog("Job-B-Gone DEBUG: load finished");
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
            DebugLog("Job-B-Gone DEBUG: no load signal observed; disarming");
        }
        DisarmPauseAfterLoad();
    }
}

static bool RefreshSelectedMemberUi(const char* source)
{
    if (!g_lastPlayerInterface)
    {
        return false;
    }

    bool success = false;
    bool hadSelectedBefore = false;

    __try
    {
        const hand selectedBefore = g_lastPlayerInterface->selectedCharacter;
        if (selectedBefore && selectedBefore.isValid())
        {
            hadSelectedBefore = true;
            if (selectedBefore.getCharacter())
            {
                // Non-invasive UI nudge: avoid selection API calls that can clear focus.
                g_lastPlayerInterface->selectedObjectsChangedThisFrame = true;
                success = true;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ErrorLog("Job-B-Gone WARN: exception while refreshing selected member UI");
        return false;
    }

    if (g_config.debugLogTransitions)
    {
        char logline[256] = { 0 };
        _snprintf(
            logline,
            sizeof(logline) - 1,
            "Job-B-Gone DEBUG: selected_member_ui_refresh source=%s had_selected_before=%s success=%s",
            source ? source : "unknown",
            hadSelectedBefore ? "true" : "false",
            success ? "true" : "false");
        DebugLog(logline);
    }

    return success;
}

static void ArmSelectedMemberUiRefresh(const char* source)
{
    g_pendingSelectedMemberUiRefresh = true;
    g_pendingSelectedMemberUiRefreshStartMs = GetTickCount();
    g_lastSelectedMemberUiRefreshAttemptMs = 0;
    g_selectedMemberUiRefreshAttempts = 0;

    std::stringstream info;
    info << "Job-B-Gone DEBUG: armed_selected_member_ui_refresh source="
         << (source ? source : "unknown");
    DebugLog(info.str().c_str());
}

static void TickSelectedMemberUiRefresh()
{
    if (!g_pendingSelectedMemberUiRefresh)
    {
        return;
    }

    const DWORD nowMs = GetTickCount();
    if (g_pendingSelectedMemberUiRefreshStartMs != 0
        && DebounceWindowElapsed(nowMs, g_pendingSelectedMemberUiRefreshStartMs, 1000))
    {
        g_pendingSelectedMemberUiRefresh = false;
        DebugLog("Job-B-Gone DEBUG: selected_member_ui_refresh timeout");
        return;
    }

    if (g_lastSelectedMemberUiRefreshAttemptMs != 0
        && !DebounceWindowElapsed(nowMs, g_lastSelectedMemberUiRefreshAttemptMs, 100))
    {
        return;
    }

    g_lastSelectedMemberUiRefreshAttemptMs = nowMs;
    ++g_selectedMemberUiRefreshAttempts;
    if (RefreshSelectedMemberUi("deferred_tick"))
    {
        g_pendingSelectedMemberUiRefresh = false;
        return;
    }

    if (g_selectedMemberUiRefreshAttempts >= 5)
    {
        g_pendingSelectedMemberUiRefresh = false;
        DebugLog("Job-B-Gone DEBUG: selected_member_ui_refresh exhausted retries");
    }
}

static void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    PlayerInterface_updateUT_orig(thisptr);
    TickPauseOnLoad();
    TickSelectedMemberUiRefresh();
    EnsureSelectedMemberJobPanelButton(thisptr);
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

static bool InitPluginMenuFunctions(unsigned int platform, const std::string& version, uintptr_t baseAddr)
{
    g_selectedMemberInfoWindowAddress = 0;

    if (platform == 1)
    {
        if (version == "1.0.65")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(baseAddr + 0x003F0120);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(baseAddr + 0x0073F4B0);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(baseAddr + 0x02132750);
            g_selectedMemberInfoWindowAddress = baseAddr + 0x0212CB70;
            return true;
        }
        if (version == "1.0.68")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(baseAddr + 0x003F0260);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(baseAddr + 0x0073FFE0);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(baseAddr + 0x021337B0);
            g_selectedMemberInfoWindowAddress = baseAddr + 0x0212CB70;
            return true;
        }
    }
    else if (platform == 0)
    {
        if (version == "1.0.65")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(baseAddr + 0x003EFD40);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(baseAddr + 0x0073EE10);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(baseAddr + 0x021306C0);
            g_selectedMemberInfoWindowAddress = baseAddr + 0x0212CB70;
            return true;
        }
        if (version == "1.0.68")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(baseAddr + 0x003EFC00);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(baseAddr + 0x0073F980);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(baseAddr + 0x021326E0);
            g_selectedMemberInfoWindowAddress = baseAddr + 0x0212CB70;
            return true;
        }
    }

    return false;
}

static Character* ResolveSelectedMember()
{
    if (!g_lastPlayerInterface)
    {
        return 0;
    }

    __try
    {
        const hand selectedMemberHandle = g_lastPlayerInterface->selectedCharacter;
        if (!selectedMemberHandle || !selectedMemberHandle.isValid())
        {
            return 0;
        }

        return selectedMemberHandle.getCharacter();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ErrorLog("Job-B-Gone WARN: exception while resolving selected member");
        return 0;
    }
}

static bool TryResolveSelectedMemberForDebug()
{
    return ResolveSelectedMember() != 0;
}

static bool TryGetPermajobCount(Character* member, int* countOut)
{
    if (!member || !countOut)
    {
        return false;
    }

    __try
    {
        int count = member->getPermajobCount();
        if (count < 0)
        {
            count = 0;
        }
        *countOut = count;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ErrorLog("Job-B-Gone WARN: exception while reading permajob count");
        return false;
    }
}

static bool TryGetPermajobRow(
    Character* member,
    int slot,
    TaskType* taskTypeOut,
    std::string* nameOut,
    uintptr_t* taskDataPtrOut)
{
    if (!member || slot < 0)
    {
        return false;
    }

    __try
    {
        const int count = member->getPermajobCount();
        if (slot >= count)
        {
            return false;
        }

        if (taskTypeOut)
        {
            *taskTypeOut = member->getPermajob(slot);
        }
        if (nameOut)
        {
            *nameOut = member->getPermajobName(slot);
        }
        if (taskDataPtrOut)
        {
            const Tasker* taskData = member->getPermajobData(slot);
            *taskDataPtrOut = reinterpret_cast<uintptr_t>(taskData);
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ErrorLog("Job-B-Gone WARN: exception while reading permajob row");
        return false;
    }
}

static void LogSelectedMemberJobSnapshot(Character* member, const char* phase)
{
    if (!g_config.logSelectedMemberJobSnapshot || !member)
    {
        return;
    }

    int count = 0;
    if (!TryGetPermajobCount(member, &count))
    {
        std::stringstream warn;
        warn << "Job-B-Gone WARN: selected_member_job_snapshot_failed phase="
             << (phase ? phase : "unknown")
             << " reason=count_read_failed";
        ErrorLog(warn.str().c_str());
        return;
    }

    std::stringstream summary;
    summary << "Job-B-Gone INFO: selected_member_job_snapshot phase="
            << (phase ? phase : "unknown")
            << " selected_member_ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(member)
            << std::dec << " count=" << count;
    DebugLog(summary.str().c_str());

    for (int slot = 0; slot < count; ++slot)
    {
        TaskType taskType = static_cast<TaskType>(0);
        std::string taskName;
        uintptr_t taskDataPtr = 0;
        if (!TryGetPermajobRow(member, slot, &taskType, &taskName, &taskDataPtr))
        {
            std::stringstream warn;
            warn << "Job-B-Gone WARN: selected_member_job_snapshot_row_failed phase="
                 << (phase ? phase : "unknown")
                 << " slot=" << slot;
            ErrorLog(warn.str().c_str());
            continue;
        }

        std::stringstream rowLog;
        rowLog << "Job-B-Gone INFO: selected_member_job_snapshot_row phase="
               << (phase ? phase : "unknown")
               << " slot=" << slot
               << " task_type=" << static_cast<int>(taskType)
               << " task_name=\"" << taskName << "\""
               << " task_data_ptr=0x" << std::hex << taskDataPtr;
        DebugLog(rowLog.str().c_str());
    }
}

static bool TryDeleteAllJobsForSelectedMember(int* beforeOut, int* afterOut, int* deletedOut, const char** resultOut)
{
    if (beforeOut)
    {
        *beforeOut = -1;
    }
    if (afterOut)
    {
        *afterOut = -1;
    }
    if (deletedOut)
    {
        *deletedOut = -1;
    }
    if (resultOut)
    {
        *resultOut = "not_run";
    }

    Character* member = ResolveSelectedMember();
    if (!member)
    {
        if (resultOut)
        {
            *resultOut = "no_selected_member";
        }
        return false;
    }

    int beforeCount = 0;
    if (!TryGetPermajobCount(member, &beforeCount))
    {
        if (resultOut)
        {
            *resultOut = "before_count_failed";
        }
        return false;
    }
    if (beforeOut)
    {
        *beforeOut = beforeCount;
    }

    LogSelectedMemberJobSnapshot(member, "before_delete_all");

    __try
    {
        member->clearPermajobs();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (resultOut)
        {
            *resultOut = "clear_exception";
        }
        return false;
    }

    int afterCount = 0;
    if (!TryGetPermajobCount(member, &afterCount))
    {
        if (resultOut)
        {
            *resultOut = "after_count_failed";
        }
        return false;
    }
    if (afterOut)
    {
        *afterOut = afterCount;
    }

    const int deletedCount = (beforeCount >= afterCount) ? (beforeCount - afterCount) : 0;
    if (deletedOut)
    {
        *deletedOut = deletedCount;
    }

    LogSelectedMemberJobSnapshot(member, "after_delete_all");

    if (beforeCount == 0 && afterCount == 0)
    {
        if (resultOut)
        {
            *resultOut = "already_empty";
        }
        return true;
    }

    if (afterCount == 0)
    {
        if (resultOut)
        {
            *resultOut = "cleared_all";
        }
        return true;
    }

    if (resultOut)
    {
        *resultOut = "non_empty_after_clear";
    }
    return false;
}

static void OnDeleteAllJobsSelectedMemberButtonClicked(MyGUI::Widget*)
{
    const bool actionEnabled = g_config.enableDeleteAllJobsSelectedMemberAction;
    int beforeCount = -1;
    int afterCount = -1;
    int deletedCount = -1;
    const char* result = "disabled_by_config";
    bool success = false;

    if (actionEnabled)
    {
        success = TryDeleteAllJobsForSelectedMember(&beforeCount, &afterCount, &deletedCount, &result);
        if (!result)
        {
            result = success ? "success" : "failed";
        }
        if (success)
        {
            RefreshSelectedMemberUi("post_delete_immediate");
            ArmSelectedMemberUiRefresh("post_delete_deferred");
        }
    }

    std::stringstream logline;
    logline << "Job-B-Gone INFO: action=delete_all_jobs_selected_member"
            << " success=" << (success ? "true" : "false")
            << " result=" << result
            << " before=" << beforeCount
            << " after=" << afterCount
            << " deleted=" << deletedCount
            << " action_enabled=" << (actionEnabled ? "true" : "false")
            << " anchor_mode=" << (g_buttonAttachedToGlobalLayer ? "global_fallback" : "selected_member_panel");
    DebugLog(logline.str().c_str());
}

static MyGUI::IntCoord GetFallbackButtonCoord()
{
    const int width = 360;
    const int height = 36;

    int left = 1200;
    int top = 760;

    MyGUI::RenderManager* renderManager = MyGUI::RenderManager::getInstancePtr();
    if (renderManager)
    {
        const MyGUI::IntSize view = renderManager->getViewSize();
        if (view.width > 0 && view.height > 0)
        {
            // Anchor near the lower-right jobs panel with conservative insets.
            left = view.width - 650;
            top = view.height - 260;
        }
    }

    if (left < 20)
    {
        left = 20;
    }
    if (top < 20)
    {
        top = 20;
    }

    return MyGUI::IntCoord(left, top, width, height);
}

static void DestroySelectedMemberJobPanelButton()
{
    if (!g_deleteAllJobsSelectedMemberButton)
    {
        g_deleteAllJobsSelectedMemberButtonParent = 0;
        return;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui)
    {
        gui->destroyWidget(g_deleteAllJobsSelectedMemberButton);
    }

    g_deleteAllJobsSelectedMemberButton = 0;
    g_deleteAllJobsSelectedMemberButtonParent = 0;
    g_buttonAttachedToGlobalLayer = false;
}

static void EnsureSelectedMemberJobPanelButton(PlayerInterface* thisptr)
{
    g_lastPlayerInterface = thisptr;

    DatapanelGUI* selectedMemberInfoPanel = 0;
    if (g_selectedMemberInfoWindowAddress != 0)
    {
        DatapanelGUI** infoWindowSlot = reinterpret_cast<DatapanelGUI**>(g_selectedMemberInfoWindowAddress);
        if (infoWindowSlot)
        {
            selectedMemberInfoPanel = *infoWindowSlot;
        }
    }

    const bool infoPanelNull = (selectedMemberInfoPanel == 0);
    if (infoPanelNull != g_lastLoggedInfoPanelNull)
    {
        std::stringstream logline;
        logline << "Job-B-Gone DEBUG: selected_member_info_panel_null="
                << (infoPanelNull ? "true" : "false")
                << " addr=0x" << std::hex << g_selectedMemberInfoWindowAddress;
        DebugLog(logline.str().c_str());
        g_lastLoggedInfoPanelNull = infoPanelNull;
        g_lastInfoPanelNullLogMs = GetTickCount();
    }
    else if (infoPanelNull)
    {
        const DWORD nowMs = GetTickCount();
        if (DebounceWindowElapsed(nowMs, g_lastInfoPanelNullLogMs, 5000))
        {
            std::stringstream logline;
            logline << "Job-B-Gone DEBUG: selected_member_info_panel_null_still_true addr=0x"
                    << std::hex << g_selectedMemberInfoWindowAddress;
            DebugLog(logline.str().c_str());
            g_lastInfoPanelNullLogMs = nowMs;
        }
    }

    MyGUI::Widget* panelWidget = selectedMemberInfoPanel ? selectedMemberInfoPanel->getWidget() : 0;
    const bool panelWidgetAvailable = (panelWidget != 0);
    if (panelWidgetAvailable != g_lastLoggedPanelWidgetAvailable)
    {
        std::stringstream logline;
        logline << "Job-B-Gone DEBUG: selected_member_panel_widget_available="
                << (panelWidgetAvailable ? "true" : "false")
                << " panel_ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(selectedMemberInfoPanel)
                << " widget_ptr=0x" << reinterpret_cast<uintptr_t>(panelWidget)
                << " info_window_addr=0x" << g_selectedMemberInfoWindowAddress;
        DebugLog(logline.str().c_str());
        g_lastLoggedPanelWidgetAvailable = panelWidgetAvailable;
    }

    if (!panelWidget)
    {
        // Fallback: if native selected-member panel isn't available, attach button to a
        // global layer so the feature remains visible/testable.
        if (!g_deleteAllJobsSelectedMemberButton || !g_buttonAttachedToGlobalLayer)
        {
            DestroySelectedMemberJobPanelButton();
            MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
            if (gui)
            {
                const MyGUI::IntCoord coord = GetFallbackButtonCoord();
                g_deleteAllJobsSelectedMemberButton = gui->createWidget<MyGUI::Button>(
                    "Kenshi_Button1",
                    coord,
                    MyGUI::Align::Default,
                    "Main");
                if (!g_deleteAllJobsSelectedMemberButton)
                {
                    g_deleteAllJobsSelectedMemberButton = gui->createWidget<MyGUI::Button>(
                        "Kenshi_Button1",
                        coord,
                        MyGUI::Align::Default,
                        "Overlapped");
                }
            }

            if (g_deleteAllJobsSelectedMemberButton)
            {
                g_buttonAttachedToGlobalLayer = true;
                g_loggedSelectedMemberButtonCreateFailure = false;
                g_deleteAllJobsSelectedMemberButtonParent = 0;
                g_deleteAllJobsSelectedMemberButton->setCaption("Delete All Jobs (Selected Member)");
                g_deleteAllJobsSelectedMemberButton->setNeedToolTip(true);
                g_deleteAllJobsSelectedMemberButton->setUserString(
                    "ToolTip",
                    "Delete all queued jobs for the currently selected squad member.");
                g_deleteAllJobsSelectedMemberButton->eventMouseButtonClick
                    += MyGUI::newDelegate(&OnDeleteAllJobsSelectedMemberButtonClicked);

                DebugLog("Job-B-Gone DEBUG: attached selected-member button to global GUI layer fallback");
                g_lastLoggedButtonExists = true;
            }
            else if (!g_loggedSelectedMemberButtonCreateFailure)
            {
                ErrorLog("Job-B-Gone: failed to create selected-member button on global GUI fallback layer");
                g_loggedSelectedMemberButtonCreateFailure = true;
            }
        }

        if (!g_deleteAllJobsSelectedMemberButton)
        {
            return;
        }

        g_deleteAllJobsSelectedMemberButton->setCoord(GetFallbackButtonCoord());
    }

    if (panelWidget && g_deleteAllJobsSelectedMemberButtonParent != panelWidget)
    {
        DestroySelectedMemberJobPanelButton();
        g_deleteAllJobsSelectedMemberButton = panelWidget->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(40, 60, 520, 40),
            MyGUI::Align::Default);
        if (!g_deleteAllJobsSelectedMemberButton)
        {
            if (!g_loggedSelectedMemberButtonCreateFailure)
            {
                ErrorLog("Job-B-Gone: failed to create selected-member delete-all-jobs button in info panel");
                g_loggedSelectedMemberButtonCreateFailure = true;
            }
            return;
        }

        g_loggedSelectedMemberButtonCreateFailure = false;
        g_deleteAllJobsSelectedMemberButtonParent = panelWidget;
        g_deleteAllJobsSelectedMemberButton->setCaption("Delete All Jobs (Selected Member)");
        g_deleteAllJobsSelectedMemberButton->setNeedToolTip(true);
        g_deleteAllJobsSelectedMemberButton->setUserString(
            "ToolTip",
            "Delete all queued jobs for the currently selected squad member.");
        g_deleteAllJobsSelectedMemberButton->eventMouseButtonClick
            += MyGUI::newDelegate(&OnDeleteAllJobsSelectedMemberButtonClicked);

        std::stringstream logline;
        logline << "Job-B-Gone DEBUG: created selected-member button parent_widget=0x"
                << std::hex << reinterpret_cast<uintptr_t>(panelWidget)
                << " button_ptr=0x" << reinterpret_cast<uintptr_t>(g_deleteAllJobsSelectedMemberButton);
        DebugLog(logline.str().c_str());
        g_lastLoggedButtonExists = true;
    }

    const bool hasSelectedMember = TryResolveSelectedMemberForDebug();
    if (hasSelectedMember != g_lastLoggedHasSelectedMemberForButton)
    {
        std::stringstream logline;
        logline << "Job-B-Gone DEBUG: selected_member_resolved_for_button="
                << (hasSelectedMember ? "true" : "false")
                << " selected_character_handle_valid="
                << ((g_lastPlayerInterface && g_lastPlayerInterface->selectedCharacter
                    && g_lastPlayerInterface->selectedCharacter.isValid()) ? "true" : "false");
        DebugLog(logline.str().c_str());
        g_lastLoggedHasSelectedMemberForButton = hasSelectedMember;
    }

    const bool shouldShowButton = hasSelectedMember && g_config.enableDeleteAllJobsSelectedMemberAction;
    g_deleteAllJobsSelectedMemberButton->setVisible(shouldShowButton);
    if (shouldShowButton != g_lastLoggedButtonVisibleState)
    {
        std::stringstream logline;
        logline << "Job-B-Gone DEBUG: selected-member button visible="
                << (shouldShowButton ? "true" : "false")
                << " selected_member_resolved=" << (hasSelectedMember ? "true" : "false")
                << " action_enabled=" << (g_config.enableDeleteAllJobsSelectedMemberAction ? "true" : "false")
                << " button_ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(g_deleteAllJobsSelectedMemberButton);
        DebugLog(logline.str().c_str());
        g_lastLoggedButtonVisibleState = shouldShowButton;
    }
}

static void OptionsWindowInitHook(OptionsWindow* self)
{
    if (g_fnOptionsInitOrig)
    {
        g_fnOptionsInitOrig(self);
    }
}

__declspec(dllexport) void startPlugin()
{
    DebugLog("Job-B-Gone: startPlugin()");

    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    if (platform == KenshiLib::BinaryVersion::UNKNOWN || (version != "1.0.65" && version != "1.0.68"))
    {
        ErrorLog("Job-B-Gone: unsupported Kenshi version/platform");
        return;
    }

    const uintptr_t baseAddr = reinterpret_cast<uintptr_t>(GetModuleHandleA(0));
    if (!InitPluginMenuFunctions(platform, version, baseAddr))
    {
        ErrorLog("Job-B-Gone WARN: failed to initialize plugin menu pointers; options button disabled");
    }
    else
    {
        if (KenshiLib::SUCCESS != KenshiLib::AddHook(g_fnOptionsInit, OptionsWindowInitHook, &g_fnOptionsInitOrig))
        {
            ErrorLog("Job-B-Gone WARN: Could not hook options init; options button disabled");
        }
    }

    LoadConfigState();
    if (g_configNeedsWriteBack)
    {
        if (!SaveConfigState())
        {
            ErrorLog("Job-B-Gone WARN: failed to persist normalized mod-config.json");
        }
    }

    if (!RunInternalSelfChecks())
    {
        ErrorLog("Job-B-Gone ERROR: internal self-check failed");
        return;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        &PlayerInterface_updateUT_orig))
    {
        ErrorLog("Job-B-Gone: Could not hook PlayerInterface::updateUT");
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
        ErrorLog("Job-B-Gone: Could not hook SaveManager::load(SaveInfo,bool)");
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
        ErrorLog("Job-B-Gone: Could not hook SaveManager::load(std::string)");
    }

    if (!g_hasSaveLoadHook)
    {
        ErrorLog("Job-B-Gone: no SaveManager load hooks active; feature disabled");
    }

    std::stringstream info;
    info << "Job-B-Gone INFO: initialized (enabled=" << (g_config.enabled ? "true" : "false")
         << ", pause_debounce_ms=" << g_config.pauseDebounceMs
         << ", enable_delete_all_jobs_selected_member_action="
         << (g_config.enableDeleteAllJobsSelectedMemberAction ? "true" : "false")
         << ", enable_experimental_single_job_delete="
         << (g_config.enableExperimentalSingleJobDelete ? "true" : "false")
         << ", log_selected_member_job_snapshot="
         << (g_config.logSelectedMemberJobSnapshot ? "true" : "false")
         << ", save_load_hooks=" << (g_hasSaveLoadHook ? "true" : "false") << ")";
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
