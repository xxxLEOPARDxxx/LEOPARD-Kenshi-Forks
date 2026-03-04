#include <Debug.h>

#include <core/Functions.h>

#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/Kenshi.h>
#include <kenshi/Character.h>
#include <kenshi/Damages.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/RootObject.h>

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_MouseButton.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>

#include <Windows.h>
#include <intrin.h>

#include <cctype>
#include <climits>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "LootScootExecuteSharedContracts.h"
#include "emc/mod_hub_api.h"
#include "emc/mod_hub_client.h"

static const char* kPluginName = "Loot-Scoot-Execute";
static const char* kConfigFileName = "mod-config.json";
static const DWORD kCanExecuteDecisionMinIntervalMs = 300;
static const DWORD kDebugExecuteContextTargetMaxAgeMs = 1500;
static const DWORD kNativeMenuExecuteArmMaxAgeMs = 2500;
static const DWORD kQueuedExecuteMaxLifetimeMs = 45000;
static const DWORD kQueuedExecuteRepathIntervalMs = 200;
static const DWORD kQueuedExecuteStateLogMinIntervalMs = 500;
static const DWORD kQueuedExecuteAttackWindupMs = 180;
static const DWORD kQueuedExecutePostTriggerMaxDurationMs = 4000;
static const DWORD kQueuedExecuteInRangeConfirmMs = 0;
static const DWORD kQueuedExecuteFacingGraceMs = 1500;
static const float kQueuedExecuteFacingDotGraceMin = 0.75f;
static const int kDebugExecuteHotkeyVirtualKey = VK_F8;
static const float kQueuedExecuteMaxDistanceMeters = 2.0f;
static const float kQueuedExecutePostTriggerDispatchExtraDistanceMeters = 1.5f;
static const float kQueuedExecutePostTriggerAbortExtraDistanceMeters = 6.0f;
static const float kQueuedExecuteFacingDotMin = 0.90f;
static const DWORD kCustomExecutePanelDispatchDedupMs = 250;
static const std::string kQueuedExecuteSlaveAnimName = "salute";
static const char* kExecuteKillSoundEventCandidates[] =
{
    "Heavy_Hit",
    "Play_Heavy_Hit",
    "Light_Hit",
    "Play_Light_Hit",
    "VO_Get_Hit",
    "Play_VO_Get_Hit",
    "VO_Creature_Die",
    "Play_VO_Creature_Die",
    "VO_Creature_Victory",
    "Play_VO_Creature_Victory"
};
static const size_t kExecuteKillSoundEventCandidateCount =
    sizeof(kExecuteKillSoundEventCandidates) / sizeof(kExecuteKillSoundEventCandidates[0]);
static const size_t kContextMenuRowMaterializationMaxRows = 12;
static const bool kDebugForceAcceptCarryTaskForFilterSanity = false;
static const bool kDebugEnableRowInsertSubstitute = false;
static const bool kDebugEnableShowContextMenuOrderMutationFallback = false;
static const bool kEnableNativeContextMenuIntegration = false;
static const bool kEnableBuildRowsPreloopInjection = false;
static const bool kEnableRowInsertLateInjection = false;
static const bool kEnableCustomExecutePanelOverlay = true;
static const int kCustomExecutePanelMinWidth = 280;
static const int kCustomExecutePanelMinRowHeight = 24;
static const int kCustomExecutePanelVerticalGap = 1;
static const int kCustomExecutePanelExtraWidth = 30;
static const int kCustomExecutePanelExtraHeight = 8;
static const int kCustomExecutePanelHorizontalOffset = 5;
static const int kCustomExecutePanelBottomExtraYOffset = 6;
static const int kCustomExecutePanelAdditionalYOffset = 5;
static const int kCustomExecutePanelFallbackExtraYOffset = 6;
static const int kExecuteButtonWidthMin = 0;
static const int kExecuteButtonWidthMax = 4096;
static const int kExecuteButtonHeightMin = 0;
static const int kExecuteButtonHeightMax = 1024;
static const int kExecuteButtonOffsetMin = -4096;
static const int kExecuteButtonOffsetMax = 4096;
static const int kExecuteButtonRuntimeWidthMin = 20;
static const int kExecuteButtonRuntimeHeightMin = 12;
static const int kExecuteButtonDefaultWidth = 310;
static const int kExecuteButtonDefaultHeight = 56;
static const int kExecuteButtonDefaultAbsoluteX = 0;
static const int kExecuteButtonDefaultAbsoluteY = 0;
static const std::string kContextMenuOptionsListWidgetName = "OptionsList";
static const std::string kContextMenuNameTextWidgetName = "NameText";
static const bool kEnableInternalDebugLogs = false;
static const int kContextMenuOrderIdLoot = 26;
static const int kContextMenuOrderIdLiftPersonPlayerOrder = static_cast<int>(LIFT_PERSON_PLAYER_ORDER);
static const int kContextMenuOrderIdStealthKill = static_cast<int>(STEALTH_KILL);
static const int kContextMenuOrderIdExecuteProxy = static_cast<int>(KILL_CAGE_OCCUPANT);
static const size_t kContextMenuProbeOrderSampleCount = 6;
static const uintptr_t kExpectedRvaContextMenuShow_1_0_65 = 0x007A5960;
static const uintptr_t kExpectedRvaContextMenuBuildRows_1_0_65 = 0x007A7FE0;
static const uintptr_t kExpectedRvaContextMenuUpdate_1_0_65 = 0x008055A0;
static const uintptr_t kExpectedRvaPlayerInterfaceUpdateUT_1_0_65 = 0x007F6C80;
static const uintptr_t kExpectedRvaPlayerInterfaceAddOrderSelectedCharacters_1_0_65 = 0x007F8BC0;
static const uintptr_t kExpectedRvaPlayerInterfaceGetPlayerTaskProbability_1_0_65 = 0x007F5380;
static const uintptr_t kExpectedRvaPlayerInterfaceContextMenuOrderFilterThunk_1_0_65 = 0x000193B7;
static const uintptr_t kExpectedRvaPlayerInterfaceContextMenuTaskProbabilityThunk_1_0_65 = 0x000210D0;
static const uintptr_t kExpectedRvaPlayerInterfaceIsOrderValidForSelection_1_0_65 = 0x007F1150;
static const uintptr_t kExpectedRvaContextMenuAppendOrderThunk_1_0_65 = 0x000115CC;
static const uintptr_t kExpectedRvaContextMenuTaskLabelThunk_1_0_65 = 0x00024758;
static const uintptr_t kExpectedRvaContextMenuOrderFilterCallReturn_1_0_65 = 0x007A75A2;
static const uintptr_t kExpectedRvaContextMenuTaskProbabilityCallReturn_1_0_65 = 0x007A787D;
static const uintptr_t kExpectedRvaContextMenuRowInsertCallReturn_1_0_65 = 0x007A7698;
static const uintptr_t kExpectedRvaContextMenuLoopEntry_1_0_65 = 0x007A7570;

static PluginConfig g_config = {
    true,
    false,
    false,
    false,
    true,
    kExecuteButtonDefaultWidth,
    kExecuteButtonDefaultHeight,
    kExecuteButtonDefaultAbsoluteX,
    kExecuteButtonDefaultAbsoluteY };

static std::string g_settingsPath;
static bool g_configNeedsWriteBack = false;
static bool g_contextMenuHookInstallVerified = false;
static bool g_effectiveEnableContextMenuProbe = false;
static bool g_effectiveEnableContextMenuInjection = false;
static bool g_effectiveEnableExecuteAction = false;
static uintptr_t g_resolvedContextMenuShowAddress = 0;
static uintptr_t g_contextMenuLoopScanSeedAddr = 0;
static int g_contextMenuLoopScanBaseReg = -1;
static uintptr_t g_resolvedContextMenuBuildRowsAddress = 0;
static uintptr_t g_resolvedContextMenuUpdateAddress = 0;
static uintptr_t g_resolvedPlayerInterfaceUpdateUTAddress = 0;
static uintptr_t g_resolvedPlayerInterfaceAddOrderSelectedCharactersAddress = 0;
static uintptr_t g_resolvedPlayerInterfaceGetPlayerTaskProbabilityAddress = 0;
static uintptr_t g_resolvedPlayerInterfaceContextMenuOrderFilterThunkAddress = 0;
static uintptr_t g_resolvedPlayerInterfaceContextMenuTaskProbabilityThunkAddress = 0;
static uintptr_t g_resolvedPlayerInterfaceIsOrderValidForSelectionAddress = 0;
static uintptr_t g_resolvedContextMenuAppendOrderThunkAddress = 0;
static uintptr_t g_resolvedContextMenuTaskLabelThunkAddress = 0;
static uintptr_t g_hookPlayerInterfaceContextMenuOrderFilterAddress = 0;
static uintptr_t g_hookPlayerInterfaceContextMenuTaskProbabilityAddress = 0;
static uintptr_t g_hookContextMenuAppendOrderAddress = 0;
static uintptr_t g_hookContextMenuTaskLabelAddress = 0;
static uintptr_t g_resolvedContextMenuOrderFilterCallReturnAddress = 0;
static uintptr_t g_resolvedContextMenuOrderFilterCallTargetAddress = 0;
static uintptr_t g_resolvedContextMenuTaskProbabilityCallReturnAddress = 0;
static uintptr_t g_resolvedContextMenuTaskProbabilityCallTargetAddress = 0;
static uintptr_t g_resolvedContextMenuRowInsertCallReturnAddress = 0;
static uintptr_t g_resolvedContextMenuRowInsertCallTargetAddress = 0;
static uintptr_t g_resolvedContextMenuLoopEntryAddress = 0;
static uintptr_t g_hookPlayerInterfaceContextMenuOrderFilterAlternateAddress = 0;
static uintptr_t g_hookPlayerInterfaceContextMenuTaskProbabilityAlternateAddress = 0;
static uintptr_t g_hookContextMenuAppendOrderAlternateAddress = 0;
static uintptr_t g_hookContextMenuTaskLabelAlternateAddress = 0;
static uint64_t g_currentShowSeq = 0;
static uint64_t g_lastShowSeq = 0;
static DWORD g_lastShowTimeMs = 0;
static uintptr_t g_lastMenuPtr = 0;
static bool g_lastWasDownedEnemy = false;
static bool g_lastShowTargetIsEnemy = false;
static bool g_lastShowTargetIsIncapacitated = false;
static bool g_lastShowTargetIsDead = false;
static std::string g_runtimeGameVersion;
static std::string g_runtimeLocaleTag;

static bool IsInternalDebugLogLine(const char* message)
{
    if (!message || message[0] == '\0')
    {
        return false;
    }

    return std::strstr(message, " DEBUG:") != nullptr;
}

static void PluginLog(const char* message)
{
    if (!message || message[0] == '\0')
    {
        return;
    }

    if (!kEnableInternalDebugLogs && IsInternalDebugLogLine(message))
    {
        return;
    }

    DebugLog(message);
}

