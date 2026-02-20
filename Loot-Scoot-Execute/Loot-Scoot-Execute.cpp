#include <Debug.h>

#include <core/Functions.h>

#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/Kenshi.h>
#include <kenshi/Character.h>
#include <kenshi/Damages.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/RootObject.h>
#include <kenshi/SaveManager.h>

#include <Windows.h>
#include <intrin.h>

#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "LootScootExecuteSharedContracts.h"

static const char* kPluginName = "Loot-Scoot-Execute";
static const char* kConfigFileName = "mod-config.json";
static const DWORD kTickAliveIntervalMs = 5000;
static const DWORD kNoSignalDisarmMs = 1500;
static const DWORD kArmedTimeoutMs = 60000;
static const DWORD kContextMenuProbeMinIntervalMs = 300;
static const DWORD kContextMenuProbePeriodicMs = 3000;
static const DWORD kContextMenuObserverSampleMinIntervalMs = 120;
static const DWORD kContextMenuObserverPeriodicMs = 3000;
static const DWORD kContextMenuObserverCorrelationWindowMs = 2000;
static const DWORD kContextMenuCloseBlockWindowMs = 2000;
static const DWORD kCanExecuteDecisionMinIntervalMs = 300;
static const DWORD kDebugExecuteContextTargetMaxAgeMs = 1500;
static const DWORD kNativeMenuExecuteArmMaxAgeMs = 2500;
static const int kDebugExecuteHotkeyVirtualKey = VK_F8;
static const size_t kContextMenuRowMaterializationMaxRows = 12;
static const bool kDebugForceAcceptCarryTaskForFilterSanity = false;
static const bool kDebugEnableRowInsertSubstitute = false;
static const bool kDebugEnableShowContextMenuOrderMutationFallback = false;
static const bool kEnableBuildRowsPreloopInjection = false;
static const bool kEnableRowInsertLateInjection = false;
static bool g_enableShowContextMenuPreInjection = false;
static bool g_forceMenuPersistence = false;
static bool g_enableBlockCloseForDebug = false;
static const int kContextMenuOrderIdLoot = 26;
static const int kContextMenuOrderIdLiftPersonPlayerOrder = static_cast<int>(LIFT_PERSON_PLAYER_ORDER);
static const int kContextMenuOrderIdStealthKill = static_cast<int>(STEALTH_KILL);
static const int kContextMenuOrderIdExecuteProxy = static_cast<int>(KILL_CAGE_OCCUPANT);
static const uint32_t kContextMenuMappingConfidenceMinSamples = 3;
static const uint32_t kContextMenuMappingConfidenceMinStabilityPercent = 95;
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

static PluginConfig g_config = { true, 2000, false, false, false, false, false, false };
static RuntimeState g_state = { false, false, false, 0, 0, 0, false };

