#include "emc/mod_hub_client.h"

static const DWORD kModHubAttachRetryIntervalMs = 2000;
static const DWORD kModHubAttachRetryMaxAttempts = 30;

static const char* kHubNamespaceId = "emkej.qol";
static const char* kHubNamespaceDisplayName = "Emkej QoL";
static const char* kHubModId = "auto_pause_on_load";
static const char* kHubModDisplayName = "Auto Pause on Load";

static emc::ModHubClient g_modHubClient;
static bool g_modHubAttachRetryActive = false;
static DWORD g_modHubAttachRetryAttempts = 0;
static DWORD g_modHubAttachRetryLastAttemptMs = 0;
static bool g_loggedOptionsObserverCallback = false;

static bool TryInstallGameHooks();

static void __cdecl OnModHubOptionsWindowInitProbe(void* user_data)
{
    static_cast<void>(user_data);

    if (g_loggedOptionsObserverCallback)
    {
        return;
    }

    g_loggedOptionsObserverCallback = true;
    std::stringstream line;
    line << "Auto-Pause-on-Load INFO: [investigate][options-observer]"
         << " stage=callback_enter"
         << " thread=" << GetCurrentThreadId()
         << " source=mod_hub_options_init";
    DebugLog(line.str().c_str());

    TryInstallGameHooks();
}

static void WriteHubErrorText(char* err_buf, uint32_t err_buf_size, const char* text)
{
    if (!err_buf || err_buf_size == 0u)
    {
        return;
    }

    if (!text)
    {
        err_buf[0] = '\0';
        return;
    }

    const size_t copyLen = static_cast<size_t>(err_buf_size - 1u);
    std::strncpy(err_buf, text, copyLen);
    err_buf[copyLen] = '\0';
}

static EMC_Result ApplyHubConfigUpdate(
    PluginConfig* config,
    const PluginConfig& updated,
    char* err_buf,
    uint32_t err_buf_size)
{
    if (!config || config != &g_config)
    {
        WriteHubErrorText(err_buf, err_buf_size, "invalid_config_target");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const PluginConfig previous = *config;
    *config = updated;
    if (!SaveConfigState())
    {
        *config = previous;
        WriteHubErrorText(err_buf, err_buf_size, "save_config_failed");
        return EMC_ERR_INTERNAL;
    }

    g_configNeedsWriteBack = false;
    if (!config->enabled)
    {
        DisarmPauseAfterLoad();
        ResetUiTracking();
    }

    return EMC_OK;
}

typedef bool PluginConfig::*PluginConfigBoolField;

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
        WriteHubErrorText(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    PluginConfig* config = static_cast<PluginConfig*>(user_data);
    PluginConfig updated = *config;
    updated.*field = value != 0;
    return ApplyHubConfigUpdate(config, updated, err_buf, err_buf_size);
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
        WriteHubErrorText(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value < static_cast<int32_t>(kPauseDebounceMsMin) || value > static_cast<int32_t>(kPauseDebounceMsMax))
    {
        WriteHubErrorText(err_buf, err_buf_size, "pause_debounce_ms_out_of_range");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    PluginConfig* config = static_cast<PluginConfig*>(user_data);
    PluginConfig updated = *config;
    updated.pauseDebounceMs = static_cast<DWORD>(value);
    return ApplyHubConfigUpdate(config, updated, err_buf, err_buf_size);
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
         << " use_hub_ui=0";
    ErrorLog(line.str().c_str());
}

static void LogModHubRetryEvent(const char* eventName, emc::ModHubClient::AttemptResult result)
{
    std::stringstream line;
    line << kPluginName
         << " INFO: event=" << (eventName ? eventName : "mod_hub_retry")
         << " attempt=" << g_modHubAttachRetryAttempts
         << " result_enum=" << static_cast<int32_t>(result)
         << " result=" << g_modHubClient.LastAttemptFailureResult()
         << " use_hub_ui=" << (g_modHubClient.UseHubUi() ? 1 : 0);
    DebugLog(line.str().c_str());
}

static bool ShouldLogModHubRetryEvent(emc::ModHubClient::AttemptResult result)
{
    if (g_config.debugLogTransitions)
    {
        return true;
    }

    if (g_modHubAttachRetryAttempts <= 1)
    {
        return true;
    }

    return result != emc::ModHubClient::ATTACH_FAILED;
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
    hubConfig.options_window_init_callback = &OnModHubOptionsWindowInitProbe;
    g_modHubClient.SetConfig(hubConfig);
}

static void StartModHubClient()
{
    g_modHubAttachRetryActive = false;
    g_modHubAttachRetryAttempts = 0;
    g_modHubAttachRetryLastAttemptMs = GetTickCount();

    const emc::ModHubClient::AttemptResult result = g_modHubClient.OnStartup();
    {
        std::stringstream line;
        line << "Auto-Pause-on-Load INFO: [investigate][options-observer]"
             << " stage="
             << (g_modHubClient.IsOptionsWindowInitObserverRegistered() ? "registered" : "not_registered")
             << " thread=" << GetCurrentThreadId()
             << " source=mod_hub_options_init";
        DebugLog(line.str().c_str());
    }
    if (result == emc::ModHubClient::ATTACH_SUCCESS)
    {
        DebugLog("Auto-Pause-on-Load INFO: event=mod_hub_attached use_hub_ui=1");
        return;
    }

    if (result == emc::ModHubClient::ATTACH_FAILED)
    {
        LogModHubFallback("get_api_failed");
        g_modHubAttachRetryActive = true;
        return;
    }

    if (result == emc::ModHubClient::REGISTRATION_FAILED)
    {
        LogModHubFallback("register_mod_or_setting_failed");
        return;
    }

    LogModHubFallback("invalid_client_configuration");
}

static void TickModHubAttachRetry()
{
    if (!g_modHubAttachRetryActive || g_modHubClient.UseHubUi())
    {
        return;
    }

    if (g_modHubAttachRetryAttempts >= kModHubAttachRetryMaxAttempts)
    {
        g_modHubAttachRetryActive = false;
        ErrorLog("Auto-Pause-on-Load WARN: event=mod_hub_retry_stopped reason=max_attempts_reached");
        return;
    }

    const DWORD nowMs = GetTickCount();
    if (!DebounceWindowElapsed(nowMs, g_modHubAttachRetryLastAttemptMs, kModHubAttachRetryIntervalMs))
    {
        return;
    }

    ++g_modHubAttachRetryAttempts;
    g_modHubAttachRetryLastAttemptMs = nowMs;

    const emc::ModHubClient::AttemptResult result = g_modHubClient.OnStartup();
    if (ShouldLogModHubRetryEvent(result))
    {
        LogModHubRetryEvent("mod_hub_retry_attempt", result);
    }
    if (result == emc::ModHubClient::ATTACH_SUCCESS)
    {
        g_modHubAttachRetryActive = false;
        DebugLog("Auto-Pause-on-Load INFO: event=mod_hub_retry_success");
        return;
    }

    if (result == emc::ModHubClient::REGISTRATION_FAILED)
    {
        g_modHubAttachRetryActive = false;
        LogModHubFallback("register_mod_or_setting_failed");
        return;
    }
}
