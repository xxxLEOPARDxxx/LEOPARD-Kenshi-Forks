#include <Debug.h>

#include <core/Functions.h>

#include <kenshi/Character.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/Kenshi.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/RootObjectBase.h>
#include <kenshi/SaveManager.h>
#include <kenshi/TitleScreen.h>
#include <kenshi/util/lektor.h>
#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_InputManager.h>
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
#include <vector>

#include "JobBGoneSharedContracts.h"

static const char* kPluginName = "Job-B-Gone";
static const char* kConfigFileName = "mod-config.json";
static const DWORD kTickAliveIntervalMs = 5000;
static const DWORD kNoSignalDisarmMs = 1500;
static const DWORD kArmedTimeoutMs = 60000;
static const DWORD kDangerScopeArmTimeoutMs = 3000;
static const int kPanelWidth = 740;
static const int kPanelExpandedMinHeight = 184;
static const int kPanelExpandedMaxHeight = 440;
static const int kPanelExpandedConfirmHeight = 320;
static const int kPanelExpandedHeight = kPanelExpandedMaxHeight;
static const int kPanelCollapsedHeight = 46;
static const int kPanelViewportPadding = 20;
static const int kPanelMaxPersistedCoord = 100000;
static const char* kPluginTabName = "Job-B-Gone";
static const char* kPluginPanelName = "job_b_gone_options";
static const int kMaxVisibleJobRows = 8;

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

static MyGUI::TextBox* g_deleteAllJobsTitleText = 0;
static MyGUI::Button* g_deleteAllJobsSelectedMemberButton = 0;
static MyGUI::Button* g_deleteAllJobsSelectedMembersButton = 0;
static MyGUI::Button* g_deleteAllJobsWholeSquadButton = 0;
static MyGUI::Button* g_deleteAllJobsEveryoneButton = 0;
static MyGUI::TextBox* g_jobBGoneHoverHintText = 0;
static MyGUI::Button* g_jobBGoneConfirmOverlay = 0;
static MyGUI::TextBox* g_jobBGoneConfirmTitleText = 0;
static MyGUI::TextBox* g_jobBGoneConfirmTitleTextBold = 0;
static MyGUI::TextBox* g_jobBGoneConfirmBodyText = 0;
static MyGUI::Button* g_jobBGoneConfirmYesButton = 0;
static MyGUI::Button* g_jobBGoneConfirmNoButton = 0;
static bool g_jobBGoneConfirmVisible = false;
static bool g_dangerScopeArmed = false;
static JobDeleteScope g_armedDangerScope = JobDeleteScope_SelectedMember;
static MyGUI::Button* g_armedDangerButton = 0;
static DWORD g_dangerScopeArmedAtMs = 0;

enum PendingConfirmationActionType
{
    PendingConfirmationAction_None = 0,
    PendingConfirmationAction_DeleteAllScope = 1,
    PendingConfirmationAction_DeleteRowScope = 2
};

struct PendingConfirmationAction
{
    PendingConfirmationActionType type;
    JobDeleteScope scope;
    std::string jobKey;
    TaskType taskType;
    std::string taskName;
};

static PendingConfirmationAction g_pendingConfirmationAction = {
    PendingConfirmationAction_None,
    JobDeleteScope_SelectedMember,
    std::string(),
    static_cast<TaskType>(0),
    std::string()
};
static MyGUI::Widget* g_jobBGonePanel = 0;
static MyGUI::Button* g_jobBGoneHeaderButton = 0;
static MyGUI::Button* g_jobBGoneBodyFrame = 0;
static MyGUI::TextBox* g_jobBGoneStatusText = 0;
static MyGUI::TextBox* g_jobBGoneEmptyStateText = 0;
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
static bool g_runtimePanelHasCustomPosition = false;
static int g_runtimePanelPosX = 0;
static int g_runtimePanelPosY = 0;
static bool g_lastLoggedConfirmOverlayVisible = false;
static int g_panelExpandedHeight = kPanelExpandedHeight;
static int g_jobRowScrollOffset = 0;
static MyGUI::Button* g_jobRowsScrollUpButton = 0;
static MyGUI::Button* g_jobRowsScrollDownButton = 0;

struct JobRowWidgets
{
    MyGUI::TextBox* label;
    MyGUI::Button* deleteSelectedMemberButton;
    MyGUI::Button* deleteSelectedMembersButton;
    MyGUI::Button* deleteWholeSquadButton;
    MyGUI::Button* deleteEveryoneButton;
};

static std::vector<JobRowWidgets> g_jobRowWidgets;
static std::vector<JobRowModel> g_selectedMemberJobRows;

