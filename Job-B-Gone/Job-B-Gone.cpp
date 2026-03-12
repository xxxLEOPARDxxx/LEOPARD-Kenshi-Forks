#include <Debug.h>

#include <core/Functions.h>

#include <kenshi/Character.h>
#include <kenshi/Globals.h>
#include <kenshi/Kenshi.h>
#include <kenshi/Inventory.h>
#include <kenshi/InputHandler.h>
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
static const std::string kToggleBarCommandName = "toggle_bar";
static const char* kPanelVisibilityToggleHotkeyDefault = "CTRL+B";
static const char* kPanelVisibilityToggleHotkeyDisabled = "NONE";

static PluginConfig g_config = {
    true,
    false,
    true,
    false,
    true,
    true,
    true,
    true,
    false,
    false,
    0,
    0,
    kPanelVisibilityToggleHotkeyDefault
};

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
class Dialogue
{
public:
    hand getConversationTarget();
    bool conversationHasEndedPrettyMuch() const;
};

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
static bool g_lastLoggedPanelSuppressedByUiState = false;
static int g_lastLoggedPanelSuppressionReasonMask = 0;
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
static bool g_panelVisibilityToggleEnabled = true;
static bool g_panelVisibilityToggleRequireCtrl = true;
static bool g_panelVisibilityToggleRequireAlt = false;
static bool g_panelVisibilityToggleRequireShift = false;
static int g_panelVisibilityToggleVirtualKey = 'B';
static bool g_panelVisibilityTogglePrevDown = false;
static bool g_panelHiddenByToggle = false;
static bool g_lastLoggedPanelSuppressedByToggle = false;

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
static void OnJobBGonePanelMouseWheel(MyGUI::Widget*, int rel);
static bool TryNormalizePanelVisibilityToggleHotkey(
    const std::string& rawValue,
    std::string* canonicalOut,
    bool* enabledOut,
    bool* requireCtrlOut,
    bool* requireAltOut,
    bool* requireShiftOut,
    int* virtualKeyOut);
static void RefreshPanelVisibilityToggleHotkeyBinding();
static bool IsPanelVisibilityToggleHotkeyDown();
static void TickPanelVisibilityToggleHotkey(bool allowToggle, const char* blockedReasonSummary);

static void ResetConfigParseDiagnostics(ConfigParseDiagnostics* diagnostics)
{
    if (!diagnostics)
    {
        return;
    }

    diagnostics->foundEnabled = false;
    diagnostics->invalidEnabled = false;
    diagnostics->foundDebugLogTransitions = false;
    diagnostics->invalidDebugLogTransitions = false;
    diagnostics->foundEnableDeleteAllJobsSelectedMemberAction = false;
    diagnostics->invalidEnableDeleteAllJobsSelectedMemberAction = false;
    diagnostics->foundEnableExperimentalSingleJobDelete = false;
    diagnostics->invalidEnableExperimentalSingleJobDelete = false;
    diagnostics->foundLogSelectedMemberJobSnapshot = false;
    diagnostics->invalidLogSelectedMemberJobSnapshot = false;
    diagnostics->foundHidePanelDuringCharacterCreation = false;
    diagnostics->invalidHidePanelDuringCharacterCreation = false;
    diagnostics->foundHidePanelDuringInventoryOpen = false;
    diagnostics->invalidHidePanelDuringInventoryOpen = false;
    diagnostics->foundHidePanelDuringCharacterInteraction = false;
    diagnostics->invalidHidePanelDuringCharacterInteraction = false;
    diagnostics->foundJobBGonePanelCollapsed = false;
    diagnostics->invalidJobBGonePanelCollapsed = false;
    diagnostics->foundJobBGonePanelHasCustomPosition = false;
    diagnostics->invalidJobBGonePanelHasCustomPosition = false;
    diagnostics->foundJobBGonePanelPosX = false;
    diagnostics->invalidJobBGonePanelPosX = false;
    diagnostics->clampedJobBGonePanelPosX = false;
    diagnostics->foundJobBGonePanelPosY = false;
    diagnostics->invalidJobBGonePanelPosY = false;
    diagnostics->clampedJobBGonePanelPosY = false;
    diagnostics->foundPanelVisibilityToggleHotkey = false;
    diagnostics->invalidPanelVisibilityToggleHotkey = false;
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

static std::string ToUpperAscii(const std::string& value)
{
    std::string upper;
    upper.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i)
    {
        upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(value[i]))));
    }
    return upper;
}

