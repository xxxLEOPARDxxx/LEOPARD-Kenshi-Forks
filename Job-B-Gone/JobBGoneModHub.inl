#include "emc/mod_hub_client.h"
#include "emc/mod_hub_consumer_helpers.h"

namespace
{
#define JBG_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

const char* kHubNamespaceId = "emkej.qol";
const char* kHubNamespaceDisplayName = "Emkej QoL";
const char* kHubModId = "job_b_gone";
const char* kHubModDisplayName = "Job-B-Gone";

struct HubBoolSettingDescriptor
{
    const char* settingId;
    const char* label;
    const char* description;
    bool PluginConfig::*field;
};

EMC_Result __cdecl GetBoolSettingValue(void* user_data, int32_t* out_value);
EMC_Result __cdecl SetBoolSettingValue(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size);

emc::ModHubClient g_modHubClient;
bool g_modHubClientConfigured = false;
bool g_modHubRowsInitialized = false;

HubBoolSettingDescriptor g_boolSettingDescriptors[] = {
    { "enabled", "Enabled", "Enable the Job-B-Gone panel and actions", &PluginConfig::enabled },
    { "enable_delete_all_jobs_top_actions", "Top actions", "Show the top scoped delete-all buttons", &PluginConfig::enableDeleteAllJobsTopActions },
    { "job_b_gone_panel_collapsed", "Start collapsed", "Persist the panel collapsed state", &PluginConfig::jobBGonePanelCollapsed },
    { "hide_panel_during_character_creation", "Hide in creation", "Hide the panel while character creation or editing is open", &PluginConfig::hidePanelDuringCharacterCreation },
    { "hide_panel_during_inventory_open", "Hide in inventory", "Hide the panel while inventory or trade windows are open", &PluginConfig::hidePanelDuringInventoryOpen },
    { "hide_panel_during_character_interaction", "Hide in dialogue", "Hide the panel while characters are in dialogue or interaction", &PluginConfig::hidePanelDuringCharacterInteraction },
    { "debug_log_transitions", "Debug transitions", "Enable transition and visibility diagnostics", &PluginConfig::debugLogTransitions },
    { "log_selected_member_job_snapshot", "Debug snapshots", "Log selected-member job snapshots for diagnostics", &PluginConfig::logSelectedMemberJobSnapshot }
};

enum
{
    kHubBoolSettingCount = static_cast<int>(JBG_ARRAY_COUNT(g_boolSettingDescriptors))
};

EMC_BoolSettingDefV1 g_boolSettingDefs[kHubBoolSettingCount];
emc::ModHubClientSettingRowV1 g_modHubRows[kHubBoolSettingCount];

const EMC_ModDescriptorV1 kModHubDescriptor = {
    kHubNamespaceId,
    kHubNamespaceDisplayName,
    kHubModId,
    kHubModDisplayName,
    &g_modHubClient
};

emc::ModHubClientTableRegistrationV1 g_modHubRegistration = {
    &kModHubDescriptor,
    g_modHubRows,
    0u
};

void InitializeSettingDefinitions()
{
    if (g_modHubRowsInitialized)
    {
        return;
    }

    for (size_t i = 0u; i < JBG_ARRAY_COUNT(g_boolSettingDescriptors); ++i)
    {
        g_boolSettingDefs[i].setting_id = g_boolSettingDescriptors[i].settingId;
        g_boolSettingDefs[i].label = g_boolSettingDescriptors[i].label;
        g_boolSettingDefs[i].description = g_boolSettingDescriptors[i].description;
        g_boolSettingDefs[i].user_data = &g_boolSettingDescriptors[i];
        g_boolSettingDefs[i].get_value = &GetBoolSettingValue;
        g_boolSettingDefs[i].set_value = &SetBoolSettingValue;

        g_modHubRows[i].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL;
        g_modHubRows[i].def = &g_boolSettingDefs[i];
    }

    g_modHubRegistration.row_count = static_cast<uint32_t>(JBG_ARRAY_COUNT(g_modHubRows));
    g_modHubRowsInitialized = true;
}

EMC_Result ApplyHubConfigUpdate(const PluginConfig& updated, char* err_buf, uint32_t err_buf_size)
{
    const PluginConfig previous = g_config;
    const bool previousCollapsed = g_jobBGonePanelCollapsed;
    const bool previousNeedsWriteBack = g_configNeedsWriteBack;

    g_config = updated;
    g_jobBGonePanelCollapsed = updated.jobBGonePanelCollapsed;

    if (!SaveConfigState())
    {
        g_config = previous;
        g_jobBGonePanelCollapsed = previousCollapsed;
        g_configNeedsWriteBack = previousNeedsWriteBack;
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "persist_failed");
        return EMC_ERR_INTERNAL;
    }

    g_configNeedsWriteBack = false;

    if (!updated.enabled)
    {
        DestroySelectedMemberJobPanelButton();
    }
    else if (!updated.enableDeleteAllJobsTopActions || updated.jobBGonePanelCollapsed)
    {
        HideConfirmationOverlay();
    }

    emc::consumer::WriteErrorMessage(err_buf, err_buf_size, 0);
    return EMC_OK;
}

EMC_Result __cdecl GetBoolSettingValue(void* user_data, int32_t* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HubBoolSettingDescriptor* descriptor = static_cast<const HubBoolSettingDescriptor*>(user_data);
    *out_value = (g_config.*(descriptor->field)) ? 1 : 0;
    return EMC_OK;
}

EMC_Result __cdecl SetBoolSettingValue(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    if (user_data == 0)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_setting_context");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value != 0 && value != 1)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_bool");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HubBoolSettingDescriptor* descriptor = static_cast<const HubBoolSettingDescriptor*>(user_data);
    PluginConfig updated = g_config;
    updated.*(descriptor->field) = value != 0;
    return ApplyHubConfigUpdate(updated, err_buf, err_buf_size);
}

void LogModHubFallback(const char* reason)
{
    const bool modHubMissing = g_modHubClient.LastAttemptFailureResult() == EMC_ERR_NOT_FOUND;
    std::stringstream line;
    line << (modHubMissing ? "Job-B-Gone INFO: mod_hub_fallback" : "Job-B-Gone WARN: mod_hub_fallback")
         << " reason=" << (reason != 0 ? reason : "unknown")
         << " result=" << g_modHubClient.LastAttemptFailureResult()
         << " use_hub_ui=0";

    if (modHubMissing)
    {
        DebugLog(line.str().c_str());
        return;
    }

    ErrorLog(line.str().c_str());
}

void EnsureModHubClientConfigured()
{
    if (g_modHubClientConfigured)
    {
        return;
    }

    InitializeSettingDefinitions();

    emc::ModHubClient::Config config;
    config.table_registration = &g_modHubRegistration;
    g_modHubClient.SetConfig(config);
    g_modHubClientConfigured = true;
}

void StartModHubClient()
{
    EnsureModHubClientConfigured();

    const emc::ModHubClient::AttemptResult result = g_modHubClient.OnStartup();
    if (result == emc::ModHubClient::ATTACH_SUCCESS)
    {
        DebugLog("Job-B-Gone INFO: mod_hub_attached use_hub_ui=1");
        return;
    }

    if (result == emc::ModHubClient::ATTACH_FAILED)
    {
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

#undef JBG_ARRAY_COUNT
} // namespace
