#include "emc/mod_hub_api.h"
#include "emc/mod_hub_client.h"

const char* kModHubNamespaceId = "emkej.qol";
const char* kModHubNamespaceDisplayName = "Emkej QoL";
const char* kModHubModId = "loot_scoot_execute";
const char* kModHubModDisplayName = "Loot-Scoot-Execute";

const char* kModHubSettingEnabledId = "enabled";
const char* kModHubSettingExecuteKillSoundId = "enable_execute_kill_sound";

const uintptr_t kExpectedRvaOptionsInitPlatform1_1_0_65 = 0x003F0120;
const uintptr_t kExpectedRvaOptionsInitPlatform0_1_0_65 = 0x003EFD40;

typedef void (*FnOptionsWindowInit)(void* self);

struct LootScootExecuteModHubState
{
    PluginConfig* config;
};

LootScootExecuteModHubState g_modHubState = { &g_config };
emc::ModHubClient g_modHubClient;
bool g_modHubClientConfigured = false;
bool g_modHubLoggedRegisterFallback = false;
FnOptionsWindowInit g_optionsWindowInitOrig = 0;
bool g_optionsWindowInitHookInstalled = false;

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

static EMC_Result __cdecl HubGetEnabled(void* user_data, int32_t* out_value)
{
    if (!IsHubStateValid(user_data) || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = g_modHubState.config->enabled ? 1 : 0;
    return EMC_OK;
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
    if (!IsHubStateValid(user_data) || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = g_modHubState.config->enableExecuteKillSound ? 1 : 0;
    return EMC_OK;
}

static EMC_Result __cdecl HubSetExecuteKillSound(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    if (!IsHubStateValid(user_data))
    {
        WriteHubErrorText(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const bool next_value = value != 0;
    const bool previous_value = g_modHubState.config->enableExecuteKillSound;
    if (previous_value == next_value)
    {
        return EMC_OK;
    }

    g_modHubState.config->enableExecuteKillSound = next_value;
    if (!SaveConfigState())
    {
        g_modHubState.config->enableExecuteKillSound = previous_value;
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

    static const emc::ModHubClientSettingRowV1 kSettingRows[] = {
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kEnabledSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kExecuteKillSoundSettingDef }
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

static void ModHub_OnOptionsWindowInit()
{
    if (!g_modHubClientConfigured)
    {
        return;
    }

    if (!g_modHubClient.IsAttachRetryPending() || g_modHubClient.HasAttachRetryAttempted())
    {
        return;
    }

    const emc::ModHubClient::AttemptResult result = g_modHubClient.OnOptionsWindowInit();
    if (result == emc::ModHubClient::ATTACH_FAILED)
    {
        LogModHubAttachFailure("options_init_retry", g_modHubClient.LastAttemptFailureResult(), "get_api_failed");
        LogModHubFallback("attach_retry_failed", g_modHubClient.LastAttemptFailureResult());
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

static void OptionsWindowInit_mod_hub_hook(void* self)
{
    if (g_optionsWindowInitOrig != 0)
    {
        g_optionsWindowInitOrig(self);
    }

    ModHub_OnOptionsWindowInit();
}

static bool TryResolveOptionsWindowInitAddress(unsigned int platform, const std::string& version, uintptr_t* out_address)
{
    if (out_address == 0)
    {
        return false;
    }

    *out_address = 0;

    const uintptr_t base_addr = reinterpret_cast<uintptr_t>(GetModuleHandleA(0));
    if (base_addr == 0)
    {
        return false;
    }

    if (version == "1.0.65")
    {
        if (platform == 1u)
        {
            *out_address = base_addr + kExpectedRvaOptionsInitPlatform1_1_0_65;
            return true;
        }

        if (platform == 0u)
        {
            *out_address = base_addr + kExpectedRvaOptionsInitPlatform0_1_0_65;
            return true;
        }
    }

    return false;
}

static bool InstallModHubOptionsWindowInitHook(unsigned int platform, const std::string& version)
{
    if (g_optionsWindowInitHookInstalled)
    {
        return true;
    }

    uintptr_t options_init_addr = 0;
    if (!TryResolveOptionsWindowInitAddress(platform, version, &options_init_addr) || options_init_addr == 0)
    {
        ErrorLog("Loot-Scoot-Execute WARN: unsupported runtime for Mod Hub options-init retry hook; startup attach only");
        return false;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        reinterpret_cast<void*>(options_init_addr),
        OptionsWindowInit_mod_hub_hook,
        &g_optionsWindowInitOrig))
    {
        ErrorLog("Loot-Scoot-Execute WARN: could not hook options init; Mod Hub retry-on-options-init disabled");
        return false;
    }

    g_optionsWindowInitHookInstalled = true;
    PluginLog("Loot-Scoot-Execute INFO: Mod Hub options-init hook verification passed");
    return true;
}
