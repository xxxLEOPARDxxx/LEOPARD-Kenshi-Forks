#include "emc/mod_hub_client.h"

#if defined(_WIN32)
#include <Windows.h>
#include <TlHelp32.h>
#endif

#include <cstdio>

namespace
{
#if defined(EMC_ENABLE_TEST_EXPORTS)
const int32_t kDefaultLookupModeAuto = 0;
const int32_t kDefaultLookupModeAliasOnly = 1;
const int32_t kDefaultLookupModeMissing = 2;
#endif

#if defined(_WIN32)
const char* kDefaultGetApiAliasExportNames[] = {
    EMC_MOD_HUB_GET_API_COMPAT_EXPORT_NAME };

struct DefaultGetApiResolverState
{
    emc::ModHubClientGetApiFn fn;
    bool initialized;
#if defined(EMC_ENABLE_TEST_EXPORTS)
    int32_t lookup_mode;
#endif
};

DefaultGetApiResolverState g_default_get_api_resolver_state = {
    0,
    false,
#if defined(EMC_ENABLE_TEST_EXPORTS)
    kDefaultLookupModeAuto
#endif
};

void ResetDefaultGetApiResolverCache()
{
    g_default_get_api_resolver_state.fn = 0;
    g_default_get_api_resolver_state.initialized = false;
}

emc::ModHubClientGetApiFn ResolveGetApiFromModule(HMODULE module, bool allow_canonical, bool allow_alias)
{
    if (module == 0)
    {
        return 0;
    }

    if (allow_canonical)
    {
        FARPROC proc = GetProcAddress(module, EMC_MOD_HUB_GET_API_EXPORT_NAME);
        if (proc != 0)
        {
            return reinterpret_cast<emc::ModHubClientGetApiFn>(proc);
        }
    }

    if (!allow_alias)
    {
        return 0;
    }

    for (size_t alias_index = 0u;
         alias_index < sizeof(kDefaultGetApiAliasExportNames) / sizeof(kDefaultGetApiAliasExportNames[0]);
         ++alias_index)
    {
        FARPROC proc = GetProcAddress(module, kDefaultGetApiAliasExportNames[alias_index]);
        if (proc != 0)
        {
            return reinterpret_cast<emc::ModHubClientGetApiFn>(proc);
        }
    }

    return 0;
}

emc::ModHubClientGetApiFn ResolveGetApiFromLoadedModules(HMODULE skip_module, bool allow_canonical, bool allow_alias)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    MODULEENTRY32 module_entry;
    module_entry.dwSize = sizeof(module_entry);
    if (!Module32First(snapshot, &module_entry))
    {
        CloseHandle(snapshot);
        return 0;
    }

    do
    {
        HMODULE module = module_entry.hModule;
        if (module == skip_module)
        {
            continue;
        }

        emc::ModHubClientGetApiFn fn = ResolveGetApiFromModule(module, allow_canonical, allow_alias);
        if (fn != 0)
        {
            CloseHandle(snapshot);
            return fn;
        }
    } while (Module32Next(snapshot, &module_entry));

    CloseHandle(snapshot);
    return 0;
}

emc::ModHubClientGetApiFn ResolveDefaultGetApiFn()
{
    bool allow_canonical = true;
    bool allow_alias = true;

#if defined(EMC_ENABLE_TEST_EXPORTS)
    if (g_default_get_api_resolver_state.lookup_mode == kDefaultLookupModeAliasOnly)
    {
        allow_canonical = false;
        allow_alias = true;
    }
    else if (g_default_get_api_resolver_state.lookup_mode == kDefaultLookupModeMissing)
    {
        allow_canonical = false;
        allow_alias = false;
    }
#endif

    if (!allow_canonical && !allow_alias)
    {
        return 0;
    }

    HMODULE self_module = 0;
    if (GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&ResolveDefaultGetApiFn),
            &self_module) != 0)
    {
        emc::ModHubClientGetApiFn fn = ResolveGetApiFromModule(self_module, allow_canonical, allow_alias);
        if (fn != 0)
        {
            return fn;
        }
    }

    return ResolveGetApiFromLoadedModules(self_module, allow_canonical, allow_alias);
}

