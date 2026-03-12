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

    if (g_config.debugLogTransitions)
    {
        std::stringstream info;
        info << "Job-B-Gone DEBUG: armed_selected_member_ui_refresh source="
             << (source ? source : "unknown");
        DebugLog(info.str().c_str());
    }
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
        if (g_config.debugLogTransitions)
        {
            DebugLog("Job-B-Gone DEBUG: selected_member_ui_refresh timeout");
        }
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
        if (g_config.debugLogTransitions)
        {
            DebugLog("Job-B-Gone DEBUG: selected_member_ui_refresh exhausted retries");
        }
    }
}

static void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    CaptureHudToggleEventSignal();
    PlayerInterface_updateUT_orig(thisptr);
    TickSelectedMemberUiRefresh();
    EnsureSelectedMemberJobPanelButton(thisptr);
}

static void SaveManager_loadByInfo_hook(SaveManager* thisptr, const SaveInfo& saveInfo, bool resetPos)
{
    OnSaveLoadTransitionStart("SaveManager::load(saveInfo,resetPos)");
    if (SaveManager_loadByInfo_orig)
    {
        SaveManager_loadByInfo_orig(thisptr, saveInfo, resetPos);
    }
}

static void SaveManager_loadByName_hook(SaveManager* thisptr, const std::string& saveName)
{
    OnSaveLoadTransitionStart("SaveManager::load(name)");
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
            g_selectedMemberInfoWindowAddress = baseAddr + 0x0212CB70;
            return true;
        }
        if (version == "1.0.68")
        {
            g_selectedMemberInfoWindowAddress = baseAddr + 0x0212CB70;
            return true;
        }
    }
    else if (platform == 0)
    {
        if (version == "1.0.65")
        {
            g_selectedMemberInfoWindowAddress = baseAddr + 0x0212CB70;
            return true;
        }
        if (version == "1.0.68")
        {
            g_selectedMemberInfoWindowAddress = baseAddr + 0x0212CB70;
            return true;
        }
    }

    return false;
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
        ErrorLog("Job-B-Gone WARN: failed to initialize selected-member UI pointers; info-panel detection may be reduced");
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
        ErrorLog("Job-B-Gone: no SaveManager load hooks active; save-load UI reset disabled");
    }

    std::stringstream info;
    info << "Job-B-Gone INFO: initialized (enabled=" << (g_config.enabled ? "true" : "false")
         << ", enable_delete_all_jobs_top_actions="
         << (g_config.enableDeleteAllJobsTopActions ? "true" : "false")
         << ", log_selected_member_job_snapshot="
         << (g_config.logSelectedMemberJobSnapshot ? "true" : "false")
         << ", job_b_gone_panel_collapsed="
         << (g_config.jobBGonePanelCollapsed ? "true" : "false")
         << ", job_b_gone_panel_has_custom_position="
         << (g_config.jobBGonePanelHasCustomPosition ? "true" : "false")
         << ", job_b_gone_panel_pos_x=" << g_config.jobBGonePanelPosX
         << ", job_b_gone_panel_pos_y=" << g_config.jobBGonePanelPosY
         << ", panel_visibility_toggle_hotkey=\"" << g_config.panelVisibilityToggleHotkey << "\""
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