static bool DebounceWindowElapsed(DWORD nowMs, DWORD lastEventMs, DWORD minGapMs);
static void DisarmPauseAfterLoad();
static bool InitPluginMenuFunctions(unsigned int platform, const std::string& version, uintptr_t baseAddr);
static void OptionsWindowInitHook(OptionsWindow* self);
static void EnsureSelectedMemberJobPanelButton(PlayerInterface* thisptr);
static void OnSaveLoadTransitionStart(const char* source);
static Character* ResolveSelectedMember();
static bool TryResolveSelectedMemberForDebug();
static bool TryGetPermajobCount(Character* member, int* countOut);
static bool TryGetPermajobRow(Character* member, int slot, TaskType* taskTypeOut, std::string* nameOut, uintptr_t* taskDataPtrOut);
static bool BuildSelectedMemberJobSnapshot(Character* member, std::vector<JobRowModel>* rowsOut, const char** resultOut);
static void ValidateSelectedMemberJobKeyStability(Character* member, const std::vector<JobRowModel>& snapshotRows, const char* phase);
static void LogSelectedMemberJobSnapshot(Character* member, const char* phase);
static bool TryDeleteAllJobsForSelectedMember(int* beforeOut, int* afterOut, int* deletedOut, const char** resultOut);
static bool RefreshSelectedMemberUi(const char* source);
static void ArmSelectedMemberUiRefresh(const char* source);
static void TickSelectedMemberUiRefresh();
static void OnDeleteAllJobsSelectedMemberButtonClicked(MyGUI::Widget*);
static void OnDeleteAllJobsSelectedMembersButtonClicked(MyGUI::Widget*);
static void OnDeleteAllJobsWholeSquadButtonClicked(MyGUI::Widget*);
static void OnDeleteAllJobsEveryoneButtonClicked(MyGUI::Widget*);
static void OnDeleteJobSelectedMemberButtonClicked(MyGUI::Widget*);
static void OnDeleteJobSelectedMembersButtonClicked(MyGUI::Widget*);
static void OnDeleteJobWholeSquadButtonClicked(MyGUI::Widget*);
static void OnDeleteJobEveryoneButtonClicked(MyGUI::Widget*);
static void OnJobBGoneHeaderButtonClicked(MyGUI::Widget*);
static void OnJobBGoneHeaderMousePressed(MyGUI::Widget*, int, int, MyGUI::MouseButton);
static void OnJobBGoneHeaderMouseDrag(MyGUI::Widget*, int, int, MyGUI::MouseButton);
static void OnJobBGoneHeaderMouseMove(MyGUI::Widget*, int, int);
static void OnJobBGoneHeaderMouseReleased(MyGUI::Widget*, int, int, MyGUI::MouseButton);
static bool TryGetViewportSize(int* widthOut, int* heightOut);
static bool TryGetMousePosition(int* xOut, int* yOut);
static void MoveJobBGonePanelByDelta(int deltaX, int deltaY);
static void FinalizeJobBGonePanelDrag(const char* source);
static void TickJobBGonePanelDrag();
static void SetWidgetTooltipAndHoverHint(MyGUI::Widget* widget, const char* tooltipText);
static void OnActionButtonMouseSetFocus(MyGUI::Widget*, MyGUI::Widget*);
static void OnActionButtonMouseLostFocus(MyGUI::Widget*, MyGUI::Widget*);
static bool ShouldRequireConfirmationForScope(JobDeleteScope scope);
static void HideConfirmationOverlay();
static bool ShowDeleteAllConfirmation(JobDeleteScope scope);
static bool ShowDeleteRowConfirmation(JobDeleteScope scope, const std::string& jobKey, TaskType taskType, const std::string& taskName);
static void ExecutePendingConfirmationAction();
static void OnConfirmationAcceptClicked(MyGUI::Widget*);
static void OnConfirmationCancelClicked(MyGUI::Widget*);
static void OnConfirmationKeyPressed(MyGUI::Widget*, MyGUI::KeyCode, MyGUI::Char);
static int ClampIntValue(int value, int minValue, int maxValue);
static MyGUI::IntCoord ClampPanelCoordToViewport(const MyGUI::IntCoord& inputCoord);
static MyGUI::IntCoord BuildPanelCoordFromAnchor(int left, int top);
static MyGUI::IntCoord ResolvePanelCoordFromConfig();
static void ApplyPanelLayout(const MyGUI::IntCoord& panelCoord);
static void PersistPanelPositionIfChanged(const MyGUI::IntCoord& panelCoord, const char* source);
static MyGUI::IntCoord GetFallbackButtonCoord();
static int ComputeVisibleJobRowCapacityForHeight(int expandedHeight);
static int ComputeAdaptiveExpandedPanelHeight(bool showJobRows, bool showEmptyState, int jobCount, bool confirmationOverlayVisible);
static int GetExpandedPanelHeight();
static void OnJobRowsScrollUpButtonClicked(MyGUI::Widget*);
static void OnJobRowsScrollDownButtonClicked(MyGUI::Widget*);

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

#include "JobBGoneConfigParsing.inl"

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
    g_runtimePanelHasCustomPosition = false;
    g_runtimePanelPosX = 0;
    g_runtimePanelPosY = 0;

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
         << " settings_path=\"" << g_settingsPath << "\""
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

    if (g_config.jobBGonePanelHasCustomPosition)
    {
        g_runtimePanelHasCustomPosition = true;
        g_runtimePanelPosX = g_config.jobBGonePanelPosX;
        g_runtimePanelPosY = g_config.jobBGonePanelPosY;
    }
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
        std::stringstream error;
        error << "Job-B-Gone ERROR: failed to save mod-config.json path=\"" << g_settingsPath << "\"";
        ErrorLog(error.str().c_str());
        return false;
    }

    std::stringstream info;
    info << "Job-B-Gone INFO: saved mod-config.json path=\"" << g_settingsPath << "\"";
    DebugLog(info.str().c_str());

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

#include "JobBGoneJobActions.inl"

#include "JobBGonePanelUi.inl"

#include "JobBGoneHooksEntry.inl"
