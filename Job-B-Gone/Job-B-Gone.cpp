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
#include <mygui/MyGUI_TextBox.h>
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
    bool jobBGonePanelHasCustomPosition;
    int jobBGonePanelPosX;
    int jobBGonePanelPosY;
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
static const int kPanelWidth = 500;
static const int kPanelExpandedHeight = 112;
static const int kPanelCollapsedHeight = 42;
static const int kPanelViewportPadding = 20;
static const int kPanelMaxPersistedCoord = 100000;
static const char* kPluginTabName = "Job-B-Gone";
static const char* kPluginPanelName = "job_b_gone_options";

static PluginConfig g_config = { true, 2000, false, true, false, true, false, 0, 0 };
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
static MyGUI::Widget* g_jobBGonePanel = 0;
static MyGUI::Button* g_jobBGoneHeaderButton = 0;
static MyGUI::Button* g_jobBGoneBodyFrame = 0;
static MyGUI::TextBox* g_jobBGoneStatusText = 0;
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
static bool g_jobBGonePanelCollapsed = false;
static bool g_jobBGonePanelDragging = false;
static bool g_jobBGonePanelDragMoved = false;
static int g_jobBGonePanelDragLastMouseX = 0;
static int g_jobBGonePanelDragLastMouseY = 0;
static int g_jobBGonePanelDragMovedDistance = 0;

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
static void OnJobBGoneHeaderButtonClicked(MyGUI::Widget*);
static void OnJobBGoneHeaderMousePressed(MyGUI::Widget*, int, int, MyGUI::MouseButton);
static void OnJobBGoneHeaderMouseDrag(MyGUI::Widget*, int, int);
static void OnJobBGoneHeaderMouseReleased(MyGUI::Widget*, int, int, MyGUI::MouseButton);
static bool TryGetViewportSize(int* widthOut, int* heightOut);
static int ClampIntValue(int value, int minValue, int maxValue);
static MyGUI::IntCoord ClampPanelCoordToViewport(const MyGUI::IntCoord& inputCoord);
static MyGUI::IntCoord BuildPanelCoordFromAnchor(int left, int top);
static MyGUI::IntCoord ResolvePanelCoordFromConfig();
static void ApplyPanelLayout(const MyGUI::IntCoord& panelCoord);
static void PersistPanelPositionIfChanged(const MyGUI::IntCoord& panelCoord, const char* source);
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
    bool foundJobBGonePanelHasCustomPosition;
    bool invalidJobBGonePanelHasCustomPosition;
    bool foundJobBGonePanelPosX;
    bool invalidJobBGonePanelPosX;
    bool clampedJobBGonePanelPosX;
    bool foundJobBGonePanelPosY;
    bool invalidJobBGonePanelPosY;
    bool clampedJobBGonePanelPosY;
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
    diagnostics->foundJobBGonePanelHasCustomPosition = false;
    diagnostics->invalidJobBGonePanelHasCustomPosition = false;
    diagnostics->foundJobBGonePanelPosX = false;
    diagnostics->invalidJobBGonePanelPosX = false;
    diagnostics->clampedJobBGonePanelPosX = false;
    diagnostics->foundJobBGonePanelPosY = false;
    diagnostics->invalidJobBGonePanelPosY = false;
    diagnostics->clampedJobBGonePanelPosY = false;
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