static bool TryParsePanelVisibilityTogglePrimaryKeyToken(
    const std::string& tokenUpper,
    int* virtualKeyOut,
    std::string* canonicalTokenOut)
{
    if (!virtualKeyOut || !canonicalTokenOut)
    {
        return false;
    }

    if (tokenUpper.size() == 1)
    {
        const char ch = tokenUpper[0];
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))
        {
            *virtualKeyOut = static_cast<int>(ch);
            canonicalTokenOut->assign(1, ch);
            return true;
        }
    }

    if (tokenUpper == "SPACE")
    {
        *virtualKeyOut = VK_SPACE;
        *canonicalTokenOut = "SPACE";
        return true;
    }
    if (tokenUpper == "TAB")
    {
        *virtualKeyOut = VK_TAB;
        *canonicalTokenOut = "TAB";
        return true;
    }
    if (tokenUpper == "ENTER" || tokenUpper == "RETURN")
    {
        *virtualKeyOut = VK_RETURN;
        *canonicalTokenOut = "ENTER";
        return true;
    }
    if (tokenUpper == "ESC" || tokenUpper == "ESCAPE")
    {
        *virtualKeyOut = VK_ESCAPE;
        *canonicalTokenOut = "ESC";
        return true;
    }
    if (tokenUpper == "BACKSPACE")
    {
        *virtualKeyOut = VK_BACK;
        *canonicalTokenOut = "BACKSPACE";
        return true;
    }
    if (tokenUpper == "INSERT")
    {
        *virtualKeyOut = VK_INSERT;
        *canonicalTokenOut = "INSERT";
        return true;
    }
    if (tokenUpper == "DELETE")
    {
        *virtualKeyOut = VK_DELETE;
        *canonicalTokenOut = "DELETE";
        return true;
    }
    if (tokenUpper == "HOME")
    {
        *virtualKeyOut = VK_HOME;
        *canonicalTokenOut = "HOME";
        return true;
    }
    if (tokenUpper == "END")
    {
        *virtualKeyOut = VK_END;
        *canonicalTokenOut = "END";
        return true;
    }
    if (tokenUpper == "PAGEUP" || tokenUpper == "PGUP")
    {
        *virtualKeyOut = VK_PRIOR;
        *canonicalTokenOut = "PAGEUP";
        return true;
    }
    if (tokenUpper == "PAGEDOWN" || tokenUpper == "PGDN")
    {
        *virtualKeyOut = VK_NEXT;
        *canonicalTokenOut = "PAGEDOWN";
        return true;
    }
    if (tokenUpper == "UP")
    {
        *virtualKeyOut = VK_UP;
        *canonicalTokenOut = "UP";
        return true;
    }
    if (tokenUpper == "DOWN")
    {
        *virtualKeyOut = VK_DOWN;
        *canonicalTokenOut = "DOWN";
        return true;
    }
    if (tokenUpper == "LEFT")
    {
        *virtualKeyOut = VK_LEFT;
        *canonicalTokenOut = "LEFT";
        return true;
    }
    if (tokenUpper == "RIGHT")
    {
        *virtualKeyOut = VK_RIGHT;
        *canonicalTokenOut = "RIGHT";
        return true;
    }

    if (tokenUpper.size() >= 2 && tokenUpper[0] == 'F')
    {
        int functionIndex = 0;
        for (size_t i = 1; i < tokenUpper.size(); ++i)
        {
            const unsigned char ch = static_cast<unsigned char>(tokenUpper[i]);
            if (std::isdigit(ch) == 0)
            {
                return false;
            }
            functionIndex = (functionIndex * 10) + (tokenUpper[i] - '0');
        }

        if (functionIndex >= 1 && functionIndex <= 24)
        {
            *virtualKeyOut = VK_F1 + (functionIndex - 1);
            std::stringstream name;
            name << "F" << functionIndex;
            *canonicalTokenOut = name.str();
            return true;
        }
    }

    return false;
}