static MyGUI::Widget* g_customExecutePanelRoot = 0;
static MyGUI::Button* g_customExecutePanelButton = 0;
static MyGUI::TextBox* g_customExecutePanelValue = 0;
static bool g_customExecutePanelVisible = false;
static bool g_customExecutePanelArmed = false;
static bool g_customExecutePanelRightMouseWasDown = false;
static hand g_customExecutePanelActorHandle;
static uintptr_t g_customExecutePanelActorPtr = 0;
static uintptr_t g_customExecutePanelTargetPtr = 0;
static uintptr_t g_customExecutePanelMenuPtr = 0;
static uint32_t g_customExecutePanelOrdersCount = 0;
static uint64_t g_customExecutePanelShowSeq = 0;
static DWORD g_customExecutePanelArmMs = 0;
static DWORD g_customExecutePanelLastDispatchMs = 0;
static uintptr_t g_customExecutePanelLastDispatchActorPtr = 0;
static uintptr_t g_customExecutePanelLastDispatchTargetPtr = 0;
static int g_customExecutePanelAnchorSource = 0;
static DWORD g_lastCanExecuteDecisionLogMs = 0;
static uintptr_t g_lastCanExecuteDecisionTargetPtr = 0;
static bool g_hasLastCanExecuteDecision = false;
static bool g_lastCanExecuteDecisionResult = false;
static bool g_queuedExecuteActive = false;
static hand g_queuedExecuteActorHandle;
static hand g_queuedExecuteTargetHandle;
static uintptr_t g_queuedExecuteActorPtr = 0;
static uintptr_t g_queuedExecuteTargetPtr = 0;
static DWORD g_queuedExecuteArmedMs = 0;
static DWORD g_queuedExecuteLastApproachCommandMs = 0;
static DWORD g_queuedExecuteLastStateLogMs = 0;
static bool g_queuedExecuteAttackTriggered = false;
static DWORD g_queuedExecuteAttackTriggeredMs = 0;
static DWORD g_queuedExecuteInRangeSinceMs = 0;
static const char* g_queuedExecuteAnimationMode = "none";
static bool g_queuedExecuteSlaveAnimPlaying = false;
static bool g_debugExecuteHotkeyWasDown = false;
static uintptr_t g_lastDebugExecuteContextTargetPtr = 0;
static DWORD g_lastDebugExecuteContextTargetCaptureMs = 0;
static bool g_nativeExecuteSelectionHookInstallVerified = false;
static bool g_nativeExecuteProbabilityHookInstallVerified = false;
static bool g_nativeExecuteOrderFilterHookInstallVerified = false;
static bool g_nativeExecuteContextMenuProbabilityHookInstallVerified = false;
static bool g_nativeExecuteOrderValidityHookInstallVerified = false;
static bool g_nativeExecuteOrderAppendHookInstallVerified = false;
static bool g_nativeExecuteTaskLabelHookInstallVerified = false;
static bool g_nativeExecuteMenuBuildHookInstallVerified = false;
static bool g_nativeExecuteRowInsertHookInstallVerified = false;
static bool g_nativeExecuteLoopEntryHookInstallVerified = false;
static bool g_nativeExecuteOrderFilterAlternateHookInstallVerified = false;
static bool g_nativeExecuteContextMenuProbabilityAlternateHookInstallVerified = false;
static bool g_nativeExecuteOrderAppendAlternateHookInstallVerified = false;
static bool g_nativeExecuteTaskLabelAlternateHookInstallVerified = false;
static bool g_nativeMenuExecuteDispatchArmed = false;
static uintptr_t g_nativeMenuExecuteDispatchTargetPtr = 0;
static DWORD g_nativeMenuExecuteDispatchArmMs = 0;
static bool g_nativeMenuOrderRemapArmed = false;
static uintptr_t g_nativeMenuOrderRemapOrdersPtr = 0;
static uintptr_t g_nativeMenuOrderRemapTargetPtr = 0;
static DWORD g_nativeMenuOrderRemapArmMs = 0;
static bool g_menuLatchActive = false;
static DWORD g_menuLatchTimestampMs = 0;
static uintptr_t g_nativeMenuExecuteRowInjectedOrdersPtr = 0;
static DWORD g_nativeMenuExecuteRowInjectedArmMs = 0;
static uintptr_t g_nativeMenuBuildRowsInjectedOrdersPtr = 0;
static uint64_t g_nativeMenuBuildRowsInjectedShowSeq = 0;
static uintptr_t g_nativeMenuRowDescriptorTemplate = 0;
static uintptr_t g_nativeMenuRowDescriptorTemplateOrdersPtr = 0;
static DWORD g_nativeMenuRowDescriptorTemplateArmMs = 0;
static __declspec(thread) bool g_nativeMenuAppendInjectionInProgress = false;
static uintptr_t g_nativeMenuLoopEntryInjectedOrdersPtr = 0;
static DWORD g_nativeMenuLoopEntryInjectedArmMs = 0;
static __declspec(thread) bool g_nativeMenuLoopEntryInjectionInProgress = false;
static bool g_contextMenuLoopEntryInlineHookInstalled = false;
static uintptr_t g_contextMenuLoopEntryInlineTargetAddress = 0;
static uintptr_t g_contextMenuLoopEntryInlineReturnAddress = 0;
static void* g_contextMenuLoopEntryInlineStubAddress = 0;
static unsigned char g_contextMenuLoopEntryInlineOriginalBytes[16] = { 0 };

enum ExecutePredicateEntryPoint
{
    ExecutePredicateEntryPoint_DEBUG_TRIGGER = 0,
    ExecutePredicateEntryPoint_NATIVE_MENU = 1,
    ExecutePredicateEntryPoint_FALLBACK_POPUP = 2
};

struct CanExecuteDiagnostics
{
    bool actorResolved;
    bool targetResolved;
    bool targetIsCharacter;
    bool targetIsEnemy;
    bool targetIsIncapacitated;
    bool targetIsDead;
    bool targetIsDown;
    bool targetIsUnconscious;
    bool targetIsLiterallyUnconscious;
    itemType targetType;
    uintptr_t actorPtr;
    uintptr_t targetPtr;
};

static void (*PlayerInterface_updateUT_orig)(PlayerInterface*) = 0;
static void (*PlayerInterface_addOrderSelectedCharacters_orig)(PlayerInterface*, Building*, TaskType, RootObject*, bool, bool, const Ogre::Vector3&) = 0;
static bool (*PlayerInterface_getPlayerTaskProbability_orig)(PlayerInterface*, TaskType, RootObject*, float&) = 0;
static bool (*PlayerInterface_contextMenuOrderFilterThunk_orig)(PlayerInterface*, TaskType) = 0;
static bool (*PlayerInterface_getContextMenuTaskProbabilityThunk_orig)(PlayerInterface*, TaskType, RootObject*, float&) = 0;
static bool (*PlayerInterface_isOrderValidForSelection_orig)(PlayerInterface*, TaskType) = 0;
static void (__fastcall *ContextMenu_appendOrderThunk_orig)(lektor<int>*, int) = 0;
static void (*ContextMenu_taskLabelThunk_orig)(std::string*, int, void*) = 0;
static void* (*ContextMenu_rowInsertCall_orig)(void*, void*, unsigned char, void*, void*) = 0;
static bool (*PlayerInterface_contextMenuOrderFilterThunk_probe_orig)(PlayerInterface*, TaskType) = 0;
static bool (*PlayerInterface_getContextMenuTaskProbabilityThunk_probe_orig)(PlayerInterface*, TaskType, RootObject*, float&) = 0;
static void (__fastcall *ContextMenu_appendOrderThunk_probe_orig)(lektor<int>*, int) = 0;
static void (*ContextMenu_taskLabelThunk_probe_orig)(std::string*, int, void*) = 0;
static void (*ContextMenu_showContextMenu_orig)(ContextMenu*, bool, RootObject*) = 0;
static void (*ContextMenu_buildRows_orig)(ContextMenu*, void*, void*, void*) = 0;
static void (*ContextMenu_update_orig)(ContextMenu*) = 0;

static bool DebounceWindowElapsed(DWORD nowMs, DWORD lastEventMs, DWORD minGapMs);
static std::string DetectRuntimeLocaleTag();
static Character* ResolveExecuteActorForPredicate();
static Character* ResolveExecuteActorForPredicateWithTarget(RootObject* target, bool allowAnyFallback);
static bool CanExecuteTarget(
    ExecutePredicateEntryPoint entryPoint,
    Character* actor,
    RootObject* target,
    CanExecuteDiagnostics* diagnosticsOut,
    bool verboseLog);
static bool CanExecuteFromDebugTrigger(Character* actor, RootObject* target, CanExecuteDiagnostics* diagnosticsOut, bool verboseLog);
static bool CanExecuteFromNativeMenuSelection(Character* actor, RootObject* target, CanExecuteDiagnostics* diagnosticsOut, bool verboseLog);
static bool CanExecuteFromFallbackPopup(Character* actor, RootObject* target, CanExecuteDiagnostics* diagnosticsOut, bool verboseLog);
static bool DispatchExecuteFromDebugTrigger(Character* actor, RootObject* target, bool verboseLog);
static bool DispatchExecuteFromNativeMenuSelection(Character* actor, RootObject* target, bool verboseLog);
static bool DispatchExecuteFromFallbackPopup(Character* actor, RootObject* target, bool verboseLog);
static void DisarmQueuedExecuteAction(const char* reason, bool verboseLog);
static bool QueueExecuteFromDebugTrigger(Character* actor, RootObject* target, bool verboseLog);
static bool QueueExecuteFromNativeMenuSelection(Character* actor, RootObject* target, bool verboseLog);
static bool QueueExecuteFromFallbackPopup(Character* actor, RootObject* target, bool verboseLog);
static void TickQueuedExecuteAction(PlayerInterface* player);
static bool TryReadRootObjectPosition(RootObject* object, Ogre::Vector3* positionOut);
static Character* TryResolveCharacterFromHandleSafe(const hand& characterHandle);
static Character* ResolvePreferredExecuteActorForQueue(RootObject* target, Character* fallbackActor);
static bool TryIssueQueuedExecuteFacingAdjust(Character* actor, const Ogre::Vector3& targetPos);
static bool TryTriggerQueuedExecuteAttackAnimation(Character* actor, RootObject* target);
static bool TryPlayCharacterAudioEvent(Character* character, const char* eventName, SoundRange range);
static bool TryPlayExecuteKillSound(
    Character* actor,
    Character* targetCharacter,
    const char** playedEventOut,
    const char** playedEmitterOut);
static void RefreshEffectiveContextMenuFeatureFlags(const char* source);
static bool IsNativeExecuteMenuMutationEnabled();
static bool IsCustomExecutePanelOverlayEnabled();
static void DisarmNativeMenuExecuteDispatchContext();
static void DisarmNativeMenuOrderRemapContext();
static void HideCustomExecutePanelOverlay();
static void ArmCustomExecutePanelOverlay(
    ContextMenu* menu,
    RootObject* target,
    uint32_t ordersCount,
    uint64_t showSeq,
    DWORD nowMs);
static void TickCustomExecutePanelOverlay(ContextMenu* menu, DWORD nowMs);
static void TickDebugExecuteHotkey(PlayerInterface* thisptr);
static bool InstallModHubOptionsWindowInitHook(unsigned int platform, const std::string& version);
static void ModHub_OnPluginStart();
static bool ModHub_UseHubUi();
static bool ModHub_IsAttachRetryPending();
static EMC_Result ModHub_LastAttachFailureResult();