emc::ModHubClientGetApiFn GetDefaultGetApiFn()
{
    if (!g_default_get_api_resolver_state.initialized)
    {
        g_default_get_api_resolver_state.fn = ResolveDefaultGetApiFn();
        g_default_get_api_resolver_state.initialized = true;
    }

    return g_default_get_api_resolver_state.fn;
}
#endif

bool HasOptionsWindowInitObserverSupport(const EMC_HubApiV1* api, uint32_t api_size)
{
    return api != 0
        && api_size >= EMC_HUB_API_V1_OPTIONS_WINDOW_INIT_OBSERVER_MIN_SIZE
        && api->register_options_window_init_observer != 0
        && api->unregister_options_window_init_observer != 0;
}

bool HasIntSettingV2Support(const EMC_HubApiV1* api, uint32_t api_size)
{
    return api != 0
        && api_size >= EMC_HUB_API_V1_INT_SETTING_V2_MIN_SIZE
        && api->api_size >= EMC_HUB_API_V1_INT_SETTING_V2_MIN_SIZE
        && api->register_int_setting_v2 != 0;
}

bool HasSelectSettingSupport(const EMC_HubApiV1* api, uint32_t api_size)
{
    return api != 0
        && api_size >= EMC_HUB_API_V1_SELECT_SETTING_MIN_SIZE
        && api->api_size >= EMC_HUB_API_V1_SELECT_SETTING_MIN_SIZE
        && api->register_select_setting != 0;
}

bool HasTextSettingSupport(const EMC_HubApiV1* api, uint32_t api_size)
{
    return api != 0
        && api_size >= EMC_HUB_API_V1_TEXT_SETTING_MIN_SIZE
        && api->api_size >= EMC_HUB_API_V1_TEXT_SETTING_MIN_SIZE
        && api->register_text_setting != 0;
}

bool HasColorSettingSupport(const EMC_HubApiV1* api, uint32_t api_size)
{
    return api != 0
        && api_size >= EMC_HUB_API_V1_COLOR_SETTING_MIN_SIZE
        && api->api_size >= EMC_HUB_API_V1_COLOR_SETTING_MIN_SIZE
        && api->register_color_setting != 0;
}

bool HasSettingSectionSupport(const EMC_HubApiV1* api, uint32_t api_size)
{
    return api != 0
        && api_size >= EMC_HUB_API_V1_SETTING_SECTION_MIN_SIZE
        && api->api_size >= EMC_HUB_API_V1_SETTING_SECTION_MIN_SIZE
        && api->register_setting_section != 0;
}

bool HasBoolSettingV2Support(const EMC_HubApiV1* api, uint32_t api_size)
{
    return api != 0
        && api_size >= EMC_HUB_API_V1_BOOL_SETTING_V2_MIN_SIZE
        && api->api_size >= EMC_HUB_API_V1_BOOL_SETTING_V2_MIN_SIZE
        && api->register_bool_setting_v2 != 0;
}

bool HasKeybindSettingV2Support(const EMC_HubApiV1* api, uint32_t api_size)
{
    return api != 0
        && api_size >= EMC_HUB_API_V1_KEYBIND_SETTING_V2_MIN_SIZE
        && api->api_size >= EMC_HUB_API_V1_KEYBIND_SETTING_V2_MIN_SIZE
        && api->register_keybind_setting_v2 != 0;
}

bool HasSelectSettingV2Support(const EMC_HubApiV1* api, uint32_t api_size)
{
    return api != 0
        && api_size >= EMC_HUB_API_V1_SELECT_SETTING_V2_MIN_SIZE
        && api->api_size >= EMC_HUB_API_V1_SELECT_SETTING_V2_MIN_SIZE
        && api->register_select_setting_v2 != 0;
}

