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

struct HubIntSettingDescriptor
{
    const char* settingId;
    const char* label;
    const char* description;
    DWORD* field;
    int32_t minValue;
    int32_t maxValue;
    int32_t step;
};

emc::ModHubClient g_modHubClient;
bool g_modHubRowsInitialized = false;

HubBoolSettingDescriptor g_hubBoolSettingDescriptors[] = {
    { "enabled", "Enabled", "Enable Vital Read runtime probes and overlays", &g_enabled },
    { "show_icons", "Show icons", "Show portrait state icon overlays", &g_showIcons },
    { "show_text", "Show text", "Show portrait state text badges", &g_showText },
    { "debug_logging", "Debug logging", "Enable general Vital Read debug logs", &g_debugLogging },
    { "debug_search_logging", "Search debug logging", "Enable portrait search diagnostics (requires Debug logging)", &g_debugSearchLogging },
    { "debug_binding_logging", "Binding debug logging", "Enable portrait binding diagnostics (requires Debug logging)", &g_debugBindingLogging }
};

enum
{
    kHubBoolSettingCount =
        static_cast<int>(sizeof(g_hubBoolSettingDescriptors) / sizeof(g_hubBoolSettingDescriptors[0]))
};

HubIntSettingDescriptor g_hubIntSettingDescriptors[] = {
    { "portrait_text_font_height_px", "Text size", "Set portrait state text badge font height", &g_portraitTextFontHeightPx, 8, 48, 1 }
};

enum
{
    kHubIntSettingCount =
        static_cast<int>(sizeof(g_hubIntSettingDescriptors) / sizeof(g_hubIntSettingDescriptors[0])),
    kHubSettingRowCount = kHubBoolSettingCount + kHubIntSettingCount
};

EMC_BoolSettingDefV1 g_hubBoolSettingDefs[kHubBoolSettingCount];
EMC_IntSettingDefV1 g_hubIntSettingDefs[kHubIntSettingCount];
emc::ModHubClientSettingRowV1 g_hubSettingRows[kHubSettingRowCount];

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

    if (descriptor->field == &g_enabled)
    {
        ResetStateOverlays();
        if (!g_enabled)
        {
            HideHoveredMarker();
        }
    }
    else if (descriptor->field == &g_showIcons || descriptor->field == &g_showText)
    {
        ResetStateOverlays();
    }

    emc::consumer::WriteErrorMessage(err_buf, err_buf_size, 0);
    return EMC_OK;
}

EMC_Result __cdecl GetHubIntSetting(void* user_data, int32_t* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HubIntSettingDescriptor* descriptor = static_cast<const HubIntSettingDescriptor*>(user_data);
    if (descriptor->field == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = static_cast<int32_t>(*descriptor->field);
    return EMC_OK;
}

EMC_Result __cdecl SetHubIntSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    if (user_data == 0)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_setting");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    HubIntSettingDescriptor* descriptor = static_cast<HubIntSettingDescriptor*>(user_data);
    if (descriptor->field == 0)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_setting");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value < descriptor->minValue || value > descriptor->maxValue)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "out_of_range");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const int32_t offset = value - descriptor->minValue;
    if (descriptor->step > 0 && (offset % descriptor->step) != 0)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_step");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const DWORD previousValue = *descriptor->field;
    *descriptor->field = static_cast<DWORD>(value);
    if (!SaveConfigState())
    {
        *descriptor->field = previousValue;
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "persist_failed");
        return EMC_ERR_INTERNAL;
    }

    ResetStateOverlays();
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

    for (size_t index = 0u; index < static_cast<size_t>(kHubIntSettingCount); ++index)
    {
        const size_t rowIndex = static_cast<size_t>(kHubBoolSettingCount) + index;
        g_hubIntSettingDefs[index].setting_id = g_hubIntSettingDescriptors[index].settingId;
        g_hubIntSettingDefs[index].label = g_hubIntSettingDescriptors[index].label;
        g_hubIntSettingDefs[index].description = g_hubIntSettingDescriptors[index].description;
        g_hubIntSettingDefs[index].user_data = &g_hubIntSettingDescriptors[index];
        g_hubIntSettingDefs[index].min_value = g_hubIntSettingDescriptors[index].minValue;
        g_hubIntSettingDefs[index].max_value = g_hubIntSettingDescriptors[index].maxValue;
        g_hubIntSettingDefs[index].step = g_hubIntSettingDescriptors[index].step;
        g_hubIntSettingDefs[index].get_value = &GetHubIntSetting;
        g_hubIntSettingDefs[index].set_value = &SetHubIntSetting;

        g_hubSettingRows[rowIndex].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_INT;
        g_hubSettingRows[rowIndex].def = &g_hubIntSettingDefs[index];
    }

    g_modHubTableRegistration.row_count = static_cast<uint32_t>(kHubSettingRowCount);
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
