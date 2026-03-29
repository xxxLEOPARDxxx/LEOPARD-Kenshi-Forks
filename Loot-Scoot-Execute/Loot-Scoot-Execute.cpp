#include <Debug.h>

#include <core/Functions.h>

#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/Kenshi.h>
#include <kenshi/Character.h>
#include <kenshi/Damages.h>
#include <kenshi/Faction.h>
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
static const DWORD kNativeMenuExecuteArmMaxAgeMs = 2500;
static const DWORD kQueuedExecuteMaxLifetimeMs = 45000;
static const DWORD kQueuedExecuteRepathIntervalMs = 200;
static const DWORD kQueuedExecuteAttackWindupMs = 180;
static const DWORD kQueuedExecutePostTriggerMaxDurationMs = 4000;
static const DWORD kQueuedExecuteInRangeConfirmMs = 0;
static const DWORD kQueuedExecuteFacingGraceMs = 1500;
static const float kQueuedExecuteFacingDotGraceMin = 0.75f;
static const int kQueuedExecuteDistanceMinMeters = 1;
static const int kQueuedExecuteDistanceMaxMeters = 200;
static const int kQueuedExecuteDefaultDistanceMeters = 2;
static const int kExecuteAllRadiusMinUnits = 1;
static const int kExecuteAllRadiusMaxUnits = 200;
static const int kExecuteAllRadiusDefaultUnits = 10;
static const float kQueuedExecuteApproachInsetMaxMeters = 1.0f;
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
static const size_t kExecuteAllBatchMaxTargets = 64;
static const int kExecuteAllWorldQueryMaxTargets = 128;
static const bool kEnableCustomExecutePanelOverlay = true;
static const int kCustomExecutePanelMinWidth = 280;
static const int kCustomExecutePanelMinRowHeight = 24;
static const int kCustomExecutePanelVerticalGap = 1;
static const int kCustomExecutePanelActionRowCount = 2;
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
    true,
    false,
    false,
    kQueuedExecuteDefaultDistanceMeters,
    kExecuteAllRadiusDefaultUnits,
    kExecuteButtonDefaultWidth,
    kExecuteButtonDefaultHeight,
    kExecuteButtonDefaultAbsoluteX,
    kExecuteButtonDefaultAbsoluteY };

static std::string g_settingsPath;
static bool g_configNeedsWriteBack = false;
static bool g_contextMenuHookInstallVerified = false;
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

static bool ShouldLogExecuteDebug()
{
    return kEnableInternalDebugLogs || g_config.debugExecuteLogging;
}

static void PluginLog(const char* message)
{
    if (!message || message[0] == '\0')
    {
        return;
    }

    if (!ShouldLogExecuteDebug() && IsInternalDebugLogLine(message))
    {
        return;
    }

    DebugLog(message);
}

static MyGUI::Widget* g_customExecutePanelRoot = 0;
static MyGUI::Button* g_customExecutePanelButton = 0;
static MyGUI::Button* g_customExecuteAllPanelButton = 0;
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
static int g_customExecutePanelLastDispatchAction = 0;
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
static bool g_queuedExecuteAttackTriggered = false;
static DWORD g_queuedExecuteAttackTriggeredMs = 0;
static DWORD g_queuedExecuteInRangeSinceMs = 0;
static const char* g_queuedExecuteAnimationMode = "none";
static bool g_queuedExecuteSlaveAnimPlaying = false;
static bool g_executeAllBatchActive = false;
static hand g_executeAllBatchActorHandle;
static uintptr_t g_executeAllBatchActorPtr = 0;
static hand g_executeAllBatchTargetHandles[kExecuteAllBatchMaxTargets];
static uintptr_t g_executeAllBatchTargetPtrs[kExecuteAllBatchMaxTargets];
static size_t g_executeAllBatchTargetCount = 0;
static size_t g_executeAllBatchNextTargetIndex = 0;
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

enum ExecutePredicateEntryPoint
{
    ExecutePredicateEntryPoint_NATIVE_MENU = 1
};

enum CustomExecutePanelAction
{
    CustomExecutePanelAction_NONE = 0,
    CustomExecutePanelAction_EXECUTE = 1,
    CustomExecutePanelAction_EXECUTE_ALL = 2
};

struct CanExecuteDiagnostics
{
    bool actorResolved;
    bool targetResolved;
    bool targetIsCharacter;
    bool targetIsEnemy;
    bool targetIsEnemyByPlayerFaction;
    bool targetIsEnemyByActor;
    bool targetIsPlayerCharacter;
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
static Character* ResolveExecuteActorForPredicateWithTarget(RootObject* target, bool allowAnyFallback);
static bool CanExecuteTarget(
    ExecutePredicateEntryPoint entryPoint,
    Character* actor,
    RootObject* target,
    CanExecuteDiagnostics* diagnosticsOut,
    bool verboseLog);
static bool CanExecuteFromNativeMenuSelection(Character* actor, RootObject* target, CanExecuteDiagnostics* diagnosticsOut, bool verboseLog);
static bool DispatchExecuteFromNativeMenuSelection(Character* actor, RootObject* target, bool verboseLog);
static void DisarmQueuedExecuteAction(const char* reason, bool verboseLog);
static bool QueueExecuteFromNativeMenuSelection(Character* actor, RootObject* target, bool verboseLog);
static bool QueueExecuteAllFromNativeMenuSelection(Character* actor, RootObject* target, bool verboseLog);
static void TickQueuedExecuteAction(PlayerInterface* player);
static bool TryResolvePlayerInterface(PlayerInterface** playerOut);
static bool TryReadRootObjectPosition(RootObject* object, Ogre::Vector3* positionOut);
static bool TryGetRootObjectHandleSafe(RootObject* object, hand* handleOut);
static RootObject* TryResolveRootObjectFromHandleSafe(const hand& rootObjectHandle);
static Character* TryResolveCharacterFromHandleSafe(const hand& characterHandle);
static float ComputeQueuedExecuteDistanceMeters(Character* actor, RootObject* target, bool targetIsCharacter);
static bool TryIssueQueuedExecuteFacingAdjust(Character* actor, const Ogre::Vector3& targetPos);
static bool TryTriggerQueuedExecuteAttackAnimation(Character* actor, RootObject* target);
static bool TryPlayCharacterAudioEvent(Character* character, const char* eventName, SoundRange range);
static bool TryPlayExecuteKillSound(
    Character* actor,
    Character* targetCharacter,
    const char** playedEventOut,
    const char** playedEmitterOut);
static void RefreshEffectiveContextMenuFeatureFlags(const char* source);
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
static void ModHub_OnPluginStart();
static bool ModHub_UseHubUi();
static bool ModHub_IsAttachRetryPending();
static EMC_Result ModHub_LastAttachFailureResult();

static CanExecuteDiagnostics MakeCanExecuteDiagnostics()
{
    CanExecuteDiagnostics diagnostics;
    std::memset(&diagnostics, 0, sizeof(diagnostics));
    diagnostics.targetType = NULL_ITEM;
    return diagnostics;
}

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
    diagnostics->foundDebugExecuteLogging = false;
    diagnostics->invalidDebugExecuteLogging = false;
    diagnostics->foundIgnoreExecuteAllianceCheck = false;
    diagnostics->invalidIgnoreExecuteAllianceCheck = false;
    diagnostics->foundExecuteDistanceMeters = false;
    diagnostics->usedLegacyExecuteDistanceMetersKey = false;
    diagnostics->invalidExecuteDistanceMeters = false;
    diagnostics->clampedExecuteDistanceMeters = false;
    diagnostics->foundExecuteAllRadiusUnits = false;
    diagnostics->invalidExecuteAllRadiusUnits = false;
    diagnostics->clampedExecuteAllRadiusUnits = false;
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
    g_config.enableExecuteKillSound = true;
    g_config.debugExecuteLogging = false;
    g_config.ignoreExecuteAllianceCheck = false;
    g_config.executeDistanceMeters = kQueuedExecuteDefaultDistanceMeters;
    g_config.executeAllRadiusUnits = kExecuteAllRadiusDefaultUnits;
    g_config.executeButtonWidthPx = kExecuteButtonDefaultWidth;
    g_config.executeButtonHeightPx = kExecuteButtonDefaultHeight;
    g_config.executeButtonOffsetXPx = kExecuteButtonDefaultAbsoluteX;
    g_config.executeButtonOffsetYPx = kExecuteButtonDefaultAbsoluteY;
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
         << " debug_execute_logging=" << (g_config.debugExecuteLogging ? "true" : "false")
         << " ignore_execute_alliance_check=" << (g_config.ignoreExecuteAllianceCheck ? "true" : "false")
         << " execute_distance_units=" << g_config.executeDistanceMeters
         << " execute_all_radius_units=" << g_config.executeAllRadiusUnits
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
    case ExecutePredicateEntryPoint_NATIVE_MENU:
        return "native_menu";
    default:
        return "unknown";
    }
}

