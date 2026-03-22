const char* kHubNamespaceId = "emkej.qol";
const char* kHubNamespaceDisplayName = "Emkej QoL";
const char* kHubModId = "vital_read";
const char* kHubModDisplayName = "Vital Read";

struct HubBoolSettingDescriptor
{
    const char* settingId;
    const char* label;
    const char* description;
    bool* field;
};

emc::ModHubClient g_modHubClient;
bool g_modHubRowsInitialized = false;

HubBoolSettingDescriptor g_hubBoolSettingDescriptors[] = {
    { "enabled", "Enabled", "Enable Vital Read runtime probes and overlays", &g_enabled },
    { "debug_logging", "Debug logging", "Enable general Vital Read debug logs", &g_debugLogging },
    { "debug_search_logging", "Search debug logging", "Enable portrait search diagnostics (requires Debug logging)", &g_debugSearchLogging },
    { "debug_binding_logging", "Binding debug logging", "Enable portrait binding diagnostics (requires Debug logging)", &g_debugBindingLogging }
};

enum
{
    kHubBoolSettingCount =
        static_cast<int>(sizeof(g_hubBoolSettingDescriptors) / sizeof(g_hubBoolSettingDescriptors[0]))
};

EMC_BoolSettingDefV1 g_hubBoolSettingDefs[kHubBoolSettingCount];
emc::ModHubClientSettingRowV1 g_hubSettingRows[kHubBoolSettingCount];

const EMC_ModDescriptorV1 kModHubDescriptor = {
    kHubNamespaceId,
    kHubNamespaceDisplayName,
    kHubModId,
    kHubModDisplayName,
    &g_modHubClient
};

emc::ModHubClientTableRegistrationV1 g_modHubTableRegistration = {
    &kModHubDescriptor,
    g_hubSettingRows,
    0u
};

EMC_Result __cdecl GetHubBoolSetting(void* user_data, int32_t* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HubBoolSettingDescriptor* descriptor = static_cast<const HubBoolSettingDescriptor*>(user_data);
    if (descriptor->field == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = *descriptor->field ? 1 : 0;
    return EMC_OK;
}

EMC_Result __cdecl SetHubBoolSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    if (user_data == 0)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_setting");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value != 0 && value != 1)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_bool");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    HubBoolSettingDescriptor* descriptor = static_cast<HubBoolSettingDescriptor*>(user_data);
    if (descriptor->field == 0)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_setting");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const bool previousValue = *descriptor->field;
    *descriptor->field = value != 0;
    if (!SaveConfigState())
    {
        *descriptor->field = previousValue;
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "persist_failed");
        return EMC_ERR_INTERNAL;
    }

    if (descriptor->field == &g_enabled && !g_enabled)
    {
        HideHoveredMarker();
        ResetStateOverlays();
    }

    emc::consumer::WriteErrorMessage(err_buf, err_buf_size, 0);
    return EMC_OK;
}

void InitializeModHubSettingRows()
{
    if (g_modHubRowsInitialized)
    {
        return;
    }

    for (size_t index = 0u; index < static_cast<size_t>(kHubBoolSettingCount); ++index)
    {
        g_hubBoolSettingDefs[index].setting_id = g_hubBoolSettingDescriptors[index].settingId;
        g_hubBoolSettingDefs[index].label = g_hubBoolSettingDescriptors[index].label;
        g_hubBoolSettingDefs[index].description = g_hubBoolSettingDescriptors[index].description;
        g_hubBoolSettingDefs[index].user_data = &g_hubBoolSettingDescriptors[index];
        g_hubBoolSettingDefs[index].get_value = &GetHubBoolSetting;
        g_hubBoolSettingDefs[index].set_value = &SetHubBoolSetting;

        g_hubSettingRows[index].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL;
        g_hubSettingRows[index].def = &g_hubBoolSettingDefs[index];
    }

    g_modHubTableRegistration.row_count = static_cast<uint32_t>(kHubBoolSettingCount);
    g_modHubRowsInitialized = true;
}

void ConfigureModHubClient()
{
    InitializeModHubSettingRows();

    emc::ModHubClient::Config config;
    config.table_registration = &g_modHubTableRegistration;
    g_modHubClient.SetConfig(config);
}

void LogModHubFallback(const char* reason)
{
    std::stringstream line;
    line << "event=mod_hub_fallback"
         << " reason=" << (reason == 0 ? "unknown" : reason)
         << " result=" << g_modHubClient.LastAttemptFailureResult()
         << " retry_pending=" << (g_modHubClient.IsAttachRetryPending() ? 1 : 0)
         << " use_hub_ui=0";
    LogWarnLine(line.str());
}

void StartModHubClient()
{
    const emc::ModHubClient::AttemptResult result = g_modHubClient.OnStartup();
    if (result == emc::ModHubClient::ATTACH_SUCCESS)
    {
        LogInfoLine("event=mod_hub_attached use_hub_ui=1");
        return;
    }

    if (result == emc::ModHubClient::ATTACH_FAILED)
    {
        if (g_modHubClient.IsAttachRetryPending())
        {
            LogInfoLine("event=mod_hub_attach_retry_pending use_hub_ui=0");
            return;
        }

        if (g_modHubClient.LastAttemptFailureResult() == EMC_ERR_NOT_FOUND)
        {
            LogDebugLine("event=mod_hub_unavailable result=5 use_hub_ui=0");
            return;
        }

        LogModHubFallback("get_api_failed");
        return;
    }

    if (result == emc::ModHubClient::REGISTRATION_FAILED)
    {
        LogModHubFallback("register_mod_or_setting_failed");
        return;
    }

    LogModHubFallback("invalid_client_configuration");
}