static bool TryNormalizePanelVisibilityToggleHotkey(
    const std::string& rawValue,
    std::string* canonicalOut,
    bool* enabledOut,
    bool* requireCtrlOut,
    bool* requireAltOut,
    bool* requireShiftOut,
    int* virtualKeyOut)
{
    if (!canonicalOut)
    {
        return false;
    }

    const std::string trimmed = TrimAscii(rawValue);
    if (trimmed.empty())
    {
        return false;
    }

    const std::string upper = ToUpperAscii(trimmed);
    if (upper == kPanelVisibilityToggleHotkeyDisabled)
    {
        *canonicalOut = kPanelVisibilityToggleHotkeyDisabled;
        if (enabledOut)
        {
            *enabledOut = false;
        }
        if (requireCtrlOut)
        {
            *requireCtrlOut = false;
        }
        if (requireAltOut)
        {
            *requireAltOut = false;
        }
        if (requireShiftOut)
        {
            *requireShiftOut = false;
        }
        if (virtualKeyOut)
        {
            *virtualKeyOut = 0;
        }
        return true;
    }

    bool requireCtrl = false;
    bool requireAlt = false;
    bool requireShift = false;
    bool hasPrimaryKey = false;
    int virtualKey = 0;
    std::string primaryKeyName;

    size_t tokenStart = 0;
    while (tokenStart <= upper.size())
    {
        const size_t plusPos = upper.find('+', tokenStart);
        const std::string tokenRaw = (plusPos == std::string::npos)
            ? upper.substr(tokenStart)
            : upper.substr(tokenStart, plusPos - tokenStart);
        const std::string token = TrimAscii(tokenRaw);
        if (token.empty())
        {
            return false;
        }

        if (token == "CTRL" || token == "CONTROL")
        {
            if (requireCtrl)
            {
                return false;
            }
            requireCtrl = true;
        }
        else if (token == "ALT")
        {
            if (requireAlt)
            {
                return false;
            }
            requireAlt = true;
        }
        else if (token == "SHIFT")
        {
            if (requireShift)
            {
                return false;
            }
            requireShift = true;
        }
        else
        {
            if (hasPrimaryKey)
            {
                return false;
            }
            if (!TryParsePanelVisibilityTogglePrimaryKeyToken(token, &virtualKey, &primaryKeyName))
            {
                return false;
            }
            hasPrimaryKey = true;
        }

        if (plusPos == std::string::npos)
        {
            break;
        }
        tokenStart = plusPos + 1;
    }

    if (!hasPrimaryKey)
    {
        return false;
    }

    std::stringstream canonical;
    if (requireCtrl)
    {
        canonical << "CTRL+";
    }
    if (requireAlt)
    {
        canonical << "ALT+";
    }
    if (requireShift)
    {
        canonical << "SHIFT+";
    }
    canonical << primaryKeyName;

    *canonicalOut = canonical.str();
    if (enabledOut)
    {
        *enabledOut = true;
    }
    if (requireCtrlOut)
    {
        *requireCtrlOut = requireCtrl;
    }
    if (requireAltOut)
    {
        *requireAltOut = requireAlt;
    }
    if (requireShiftOut)
    {
        *requireShiftOut = requireShift;
    }
    if (virtualKeyOut)
    {
        *virtualKeyOut = virtualKey;
    }
    return true;
}

