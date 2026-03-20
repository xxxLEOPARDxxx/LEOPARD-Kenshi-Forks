#include "emc/mod_hub_api.h"
#include "emc/mod_hub_client.h"

const char* kModHubNamespaceId = "emkej.qol";
const char* kModHubNamespaceDisplayName = "Emkej QoL";
const char* kModHubModId = "loot_scoot_execute";
const char* kModHubModDisplayName = "Loot-Scoot-Execute";

const char* kModHubSettingEnabledId = "enabled";
const char* kModHubSettingExecuteKillSoundId = "enable_execute_kill_sound";
const char* kModHubSettingDebugExecuteLoggingId = "debug_execute_logging";
const char* kModHubSettingIgnoreExecuteAllianceCheckId = "ignore_execute_alliance_check";
const char* kModHubSettingExecuteButtonWidthId = "execute_button_width";
const char* kModHubSettingExecuteButtonHeightId = "execute_button_height";
const char* kModHubSettingExecuteButtonXId = "execute_button_x";
const char* kModHubSettingExecuteButtonYId = "execute_button_y";
const char* kModHubActionResetExecuteButtonDefaultsId = "reset_execute_button_defaults";

struct LootScootExecuteModHubState
{
    PluginConfig* config;
};

LootScootExecuteModHubState g_modHubState = { &g_config };
emc::ModHubClient g_modHubClient;
bool g_modHubClientConfigured = false;
bool g_modHubLoggedRegisterFallback = false;

static const char* SafeHubLogValue(const char* value)
{
    if (value == 0 || value[0] == '\0')
    {
        return "none";
    }

    return value;
}

static void WriteHubErrorText(char* err_buf, uint32_t err_buf_size, const char* text)
{
    if (err_buf == 0 || err_buf_size == 0u || text == 0)
    {
        return;
    }

    const size_t copy_len = static_cast<size_t>(err_buf_size - 1u);
    std::strncpy(err_buf, text, copy_len);
    err_buf[copy_len] = '\0';
}

static bool IsHubStateValid(void* user_data)
{
    return user_data == &g_modHubState && g_modHubState.config != 0;
}