static void CopySanitizedLogText(const char* source, char* dest, size_t destSize)
{
    if (!dest || destSize == 0)
    {
        return;
    }

    if (!source || source[0] == '\0')
    {
        source = "unnamed";
    }

    size_t writeIndex = 0;
    while (source[writeIndex] != '\0' && (writeIndex + 1) < destSize)
    {
        const unsigned char c = static_cast<unsigned char>(source[writeIndex]);
        if (c < 0x20)
        {
            dest[writeIndex] = ' ';
        }
        else if (source[writeIndex] == '"')
        {
            dest[writeIndex] = '\'';
        }
        else
        {
            dest[writeIndex] = source[writeIndex];
        }
        ++writeIndex;
    }

    dest[writeIndex] = '\0';
}

static Faction* TryGetRootObjectFactionForLog(RootObject* object)
{
    if (!object)
    {
        return 0;
    }

    __try
    {
        return object->getFaction();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

static Faction* TryGetPlayerFactionForLog()
{
    PlayerInterface* player = 0;
    if (!TryResolvePlayerInterface(&player) || !player)
    {
        return 0;
    }

    __try
    {
        return player->getFaction();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

static bool TryReadFactionNameForLog(Faction* faction, char* buffer, size_t bufferSize)
{
    if (!buffer || bufferSize == 0)
    {
        return false;
    }

    if (!faction)
    {
        CopySanitizedLogText("null", buffer, bufferSize);
        return false;
    }

    __try
    {
        const char* name = 0;
        if (!faction->name.empty())
        {
            name = faction->name.c_str();
        }
        CopySanitizedLogText(name, buffer, bufferSize);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        CopySanitizedLogText("exception", buffer, bufferSize);
        return false;
    }
}

static bool TryReadRootObjectNameForLog(RootObject* object, char* buffer, size_t bufferSize)
{
    if (!buffer || bufferSize == 0)
    {
        return false;
    }

    if (!object)
    {
        CopySanitizedLogText("null", buffer, bufferSize);
        return false;
    }

    __try
    {
        const char* name = 0;
        if (!object->displayName.empty())
        {
            name = object->displayName.c_str();
        }
        CopySanitizedLogText(name, buffer, bufferSize);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        CopySanitizedLogText("exception", buffer, bufferSize);
        return false;
    }
}

static const char* DescribeCanExecuteFailure(const CanExecuteDiagnostics& diagnostics)
{
    const bool requireEnemy = !g_config.ignoreExecuteAllianceCheck;
    if (!diagnostics.targetResolved)
    {
        return "target_null";
    }
    if (!diagnostics.targetIsCharacter)
    {
        return "target_not_character";
    }
    if (diagnostics.targetIsDead)
    {
        return "target_dead";
    }
    if (diagnostics.targetIsPlayerCharacter)
    {
        return "target_is_player_character";
    }
    if (requireEnemy && !diagnostics.targetIsEnemy)
    {
        return "target_not_enemy";
    }
    if (!diagnostics.targetIsIncapacitated)
    {
        return "target_not_incapacitated";
    }

    return "ok";
}

static void LogExecutePredicateInvestigation(
    ExecutePredicateEntryPoint entryPoint,
    Character* actor,
    RootObject* target,
    const CanExecuteDiagnostics& diagnostics,
    bool canExecute)
{
    if (!ShouldLogExecuteDebug())
    {
        return;
    }

    char actorName[128];
    char actorFaction[128];
    char playerFaction[128];
    char targetName[128];
    char targetFaction[128];
    (void)TryReadRootObjectNameForLog(actor, actorName, sizeof(actorName));
    (void)TryReadFactionNameForLog(TryGetRootObjectFactionForLog(actor), actorFaction, sizeof(actorFaction));
    (void)TryReadFactionNameForLog(TryGetPlayerFactionForLog(), playerFaction, sizeof(playerFaction));
    (void)TryReadRootObjectNameForLog(target, targetName, sizeof(targetName));
    (void)TryReadFactionNameForLog(TryGetRootObjectFactionForLog(target), targetFaction, sizeof(targetFaction));

    std::stringstream line;
    line << "[investigate][execute] predicate"
         << " source=" << ExecutePredicateEntryPointToString(entryPoint)
         << " result=" << (canExecute ? "true" : "false")
         << " reason=" << DescribeCanExecuteFailure(diagnostics)
         << " ignore_execute_alliance_check=" << (!g_config.ignoreExecuteAllianceCheck ? "false" : "true")
         << " actor=0x" << std::hex << diagnostics.actorPtr
         << " actor_name=\"" << actorName << "\""
         << " actor_faction=\"" << actorFaction << "\""
         << " player_faction=\"" << playerFaction << "\""
         << " target=0x" << diagnostics.targetPtr
         << " target_name=\"" << targetName << "\""
         << " target_faction=\"" << targetFaction << "\""
         << " target_type=" << std::dec << static_cast<int>(diagnostics.targetType)
         << " target_is_enemy_by_player=" << (diagnostics.targetIsEnemyByPlayerFaction ? "true" : "false")
         << " target_is_enemy_by_actor=" << (diagnostics.targetIsEnemyByActor ? "true" : "false")
         << " target_is_player_character=" << (diagnostics.targetIsPlayerCharacter ? "true" : "false")
         << " target_is_down=" << (diagnostics.targetIsDown ? "true" : "false")
         << " target_is_unconscious=" << (diagnostics.targetIsUnconscious ? "true" : "false")
         << " target_is_literally_unconscious=" << (diagnostics.targetIsLiterallyUnconscious ? "true" : "false")
         << " target_is_dead=" << (diagnostics.targetIsDead ? "true" : "false");
    PluginLog(line.str().c_str());
}

static void LogExecuteDispatchInvestigation(
    ExecutePredicateEntryPoint entryPoint,
    Character* actor,
    RootObject* target,
    const CanExecuteDiagnostics& canExecuteDiagnostics,
    bool dispatchSucceeded,
    const char* failureReason,
    bool actorIsPlayerCharacter,
    bool actorIsDead,
    bool actorIsUnconscious,
    bool targetHandleValid,
    bool directDamageDispatchSucceeded,
    bool targetDeadAfterDamageCheck,
    bool declareDeadAttempted,
    bool declareDeadCallSucceeded,
    bool targetDeadAfterFinalizeCheck)
{
    if (!ShouldLogExecuteDebug())
    {
        return;
    }

    char actorName[128];
    char actorFaction[128];
    char targetName[128];
    char targetFaction[128];
    (void)TryReadRootObjectNameForLog(actor, actorName, sizeof(actorName));
    (void)TryReadFactionNameForLog(TryGetRootObjectFactionForLog(actor), actorFaction, sizeof(actorFaction));
    (void)TryReadRootObjectNameForLog(target, targetName, sizeof(targetName));
    (void)TryReadFactionNameForLog(TryGetRootObjectFactionForLog(target), targetFaction, sizeof(targetFaction));

    std::stringstream line;
    line << "[investigate][execute] dispatch"
         << " source=" << ExecutePredicateEntryPointToString(entryPoint)
         << " result=" << (dispatchSucceeded ? "true" : "false")
         << " reason=" << (dispatchSucceeded ? "none" : (failureReason ? failureReason : "unknown"))
         << " predicate_reason=" << DescribeCanExecuteFailure(canExecuteDiagnostics)
         << " ignore_execute_alliance_check=" << (!g_config.ignoreExecuteAllianceCheck ? "false" : "true")
         << " actor=0x" << std::hex << reinterpret_cast<uintptr_t>(actor)
         << " actor_name=\"" << actorName << "\""
         << " actor_faction=\"" << actorFaction << "\""
         << " target=0x" << reinterpret_cast<uintptr_t>(target)
         << " target_name=\"" << targetName << "\""
         << " target_faction=\"" << targetFaction << "\""
         << " actor_is_player_character=" << (actorIsPlayerCharacter ? "true" : "false")
         << " actor_is_dead=" << (actorIsDead ? "true" : "false")
         << " actor_is_unconscious=" << (actorIsUnconscious ? "true" : "false")
         << " target_handle_valid=" << (targetHandleValid ? "true" : "false")
         << " direct_damage_dispatch_succeeded=" << (directDamageDispatchSucceeded ? "true" : "false")
         << " target_dead_after_damage_check=" << (targetDeadAfterDamageCheck ? "true" : "false")
         << " declare_dead_attempted=" << (declareDeadAttempted ? "true" : "false")
         << " declare_dead_call_succeeded=" << (declareDeadCallSucceeded ? "true" : "false")
         << " target_dead_after_finalize_check=" << (targetDeadAfterFinalizeCheck ? "true" : "false");
    PluginLog(line.str().c_str());
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
    bool* targetIsEnemyByPlayerOut,
    bool* targetIsEnemyByActorOut,
    bool* targetIsPlayerCharacterOut,
    bool* targetIsDeadOut,
    bool* targetIsDownOut,
    bool* targetIsUnconsciousOut,
    bool* targetIsLiterallyUnconsciousOut)
{
    if (!targetCharacter
        || !targetIsEnemyOut
        || !targetIsEnemyByPlayerOut
        || !targetIsEnemyByActorOut
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
        *targetIsEnemyByPlayerOut = false;
        *targetIsEnemyByActorOut = false;
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
                *targetIsEnemyByPlayerOut = player->isEnemy(targetCharacter);
            }
            if (actor)
            {
                *targetIsEnemyByActorOut = actor->isEnemy(targetCharacter, true);
            }
            *targetIsEnemyOut = *targetIsEnemyByPlayerOut || *targetIsEnemyByActorOut;
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

static bool CanExecuteTarget(
    ExecutePredicateEntryPoint entryPoint,
    Character* actor,
    RootObject* target,
    CanExecuteDiagnostics* diagnosticsOut,
    bool verboseLog)
{
    CanExecuteDiagnostics diagnostics = MakeCanExecuteDiagnostics();
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
        if (TryEvaluateCharacterExecuteFlags(
            actor,
            targetCharacter,
            &diagnostics.targetIsEnemy,
            &diagnostics.targetIsEnemyByPlayerFaction,
            &diagnostics.targetIsEnemyByActor,
            &diagnostics.targetIsPlayerCharacter,
            &diagnostics.targetIsDead,
            &diagnostics.targetIsDown,
            &diagnostics.targetIsUnconscious,
            &diagnostics.targetIsLiterallyUnconscious))
        {
            diagnostics.targetIsIncapacitated = diagnostics.targetIsDown
                || diagnostics.targetIsUnconscious
                || diagnostics.targetIsLiterallyUnconscious;
            if (diagnostics.targetIsPlayerCharacter)
            {
                diagnostics.targetIsEnemy = false;
            }
        }
        else
        {
            diagnostics.targetIsCharacter = false;
            diagnostics.targetIsEnemy = false;
            diagnostics.targetIsEnemyByPlayerFaction = false;
            diagnostics.targetIsEnemyByActor = false;
            diagnostics.targetIsPlayerCharacter = false;
            diagnostics.targetIsIncapacitated = false;
            diagnostics.targetIsDead = false;
            diagnostics.targetIsDown = false;
            diagnostics.targetIsUnconscious = false;
            diagnostics.targetIsLiterallyUnconscious = false;
        }
    }

    const bool passesAllianceCheck = g_config.ignoreExecuteAllianceCheck || diagnostics.targetIsEnemy;
    const bool canExecute = diagnostics.targetIsCharacter
        && !diagnostics.targetIsPlayerCharacter
        && passesAllianceCheck
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
                << " target_is_enemy_by_player=" << (diagnostics.targetIsEnemyByPlayerFaction ? "true" : "false")
                << " target_is_enemy_by_actor=" << (diagnostics.targetIsEnemyByActor ? "true" : "false")
                << " target_is_player_character=" << (diagnostics.targetIsPlayerCharacter ? "true" : "false")
                << " target_is_down=" << (diagnostics.targetIsDown ? "true" : "false")
                << " target_is_unconscious=" << (diagnostics.targetIsUnconscious ? "true" : "false")
                << " target_is_literally_unconscious=" << (diagnostics.targetIsLiterallyUnconscious ? "true" : "false")
                << " target_is_incapacitated=" << (diagnostics.targetIsIncapacitated ? "true" : "false")
                << " target_is_dead=" << (diagnostics.targetIsDead ? "true" : "false")
                << " reason=" << DescribeCanExecuteFailure(diagnostics);
        PluginLog(logline.str().c_str());
        LogExecutePredicateInvestigation(entryPoint, actor, target, diagnostics, canExecute);

        g_hasLastCanExecuteDecision = true;
        g_lastCanExecuteDecisionTargetPtr = diagnostics.targetPtr;
        g_lastCanExecuteDecisionResult = canExecute;
        g_lastCanExecuteDecisionLogMs = nowMs;
    }

    return canExecute;
}

static bool CanExecuteFromNativeMenuSelection(Character* actor, RootObject* target, CanExecuteDiagnostics* diagnosticsOut, bool verboseLog)
{
    return CanExecuteTarget(ExecutePredicateEntryPoint_NATIVE_MENU, actor, target, diagnosticsOut, verboseLog);
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

static bool TryGetRootObjectHandleSafe(RootObject* object, hand* handleOut)
{
    if (!object || !handleOut)
    {
        return false;
    }

    __try
    {
        *handleOut = object->getHandle();
        return handleOut->isValid();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        handleOut->setNull();
        return false;
    }
}

static RootObject* TryResolveRootObjectFromHandleSafe(const hand& rootObjectHandle)
{
    RootObject* resolved = 0;
    __try
    {
        if (rootObjectHandle.isValid())
        {
            resolved = rootObjectHandle.getRootObject();
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        resolved = 0;
    }
    return resolved;
}

static bool TryGetCharactersWithinSphere(
    lektor<RootObject*>* resultsOut,
    const Ogre::Vector3& spherePos,
    float radiusUnits,
    RootObject* skip)
{
    if (!resultsOut || !ou)
    {
        return false;
    }

    resultsOut->clear();

    __try
    {
        ou->getCharactersWithinSphere(
            *resultsOut,
            spherePos,
            radiusUnits,
            radiusUnits,
            radiusUnits,
            kExecuteAllWorldQueryMaxTargets,
            kExecuteAllWorldQueryMaxTargets,
            skip);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        resultsOut->clear();
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

static void ResetExecuteAllBatchState()
{
    g_executeAllBatchActive = false;
    g_executeAllBatchActorHandle.setNull();
    g_executeAllBatchActorPtr = 0;
    for (size_t i = 0; i < kExecuteAllBatchMaxTargets; ++i)
    {
        g_executeAllBatchTargetHandles[i].setNull();
        g_executeAllBatchTargetPtrs[i] = 0;
    }
    g_executeAllBatchTargetCount = 0;
    g_executeAllBatchNextTargetIndex = 0;
}

static bool IsExecuteAllBatchContinuationReason(const char* reason)
{
    if (!reason || reason[0] == '\0')
    {
        return false;
    }

    return std::strcmp(reason, "queue_completed") == 0
        || std::strcmp(reason, "queue_target_missing") == 0
        || std::strcmp(reason, "target_not_executable") == 0;
}

static bool AddExecuteAllBatchTarget(RootObject* target)
{
    if (!target || g_executeAllBatchTargetCount >= kExecuteAllBatchMaxTargets)
    {
        return false;
    }

    const uintptr_t targetPtr = reinterpret_cast<uintptr_t>(target);
    for (size_t i = 0; i < g_executeAllBatchTargetCount; ++i)
    {
        if (g_executeAllBatchTargetPtrs[i] == targetPtr)
        {
            return true;
        }
    }

    hand targetHandle;
    if (!TryGetRootObjectHandleSafe(target, &targetHandle))
    {
        return false;
    }

    const size_t nextIndex = g_executeAllBatchTargetCount;
    g_executeAllBatchTargetHandles[nextIndex] = targetHandle;
    g_executeAllBatchTargetPtrs[nextIndex] = targetPtr;
    g_executeAllBatchTargetCount = nextIndex + 1;
    return true;
}

static bool TryQueueNextExecuteAllBatchTarget(bool verboseLog)
{
    if (!g_executeAllBatchActive)
    {
        return false;
    }

    Character* actor = TryResolveCharacterFromHandleSafe(g_executeAllBatchActorHandle);
    if (!actor && g_executeAllBatchActorPtr != 0)
    {
        actor = reinterpret_cast<Character*>(g_executeAllBatchActorPtr);
    }
    if (!actor)
    {
        ResetExecuteAllBatchState();
        return false;
    }

    while (g_executeAllBatchNextTargetIndex < g_executeAllBatchTargetCount)
    {
        const size_t targetIndex = g_executeAllBatchNextTargetIndex;
        ++g_executeAllBatchNextTargetIndex;

        RootObject* target = TryResolveRootObjectFromHandleSafe(g_executeAllBatchTargetHandles[targetIndex]);
        if (!target)
        {
            continue;
        }

        if (QueueExecuteFromNativeMenuSelection(actor, target, verboseLog))
        {
            if (ShouldLogExecuteDebug())
            {
                std::stringstream line;
                line << "Loot-Scoot-Execute DEBUG: execute_all_batch_queue_next"
                     << " index=" << (targetIndex + 1)
                     << " total=" << g_executeAllBatchTargetCount
                     << " actor=0x" << std::hex << reinterpret_cast<uintptr_t>(actor)
                     << " target=0x" << reinterpret_cast<uintptr_t>(target);
                PluginLog(line.str().c_str());
            }
            return true;
        }
    }

    ResetExecuteAllBatchState();
    return false;
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

    if (verboseLog && g_queuedExecuteActive && ShouldLogExecuteDebug())
    {
        std::stringstream logline;
        logline << "Loot-Scoot-Execute DEBUG: queued_execute_disarmed"
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
    g_queuedExecuteAttackTriggered = false;
    g_queuedExecuteAttackTriggeredMs = 0;
    g_queuedExecuteInRangeSinceMs = 0;
    g_queuedExecuteAnimationMode = "none";
    g_queuedExecuteSlaveAnimPlaying = false;

    if (IsExecuteAllBatchContinuationReason(reason))
    {
        if (TryQueueNextExecuteAllBatchTarget(verboseLog))
        {
            return;
        }
    }

    ResetExecuteAllBatchState();
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

static void LogQueuedExecuteApproachSkip(
    Character* actor,
    RootObject* target,
    float executeDistance,
    float currentDistance)
{
    if (!ShouldLogExecuteDebug())
    {
        return;
    }

    std::stringstream line;
    line << "[investigate][execute] approach_skip"
         << " reason=already_in_range"
         << " actor=0x" << std::hex << reinterpret_cast<uintptr_t>(actor)
         << " target=0x" << reinterpret_cast<uintptr_t>(target)
         << std::dec
         << " execute_distance=" << executeDistance
         << " current_distance=" << currentDistance;
    PluginLog(line.str().c_str());
}

static void LogQueuedExecuteApproach(
    Character* actor,
    RootObject* target,
    float executeDistance,
    float approachDistance,
    float currentDistance,
    bool moveIssued,
    const Ogre::Vector3& actorPos,
    const Ogre::Vector3& targetPos,
    const Ogre::Vector3& approachPos)
{
    if (!ShouldLogExecuteDebug())
    {
        return;
    }

    std::stringstream line;
    line << "[investigate][execute] approach"
         << " actor=0x" << std::hex << reinterpret_cast<uintptr_t>(actor)
         << " target=0x" << reinterpret_cast<uintptr_t>(target)
         << std::dec
         << " execute_distance=" << executeDistance
         << " approach_distance=" << approachDistance
         << " current_distance=" << currentDistance
         << " move_issued=" << (moveIssued ? "true" : "false")
         << " actor_x=" << actorPos.x
         << " actor_z=" << actorPos.z
         << " target_x=" << targetPos.x
         << " target_z=" << targetPos.z
         << " approach_x=" << approachPos.x
         << " approach_z=" << approachPos.z;
    PluginLog(line.str().c_str());
}

static void LogQueuedExecuteSimpleProbe(
    const char* eventTag,
    const char* reason,
    Character* actor,
    RootObject* target)
{
    if (!ShouldLogExecuteDebug())
    {
        return;
    }

    std::stringstream line;
    line << "[investigate][execute] " << (eventTag ? eventTag : "queue_event")
         << " reason=" << (reason ? reason : "none")
         << " actor=0x" << std::hex << reinterpret_cast<uintptr_t>(actor)
         << " target=0x" << reinterpret_cast<uintptr_t>(target);
    PluginLog(line.str().c_str());
}

static void LogQueuedExecuteArmProbe(
    ExecutePredicateEntryPoint entryPoint,
    Character* actor,
    RootObject* target,
    const CanExecuteDiagnostics& diagnostics,
    bool initialApproachIssued,
    float executeDistance,
    float currentDistance)
{
    if (!ShouldLogExecuteDebug())
    {
        return;
    }

    std::stringstream line;
    line << "[investigate][execute] queue_arm"
         << " source=" << ExecutePredicateEntryPointToString(entryPoint)
         << " actor=0x" << std::hex << reinterpret_cast<uintptr_t>(actor)
         << " target=0x" << reinterpret_cast<uintptr_t>(target)
         << std::dec
         << " config_execute_distance=" << g_config.executeDistanceMeters
         << " effective_execute_distance=" << executeDistance
         << " current_distance=" << currentDistance
         << " initial_approach_issued=" << (initialApproachIssued ? "true" : "false")
         << " target_is_character=" << (diagnostics.targetIsCharacter ? "true" : "false")
         << " predicate_reason=" << DescribeCanExecuteFailure(diagnostics);
    PluginLog(line.str().c_str());
}

static void LogQueuedExecuteAnimationProbe(
    Character* actor,
    RootObject* target,
    bool targetIsCharacter,
    bool slaveAnimTriggered,
    bool fallbackQueued)
{
    if (!ShouldLogExecuteDebug())
    {
        return;
    }

    std::stringstream line;
    line << "[investigate][execute] animation"
         << " actor=0x" << std::hex << reinterpret_cast<uintptr_t>(actor)
         << " target=0x" << reinterpret_cast<uintptr_t>(target)
         << std::dec
         << " target_is_character=" << (targetIsCharacter ? "true" : "false")
         << " slave_anim_triggered=" << (slaveAnimTriggered ? "true" : "false")
         << " fallback_queued=" << (fallbackQueued ? "true" : "false")
         << " animation_mode=" << (g_queuedExecuteAnimationMode ? g_queuedExecuteAnimationMode : "none");
    PluginLog(line.str().c_str());
}

static void LogQueuedExecuteDispatchProbe(
    Character* actor,
    RootObject* target,
    float executeDistance,
    float distanceSq,
    bool queueReadyByRange,
    bool facingTarget)
{
    if (!ShouldLogExecuteDebug())
    {
        return;
    }

    std::stringstream line;
    line << "[investigate][execute] dispatch_attempt"
         << " actor=0x" << std::hex << reinterpret_cast<uintptr_t>(actor)
         << " target=0x" << reinterpret_cast<uintptr_t>(target)
         << std::dec
         << " config_execute_distance=" << g_config.executeDistanceMeters
         << " effective_execute_distance=" << executeDistance
         << " current_distance=" << std::sqrt(distanceSq)
         << " queue_ready=" << (queueReadyByRange ? "true" : "false")
         << " facing_target=" << (facingTarget ? "true" : "false")
         << " attack_triggered=" << (g_queuedExecuteAttackTriggered ? "true" : "false")
         << " animation_mode=" << (g_queuedExecuteAnimationMode ? g_queuedExecuteAnimationMode : "none");
    PluginLog(line.str().c_str());
}

static bool TryIssueQueuedExecuteApproach(Character* actor, RootObject* target)
{
    if (!actor || !target)
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

    itemType targetType = NULL_ITEM;
    const bool targetIsCharacter = TryGetRootObjectTypeForExecutePredicate(target, &targetType)
        && IsCharacterDataType(targetType);
    const float executeDistance = ComputeQueuedExecuteDistanceMeters(actor, target, targetIsCharacter);

    const float dx = actorPos.x - targetPos.x;
    const float dz = actorPos.z - targetPos.z;
    const float distanceSq = (dx * dx) + (dz * dz);
    const float executeDistanceSq = executeDistance * executeDistance;
    const float distance = distanceSq > 1.0e-6f ? std::sqrt(distanceSq) : 0.0f;
    if (distanceSq <= executeDistanceSq)
    {
        if (g_queuedExecuteLastApproachCommandMs == 0)
        {
            LogQueuedExecuteApproachSkip(actor, target, executeDistance, distance);
        }
        return false;
    }

    float approachInset = executeDistance * 0.5f;
    if (approachInset > kQueuedExecuteApproachInsetMaxMeters)
    {
        approachInset = kQueuedExecuteApproachInsetMaxMeters;
    }

    float approachDistance = executeDistance - approachInset;
    if (approachDistance < 0.0f)
    {
        approachDistance = 0.0f;
    }

    Ogre::Vector3 approachPos = targetPos;
    if (distance > 1.0e-6f)
    {
        const float scale = approachDistance / distance;
        approachPos.x = targetPos.x + (dx * scale);
        approachPos.z = targetPos.z + (dz * scale);
    }

    bool moveIssued = false;
    __try
    {
        actor->clearAllAIGoals();
        actor->setDestination(approachPos, false);
        moveIssued = true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        moveIssued = false;
    }

    if (g_queuedExecuteLastApproachCommandMs == 0 || !moveIssued)
    {
        LogQueuedExecuteApproach(
            actor,
            target,
            executeDistance,
            approachDistance,
            distance,
            moveIssued,
            actorPos,
            targetPos,
            approachPos);
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
        LogQueuedExecuteSimpleProbe("animation_skip", "actor_or_target_null", actor, target);
        return false;
    }

    itemType targetType = NULL_ITEM;
    if (!TryGetRootObjectTypeForExecutePredicate(target, &targetType)
        || !IsCharacterDataType(targetType))
    {
        LogQueuedExecuteSimpleProbe("animation_skip", "target_not_character", actor, target);
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
        LogQueuedExecuteAnimationProbe(actor, target, true, true, false);
        return true;
    }

    Ogre::Vector3 actorPos;
    const bool actorPosResolved = TryReadRootObjectPosition(actor, &actorPos);
    const Ogre::Vector3 crouchLocation = actorPosResolved ? actorPos : actor->getPosition();
    const bool queueToFront = true;
    actor->addOrder(0, CROUCH, actor, false, queueToFront, crouchLocation);
    g_queuedExecuteAnimationMode = "crouch_order_fallback";
    LogQueuedExecuteAnimationProbe(actor, target, true, false, true);
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
    if (g_config.executeDistanceMeters < kQueuedExecuteDistanceMinMeters)
    {
        return static_cast<float>(kQueuedExecuteDistanceMinMeters);
    }

    if (g_config.executeDistanceMeters > kQueuedExecuteDistanceMaxMeters)
    {
        return static_cast<float>(kQueuedExecuteDistanceMaxMeters);
    }

    return static_cast<float>(g_config.executeDistanceMeters);
}

static float ComputeExecuteAllRadiusUnits()
{
    if (g_config.executeAllRadiusUnits < kExecuteAllRadiusMinUnits)
    {
        return static_cast<float>(kExecuteAllRadiusMinUnits);
    }

    if (g_config.executeAllRadiusUnits > kExecuteAllRadiusMaxUnits)
    {
        return static_cast<float>(kExecuteAllRadiusMaxUnits);
    }

    return static_cast<float>(g_config.executeAllRadiusUnits);
}

static bool QueueExecuteAllFromNativeMenuSelection(Character* actor, RootObject* target, bool verboseLog)
{
    if (!g_effectiveEnableExecuteAction || !target)
    {
        return false;
    }

    if (!actor)
    {
        actor = ResolveExecuteActorForPredicateWithTarget(target, true);
    }
    if (!actor)
    {
        return false;
    }

    CanExecuteDiagnostics primaryDiagnostics = MakeCanExecuteDiagnostics();
    if (!CanExecuteFromNativeMenuSelection(actor, target, &primaryDiagnostics, verboseLog))
    {
        return false;
    }

    if (g_queuedExecuteActive)
    {
        DisarmQueuedExecuteAction("queue_replaced", false);
    }
    ResetExecuteAllBatchState();

    hand actorHandle;
    if (!TryGetRootObjectHandleSafe(actor, &actorHandle))
    {
        return false;
    }

    g_executeAllBatchActive = true;
    g_executeAllBatchActorHandle = actorHandle;
    g_executeAllBatchActorPtr = reinterpret_cast<uintptr_t>(actor);

    if (!AddExecuteAllBatchTarget(target))
    {
        ResetExecuteAllBatchState();
        return false;
    }

    const float radiusUnits = ComputeExecuteAllRadiusUnits();
    Ogre::Vector3 targetPos;
    if (ou && TryReadRootObjectPosition(target, &targetPos))
    {
        lektor<RootObject*> nearbyTargets;
        (void)TryGetCharactersWithinSphere(&nearbyTargets, targetPos, radiusUnits, target);

        const uint32_t nearbyCount = nearbyTargets.size();
        for (uint32_t i = 0; i < nearbyCount && g_executeAllBatchTargetCount < kExecuteAllBatchMaxTargets; ++i)
        {
            RootObject* nearbyTarget = nearbyTargets[i];
            if (!nearbyTarget || nearbyTarget == target)
            {
                continue;
            }

            CanExecuteDiagnostics nearbyDiagnostics = MakeCanExecuteDiagnostics();
            if (!CanExecuteFromNativeMenuSelection(actor, nearbyTarget, &nearbyDiagnostics, false))
            {
                continue;
            }

            (void)AddExecuteAllBatchTarget(nearbyTarget);
        }
    }

    if (ShouldLogExecuteDebug())
    {
        std::stringstream line;
        line << "Loot-Scoot-Execute DEBUG: execute_all_batch_armed"
             << " actor=0x" << std::hex << reinterpret_cast<uintptr_t>(actor)
             << " target=0x" << reinterpret_cast<uintptr_t>(target)
             << std::dec
             << " radius_units=" << radiusUnits
             << " target_count=" << g_executeAllBatchTargetCount;
        PluginLog(line.str().c_str());
    }

    if (!TryQueueNextExecuteAllBatchTarget(verboseLog))
    {
        ResetExecuteAllBatchState();
        return false;
    }

    return true;
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

    CanExecuteDiagnostics diagnostics = MakeCanExecuteDiagnostics();
    const bool canQueue = (entryPoint == ExecutePredicateEntryPoint_NATIVE_MENU)
        && CanExecuteFromNativeMenuSelection(actor, target, &diagnostics, verboseLog);
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
    g_queuedExecuteAttackTriggered = false;
    g_queuedExecuteAttackTriggeredMs = 0;
    g_queuedExecuteInRangeSinceMs = 0;
    g_queuedExecuteAnimationMode = "none";
    g_queuedExecuteSlaveAnimPlaying = false;

    bool initialApproachIssued = false;
    if (TryIssueQueuedExecuteApproach(actor, target))
    {
        initialApproachIssued = true;
        g_queuedExecuteLastApproachCommandMs = nowMs;
    }

    if (verboseLog && ShouldLogExecuteDebug())
    {
        float executeDistance = -1.0f;
        float currentDistance = -1.0f;
        Ogre::Vector3 actorPos;
        Ogre::Vector3 targetPos;
        if (TryReadRootObjectPosition(actor, &actorPos)
            && TryReadRootObjectPosition(target, &targetPos))
        {
            executeDistance = ComputeQueuedExecuteDistanceMeters(actor, target, diagnostics.targetIsCharacter);
            currentDistance = std::sqrt(ComputeSquaredDistanceXZ(actorPos, targetPos));
        }

        std::stringstream logline;
        logline << "Loot-Scoot-Execute DEBUG: queued_execute_armed"
                << " source=" << ExecutePredicateEntryPointToString(entryPoint)
                << " actor=0x" << std::hex << reinterpret_cast<uintptr_t>(actor)
                << " target=0x" << reinterpret_cast<uintptr_t>(target)
                << std::dec
                << " can_execute=true"
                << " execute_distance=" << executeDistance
                << " current_distance=" << currentDistance;
        PluginLog(logline.str().c_str());
    }

    float armExecuteDistance = -1.0f;
    float armCurrentDistance = -1.0f;
    Ogre::Vector3 armActorPos;
    Ogre::Vector3 armTargetPos;
    if (TryReadRootObjectPosition(actor, &armActorPos)
        && TryReadRootObjectPosition(target, &armTargetPos))
    {
        armExecuteDistance = ComputeQueuedExecuteDistanceMeters(actor, target, diagnostics.targetIsCharacter);
        armCurrentDistance = std::sqrt(ComputeSquaredDistanceXZ(armActorPos, armTargetPos));
    }
    LogQueuedExecuteArmProbe(
        entryPoint,
        actor,
        target,
        diagnostics,
        initialApproachIssued,
        armExecuteDistance,
        armCurrentDistance);

    return true;
}

static bool QueueExecuteFromNativeMenuSelection(Character* actor, RootObject* target, bool verboseLog)
{
    return QueueExecuteTarget(ExecutePredicateEntryPoint_NATIVE_MENU, actor, target, verboseLog);
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
        LogQueuedExecuteSimpleProbe("queue_timeout", "max_lifetime_elapsed", 0, 0);
        DisarmQueuedExecuteAction("queue_timeout", true);
        return;
    }

    Character* actor = 0;
    RootObject* target = 0;
    const char* resolveReason = "none";
    if (!TryResolveQueuedExecuteParticipants(&actor, &target, &resolveReason))
    {
        LogQueuedExecuteSimpleProbe("queue_resolve_failed", resolveReason, actor, target);
        DisarmQueuedExecuteAction(resolveReason, true);
        return;
    }

    CanExecuteDiagnostics diagnostics = MakeCanExecuteDiagnostics();
    if (!CanExecuteFromNativeMenuSelection(actor, target, &diagnostics, false))
    {
        if (ShouldLogExecuteDebug())
        {
            (void)CanExecuteFromNativeMenuSelection(actor, target, &diagnostics, true);
        }
        LogQueuedExecuteSimpleProbe("queue_predicate_failed", DescribeCanExecuteFailure(diagnostics), actor, target);
        DisarmQueuedExecuteAction("target_not_executable", true);
        return;
    }

    Ogre::Vector3 actorPos;
    Ogre::Vector3 targetPos;
    if (!TryReadRootObjectPosition(actor, &actorPos)
        || !TryReadRootObjectPosition(target, &targetPos))
    {
        LogQueuedExecuteSimpleProbe("queue_position_failed", "position_read_failed", actor, target);
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
        LogQueuedExecuteSimpleProbe("queue_post_trigger_timeout", "post_trigger_timeout", actor, target);
        DisarmQueuedExecuteAction("post_trigger_timeout", true);
        return;
    }

    if (g_queuedExecuteAttackTriggered && postTriggerAbortDistanceExceeded)
    {
        LogQueuedExecuteSimpleProbe("queue_post_trigger_abort", "post_trigger_drift_too_far", actor, target);
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
    const bool queueReadyByRange = inRangeConfirmed || (g_queuedExecuteAttackTriggered && inPostTriggerDispatchRange);

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
        if (!postTriggerCommitHold || !inPostTriggerDispatchRange)
        {
            return;
        }
    }
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
    }

    if (g_queuedExecuteAttackTriggeredMs != 0
        && !DebounceWindowElapsed(nowMs, g_queuedExecuteAttackTriggeredMs, kQueuedExecuteAttackWindupMs))
    {
        return;
    }

    LogQueuedExecuteDispatchProbe(actor, target, executeDistance, distanceSq, queueReadyByRange, facingTarget);
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

    CanExecuteDiagnostics canExecuteDiagnostics = MakeCanExecuteDiagnostics();
    const bool canExecute = (entryPoint == ExecutePredicateEntryPoint_NATIVE_MENU)
        && CanExecuteFromNativeMenuSelection(actor, target, &canExecuteDiagnostics, verboseLog);

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
                << " predicate_reason=" << DescribeCanExecuteFailure(canExecuteDiagnostics)
                << " reason=" << (dispatchSucceeded ? "none" : failureReason);
        PluginLog(logline.str().c_str());
        LogExecuteDispatchInvestigation(
            entryPoint,
            actor,
            target,
            canExecuteDiagnostics,
            dispatchSucceeded,
            failureReason,
            actorIsPlayerCharacter,
            actorIsDead,
            actorIsUnconscious,
            targetHandleValid,
            directDamageDispatchSucceeded,
            targetDeadAfterDamageCheck,
            declareDeadAttempted,
            declareDeadCallSucceeded,
            targetDeadAfterFinalizeCheck);
    }

    return dispatchSucceeded;
}

static bool DispatchExecuteFromNativeMenuSelection(Character* actor, RootObject* target, bool verboseLog)
{
    return DispatchExecuteTarget(ExecutePredicateEntryPoint_NATIVE_MENU, actor, target, verboseLog);
}

static bool IsCustomExecutePanelOverlayEnabled()
{
    return kEnableCustomExecutePanelOverlay
        && g_effectiveEnableContextMenuInjection
        && g_effectiveEnableExecuteAction;
}

static const char* CustomExecutePanelActionToString(CustomExecutePanelAction action)
{
    switch (action)
    {
    case CustomExecutePanelAction_EXECUTE:
        return "execute";
    case CustomExecutePanelAction_EXECUTE_ALL:
        return "execute_all";
    default:
        return "none";
    }
}

static bool TryGetCustomExecutePanelButtonRect(MyGUI::Button* button, MyGUI::IntCoord* buttonRectOut)
{
    if (!button || !buttonRectOut)
    {
        return false;
    }

    __try
    {
        if (!button->getInheritedVisible())
        {
            return false;
        }
        *buttonRectOut = button->getAbsoluteCoord();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool IsCustomExecutePanelButtonHovered(MyGUI::Button* button)
{
    if (!button || !g_customExecutePanelVisible)
    {
        return false;
    }

    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    if (!input)
    {
        return false;
    }

    MyGUI::IntCoord buttonRect(0, 0, 0, 0);
    if (!TryGetCustomExecutePanelButtonRect(button, &buttonRect))
    {
        return false;
    }

    const MyGUI::IntPoint mousePos = input->getMousePosition();
    return mousePos.left >= buttonRect.left
        && mousePos.left < (buttonRect.left + buttonRect.width)
        && mousePos.top >= buttonRect.top
        && mousePos.top < (buttonRect.top + buttonRect.height);
}

static CustomExecutePanelAction ResolveCustomExecutePanelActionForSender(MyGUI::Widget* sender)
{
    if (sender == g_customExecuteAllPanelButton)
    {
        return CustomExecutePanelAction_EXECUTE_ALL;
    }
    if (sender == g_customExecutePanelButton)
    {
        return CustomExecutePanelAction_EXECUTE;
    }

    return CustomExecutePanelAction_NONE;
}

static CustomExecutePanelAction GetHoveredCustomExecutePanelAction()
{
    if (IsCustomExecutePanelButtonHovered(g_customExecutePanelButton))
    {
        return CustomExecutePanelAction_EXECUTE;
    }
    if (IsCustomExecutePanelButtonHovered(g_customExecuteAllPanelButton))
    {
        return CustomExecutePanelAction_EXECUTE_ALL;
    }

    return CustomExecutePanelAction_NONE;
}

static bool DispatchCustomExecutePanelAction(CustomExecutePanelAction action, const char* sourceTag)
{
    if (!IsCustomExecutePanelOverlayEnabled() || action == CustomExecutePanelAction_NONE)
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
        && g_customExecutePanelLastDispatchAction == static_cast<int>(action)
        && g_customExecutePanelLastDispatchMs != 0
        && !DebounceWindowElapsed(nowMs, g_customExecutePanelLastDispatchMs, kCustomExecutePanelDispatchDedupMs);
    if (dedupDispatch)
    {
        return false;
    }

    const bool queued = (action == CustomExecutePanelAction_EXECUTE_ALL)
        ? QueueExecuteAllFromNativeMenuSelection(actor, target, true)
        : QueueExecuteFromNativeMenuSelection(actor, target, true);
    if (queued)
    {
        g_customExecutePanelLastDispatchMs = nowMs;
        g_customExecutePanelLastDispatchActorPtr = actorPtr;
        g_customExecutePanelLastDispatchTargetPtr = targetPtr;
        g_customExecutePanelLastDispatchAction = static_cast<int>(action);
    }

    if (ShouldLogExecuteDebug())
    {
        std::stringstream logline;
        logline << "Loot-Scoot-Execute DEBUG: custom_execute_panel_dispatch"
                << " action=" << CustomExecutePanelActionToString(action)
                << " source=" << (sourceTag ? sourceTag : "unknown")
                << " queued=" << (queued ? "true" : "false")
                << " actor=0x" << std::hex << actorPtr
                << " target=0x" << targetPtr;
        PluginLog(logline.str().c_str());
    }

    HideCustomExecutePanelOverlay();
    DisarmNativeMenuExecuteDispatchContext();
    DisarmNativeMenuOrderRemapContext();
    return queued;
}

static void OnCustomExecutePanelButtonPressed(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id)
{
    (void)left;
    (void)top;

    if (id != MyGUI::MouseButton::Left)
    {
        return;
    }

    const CustomExecutePanelAction action = ResolveCustomExecutePanelActionForSender(sender);
    if (action != CustomExecutePanelAction_NONE)
    {
        (void)DispatchCustomExecutePanelAction(action, "mouse_pressed");
    }
}

static void OnCustomExecutePanelButtonClick(MyGUI::Widget* sender)
{
    const CustomExecutePanelAction action = ResolveCustomExecutePanelActionForSender(sender);
    if (action != CustomExecutePanelAction_NONE)
    {
        (void)DispatchCustomExecutePanelAction(action, "mouse_click");
    }
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
    g_customExecuteAllPanelButton = 0;
    g_customExecutePanelValue = 0;
}

static bool EnsureCustomExecutePanelOverlayWidgets()
{
    if (g_customExecutePanelRoot && g_customExecutePanelButton && g_customExecuteAllPanelButton)
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
        const int minPanelHeight =
            (kCustomExecutePanelMinRowHeight * kCustomExecutePanelActionRowCount)
            + (kCustomExecutePanelVerticalGap * (kCustomExecutePanelActionRowCount - 1));
        g_customExecutePanelRoot = gui->createWidget<MyGUI::Widget>(
            "PanelEmpty",
            MyGUI::IntCoord(0, 0, kCustomExecutePanelMinWidth, minPanelHeight),
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

        const int executeAllTop = kCustomExecutePanelMinRowHeight + kCustomExecutePanelVerticalGap + 1;
        g_customExecuteAllPanelButton = g_customExecutePanelRoot->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(
                2,
                executeAllTop,
                kCustomExecutePanelMinWidth - 4,
                kCustomExecutePanelMinRowHeight - 2),
            MyGUI::Align::Default,
            "LSE_CustomExecuteAllOverlayButton");
        if (!g_customExecuteAllPanelButton)
        {
            g_customExecuteAllPanelButton = g_customExecutePanelRoot->createWidget<MyGUI::Button>(
                "Button",
                MyGUI::IntCoord(
                    2,
                    executeAllTop,
                    kCustomExecutePanelMinWidth - 4,
                    kCustomExecutePanelMinRowHeight - 2),
                MyGUI::Align::Default,
                "LSE_CustomExecuteAllOverlayButtonFallback");
        }
        if (!g_customExecuteAllPanelButton)
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

        g_customExecuteAllPanelButton->setCaption("Execute All");
        g_customExecuteAllPanelButton->setNeedMouseFocus(true);
        g_customExecuteAllPanelButton->setNeedKeyFocus(true);
        g_customExecuteAllPanelButton->setEnabled(true);
        g_customExecuteAllPanelButton->eventMouseButtonPressed += MyGUI::newDelegate(&OnCustomExecutePanelButtonPressed);
        g_customExecuteAllPanelButton->eventMouseButtonClick += MyGUI::newDelegate(&OnCustomExecutePanelButtonClick);
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

    // Anchor to the options list rect (not the root frame) to keep horizontal
    // alignment stable when menu root sizing/placement varies between targets.
    int left = optionsRect.left;
    const int top = optionsRect.top;
    int right = optionsRect.left + optionsRect.width;
    const int bottom = optionsRect.top + optionsRect.height;
    if (right <= left)
    {
        left = rootRect.left;
        right = rootRect.left + rootRect.width;
    }
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

        MyGUI::IntCoord directRect;
        if (TryResolveAnchorFromContextMenuRootWidget(
                reinterpret_cast<MyGUI::Widget*>(guiPtr),
                &directRect))
        {
            *anchorOut = directRect;
            return true;
        }

        for (size_t offset = 0; offset <= 0x180; offset += sizeof(uintptr_t))
        {
            uintptr_t candidatePtr = 0;
            if (!TryReadUintptrAtOffset(reinterpret_cast<const void*>(guiPtr), offset, &candidatePtr)
                || candidatePtr == 0)
            {
                continue;
            }

            MyGUI::IntCoord candidateRect;
            if (!TryResolveAnchorFromContextMenuRootWidget(
                    reinterpret_cast<MyGUI::Widget*>(candidatePtr),
                    &candidateRect))
            {
                continue;
            }

            *anchorOut = candidateRect;
            return true;
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
    if (!g_customExecutePanelRoot || !g_customExecutePanelButton || !g_customExecuteAllPanelButton)
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
    const int panelHeight =
        (rowHeight * kCustomExecutePanelActionRowCount)
        + (kCustomExecutePanelVerticalGap * (kCustomExecutePanelActionRowCount - 1));

    g_customExecutePanelRoot->setCoord(
        panelLeft,
        panelTop,
        width,
        panelHeight);

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
    const int secondButtonTop = rowHeight + kCustomExecutePanelVerticalGap + innerTop;
    g_customExecuteAllPanelButton->setCoord(buttonLeft, secondButtonTop, buttonWidth, innerHeight);
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
    CanExecuteDiagnostics diagnostics = MakeCanExecuteDiagnostics();
    const bool canExecute = CanExecuteFromNativeMenuSelection(actor, target, &diagnostics, false);
    if (!canExecute)
    {
        if (ShouldLogExecuteDebug())
        {
            (void)CanExecuteFromNativeMenuSelection(actor, target, &diagnostics, true);
        }
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
    const CustomExecutePanelAction hoveredAction = GetHoveredCustomExecutePanelAction();
    if (rightReleasedThisFrame && hoveredAction != CustomExecutePanelAction_NONE)
    {
        (void)DispatchCustomExecutePanelAction(hoveredAction, "right_release_hover");
        return;
    }
}

#include "LootScootExecuteModHubClient.inl"
#include "LootScootExecuteModHubBridge.inl"
#include "LootScootExecuteHooksEntry.inl"
