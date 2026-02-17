static bool IsExecutableProtectFlags(DWORD protectFlags)
{
    if ((protectFlags & PAGE_GUARD) != 0 || (protectFlags & PAGE_NOACCESS) != 0)
    {
        return false;
    }

    const DWORD baseProtect = protectFlags & 0xFF;
    return baseProtect == PAGE_EXECUTE
        || baseProtect == PAGE_EXECUTE_READ
        || baseProtect == PAGE_EXECUTE_READWRITE
        || baseProtect == PAGE_EXECUTE_WRITECOPY;
}

static bool IsExecutableCodeAddress(uintptr_t address)
{
    if (!address)
    {
        return false;
    }

    MEMORY_BASIC_INFORMATION mbi;
    std::memset(&mbi, 0, sizeof(mbi));
    if (!VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)))
    {
        return false;
    }

    if (mbi.State != MEM_COMMIT)
    {
        return false;
    }

    return IsExecutableProtectFlags(mbi.Protect);
}

static bool TryReadCodeBytes(uintptr_t address, unsigned char* bytesOut, size_t byteCount)
{
    if (!address || !bytesOut || byteCount == 0)
    {
        return false;
    }

    __try
    {
        const unsigned char* ptr = reinterpret_cast<const unsigned char*>(address);
        for (size_t i = 0; i < byteCount; ++i)
        {
            bytesOut[i] = ptr[i];
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static std::string FormatCodeBytes(uintptr_t address, size_t byteCount)
{
    if (byteCount == 0)
    {
        return std::string();
    }

    unsigned char bytes[8] = { 0 };
    if (byteCount > sizeof(bytes))
    {
        byteCount = sizeof(bytes);
    }

    if (!TryReadCodeBytes(address, bytes, byteCount))
    {
        return "unreadable";
    }

    std::stringstream ss;
    for (size_t i = 0; i < byteCount; ++i)
    {
        if (i > 0)
        {
            ss << " ";
        }
        const int value = static_cast<int>(bytes[i]);
        if (value < 16)
        {
            ss << '0';
        }
        ss << std::hex << std::uppercase << value;
    }
    return ss.str();
}

static bool ValidateExpectedRvaForSymbol(
    const char* symbolName,
    uintptr_t baseAddr,
    uintptr_t resolvedAddr,
    uintptr_t expectedRva,
    intptr_t* rvaDeltaOut,
    bool* rvaMatchOut)
{
    if (!symbolName || !baseAddr || !resolvedAddr)
    {
        return false;
    }

    const uintptr_t expectedAddr = baseAddr + expectedRva;
    const bool rvaMatch = (resolvedAddr == expectedAddr);
    const intptr_t rvaDelta = static_cast<intptr_t>(resolvedAddr) - static_cast<intptr_t>(expectedAddr);
    if (rvaDeltaOut)
    {
        *rvaDeltaOut = rvaDelta;
    }
    if (rvaMatchOut)
    {
        *rvaMatchOut = rvaMatch;
    }
    const bool executable = IsExecutableCodeAddress(resolvedAddr);
    unsigned char firstByte = 0;
    const bool readableFirstByte = TryReadCodeBytes(resolvedAddr, &firstByte, 1);
    const bool signatureLooksReasonable = readableFirstByte && firstByte != 0x00 && firstByte != 0xCC;

    std::stringstream detail;
    detail << "Loot-Scoot-Execute DEBUG: compatibility_check symbol=" << symbolName
           << " resolved=0x" << std::hex << resolvedAddr
           << " expected=0x" << expectedAddr
           << " rva_match=" << (rvaMatch ? "true" : "false")
           << " rva_delta=0x" << std::hex << rvaDelta
           << " executable=" << (executable ? "true" : "false")
           << " signature_ok=" << (signatureLooksReasonable ? "true" : "false")
           << " first_bytes=\"" << FormatCodeBytes(resolvedAddr, 6) << "\"";
    DebugLog(detail.str().c_str());

    if (!executable)
    {
        std::stringstream warn;
        warn << "Loot-Scoot-Execute WARN: compatibility_check_failed symbol=" << symbolName
             << " reason=non_executable_address resolved=0x" << std::hex << resolvedAddr;
        ErrorLog(warn.str().c_str());
    }
    else if (!signatureLooksReasonable)
    {
        std::stringstream warn;
        warn << "Loot-Scoot-Execute WARN: compatibility_check_failed symbol=" << symbolName
             << " reason=unexpected_signature resolved=0x" << std::hex << resolvedAddr
             << " first_bytes=\"" << FormatCodeBytes(resolvedAddr, 6) << "\"";
        ErrorLog(warn.str().c_str());
    }

    return executable && signatureLooksReasonable;
}

static bool EvaluateContextMenuCompatibilityGate(unsigned int platform, const std::string& version, uintptr_t baseAddr)
{
    g_contextMenuCompatibilityGatePassed = false;
    g_contextMenuGateFailureReason.clear();
    g_resolvedContextMenuShowAddress = 0;
    g_resolvedContextMenuUpdateAddress = 0;
    g_resolvedPlayerInterfaceUpdateUTAddress = 0;

    std::stringstream begin;
    begin << "Loot-Scoot-Execute DEBUG: compatibility_gate_begin platform=" << platform
          << " version=" << version
          << " base=0x" << std::hex << baseAddr;
    DebugLog(begin.str().c_str());

    if (platform == KenshiLib::BinaryVersion::UNKNOWN)
    {
        g_contextMenuGateFailureReason = "unknown_platform";
        ErrorLog("Loot-Scoot-Execute WARN: compatibility gate failed: unknown platform");
        return false;
    }

    if (version != "1.0.65")
    {
        g_contextMenuGateFailureReason = "unsupported_version";
        ErrorLog("Loot-Scoot-Execute WARN: compatibility gate failed: version is not 1.0.65");
        return false;
    }

    if (baseAddr == 0)
    {
        g_contextMenuGateFailureReason = "null_module_base";
        ErrorLog("Loot-Scoot-Execute WARN: compatibility gate failed: null module base");
        return false;
    }

    g_resolvedContextMenuShowAddress = static_cast<uintptr_t>(KenshiLib::GetRealAddress(&ContextMenu::showContextMenu));
    g_resolvedContextMenuUpdateAddress = static_cast<uintptr_t>(KenshiLib::GetRealAddress(&ContextMenu::update));
    g_resolvedPlayerInterfaceUpdateUTAddress = static_cast<uintptr_t>(KenshiLib::GetRealAddress(&PlayerInterface::updateUT));

    intptr_t showDelta = 0;
    intptr_t updateDelta = 0;
    intptr_t updateUTDelta = 0;
    bool showRvaMatch = false;
    bool updateRvaMatch = false;
    bool updateUTRvaMatch = false;

    const bool showValid = ValidateExpectedRvaForSymbol(
        "ContextMenu::showContextMenu",
        baseAddr,
        g_resolvedContextMenuShowAddress,
        kExpectedRvaContextMenuShow_1_0_65,
        &showDelta,
        &showRvaMatch);
    const bool updateValid = ValidateExpectedRvaForSymbol(
        "ContextMenu::update",
        baseAddr,
        g_resolvedContextMenuUpdateAddress,
        kExpectedRvaContextMenuUpdate_1_0_65,
        &updateDelta,
        &updateRvaMatch);
    const bool updateUTValid = ValidateExpectedRvaForSymbol(
        "PlayerInterface::updateUT",
        baseAddr,
        g_resolvedPlayerInterfaceUpdateUTAddress,
        kExpectedRvaPlayerInterfaceUpdateUT_1_0_65,
        &updateUTDelta,
        &updateUTRvaMatch);

    if (!showValid || !updateValid || !updateUTValid)
    {
        g_contextMenuCompatibilityGatePassed = false;
        g_contextMenuGateFailureReason = "rva_or_signature_validation_failed";
        ErrorLog("Loot-Scoot-Execute WARN: compatibility gate failed: one or more symbol checks failed");
        return false;
    }

    const bool exactRvaMatch = showRvaMatch && updateRvaMatch && updateUTRvaMatch;
    const bool consistentDelta = !exactRvaMatch
        && showDelta == updateDelta
        && showDelta == updateUTDelta;

    g_contextMenuCompatibilityGatePassed = exactRvaMatch || consistentDelta;
    if (!g_contextMenuCompatibilityGatePassed)
    {
        g_contextMenuGateFailureReason = "inconsistent_rva_delta";
        std::stringstream warn;
        warn << "Loot-Scoot-Execute WARN: compatibility gate failed: inconsistent RVA deltas"
             << " show=0x" << std::hex << showDelta
             << " update=0x" << updateDelta
             << " updateUT=0x" << updateUTDelta;
        ErrorLog(warn.str().c_str());
        return false;
    }

    if (consistentDelta)
    {
        std::stringstream shifted;
        shifted << "Loot-Scoot-Execute INFO: compatibility gate accepted consistent RVA shift"
                << " delta=0x" << std::hex << showDelta;
        DebugLog(shifted.str().c_str());
    }

    DebugLog("Loot-Scoot-Execute INFO: compatibility gate passed for native context-menu work");
    return true;
}

static void RefreshEffectiveContextMenuFeatureFlags(const char* source)
{
    const bool gateAllowsFeatures = g_contextMenuCompatibilityGatePassed && g_contextMenuHookInstallVerified;
    g_effectiveEnableContextMenuProbe = gateAllowsFeatures && g_config.enableContextMenuProbe;
    g_effectiveEnableContextMenuInjection = gateAllowsFeatures && g_config.enableContextMenuInjection;
    g_effectiveEnableExecuteAction = gateAllowsFeatures && g_config.enableExecuteAction;

    const bool anyConfiguredOn = g_config.enableContextMenuProbe || g_config.enableContextMenuInjection || g_config.enableExecuteAction;
    if (anyConfiguredOn && !gateAllowsFeatures)
    {
        std::stringstream warn;
        warn << "Loot-Scoot-Execute WARN: fail_closed_context_menu_features source="
             << (source ? source : "unknown")
             << " reason="
             << (g_contextMenuCompatibilityGatePassed ? "required_hooks_not_ready" : g_contextMenuGateFailureReason)
             << " probe=false injection=false execute=false";
        ErrorLog(warn.str().c_str());
    }

    std::stringstream detail;
    detail << "Loot-Scoot-Execute DEBUG: context_menu_feature_flags source=" << (source ? source : "unknown")
           << " gate_passed=" << (g_contextMenuCompatibilityGatePassed ? "true" : "false")
           << " hooks_verified=" << (g_contextMenuHookInstallVerified ? "true" : "false")
           << " cfg_probe=" << (g_config.enableContextMenuProbe ? "true" : "false")
           << " cfg_injection=" << (g_config.enableContextMenuInjection ? "true" : "false")
           << " cfg_execute=" << (g_config.enableExecuteAction ? "true" : "false")
           << " effective_probe=" << (g_effectiveEnableContextMenuProbe ? "true" : "false")
           << " effective_injection=" << (g_effectiveEnableContextMenuInjection ? "true" : "false")
           << " effective_execute=" << (g_effectiveEnableExecuteAction ? "true" : "false");
    DebugLog(detail.str().c_str());
}

static bool TryResolveRootObjectType(RootObject* object, int* dataTypeOut)
{
    if (!object || !dataTypeOut)
    {
        return false;
    }

    __try
    {
        *dataTypeOut = static_cast<int>(object->getDataType());
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryReadContextMenuProbeSnapshot(
    ContextMenu* menu,
    bool* visibleOut,
    std::string* menuNameOut,
    uint32_t* ordersCountOut,
    int* orderSampleOut,
    size_t* orderSampleCountOut)
{
    if (!menu || !visibleOut || !menuNameOut || !ordersCountOut || !orderSampleOut || !orderSampleCountOut)
    {
        return false;
    }

    __try
    {
        *visibleOut = menu->isVisible();
        *menuNameOut = menu->contextMenuName;

        const uint32_t orderCount = menu->orders.size();
        *ordersCountOut = orderCount;

        size_t sampleCount = static_cast<size_t>(orderCount);
        if (sampleCount > kContextMenuProbeOrderSampleCount)
        {
            sampleCount = kContextMenuProbeOrderSampleCount;
        }

        for (size_t i = 0; i < sampleCount; ++i)
        {
            orderSampleOut[i] = menu->orders[static_cast<uint32_t>(i)];
        }

        *orderSampleCountOut = sampleCount;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryReadMouseRightTargetForContextMenu(
    ContextMenu* menu,
    bool* targetSetOut,
    RootObject** targetOut,
    bool* ownerMatchedOut)
{
    if (!targetSetOut || !targetOut || !ownerMatchedOut)
    {
        return false;
    }

    *targetSetOut = false;
    *targetOut = 0;
    *ownerMatchedOut = false;

    if (!ou)
    {
        return false;
    }

    PlayerInterface* player = 0;
    __try
    {
        player = ou->player;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    if (!player)
    {
        return false;
    }

    __try
    {
        *ownerMatchedOut = (&player->contextMenu == menu);
        *targetSetOut = player->mouseRightTargetSet;
        if (*targetSetOut)
        {
            *targetOut = player->mouseRightTarget;
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool ContextMenuOrderSampleChanged(const int* orderSample, size_t orderSampleCount)
{
    if (!orderSample && orderSampleCount > 0)
    {
        return false;
    }

    if (!g_hasContextMenuProbeSnapshot)
    {
        return true;
    }

    if (g_lastContextMenuProbeSampleCount != orderSampleCount)
    {
        return true;
    }

    for (size_t i = 0; i < orderSampleCount; ++i)
    {
        if (g_lastContextMenuProbeOrderSample[i] != orderSample[i])
        {
            return true;
        }
    }

    return false;
}

static void UpdateContextMenuProbeSnapshot(
    bool on,
    bool visible,
    uintptr_t whatPtr,
    uintptr_t mouseRightTargetPtr,
    uint32_t ordersCount,
    const std::string& menuName,
    const int* orderSample,
    size_t orderSampleCount,
    DWORD nowMs)
{
    g_hasContextMenuProbeSnapshot = true;
    g_lastContextMenuProbeLogMs = nowMs;
    g_lastContextMenuProbeOn = on;
    g_lastContextMenuProbeVisible = visible;
    g_lastContextMenuProbeWhatPtr = whatPtr;
    g_lastContextMenuProbeMouseRightTargetPtr = mouseRightTargetPtr;
    g_lastContextMenuProbeOrdersCount = ordersCount;
    g_lastContextMenuProbeName = menuName;
    g_lastContextMenuProbeSampleCount = orderSampleCount;

    for (size_t i = 0; i < orderSampleCount; ++i)
    {
        g_lastContextMenuProbeOrderSample[i] = orderSample[i];
    }
}

static void ContextMenu_showContextMenu_hook(ContextMenu* thisptr, bool on, RootObject* what)
{
    if (ContextMenu_showContextMenu_orig)
    {
        ContextMenu_showContextMenu_orig(thisptr, on, what);
    }

    if (!g_effectiveEnableContextMenuProbe || !thisptr)
    {
        return;
    }

    bool visible = false;
    std::string contextMenuName;
    uint32_t ordersCount = 0;
    int orderSample[kContextMenuProbeOrderSampleCount] = { 0 };
    size_t orderSampleCount = 0;
    if (!TryReadContextMenuProbeSnapshot(
        thisptr,
        &visible,
        &contextMenuName,
        &ordersCount,
        orderSample,
        &orderSampleCount))
    {
        if (g_config.debugContextMenu)
        {
            ErrorLog("Loot-Scoot-Execute WARN: context_menu_probe_snapshot_failed");
        }
        return;
    }

    bool mouseRightTargetSet = false;
    bool contextMenuOwnerMatched = false;
    RootObject* mouseRightTarget = 0;
    const bool mouseRightTargetResolved = TryReadMouseRightTargetForContextMenu(
        thisptr,
        &mouseRightTargetSet,
        &mouseRightTarget,
        &contextMenuOwnerMatched);

    int whatType = 0;
    const bool whatTypeResolved = TryResolveRootObjectType(what, &whatType);

    int mouseRightTargetType = 0;
    const bool mouseRightTargetTypeResolved = mouseRightTargetResolved
        && mouseRightTargetSet
        && mouseRightTarget
        && TryResolveRootObjectType(mouseRightTarget, &mouseRightTargetType);

    const uintptr_t whatPtr = reinterpret_cast<uintptr_t>(what);
    const uintptr_t mouseRightTargetPtr = reinterpret_cast<uintptr_t>(mouseRightTarget);

    const bool transitionChanged = !g_hasContextMenuProbeSnapshot
        || g_lastContextMenuProbeOn != on
        || g_lastContextMenuProbeVisible != visible
        || g_lastContextMenuProbeWhatPtr != whatPtr
        || g_lastContextMenuProbeMouseRightTargetPtr != mouseRightTargetPtr
        || g_lastContextMenuProbeOrdersCount != ordersCount
        || g_lastContextMenuProbeName != contextMenuName
        || ContextMenuOrderSampleChanged(orderSample, orderSampleCount);

    const bool importantTransition = !g_hasContextMenuProbeSnapshot
        || g_lastContextMenuProbeOn != on
        || g_lastContextMenuProbeVisible != visible;

    const DWORD nowMs = GetTickCount();
    const bool periodicSnapshot = !g_hasContextMenuProbeSnapshot
        || DebounceWindowElapsed(nowMs, g_lastContextMenuProbeLogMs, kContextMenuProbePeriodicMs);

    if (!transitionChanged && !periodicSnapshot)
    {
        return;
    }

    if (transitionChanged
        && !importantTransition
        && g_hasContextMenuProbeSnapshot
        && !DebounceWindowElapsed(nowMs, g_lastContextMenuProbeLogMs, kContextMenuProbeMinIntervalMs))
    {
        return;
    }

    std::stringstream logline;
    logline << "Loot-Scoot-Execute DEBUG: context_menu_show_probe"
            << " on=" << (on ? "true" : "false")
            << " visible=" << (visible ? "true" : "false")
            << " context_menu_name=\"" << contextMenuName << "\""
            << " orders_count=" << std::dec << ordersCount
            << " first_orders=[";

    for (size_t i = 0; i < orderSampleCount; ++i)
    {
        if (i > 0)
        {
            logline << ",";
        }
        logline << orderSample[i];
    }

    if (static_cast<size_t>(ordersCount) > orderSampleCount)
    {
        if (orderSampleCount > 0)
        {
            logline << ",";
        }
        logline << "...";
    }

    logline << "]"
            << " what=0x" << std::hex << whatPtr
            << " what_type=";
    if (whatTypeResolved)
    {
        logline << std::dec << whatType;
    }
    else
    {
        logline << "unresolved";
    }

    logline << " mouse_right_target_set=";
    if (mouseRightTargetResolved)
    {
        logline << (mouseRightTargetSet ? "true" : "false");
    }
    else
    {
        logline << "unresolved";
    }

    logline << " mouse_right_target=0x" << std::hex << mouseRightTargetPtr
            << " mouse_right_target_type=";
    if (mouseRightTargetTypeResolved)
    {
        logline << std::dec << mouseRightTargetType;
    }
    else
    {
        logline << "unresolved";
    }

    logline << " menu_owner_match=";
    if (mouseRightTargetResolved)
    {
        logline << (contextMenuOwnerMatched ? "true" : "false");
    }
    else
    {
        logline << "unresolved";
    }

    DebugLog(logline.str().c_str());

    UpdateContextMenuProbeSnapshot(
        on,
        visible,
        whatPtr,
        mouseRightTargetPtr,
        ordersCount,
        contextMenuName,
        orderSample,
        orderSampleCount,
        nowMs);
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
            DebugLog("Loot-Scoot-Execute DEBUG: tick alive");
            g_state.lastTickAliveLogMs = nowMs;
        }
    }

    if (!g_hasSaveLoadHook || !g_state.pauseArmed)
    {
        return;
    }

    if (g_state.armTimestampMs != 0 && DebounceWindowElapsed(nowMs, g_state.armTimestampMs, kArmedTimeoutMs))
    {
        ErrorLog("Loot-Scoot-Execute WARN: armed pause timed out before load completion");
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
            DebugLog("Loot-Scoot-Execute DEBUG: load started");
        }
        g_state.loadInProgress = true;
        g_state.loadSignalSeenAfterArm = true;
        return;
    }

    if (g_state.loadInProgress)
    {
        if (g_config.debugLogTransitions)
        {
            DebugLog("Loot-Scoot-Execute DEBUG: load finished");
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
            DebugLog("Loot-Scoot-Execute DEBUG: no load signal observed; disarming");
        }
        DisarmPauseAfterLoad();
    }
}

static void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    (void)thisptr;
    PlayerInterface_updateUT_orig(thisptr);
    TickPauseOnLoad();
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

__declspec(dllexport) void startPlugin()
{
    DebugLog("Loot-Scoot-Execute: startPlugin()");

    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    {
        std::stringstream detected;
        detected << "Loot-Scoot-Execute INFO: detected platform=" << platform
                 << " version=" << version;
        DebugLog(detected.str().c_str());
    }

    if (platform == KenshiLib::BinaryVersion::UNKNOWN || version != "1.0.65")
    {
        ErrorLog("Loot-Scoot-Execute: unsupported Kenshi version/platform (requires 1.0.65)");
        return;
    }

    LoadConfigState();
    if (g_configNeedsWriteBack)
    {
        if (!SaveConfigState())
        {
            ErrorLog("Loot-Scoot-Execute WARN: failed to persist normalized mod-config.json");
        }
    }

    const uintptr_t baseAddr = reinterpret_cast<uintptr_t>(GetModuleHandleA(0));
    g_contextMenuHookInstallVerified = false;
    EvaluateContextMenuCompatibilityGate(platform, version, baseAddr);
    RefreshEffectiveContextMenuFeatureFlags("post_compatibility_gate");

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
    g_contextMenuHookInstallVerified = false;
    RefreshEffectiveContextMenuFeatureFlags("post_updateUT_hook");

    bool contextMenuShowHookInstalled = false;
    if (g_contextMenuCompatibilityGatePassed)
    {
        if (KenshiLib::SUCCESS == KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&ContextMenu::showContextMenu),
            ContextMenu_showContextMenu_hook,
            &ContextMenu_showContextMenu_orig))
        {
            contextMenuShowHookInstalled = true;
            DebugLog("Loot-Scoot-Execute INFO: ContextMenu::showContextMenu hook verification passed");
        }
        else
        {
            ErrorLog("Loot-Scoot-Execute WARN: could not hook ContextMenu::showContextMenu; context-menu features fail-closed");
        }
    }
    else
    {
        ErrorLog("Loot-Scoot-Execute WARN: skipping ContextMenu::showContextMenu hook because compatibility gate did not pass");
    }

    g_contextMenuHookInstallVerified = contextMenuShowHookInstalled;
    RefreshEffectiveContextMenuFeatureFlags("post_showContextMenu_hook");

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
        ErrorLog("Loot-Scoot-Execute: Could not hook SaveManager::load(SaveInfo,bool)");
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
        ErrorLog("Loot-Scoot-Execute: Could not hook SaveManager::load(std::string)");
    }

    if (!g_hasSaveLoadHook)
    {
        ErrorLog("Loot-Scoot-Execute: no SaveManager load hooks active; feature disabled");
    }
    else
    {
        DebugLog("Loot-Scoot-Execute INFO: SaveManager load hook verification passed");
    }

    std::stringstream info;
    info << "Loot-Scoot-Execute INFO: initialized (enabled=" << (g_config.enabled ? "true" : "false")
         << ", pause_debounce_ms=" << g_config.pauseDebounceMs
         << ", enable_context_menu_probe=" << (g_config.enableContextMenuProbe ? "true" : "false")
         << ", enable_context_menu_injection=" << (g_config.enableContextMenuInjection ? "true" : "false")
         << ", enable_execute_action=" << (g_config.enableExecuteAction ? "true" : "false")
         << ", debug_context_menu=" << (g_config.debugContextMenu ? "true" : "false")
         << ", effective_context_menu_probe=" << (g_effectiveEnableContextMenuProbe ? "true" : "false")
         << ", effective_context_menu_injection=" << (g_effectiveEnableContextMenuInjection ? "true" : "false")
         << ", effective_execute_action=" << (g_effectiveEnableExecuteAction ? "true" : "false")
         << ", compatibility_gate=" << (g_contextMenuCompatibilityGatePassed ? "passed" : "failed")
         << ", hook_verification=" << (g_contextMenuHookInstallVerified ? "passed" : "failed")
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