static std::string g_settingsPath;
static bool g_hasSaveLoadHook = false;
static bool g_configNeedsWriteBack = false;
static bool g_contextMenuCompatibilityGatePassed = false;
static bool g_contextMenuHookInstallVerified = false;
static bool g_effectiveEnableContextMenuProbe = false;
static bool g_effectiveEnableContextMenuInjection = false;
static bool g_effectiveEnableExecuteAction = false;
static std::string g_contextMenuGateFailureReason;
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
static DWORD g_lastContextMenuProbeLogMs = 0;
static bool g_hasContextMenuProbeSnapshot = false;
static bool g_lastContextMenuProbeOn = false;
static bool g_lastContextMenuProbeVisible = false;
static uintptr_t g_lastContextMenuProbeWhatPtr = 0;
static uintptr_t g_lastContextMenuProbeMouseRightTargetPtr = 0;
static uint32_t g_lastContextMenuProbeOrdersCount = 0;
static size_t g_lastContextMenuProbeSampleCount = 0;
static int g_lastContextMenuProbeOrderSample[kContextMenuProbeOrderSampleCount] = { 0 };
static std::string g_lastContextMenuProbeName;
static uint64_t g_contextMenuShowProbeEventSeq = 0;
static DWORD g_contextMenuShowProbeEventMs = 0;
static bool g_contextMenuShowProbeEventOn = false;
static bool g_contextMenuShowProbeEventVisible = false;
static uintptr_t g_contextMenuShowProbeEventWhatPtr = 0;
static uintptr_t g_contextMenuShowProbeEventMouseRightTargetPtr = 0;
static uint32_t g_contextMenuShowProbeEventOrdersCount = 0;
static size_t g_contextMenuShowProbeEventSampleCount = 0;
static int g_contextMenuShowProbeEventOrderSample[kContextMenuProbeOrderSampleCount] = { 0 };
static uint64_t g_currentShowSeq = 0;
static uint64_t g_lastShowSeq = 0;
static DWORD g_lastShowTimeMs = 0;
static uintptr_t g_lastMenuPtr = 0;
static bool g_lastWasDownedEnemy = false;
static bool g_lastShowTargetIsEnemy = false;
static bool g_lastShowTargetIsIncapacitated = false;
static bool g_lastShowTargetIsDead = false;
static DWORD g_lastContextMenuObserverSampleMs = 0;
static DWORD g_lastContextMenuObserverLogMs = 0;
static bool g_hasContextMenuObserverSnapshot = false;
static bool g_lastContextMenuObserverVisible = false;
static uintptr_t g_lastContextMenuObserverMouseRightTargetPtr = 0;
static uint32_t g_lastContextMenuObserverOrdersCount = 0;
static size_t g_lastContextMenuObserverSampleCount = 0;
static int g_lastContextMenuObserverOrderSample[kContextMenuProbeOrderSampleCount] = { 0 };
static uint64_t g_lastContextMenuObserverShowEventSeq = 0;
static uint64_t g_lastContextMenuRowSnapshotShowSeq = 0;
static DWORD g_lastContextMenuRowSnapshotMs = 0;
static std::string g_runtimeGameVersion;
static std::string g_runtimeLocaleTag;
static bool g_contextMenuMappingConfidenceGatePassed = false;
static std::string g_contextMenuMappingGateFailureReason;

enum ContextTypeKey
{
    ContextTypeKey_UNKNOWN = 0,
    ContextTypeKey_DOWNED_ENEMY = 1,
    ContextTypeKey_CONSCIOUS_ENEMY = 2,
    ContextTypeKey_ALLY_DOWNED = 3,
    ContextTypeKey_CORPSE = 4,
    ContextTypeKey_ITEM_CONTAINER = 5,
    ContextTypeKey_BUILDING = 6
};

struct ContextMenuMappingEntry
{
    std::string gameVersion;
    std::string localeTag;
    ContextTypeKey contextType;
    uint32_t sampleCount;
    uint32_t stableSampleCount;
    uint32_t mismatchSampleCount;
    uint32_t baselineOrdersCount;
    size_t baselineSampleCount;
    int baselineOrderSample[kContextMenuProbeOrderSampleCount];
};

static std::vector<ContextMenuMappingEntry> g_contextMenuMappingTable;
static DWORD g_lastCanExecuteDecisionLogMs = 0;
static uintptr_t g_lastCanExecuteDecisionTargetPtr = 0;
static bool g_hasLastCanExecuteDecision = false;
static bool g_lastCanExecuteDecisionResult = false;
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
static void (*SaveManager_loadByInfo_orig)(SaveManager*, const SaveInfo&, bool) = 0;
static void (*SaveManager_loadByName_orig)(SaveManager*, const std::string&) = 0;

