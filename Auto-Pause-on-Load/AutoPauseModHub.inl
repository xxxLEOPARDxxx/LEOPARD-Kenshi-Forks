#include "emc/mod_hub_client.h"
#include "emc/mod_hub_consumer_helpers.h"

static const char* kHubNamespaceId = "emkej.qol";
static const char* kHubNamespaceDisplayName = "Emkej QoL";
static const char* kHubModId = "auto_pause_on_load";
static const char* kHubModDisplayName = "Auto Pause on Load";

static emc::ModHubClient g_modHubClient;

static bool PersistHubConfig(const PluginConfig& next_config)
{
    (void)next_config;
    if (!SaveConfigState())
    {
        return false;
    }

    g_configNeedsWriteBack = false;
    if (!g_config.enabled)
    {
        DisarmPauseAfterLoad();
        ResetUiTracking();
    }

    return true;
}

typedef bool PluginConfig::*PluginConfigBoolField;

template <typename UpdateFn>
static EMC_Result ApplyHubConfigUpdate(
    void* user_data,
    char* err_buf,
    uint32_t err_buf_size,
    UpdateFn update)
{
    PluginConfig* config = static_cast<PluginConfig*>(user_data);
    if (!config || config != &g_config)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_config_target");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const PluginConfig previous = *config;
    PluginConfig updated = previous;
    update(updated);

    return emc::consumer::ApplyUpdateWithRollback(
        previous,
        updated,
        err_buf,
        err_buf_size,
        [config](const PluginConfig& snapshot) {
            *config = snapshot;
        },
        &PersistHubConfig,
        "save_config_failed");
}

static EMC_Result GetBoolHubSettingValue(void* user_data, int32_t* out_value, PluginConfigBoolField field)
{
    if (!user_data || !out_value)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const PluginConfig* config = static_cast<const PluginConfig*>(user_data);
    *out_value = (config->*field) ? 1 : 0;
    return EMC_OK;
}

static EMC_Result SetBoolHubSettingValue(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size,
    PluginConfigBoolField field)
{
    if (!user_data)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const EMC_Result boolValidation = emc::consumer::ValidateBoolValue(value, err_buf, err_buf_size);
    if (boolValidation != EMC_OK)
    {
        return boolValidation;
    }

    return ApplyHubConfigUpdate(
        user_data,
        err_buf,
        err_buf_size,
        [field, value](PluginConfig& updated) {
            updated.*field = value == 1;
        });
}

static EMC_Result __cdecl GetEnabledSetting(void* user_data, int32_t* out_value)
{
    return GetBoolHubSettingValue(user_data, out_value, &PluginConfig::enabled);
}

static EMC_Result __cdecl SetEnabledSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return SetBoolHubSettingValue(user_data, value, err_buf, err_buf_size, &PluginConfig::enabled);
}

static EMC_Result __cdecl GetPauseDebounceMsSetting(void* user_data, int32_t* out_value)
{
    if (!user_data || !out_value)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const PluginConfig* config = static_cast<const PluginConfig*>(user_data);
    *out_value = static_cast<int32_t>(config->pauseDebounceMs);
    return EMC_OK;
}

static EMC_Result __cdecl SetPauseDebounceMsSetting(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size)
{
    if (!user_data)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const EMC_Result rangeValidation = emc::consumer::ValidateValueInRange<int32_t>(
        value,
        static_cast<int32_t>(kPauseDebounceMsMin),
        static_cast<int32_t>(kPauseDebounceMsMax),
        err_buf,
        err_buf_size,
        "pause_debounce_ms_out_of_range");
    if (rangeValidation != EMC_OK)
    {
        return rangeValidation;
    }

    return ApplyHubConfigUpdate(
        user_data,
        err_buf,
        err_buf_size,
        [value](PluginConfig& updated) {
            updated.pauseDebounceMs = static_cast<DWORD>(value);
        });
}

static EMC_Result __cdecl GetDebugLogTransitionsSetting(void* user_data, int32_t* out_value)
{
    return GetBoolHubSettingValue(user_data, out_value, &PluginConfig::debugLogTransitions);
}

static EMC_Result __cdecl SetDebugLogTransitionsSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return SetBoolHubSettingValue(
        user_data,
        value,
        err_buf,
        err_buf_size,
        &PluginConfig::debugLogTransitions);
}

static EMC_Result __cdecl GetPauseOnTradeSetting(void* user_data, int32_t* out_value)
{
    return GetBoolHubSettingValue(user_data, out_value, &PluginConfig::pauseOnTrade);
}

static EMC_Result __cdecl SetPauseOnTradeSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return SetBoolHubSettingValue(user_data, value, err_buf, err_buf_size, &PluginConfig::pauseOnTrade);
}

static EMC_Result __cdecl GetResumeAfterTradeSetting(void* user_data, int32_t* out_value)
{
    return GetBoolHubSettingValue(user_data, out_value, &PluginConfig::resumeAfterTrade);
}

static EMC_Result __cdecl SetResumeAfterTradeSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return SetBoolHubSettingValue(user_data, value, err_buf, err_buf_size, &PluginConfig::resumeAfterTrade);
}