static EMC_Result HubGetIntConfigValue(void* user_data, const int* source, int32_t* out_value)
{
    if (!IsHubStateValid(user_data) || source == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = static_cast<int32_t>(*source);
    return EMC_OK;
}

static EMC_Result HubGetBoolConfigValue(void* user_data, const bool* source, int32_t* out_value)
{
    if (!IsHubStateValid(user_data) || source == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = (*source) ? 1 : 0;
    return EMC_OK;
}

static EMC_Result HubSetBoolConfigValue(
    void* user_data,
    int32_t value,
    bool* destination,
    char* err_buf,
    uint32_t err_buf_size)
{
    if (!IsHubStateValid(user_data) || destination == 0)
    {
        WriteHubErrorText(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const bool next_value = value != 0;
    const bool previous_value = *destination;
    if (previous_value == next_value)
    {
        return EMC_OK;
    }

    *destination = next_value;
    if (!SaveConfigState())
    {
        *destination = previous_value;
        WriteHubErrorText(err_buf, err_buf_size, "save_config_failed");
        return EMC_ERR_INTERNAL;
    }

    return EMC_OK;
}

static EMC_Result HubSetIntConfigValue(
    void* user_data,
    int32_t value,
    int min_value,
    int max_value,
    int* destination,
    char* err_buf,
    uint32_t err_buf_size)
{
    if (!IsHubStateValid(user_data) || destination == 0)
    {
        WriteHubErrorText(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value < min_value || value > max_value)
    {
        WriteHubErrorText(err_buf, err_buf_size, "value_out_of_range");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const int next_value = static_cast<int>(value);
    const int previous_value = *destination;
    if (previous_value == next_value)
    {
        return EMC_OK;
    }

    *destination = next_value;
    if (!SaveConfigState())
    {
        *destination = previous_value;
        WriteHubErrorText(err_buf, err_buf_size, "save_config_failed");
        return EMC_ERR_INTERNAL;
    }

    return EMC_OK;
}

static EMC_Result __cdecl HubGetEnabled(void* user_data, int32_t* out_value)
{
    return HubGetBoolConfigValue(user_data, &g_modHubState.config->enabled, out_value);
}

static EMC_Result __cdecl HubSetEnabled(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    if (!IsHubStateValid(user_data))
    {
        WriteHubErrorText(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const bool next_value = value != 0;
    const bool previous_value = g_modHubState.config->enabled;
    if (previous_value == next_value)
    {
        return EMC_OK;
    }

    g_modHubState.config->enabled = next_value;
    if (!next_value)
    {
        DisarmQueuedExecuteAction("hub_setting_plugin_disabled", false);
        DisarmNativeMenuExecuteDispatchContext();
        DisarmNativeMenuOrderRemapContext();
        HideCustomExecutePanelOverlay();
    }
    RefreshEffectiveContextMenuFeatureFlags("hub_set_enabled");

    if (!SaveConfigState())
    {
        g_modHubState.config->enabled = previous_value;
        RefreshEffectiveContextMenuFeatureFlags("hub_rollback_enabled");
        WriteHubErrorText(err_buf, err_buf_size, "save_config_failed");
        return EMC_ERR_INTERNAL;
    }

    return EMC_OK;
}

static EMC_Result __cdecl HubGetExecuteKillSound(void* user_data, int32_t* out_value)
{
    return HubGetBoolConfigValue(user_data, &g_modHubState.config->enableExecuteKillSound, out_value);
}

static EMC_Result __cdecl HubSetExecuteKillSound(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return HubSetBoolConfigValue(
        user_data,
        value,
        &g_modHubState.config->enableExecuteKillSound,
        err_buf,
        err_buf_size);
}

static EMC_Result __cdecl HubGetDebugExecuteLogging(void* user_data, int32_t* out_value)
{
    return HubGetBoolConfigValue(user_data, &g_modHubState.config->debugExecuteLogging, out_value);
}

static EMC_Result __cdecl HubSetDebugExecuteLogging(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return HubSetBoolConfigValue(
        user_data,
        value,
        &g_modHubState.config->debugExecuteLogging,
        err_buf,
        err_buf_size);
}

static EMC_Result __cdecl HubGetIgnoreExecuteAllianceCheck(void* user_data, int32_t* out_value)
{
    return HubGetBoolConfigValue(user_data, &g_modHubState.config->ignoreExecuteAllianceCheck, out_value);
}

static EMC_Result __cdecl HubSetIgnoreExecuteAllianceCheck(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return HubSetBoolConfigValue(
        user_data,
        value,
        &g_modHubState.config->ignoreExecuteAllianceCheck,
        err_buf,
        err_buf_size);
}

static EMC_Result __cdecl HubGetExecuteButtonWidth(void* user_data, int32_t* out_value)
{
    return HubGetIntConfigValue(user_data, &g_modHubState.config->executeButtonWidthPx, out_value);
}

static EMC_Result __cdecl HubSetExecuteButtonWidth(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return HubSetIntConfigValue(
        user_data,
        value,
        kExecuteButtonRuntimeWidthMin,
        kExecuteButtonWidthMax,
        &g_modHubState.config->executeButtonWidthPx,
        err_buf,
        err_buf_size);
}

static EMC_Result __cdecl HubGetExecuteButtonHeight(void* user_data, int32_t* out_value)
{
    return HubGetIntConfigValue(user_data, &g_modHubState.config->executeButtonHeightPx, out_value);
}

static EMC_Result __cdecl HubSetExecuteButtonHeight(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return HubSetIntConfigValue(
        user_data,
        value,
        kExecuteButtonRuntimeHeightMin,
        kExecuteButtonHeightMax,
        &g_modHubState.config->executeButtonHeightPx,
        err_buf,
        err_buf_size);
}

static EMC_Result __cdecl HubGetExecuteButtonX(void* user_data, int32_t* out_value)
{
    return HubGetIntConfigValue(user_data, &g_modHubState.config->executeButtonOffsetXPx, out_value);
}

static EMC_Result __cdecl HubSetExecuteButtonX(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return HubSetIntConfigValue(
        user_data,
        value,
        kExecuteButtonOffsetMin,
        kExecuteButtonOffsetMax,
        &g_modHubState.config->executeButtonOffsetXPx,
        err_buf,
        err_buf_size);
}

static EMC_Result __cdecl HubGetExecuteButtonY(void* user_data, int32_t* out_value)
{
    return HubGetIntConfigValue(user_data, &g_modHubState.config->executeButtonOffsetYPx, out_value);
}

static EMC_Result __cdecl HubSetExecuteButtonY(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return HubSetIntConfigValue(
        user_data,
        value,
        kExecuteButtonOffsetMin,
        kExecuteButtonOffsetMax,
        &g_modHubState.config->executeButtonOffsetYPx,
        err_buf,
        err_buf_size);
}

static EMC_Result __cdecl HubResetExecuteButtonDefaults(void* user_data, char* err_buf, uint32_t err_buf_size)
{
    if (!IsHubStateValid(user_data))
    {
        WriteHubErrorText(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const int previous_width = g_modHubState.config->executeButtonWidthPx;
    const int previous_height = g_modHubState.config->executeButtonHeightPx;
    const int previous_x = g_modHubState.config->executeButtonOffsetXPx;
    const int previous_y = g_modHubState.config->executeButtonOffsetYPx;
    if (previous_width == kExecuteButtonDefaultWidth
        && previous_height == kExecuteButtonDefaultHeight
        && previous_x == kExecuteButtonDefaultAbsoluteX
        && previous_y == kExecuteButtonDefaultAbsoluteY)
    {
        return EMC_OK;
    }

    g_modHubState.config->executeButtonWidthPx = kExecuteButtonDefaultWidth;
    g_modHubState.config->executeButtonHeightPx = kExecuteButtonDefaultHeight;
    g_modHubState.config->executeButtonOffsetXPx = kExecuteButtonDefaultAbsoluteX;
    g_modHubState.config->executeButtonOffsetYPx = kExecuteButtonDefaultAbsoluteY;

    if (!SaveConfigState())
    {
        g_modHubState.config->executeButtonWidthPx = previous_width;
        g_modHubState.config->executeButtonHeightPx = previous_height;
        g_modHubState.config->executeButtonOffsetXPx = previous_x;
        g_modHubState.config->executeButtonOffsetYPx = previous_y;
        WriteHubErrorText(err_buf, err_buf_size, "save_config_failed");
        return EMC_ERR_INTERNAL;
    }

    return EMC_OK;
}

static const emc::ModHubClientTableRegistrationV1* GetModHubTableRegistration()
{
    static const EMC_ModDescriptorV1 kModDescriptor = {
        kModHubNamespaceId,
        kModHubNamespaceDisplayName,
        kModHubModId,
        kModHubModDisplayName,
        &g_modHubState};

    static const EMC_BoolSettingDefV1 kEnabledSettingDef = {
        kModHubSettingEnabledId,
        "Enabled",
        "Enable Loot-Scoot-Execute",
        &g_modHubState,
        &HubGetEnabled,
        &HubSetEnabled};

    static const EMC_BoolSettingDefV1 kExecuteKillSoundSettingDef = {
        kModHubSettingExecuteKillSoundId,
        "Enable execute kill sound",
        "Play execute kill sound when execute dispatch succeeds",
        &g_modHubState,
        &HubGetExecuteKillSound,
        &HubSetExecuteKillSound};

    static const EMC_BoolSettingDefV1 kDebugExecuteLoggingSettingDef = {
        kModHubSettingDebugExecuteLoggingId,
        "Debug execute logging",
        "Enable focused execute investigation logs in RE_Kenshi_log.txt",
        &g_modHubState,
        &HubGetDebugExecuteLogging,
        &HubSetDebugExecuteLogging};

    static const EMC_BoolSettingDefV1 kIgnoreExecuteAllianceCheckSettingDef = {
        kModHubSettingIgnoreExecuteAllianceCheckId,
        "Ignore execute alliance check",
        "Allow execute on incapacitated non-hostiles; player characters remain blocked",
        &g_modHubState,
        &HubGetIgnoreExecuteAllianceCheck,
        &HubSetIgnoreExecuteAllianceCheck};

    static const EMC_IntSettingDefV1 kExecuteButtonWidthSettingDef = {
        kModHubSettingExecuteButtonWidthId,
        "Execute button width",
        "Execute button width in pixels (default 310)",
        &g_modHubState,
        kExecuteButtonRuntimeWidthMin,
        kExecuteButtonWidthMax,
        1,
        &HubGetExecuteButtonWidth,
        &HubSetExecuteButtonWidth};

    static const EMC_IntSettingDefV1 kExecuteButtonHeightSettingDef = {
        kModHubSettingExecuteButtonHeightId,
        "Execute button height",
        "Execute button height in pixels (default 56)",
        &g_modHubState,
        kExecuteButtonRuntimeHeightMin,
        kExecuteButtonHeightMax,
        1,
        &HubGetExecuteButtonHeight,
        &HubSetExecuteButtonHeight};

    static const EMC_IntSettingDefV1 kExecuteButtonXSettingDef = {
        kModHubSettingExecuteButtonXId,
        "Execute button X",
        "Horizontal offset in pixels from the default anchored position",
        &g_modHubState,
        kExecuteButtonOffsetMin,
        kExecuteButtonOffsetMax,
        1,
        &HubGetExecuteButtonX,
        &HubSetExecuteButtonX};

    static const EMC_IntSettingDefV1 kExecuteButtonYSettingDef = {
        kModHubSettingExecuteButtonYId,
        "Execute button Y",
        "Vertical offset in pixels from the default anchored position",
        &g_modHubState,
        kExecuteButtonOffsetMin,
        kExecuteButtonOffsetMax,
        1,
        &HubGetExecuteButtonY,
        &HubSetExecuteButtonY};

    static const EMC_ActionRowDefV1 kResetExecuteButtonDefaultsActionDef = {
        kModHubActionResetExecuteButtonDefaultsId,
        "Reset Execute button defaults",
        "Restore Execute button defaults to 310 width, 56 height, X 0, and Y 0",
        &g_modHubState,
        EMC_ACTION_FORCE_REFRESH,
        &HubResetExecuteButtonDefaults};

    static const emc::ModHubClientSettingRowV1 kSettingRows[] = {
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kEnabledSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kExecuteKillSoundSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kDebugExecuteLoggingSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kIgnoreExecuteAllianceCheckSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &kExecuteButtonWidthSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &kExecuteButtonHeightSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &kExecuteButtonXSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &kExecuteButtonYSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION, &kResetExecuteButtonDefaultsActionDef }
    };

    static const emc::ModHubClientTableRegistrationV1 kRegistration = {
        &kModDescriptor,
        kSettingRows,
        static_cast<uint32_t>(sizeof(kSettingRows) / sizeof(kSettingRows[0]))};

    return &kRegistration;
}

static void ConfigureModHubClient()
{
    if (g_modHubClientConfigured)
    {
        return;
    }

    emc::ModHubClient::Config config;
    config.table_registration = GetModHubTableRegistration();
    g_modHubClient.SetConfig(config);
    g_modHubClientConfigured = true;
}

static void LogModHubAttachFailure(const char* phase, EMC_Result result, const char* reason)
{
    std::stringstream line;
    line << "Loot-Scoot-Execute WARN: event=loot_scoot_execute_hub_attach_failed"
         << " phase=" << SafeHubLogValue(phase)
         << " result=" << result
         << " reason=" << SafeHubLogValue(reason);
    ErrorLog(line.str().c_str());
}

static void LogModHubFallback(const char* reason, EMC_Result result)
{
    std::stringstream line;
    line << "Loot-Scoot-Execute WARN: event=loot_scoot_execute_hub_fallback"
         << " reason=" << SafeHubLogValue(reason)
         << " result=" << result
         << " use_hub_ui=0";
    ErrorLog(line.str().c_str());
}

static void ModHub_OnPluginStart()
{
    ConfigureModHubClient();
    g_modHubLoggedRegisterFallback = false;

    const emc::ModHubClient::AttemptResult result = g_modHubClient.OnStartup();
    if (result == emc::ModHubClient::ATTACH_FAILED)
    {
        LogModHubAttachFailure("startup", g_modHubClient.LastAttemptFailureResult(), "get_api_failed");
    }
    else if (result == emc::ModHubClient::REGISTRATION_FAILED)
    {
        if (!g_modHubLoggedRegisterFallback)
        {
            g_modHubLoggedRegisterFallback = true;
            LogModHubFallback("register_mod_or_setting_failed", g_modHubClient.LastAttemptFailureResult());
        }
    }
    else if (result == emc::ModHubClient::INVALID_CONFIGURATION)
    {
        LogModHubFallback("invalid_client_configuration", g_modHubClient.LastAttemptFailureResult());
    }
}

static bool ModHub_UseHubUi()
{
    if (!g_modHubClientConfigured)
    {
        return false;
    }

    return g_modHubClient.UseHubUi();
}

static bool ModHub_IsAttachRetryPending()
{
    if (!g_modHubClientConfigured)
    {
        return false;
    }

    return g_modHubClient.IsAttachRetryPending();
}

static EMC_Result ModHub_LastAttachFailureResult()
{
    if (!g_modHubClientConfigured)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return g_modHubClient.LastAttemptFailureResult();
}