static bool DebounceWindowElapsed(DWORD nowMs, DWORD lastEventMs, DWORD minGapMs);
static void DisarmPauseAfterLoad();
static const char* ContextTypeKeyToString(ContextTypeKey type);
static std::string DetectRuntimeLocaleTag();
static bool ReevaluateContextMenuMappingConfidenceGate(const char* source, bool forceLog);
static Character* ResolveExecuteActorForPredicate();
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
static void TickDebugExecuteHotkey(PlayerInterface* thisptr);

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
    diagnostics->foundEnableContextMenuProbe = false;
    diagnostics->invalidEnableContextMenuProbe = false;
    diagnostics->foundEnableContextMenuInjection = false;
    diagnostics->invalidEnableContextMenuInjection = false;
    diagnostics->foundEnableExecuteAction = false;
    diagnostics->invalidEnableExecuteAction = false;
    diagnostics->foundDebugContextMenu = false;
    diagnostics->invalidDebugContextMenu = false;
    diagnostics->foundEnableDebugDirectDamageFallback = false;
    diagnostics->invalidEnableDebugDirectDamageFallback = false;
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
    g_config.pauseDebounceMs = 2000;
    g_config.debugLogTransitions = false;
    g_config.enableContextMenuProbe = false;
    g_config.enableContextMenuInjection = false;
    g_config.enableExecuteAction = false;
    g_config.debugContextMenu = false;
    g_config.enableDebugDirectDamageFallback = false;
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

    g_configNeedsWriteBack = (!foundConfigFile) || needsWriteBack;
    if (!foundConfigFile)
    {
        DebugLog("Loot-Scoot-Execute INFO: mod-config.json not found; using defaults");
    }

    std::stringstream info;
    info << "Loot-Scoot-Execute INFO: loaded config enabled=" << (g_config.enabled ? "true" : "false")
         << " settings_path=\"" << g_settingsPath << "\""
         << " pause_debounce_ms=" << g_config.pauseDebounceMs
         << " debug_log_transitions=" << (g_config.debugLogTransitions ? "true" : "false")
         << " enable_context_menu_probe=" << (g_config.enableContextMenuProbe ? "true" : "false")
         << " enable_context_menu_injection=" << (g_config.enableContextMenuInjection ? "true" : "false")
         << " enable_execute_action=" << (g_config.enableExecuteAction ? "true" : "false")
         << " debug_context_menu=" << (g_config.debugContextMenu ? "true" : "false")
         << " enable_debug_direct_damage_fallback=" << (g_config.enableDebugDirectDamageFallback ? "true" : "false");
    DebugLog(info.str().c_str());
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
        logline << "Loot-Scoot-Execute DEBUG: armed from " << source;
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
            ErrorLog("Loot-Scoot-Execute WARN: game world unavailable while waiting for save-load completion");
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
        ErrorLog("Loot-Scoot-Execute WARN: exception while querying save-load state");
        return false;
    }
}

static bool ForcePauseTrue()
{
    if (!ou)
    {
        if (!g_state.loggedWorldUnavailable)
        {
            ErrorLog("Loot-Scoot-Execute WARN: game world unavailable; cannot force pause");
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
        ErrorLog("Loot-Scoot-Execute WARN: exception while forcing paused state");
        return false;
    }
}

static void TryPauseAndDisarm(DWORD nowMs, const char* reason)
{
    if (!DebounceWindowElapsed(nowMs, g_state.lastPauseMs, g_config.pauseDebounceMs))
    {
        if (g_config.debugLogTransitions)
        {
            DebugLog("Loot-Scoot-Execute DEBUG: pause skipped (debounce)");
        }
        DisarmPauseAfterLoad();
        return;
    }

    if (ForcePauseTrue())
    {
        g_state.lastPauseMs = nowMs;
        std::stringstream info;
        info << "Loot-Scoot-Execute INFO: paused_after_load=true source=" << reason;
        DebugLog(info.str().c_str());
    }

    DisarmPauseAfterLoad();
}

static const char* ContextTypeKeyToString(ContextTypeKey type)
{
    switch (type)
    {
    case ContextTypeKey_DOWNED_ENEMY:
        return "downed_enemy";
    case ContextTypeKey_CONSCIOUS_ENEMY:
        return "conscious_enemy";
    case ContextTypeKey_ALLY_DOWNED:
        return "ally_downed";
    case ContextTypeKey_CORPSE:
        return "corpse";
    case ContextTypeKey_ITEM_CONTAINER:
        return "item_container";
    case ContextTypeKey_BUILDING:
        return "building";
    default:
        return "unknown";
    }
}

static std::string ToLowerAscii(const std::string& value)
{
    std::string lowered(value);
    for (size_t i = 0; i < lowered.size(); ++i)
    {
        lowered[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
    }
    return lowered;
}

static bool ContainsInsensitive(const std::string& haystack, const char* needle)
{
    if (!needle || !needle[0])
    {
        return false;
    }

    const std::string loweredHaystack = ToLowerAscii(haystack);
    std::string loweredNeedle;
    for (size_t i = 0; needle[i] != '\0'; ++i)
    {
        loweredNeedle.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(needle[i]))));
    }

    return loweredHaystack.find(loweredNeedle) != std::string::npos;
}