static EMC_Result __cdecl GetPauseOnInventoryOpenSetting(void* user_data, int32_t* out_value)
{
    return GetBoolHubSettingValue(user_data, out_value, &PluginConfig::pauseOnInventoryOpen);
}

static EMC_Result __cdecl SetPauseOnInventoryOpenSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return SetBoolHubSettingValue(
        user_data,
        value,
        err_buf,
        err_buf_size,
        &PluginConfig::pauseOnInventoryOpen);
}

static EMC_Result __cdecl GetResumeAfterInventoryCloseSetting(void* user_data, int32_t* out_value)
{
    return GetBoolHubSettingValue(user_data, out_value, &PluginConfig::resumeAfterInventoryClose);
}

static EMC_Result __cdecl SetResumeAfterInventoryCloseSetting(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size)
{
    return SetBoolHubSettingValue(
        user_data,
        value,
        err_buf,
        err_buf_size,
        &PluginConfig::resumeAfterInventoryClose);
}

static void LogModHubFallback(const char* reason)
{
    std::stringstream line;
    line << kPluginName
         << " WARN: event=mod_hub_fallback"
         << " reason=" << (reason ? reason : "unknown")
         << " result=" << g_modHubClient.LastAttemptFailureResult()
         << " retry_pending=" << (g_modHubClient.IsAttachRetryPending() ? 1 : 0)
         << " use_hub_ui=0";
    ErrorLog(line.str().c_str());
}

static const EMC_ModDescriptorV1 kModHubDescriptor = {
    kHubNamespaceId,
    kHubNamespaceDisplayName,
    kHubModId,
    kHubModDisplayName,
    &g_config };

static const EMC_BoolSettingDefV1 kHubEnabledSetting = {
    "enabled",
    "Enabled",
    "Enable all Auto-Pause-on-Load behavior",
    &g_config,
    &GetEnabledSetting,
    &SetEnabledSetting };

static const EMC_IntSettingDefV1 kHubPauseDebounceMsSetting = {
    "pause_debounce_ms",
    "Pause debounce (ms)",
    "Minimum interval between automatic pause events",
    &g_config,
    static_cast<int32_t>(kPauseDebounceMsMin),
    static_cast<int32_t>(kPauseDebounceMsMax),
    100,
    &GetPauseDebounceMsSetting,
    &SetPauseDebounceMsSetting };

static const EMC_BoolSettingDefV1 kHubDebugLogTransitionsSetting = {
    "debug_log_transitions",
    "Debug log transitions",
    "Emit verbose transition logs for load and UI pause flows",
    &g_config,
    &GetDebugLogTransitionsSetting,
    &SetDebugLogTransitionsSetting };

static const EMC_BoolSettingDefV1 kHubPauseOnTradeSetting = {
    "pause_on_trade",
    "Pause on trade",
    "Pause the game when trade UI opens",
    &g_config,
    &GetPauseOnTradeSetting,
    &SetPauseOnTradeSetting };

static const EMC_BoolSettingDefV1 kHubResumeAfterTradeSetting = {
    "resume_after_trade",
    "Resume after trade",
    "Resume when trade UI closes if this mod paused the game",
    &g_config,
    &GetResumeAfterTradeSetting,
    &SetResumeAfterTradeSetting };

static const EMC_BoolSettingDefV1 kHubPauseOnInventoryOpenSetting = {
    "pause_on_inventory_open",
    "Pause on inventory open",
    "Pause the game when inventory opens",
    &g_config,
    &GetPauseOnInventoryOpenSetting,
    &SetPauseOnInventoryOpenSetting };

static const EMC_BoolSettingDefV1 kHubResumeAfterInventoryCloseSetting = {
    "resume_after_inventory_close",
    "Resume after inventory close",
    "Resume when inventory closes if this mod paused the game",
    &g_config,
    &GetResumeAfterInventoryCloseSetting,
    &SetResumeAfterInventoryCloseSetting };

static const emc::ModHubClientSettingRowV1 kModHubSettingRows[] = {
    { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kHubEnabledSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &kHubPauseDebounceMsSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kHubDebugLogTransitionsSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kHubPauseOnTradeSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kHubResumeAfterTradeSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kHubPauseOnInventoryOpenSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kHubResumeAfterInventoryCloseSetting }
};

static const emc::ModHubClientTableRegistrationV1 kModHubTableRegistration = {
    &kModHubDescriptor,
    kModHubSettingRows,
    static_cast<uint32_t>(sizeof(kModHubSettingRows) / sizeof(kModHubSettingRows[0])) };

static void ConfigureModHubClient()
{
    emc::ModHubClient::Config hubConfig;
    hubConfig.table_registration = &kModHubTableRegistration;
    g_modHubClient.SetConfig(hubConfig);
}

static void StartModHubClient()
{
    const emc::ModHubClient::AttemptResult result = g_modHubClient.OnStartup();
    if (result == emc::ModHubClient::ATTACH_SUCCESS)
    {
        DebugLog("Auto-Pause-on-Load INFO: event=mod_hub_attached use_hub_ui=1");
        return;
    }

    if (result == emc::ModHubClient::ATTACH_FAILED)
    {
        if (g_modHubClient.IsAttachRetryPending())
        {
            DebugLog("Auto-Pause-on-Load INFO: event=mod_hub_attach_retry_pending use_hub_ui=0");
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
