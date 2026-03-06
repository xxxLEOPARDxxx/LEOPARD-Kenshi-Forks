static void RefreshEffectiveContextMenuFeatureFlags(const char* source)
{
    (void)source;

    const bool hooksReady = g_contextMenuHookInstallVerified;
    const bool pluginEnabled = g_config.enabled;
    g_effectiveEnableContextMenuProbe = hooksReady && pluginEnabled && g_config.enableContextMenuProbe;
    g_effectiveEnableContextMenuInjection = hooksReady && pluginEnabled;
    g_effectiveEnableExecuteAction = hooksReady && pluginEnabled;

    if (g_config.debugContextMenu)
    {
        std::stringstream detail;
        detail << "Loot-Scoot-Execute DEBUG: context_menu_feature_flags source=" << (source ? source : "unknown")
               << " native_integration_enabled=false"
               << " hooks_verified=" << (g_contextMenuHookInstallVerified ? "true" : "false")
               << " cfg_enabled=" << (g_config.enabled ? "true" : "false")
               << " cfg_probe=" << (g_config.enableContextMenuProbe ? "true" : "false")
               << " effective_probe=" << (g_effectiveEnableContextMenuProbe ? "true" : "false")
               << " effective_injection=" << (g_effectiveEnableContextMenuInjection ? "true" : "false")
               << " effective_execute=" << (g_effectiveEnableExecuteAction ? "true" : "false");
        PluginLog(detail.str().c_str());
    }
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
    uint32_t* ordersCountOut,
    int* orderSampleOut,
    size_t* orderSampleCountOut)
{
    if (!menu || !visibleOut || !ordersCountOut || !orderSampleOut || !orderSampleCountOut)
    {
        return false;
    }

    *visibleOut = false;
    *ordersCountOut = 0;
    *orderSampleCountOut = 0;
    for (size_t i = 0; i < kContextMenuProbeOrderSampleCount; ++i)
    {
        orderSampleOut[i] = 0;
    }

    __try
    {
        *visibleOut = menu->isVisible();
        const uint32_t count = static_cast<uint32_t>(menu->orders.size());
        *ordersCountOut = count;

        const size_t sampleCount = count < kContextMenuProbeOrderSampleCount
            ? static_cast<size_t>(count)
            : kContextMenuProbeOrderSampleCount;
        *orderSampleCountOut = sampleCount;

        for (size_t i = 0; i < sampleCount; ++i)
        {
            orderSampleOut[i] = static_cast<int>(menu->orders[static_cast<uint32_t>(i)]);
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void ContextMenu_showContextMenu_hook(ContextMenu* thisptr, bool on, RootObject* what)
{
    if (!on
        && IsCustomExecutePanelOverlayEnabled()
        && g_customExecutePanelVisible
        && IsCustomExecutePanelButtonHovered())
    {
        const bool rightDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        if (!rightDown)
        {
            (void)DispatchCustomExecutePanelAction("show_close_right_release_hover");
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
        g_lastDebugExecuteContextTargetPtr = 0;
        g_lastDebugExecuteContextTargetCaptureMs = 0;
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
    int orderSample[kContextMenuProbeOrderSampleCount] = { 0 };
    size_t orderSampleCount = 0;
    if (!TryReadSimpleContextMenuSnapshot(
        thisptr,
        &visible,
        &ordersCount,
        orderSample,
        &orderSampleCount))
    {
        HideCustomExecutePanelOverlay();
        return;
    }

    Character* executeActor = ResolveExecuteActorForPredicateWithTarget(what, true);
    CanExecuteDiagnostics diagnostics = { false, false, false, false, false, false, false, false, false, NULL_ITEM, 0, 0 };
    const bool canExecuteTarget = CanExecuteFromNativeMenuSelection(
        executeActor,
        what,
        &diagnostics,
        g_config.debugContextMenu);

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
        ArmCustomExecutePanelOverlay(thisptr, what, ordersCount, g_currentShowSeq, nowMs);
    }
    else
    {
        HideCustomExecutePanelOverlay();
    }

    if (on && visible && canExecuteTarget && whatPtr != 0)
    {
        g_lastDebugExecuteContextTargetPtr = whatPtr;
        g_lastDebugExecuteContextTargetCaptureMs = nowMs;
    }

    if (g_effectiveEnableContextMenuProbe && g_config.debugContextMenu)
    {
        std::stringstream probe;
        probe << "Loot-Scoot-Execute DEBUG: context_menu_show_probe"
              << " show_seq=" << std::dec << g_currentShowSeq
              << " on=" << (on ? "true" : "false")
              << " visible=" << (visible ? "true" : "false")
              << " orders_count=" << std::dec << ordersCount
              << " first_orders=[";

        for (size_t i = 0; i < orderSampleCount; ++i)
        {
            if (i > 0)
            {
                probe << ",";
            }
            probe << std::dec << orderSample[i];
        }
        if (static_cast<size_t>(ordersCount) > orderSampleCount)
        {
            if (orderSampleCount > 0)
            {
                probe << ",";
            }
            probe << "...";
        }

        probe << "]"
              << " what=0x" << std::hex << whatPtr
              << " can_execute_target=" << (canExecuteTarget ? "true" : "false")
              << " actor_resolved=" << (diagnostics.actorResolved ? "true" : "false")
              << " target_is_enemy=" << (diagnostics.targetIsEnemy ? "true" : "false")
              << " target_is_incapacitated=" << (diagnostics.targetIsIncapacitated ? "true" : "false")
              << " target_is_dead=" << (diagnostics.targetIsDead ? "true" : "false");
        PluginLog(probe.str().c_str());
    }
}

static void ContextMenu_update_hook(ContextMenu* thisptr)
{
    if (thisptr)
    {
        TickCustomExecutePanelOverlay(thisptr, GetTickCount());
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
    TickDebugExecuteHotkey(thisptr);
    TickQueuedExecuteAction(thisptr);
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
    g_nativeExecuteSelectionHookInstallVerified = false;
    g_nativeExecuteProbabilityHookInstallVerified = false;
    g_nativeExecuteOrderFilterHookInstallVerified = false;
    g_nativeExecuteContextMenuProbabilityHookInstallVerified = false;
    g_nativeExecuteOrderValidityHookInstallVerified = false;
    g_nativeExecuteOrderAppendHookInstallVerified = false;
    g_nativeExecuteTaskLabelHookInstallVerified = false;
    g_nativeExecuteMenuBuildHookInstallVerified = false;
    g_nativeExecuteRowInsertHookInstallVerified = false;
    g_nativeExecuteLoopEntryHookInstallVerified = false;
    g_nativeExecuteOrderFilterAlternateHookInstallVerified = false;
    g_nativeExecuteContextMenuProbabilityAlternateHookInstallVerified = false;
    g_nativeExecuteOrderAppendAlternateHookInstallVerified = false;
    g_nativeExecuteTaskLabelAlternateHookInstallVerified = false;

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
         << ", execute_button_width=" << g_config.executeButtonWidthPx
         << ", execute_button_height=" << g_config.executeButtonHeightPx
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
                g_settingsPath = myDirectory + "\\" + kConfigFileName;
            }
        }
    }
    return TRUE;
}