static bool IsOrderSampleEqual(
    uint32_t lhsOrdersCount,
    const int* lhsOrderSample,
    size_t lhsSampleCount,
    uint32_t rhsOrdersCount,
    const int* rhsOrderSample,
    size_t rhsSampleCount)
{
    if (lhsOrdersCount != rhsOrdersCount || lhsSampleCount != rhsSampleCount)
    {
        return false;
    }

    if ((!lhsOrderSample && lhsSampleCount > 0) || (!rhsOrderSample && rhsSampleCount > 0))
    {
        return false;
    }

    for (size_t i = 0; i < lhsSampleCount; ++i)
    {
        if (lhsOrderSample[i] != rhsOrderSample[i])
        {
            return false;
        }
    }
    return true;
}

static bool IsProvisionalDownedEnemyOrderSignature(
    uint32_t ordersCount,
    const int* orderSample,
    size_t orderSampleCount)
{
    return ordersCount == 3
        && orderSample
        && orderSampleCount >= 3
        && orderSample[0] == 26
        && orderSample[1] == 225
        && orderSample[2] == 25;
}

static ContextTypeKey InferContextTypeKeyFromProbe(
    bool whatTypeResolved,
    int whatType,
    const std::string& contextMenuName,
    uint32_t ordersCount,
    const int* orderSample,
    size_t orderSampleCount)
{
    if (IsProvisionalDownedEnemyOrderSignature(ordersCount, orderSample, orderSampleCount))
    {
        return ContextTypeKey_DOWNED_ENEMY;
    }

    if (ContainsInsensitive(contextMenuName, "corpse"))
    {
        return ContextTypeKey_CORPSE;
    }

    if (ContainsInsensitive(contextMenuName, "building")
        || ContainsInsensitive(contextMenuName, "house")
        || ContainsInsensitive(contextMenuName, "gate"))
    {
        return ContextTypeKey_BUILDING;
    }

    if (ContainsInsensitive(contextMenuName, "container")
        || ContainsInsensitive(contextMenuName, "chest")
        || ContainsInsensitive(contextMenuName, "barrel")
        || ContainsInsensitive(contextMenuName, "crate"))
    {
        return ContextTypeKey_ITEM_CONTAINER;
    }

    if (whatTypeResolved
        && (whatType == static_cast<int>(CHARACTER)
            || whatType == static_cast<int>(HUMAN_CHARACTER)
            || whatType == static_cast<int>(ANIMAL_CHARACTER)))
    {
        return ContextTypeKey_CONSCIOUS_ENEMY;
    }

    return ContextTypeKey_UNKNOWN;
}

static uint32_t ComputeStabilityPercent(const ContextMenuMappingEntry& entry)
{
    if (entry.sampleCount == 0)
    {
        return 0;
    }
    return static_cast<uint32_t>((entry.stableSampleCount * 100U) / entry.sampleCount);
}

static ContextMenuMappingEntry* FindContextMenuMappingEntry(
    const std::string& gameVersion,
    const std::string& localeTag,
    ContextTypeKey contextType)
{
    for (size_t i = 0; i < g_contextMenuMappingTable.size(); ++i)
    {
        ContextMenuMappingEntry& entry = g_contextMenuMappingTable[i];
        if (entry.gameVersion == gameVersion
            && entry.localeTag == localeTag
            && entry.contextType == contextType)
        {
            return &entry;
        }
    }
    return 0;
}

