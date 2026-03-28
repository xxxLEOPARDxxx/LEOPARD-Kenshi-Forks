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
    DWORD* unsignedField;
    int* signedField;
    std::string* stringField;
    int32_t fallbackValue;
    int32_t minValue;
    int32_t maxValue;
    int32_t step;
    bool useCustomButtons;
    int32_t decButtonDeltas[3];
    int32_t incButtonDeltas[3];
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
    { "portrait_icon_display_size_px", "Icon size", "Set portrait state icon display size in pixels (0 keeps automatic sizing)", &g_portraitIconDisplaySizePx, 0, 0, 0, 0, 64, 1, false, { 0, 0, 0 }, { 0, 0, 0 } },
    { "portrait_text_font_height_px", "Text size", "Set portrait state text badge font height", &g_portraitTextFontHeightPx, 0, 0, 0, 8, 48, 1, false, { 0, 0, 0 }, { 0, 0, 0 } },
    { "portrait_overlay_margin_x_px", "Overlay X margin", "Set horizontal edge margin for portrait icons and text; positive moves inward, negative moves outward", 0, &g_portraitOverlayMarginXPx, 0, 0, -16, 16, 1, true, { 5, 1, 0 }, { 1, 5, 0 } },
    { "portrait_overlay_margin_y_px", "Overlay Y margin", "Set vertical edge margin for portrait icons and text; positive moves inward, negative moves outward", 0, &g_portraitOverlayMarginYPx, 0, 0, -16, 16, 1, true, { 5, 1, 0 }, { 1, 5, 0 } },
    { "portrait_icon_anchor", "Icon corner", "Set portrait icon corner: 0=bottom left, 1=bottom right, 2=top left, 3=top right", 0, 0, &g_portraitIconAnchor, 0, 0, 3, 1, true, { 1, 0, 0 }, { 1, 0, 0 } },
    { "portrait_text_anchor", "Text corner", "Set portrait text corner: 0=bottom left, 1=bottom right, 2=top left, 3=top right", 0, 0, &g_portraitTextAnchor, 3, 0, 3, 1, true, { 1, 0, 0 }, { 1, 0, 0 } }
};

enum
{
    kHubIntSettingCount =
        static_cast<int>(sizeof(g_hubIntSettingDescriptors) / sizeof(g_hubIntSettingDescriptors[0])),
    kHubSettingRowCount = kHubBoolSettingCount + kHubIntSettingCount
};

EMC_BoolSettingDefV1 g_hubBoolSettingDefs[kHubBoolSettingCount];
EMC_IntSettingDefV1 g_hubIntSettingDefs[kHubIntSettingCount];
EMC_IntSettingDefV2 g_hubIntSettingDefsV2[kHubIntSettingCount];
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
    if (descriptor->unsignedField != 0)
    {
        *out_value = static_cast<int32_t>(*descriptor->unsignedField);
        return EMC_OK;
    }

    if (descriptor->signedField != 0)
    {
        *out_value = *descriptor->signedField;
        return EMC_OK;
    }

    if (descriptor->stringField == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const std::string lowered = ToLowerAscii(*descriptor->stringField);
    if (lowered == "bottom_left")
    {
        *out_value = 0;
    }
    else if (lowered == "bottom_right")
    {
        *out_value = 1;
    }
    else if (lowered == "top_left")
    {
        *out_value = 2;
    }
    else if (lowered == "top_right")
    {
        *out_value = 3;
    }
    else
    {
        *out_value = descriptor->fallbackValue;
    }
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
    if (descriptor->unsignedField == 0 && descriptor->signedField == 0 && descriptor->stringField == 0)
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

    DWORD previousUnsignedValue = 0u;
    int previousSignedValue = 0;
    std::string previousStringValue;
    if (descriptor->unsignedField != 0)
    {
        previousUnsignedValue = *descriptor->unsignedField;
        *descriptor->unsignedField = static_cast<DWORD>(value);
    }
    else if (descriptor->signedField != 0)
    {
        previousSignedValue = *descriptor->signedField;
        *descriptor->signedField = value;
    }
    else
    {
        previousStringValue = *descriptor->stringField;
        switch (value)
        {
        case 0:
            *descriptor->stringField = "bottom_left";
            break;
        case 1:
            *descriptor->stringField = "bottom_right";
            break;
        case 2:
            *descriptor->stringField = "top_left";
            break;
        case 3:
        default:
            *descriptor->stringField = "top_right";
            break;
        }
    }

    if (!SaveConfigState())
    {
        if (descriptor->unsignedField != 0)
        {
            *descriptor->unsignedField = previousUnsignedValue;
        }
        else if (descriptor->signedField != 0)
        {
            *descriptor->signedField = previousSignedValue;
        }
        else
        {
            *descriptor->stringField = previousStringValue;
        }
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
        if (g_hubIntSettingDescriptors[index].useCustomButtons)
        {
            g_hubIntSettingDefsV2[index].setting_id = g_hubIntSettingDescriptors[index].settingId;
            g_hubIntSettingDefsV2[index].label = g_hubIntSettingDescriptors[index].label;
            g_hubIntSettingDefsV2[index].description = g_hubIntSettingDescriptors[index].description;
            g_hubIntSettingDefsV2[index].user_data = &g_hubIntSettingDescriptors[index];
            g_hubIntSettingDefsV2[index].min_value = g_hubIntSettingDescriptors[index].minValue;
            g_hubIntSettingDefsV2[index].max_value = g_hubIntSettingDescriptors[index].maxValue;
            g_hubIntSettingDefsV2[index].step = g_hubIntSettingDescriptors[index].step;
            std::memcpy(
                g_hubIntSettingDefsV2[index].dec_button_deltas,
                g_hubIntSettingDescriptors[index].decButtonDeltas,
                sizeof(g_hubIntSettingDefsV2[index].dec_button_deltas));
            std::memcpy(
                g_hubIntSettingDefsV2[index].inc_button_deltas,
                g_hubIntSettingDescriptors[index].incButtonDeltas,
                sizeof(g_hubIntSettingDefsV2[index].inc_button_deltas));
            g_hubIntSettingDefsV2[index].get_value = &GetHubIntSetting;
            g_hubIntSettingDefsV2[index].set_value = &SetHubIntSetting;

            g_hubSettingRows[rowIndex].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_INT_V2;
            g_hubSettingRows[rowIndex].def = &g_hubIntSettingDefsV2[index];
            continue;
        }

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
        LogDebugLine("event=mod_hub_attached use_hub_ui=1");
        return;
    }

    if (result == emc::ModHubClient::ATTACH_FAILED)
    {
        if (g_modHubClient.IsAttachRetryPending())
        {
            LogDebugLine("event=mod_hub_attach_retry_pending use_hub_ui=0");
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