static bool ParseJsonUnsignedIntValue(
    const std::string& text,
    size_t* pos,
    int maxValue,
    int* valueOut,
    bool* clampedOut)
{
    if (!pos || !valueOut || maxValue < 0)
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
    if (parsed > static_cast<unsigned long>(maxValue))
    {
        parsed = static_cast<unsigned long>(maxValue);
        clamped = true;
    }

    *valueOut = static_cast<int>(parsed);
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
        else if (key == "job_b_gone_panel_has_custom_position")
        {
            bool parsedBool = false;
            size_t valuePos = pos;
            if (ParseJsonBoolValue(body, &valuePos, &parsedBool))
            {
                diagnostics->foundJobBGonePanelHasCustomPosition = true;
                configOut->jobBGonePanelHasCustomPosition = parsedBool;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidJobBGonePanelHasCustomPosition = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "job_b_gone_panel_pos_x")
        {
            int parsedInt = 0;
            bool clamped = false;
            size_t valuePos = pos;
            if (ParseJsonUnsignedIntValue(body, &valuePos, kPanelMaxPersistedCoord, &parsedInt, &clamped))
            {
                diagnostics->foundJobBGonePanelPosX = true;
                diagnostics->clampedJobBGonePanelPosX = diagnostics->clampedJobBGonePanelPosX || clamped;
                configOut->jobBGonePanelPosX = parsedInt;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidJobBGonePanelPosX = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "job_b_gone_panel_pos_y")
        {
            int parsedInt = 0;
            bool clamped = false;
            size_t valuePos = pos;
            if (ParseJsonUnsignedIntValue(body, &valuePos, kPanelMaxPersistedCoord, &parsedInt, &clamped))
            {
                diagnostics->foundJobBGonePanelPosY = true;
                diagnostics->clampedJobBGonePanelPosY = diagnostics->clampedJobBGonePanelPosY || clamped;
                configOut->jobBGonePanelPosY = parsedInt;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidJobBGonePanelPosY = true;
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
    PluginConfig parsedConfig = { true, 2000, false, true, false, true, false, 0, 0 };
    ConfigParseDiagnostics diagnostics;
    ResetConfigParseDiagnostics(&diagnostics);

    if (!ParseConfigJson(
            "{\"enabled\":false,\"pause_debounce_ms\":1234,\"debug_log_transitions\":true,"
            "\"enable_delete_all_jobs_selected_member_action\":false,"
            "\"enable_experimental_single_job_delete\":true,"
            "\"log_selected_member_job_snapshot\":false,"
            "\"job_b_gone_panel_has_custom_position\":true,"
            "\"job_b_gone_panel_pos_x\":321,"
            "\"job_b_gone_panel_pos_y\":654}",
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
        || parsedConfig.logSelectedMemberJobSnapshot
        || !parsedConfig.jobBGonePanelHasCustomPosition
        || parsedConfig.jobBGonePanelPosX != 321
        || parsedConfig.jobBGonePanelPosY != 654)
    {
        return false;
    }

    const std::string bomJson = std::string("\xEF\xBB\xBF")
        + "{\"enabled\":true,\"pause_debounce_ms\":2000,\"debug_log_transitions\":false,"
          "\"enable_delete_all_jobs_selected_member_action\":true,"
          "\"enable_experimental_single_job_delete\":false,"
          "\"log_selected_member_job_snapshot\":true,"
          "\"job_b_gone_panel_has_custom_position\":false,"
          "\"job_b_gone_panel_pos_x\":11,"
          "\"job_b_gone_panel_pos_y\":22}";
    parsedConfig.enabled = false;
    parsedConfig.pauseDebounceMs = 1;
    parsedConfig.debugLogTransitions = true;
    parsedConfig.enableDeleteAllJobsSelectedMemberAction = false;
    parsedConfig.enableExperimentalSingleJobDelete = true;
    parsedConfig.logSelectedMemberJobSnapshot = false;
    parsedConfig.jobBGonePanelHasCustomPosition = true;
    parsedConfig.jobBGonePanelPosX = 0;
    parsedConfig.jobBGonePanelPosY = 0;
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
        || !parsedConfig.logSelectedMemberJobSnapshot
        || parsedConfig.jobBGonePanelHasCustomPosition
        || parsedConfig.jobBGonePanelPosX != 11
        || parsedConfig.jobBGonePanelPosY != 22)
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
    if (!diagnostics.foundJobBGonePanelHasCustomPosition || diagnostics.invalidJobBGonePanelHasCustomPosition)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: invalid/missing key \"job_b_gone_panel_has_custom_position\"; using default");
    }
    if (!diagnostics.foundJobBGonePanelPosX || diagnostics.invalidJobBGonePanelPosX)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: invalid/missing key \"job_b_gone_panel_pos_x\"; using default");
    }
    if (diagnostics.clampedJobBGonePanelPosX)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: \"job_b_gone_panel_pos_x\" exceeded max; clamped");
    }
    if (!diagnostics.foundJobBGonePanelPosY || diagnostics.invalidJobBGonePanelPosY)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: invalid/missing key \"job_b_gone_panel_pos_y\"; using default");
    }
    if (diagnostics.clampedJobBGonePanelPosY)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: \"job_b_gone_panel_pos_y\" exceeded max; clamped");
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
        << (config.logSelectedMemberJobSnapshot ? "true" : "false") << ",\n";
    out << "  \"job_b_gone_panel_has_custom_position\": "
        << (config.jobBGonePanelHasCustomPosition ? "true" : "false") << ",\n";
    out << "  \"job_b_gone_panel_pos_x\": " << config.jobBGonePanelPosX << ",\n";
    out << "  \"job_b_gone_panel_pos_y\": " << config.jobBGonePanelPosY << "\n";
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
    g_config.jobBGonePanelHasCustomPosition = false;
    g_config.jobBGonePanelPosX = 0;
    g_config.jobBGonePanelPosY = 0;

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
         << (g_config.logSelectedMemberJobSnapshot ? "true" : "false")
         << " job_b_gone_panel_has_custom_position="
         << (g_config.jobBGonePanelHasCustomPosition ? "true" : "false")
         << " job_b_gone_panel_pos_x=" << g_config.jobBGonePanelPosX
         << " job_b_gone_panel_pos_y=" << g_config.jobBGonePanelPosY;
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
            << " anchor_mode=job_b_gone_panel";
    DebugLog(logline.str().c_str());
}

