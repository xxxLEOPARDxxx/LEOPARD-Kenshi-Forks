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

struct JobRowModel
{
    int slot;
    TaskType taskType;
    std::string taskName;
    uintptr_t taskDataPtr;
    std::string jobKeyBase;
    int duplicateOrdinal;
    std::string jobKey;
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
static bool BuildSelectedMemberJobSnapshot(Character* member, std::vector<JobRowModel>* rowsOut, const char** resultOut);
static void ValidateSelectedMemberJobKeyStability(Character* member, const std::vector<JobRowModel>& snapshotRows, const char* phase);
static void LogSelectedMemberJobSnapshot(Character* member, const char* phase);
static bool TryDeleteAllJobsForSelectedMember(int* beforeOut, int* afterOut, int* deletedOut, const char** resultOut);
static bool RefreshSelectedMemberUi(const char* source);
static void ArmSelectedMemberUiRefresh(const char* source);
static void TickSelectedMemberUiRefresh();
static void OnDeleteAllJobsSelectedMemberButtonClicked(MyGUI::Widget*);
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

#include "JobBGoneJobActions.inl"

#include "JobBGonePanelUi.inl"

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