static void ResetConfigParseDiagnostics(ConfigParseDiagnostics* diagnostics)
{
    if (!diagnostics)
    {
        return;
    }

    diagnostics->foundEnabled = false;
    diagnostics->invalidEnabled = false;
    diagnostics->foundEnableExecuteKillSound = false;
    diagnostics->invalidEnableExecuteKillSound = false;
    diagnostics->foundExecuteButtonWidthPx = false;
    diagnostics->invalidExecuteButtonWidthPx = false;
    diagnostics->clampedExecuteButtonWidthPx = false;
    diagnostics->foundExecuteButtonHeightPx = false;
    diagnostics->invalidExecuteButtonHeightPx = false;
    diagnostics->clampedExecuteButtonHeightPx = false;
    diagnostics->foundExecuteButtonOffsetXPx = false;
    diagnostics->invalidExecuteButtonOffsetXPx = false;
    diagnostics->clampedExecuteButtonOffsetXPx = false;
    diagnostics->foundExecuteButtonOffsetYPx = false;
    diagnostics->invalidExecuteButtonOffsetYPx = false;
    diagnostics->clampedExecuteButtonOffsetYPx = false;
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

#include "LootScootExecuteConfigParsing.inl"

static void LoadConfigState()
{
    g_configNeedsWriteBack = false;
    g_config.enabled = true;
    g_config.enableContextMenuProbe = false;
    g_config.debugContextMenu = false;
    g_config.enableDebugDirectDamageFallback = false;
    g_config.enableExecuteKillSound = true;
    g_config.executeButtonWidthPx = kExecuteButtonDefaultWidth;
    g_config.executeButtonHeightPx = kExecuteButtonDefaultHeight;
    g_config.executeButtonOffsetXPx = kExecuteButtonDefaultAbsoluteX;
    g_config.executeButtonOffsetYPx = kExecuteButtonDefaultAbsoluteY;
    g_effectiveEnableContextMenuProbe = false;
    g_effectiveEnableContextMenuInjection = false;
    g_effectiveEnableExecuteAction = false;

    if (g_settingsPath.empty())
    {
        return;
    }

    bool foundConfigFile = false;
    bool needsWriteBack = false;
    if (!ReadConfigFromFile(g_settingsPath, &g_config, &foundConfigFile, &needsWriteBack))
    {
        ErrorLog("Loot-Scoot-Execute ERROR: failed to read mod-config.json; using defaults and rewriting file");
        g_configNeedsWriteBack = true;
        return;
    }

    // Legacy/internal-only fields are not user-configurable.
    g_config.enableContextMenuProbe = false;
    g_config.debugContextMenu = false;
    g_config.enableDebugDirectDamageFallback = false;

    if (g_config.executeButtonWidthPx <= 0)
    {
        g_config.executeButtonWidthPx = kExecuteButtonDefaultWidth;
        needsWriteBack = true;
    }
    else if (g_config.executeButtonWidthPx < kExecuteButtonRuntimeWidthMin)
    {
        g_config.executeButtonWidthPx = kExecuteButtonRuntimeWidthMin;
        needsWriteBack = true;
    }

    if (g_config.executeButtonHeightPx <= 0)
    {
        g_config.executeButtonHeightPx = kExecuteButtonDefaultHeight;
        needsWriteBack = true;
    }
    else if (g_config.executeButtonHeightPx < kExecuteButtonRuntimeHeightMin)
    {
        g_config.executeButtonHeightPx = kExecuteButtonRuntimeHeightMin;
        needsWriteBack = true;
    }

    g_configNeedsWriteBack = (!foundConfigFile) || needsWriteBack;
    if (!foundConfigFile)
    {
        PluginLog("Loot-Scoot-Execute INFO: mod-config.json not found; using defaults");
    }

    std::stringstream info;
    info << "Loot-Scoot-Execute INFO: loaded config enabled=" << (g_config.enabled ? "true" : "false")
         << " settings_path=\"" << g_settingsPath << "\""
         << " enable_execute_kill_sound=" << (g_config.enableExecuteKillSound ? "true" : "false")
         << " execute_button_width=" << g_config.executeButtonWidthPx
         << " execute_button_height=" << g_config.executeButtonHeightPx
         << " execute_button_x=" << g_config.executeButtonOffsetXPx
         << " execute_button_y=" << g_config.executeButtonOffsetYPx;
    PluginLog(info.str().c_str());
}

static bool SaveConfigState()
{
    if (g_settingsPath.empty())
    {
        ErrorLog("Loot-Scoot-Execute ERROR: settings path is empty; cannot save mod-config.json");
        return false;
    }

    if (!SaveConfigToFile(g_settingsPath, g_config))
    {
        std::stringstream error;
        error << "Loot-Scoot-Execute ERROR: failed to save mod-config.json path=\"" << g_settingsPath << "\"";
        ErrorLog(error.str().c_str());
        return false;
    }

    std::stringstream info;
    info << "Loot-Scoot-Execute INFO: saved mod-config.json path=\"" << g_settingsPath << "\"";
    PluginLog(info.str().c_str());

    return true;
}

static bool DebounceWindowElapsed(DWORD nowMs, DWORD lastEventMs, DWORD minGapMs)
{
    const DWORD elapsed = nowMs - lastEventMs;
    return elapsed >= minGapMs;
}

static std::string DetectRuntimeLocaleTag()
{
    wchar_t localeName[LOCALE_NAME_MAX_LENGTH] = { 0 };
    const int localeLen = GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH);
    if (localeLen > 0)
    {
        char utf8[LOCALE_NAME_MAX_LENGTH * 4] = { 0 };
        const int utf8Len = WideCharToMultiByte(
            CP_UTF8,
            0,
            localeName,
            -1,
            utf8,
            static_cast<int>(sizeof(utf8)),
            0,
            0);
        if (utf8Len > 0 && utf8[0] != '\0')
        {
            return std::string(utf8);
        }
    }

    return "unknown";
}

static const char* ExecutePredicateEntryPointToString(ExecutePredicateEntryPoint entryPoint)
{
    switch (entryPoint)
    {
    case ExecutePredicateEntryPoint_DEBUG_TRIGGER:
        return "debug_trigger";
    case ExecutePredicateEntryPoint_NATIVE_MENU:
        return "native_menu";
    case ExecutePredicateEntryPoint_FALLBACK_POPUP:
        return "fallback_popup";
    default:
        return "unknown";
    }
}

static bool IsCharacterDataType(itemType type)
{
    return type == CHARACTER
        || type == HUMAN_CHARACTER
        || type == ANIMAL_CHARACTER
        || type == WORLDMAP_CHARACTER;
}