static bool TryGetViewportSize(int* widthOut, int* heightOut)
{
    if (!widthOut || !heightOut)
    {
        return false;
    }

    MyGUI::RenderManager* renderManager = MyGUI::RenderManager::getInstancePtr();
    if (!renderManager)
    {
        return false;
    }

    const MyGUI::IntSize view = renderManager->getViewSize();
    if (view.width <= 0 || view.height <= 0)
    {
        return false;
    }

    *widthOut = view.width;
    *heightOut = view.height;
    return true;
}

static int ClampIntValue(int value, int minValue, int maxValue)
{
    if (value < minValue)
    {
        return minValue;
    }
    if (value > maxValue)
    {
        return maxValue;
    }
    return value;
}

static MyGUI::IntCoord BuildPanelCoordFromAnchor(int left, int top)
{
    const int height = g_jobBGonePanelCollapsed ? kPanelCollapsedHeight : kPanelExpandedHeight;
    return MyGUI::IntCoord(left, top, kPanelWidth, height);
}

static MyGUI::IntCoord ClampPanelCoordToViewport(const MyGUI::IntCoord& inputCoord)
{
    int left = inputCoord.left;
    int top = inputCoord.top;
    const int width = (inputCoord.width > 0) ? inputCoord.width : kPanelWidth;
    const int height = (inputCoord.height > 0) ? inputCoord.height : kPanelExpandedHeight;

    int viewWidth = 0;
    int viewHeight = 0;
    if (!TryGetViewportSize(&viewWidth, &viewHeight))
    {
        if (left < kPanelViewportPadding)
        {
            left = kPanelViewportPadding;
        }
        if (top < kPanelViewportPadding)
        {
            top = kPanelViewportPadding;
        }
        return MyGUI::IntCoord(left, top, width, height);
    }

    int minLeft = kPanelViewportPadding;
    int minTop = kPanelViewportPadding;
    int maxLeft = viewWidth - width - kPanelViewportPadding;
    int maxTop = viewHeight - height - kPanelViewportPadding;

    if (maxLeft < minLeft)
    {
        minLeft = 0;
        maxLeft = viewWidth - width;
        if (maxLeft < minLeft)
        {
            maxLeft = minLeft;
        }
    }

    if (maxTop < minTop)
    {
        minTop = 0;
        maxTop = viewHeight - height;
        if (maxTop < minTop)
        {
            maxTop = minTop;
        }
    }

    left = ClampIntValue(left, minLeft, maxLeft);
    top = ClampIntValue(top, minTop, maxTop);
    return MyGUI::IntCoord(left, top, width, height);
}

static MyGUI::IntCoord GetFallbackButtonCoord()
{
    int left = 1200;
    int top = 760;

    int viewWidth = 0;
    int viewHeight = 0;
    if (TryGetViewportSize(&viewWidth, &viewHeight))
    {
        const int height = g_jobBGonePanelCollapsed ? kPanelCollapsedHeight : kPanelExpandedHeight;
        // Anchor above the squad portraits panel (right-side roster block).
        left = viewWidth - kPanelWidth - 700;
        top = viewHeight - height - 250;
    }

    return ClampPanelCoordToViewport(BuildPanelCoordFromAnchor(left, top));
}

