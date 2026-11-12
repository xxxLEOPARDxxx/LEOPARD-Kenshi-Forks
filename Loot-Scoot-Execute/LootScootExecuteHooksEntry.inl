#include <IniDefaults.h>   // LEOPARD: mod-config.json from mod-config.defaults.json
static void RefreshEffectiveContextMenuFeatureFlags(const char* source)
{
    (void)source;

    const bool hooksReady = g_contextMenuHookInstallVerified;
    const bool pluginEnabled = g_config.enabled;
    g_effectiveEnableContextMenuInjection = hooksReady && pluginEnabled;
    g_effectiveEnableExecuteAction = hooksReady && pluginEnabled;
}

static void DisarmNativeMenuExecuteDispatchContext()
{
    g_nativeMenuExecuteDispatchArmed = false;
    g_nativeMenuExecuteDispatchTargetPtr = 0;
    g_nativeMenuExecuteDispatchArmMs = 0;
}

static void ResetNativeMenuExecuteRowInsertState()
{
    g_nativeMenuExecuteRowInjectedOrdersPtr = 0;
    g_nativeMenuExecuteRowInjectedArmMs = 0;
}

static void DisarmNativeMenuOrderRemapContext()
{
    g_nativeMenuOrderRemapArmed = false;
    g_nativeMenuOrderRemapOrdersPtr = 0;
    g_nativeMenuOrderRemapTargetPtr = 0;
    g_nativeMenuOrderRemapArmMs = 0;

    g_nativeMenuBuildRowsInjectedOrdersPtr = 0;
    g_nativeMenuBuildRowsInjectedShowSeq = 0;
    g_nativeMenuRowDescriptorTemplate = 0;
    g_nativeMenuRowDescriptorTemplateOrdersPtr = 0;
    g_nativeMenuRowDescriptorTemplateArmMs = 0;
    g_nativeMenuLoopEntryInjectedOrdersPtr = 0;
    g_nativeMenuLoopEntryInjectedArmMs = 0;
    g_nativeMenuLoopEntryInjectionInProgress = false;
    g_nativeMenuAppendInjectionInProgress = false;
    ResetNativeMenuExecuteRowInsertState();
}