bool HasTextSettingV2Support(const EMC_HubApiV1* api, uint32_t api_size)
{
    return api != 0
        && api_size >= EMC_HUB_API_V1_TEXT_SETTING_V2_MIN_SIZE
        && api->api_size >= EMC_HUB_API_V1_TEXT_SETTING_V2_MIN_SIZE
        && api->register_text_setting_v2 != 0;
}

bool HasActionRowV2Support(const EMC_HubApiV1* api, uint32_t api_size)
{
    return api != 0
        && api_size >= EMC_HUB_API_V1_ACTION_ROW_V2_MIN_SIZE
        && api->api_size >= EMC_HUB_API_V1_ACTION_ROW_V2_MIN_SIZE
        && api->register_action_row_v2 != 0;
}

uint32_t ResolveExpectedSdkApiVersion(const emc::ModHubClient::Config& config)
{
    return config.expected_sdk_api_version != 0u
        ? config.expected_sdk_api_version
        : EMC_HUB_API_VERSION_1;
}

uint32_t ResolveExpectedSdkMinApiSize(const emc::ModHubClient::Config& config)
{
    return config.expected_sdk_min_api_size != 0u
        ? config.expected_sdk_min_api_size
        : EMC_HUB_API_V1_OPTIONS_WINDOW_INIT_OBSERVER_MIN_SIZE;
}

bool IsSdkStampDriftDetected(
    uint32_t expected_api_version,
    uint32_t expected_min_api_size,
    const EMC_HubApiV1* api,
    uint32_t api_size)
{
    if (api == 0)
    {
        return false;
    }

    return api->api_version != expected_api_version
        || api_size < expected_min_api_size
        || api->api_size < expected_min_api_size
        || api->api_size != api_size;
}

void EmitSdkStampWarning(
    uint32_t expected_api_version,
    uint32_t expected_min_api_size,
    uint32_t runtime_api_version,
    uint32_t runtime_api_size,
    uint32_t runtime_struct_api_size)
{
    char message[320];
    message[0] = '\0';

#if defined(_MSC_VER) && _MSC_VER < 1900
    _snprintf_s(
        message,
        sizeof(message),
        _TRUNCATE,
        "Emkejs-Mod-Core: Mod Hub SDK stamp drift detected (expected version=%u min_api_size=%u, runtime version=%u out_api_size=%u api_struct_size=%u).",
        (unsigned int)expected_api_version,
        (unsigned int)expected_min_api_size,
        (unsigned int)runtime_api_version,
        (unsigned int)runtime_api_size,
        (unsigned int)runtime_struct_api_size);
#else
    std::snprintf(
        message,
        sizeof(message),
        "Emkejs-Mod-Core: Mod Hub SDK stamp drift detected (expected version=%u min_api_size=%u, runtime version=%u out_api_size=%u api_struct_size=%u).",
        (unsigned int)expected_api_version,
        (unsigned int)expected_min_api_size,
        (unsigned int)runtime_api_version,
        (unsigned int)runtime_api_size,
        (unsigned int)runtime_struct_api_size);
#endif

    if (message[0] == '\0')
    {
        return;
    }

#if defined(_WIN32)
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
#else
    std::fprintf(stderr, "%s\n", message);
#endif
}