static MyGUI::IntCoord ResolvePanelCoordFromConfig()
{
    if (g_config.jobBGonePanelHasCustomPosition)
    {
        return ClampPanelCoordToViewport(
            BuildPanelCoordFromAnchor(g_config.jobBGonePanelPosX, g_config.jobBGonePanelPosY));
    }
    return GetFallbackButtonCoord();
}

static void ApplyPanelLayout(const MyGUI::IntCoord& panelCoord)
{
    if (!g_jobBGonePanel || !g_jobBGoneHeaderButton || !g_jobBGoneBodyFrame || !g_jobBGoneStatusText
        || !g_deleteAllJobsSelectedMemberButton)
    {
        return;
    }

    g_jobBGonePanel->setCoord(panelCoord);
    g_jobBGoneHeaderButton->setCoord(MyGUI::IntCoord(0, 0, panelCoord.width, 34));
    g_jobBGoneBodyFrame->setCoord(MyGUI::IntCoord(0, 36, panelCoord.width, panelCoord.height - 36));
    g_jobBGoneStatusText->setCoord(MyGUI::IntCoord(14, 46, panelCoord.width - 28, 22));
    g_deleteAllJobsSelectedMemberButton->setCoord(MyGUI::IntCoord(14, 70, panelCoord.width - 28, 34));
}

static void PersistPanelPositionIfChanged(const MyGUI::IntCoord& panelCoord, const char* source)
{
    if (g_config.jobBGonePanelHasCustomPosition && g_config.jobBGonePanelPosX == panelCoord.left
        && g_config.jobBGonePanelPosY == panelCoord.top)
    {
        return;
    }

    g_config.jobBGonePanelHasCustomPosition = true;
    g_config.jobBGonePanelPosX = panelCoord.left;
    g_config.jobBGonePanelPosY = panelCoord.top;
    if (!SaveConfigState())
    {
        ErrorLog("Job-B-Gone WARN: failed to persist Job-B-Gone panel position");
        return;
    }

    std::stringstream info;
    info << "Job-B-Gone INFO: persisted_panel_position source=" << (source ? source : "unknown")
         << " x=" << panelCoord.left
         << " y=" << panelCoord.top;
    DebugLog(info.str().c_str());
}

static void DestroySelectedMemberJobPanelButton()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui && g_jobBGonePanel)
    {
        gui->destroyWidget(g_jobBGonePanel);
    }

    g_jobBGonePanel = 0;
    g_jobBGoneHeaderButton = 0;
    g_jobBGoneBodyFrame = 0;
    g_jobBGoneStatusText = 0;
    g_deleteAllJobsSelectedMemberButton = 0;
    g_deleteAllJobsSelectedMemberButtonParent = 0;
    g_buttonAttachedToGlobalLayer = false;
    g_jobBGonePanelDragging = false;
    g_jobBGonePanelDragMoved = false;
    g_jobBGonePanelDragLastMouseX = 0;
    g_jobBGonePanelDragLastMouseY = 0;
    g_jobBGonePanelDragMovedDistance = 0;
}

static void OnJobBGoneHeaderButtonClicked(MyGUI::Widget*)
{
    if (g_jobBGonePanelDragMoved)
    {
        g_jobBGonePanelDragMoved = false;
        g_jobBGonePanelDragMovedDistance = 0;
        return;
    }

    g_jobBGonePanelCollapsed = !g_jobBGonePanelCollapsed;

    if (g_jobBGonePanel)
    {
        const MyGUI::IntCoord currentCoord = g_jobBGonePanel->getCoord();
        const MyGUI::IntCoord nextCoord = ClampPanelCoordToViewport(
            BuildPanelCoordFromAnchor(currentCoord.left, currentCoord.top));
        ApplyPanelLayout(nextCoord);

        if (g_config.jobBGonePanelHasCustomPosition)
        {
            PersistPanelPositionIfChanged(nextCoord, "collapse_toggle");
        }
    }
}

static void OnJobBGoneHeaderMousePressed(MyGUI::Widget*, int left, int top, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left)
    {
        return;
    }

    g_jobBGonePanelDragging = true;
    g_jobBGonePanelDragMoved = false;
    g_jobBGonePanelDragLastMouseX = left;
    g_jobBGonePanelDragLastMouseY = top;
    g_jobBGonePanelDragMovedDistance = 0;
}