static bool TryReadSimpleContextMenuSnapshot(
    ContextMenu* menu,
    bool* visibleOut,
    uint32_t* ordersCountOut)
{
    if (!menu || !visibleOut || !ordersCountOut)
    {
        return false;
    }

    *visibleOut = false;
    *ordersCountOut = 0;

    __try
    {
        *visibleOut = menu->isVisible();
        *ordersCountOut = static_cast<uint32_t>(menu->orders.size());
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void ContextMenu_showContextMenu_hook(ContextMenu* thisptr, bool on, RootObject* what)
{
    const CustomExecutePanelAction hoveredAction = GetHoveredCustomExecutePanelAction();
    if (!on
        && IsCustomExecutePanelOverlayEnabled()
        && g_customExecutePanelVisible
        && hoveredAction != CustomExecutePanelAction_NONE)
    {
        const bool rightDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        if (!rightDown)
        {
            (void)DispatchCustomExecutePanelAction(hoveredAction);
        }
    }

    if (ContextMenu_showContextMenu_orig)
    {
        ContextMenu_showContextMenu_orig(thisptr, on, what);
    }

    const DWORD nowMs = GetTickCount();
    if (!on)
    {
        g_currentShowSeq = 0;
        g_lastShowSeq = 0;
        g_lastShowTimeMs = 0;
        g_lastMenuPtr = 0;
        g_lastWasDownedEnemy = false;
        g_lastShowTargetIsEnemy = false;
        g_lastShowTargetIsIncapacitated = false;
        g_lastShowTargetIsDead = false;
        HideCustomExecutePanelOverlay();
        DisarmNativeMenuExecuteDispatchContext();
        DisarmNativeMenuOrderRemapContext();
    }

    if (!thisptr)
    {
        HideCustomExecutePanelOverlay();
        return;
    }

    bool visible = false;
    uint32_t ordersCount = 0;
    if (!TryReadSimpleContextMenuSnapshot(
            thisptr,
            &visible,
            &ordersCount))
    {
        HideCustomExecutePanelOverlay();
        return;
    }

    Character* executeActor = ResolveExecuteActorForPredicateWithTarget(what, true);
    CanExecuteDiagnostics diagnostics = MakeCanExecuteDiagnostics();
    const bool canExecuteTarget = CanExecuteFromNativeMenuSelection(
        executeActor,
        what,
        &diagnostics,
        ShouldLogExecuteDebug());

    const uintptr_t whatPtr = reinterpret_cast<uintptr_t>(what);
    if (on)
    {
        ++g_currentShowSeq;
        g_lastShowSeq = g_currentShowSeq;
        g_lastShowTimeMs = nowMs;
        g_lastMenuPtr = reinterpret_cast<uintptr_t>(thisptr);
        g_lastWasDownedEnemy = canExecuteTarget;
        g_lastShowTargetIsEnemy = diagnostics.targetIsEnemy;
        g_lastShowTargetIsIncapacitated = diagnostics.targetIsIncapacitated;
        g_lastShowTargetIsDead = diagnostics.targetIsDead;
    }

    if (IsCustomExecutePanelOverlayEnabled() && on && visible && canExecuteTarget && whatPtr != 0)
    {
        // LEOPARD: сначала - пунктами самого меню, как HarvestOrgans
        if (TryAddNativeExecuteOptions(thisptr, what, executeActor))
        {
            HideCustomExecutePanelOverlay();
        }
        else
        {
            ArmCustomExecutePanelOverlay(thisptr, what, ordersCount, g_currentShowSeq, nowMs);
        }
    }
    else
    {
        HideCustomExecutePanelOverlay();
    }
}

static void ContextMenu_update_hook(ContextMenu* thisptr)
{
    const DWORD nowMs = GetTickCount();
    if (thisptr)
    {
        g_customExecutePanelLastContextMenuTickMs = nowMs;
        TickCustomExecutePanelOverlay(thisptr, nowMs);
    }
    else
    {
        HideCustomExecutePanelOverlay();
    }

    if (ContextMenu_update_orig)
    {
        ContextMenu_update_orig(thisptr);
    }
}

static void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    PlayerInterface_updateUT_orig(thisptr);
    const DWORD nowMs = GetTickCount();
    if (g_customExecutePanelMenuPtr != 0)
    {
        const bool contextMenuTickRecent = g_customExecutePanelLastContextMenuTickMs != 0
            && !DebounceWindowElapsed(
                nowMs,
                g_customExecutePanelLastContextMenuTickMs,
                kCustomExecutePanelFallbackTickMinGapMs);
        if (!contextMenuTickRecent)
        {
            TickCustomExecutePanelOverlay(
                reinterpret_cast<ContextMenu*>(g_customExecutePanelMenuPtr),
                nowMs);
        }
    }
    TickQueuedExecuteAction(thisptr);
}

// Приказы игрока выделенным - снимают их добивание (CancelExecuteForSelectedCharacters).
static void (*PlayerInterface_newPlayerTask_orig)(PlayerInterface*, TaskType, const hand&, Building*, const Ogre::Vector3&, bool) = 0;
static void (*PlayerInterface_moveWaypoint_orig)(PlayerInterface*, const Ogre::Vector3&, Building*) = 0;

static void PlayerInterface_newPlayerTask_hook(PlayerInterface* thisptr, TaskType t, const hand& target,
                                               Building* indoors, const Ogre::Vector3& clickpos, bool addDontClear)
{
    if (static_cast<int>(t) != kContextMenuOrderIdExecuteProxy)
    {
        CancelExecuteForSelectedCharacters("player_order");
    }
    PlayerInterface_newPlayerTask_orig(thisptr, t, target, indoors, clickpos, addDontClear);
}

static void PlayerInterface_moveWaypoint_hook(PlayerInterface* thisptr, const Ogre::Vector3& location, Building* dest)
{
    CancelExecuteForSelectedCharacters("player_move");
    PlayerInterface_moveWaypoint_orig(thisptr, location, dest);
}

typedef std::map<float, Tasker*, std::less<float>,
    Ogre::STLAllocator<std::pair<const float, Tasker*>, Ogre::GeneralAllocPolicy> > ExecuteGoalList;

static void (*choosePermaJob_orig)(AITaskSytem*, ExecuteGoalList&, TaskMatch&, bool, bool) = 0;

static void choosePermaJob_hook(AITaskSytem* tasks, ExecuteGoalList& goals,
                                TaskMatch& current, bool urgent, bool jobsEnabled)
{
    if (!ExecuteSuppressesPermanentWork(tasks))
        choosePermaJob_orig(tasks, goals, current, urgent, jobsEnabled);
}

static void (*clearWorld_orig)(GameWorld*) = 0;

static void clearWorld_hook(GameWorld* world)
{
    InterlockedExchange64(&g_executeWorkMask, 0);
    InterlockedExchange64(&g_finisherWeaponHoldMask, 0);
    InterlockedIncrement(&g_executeWorldGeneration);
    clearWorld_orig(world);
}

static bool InstallExecutePermanentWorkHook()
{
    const HMODULE lib = GetModuleHandleW(L"KenshiLib.dll");
    if (!lib)
        return false;

    // Resolve the SDK export directly: the C++ header exposes this member
    // as protected, and GetRealAddress expects the SDK's own function stub.
    const FARPROC stub = GetProcAddress(lib,
        "?choosePermaJob@AITaskSytem@@QEAAXAEAV?$map@MPEAVTasker@@U?$less@M@std@@V?$STLAllocator@U?$pair@$$CBMPEAVTasker@@@std@@V?$CategorisedAllocPolicy@$0A@@Ogre@@@Ogre@@@std@@AEAVTaskMatch@@_N2@Z");
    if (!stub)
        return false;
    const intptr_t address = KenshiLib::GetRealAddress(reinterpret_cast<void*>(stub));
    return address && KenshiLib::SUCCESS == KenshiLib::AddHook(
        address, choosePermaJob_hook, &choosePermaJob_orig);
}

static void (*executeAutoSheathe_orig)(Tasker*, CharBody*) = 0;

static void executeAutoSheathe_hook(Tasker* task, CharBody* body)
{
    if (!KeepWeaponForFinisher(body))
        executeAutoSheathe_orig(task, body);
}

static bool InstallFinisherWeaponHoldHook(unsigned int platform)
{
    // Task_SheatheWeapon::runAction is absent from the SDK. Verify its
    // complete body in the stock and RE-patched executables before hooking.
    static const unsigned char expected[] = {
        0x48, 0x83, 0xec, 0x28, 0x48, 0x8b, 0x02, 0x48, 0x8b, 0xca,
        0xff, 0x50, 0x48, 0x48, 0x8b, 0xc8, 0x48, 0x8b, 0x10, 0x48,
        0x83, 0xc4, 0x28, 0x48, 0xff, 0xa2, 0xd0, 0x02, 0x00, 0x00
    };
    static const size_t steamRvas[] = {0x336bc0, 0x336c00};
    static const size_t gogRvas[] = {0x336750, 0x3365a0};
    const size_t* candidates = platform == KenshiLib::BinaryVersion::STEAM ? steamRvas : gogRvas;
    const unsigned char* image = reinterpret_cast<const unsigned char*>(GetModuleHandleW(0));
    if (!image)
        return false;

    __try
    {
        for (size_t i = 0; i < 2; ++i)
        {
            const unsigned char* target = image + candidates[i];
            if (std::memcmp(target, expected, sizeof(expected)) == 0)
            {
                return KenshiLib::SUCCESS == KenshiLib::AddHook(
                    reinterpret_cast<intptr_t>(target), executeAutoSheathe_hook, &executeAutoSheathe_orig);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    return false;
}

__declspec(dllexport) void startPlugin()
{
    PluginLog("Loot-Scoot-Execute: startPlugin()");

    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();
    if (platform == KenshiLib::BinaryVersion::UNKNOWN || version != "1.0.65")
    {
        ErrorLog("Loot-Scoot-Execute: unsupported Kenshi version/platform (requires 1.0.65)");
        return;
    }

    g_runtimeGameVersion = version;
    g_runtimeLocaleTag = DetectRuntimeLocaleTag();

    LoadConfigState();
    if (g_configNeedsWriteBack && !SaveConfigState())
    {
        ErrorLog("Loot-Scoot-Execute WARN: failed to persist normalized mod-config.json");
    }

    g_contextMenuHookInstallVerified = false;

    DisarmNativeMenuExecuteDispatchContext();
    DisarmNativeMenuOrderRemapContext();
    HideCustomExecutePanelOverlay();
    RefreshEffectiveContextMenuFeatureFlags("startup_pre_hooks");

    if (!RunInternalSelfChecks())
    {
        ErrorLog("Loot-Scoot-Execute ERROR: internal self-check failed");
        return;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        &PlayerInterface_updateUT_orig))
    {
        ErrorLog("Loot-Scoot-Execute: Could not hook PlayerInterface::updateUT");
        return;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::newPlayerTaskSelectedCharacters),
        PlayerInterface_newPlayerTask_hook,
        &PlayerInterface_newPlayerTask_orig))
    {
        ErrorLog("Loot-Scoot-Execute WARN: could not hook newPlayerTaskSelectedCharacters - orders will not cancel executes");
    }
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateLastMoveWaypointSelectedCharacters),
        PlayerInterface_moveWaypoint_hook,
        &PlayerInterface_moveWaypoint_orig))
    {
        ErrorLog("Loot-Scoot-Execute WARN: could not hook move orders - a ground click will not cancel executes");
    }

    const bool worldHookInstalled = KenshiLib::SUCCESS == KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&GameWorld::_clearAndDestroyGameWorldStuff),
        clearWorld_hook, &clearWorld_orig);
    g_executeWorkHookInstalled = worldHookInstalled && InstallExecutePermanentWorkHook();
    if (!g_executeWorkHookInstalled)
        ErrorLog("Loot-Scoot-Execute: permanent work coordination hook unavailable");

    g_finisherWeaponHoldInstalled = worldHookInstalled && InstallFinisherWeaponHoldHook(platform);
    if (!g_finisherWeaponHoldInstalled)
        ErrorLog("Loot-Scoot-Execute: automatic weapon sheathe hook unavailable");

    bool contextMenuShowHookInstalled = false;
    bool contextMenuUpdateHookInstalled = false;
    if (KenshiLib::SUCCESS == KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&ContextMenu::showContextMenu),
        ContextMenu_showContextMenu_hook,
        &ContextMenu_showContextMenu_orig))
    {
        contextMenuShowHookInstalled = true;
        PluginLog("Loot-Scoot-Execute INFO: ContextMenu::showContextMenu hook verification passed");
    }
    else
    {
        ErrorLog("Loot-Scoot-Execute WARN: could not hook ContextMenu::showContextMenu; context-menu features disabled");
    }

    if (KenshiLib::SUCCESS == KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&ContextMenu::update),
        ContextMenu_update_hook,
        &ContextMenu_update_orig))
    {
        contextMenuUpdateHookInstalled = true;
        PluginLog("Loot-Scoot-Execute INFO: ContextMenu::update hook verification passed");
    }
    else
    {
        ErrorLog("Loot-Scoot-Execute WARN: could not hook ContextMenu::update; context-menu features disabled");
    }

    g_contextMenuHookInstallVerified = contextMenuShowHookInstalled && contextMenuUpdateHookInstalled;
    RefreshEffectiveContextMenuFeatureFlags("post_context_menu_hooks");

    ModHub_OnPluginStart();

    std::stringstream info;
    info << "Loot-Scoot-Execute INFO: initialized (enabled=" << (g_config.enabled ? "true" : "false")
         << ", runtime_mapping_key=" << g_runtimeGameVersion << "|" << g_runtimeLocaleTag << "|downed_enemy"
         << ", native_context_menu_integration=removed"
         << ", enable_execute_kill_sound=" << (g_config.enableExecuteKillSound ? "true" : "false")
         << ", debug_execute_logging=" << (g_config.debugExecuteLogging ? "true" : "false")
         << ", enable_execute_all=" << (g_config.enableExecuteAll ? "true" : "false")
         << ", execute_all_radius_units=" << g_config.executeAllRadiusUnits
         << ", execute_button_width=" << g_config.executeButtonWidthPx
         << ", execute_button_height=" << g_config.executeButtonHeightPx
         << ", execute_button_gap=" << g_config.executeButtonGapPx
         << ", execute_button_x=" << g_config.executeButtonOffsetXPx
         << ", execute_button_y=" << g_config.executeButtonOffsetYPx
         << ", effective_context_menu_injection=" << (g_effectiveEnableContextMenuInjection ? "true" : "false")
         << ", effective_execute_action=" << (g_effectiveEnableExecuteAction ? "true" : "false")
         << ", hook_verification=" << (g_contextMenuHookInstallVerified ? "passed" : "failed")
         << ", mod_hub_use_ui=" << (ModHub_UseHubUi() ? "true" : "false")
         << ", mod_hub_retry_pending=" << (ModHub_IsAttachRetryPending() ? "true" : "false")
         << ", mod_hub_last_result=" << ModHub_LastAttachFailureResult()
         << ")";
    PluginLog(info.str().c_str());
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
                g_settingsPath = IniDefaults::PrepareJson(myDirectory + "\\" + kConfigFileName);
            }
        }
    }
    return TRUE;
}
