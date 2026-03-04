#include "emc/mod_hub_client.h"

#include <Windows.h>

namespace
{
const char* kDefaultHubModuleName = "Emkejs-Mod-Core.dll";
const char* kDefaultGetApiExportName = "EMC_ModHub_GetApi";

typedef EMC_Result(__cdecl* FnModHubGetApi)(
    uint32_t requested_version,
    uint32_t caller_api_size,
    const EMC_HubApiV1** out_api,
    uint32_t* out_api_size);

EMC_Result DefaultGetApi(
    uint32_t requested_version,
    uint32_t caller_api_size,
    const EMC_HubApiV1** out_api,
    uint32_t* out_api_size)
{
    if (out_api == 0 || out_api_size == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_api = 0;
    *out_api_size = 0u;

    const HMODULE hub_module = GetModuleHandleA(kDefaultHubModuleName);
    if (hub_module == 0)
    {
        return EMC_ERR_NOT_FOUND;
    }

    const FnModHubGetApi get_api =
        reinterpret_cast<FnModHubGetApi>(GetProcAddress(hub_module, kDefaultGetApiExportName));
    if (get_api == 0)
    {
        return EMC_ERR_NOT_FOUND;
    }

    return get_api(requested_version, caller_api_size, out_api, out_api_size);
}

EMC_Result RegisterSettingsRow(
    const EMC_HubApiV1* api,
    EMC_ModHandle mod_handle,
    const emc::ModHubClientSettingRowV1* row)
{
    if (api == 0 || row == 0 || row->def == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    switch (row->kind)
    {
    case emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL:
        if (api->register_bool_setting == 0)
        {
            return EMC_ERR_INTERNAL;
        }
        return api->register_bool_setting(mod_handle, static_cast<const EMC_BoolSettingDefV1*>(row->def));

    case emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND:
        if (api->register_keybind_setting == 0)
        {
            return EMC_ERR_INTERNAL;
        }
        return api->register_keybind_setting(mod_handle, static_cast<const EMC_KeybindSettingDefV1*>(row->def));

    case emc::MOD_HUB_CLIENT_SETTING_KIND_INT:
        if (api->register_int_setting == 0)
        {
            return EMC_ERR_INTERNAL;
        }
        return api->register_int_setting(mod_handle, static_cast<const EMC_IntSettingDefV1*>(row->def));

    case emc::MOD_HUB_CLIENT_SETTING_KIND_FLOAT:
        if (api->register_float_setting == 0)
        {
            return EMC_ERR_INTERNAL;
        }
        return api->register_float_setting(mod_handle, static_cast<const EMC_FloatSettingDefV1*>(row->def));

    case emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION:
        if (api->register_action_row == 0)
        {
            return EMC_ERR_INTERNAL;
        }
        return api->register_action_row(mod_handle, static_cast<const EMC_ActionRowDefV1*>(row->def));

    default:
        return EMC_ERR_INVALID_ARGUMENT;
    }
}
}

namespace emc
{
EMC_Result RegisterSettingsTableV1(
    const EMC_HubApiV1* api,
    const ModHubClientTableRegistrationV1* table_registration)
{
    if (api == 0 || table_registration == 0 || table_registration->mod_desc == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (api->register_mod == 0)
    {
        return EMC_ERR_INTERNAL;
    }

    if (table_registration->row_count > 0u && table_registration->rows == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EMC_ModHandle mod_handle = 0;
    EMC_Result result = api->register_mod(table_registration->mod_desc, &mod_handle);
    if (result != EMC_OK)
    {
        return result;
    }

    if (mod_handle == 0)
    {
        return EMC_ERR_INTERNAL;
    }

    for (uint32_t row_index = 0u; row_index < table_registration->row_count; ++row_index)
    {
        const ModHubClientSettingRowV1* row = &table_registration->rows[row_index];
        result = RegisterSettingsRow(api, mod_handle, row);
        if (result != EMC_OK)
        {
            return result;
        }
    }

    return EMC_OK;
}

ModHubClient::Config::Config()
    : get_api_fn(0)
    , register_fn(0)
    , register_user_data(0)
    , table_registration(0)
    , should_force_attach_failure_fn(0)
    , attach_failure_user_data(0)
{
}

ModHubClient::ModHubClient()
{
    Reset();
}

ModHubClient::ModHubClient(const Config& config)
    : config_(config)
{
    Reset();
}

void ModHubClient::SetConfig(const Config& config)
{
    config_ = config;
}

const ModHubClient::Config& ModHubClient::GetConfig() const
{
    return config_;
}

void ModHubClient::Reset()
{
    use_hub_ui_ = false;
    attach_retry_pending_ = false;
    attach_retry_attempted_ = false;
    last_attempt_failure_result_ = EMC_OK;
}

ModHubClient::AttemptResult ModHubClient::OnStartup()
{
    Reset();

    const AttemptResult result = AttemptAttachAndRegister(false);
    if (result == ATTACH_FAILED)
    {
        attach_retry_pending_ = true;
    }

    return result;
}

ModHubClient::AttemptResult ModHubClient::OnOptionsWindowInit()
{
    if (!attach_retry_pending_ || attach_retry_attempted_)
    {
        return use_hub_ui_ ? ATTACH_SUCCESS : ATTACH_FAILED;
    }

    attach_retry_attempted_ = true;
    attach_retry_pending_ = false;
    return AttemptAttachAndRegister(true);
}

bool ModHubClient::UseHubUi() const
{
    return use_hub_ui_;
}

bool ModHubClient::IsAttachRetryPending() const
{
    return attach_retry_pending_;
}

bool ModHubClient::HasAttachRetryAttempted() const
{
    return attach_retry_attempted_;
}

EMC_Result ModHubClient::LastAttemptFailureResult() const
{
    return last_attempt_failure_result_;
}

ModHubClient::AttemptResult ModHubClient::AttemptAttachAndRegister(bool is_retry)
{
    if (config_.register_fn == 0 && config_.table_registration == 0)
    {
        use_hub_ui_ = false;
        last_attempt_failure_result_ = EMC_ERR_INVALID_ARGUMENT;
        return INVALID_CONFIGURATION;
    }

    if (config_.should_force_attach_failure_fn != 0)
    {
        EMC_Result forced_result = EMC_ERR_INTERNAL;
        if (config_.should_force_attach_failure_fn(config_.attach_failure_user_data, is_retry, &forced_result))
        {
            use_hub_ui_ = false;
            last_attempt_failure_result_ = forced_result;
            return ATTACH_FAILED;
        }
    }

    const ModHubClientGetApiFn get_api_fn = config_.get_api_fn != 0 ? config_.get_api_fn : &DefaultGetApi;
    const EMC_HubApiV1* api = 0;
    uint32_t api_size = 0u;
    EMC_Result get_api_result = get_api_fn(
        EMC_HUB_API_VERSION_1,
        EMC_HUB_API_V1_MIN_SIZE,
        &api,
        &api_size);
    if (get_api_result != EMC_OK)
    {
        use_hub_ui_ = false;
        last_attempt_failure_result_ = get_api_result;
        return ATTACH_FAILED;
    }

    if (api == 0 || api_size < EMC_HUB_API_V1_MIN_SIZE)
    {
        use_hub_ui_ = false;
        last_attempt_failure_result_ = EMC_ERR_INTERNAL;
        return ATTACH_FAILED;
    }

    EMC_Result register_result = EMC_ERR_INVALID_ARGUMENT;
    if (config_.table_registration != 0)
    {
        register_result = RegisterSettingsTableV1(api, config_.table_registration);
    }
    else
    {
        register_result = config_.register_fn(api, config_.register_user_data);
    }
    if (register_result != EMC_OK)
    {
        use_hub_ui_ = false;
        last_attempt_failure_result_ = register_result;
        return REGISTRATION_FAILED;
    }

    use_hub_ui_ = true;
    last_attempt_failure_result_ = EMC_OK;
    return ATTACH_SUCCESS;
}
}