static void OnJobBGoneHeaderMouseDrag(MyGUI::Widget*, int left, int top)
{
    if (!g_jobBGonePanelDragging || !g_jobBGonePanel)
    {
        return;
    }

    const int deltaX = left - g_jobBGonePanelDragLastMouseX;
    const int deltaY = top - g_jobBGonePanelDragLastMouseY;
    if (deltaX == 0 && deltaY == 0)
    {
        return;
    }

    const int moveX = (deltaX < 0) ? -deltaX : deltaX;
    const int moveY = (deltaY < 0) ? -deltaY : deltaY;
    g_jobBGonePanelDragMovedDistance += moveX + moveY;
    if (g_jobBGonePanelDragMovedDistance >= 3)
    {
        g_jobBGonePanelDragMoved = true;
    }

    const MyGUI::IntCoord currentCoord = g_jobBGonePanel->getCoord();
    const MyGUI::IntCoord movedCoord = ClampPanelCoordToViewport(
        BuildPanelCoordFromAnchor(currentCoord.left + deltaX, currentCoord.top + deltaY));
    ApplyPanelLayout(movedCoord);

    g_jobBGonePanelDragLastMouseX = left;
    g_jobBGonePanelDragLastMouseY = top;
}

static void OnJobBGoneHeaderMouseReleased(MyGUI::Widget*, int, int, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left || !g_jobBGonePanelDragging)
    {
        return;
    }

    g_jobBGonePanelDragging = false;
    if (!g_jobBGonePanel)
    {
        return;
    }

    const MyGUI::IntCoord clampedCoord = ClampPanelCoordToViewport(g_jobBGonePanel->getCoord());
    ApplyPanelLayout(clampedCoord);
    if (g_jobBGonePanelDragMoved)
    {
        PersistPanelPositionIfChanged(clampedCoord, "drag_release");
    }

    g_jobBGonePanelDragMovedDistance = 0;
}