EMC_Result DefaultGetApi(
    uint32_t requested_version,
    uint32_t caller_api_size,
    const EMC_HubApiV1** out_api,
    uint32_t* out_api_size)
{
#if defined(_WIN32)
    if (out_api == 0 || out_api_size == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_api = 0;
    *out_api_size = 0u;

    const emc::ModHubClientGetApiFn get_api_fn = GetDefaultGetApiFn();
    if (get_api_fn == 0)
    {
        return EMC_ERR_NOT_FOUND;
    }

    return get_api_fn(requested_version, caller_api_size, out_api, out_api_size);
#else
    return EMC_ModHub_GetApi(requested_version, caller_api_size, out_api, out_api_size);
#endif
}

EMC_Result RegisterSettingsRow(
    const EMC_HubApiV1* api,
    uint32_t api_size,
    EMC_ModHandle mod_handle,
    const emc::ModHubClientSettingRowV1* row)
{
    if (api == 0 || row == 0 || row->def == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EMC_Result result = EMC_ERR_INVALID_ARGUMENT;
    switch (row->kind)
    {
    case emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL:
        if (api->register_bool_setting == 0)
        {
            return EMC_ERR_INTERNAL;
        }
        result = api->register_bool_setting(mod_handle, static_cast<const EMC_BoolSettingDefV1*>(row->def));
        break;

    case emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2:
        if (!HasBoolSettingV2Support(api, api_size))
        {
            return EMC_ERR_API_SIZE_MISMATCH;
        }
        result = api->register_bool_setting_v2(mod_handle, static_cast<const EMC_BoolSettingDefV2*>(row->def));
        break;

    case emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND:
        if (api->register_keybind_setting == 0)
        {
            return EMC_ERR_INTERNAL;
        }
        result = api->register_keybind_setting(mod_handle, static_cast<const EMC_KeybindSettingDefV1*>(row->def));
        break;

    case emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND_V2:
        if (!HasKeybindSettingV2Support(api, api_size))
        {
            return EMC_ERR_API_SIZE_MISMATCH;
        }
        result = api->register_keybind_setting_v2(mod_handle, static_cast<const EMC_KeybindSettingDefV2*>(row->def));
        break;

    case emc::MOD_HUB_CLIENT_SETTING_KIND_INT:
        if (api->register_int_setting == 0)
        {
            return EMC_ERR_INTERNAL;
        }
        result = api->register_int_setting(mod_handle, static_cast<const EMC_IntSettingDefV1*>(row->def));
        break;

    case emc::MOD_HUB_CLIENT_SETTING_KIND_INT_V2:
        if (!HasIntSettingV2Support(api, api_size))
        {
            return EMC_ERR_API_SIZE_MISMATCH;
        }
        result = api->register_int_setting_v2(mod_handle, static_cast<const EMC_IntSettingDefV2*>(row->def));
        break;

    case emc::MOD_HUB_CLIENT_SETTING_KIND_FLOAT:
        if (api->register_float_setting == 0)
        {
            return EMC_ERR_INTERNAL;
        }
        result = api->register_float_setting(mod_handle, static_cast<const EMC_FloatSettingDefV1*>(row->def));
        break;

    case emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION:
        if (api->register_action_row == 0)
        {
            return EMC_ERR_INTERNAL;
        }
        result = api->register_action_row(mod_handle, static_cast<const EMC_ActionRowDefV1*>(row->def));
        break;

    case emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION_V2:
        if (!HasActionRowV2Support(api, api_size))
        {
            return EMC_ERR_API_SIZE_MISMATCH;
        }
        result = api->register_action_row_v2(mod_handle, static_cast<const EMC_ActionRowDefV2*>(row->def));
        break;

    case emc::MOD_HUB_CLIENT_SETTING_KIND_SELECT:
        if (!HasSelectSettingSupport(api, api_size))
        {
            return EMC_ERR_API_SIZE_MISMATCH;
        }
        result = api->register_select_setting(mod_handle, static_cast<const EMC_SelectSettingDefV1*>(row->def));
        break;

    case emc::MOD_HUB_CLIENT_SETTING_KIND_SELECT_V2:
        if (!HasSelectSettingV2Support(api, api_size))
        {
            return EMC_ERR_API_SIZE_MISMATCH;
        }
        result = api->register_select_setting_v2(mod_handle, static_cast<const EMC_SelectSettingDefV2*>(row->def));
        break;

    case emc::MOD_HUB_CLIENT_SETTING_KIND_TEXT:
        if (!HasTextSettingSupport(api, api_size))
        {
            return EMC_ERR_API_SIZE_MISMATCH;
        }
        result = api->register_text_setting(mod_handle, static_cast<const EMC_TextSettingDefV1*>(row->def));
        break;

    case emc::MOD_HUB_CLIENT_SETTING_KIND_TEXT_V2:
        if (!HasTextSettingV2Support(api, api_size))
        {
            return EMC_ERR_API_SIZE_MISMATCH;
        }
        result = api->register_text_setting_v2(mod_handle, static_cast<const EMC_TextSettingDefV2*>(row->def));
        break;

    case emc::MOD_HUB_CLIENT_SETTING_KIND_COLOR:
        if (!HasColorSettingSupport(api, api_size))
        {
            return EMC_ERR_API_SIZE_MISMATCH;
        }
        result = api->register_color_setting(mod_handle, static_cast<const EMC_ColorSettingDefV1*>(row->def));
        break;

    default:
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (result != EMC_OK)
    {
        return result;
    }

    if (row->section_id != nullptr && row->section_id[0] != '\0')
    {
        if (row->setting_id == nullptr || row->setting_id[0] == '\0')
        {
            return EMC_ERR_INVALID_ARGUMENT;
        }

        if (row->section_display_name == nullptr || row->section_display_name[0] == '\0')
        {
            return EMC_ERR_INVALID_ARGUMENT;
        }

        if (!HasSettingSectionSupport(api, api_size))
        {
            return EMC_OK;
        }

        const EMC_SettingSectionDefV1 section_def = {
            row->setting_id,
            row->section_id,
            row->section_display_name
        };
        result = api->register_setting_section(mod_handle, &section_def);
    }

    return result;
}
}

namespace emc
{
EMC_Result RegisterSettingsTableV1(
    const EMC_HubApiV1* api,
    const ModHubClientTableRegistrationV1* table_registration)
{
    return RegisterSettingsTableWithApiSizeV1(api, api != 0 ? api->api_size : 0u, table_registration);
}

EMC_Result RegisterSettingsTableWithApiSizeV1(
    const EMC_HubApiV1* api,
    uint32_t api_size,
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
        result = RegisterSettingsRow(api, api_size, mod_handle, row);
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
    , expected_sdk_api_version(EMC_HUB_API_VERSION_1)
    , expected_sdk_min_api_size(EMC_HUB_API_V1_OPTIONS_WINDOW_INIT_OBSERVER_MIN_SIZE)
{
}

ModHubClient::ModHubClient()
    : observer_api_(0)
    , options_window_init_observer_registered_(false)
    , sdk_stamp_warning_emitted_(false)
{
    Reset();
}

ModHubClient::ModHubClient(const Config& config)
    : config_(config)
    , observer_api_(0)
    , options_window_init_observer_registered_(false)
    , sdk_stamp_warning_emitted_(false)
{
    Reset();
}

ModHubClient::~ModHubClient()
{
    Reset();
}

void ModHubClient::SetConfig(const Config& config)
{
    config_ = config;
    Reset();
}

const ModHubClient::Config& ModHubClient::GetConfig() const
{
    return config_;
}

void ModHubClient::Reset()
{
    UnregisterOptionsWindowInitObserverIfNeeded();
    use_hub_ui_ = false;
    attach_retry_pending_ = false;
    attach_retry_attempted_ = false;
    last_attempt_failure_result_ = EMC_OK;
    observer_api_ = 0;
    options_window_init_observer_registered_ = false;
    sdk_stamp_warning_emitted_ = false;
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
    const AttemptResult result = AttemptAttachAndRegister(true);
    UnregisterOptionsWindowInitObserverIfNeeded();
    return result;
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

void ModHubClient::RegisterOptionsWindowInitObserverIfAvailable(const EMC_HubApiV1* api, uint32_t api_size)
{
    if (options_window_init_observer_registered_ || !HasOptionsWindowInitObserverSupport(api, api_size))
    {
        return;
    }

    if (api->register_options_window_init_observer(&ModHubClient::OnOptionsWindowInitObserverThunk, this) != EMC_OK)
    {
        return;
    }

    observer_api_ = api;
    options_window_init_observer_registered_ = true;
}

void ModHubClient::UnregisterOptionsWindowInitObserverIfNeeded()
{
    if (!options_window_init_observer_registered_ || observer_api_ == 0)
    {
        return;
    }

    if (observer_api_->unregister_options_window_init_observer != 0)
    {
        observer_api_->unregister_options_window_init_observer(&ModHubClient::OnOptionsWindowInitObserverThunk, this);
    }

    observer_api_ = 0;
    options_window_init_observer_registered_ = false;
}

void __cdecl ModHubClient::OnOptionsWindowInitObserverThunk(void* user_data)
{
    if (user_data == 0)
    {
        return;
    }

    ModHubClient* client = static_cast<ModHubClient*>(user_data);
    client->OnOptionsWindowInit();
}

ModHubClient::AttemptResult ModHubClient::AttemptAttachAndRegister(bool is_retry)
{
    if (config_.register_fn == 0 && config_.table_registration == 0)
    {
        use_hub_ui_ = false;
        last_attempt_failure_result_ = EMC_ERR_INVALID_ARGUMENT;
        return INVALID_CONFIGURATION;
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

    if (!sdk_stamp_warning_emitted_)
    {
        const uint32_t expected_api_version = ResolveExpectedSdkApiVersion(config_);
        const uint32_t expected_min_api_size = ResolveExpectedSdkMinApiSize(config_);
        if (IsSdkStampDriftDetected(expected_api_version, expected_min_api_size, api, api_size))
        {
            EmitSdkStampWarning(
                expected_api_version,
                expected_min_api_size,
                api->api_version,
                api_size,
                api->api_size);
            sdk_stamp_warning_emitted_ = true;
        }
    }

    if (!is_retry)
    {
        RegisterOptionsWindowInitObserverIfAvailable(api, api_size);
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

    EMC_Result register_result = EMC_ERR_INVALID_ARGUMENT;
    if (config_.table_registration != 0)
    {
        register_result = RegisterSettingsTableWithApiSizeV1(api, api_size, config_.table_registration);
    }
    else
    {
        register_result = config_.register_fn(api, config_.register_user_data);
    }
    if (register_result != EMC_OK)
    {
        UnregisterOptionsWindowInitObserverIfNeeded();
        use_hub_ui_ = false;
        last_attempt_failure_result_ = register_result;
        return REGISTRATION_FAILED;
    }

    UnregisterOptionsWindowInitObserverIfNeeded();
    use_hub_ui_ = true;
    last_attempt_failure_result_ = EMC_OK;
    return ATTACH_SUCCESS;
}
}

#if defined(EMC_ENABLE_TEST_EXPORTS)
extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_Client_DefaultLookup_SetMode(int32_t mode)
{
#if defined(_WIN32)
    int32_t resolved_mode = kDefaultLookupModeAuto;
    if (mode == kDefaultLookupModeAliasOnly)
    {
        resolved_mode = kDefaultLookupModeAliasOnly;
    }
    else if (mode == kDefaultLookupModeMissing)
    {
        resolved_mode = kDefaultLookupModeMissing;
    }

    g_default_get_api_resolver_state.lookup_mode = resolved_mode;
    ResetDefaultGetApiResolverCache();
#else
    (void)mode;
#endif
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_Client_DefaultLookup_Reset()
{
#if defined(_WIN32)
    g_default_get_api_resolver_state.lookup_mode = kDefaultLookupModeAuto;
    ResetDefaultGetApiResolverCache();
#endif
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_Client_DefaultLookup_CallGetApi(
    uint32_t requested_version,
    uint32_t caller_api_size,
    const EMC_HubApiV1** out_api,
    uint32_t* out_api_size)
{
    return DefaultGetApi(requested_version, caller_api_size, out_api, out_api_size);
}
#endif