static const ContextMenuMappingEntry* FindContextMenuMappingEntryConst(
    const std::string& gameVersion,
    const std::string& localeTag,
    ContextTypeKey contextType)
{
    for (size_t i = 0; i < g_contextMenuMappingTable.size(); ++i)
    {
        const ContextMenuMappingEntry& entry = g_contextMenuMappingTable[i];
        if (entry.gameVersion == gameVersion
            && entry.localeTag == localeTag
            && entry.contextType == contextType)
        {
            return &entry;
        }
    }
    return 0;
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

static void SeedContextMenuMappingTable()
{
    g_contextMenuMappingTable.clear();

    if (g_runtimeGameVersion != "1.0.65" || g_runtimeLocaleTag != "en-US")
    {
        return;
    }

    ContextMenuMappingEntry entry;
    entry.gameVersion = g_runtimeGameVersion;
    entry.localeTag = g_runtimeLocaleTag;
    entry.contextType = ContextTypeKey_DOWNED_ENEMY;
    entry.sampleCount = 4;
    entry.stableSampleCount = 4;
    entry.mismatchSampleCount = 0;
    entry.baselineOrdersCount = 3;
    entry.baselineSampleCount = 3;
    entry.baselineOrderSample[0] = 26;
    entry.baselineOrderSample[1] = 225;
    entry.baselineOrderSample[2] = 25;
    for (size_t i = 3; i < kContextMenuProbeOrderSampleCount; ++i)
    {
        entry.baselineOrderSample[i] = 0;
    }

    g_contextMenuMappingTable.push_back(entry);

    DebugLog("Loot-Scoot-Execute INFO: seeded context-menu mapping key=1.0.65|en-US|downed_enemy samples=4 stability=100");
}

static void RecordContextMenuMappingSample(
    const char* source,
    ContextTypeKey contextType,
    uint32_t ordersCount,
    const int* orderSample,
    size_t orderSampleCount)
{
    if (g_runtimeGameVersion.empty() || g_runtimeLocaleTag.empty())
    {
        return;
    }

    if (orderSampleCount > kContextMenuProbeOrderSampleCount)
    {
        orderSampleCount = kContextMenuProbeOrderSampleCount;
    }

    ContextMenuMappingEntry* entry = FindContextMenuMappingEntry(g_runtimeGameVersion, g_runtimeLocaleTag, contextType);
    if (!entry)
    {
        ContextMenuMappingEntry newEntry;
        newEntry.gameVersion = g_runtimeGameVersion;
        newEntry.localeTag = g_runtimeLocaleTag;
        newEntry.contextType = contextType;
        newEntry.sampleCount = 0;
        newEntry.stableSampleCount = 0;
        newEntry.mismatchSampleCount = 0;
        newEntry.baselineOrdersCount = ordersCount;
        newEntry.baselineSampleCount = orderSampleCount;
        for (size_t i = 0; i < orderSampleCount; ++i)
        {
            newEntry.baselineOrderSample[i] = orderSample[i];
        }
        for (size_t i = orderSampleCount; i < kContextMenuProbeOrderSampleCount; ++i)
        {
            newEntry.baselineOrderSample[i] = 0;
        }
        g_contextMenuMappingTable.push_back(newEntry);
        entry = &g_contextMenuMappingTable[g_contextMenuMappingTable.size() - 1];
    }

    entry->sampleCount += 1;
    const bool matchedBaseline = IsOrderSampleEqual(
        ordersCount,
        orderSample,
        orderSampleCount,
        entry->baselineOrdersCount,
        entry->baselineOrderSample,
        entry->baselineSampleCount);
    if (matchedBaseline)
    {
        entry->stableSampleCount += 1;
    }
    else
    {
        entry->mismatchSampleCount += 1;
    }

    std::stringstream detail;
    detail << "Loot-Scoot-Execute DEBUG: context_menu_mapping_sample"
           << " source=" << (source ? source : "unknown")
           << " key=" << entry->gameVersion << "|" << entry->localeTag << "|" << ContextTypeKeyToString(entry->contextType)
           << " sample_count=" << entry->sampleCount
           << " stable_count=" << entry->stableSampleCount
           << " mismatch_count=" << entry->mismatchSampleCount
           << " stability_percent=" << ComputeStabilityPercent(*entry)
           << " observed_orders_count=" << ordersCount
           << " baseline_orders_count=" << entry->baselineOrdersCount
           << " matched_baseline=" << (matchedBaseline ? "true" : "false");
    DebugLog(detail.str().c_str());
}

static bool ReevaluateContextMenuMappingConfidenceGate(const char* source, bool forceLog)
{
    const bool previousState = g_contextMenuMappingConfidenceGatePassed;
    const std::string previousReason = g_contextMenuMappingGateFailureReason;

    g_contextMenuMappingConfidenceGatePassed = false;
    g_contextMenuMappingGateFailureReason = "unknown";

    if (g_runtimeGameVersion.empty() || g_runtimeLocaleTag.empty())
    {
        g_contextMenuMappingGateFailureReason = "runtime_key_uninitialized";
    }
    else if (g_runtimeGameVersion == "1.0.65" && g_runtimeLocaleTag == "en-US")
    {
        // For the validated runtime key we intentionally keep injection enabled.
        // Downed-enemy menus can legitimately vary (for example [26,225] vs [26,225,25]),
        // so strict stability gating causes false fail-close behavior.
        g_contextMenuMappingConfidenceGatePassed = true;
        g_contextMenuMappingGateFailureReason.clear();
    }
    else
    {
        const ContextMenuMappingEntry* downedEnemyEntry = FindContextMenuMappingEntryConst(
            g_runtimeGameVersion,
            g_runtimeLocaleTag,
            ContextTypeKey_DOWNED_ENEMY);

        if (!downedEnemyEntry)
        {
            g_contextMenuMappingGateFailureReason = "downed_enemy_no_samples";
        }
        else if (downedEnemyEntry->sampleCount < kContextMenuMappingConfidenceMinSamples)
        {
            std::stringstream reason;
            reason << "downed_enemy_samples_below_threshold("
                   << downedEnemyEntry->sampleCount
                   << "<"
                   << kContextMenuMappingConfidenceMinSamples
                   << ")";
            g_contextMenuMappingGateFailureReason = reason.str();
        }
        else
        {
            const uint32_t stabilityPercent = ComputeStabilityPercent(*downedEnemyEntry);
            if (stabilityPercent < kContextMenuMappingConfidenceMinStabilityPercent)
            {
                std::stringstream reason;
                reason << "downed_enemy_stability_below_threshold("
                       << stabilityPercent
                       << "<"
                       << kContextMenuMappingConfidenceMinStabilityPercent
                       << ")";
                g_contextMenuMappingGateFailureReason = reason.str();
            }
            else
            {
                g_contextMenuMappingConfidenceGatePassed = true;
                g_contextMenuMappingGateFailureReason.clear();
            }
        }
    }

    const bool changed = (previousState != g_contextMenuMappingConfidenceGatePassed)
        || (previousReason != g_contextMenuMappingGateFailureReason);
    if (changed || forceLog)
    {
        std::stringstream detail;
        detail << "Loot-Scoot-Execute INFO: context_menu_mapping_gate"
               << " source=" << (source ? source : "unknown")
               << " passed=" << (g_contextMenuMappingConfidenceGatePassed ? "true" : "false")
               << " reason="
               << (g_contextMenuMappingConfidenceGatePassed ? "none" : g_contextMenuMappingGateFailureReason)
               << " key=" << g_runtimeGameVersion << "|" << g_runtimeLocaleTag << "|downed_enemy";
        DebugLog(detail.str().c_str());
    }

    return changed;
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
    bool* targetIsDeadOut,
    bool* targetIsDownOut,
    bool* targetIsUnconsciousOut,
    bool* targetIsLiterallyUnconsciousOut)
{
    if (!targetCharacter
        || !targetIsEnemyOut
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

static Character* ResolveExecuteActorForPredicate()
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
        if (TryEvaluateCharacterExecuteFlags(
            actor,
            targetCharacter,
            &diagnostics.targetIsEnemy,
            &diagnostics.targetIsDead,
            &diagnostics.targetIsDown,
            &diagnostics.targetIsUnconscious,
            &diagnostics.targetIsLiterallyUnconscious))
        {
            diagnostics.targetIsIncapacitated = diagnostics.targetIsDown
                || diagnostics.targetIsUnconscious
                || diagnostics.targetIsLiterallyUnconscious;
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
        DebugLog(logline.str().c_str());

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
                << " reason=" << (dispatchSucceeded ? "none" : failureReason);
        DebugLog(logline.str().c_str());
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
    DebugLog(logline.str().c_str());
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

    Character* actor = ResolveExecuteActorForPredicate();
    if (usedContextTargetFallback)
    {
        LogDebugExecuteTargetSourceFromContextMenu(reinterpret_cast<uintptr_t>(target));
    }
    (void)DispatchExecuteFromDebugTrigger(actor, target, true);
}

#include "LootScootExecuteHooksEntry.inl"