static bool IsAnyVirtualKeyDown(int primaryVk, int leftVk, int rightVk)
{
    return (GetAsyncKeyState(primaryVk) & 0x8000) != 0
        || (GetAsyncKeyState(leftVk) & 0x8000) != 0
        || (GetAsyncKeyState(rightVk) & 0x8000) != 0;
}

static void RefreshPanelVisibilityToggleHotkeyBinding()
{
    std::string canonical;
    bool enabled = false;
    bool requireCtrl = false;
    bool requireAlt = false;
    bool requireShift = false;
    int virtualKey = 0;
    if (!TryNormalizePanelVisibilityToggleHotkey(
            g_config.panelVisibilityToggleHotkey,
            &canonical,
            &enabled,
            &requireCtrl,
            &requireAlt,
            &requireShift,
            &virtualKey))
    {
        canonical = kPanelVisibilityToggleHotkeyDefault;
        if (!TryNormalizePanelVisibilityToggleHotkey(
                canonical,
                &canonical,
                &enabled,
                &requireCtrl,
                &requireAlt,
                &requireShift,
                &virtualKey))
        {
            canonical = kPanelVisibilityToggleHotkeyDisabled;
            enabled = false;
            requireCtrl = false;
            requireAlt = false;
            requireShift = false;
            virtualKey = 0;
        }
    }

    g_config.panelVisibilityToggleHotkey = canonical;
    g_panelVisibilityToggleEnabled = enabled;
    g_panelVisibilityToggleRequireCtrl = requireCtrl;
    g_panelVisibilityToggleRequireAlt = requireAlt;
    g_panelVisibilityToggleRequireShift = requireShift;
    g_panelVisibilityToggleVirtualKey = virtualKey;
}

static bool IsPanelVisibilityToggleHotkeyDown()
{
    if (!g_panelVisibilityToggleEnabled || g_panelVisibilityToggleVirtualKey == 0)
    {
        return false;
    }

    if (g_panelVisibilityToggleRequireCtrl && !IsAnyVirtualKeyDown(VK_CONTROL, VK_LCONTROL, VK_RCONTROL))
    {
        return false;
    }
    if (g_panelVisibilityToggleRequireAlt && !IsAnyVirtualKeyDown(VK_MENU, VK_LMENU, VK_RMENU))
    {
        return false;
    }
    if (g_panelVisibilityToggleRequireShift && !IsAnyVirtualKeyDown(VK_SHIFT, VK_LSHIFT, VK_RSHIFT))
    {
        return false;
    }

    return (GetAsyncKeyState(g_panelVisibilityToggleVirtualKey) & 0x8000) != 0;
}

static void TickPanelVisibilityToggleHotkey(bool allowToggle, const char* blockedReasonSummary)
{
    const bool hotkeyDown = IsPanelVisibilityToggleHotkeyDown();
    if (!allowToggle)
    {
        if (g_config.debugLogTransitions && hotkeyDown && !g_panelVisibilityTogglePrevDown)
        {
            std::stringstream info;
            info << "Job-B-Gone DEBUG: panel_visibility_toggle_blocked"
                 << " reason=" << ((blockedReasonSummary && blockedReasonSummary[0] != '\0')
                        ? blockedReasonSummary
                        : "ui_suppressed")
                 << " hotkey=" << g_config.panelVisibilityToggleHotkey;
            DebugLog(info.str().c_str());
        }
        g_panelVisibilityTogglePrevDown = hotkeyDown;
        return;
    }

    if (hotkeyDown && !g_panelVisibilityTogglePrevDown)
    {
        g_panelHiddenByToggle = !g_panelHiddenByToggle;
        if (g_panelHiddenByToggle)
        {
            HideConfirmationOverlay();
        }

        std::stringstream info;
        info << "Job-B-Gone INFO: panel_visibility_toggled hidden="
             << (g_panelHiddenByToggle ? "true" : "false")
             << " hotkey=" << g_config.panelVisibilityToggleHotkey;
        DebugLog(info.str().c_str());
    }

    g_panelVisibilityTogglePrevDown = hotkeyDown;
}