static void EnsureSelectedMemberJobPanelButton(PlayerInterface* thisptr)
{
    g_lastPlayerInterface = thisptr;

    if (!g_jobBGonePanel)
    {
        MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
        if (!gui)
        {
            return;
        }

        const MyGUI::IntCoord panelCoord = ResolvePanelCoordFromConfig();
        g_jobBGonePanel = gui->createWidget<MyGUI::Widget>(
            "PanelEmpty",
            panelCoord,
            MyGUI::Align::Default,
            "Main");
        if (!g_jobBGonePanel)
        {
            g_jobBGonePanel = gui->createWidget<MyGUI::Widget>(
                "PanelEmpty",
                panelCoord,
                MyGUI::Align::Default,
                "Overlapped");
        }

        if (!g_jobBGonePanel)
        {
            if (!g_loggedSelectedMemberButtonCreateFailure)
            {
                ErrorLog("Job-B-Gone: failed to create Job-B-Gone panel");
                g_loggedSelectedMemberButtonCreateFailure = true;
            }
            return;
        }

        g_jobBGoneHeaderButton = g_jobBGonePanel->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(0, 0, panelCoord.width, 34),
            MyGUI::Align::Default);
        g_jobBGoneBodyFrame = g_jobBGonePanel->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(0, 36, panelCoord.width, panelCoord.height - 36),
            MyGUI::Align::Default);
        g_jobBGoneStatusText = g_jobBGonePanel->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(14, 46, panelCoord.width - 28, 22),
            MyGUI::Align::Default);
        g_deleteAllJobsSelectedMemberButton = g_jobBGonePanel->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(14, 70, panelCoord.width - 28, 34),
            MyGUI::Align::Default);

        if (!g_jobBGoneHeaderButton || !g_jobBGoneBodyFrame || !g_jobBGoneStatusText || !g_deleteAllJobsSelectedMemberButton)
        {
            DestroySelectedMemberJobPanelButton();
            if (!g_loggedSelectedMemberButtonCreateFailure)
            {
                ErrorLog("Job-B-Gone: failed to create Job-B-Gone panel widgets");
                g_loggedSelectedMemberButtonCreateFailure = true;
            }
            return;
        }

        g_buttonAttachedToGlobalLayer = true;
        g_loggedSelectedMemberButtonCreateFailure = false;
        g_deleteAllJobsSelectedMemberButtonParent = g_jobBGonePanel;
        g_jobBGoneBodyFrame->setCaption("");
        g_jobBGoneBodyFrame->setEnabled(false);
        g_jobBGoneStatusText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        g_jobBGoneHeaderButton->eventMouseButtonClick += MyGUI::newDelegate(&OnJobBGoneHeaderButtonClicked);
        g_jobBGoneHeaderButton->eventMouseButtonPressed += MyGUI::newDelegate(&OnJobBGoneHeaderMousePressed);
        g_jobBGoneHeaderButton->eventMouseDrag += MyGUI::newDelegate(&OnJobBGoneHeaderMouseDrag);
        g_jobBGoneHeaderButton->eventMouseButtonReleased += MyGUI::newDelegate(&OnJobBGoneHeaderMouseReleased);
        g_deleteAllJobsSelectedMemberButton->setCaption("Delete All Jobs (Selected Member)");
        g_deleteAllJobsSelectedMemberButton->setNeedToolTip(true);
        g_deleteAllJobsSelectedMemberButton->setUserString(
            "ToolTip",
            "Delete all queued jobs for the currently selected squad member.");
        g_deleteAllJobsSelectedMemberButton->eventMouseButtonClick
            += MyGUI::newDelegate(&OnDeleteAllJobsSelectedMemberButtonClicked);

        DebugLog("Job-B-Gone DEBUG: created Job-B-Gone collapsible panel");
        g_lastLoggedButtonExists = true;
    }

    if (!g_jobBGonePanel || !g_jobBGoneHeaderButton || !g_jobBGoneBodyFrame || !g_jobBGoneStatusText || !g_deleteAllJobsSelectedMemberButton)
    {
        return;
    }

    if (!g_jobBGonePanelDragging)
    {
        const MyGUI::IntCoord panelCoord = ResolvePanelCoordFromConfig();
        ApplyPanelLayout(panelCoord);

        if (g_config.jobBGonePanelHasCustomPosition
            && (panelCoord.left != g_config.jobBGonePanelPosX || panelCoord.top != g_config.jobBGonePanelPosY))
        {
            PersistPanelPositionIfChanged(panelCoord, "viewport_clamp");
        }
    }

    Character* selectedMember = ResolveSelectedMember();
    const bool hasSelectedMember = (selectedMember != 0);
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

    int jobCount = 0;
    if (selectedMember)
    {
        TryGetPermajobCount(selectedMember, &jobCount);
    }

    std::stringstream headerCaption;
    headerCaption << (g_jobBGonePanelCollapsed ? "[+] " : "[-] ") << "Job-B-Gone";
    g_jobBGoneHeaderButton->setCaption(headerCaption.str());

    std::stringstream statusCaption;
    if (hasSelectedMember)
    {
        statusCaption << "Selected member jobs: " << jobCount;
    }
    else
    {
        statusCaption << "No member selected";
    }
    g_jobBGoneStatusText->setCaption(statusCaption.str());

    const bool shouldShowButton = hasSelectedMember && g_config.enableDeleteAllJobsSelectedMemberAction;
    const bool buttonVisibleNow = shouldShowButton && !g_jobBGonePanelCollapsed;
    g_jobBGoneBodyFrame->setVisible(!g_jobBGonePanelCollapsed);
    g_jobBGoneStatusText->setVisible(!g_jobBGonePanelCollapsed);
    g_deleteAllJobsSelectedMemberButton->setVisible(buttonVisibleNow);
    if (buttonVisibleNow != g_lastLoggedButtonVisibleState)
    {
        std::stringstream logline;
        logline << "Job-B-Gone DEBUG: delete-button visible="
                << (buttonVisibleNow ? "true" : "false")
                << " selected_member_resolved=" << (hasSelectedMember ? "true" : "false")
                << " action_enabled=" << (g_config.enableDeleteAllJobsSelectedMemberAction ? "true" : "false")
                << " button_ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(g_deleteAllJobsSelectedMemberButton);
        DebugLog(logline.str().c_str());
        g_lastLoggedButtonVisibleState = buttonVisibleNow;
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
         << ", job_b_gone_panel_has_custom_position="
         << (g_config.jobBGonePanelHasCustomPosition ? "true" : "false")
         << ", job_b_gone_panel_pos_x=" << g_config.jobBGonePanelPosX
         << ", job_b_gone_panel_pos_y=" << g_config.jobBGonePanelPosY
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