static bool TryGetRootObjectTypeForExecutePredicate(RootObject* target, itemType* typeOut)
{
    if (!target || !typeOut)
    {
        return false;
    }

    __try
    {
        *typeOut = target->getDataType();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryEvaluateCharacterExecuteFlags(
    Character* actor,
    Character* targetCharacter,
    bool* targetIsEnemyOut,
    bool* targetIsPlayerCharacterOut,
    bool* targetIsDeadOut,
    bool* targetIsDownOut,
    bool* targetIsUnconsciousOut,
    bool* targetIsLiterallyUnconsciousOut)
{
    if (!targetCharacter
        || !targetIsEnemyOut
        || !targetIsPlayerCharacterOut
        || !targetIsDeadOut
        || !targetIsDownOut
        || !targetIsUnconsciousOut
        || !targetIsLiterallyUnconsciousOut)
    {
        return false;
    }

    __try
    {
        *targetIsEnemyOut = false;
        *targetIsPlayerCharacterOut = targetCharacter->isPlayerCharacter();
        *targetIsDeadOut = targetCharacter->isDead();
        *targetIsDownOut = false;
        *targetIsUnconsciousOut = false;
        *targetIsLiterallyUnconsciousOut = false;

        if (!*targetIsDeadOut)
        {
            *targetIsDownOut = targetCharacter->isDown();
            *targetIsUnconsciousOut = targetCharacter->isUnconcious();
            *targetIsLiterallyUnconsciousOut = targetCharacter->isLiterallyUnconciousNotPretending();

            PlayerInterface* player = (ou ? ou->player : 0);
            if (player)
            {
                *targetIsEnemyOut = player->isEnemy(targetCharacter);
            }
            if (!*targetIsEnemyOut && actor)
            {
                *targetIsEnemyOut = actor->isEnemy(targetCharacter, true);
            }
        }

        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static Character* ResolveExecuteActorForPredicateWithTarget(RootObject* target, bool allowAnyFallback)
{
    if (!ou)
    {
        return 0;
    }

    PlayerInterface* player = 0;
    __try
    {
        player = ou->player;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }

    if (!player)
    {
        return 0;
    }

    // Prefer the currently active selected character so execute dispatch
    // stays bound to the exact user-selected actor.
    Character* selectedActor = TryResolveCharacterFromHandleSafe(player->selectedCharacter);
    if (selectedActor)
    {
        return selectedActor;
    }

    if (target)
    {
        Ogre::Vector3 targetPos;
        if (TryReadRootObjectPosition(target, &targetPos))
        {
            __try
            {
                selectedActor = player->getNearestSelectedCharacterTo(targetPos);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                selectedActor = 0;
            }
        }
    }

    if (selectedActor)
    {
        return selectedActor;
    }

    if (!allowAnyFallback)
    {
        return 0;
    }

    __try
    {
        return player->getAnyPlayerCharacter();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

static Character* ResolveExecuteActorForPredicate()
{
    return ResolveExecuteActorForPredicateWithTarget(0, true);
}

static bool CanExecuteTarget(
    ExecutePredicateEntryPoint entryPoint,
    Character* actor,
    RootObject* target,
    CanExecuteDiagnostics* diagnosticsOut,
    bool verboseLog)
{
    CanExecuteDiagnostics diagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
    diagnostics.actorResolved = actor != 0;
    diagnostics.targetResolved = target != 0;
    diagnostics.actorPtr = reinterpret_cast<uintptr_t>(actor);
    diagnostics.targetPtr = reinterpret_cast<uintptr_t>(target);

    Character* targetCharacter = 0;
    if (target)
    {
        itemType targetType = NULL_ITEM;
        if (!TryGetRootObjectTypeForExecutePredicate(target, &targetType))
        {
            targetType = NULL_ITEM;
        }
        diagnostics.targetType = targetType;

        if (IsCharacterDataType(targetType))
        {
            diagnostics.targetIsCharacter = true;
            targetCharacter = static_cast<Character*>(target);
        }
    }

    if (targetCharacter)
    {
        bool targetIsPlayerCharacter = false;
        if (TryEvaluateCharacterExecuteFlags(
            actor,
            targetCharacter,
            &diagnostics.targetIsEnemy,
            &targetIsPlayerCharacter,
            &diagnostics.targetIsDead,
            &diagnostics.targetIsDown,
            &diagnostics.targetIsUnconscious,
            &diagnostics.targetIsLiterallyUnconscious))
        {
            diagnostics.targetIsIncapacitated = diagnostics.targetIsDown
                || diagnostics.targetIsUnconscious
                || diagnostics.targetIsLiterallyUnconscious;
            if (targetIsPlayerCharacter)
            {
                diagnostics.targetIsEnemy = false;
            }
        }
        else
        {
            diagnostics.targetIsCharacter = false;
            diagnostics.targetIsEnemy = false;
            diagnostics.targetIsIncapacitated = false;
            diagnostics.targetIsDead = false;
            diagnostics.targetIsDown = false;
            diagnostics.targetIsUnconscious = false;
            diagnostics.targetIsLiterallyUnconscious = false;
        }
    }

    const bool canExecute = diagnostics.targetIsCharacter
        && diagnostics.targetIsEnemy
        && diagnostics.targetIsIncapacitated
        && !diagnostics.targetIsDead;

    if (diagnosticsOut)
    {
        *diagnosticsOut = diagnostics;
    }

    const DWORD nowMs = GetTickCount();
    const bool shouldLog = verboseLog
        && (!g_hasLastCanExecuteDecision
            || g_lastCanExecuteDecisionTargetPtr != diagnostics.targetPtr
            || g_lastCanExecuteDecisionResult != canExecute
            || DebounceWindowElapsed(nowMs, g_lastCanExecuteDecisionLogMs, kCanExecuteDecisionMinIntervalMs));
    if (shouldLog)
    {
        std::stringstream logline;
        logline << "Loot-Scoot-Execute DEBUG: can_execute_evaluate"
                << " source=" << ExecutePredicateEntryPointToString(entryPoint)
                << " result=" << (canExecute ? "true" : "false")
                << " actor_resolved=" << (diagnostics.actorResolved ? "true" : "false")
                << " actor=0x" << std::hex << diagnostics.actorPtr
                << " target_resolved=" << (diagnostics.targetResolved ? "true" : "false")
                << " target=0x" << diagnostics.targetPtr
                << " target_type=" << std::dec << static_cast<int>(diagnostics.targetType)
                << " target_is_character=" << (diagnostics.targetIsCharacter ? "true" : "false")
                << " target_is_enemy=" << (diagnostics.targetIsEnemy ? "true" : "false")
                << " target_is_down=" << (diagnostics.targetIsDown ? "true" : "false")
                << " target_is_unconscious=" << (diagnostics.targetIsUnconscious ? "true" : "false")
                << " target_is_literally_unconscious=" << (diagnostics.targetIsLiterallyUnconscious ? "true" : "false")
                << " target_is_incapacitated=" << (diagnostics.targetIsIncapacitated ? "true" : "false")
                << " target_is_dead=" << (diagnostics.targetIsDead ? "true" : "false");
        PluginLog(logline.str().c_str());

        g_hasLastCanExecuteDecision = true;
        g_lastCanExecuteDecisionTargetPtr = diagnostics.targetPtr;
        g_lastCanExecuteDecisionResult = canExecute;
        g_lastCanExecuteDecisionLogMs = nowMs;
    }

    return canExecute;
}

static bool CanExecuteFromDebugTrigger(Character* actor, RootObject* target, CanExecuteDiagnostics* diagnosticsOut, bool verboseLog)
{
    return CanExecuteTarget(ExecutePredicateEntryPoint_DEBUG_TRIGGER, actor, target, diagnosticsOut, verboseLog);
}

static bool CanExecuteFromNativeMenuSelection(Character* actor, RootObject* target, CanExecuteDiagnostics* diagnosticsOut, bool verboseLog)
{
    return CanExecuteTarget(ExecutePredicateEntryPoint_NATIVE_MENU, actor, target, diagnosticsOut, verboseLog);
}

static bool CanExecuteFromFallbackPopup(Character* actor, RootObject* target, CanExecuteDiagnostics* diagnosticsOut, bool verboseLog)
{
    return CanExecuteTarget(ExecutePredicateEntryPoint_FALLBACK_POPUP, actor, target, diagnosticsOut, verboseLog);
}

static bool TryResolvePlayerInterface(PlayerInterface** playerOut)
{
    if (!playerOut)
    {
        return false;
    }

    *playerOut = 0;
    if (!ou)
    {
        return false;
    }

    __try
    {
        *playerOut = ou->player;
        return *playerOut != 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryApplyDamageToAnatomyParts(Character* targetCharacter, const Damages& damage)
{
    if (!targetCharacter)
    {
        return false;
    }

    __try
    {
        const int anatomyCount = static_cast<int>(targetCharacter->medical.anatomy.size());
        if (anatomyCount <= 0)
        {
            return false;
        }
        for (int i = 0; i < anatomyCount; ++i)
        {
            if (targetCharacter->medical.anatomy[i])
            {
                targetCharacter->medical.anatomy[i]->applyDamage(damage);
            }
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryApplyDirectDamageExecuteFallback(Character* targetCharacter)
{
    const Damages damage(100, 100, 100, 100, 0);
    return TryApplyDamageToAnatomyParts(targetCharacter, damage);
}

static bool TryReadActorDispatchState(
    Character* actor,
    bool* isPlayerCharacterOut,
    bool* isDeadOut,
    bool* isUnconsciousOut)
{
    if (!actor || !isPlayerCharacterOut || !isDeadOut || !isUnconsciousOut)
    {
        return false;
    }

    __try
    {
        *isPlayerCharacterOut = actor->isPlayerCharacter();
        *isDeadOut = actor->isDead();
        *isUnconsciousOut = actor->isUnconcious();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryResolveTargetHandleValidity(RootObject* target, bool* isValidOut)
{
    if (!target || !isValidOut)
    {
        return false;
    }

    __try
    {
        const hand targetHandle = target->getHandle();
        *isValidOut = targetHandle.isValid();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        *isValidOut = false;
        return false;
    }
}

static bool TryIsCharacterDead(Character* targetCharacter, bool* isDeadOut)
{
    if (!targetCharacter || !isDeadOut)
    {
        return false;
    }

    __try
    {
        *isDeadOut = targetCharacter->isDead();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        *isDeadOut = false;
        return false;
    }
}

static bool TryDeclareCharacterDead(Character* targetCharacter)
{
    if (!targetCharacter)
    {
        return false;
    }

    __try
    {
        targetCharacter->declareDead();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryPlayCharacterAudioEvent(Character* character, const char* eventName, SoundRange range)
{
    if (!character || !eventName || eventName[0] == '\0')
    {
        return false;
    }

    __try
    {
        return character->audioEvent(eventName, range);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryPlayExecuteKillSound(
    Character* actor,
    Character* targetCharacter,
    const char** playedEventOut,
    const char** playedEmitterOut)
{
    if (playedEventOut)
    {
        *playedEventOut = "none";
    }
    if (playedEmitterOut)
    {
        *playedEmitterOut = "none";
    }

    for (size_t i = 0; i < kExecuteKillSoundEventCandidateCount; ++i)
    {
        const char* eventName = kExecuteKillSoundEventCandidates[i];
        if (TryPlayCharacterAudioEvent(actor, eventName, SOUNDRANGE_ALWAYS))
        {
            if (playedEventOut)
            {
                *playedEventOut = eventName;
            }
            if (playedEmitterOut)
            {
                *playedEmitterOut = "actor";
            }
            return true;
        }
    }

    for (size_t i = 0; i < kExecuteKillSoundEventCandidateCount; ++i)
    {
        const char* eventName = kExecuteKillSoundEventCandidates[i];
        if (TryPlayCharacterAudioEvent(targetCharacter, eventName, SOUNDRANGE_ALWAYS))
        {
            if (playedEventOut)
            {
                *playedEventOut = eventName;
            }
            if (playedEmitterOut)
            {
                *playedEmitterOut = "target";
            }
            return true;
        }
    }

    return false;
}

static bool TryReadContextMenuVisible(PlayerInterface* player, bool* visibleOut)
{
    if (!player || !visibleOut)
    {
        return false;
    }

    __try
    {
        *visibleOut = player->contextMenu.isVisible();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        *visibleOut = false;
        return false;
    }
}

static Character* ResolvePreferredExecuteActorForQueue(RootObject* target, Character* fallbackActor)
{
    PlayerInterface* player = 0;
    if (!TryResolvePlayerInterface(&player) || !player)
    {
        return fallbackActor;
    }

    Ogre::Vector3 targetPos;
    if (!TryReadRootObjectPosition(target, &targetPos))
    {
        return fallbackActor;
    }

    Character* selectedActor = 0;
    __try
    {
        selectedActor = player->getNearestSelectedCharacterTo(targetPos);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        selectedActor = 0;
    }

    if (selectedActor)
    {
        return selectedActor;
    }

    return fallbackActor;
}

static void TryEndQueuedExecuteSlaveAnim(Character* actor)
{
    if (!actor)
    {
        return;
    }

    __try
    {
        actor->endSlaveAnim(kQueuedExecuteSlaveAnimName);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

static void DisarmQueuedExecuteAction(const char* reason, bool verboseLog)
{
    Character* queuedActor = TryResolveCharacterFromHandleSafe(g_queuedExecuteActorHandle);
    if (!queuedActor && g_queuedExecuteActorPtr != 0)
    {
        queuedActor = reinterpret_cast<Character*>(g_queuedExecuteActorPtr);
    }
    if (g_queuedExecuteSlaveAnimPlaying && queuedActor)
    {
        TryEndQueuedExecuteSlaveAnim(queuedActor);
    }

    if (verboseLog && g_queuedExecuteActive)
    {
        std::stringstream logline;
        logline << "Loot-Scoot-Execute INFO: queued_execute_disarmed"
                << " reason=" << (reason ? reason : "none")
                << " actor=0x" << std::hex << g_queuedExecuteActorPtr
                << " target=0x" << g_queuedExecuteTargetPtr;
        PluginLog(logline.str().c_str());
    }

    g_queuedExecuteActive = false;
    g_queuedExecuteActorHandle.setNull();
    g_queuedExecuteTargetHandle.setNull();
    g_queuedExecuteActorPtr = 0;
    g_queuedExecuteTargetPtr = 0;
    g_queuedExecuteArmedMs = 0;
    g_queuedExecuteLastApproachCommandMs = 0;
    g_queuedExecuteLastStateLogMs = 0;
    g_queuedExecuteAttackTriggered = false;
    g_queuedExecuteAttackTriggeredMs = 0;
    g_queuedExecuteInRangeSinceMs = 0;
    g_queuedExecuteAnimationMode = "none";
    g_queuedExecuteSlaveAnimPlaying = false;
}

static bool TryResolveQueuedExecuteParticipants(
    Character** actorOut,
    RootObject** targetOut,
    const char** reasonOut)
{
    if (!actorOut || !targetOut)
    {
        return false;
    }

    *actorOut = 0;
    *targetOut = 0;
    if (reasonOut)
    {
        *reasonOut = "none";
    }

    if (!g_queuedExecuteActive)
    {
        if (reasonOut)
        {
            *reasonOut = "queue_inactive";
        }
        return false;
    }

    Character* actor = 0;
    RootObject* target = 0;
    __try
    {
        if (g_queuedExecuteActorHandle.isValid())
        {
            actor = g_queuedExecuteActorHandle.getCharacter();
        }
        if (g_queuedExecuteTargetHandle.isValid())
        {
            target = g_queuedExecuteTargetHandle.getRootObject();
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (reasonOut)
        {
            *reasonOut = "queue_handle_exception";
        }
        return false;
    }

    if (!actor)
    {
        if (reasonOut)
        {
            *reasonOut = "queue_actor_missing";
        }
        return false;
    }

    if (!target)
    {
        if (reasonOut)
        {
            *reasonOut = "queue_target_missing";
        }
        return false;
    }

    *actorOut = actor;
    *targetOut = target;
    return true;
}

static bool TryReadRootObjectPosition(RootObject* object, Ogre::Vector3* positionOut)
{
    if (!object || !positionOut)
    {
        return false;
    }

    __try
    {
        *positionOut = object->getPosition();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static float ComputeSquaredDistanceXZ(const Ogre::Vector3& a, const Ogre::Vector3& b)
{
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return (dx * dx) + (dz * dz);
}

static bool ComputeFacingDotToTarget(Character* actor, RootObject* target, float* dotOut)
{
    if (!actor || !target || !dotOut)
    {
        return false;
    }

    Ogre::Vector3 actorPos;
    Ogre::Vector3 targetPos;
    if (!TryReadRootObjectPosition(actor, &actorPos)
        || !TryReadRootObjectPosition(target, &targetPos))
    {
        return false;
    }

    Ogre::Vector3 toTarget = targetPos - actorPos;
    toTarget.y = 0.0f;
    const float toTargetLenSq = toTarget.squaredLength();
    if (toTargetLenSq <= 1.0e-6f)
    {
        *dotOut = 1.0f;
        return true;
    }

    Ogre::Vector3 facingDir(0.0f, 0.0f, 0.0f);
    __try
    {
        facingDir = actor->getMovementDirection();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    facingDir.y = 0.0f;
    const float facingLenSq = facingDir.squaredLength();
    if (facingLenSq <= 1.0e-6f)
    {
        *dotOut = -1.0f;
        return true;
    }

    const float invToTarget = 1.0f / std::sqrt(toTargetLenSq);
    const float invFacing = 1.0f / std::sqrt(facingLenSq);
    toTarget *= invToTarget;
    facingDir *= invFacing;
    *dotOut = facingDir.dotProduct(toTarget);
    return true;
}

static bool TryIssueQueuedExecuteApproach(Character* actor, RootObject* target)
{
    if (!actor || !target)
    {
        return false;
    }

    Ogre::Vector3 targetPos;
    if (!TryReadRootObjectPosition(target, &targetPos))
    {
        return false;
    }

    bool moveIssued = false;
    __try
    {
        // Keep execute approach at the front so combat AI doesn't steal the actor.
        actor->addOrder(0, GET_NEAR_TO, target, false, true, targetPos);
        actor->setDestination(targetPos, false);
        moveIssued = true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        moveIssued = false;
    }
    return moveIssued;
}

static bool TryIssueQueuedExecuteFacingAdjust(Character* actor, const Ogre::Vector3& targetPos)
{
    (void)actor;
    (void)targetPos;
    return false;
}

static bool TryTriggerQueuedExecuteAttackAnimation(Character* actor, RootObject* target)
{
    if (!actor || !target)
    {
        return false;
    }

    itemType targetType = NULL_ITEM;
    if (!TryGetRootObjectTypeForExecutePredicate(target, &targetType)
        || !IsCharacterDataType(targetType))
    {
        return false;
    }

    bool slaveAnimTriggered = false;
    __try
    {
        actor->runSlaveAnim(kQueuedExecuteSlaveAnimName, 1.0f, 0.0f);
        slaveAnimTriggered = true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        slaveAnimTriggered = false;
    }

    g_queuedExecuteSlaveAnimPlaying = slaveAnimTriggered;
    if (slaveAnimTriggered)
    {
        g_queuedExecuteAnimationMode = "slave_anim_salute";
        return true;
    }

    Ogre::Vector3 actorPos;
    const bool actorPosResolved = TryReadRootObjectPosition(actor, &actorPos);
    const Ogre::Vector3 crouchLocation = actorPosResolved ? actorPos : actor->getPosition();
    const bool queueToFront = true;
    actor->addOrder(0, CROUCH, actor, false, queueToFront, crouchLocation);
    g_queuedExecuteAnimationMode = "crouch_order_fallback";
    return true;
}

static bool TryAssignQueuedExecuteHandles(Character* actor, RootObject* target)
{
    if (!actor || !target)
    {
        return false;
    }

    __try
    {
        g_queuedExecuteActorHandle = actor;
        g_queuedExecuteTargetHandle = target;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static Character* TryResolveCharacterFromHandleSafe(const hand& characterHandle)
{
    Character* resolved = 0;
    __try
    {
        if (characterHandle.isValid())
        {
            resolved = characterHandle.getCharacter();
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        resolved = 0;
    }
    return resolved;
}

static float ComputeQueuedExecuteDistanceMeters(Character* actor, RootObject* target, bool targetIsCharacter)
{
    (void)actor;
    (void)target;
    (void)targetIsCharacter;
    return kQueuedExecuteMaxDistanceMeters;
}

static bool QueueExecuteTarget(
    ExecutePredicateEntryPoint entryPoint,
    Character* actor,
    RootObject* target,
    bool verboseLog)
{
    if (!g_effectiveEnableExecuteAction)
    {
        return false;
    }

    if (!target)
    {
        return false;
    }

    if (!actor)
    {
        actor = ResolveExecuteActorForPredicateWithTarget(target, true);
    }
    // For menu/fallback dispatch, keep the actor chosen by the user.
    // Auto-resolving nearest selected can switch actors mid-combat.
    if (entryPoint == ExecutePredicateEntryPoint_DEBUG_TRIGGER)
    {
        actor = ResolvePreferredExecuteActorForQueue(target, actor);
    }
    if (!actor)
    {
        return false;
    }

    const uintptr_t actorPtr = reinterpret_cast<uintptr_t>(actor);
    const uintptr_t targetPtr = reinterpret_cast<uintptr_t>(target);
    if (g_queuedExecuteActive
        && g_queuedExecuteActorPtr == actorPtr
        && g_queuedExecuteTargetPtr == targetPtr)
    {
        if (verboseLog)
        {
            std::stringstream dedup;
            dedup << "Loot-Scoot-Execute INFO: queued_execute_rearm_ignored"
                  << " source=" << ExecutePredicateEntryPointToString(entryPoint)
                  << " actor=0x" << std::hex << actorPtr
                  << " target=0x" << targetPtr;
            PluginLog(dedup.str().c_str());
        }
        return true;
    }

    // If a different execute queue is already active, tear it down first so
    // any running slave animation is ended on the old actor.
    if (g_queuedExecuteActive)
    {
        DisarmQueuedExecuteAction("queue_replaced", false);
    }

    CanExecuteDiagnostics diagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
    const bool canQueue = [&]() -> bool
    {
        switch (entryPoint)
        {
        case ExecutePredicateEntryPoint_DEBUG_TRIGGER:
            return CanExecuteFromDebugTrigger(actor, target, &diagnostics, verboseLog);
        case ExecutePredicateEntryPoint_NATIVE_MENU:
            return CanExecuteFromNativeMenuSelection(actor, target, &diagnostics, verboseLog);
        case ExecutePredicateEntryPoint_FALLBACK_POPUP:
            return CanExecuteFromFallbackPopup(actor, target, &diagnostics, verboseLog);
        default:
            return false;
        }
    }();
    if (!canQueue)
    {
        return false;
    }

    if (!TryAssignQueuedExecuteHandles(actor, target))
    {
        return false;
    }

    const DWORD nowMs = GetTickCount();
    g_queuedExecuteActive = true;
    g_queuedExecuteActorPtr = actorPtr;
    g_queuedExecuteTargetPtr = targetPtr;
    g_queuedExecuteArmedMs = nowMs;
    g_queuedExecuteLastApproachCommandMs = 0;
    g_queuedExecuteLastStateLogMs = 0;
    g_queuedExecuteAttackTriggered = false;
    g_queuedExecuteAttackTriggeredMs = 0;
    g_queuedExecuteInRangeSinceMs = 0;
    g_queuedExecuteAnimationMode = "none";

    if (TryIssueQueuedExecuteApproach(actor, target))
    {
        g_queuedExecuteLastApproachCommandMs = nowMs;
    }

    if (verboseLog)
    {
        std::stringstream logline;
        logline << "Loot-Scoot-Execute INFO: queued_execute_armed"
                << " source=" << ExecutePredicateEntryPointToString(entryPoint)
                << " actor=0x" << std::hex << reinterpret_cast<uintptr_t>(actor)
                << " target=0x" << reinterpret_cast<uintptr_t>(target)
                << " can_execute=true";
        PluginLog(logline.str().c_str());
    }

    return true;
}

static bool QueueExecuteFromDebugTrigger(Character* actor, RootObject* target, bool verboseLog)
{
    return QueueExecuteTarget(ExecutePredicateEntryPoint_DEBUG_TRIGGER, actor, target, verboseLog);
}

static bool QueueExecuteFromNativeMenuSelection(Character* actor, RootObject* target, bool verboseLog)
{
    return QueueExecuteTarget(ExecutePredicateEntryPoint_NATIVE_MENU, actor, target, verboseLog);
}

static bool QueueExecuteFromFallbackPopup(Character* actor, RootObject* target, bool verboseLog)
{
    return QueueExecuteTarget(ExecutePredicateEntryPoint_FALLBACK_POPUP, actor, target, verboseLog);
}

static void TickQueuedExecuteAction(PlayerInterface* player)
{
    if (!player || !g_queuedExecuteActive)
    {
        return;
    }

    const DWORD nowMs = GetTickCount();
    if (g_queuedExecuteArmedMs != 0
        && DebounceWindowElapsed(nowMs, g_queuedExecuteArmedMs, kQueuedExecuteMaxLifetimeMs))
    {
        DisarmQueuedExecuteAction("queue_timeout", true);
        return;
    }

    Character* actor = 0;
    RootObject* target = 0;
    const char* resolveReason = "none";
    if (!TryResolveQueuedExecuteParticipants(&actor, &target, &resolveReason))
    {
        DisarmQueuedExecuteAction(resolveReason, true);
        return;
    }

    CanExecuteDiagnostics diagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
    if (!CanExecuteFromNativeMenuSelection(actor, target, &diagnostics, false))
    {
        DisarmQueuedExecuteAction("target_not_executable", true);
        return;
    }

    Ogre::Vector3 actorPos;
    Ogre::Vector3 targetPos;
    if (!TryReadRootObjectPosition(actor, &actorPos)
        || !TryReadRootObjectPosition(target, &targetPos))
    {
        DisarmQueuedExecuteAction("position_read_failed", true);
        return;
    }

    const float executeDistance = ComputeQueuedExecuteDistanceMeters(actor, target, diagnostics.targetIsCharacter);

    const float distanceSq = ComputeSquaredDistanceXZ(actorPos, targetPos);
    const float maxDistanceSq = executeDistance * executeDistance;
    const float postTriggerDispatchDistance = executeDistance + kQueuedExecutePostTriggerDispatchExtraDistanceMeters;
    const float postTriggerDispatchDistanceSq = postTriggerDispatchDistance * postTriggerDispatchDistance;
    const float postTriggerAbortDistance = executeDistance + kQueuedExecutePostTriggerAbortExtraDistanceMeters;
    const float postTriggerAbortDistanceSq = postTriggerAbortDistance * postTriggerAbortDistance;
    const bool inRange = distanceSq <= maxDistanceSq;
    const bool inPostTriggerDispatchRange = distanceSq <= postTriggerDispatchDistanceSq;
    const bool postTriggerAbortDistanceExceeded = distanceSq > postTriggerAbortDistanceSq;
    const bool postTriggerTimedOut = g_queuedExecuteAttackTriggeredMs != 0
        && DebounceWindowElapsed(nowMs, g_queuedExecuteAttackTriggeredMs, kQueuedExecutePostTriggerMaxDurationMs);

    if (g_queuedExecuteAttackTriggered && postTriggerTimedOut)
    {
        DisarmQueuedExecuteAction("post_trigger_timeout", true);
        return;
    }

    if (g_queuedExecuteAttackTriggered && postTriggerAbortDistanceExceeded)
    {
        DisarmQueuedExecuteAction("post_trigger_drift_too_far", true);
        return;
    }

    if (inRange)
    {
        if (g_queuedExecuteInRangeSinceMs == 0)
        {
            g_queuedExecuteInRangeSinceMs = nowMs;
        }
    }
    else
    {
        g_queuedExecuteInRangeSinceMs = 0;
    }
    const bool inRangeGraceElapsed = g_queuedExecuteInRangeSinceMs != 0
        && DebounceWindowElapsed(nowMs, g_queuedExecuteInRangeSinceMs, kQueuedExecuteFacingGraceMs);
    const bool inRangeConfirmed = g_queuedExecuteInRangeSinceMs != 0
        && DebounceWindowElapsed(nowMs, g_queuedExecuteInRangeSinceMs, kQueuedExecuteInRangeConfirmMs);

    float facingDot = -1.0f;
    const bool facingResolved = ComputeFacingDotToTarget(actor, target, &facingDot);
    const bool facingTargetStrict = facingResolved && facingDot >= kQueuedExecuteFacingDotMin;
    const bool facingTargetGrace = facingResolved
        && facingDot >= kQueuedExecuteFacingDotGraceMin
        && inRangeGraceElapsed;
    const bool facingTargetFallback = inRangeGraceElapsed;
    const bool facingTarget = facingTargetStrict || facingTargetGrace || facingTargetFallback;

    if (!inRange)
    {
        const bool postTriggerCommitHold = g_queuedExecuteAttackTriggered;
        const bool shouldIssueApproach = !postTriggerCommitHold
            && (g_queuedExecuteLastApproachCommandMs == 0
                || DebounceWindowElapsed(nowMs, g_queuedExecuteLastApproachCommandMs, kQueuedExecuteRepathIntervalMs));
        if (shouldIssueApproach && TryIssueQueuedExecuteApproach(actor, target))
        {
            g_queuedExecuteLastApproachCommandMs = nowMs;
        }
        if (g_config.debugContextMenu
            && (g_queuedExecuteLastStateLogMs == 0
                || DebounceWindowElapsed(nowMs, g_queuedExecuteLastStateLogMs, kQueuedExecuteStateLogMinIntervalMs)))
        {
            std::stringstream logline;
            logline << "Loot-Scoot-Execute DEBUG: queued_execute_waiting"
                    << " actor=0x" << std::hex << reinterpret_cast<uintptr_t>(actor)
                    << " target=0x" << reinterpret_cast<uintptr_t>(target)
                    << " in_range=" << (inRange ? "true" : "false")
                    << " facing_target=" << (facingTarget ? "true" : "false")
                    << " facing_required=false"
                    << " facing_target_strict=" << (facingTargetStrict ? "true" : "false")
                    << " facing_target_grace=" << (facingTargetGrace ? "true" : "false")
                    << " facing_target_fallback=" << (facingTargetFallback ? "true" : "false")
                    << " in_range_grace_elapsed=" << (inRangeGraceElapsed ? "true" : "false")
                    << " distance_sq=" << std::dec << distanceSq
                    << " max_distance_sq=" << maxDistanceSq
                    << " post_trigger_dispatch_distance_sq=" << postTriggerDispatchDistanceSq
                    << " post_trigger_commit_hold=" << (postTriggerCommitHold ? "true" : "false")
                    << " post_trigger_dispatch_range=" << (inPostTriggerDispatchRange ? "true" : "false")
                    << " facing_dot=" << facingDot
                    << " in_range_since_ms=" << std::dec << g_queuedExecuteInRangeSinceMs;
            PluginLog(logline.str().c_str());
            g_queuedExecuteLastStateLogMs = nowMs;
        }
        if (!postTriggerCommitHold || !inPostTriggerDispatchRange)
        {
            return;
        }
    }

    const bool queueReadyByRange = inRangeConfirmed || (g_queuedExecuteAttackTriggered && inPostTriggerDispatchRange);
    if (!queueReadyByRange)
    {
        return;
    }

    if (!g_queuedExecuteAttackTriggered)
    {
        const bool attackTriggered = diagnostics.targetIsCharacter
            && TryTriggerQueuedExecuteAttackAnimation(actor, target);
        g_queuedExecuteAttackTriggered = true;
        g_queuedExecuteAttackTriggeredMs = attackTriggered ? nowMs : 0;

        if (g_config.debugContextMenu)
        {
            std::stringstream logline;
            logline << "Loot-Scoot-Execute DEBUG: queued_execute_attack_trigger"
                    << " actor=0x" << std::hex << reinterpret_cast<uintptr_t>(actor)
                    << " target=0x" << reinterpret_cast<uintptr_t>(target)
                    << " triggered=" << (attackTriggered ? "true" : "false")
                    << " mode=" << (g_queuedExecuteAnimationMode ? g_queuedExecuteAnimationMode : "none")
                    << " distance_sq=" << std::dec << distanceSq
                    << " max_distance_sq=" << maxDistanceSq
                    << " fallback_to_direct_dispatch=" << (attackTriggered ? "false" : "true");
            PluginLog(logline.str().c_str());
        }
    }

    if (g_queuedExecuteAttackTriggeredMs != 0
        && !DebounceWindowElapsed(nowMs, g_queuedExecuteAttackTriggeredMs, kQueuedExecuteAttackWindupMs))
    {
        return;
    }

    if (g_config.debugContextMenu)
    {
        std::stringstream logline;
        logline << "Loot-Scoot-Execute DEBUG: queued_execute_ready"
                << " actor=0x" << std::hex << reinterpret_cast<uintptr_t>(actor)
                << " target=0x" << reinterpret_cast<uintptr_t>(target)
                << " distance_sq=" << std::dec << distanceSq
                << " max_distance_sq=" << maxDistanceSq
                << " post_trigger_dispatch_distance_sq=" << postTriggerDispatchDistanceSq
                << " queue_ready_by_range=" << (queueReadyByRange ? "true" : "false")
                << " facing_dot=" << facingDot
                << " in_range_confirmed=" << (inRangeConfirmed ? "true" : "false");
        PluginLog(logline.str().c_str());
    }

    const bool dispatched = DispatchExecuteFromNativeMenuSelection(actor, target, true);
    if (dispatched)
    {
        DisarmQueuedExecuteAction("queue_completed", true);
    }
    else
    {
        DisarmQueuedExecuteAction("dispatch_failed", true);
    }
}

static bool DispatchExecuteTarget(
    ExecutePredicateEntryPoint entryPoint,
    Character* actor,
    RootObject* target,
    bool verboseLog)
{
    const uintptr_t actorPtr = reinterpret_cast<uintptr_t>(actor);
    const uintptr_t targetPtr = reinterpret_cast<uintptr_t>(target);

    const char* failureReason = "none";
    bool hasFailure = false;
    bool actorIsPlayerCharacter = false;
    bool actorIsDead = false;
    bool actorIsUnconscious = false;
    bool targetHandleValid = false;
    bool directDamageDispatchSucceeded = false;
    bool targetDeadAfterDamageCheck = false;
    bool declareDeadAttempted = false;
    bool declareDeadCallSucceeded = false;
    bool targetDeadAfterFinalizeCheck = false;
    const bool killSoundEnabled = g_config.enableExecuteKillSound;
    bool killSoundPlayed = false;
    const char* killSoundEvent = "none";
    const char* killSoundEmitter = "none";

    CanExecuteDiagnostics canExecuteDiagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
    const bool canExecute = [&]() -> bool
    {
        switch (entryPoint)
        {
        case ExecutePredicateEntryPoint_DEBUG_TRIGGER:
            return CanExecuteFromDebugTrigger(actor, target, &canExecuteDiagnostics, verboseLog);
        case ExecutePredicateEntryPoint_NATIVE_MENU:
            return CanExecuteFromNativeMenuSelection(actor, target, &canExecuteDiagnostics, verboseLog);
        case ExecutePredicateEntryPoint_FALLBACK_POPUP:
            return CanExecuteFromFallbackPopup(actor, target, &canExecuteDiagnostics, verboseLog);
        default:
            return false;
        }
    }();

    if (!g_effectiveEnableExecuteAction)
    {
        failureReason = "execute_action_disabled";
        hasFailure = true;
    }
    else if (!actor)
    {
        failureReason = "actor_null";
        hasFailure = true;
    }
    else if (!target)
    {
        failureReason = "target_null";
        hasFailure = true;
    }
    else
    {
        PlayerInterface* player = 0;
        if (!TryResolvePlayerInterface(&player))
        {
            failureReason = "player_unavailable";
            hasFailure = true;
        }
        else
        {
            if (!TryReadActorDispatchState(actor, &actorIsPlayerCharacter, &actorIsDead, &actorIsUnconscious))
            {
                failureReason = "actor_state_exception";
                hasFailure = true;
            }

            if (!hasFailure && !actorIsPlayerCharacter)
            {
                failureReason = "actor_not_player_character";
                hasFailure = true;
            }
            else if (!hasFailure && (actorIsDead || actorIsUnconscious))
            {
                failureReason = "actor_invalid_state";
                hasFailure = true;
            }
            else if (!hasFailure && !canExecute)
            {
                failureReason = "can_execute_false";
                hasFailure = true;
            }

            if (!hasFailure)
            {
                const bool handleRead = TryResolveTargetHandleValidity(target, &targetHandleValid);
                if (!handleRead)
                {
                    failureReason = "target_handle_exception";
                    hasFailure = true;
                }
                else if (!targetHandleValid)
                {
                    failureReason = "target_handle_invalid";
                    hasFailure = true;
                }
            }

            if (!hasFailure)
            {
                Character* targetCharacter = static_cast<Character*>(target);
                directDamageDispatchSucceeded = TryApplyDirectDamageExecuteFallback(targetCharacter);
                if (!directDamageDispatchSucceeded)
                {
                    failureReason = "direct_damage_dispatch_failed";
                    hasFailure = true;
                }
                else if (!TryIsCharacterDead(targetCharacter, &targetDeadAfterDamageCheck))
                {
                    failureReason = "post_damage_dead_check_failed";
                    hasFailure = true;
                }
                else if (!targetDeadAfterDamageCheck)
                {
                    declareDeadAttempted = true;
                    declareDeadCallSucceeded = TryDeclareCharacterDead(targetCharacter);
                    if (!declareDeadCallSucceeded)
                    {
                        failureReason = "declare_dead_failed";
                        hasFailure = true;
                    }
                    else if (!TryIsCharacterDead(targetCharacter, &targetDeadAfterFinalizeCheck))
                    {
                        failureReason = "post_finalize_dead_check_failed";
                        hasFailure = true;
                    }
                    else if (!targetDeadAfterFinalizeCheck)
                    {
                        failureReason = "target_still_alive_after_dispatch";
                        hasFailure = true;
                    }
                }
                else
                {
                    targetDeadAfterFinalizeCheck = true;
                }
            }
        }
    }

    const bool dispatchSucceeded = directDamageDispatchSucceeded && !hasFailure;
    if (dispatchSucceeded && targetDeadAfterFinalizeCheck && killSoundEnabled)
    {
        Character* targetCharacter = static_cast<Character*>(target);
        killSoundPlayed = TryPlayExecuteKillSound(actor, targetCharacter, &killSoundEvent, &killSoundEmitter);
    }

    if (verboseLog || !dispatchSucceeded)
    {
        std::stringstream logline;
        logline << "Loot-Scoot-Execute DEBUG: dispatch_execute"
                << " source=" << ExecutePredicateEntryPointToString(entryPoint)
                << " result=" << (dispatchSucceeded ? "true" : "false")
                << " actor=0x" << std::hex << actorPtr
                << " target=0x" << targetPtr
                << " actor_is_player_character=" << (actorIsPlayerCharacter ? "true" : "false")
                << " actor_is_dead=" << (actorIsDead ? "true" : "false")
                << " actor_is_unconscious=" << (actorIsUnconscious ? "true" : "false")
                << " can_execute=" << (canExecute ? "true" : "false")
                << " target_handle_valid=" << (targetHandleValid ? "true" : "false")
                << " direct_damage_dispatch_succeeded=" << (directDamageDispatchSucceeded ? "true" : "false")
                << " target_dead_after_damage_check=" << (targetDeadAfterDamageCheck ? "true" : "false")
                << " declare_dead_attempted=" << (declareDeadAttempted ? "true" : "false")
                << " declare_dead_call_succeeded=" << (declareDeadCallSucceeded ? "true" : "false")
                << " target_dead_after_finalize_check=" << (targetDeadAfterFinalizeCheck ? "true" : "false")
                << " kill_sound_enabled=" << (killSoundEnabled ? "true" : "false")
                << " kill_sound_played=" << (killSoundPlayed ? "true" : "false")
                << " kill_sound_event=" << killSoundEvent
                << " kill_sound_emitter=" << killSoundEmitter
                << " reason=" << (dispatchSucceeded ? "none" : failureReason);
        PluginLog(logline.str().c_str());
    }

    return dispatchSucceeded;
}

static bool DispatchExecuteFromDebugTrigger(Character* actor, RootObject* target, bool verboseLog)
{
    return DispatchExecuteTarget(ExecutePredicateEntryPoint_DEBUG_TRIGGER, actor, target, verboseLog);
}

static bool DispatchExecuteFromNativeMenuSelection(Character* actor, RootObject* target, bool verboseLog)
{
    return DispatchExecuteTarget(ExecutePredicateEntryPoint_NATIVE_MENU, actor, target, verboseLog);
}

static bool DispatchExecuteFromFallbackPopup(Character* actor, RootObject* target, bool verboseLog)
{
    return DispatchExecuteTarget(ExecutePredicateEntryPoint_FALLBACK_POPUP, actor, target, verboseLog);
}

static void LogDebugExecuteTargetSourceFromContextMenu(uintptr_t targetPtr)
{
    std::stringstream logline;
    logline << "Loot-Scoot-Execute DEBUG: debug_execute_target_source source=context_menu_show_target"
            << " target=0x" << std::hex << targetPtr;
    PluginLog(logline.str().c_str());
}

static void TickDebugExecuteHotkey(PlayerInterface* thisptr)
{
    if (!thisptr || !g_effectiveEnableExecuteAction || !g_config.debugContextMenu)
    {
        g_debugExecuteHotkeyWasDown = false;
        g_lastDebugExecuteContextTargetPtr = 0;
        g_lastDebugExecuteContextTargetCaptureMs = 0;
        return;
    }

    const bool keyDown = (GetAsyncKeyState(kDebugExecuteHotkeyVirtualKey) & 0x8000) != 0;
    const bool pressedThisFrame = keyDown && !g_debugExecuteHotkeyWasDown;
    g_debugExecuteHotkeyWasDown = keyDown;
    if (!pressedThisFrame)
    {
        return;
    }

    RootObject* target = 0;
    bool usedContextTargetFallback = false;
    bool contextMenuVisible = false;
    bool contextMenuVisibleResolved = TryReadContextMenuVisible(thisptr, &contextMenuVisible);
    const DWORD nowMs = GetTickCount();
    __try
    {
        if (thisptr->mouseRightTargetSet)
        {
            target = thisptr->mouseRightTarget;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        target = 0;
    }
    const bool fallbackTargetFresh = g_lastDebugExecuteContextTargetCaptureMs != 0
        && !DebounceWindowElapsed(nowMs, g_lastDebugExecuteContextTargetCaptureMs, kDebugExecuteContextTargetMaxAgeMs);
    if (!target
        && g_lastDebugExecuteContextTargetPtr != 0
        && fallbackTargetFresh
        && contextMenuVisibleResolved
        && contextMenuVisible)
    {
        target = reinterpret_cast<RootObject*>(g_lastDebugExecuteContextTargetPtr);
        usedContextTargetFallback = true;
    }
    else if (!target)
    {
        g_lastDebugExecuteContextTargetPtr = 0;
        g_lastDebugExecuteContextTargetCaptureMs = 0;
    }

    Character* actor = ResolveExecuteActorForPredicateWithTarget(target, true);
    if (usedContextTargetFallback)
    {
        LogDebugExecuteTargetSourceFromContextMenu(reinterpret_cast<uintptr_t>(target));
    }
    (void)QueueExecuteFromDebugTrigger(actor, target, true);
}

static bool IsNativeExecuteMenuMutationEnabled()
{
    return kEnableNativeContextMenuIntegration
        && g_effectiveEnableContextMenuInjection
        && g_effectiveEnableExecuteAction
        && !kEnableCustomExecutePanelOverlay;
}

static bool IsCustomExecutePanelOverlayEnabled()
{
    return kEnableCustomExecutePanelOverlay
        && g_effectiveEnableContextMenuInjection
        && g_effectiveEnableExecuteAction;
}

static bool IsCustomExecutePanelButtonHovered()
{
    if (!g_customExecutePanelButton || !g_customExecutePanelVisible)
    {
        return false;
    }

    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    if (!input)
    {
        return false;
    }

    MyGUI::IntCoord buttonRect(0, 0, 0, 0);
    __try
    {
        if (!g_customExecutePanelButton->getInheritedVisible())
        {
            return false;
        }
        buttonRect = g_customExecutePanelButton->getAbsoluteCoord();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    const MyGUI::IntPoint mousePos = input->getMousePosition();
    return mousePos.left >= buttonRect.left
        && mousePos.left < (buttonRect.left + buttonRect.width)
        && mousePos.top >= buttonRect.top
        && mousePos.top < (buttonRect.top + buttonRect.height);
}

static bool DispatchCustomExecutePanelAction(const char* sourceTag)
{
    if (!IsCustomExecutePanelOverlayEnabled())
    {
        return false;
    }

    uintptr_t targetPtr = g_customExecutePanelTargetPtr;
    if (targetPtr == 0 && g_nativeMenuExecuteDispatchTargetPtr != 0)
    {
        targetPtr = g_nativeMenuExecuteDispatchTargetPtr;
    }

    RootObject* target = reinterpret_cast<RootObject*>(targetPtr);
    Character* actor = TryResolveCharacterFromHandleSafe(g_customExecutePanelActorHandle);
    if (!actor)
    {
        actor = ResolveExecuteActorForPredicateWithTarget(target, false);
    }
    const uintptr_t actorPtr = reinterpret_cast<uintptr_t>(actor);

    const DWORD nowMs = GetTickCount();
    const bool dedupDispatch = actorPtr != 0
        && targetPtr != 0
        && g_customExecutePanelLastDispatchActorPtr == actorPtr
        && g_customExecutePanelLastDispatchTargetPtr == targetPtr
        && g_customExecutePanelLastDispatchMs != 0
        && !DebounceWindowElapsed(nowMs, g_customExecutePanelLastDispatchMs, kCustomExecutePanelDispatchDedupMs);
    if (dedupDispatch)
    {
        return false;
    }

    const bool queued = QueueExecuteFromNativeMenuSelection(actor, target, true);
    if (queued)
    {
        g_customExecutePanelLastDispatchMs = nowMs;
        g_customExecutePanelLastDispatchActorPtr = actorPtr;
        g_customExecutePanelLastDispatchTargetPtr = targetPtr;
    }

    std::stringstream logline;
    logline << "Loot-Scoot-Execute INFO: custom_execute_panel_dispatch"
            << " source=" << (sourceTag ? sourceTag : "unknown")
            << " queued=" << (queued ? "true" : "false")
            << " actor=0x" << std::hex << actorPtr
            << " target=0x" << targetPtr;
    PluginLog(logline.str().c_str());

    HideCustomExecutePanelOverlay();
    DisarmNativeMenuExecuteDispatchContext();
    DisarmNativeMenuOrderRemapContext();
    return queued;
}

static void OnCustomExecutePanelButtonPressed(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id)
{
    (void)sender;
    (void)left;
    (void)top;

    if (id != MyGUI::MouseButton::Left)
    {
        return;
    }

    (void)DispatchCustomExecutePanelAction("mouse_pressed");
}

static void OnCustomExecutePanelButtonClick(MyGUI::Widget* sender)
{
    (void)sender;
    (void)DispatchCustomExecutePanelAction("mouse_click");
}

static void DestroyCustomExecutePanelOverlayWidgets()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui && g_customExecutePanelRoot)
    {
        gui->destroyWidget(g_customExecutePanelRoot);
    }

    g_customExecutePanelRoot = 0;
    g_customExecutePanelButton = 0;
    g_customExecutePanelValue = 0;
}

static bool EnsureCustomExecutePanelOverlayWidgets()
{
    if (g_customExecutePanelRoot && g_customExecutePanelButton)
    {
        return true;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (!gui)
    {
        return false;
    }

    DestroyCustomExecutePanelOverlayWidgets();

    try
    {
        g_customExecutePanelRoot = gui->createWidget<MyGUI::Widget>(
            "PanelEmpty",
            MyGUI::IntCoord(0, 0, kCustomExecutePanelMinWidth, kCustomExecutePanelMinRowHeight),
            MyGUI::Align::Default,
            "Popup",
            "LSE_CustomExecuteOverlayRoot");
        if (!g_customExecutePanelRoot)
        {
            return false;
        }

        g_customExecutePanelRoot->setNeedMouseFocus(true);
        g_customExecutePanelRoot->setInheritsPick(true);

        g_customExecutePanelButton = g_customExecutePanelRoot->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(
                2,
                1,
                kCustomExecutePanelMinWidth - 4,
                kCustomExecutePanelMinRowHeight - 2),
            MyGUI::Align::Default,
            "LSE_CustomExecuteOverlayButton");
        if (!g_customExecutePanelButton)
        {
            g_customExecutePanelButton = g_customExecutePanelRoot->createWidget<MyGUI::Button>(
                "Button",
                MyGUI::IntCoord(
                    2,
                    1,
                    kCustomExecutePanelMinWidth - 4,
                    kCustomExecutePanelMinRowHeight - 2),
                MyGUI::Align::Default,
                "LSE_CustomExecuteOverlayButtonFallback");
        }
        if (!g_customExecutePanelButton)
        {
            DestroyCustomExecutePanelOverlayWidgets();
            return false;
        }

        g_customExecutePanelButton->setCaption("Execute");
        g_customExecutePanelButton->setNeedMouseFocus(true);
        g_customExecutePanelButton->setNeedKeyFocus(true);
        g_customExecutePanelButton->setEnabled(true);
        g_customExecutePanelButton->eventMouseButtonPressed += MyGUI::newDelegate(&OnCustomExecutePanelButtonPressed);
        g_customExecutePanelButton->eventMouseButtonClick += MyGUI::newDelegate(&OnCustomExecutePanelButtonClick);
        g_customExecutePanelValue = 0;

        g_customExecutePanelRoot->setVisible(false);
        return true;
    }
    catch (...)
    {
        DestroyCustomExecutePanelOverlayWidgets();
        return false;
    }
}

static bool TryResolveAnchorFromContextMenuRootWidget(
    MyGUI::Widget* root,
    MyGUI::IntCoord* anchorOut)
{
    if (!root || !anchorOut)
    {
        return false;
    }

    MyGUI::Widget* options = 0;
    MyGUI::Widget* nameText = 0;
    MyGUI::IntCoord optionsRect;
    MyGUI::IntCoord rootRect;
    __try
    {
        if (!root->getInheritedVisible())
        {
            return false;
        }

        options = root->findWidget(kContextMenuOptionsListWidgetName);
        nameText = root->findWidget(kContextMenuNameTextWidgetName);
        if (!options || !nameText)
        {
            return false;
        }

        optionsRect = options->getAbsoluteCoord();
        rootRect = root->getAbsoluteCoord();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    const int left = rootRect.left;
    const int top = optionsRect.top;
    const int right = rootRect.left + rootRect.width;
    const int bottom = optionsRect.top + optionsRect.height;
    const int width = right - left;
    const int height = bottom - top;
    if (width <= 0 || height <= 0)
    {
        return false;
    }

    *anchorOut = MyGUI::IntCoord(left, top, width, height);
    return true;
}

static bool TryReadUintptrAtOffset(const void* base, size_t offset, uintptr_t* valueOut)
{
    if (!base || !valueOut)
    {
        return false;
    }

    __try
    {
        const uintptr_t basePtr = reinterpret_cast<uintptr_t>(base);
        *valueOut = *reinterpret_cast<const uintptr_t*>(basePtr + offset);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryResolveAnchorFromContextMenuGuiObject(
    ContextMenu* menu,
    MyGUI::IntCoord* anchorOut)
{
    if (!menu || !anchorOut)
    {
        return false;
    }

    uintptr_t guiPtrs[2] = {
        reinterpret_cast<uintptr_t>(menu->menuGUI),
        reinterpret_cast<uintptr_t>(menu->menuGUI2)
    };

    for (size_t i = 0; i < 2; ++i)
    {
        const uintptr_t guiPtr = guiPtrs[i];
        if (guiPtr == 0)
        {
            continue;
        }

        for (size_t offset = 0; offset <= 0x180; offset += sizeof(uintptr_t))
        {
            uintptr_t candidatePtr = 0;
            if (!TryReadUintptrAtOffset(reinterpret_cast<const void*>(guiPtr), offset, &candidatePtr)
                || candidatePtr == 0)
            {
                continue;
            }

            if (TryResolveAnchorFromContextMenuRootWidget(
                reinterpret_cast<MyGUI::Widget*>(candidatePtr),
                anchorOut))
            {
                return true;
            }
        }
    }

    return false;
}

static bool TryResolveCustomExecutePanelAnchorRect(ContextMenu* menu, MyGUI::IntCoord* anchorOut)
{
    if (!anchorOut)
    {
        return false;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    bool mouseKnown = false;
    MyGUI::IntPoint mousePoint(0, 0);
    if (input)
    {
        mousePoint = input->getMousePosition();
        mouseKnown = true;
    }

    if (gui)
    {
        bool bestFound = false;
        bool bestContainsMouse = false;
        int bestDistanceScore = INT_MAX;
        int bestAreaScore = INT_MAX;
        MyGUI::IntCoord bestRect(0, 0, 0, 0);

        MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
        while (roots.next())
        {
            MyGUI::Widget* root = roots.current();
            if (!root)
            {
                continue;
            }

            MyGUI::IntCoord rootAnchor;
            if (!TryResolveAnchorFromContextMenuRootWidget(root, &rootAnchor))
            {
                continue;
            }
            const int left = rootAnchor.left;
            const int top = rootAnchor.top;
            const int right = rootAnchor.left + rootAnchor.width;
            const int bottom = rootAnchor.top + rootAnchor.height;
            const int width = rootAnchor.width;
            const int height = rootAnchor.height;

            bool containsMouse = false;
            int distanceScore = INT_MAX;
            if (mouseKnown)
            {
                const bool inX = mousePoint.left >= left && mousePoint.left < right;
                const bool inY = mousePoint.top >= top && mousePoint.top < bottom;
                containsMouse = inX && inY;

                int dx = 0;
                if (mousePoint.left < left)
                {
                    dx = left - mousePoint.left;
                }
                else if (mousePoint.left >= right)
                {
                    dx = mousePoint.left - right + 1;
                }

                int dy = 0;
                if (mousePoint.top < top)
                {
                    dy = top - mousePoint.top;
                }
                else if (mousePoint.top >= bottom)
                {
                    dy = mousePoint.top - bottom + 1;
                }

                distanceScore = dx + dy;
            }

            const int areaScore = width * height;
            const bool betterContains = !bestFound || (containsMouse && !bestContainsMouse);
            const bool sameContainment = bestFound && (containsMouse == bestContainsMouse);
            const bool betterDistance = sameContainment && distanceScore < bestDistanceScore;
            const bool sameDistance = sameContainment && distanceScore == bestDistanceScore;
            const bool betterArea = sameContainment && (betterDistance || (sameDistance && areaScore < bestAreaScore));

            if (betterContains || betterArea)
            {
                bestFound = true;
                bestContainsMouse = containsMouse;
                bestDistanceScore = distanceScore;
                bestAreaScore = areaScore;
                bestRect = MyGUI::IntCoord(left, top, width, height);
            }
        }

        if (bestFound)
        {
            *anchorOut = bestRect;
            g_customExecutePanelAnchorSource = 1;
            return true;
        }
    }

    if (TryResolveAnchorFromContextMenuGuiObject(menu, anchorOut))
    {
        g_customExecutePanelAnchorSource = 2;
        return true;
    }

    if (menu)
    {
        if (!menu->isVisible())
        {
            return false;
        }
    }

    if (input)
    {
        int ordersGuess = static_cast<int>(g_customExecutePanelOrdersCount);
        if (ordersGuess < 3)
        {
            ordersGuess = 3;
        }
        int estimatedMenuHeight = ((ordersGuess + 1) * 34) + 8;
        if (estimatedMenuHeight < 120)
        {
            estimatedMenuHeight = 120;
        }
        if (estimatedMenuHeight > 260)
        {
            estimatedMenuHeight = 260;
        }

        *anchorOut = MyGUI::IntCoord(
            mousePoint.left,
            mousePoint.top,
            kCustomExecutePanelMinWidth,
            estimatedMenuHeight);
        g_customExecutePanelAnchorSource = 3;
        return true;
    }

    g_customExecutePanelAnchorSource = 0;
    return false;
}

static void LayoutCustomExecutePanelOverlay(ContextMenu* menu)
{
    if (!g_customExecutePanelRoot || !g_customExecutePanelButton)
    {
        return;
    }

    MyGUI::IntCoord anchor;
    if (!TryResolveCustomExecutePanelAnchorRect(menu, &anchor))
    {
        return;
    }

    int ordersCount = static_cast<int>(g_customExecutePanelOrdersCount);
    if (ordersCount <= 0)
    {
        ordersCount = 3;
    }

    int rowHeight = g_config.executeButtonHeightPx;
    if (rowHeight <= 0)
    {
        rowHeight = kExecuteButtonDefaultHeight;
    }
    else if (rowHeight < kExecuteButtonRuntimeHeightMin)
    {
        rowHeight = kExecuteButtonRuntimeHeightMin;
    }

    int width = g_config.executeButtonWidthPx;
    if (width <= 0)
    {
        width = kExecuteButtonDefaultWidth;
    }
    else if (width < kExecuteButtonRuntimeWidthMin)
    {
        width = kExecuteButtonRuntimeWidthMin;
    }

    int basePanelTop = anchor.top + anchor.height + rowHeight + kCustomExecutePanelVerticalGap;
    if (g_customExecutePanelAnchorSource == 3)
    {
        basePanelTop += kCustomExecutePanelFallbackExtraYOffset;
    }
    else
    {
        basePanelTop += kCustomExecutePanelBottomExtraYOffset;
    }
    basePanelTop += kCustomExecutePanelAdditionalYOffset;

    const int basePanelLeft = anchor.left + kCustomExecutePanelHorizontalOffset;
    const int panelTop = basePanelTop + g_config.executeButtonOffsetYPx;
    const int panelLeft = basePanelLeft + g_config.executeButtonOffsetXPx;

    g_customExecutePanelRoot->setCoord(
        panelLeft,
        panelTop,
        width,
        rowHeight);

    if (g_config.debugContextMenu)
    {
        std::stringstream logline;
        logline << "Loot-Scoot-Execute DEBUG: custom_execute_panel_layout"
                << " anchor_source=" << std::dec << g_customExecutePanelAnchorSource
                << " anchor_left=" << std::dec << anchor.left
                << " anchor_top=" << anchor.top
                << " anchor_width=" << anchor.width
                << " anchor_height=" << anchor.height
                << " panel_left=" << g_customExecutePanelRoot->getLeft()
                << " panel_top=" << g_customExecutePanelRoot->getTop()
                << " panel_width=" << g_customExecutePanelRoot->getWidth()
                << " panel_height=" << g_customExecutePanelRoot->getHeight()
                << " horizontal_offset=" << kCustomExecutePanelHorizontalOffset
                << " bottom_extra_y=" << kCustomExecutePanelBottomExtraYOffset
                << " additional_y=" << kCustomExecutePanelAdditionalYOffset
                << " fallback_extra_y=" << kCustomExecutePanelFallbackExtraYOffset
                << " cfg_width=" << g_config.executeButtonWidthPx
                << " cfg_height=" << g_config.executeButtonHeightPx
                << " cfg_x=" << g_config.executeButtonOffsetXPx
                << " cfg_y=" << g_config.executeButtonOffsetYPx;
        PluginLog(logline.str().c_str());
    }

    int buttonLeft = width / 48;           // 2.0833%
    if (buttonLeft < 2)
    {
        buttonLeft = 2;
    }

    int innerTop = rowHeight / 20;         // ~5%
    if (innerTop < 1)
    {
        innerTop = 1;
    }
    int innerHeight = rowHeight - (innerTop * 2);
    if (innerHeight < 1)
    {
        innerTop = 0;
        innerHeight = rowHeight;
    }

    int buttonWidth = width - (buttonLeft * 2);
    if (buttonWidth < 20)
    {
        buttonWidth = 20;
    }

    g_customExecutePanelButton->setCoord(buttonLeft, innerTop, buttonWidth, innerHeight);
    if (g_customExecutePanelValue)
    {
        g_customExecutePanelValue->setVisible(false);
    }
}

static void HideCustomExecutePanelOverlay()
{
    if (g_customExecutePanelRoot)
    {
        g_customExecutePanelRoot->setVisible(false);
    }

    g_customExecutePanelVisible = false;
    g_customExecutePanelArmed = false;
    g_customExecutePanelRightMouseWasDown = false;
    g_customExecutePanelActorHandle.setNull();
    g_customExecutePanelActorPtr = 0;
    g_customExecutePanelTargetPtr = 0;
    g_customExecutePanelMenuPtr = 0;
    g_customExecutePanelOrdersCount = 0;
    g_customExecutePanelShowSeq = 0;
    g_customExecutePanelArmMs = 0;
    g_customExecutePanelAnchorSource = 0;
}

static void ArmCustomExecutePanelOverlay(
    ContextMenu* menu,
    RootObject* target,
    uint32_t ordersCount,
    uint64_t showSeq,
    DWORD nowMs)
{
    if (!IsCustomExecutePanelOverlayEnabled() || !menu || !target)
    {
        HideCustomExecutePanelOverlay();
        return;
    }

    if (!EnsureCustomExecutePanelOverlayWidgets())
    {
        HideCustomExecutePanelOverlay();
        return;
    }

    Character* actor = ResolveExecuteActorForPredicateWithTarget(target, false);
    if (!actor)
    {
        HideCustomExecutePanelOverlay();
        return;
    }

    g_customExecutePanelArmed = true;
    g_customExecutePanelActorHandle = actor;
    g_customExecutePanelActorPtr = reinterpret_cast<uintptr_t>(actor);
    g_customExecutePanelTargetPtr = reinterpret_cast<uintptr_t>(target);
    g_customExecutePanelMenuPtr = reinterpret_cast<uintptr_t>(menu);
    g_customExecutePanelOrdersCount = ordersCount;
    g_customExecutePanelShowSeq = showSeq;
    g_customExecutePanelArmMs = nowMs;
    g_customExecutePanelRightMouseWasDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

    LayoutCustomExecutePanelOverlay(menu);
    g_customExecutePanelRoot->setVisible(true);
    g_customExecutePanelVisible = true;

    if (g_config.debugContextMenu)
    {
        std::stringstream logline;
        logline << "Loot-Scoot-Execute INFO: custom_execute_panel_armed"
                << " show_seq=" << std::dec << showSeq
                << " menu=0x" << std::hex << reinterpret_cast<uintptr_t>(menu)
                << " actor=0x" << std::hex << g_customExecutePanelActorPtr
                << " target=0x" << std::hex << reinterpret_cast<uintptr_t>(target)
                << " orders_count=" << std::dec << ordersCount;
        PluginLog(logline.str().c_str());
    }
}

static void TickCustomExecutePanelOverlay(ContextMenu* menu, DWORD nowMs)
{
    (void)nowMs;
    if (!IsCustomExecutePanelOverlayEnabled() || !menu)
    {
        HideCustomExecutePanelOverlay();
        return;
    }

    if (!g_customExecutePanelArmed
        || g_customExecutePanelTargetPtr == 0
        || g_customExecutePanelMenuPtr == 0
        || g_customExecutePanelMenuPtr != reinterpret_cast<uintptr_t>(menu))
    {
        HideCustomExecutePanelOverlay();
        return;
    }

    bool menuVisible = false;
    menuVisible = menu->isVisible();
    if (!menuVisible)
    {
        HideCustomExecutePanelOverlay();
        return;
    }

    RootObject* target = reinterpret_cast<RootObject*>(g_customExecutePanelTargetPtr);
    Character* actor = TryResolveCharacterFromHandleSafe(g_customExecutePanelActorHandle);
    if (!actor)
    {
        actor = ResolveExecuteActorForPredicateWithTarget(target, false);
        if (actor)
        {
            g_customExecutePanelActorHandle = actor;
            g_customExecutePanelActorPtr = reinterpret_cast<uintptr_t>(actor);
        }
    }
    if (!actor)
    {
        HideCustomExecutePanelOverlay();
        return;
    }
    CanExecuteDiagnostics diagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
    const bool canExecute = CanExecuteFromNativeMenuSelection(actor, target, &diagnostics, false);
    if (!canExecute)
    {
        HideCustomExecutePanelOverlay();
        return;
    }

    if (!EnsureCustomExecutePanelOverlayWidgets())
    {
        HideCustomExecutePanelOverlay();
        return;
    }

    LayoutCustomExecutePanelOverlay(menu);
    g_customExecutePanelRoot->setVisible(true);
    g_customExecutePanelVisible = true;

    const bool rightDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    const bool rightReleasedThisFrame = g_customExecutePanelRightMouseWasDown && !rightDown;
    g_customExecutePanelRightMouseWasDown = rightDown;
    if (rightReleasedThisFrame && IsCustomExecutePanelButtonHovered())
    {
        (void)DispatchCustomExecutePanelAction("right_release_hover");
        return;
    }
}

#include "LootScootExecuteModHubClient.inl"
#include "LootScootExecuteModHubBridge.inl"
#include "LootScootExecuteHooksEntry.inl"