#include "JobBGoneConfigParsing.inl"

static void LoadConfigState()
{
    g_configNeedsWriteBack = false;
    g_config.enabled = true;
    g_config.debugLogTransitions = false;
    g_config.enableDeleteAllJobsSelectedMemberAction = true;
    g_config.enableExperimentalSingleJobDelete = false;
    g_config.logSelectedMemberJobSnapshot = false;
    g_config.hidePanelDuringCharacterCreation = true;
    g_config.hidePanelDuringInventoryOpen = true;
    g_config.hidePanelDuringCharacterInteraction = true;
    g_config.jobBGonePanelCollapsed = false;
    g_config.jobBGonePanelHasCustomPosition = false;
    g_config.jobBGonePanelPosX = 0;
    g_config.jobBGonePanelPosY = 0;
    g_config.panelVisibilityToggleHotkey = kPanelVisibilityToggleHotkeyDefault;
    g_runtimePanelHasCustomPosition = false;
    g_runtimePanelPosX = 0;
    g_runtimePanelPosY = 0;
    g_panelHiddenByToggle = false;
    g_panelVisibilityTogglePrevDown = false;
    g_jobBGonePanelCollapsed = g_config.jobBGonePanelCollapsed;

    if (g_settingsPath.empty())
    {
        RefreshPanelVisibilityToggleHotkeyBinding();
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

    RefreshPanelVisibilityToggleHotkeyBinding();

    std::stringstream info;
    info << "Job-B-Gone INFO: loaded config enabled=" << (g_config.enabled ? "true" : "false")
         << " settings_path=\"" << g_settingsPath << "\""
         << " debug_log_transitions=" << (g_config.debugLogTransitions ? "true" : "false")
         << " enable_delete_all_jobs_selected_member_action="
         << (g_config.enableDeleteAllJobsSelectedMemberAction ? "true" : "false")
         << " enable_experimental_single_job_delete="
         << (g_config.enableExperimentalSingleJobDelete ? "true" : "false")
         << " log_selected_member_job_snapshot="
         << (g_config.logSelectedMemberJobSnapshot ? "true" : "false")
         << " hide_panel_during_character_creation="
         << (g_config.hidePanelDuringCharacterCreation ? "true" : "false")
         << " hide_panel_during_inventory_open="
         << (g_config.hidePanelDuringInventoryOpen ? "true" : "false")
         << " hide_panel_during_character_interaction="
         << (g_config.hidePanelDuringCharacterInteraction ? "true" : "false")
         << " job_b_gone_panel_collapsed="
         << (g_config.jobBGonePanelCollapsed ? "true" : "false")
         << " job_b_gone_panel_has_custom_position="
         << (g_config.jobBGonePanelHasCustomPosition ? "true" : "false")
         << " job_b_gone_panel_pos_x=" << g_config.jobBGonePanelPosX
         << " job_b_gone_panel_pos_y=" << g_config.jobBGonePanelPosY
         << " panel_visibility_toggle_hotkey=\"" << g_config.panelVisibilityToggleHotkey << "\"";
    DebugLog(info.str().c_str());

    if (g_config.jobBGonePanelHasCustomPosition)
    {
        g_runtimePanelHasCustomPosition = true;
        g_runtimePanelPosX = g_config.jobBGonePanelPosX;
        g_runtimePanelPosY = g_config.jobBGonePanelPosY;
    }

    g_jobBGonePanelCollapsed = g_config.jobBGonePanelCollapsed;
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

#include "JobBGoneJobActions.inl"

#include "JobBGonePanelUi.inl"

#include "JobBGoneHooksEntry.inl"
