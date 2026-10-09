#include "emc/mod_hub_api.h"
#include "emc/mod_hub_client.h"
#include "hub_commit.h"
#include "hub_menu_bridge.h"
#include "hub_registry.h"
#include "hub_ui.h"
#include "mod_hub_dummy_consumer.h"

#include <Debug.h>
#include <Windows.h>

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <vector>

namespace
{
const char* kLogNone = "none";
const char* kEnvDisableRegistryAttach = "EMC_HUB_DISABLE_REGISTRY_ATTACH";
const char* kGetApiAliasRemovalTarget = EMC_MOD_HUB_GET_API_COMPAT_REMOVAL_TARGET;
int32_t g_get_api_alias_warning_count = 0;

struct OptionsWindowInitObserverEntry
{
    EMC_OptionsWindowInitObserverFn observer_fn;
    void* user_data;
    bool active;
};

std::vector<OptionsWindowInitObserverEntry> g_options_window_init_observers;
bool g_options_window_init_observer_dispatch_installed = false;
bool g_options_window_init_observer_notify_active = false;

#if defined(EMC_ENABLE_TEST_EXPORTS)
bool g_registry_attach_override_set = false;
bool g_registry_attach_override_enabled = true;
#endif

const char* SafeLogValue(const char* value)
{
    if (value == nullptr || value[0] == '\0')
    {
        return kLogNone;
    }

    return value;
}

bool IsEnvTruthy(const char* value)
{
    if (value == nullptr)
    {
        return false;
    }

    return std::strcmp(value, "1") == 0
        || std::strcmp(value, "true") == 0
        || std::strcmp(value, "TRUE") == 0
        || std::strcmp(value, "yes") == 0
        || std::strcmp(value, "YES") == 0
        || std::strcmp(value, "on") == 0
        || std::strcmp(value, "ON") == 0;
}

void CopyStringToBuffer(const char* value, char* out_buffer, uint32_t out_buffer_size)
{
    if (out_buffer == 0 || out_buffer_size == 0u)
    {
        return;
    }

    const char* safe_value = value != 0 ? value : "";
    const size_t max_chars = static_cast<size_t>(out_buffer_size - 1u);
    const size_t value_length = std::strlen(safe_value);
    const size_t copy_length = value_length < max_chars ? value_length : max_chars;
    if (copy_length > 0u)
    {
        std::memcpy(out_buffer, safe_value, copy_length);
    }
    out_buffer[copy_length] = '\0';
}

bool TryGetRowViewById(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    HubUiRowView* out_view)
{
    if (namespace_id == 0 || mod_id == 0 || setting_id == 0 || out_view == 0)
    {
        return false;
    }

    const uint32_t row_count = HubUi_GetRowCount();
    for (uint32_t index = 0u; index < row_count; ++index)
    {
        HubUiRowView row_view = {};
        if (!HubUi_GetRowViewByIndex(index, &row_view))
        {
            continue;
        }

        if (row_view.namespace_id == 0 || row_view.mod_id == 0 || row_view.setting_id == 0)
        {
            continue;
        }

        if (std::strcmp(row_view.namespace_id, namespace_id) == 0
            && std::strcmp(row_view.mod_id, mod_id) == 0
            && std::strcmp(row_view.setting_id, setting_id) == 0)
        {
            *out_view = row_view;
            return true;
        }
    }

    return false;
}

struct CountSettingsForModContext
{
    const char* namespace_id;
    const char* mod_id;
    int32_t count;
};

void __cdecl CountSettingsForModVisitor(
    const HubRegistryNamespaceView* namespace_view,
    const HubRegistryModView* mod_view,
    const HubRegistrySettingView* setting_view,
    void* user_data)
{
    if (namespace_view == 0 || mod_view == 0 || setting_view == 0 || user_data == 0)
    {
        return;
    }

    CountSettingsForModContext* context = static_cast<CountSettingsForModContext*>(user_data);
    if (context->namespace_id == 0 || context->mod_id == 0)
    {
        return;
    }

    if (std::strcmp(namespace_view->namespace_id, context->namespace_id) == 0
        && std::strcmp(mod_view->mod_id, context->mod_id) == 0)
    {
        context->count += 1;
    }
}

bool IsRegistryAttachEnabled()
{
#if defined(EMC_ENABLE_TEST_EXPORTS)
    if (g_registry_attach_override_set)
    {
        return g_registry_attach_override_enabled;
    }
#endif

    return !IsEnvTruthy(std::getenv(kEnvDisableRegistryAttach));
}

bool MatchesOptionsWindowInitObserver(
    const OptionsWindowInitObserverEntry& entry,
    EMC_OptionsWindowInitObserverFn observer_fn,
    void* user_data)
{
    return entry.active
        && entry.observer_fn == observer_fn
        && entry.user_data == user_data;
}

void PruneInactiveOptionsWindowInitObservers()
{
    if (g_options_window_init_observer_notify_active)
    {
        return;
    }

    for (size_t index = 0u; index < g_options_window_init_observers.size();)
    {
        if (g_options_window_init_observers[index].active)
        {
            ++index;
            continue;
        }

        g_options_window_init_observers.erase(g_options_window_init_observers.begin() + index);
    }
}

void NotifyRegisteredOptionsWindowInitObservers()
{
    if (g_options_window_init_observer_notify_active)
    {
        return;
    }

    g_options_window_init_observer_notify_active = true;

    for (size_t index = 0u; index < g_options_window_init_observers.size(); ++index)
    {
        const OptionsWindowInitObserverEntry& entry = g_options_window_init_observers[index];
        if (!entry.active || entry.observer_fn == 0)
        {
            continue;
        }

        entry.observer_fn(entry.user_data);
    }

    g_options_window_init_observer_notify_active = false;
    PruneInactiveOptionsWindowInitObservers();
}

void EnsureOptionsWindowInitObserverDispatchInstalled()
{
    if (g_options_window_init_observer_dispatch_installed)
    {
        return;
    }

    HubMenuBridge_SetOptionsWindowInitObserver(&NotifyRegisteredOptionsWindowInitObservers);
    g_options_window_init_observer_dispatch_installed = true;
}

EMC_Result __cdecl RegisterOptionsWindowInitObserverEntry(
    EMC_OptionsWindowInitObserverFn observer_fn,
    void* user_data)
{
    if (observer_fn == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (g_options_window_init_observer_notify_active)
    {
        return EMC_ERR_CONFLICT;
    }

    EnsureOptionsWindowInitObserverDispatchInstalled();

    for (size_t index = 0u; index < g_options_window_init_observers.size(); ++index)
    {
        if (MatchesOptionsWindowInitObserver(g_options_window_init_observers[index], observer_fn, user_data))
        {
            return EMC_ERR_CONFLICT;
        }
    }

    OptionsWindowInitObserverEntry entry;
    entry.observer_fn = observer_fn;
    entry.user_data = user_data;
    entry.active = true;
    g_options_window_init_observers.push_back(entry);
    return EMC_OK;
}

EMC_Result __cdecl UnregisterOptionsWindowInitObserverEntry(
    EMC_OptionsWindowInitObserverFn observer_fn,
    void* user_data)
{
    if (observer_fn == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    for (size_t index = 0u; index < g_options_window_init_observers.size(); ++index)
    {
        OptionsWindowInitObserverEntry& entry = g_options_window_init_observers[index];
        if (!MatchesOptionsWindowInitObserver(entry, observer_fn, user_data))
        {
            continue;
        }

        if (g_options_window_init_observer_notify_active)
        {
            entry.active = false;
            return EMC_OK;
        }

        g_options_window_init_observers.erase(g_options_window_init_observers.begin() + index);
        return EMC_OK;
    }

    return EMC_ERR_NOT_FOUND;
}

int32_t GetActiveOptionsWindowInitObserverCount()
{
    int32_t count = 0;
    for (size_t index = 0u; index < g_options_window_init_observers.size(); ++index)
    {
        if (g_options_window_init_observers[index].active)
        {
            count += 1;
        }
    }

    return count;
}

#if defined(EMC_ENABLE_TEST_EXPORTS)
void SetRegistryAttachOverride(bool enabled)
{
    g_registry_attach_override_set = true;
    g_registry_attach_override_enabled = enabled;
}

void ClearRegistryAttachOverride()
{
    g_registry_attach_override_set = false;
    g_registry_attach_override_enabled = true;
}
#endif

void LogGetApiAliasDeprecatedOnce(const char* alias_name)
{
    if (g_get_api_alias_warning_count > 0)
    {
        return;
    }

    g_get_api_alias_warning_count = 1;
    (void)alias_name;

    auto ToLowerAscii = [](char value) -> char {
        if (value >= 'A' && value <= 'Z')
        {
            return static_cast<char>(value + ('a' - 'A'));
        }
        return value;
    };

    auto EqualsIgnoreCaseAscii = [&](const char* left, const char* right) -> bool {
        if (left == 0 || right == 0)
        {
            return false;
        }

        size_t index = 0u;
        while (left[index] != '\0' && right[index] != '\0')
        {
            if (ToLowerAscii(left[index]) != ToLowerAscii(right[index]))
            {
                return false;
            }

            ++index;
        }

        return left[index] == '\0' && right[index] == '\0';
    };

    char process_path[MAX_PATH];
    process_path[0] = '\0';
    DWORD process_path_len = GetModuleFileNameA(0, process_path, MAX_PATH);
    if (process_path_len == 0 || process_path_len >= MAX_PATH)
    {
        return;
    }

    const char* process_name = process_path;
    for (DWORD index = 0u; index < process_path_len; ++index)
    {
        if (process_path[index] == '\\' || process_path[index] == '/')
        {
            process_name = process_path + index + 1u;
        }
    }

    const bool is_kenshi_process = EqualsIgnoreCaseAscii(process_name, "kenshi_x64.exe")
        || EqualsIgnoreCaseAscii(process_name, "kenshi.exe");
    if (!is_kenshi_process)
    {
        return;
    }

    DebugLog("event=hub_get_api_alias_deprecated alias=EMC_ModHub_GetApi_v1_compat canonical=EMC_ModHub_GetApi removal_target=v1.2.0");
}

void LogRegistrationRejected(const char* api_name, const char* reason, EMC_Result result, const char* message)
{
    std::ostringstream line;
    line << "event=hub_registration_rejected"
         << " api=" << SafeLogValue(api_name)
         << " reason=" << SafeLogValue(reason)
         << " result=" << result
         << " message=" << SafeLogValue(message);
    ErrorLog(line.str().c_str());
}

EMC_Result RejectWhenRegistryAttachDisabled(const char* api_name)
{
    if (IsRegistryAttachEnabled())
    {
        return EMC_OK;
    }

    LogRegistrationRejected(
        api_name,
        "registry_attach_disabled",
        EMC_ERR_INTERNAL,
        "registry_attach_gated_off");
    return EMC_ERR_INTERNAL;
}

EMC_Result __cdecl RegisterModEntry(const EMC_ModDescriptorV1* desc, EMC_ModHandle* out_handle)
{
    if (out_handle != nullptr)
    {
        *out_handle = nullptr;
    }

    EMC_Result gate_result = RejectWhenRegistryAttachDisabled("register_mod");
    if (gate_result != EMC_OK)
    {
        return gate_result;
    }

    return HubRegistry_RegisterMod(desc, out_handle);
}

EMC_Result __cdecl RegisterBoolSettingEntry(EMC_ModHandle mod, const EMC_BoolSettingDefV1* def)
{
    EMC_Result gate_result = RejectWhenRegistryAttachDisabled("register_bool_setting");
    if (gate_result != EMC_OK)
    {
        return gate_result;
    }

    return HubRegistry_RegisterBoolSetting(mod, def);
}

EMC_Result __cdecl RegisterKeybindSettingEntry(EMC_ModHandle mod, const EMC_KeybindSettingDefV1* def)
{
    EMC_Result gate_result = RejectWhenRegistryAttachDisabled("register_keybind_setting");
    if (gate_result != EMC_OK)
    {
        return gate_result;
    }

    return HubRegistry_RegisterKeybindSetting(mod, def);
}

EMC_Result __cdecl RegisterIntSettingEntry(EMC_ModHandle mod, const EMC_IntSettingDefV1* def)
{
    EMC_Result gate_result = RejectWhenRegistryAttachDisabled("register_int_setting");
    if (gate_result != EMC_OK)
    {
        return gate_result;
    }

    return HubRegistry_RegisterIntSetting(mod, def);
}

EMC_Result __cdecl RegisterFloatSettingEntry(EMC_ModHandle mod, const EMC_FloatSettingDefV1* def)
{
    EMC_Result gate_result = RejectWhenRegistryAttachDisabled("register_float_setting");
    if (gate_result != EMC_OK)
    {
        return gate_result;
    }

    return HubRegistry_RegisterFloatSetting(mod, def);
}

EMC_Result __cdecl RegisterActionRowEntry(EMC_ModHandle mod, const EMC_ActionRowDefV1* def)
{
    EMC_Result gate_result = RejectWhenRegistryAttachDisabled("register_action_row");
    if (gate_result != EMC_OK)
    {
        return gate_result;
    }

    return HubRegistry_RegisterActionRow(mod, def);
}

const EMC_HubApiV1 kHubApiV1 = {
    EMC_HUB_API_VERSION_1,
    (uint32_t)sizeof(EMC_HubApiV1),
    &RegisterModEntry,
    &RegisterBoolSettingEntry,
    &RegisterKeybindSettingEntry,
    &RegisterIntSettingEntry,
    &RegisterFloatSettingEntry,
    &RegisterActionRowEntry,
    &RegisterOptionsWindowInitObserverEntry,
    &UnregisterOptionsWindowInitObserverEntry};

#if defined(EMC_ENABLE_TEST_EXPORTS)
const int32_t kModHubClientTestGetApiModeSuccess = 0;
const int32_t kModHubClientTestGetApiModeReturnFailure = 1;
const int32_t kModHubClientTestGetApiModeNullApi = 2;
const int32_t kModHubClientTestGetApiModeShortApiSize = 3;
const int32_t kModHubClientTestGetApiModeLegacyNoObserver = 4;

const int32_t kModHubClientTestForceAttachNone = 0;
const int32_t kModHubClientTestForceAttachStartupOnly = 1;
const int32_t kModHubClientTestForceAttachAlways = 2;

struct ModHubClientTestState
{
    emc::ModHubClient client;
    int32_t get_api_mode;
    EMC_Result get_api_failure_result;
    EMC_Result register_result;
    int32_t force_attach_failure_mode;
};

ModHubClientTestState g_mod_hub_client_test_state;
bool g_mod_hub_client_test_state_initialized = false;

EMC_Result __cdecl ModHubClientTestGetApi(
    uint32_t requested_version,
    uint32_t caller_api_size,
    const EMC_HubApiV1** out_api,
    uint32_t* out_api_size)
{
    if (out_api == nullptr || out_api_size == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    switch (g_mod_hub_client_test_state.get_api_mode)
    {
    case kModHubClientTestGetApiModeReturnFailure:
        *out_api = nullptr;
        *out_api_size = 0u;
        return g_mod_hub_client_test_state.get_api_failure_result;
    case kModHubClientTestGetApiModeNullApi:
        *out_api = nullptr;
        *out_api_size = (uint32_t)sizeof(EMC_HubApiV1);
        return EMC_OK;
    case kModHubClientTestGetApiModeShortApiSize:
        *out_api = &kHubApiV1;
        *out_api_size = EMC_HUB_API_V1_MIN_SIZE > 0u ? EMC_HUB_API_V1_MIN_SIZE - 1u : 0u;
        return EMC_OK;
    case kModHubClientTestGetApiModeLegacyNoObserver:
        *out_api = &kHubApiV1;
        *out_api_size = EMC_HUB_API_V1_MIN_SIZE;
        return EMC_OK;
    default:
        break;
    }

    return EMC_ModHub_GetApi(requested_version, caller_api_size, out_api, out_api_size);
}

EMC_Result __cdecl ModHubClientTestRegister(const EMC_HubApiV1* api, void* user_data)
{
    (void)api;
    (void)user_data;
    return g_mod_hub_client_test_state.register_result;
}

bool __cdecl ModHubClientTestShouldForceAttachFailure(void* user_data, bool is_retry, EMC_Result* out_result)
{
    (void)user_data;

    bool should_fail = false;
    if (g_mod_hub_client_test_state.force_attach_failure_mode == kModHubClientTestForceAttachAlways)
    {
        should_fail = true;
    }
    else if (g_mod_hub_client_test_state.force_attach_failure_mode == kModHubClientTestForceAttachStartupOnly)
    {
        should_fail = !is_retry;
    }

    if (!should_fail)
    {
        return false;
    }

    if (out_result != nullptr)
    {
        *out_result = EMC_ERR_INTERNAL;
    }
    return true;
}

void ResetModHubClientTestState()
{
    g_mod_hub_client_test_state.get_api_mode = kModHubClientTestGetApiModeSuccess;
    g_mod_hub_client_test_state.get_api_failure_result = EMC_ERR_INTERNAL;
    g_mod_hub_client_test_state.register_result = EMC_OK;
    g_mod_hub_client_test_state.force_attach_failure_mode = kModHubClientTestForceAttachNone;

    emc::ModHubClient::Config config;
    config.get_api_fn = &ModHubClientTestGetApi;
    config.register_fn = &ModHubClientTestRegister;
    config.register_user_data = nullptr;
    config.should_force_attach_failure_fn = &ModHubClientTestShouldForceAttachFailure;
    config.attach_failure_user_data = nullptr;
    g_mod_hub_client_test_state.client.SetConfig(config);
    g_mod_hub_client_test_state.client.Reset();
}

void EnsureModHubClientTestStateInitialized()
{
    if (g_mod_hub_client_test_state_initialized)
    {
        return;
    }

    ResetModHubClientTestState();
    g_mod_hub_client_test_state_initialized = true;
}

const int32_t kModHubClientTableTestModeSuccess = 0;
const int32_t kModHubClientTableTestModeFailBool = 1;
const int32_t kModHubClientTableTestModeFailKeybind = 2;
const int32_t kModHubClientTableTestModeFailInt = 3;
const int32_t kModHubClientTableTestModeFailFloat = 4;
const int32_t kModHubClientTableTestModeFailAction = 5;
const int32_t kModHubClientTableTestModeInvalidRowKind = 6;
const int32_t kModHubClientTableTestModeNullRowDef = 7;

const char* kModHubClientTableTestNamespaceId = "phase8.table_namespace";
const char* kModHubClientTableTestNamespaceDisplayName = "Phase8 Table Namespace";
const char* kModHubClientTableTestModId = "phase8.table_mod";
const char* kModHubClientTableTestModDisplayName = "Phase8 Table Mod";

const char* kModHubClientTableTestBoolSettingId = "phase8_bool";
const char* kModHubClientTableTestKeybindSettingId = "phase8_keybind";
const char* kModHubClientTableTestIntSettingId = "phase8_int";
const char* kModHubClientTableTestFloatSettingId = "phase8_float";
const char* kModHubClientTableTestActionSettingId = "phase8_action";

int32_t g_mod_hub_client_table_test_mod_user_data = 17;
int32_t g_mod_hub_client_table_test_bool_value = 1;
EMC_KeybindValueV1 g_mod_hub_client_table_test_keybind_value = { 42, 0u };
int32_t g_mod_hub_client_table_test_int_value = 10;
float g_mod_hub_client_table_test_float_value = 2.5f;
int32_t g_mod_hub_client_table_test_action_count = 0;

EMC_Result __cdecl ModHubClientTableTestGetBool(void* user_data, int32_t* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = *static_cast<int32_t*>(user_data);
    return EMC_OK;
}

EMC_Result __cdecl ModHubClientTableTestSetBool(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    (void)err_buf;
    (void)err_buf_size;
    if (user_data == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *static_cast<int32_t*>(user_data) = value != 0 ? 1 : 0;
    return EMC_OK;
}

EMC_Result __cdecl ModHubClientTableTestGetKeybind(void* user_data, EMC_KeybindValueV1* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = *static_cast<EMC_KeybindValueV1*>(user_data);
    return EMC_OK;
}

EMC_Result __cdecl ModHubClientTableTestSetKeybind(
    void* user_data,
    EMC_KeybindValueV1 value,
    char* err_buf,
    uint32_t err_buf_size)
{
    (void)err_buf;
    (void)err_buf_size;
    if (user_data == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *static_cast<EMC_KeybindValueV1*>(user_data) = value;
    return EMC_OK;
}

EMC_Result __cdecl ModHubClientTableTestGetInt(void* user_data, int32_t* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = *static_cast<int32_t*>(user_data);
    return EMC_OK;
}

EMC_Result __cdecl ModHubClientTableTestSetInt(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    (void)err_buf;
    (void)err_buf_size;
    if (user_data == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *static_cast<int32_t*>(user_data) = value;
    return EMC_OK;
}

EMC_Result __cdecl ModHubClientTableTestGetFloat(void* user_data, float* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = *static_cast<float*>(user_data);
    return EMC_OK;
}

EMC_Result __cdecl ModHubClientTableTestSetFloat(void* user_data, float value, char* err_buf, uint32_t err_buf_size)
{
    (void)err_buf;
    (void)err_buf_size;
    if (user_data == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *static_cast<float*>(user_data) = value;
    return EMC_OK;
}

EMC_Result __cdecl ModHubClientTableTestAction(void* user_data, char* err_buf, uint32_t err_buf_size)
{
    (void)err_buf;
    (void)err_buf_size;
    if (user_data == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    int32_t* action_count = static_cast<int32_t*>(user_data);
    *action_count += 1;
    return EMC_OK;
}

const EMC_ModDescriptorV1 kModHubClientTableTestModDescriptor = {
    kModHubClientTableTestNamespaceId,
    kModHubClientTableTestNamespaceDisplayName,
    kModHubClientTableTestModId,
    kModHubClientTableTestModDisplayName,
    &g_mod_hub_client_table_test_mod_user_data};

const EMC_BoolSettingDefV1 kModHubClientTableTestBoolSettingDef = {
    kModHubClientTableTestBoolSettingId,
    "Phase8 Bool",
    "Phase8 bool setting",
    &g_mod_hub_client_table_test_bool_value,
    &ModHubClientTableTestGetBool,
    &ModHubClientTableTestSetBool};

const EMC_KeybindSettingDefV1 kModHubClientTableTestKeybindSettingDef = {
    kModHubClientTableTestKeybindSettingId,
    "Phase8 Keybind",
    "Phase8 keybind setting",
    &g_mod_hub_client_table_test_keybind_value,
    &ModHubClientTableTestGetKeybind,
    &ModHubClientTableTestSetKeybind};

const EMC_IntSettingDefV1 kModHubClientTableTestIntSettingDef = {
    kModHubClientTableTestIntSettingId,
    "Phase8 Int",
    "Phase8 int setting",
    &g_mod_hub_client_table_test_int_value,
    0,
    100,
    5,
    &ModHubClientTableTestGetInt,
    &ModHubClientTableTestSetInt};

const EMC_FloatSettingDefV1 kModHubClientTableTestFloatSettingDef = {
    kModHubClientTableTestFloatSettingId,
    "Phase8 Float",
    "Phase8 float setting",
    &g_mod_hub_client_table_test_float_value,
    0.0f,
    10.0f,
    0.5f,
    EMC_FLOAT_DISPLAY_DECIMALS_DEFAULT,
    &ModHubClientTableTestGetFloat,
    &ModHubClientTableTestSetFloat};

const EMC_ActionRowDefV1 kModHubClientTableTestActionSettingDef = {
    kModHubClientTableTestActionSettingId,
    "Phase8 Action",
    "Phase8 action row",
    &g_mod_hub_client_table_test_action_count,
    EMC_ACTION_FORCE_REFRESH,
    &ModHubClientTableTestAction};

struct ModHubClientTableTestHandleToken
{
    int32_t value;
};

ModHubClientTableTestHandleToken g_mod_hub_client_table_test_handle_token = { 1 };

EMC_ModHandle GetModHubClientTableTestHandle()
{
    return reinterpret_cast<EMC_ModHandle>(&g_mod_hub_client_table_test_handle_token);
}

struct ModHubClientTableTestState
{
    emc::ModHubClient client;
    int32_t mode;

    int32_t register_mod_calls;
    int32_t register_bool_calls;
    int32_t register_keybind_calls;
    int32_t register_int_calls;
    int32_t register_float_calls;
    int32_t register_action_calls;

    int32_t order_checks_passed;
    int32_t descriptor_checks_passed;
    int32_t next_expected_setting_call_index;
};

ModHubClientTableTestState g_mod_hub_client_table_test_state;
bool g_mod_hub_client_table_test_state_initialized = false;

emc::ModHubClientSettingRowV1 g_mod_hub_client_table_test_rows[5];
emc::ModHubClientTableRegistrationV1 g_mod_hub_client_table_test_registration = {
    &kModHubClientTableTestModDescriptor,
    g_mod_hub_client_table_test_rows,
    5u};

void ResetModHubClientTableTestRows()
{
    g_mod_hub_client_table_test_rows[0].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL;
    g_mod_hub_client_table_test_rows[0].def = &kModHubClientTableTestBoolSettingDef;

    g_mod_hub_client_table_test_rows[1].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND;
    g_mod_hub_client_table_test_rows[1].def = &kModHubClientTableTestKeybindSettingDef;

    g_mod_hub_client_table_test_rows[2].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_INT;
    g_mod_hub_client_table_test_rows[2].def = &kModHubClientTableTestIntSettingDef;

    g_mod_hub_client_table_test_rows[3].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_FLOAT;
    g_mod_hub_client_table_test_rows[3].def = &kModHubClientTableTestFloatSettingDef;

    g_mod_hub_client_table_test_rows[4].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION;
    g_mod_hub_client_table_test_rows[4].def = &kModHubClientTableTestActionSettingDef;
}

void ResetModHubClientTableTestCapture()
{
    g_mod_hub_client_table_test_state.register_mod_calls = 0;
    g_mod_hub_client_table_test_state.register_bool_calls = 0;
    g_mod_hub_client_table_test_state.register_keybind_calls = 0;
    g_mod_hub_client_table_test_state.register_int_calls = 0;
    g_mod_hub_client_table_test_state.register_float_calls = 0;
    g_mod_hub_client_table_test_state.register_action_calls = 0;
    g_mod_hub_client_table_test_state.order_checks_passed = 1;
    g_mod_hub_client_table_test_state.descriptor_checks_passed = 1;
    g_mod_hub_client_table_test_state.next_expected_setting_call_index = 0;
}

bool ModHubClientTableTestModeFailsKind(int32_t kind)
{
    if (g_mod_hub_client_table_test_state.mode == kModHubClientTableTestModeFailBool)
    {
        return kind == emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL;
    }
    if (g_mod_hub_client_table_test_state.mode == kModHubClientTableTestModeFailKeybind)
    {
        return kind == emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND;
    }
    if (g_mod_hub_client_table_test_state.mode == kModHubClientTableTestModeFailInt)
    {
        return kind == emc::MOD_HUB_CLIENT_SETTING_KIND_INT;
    }
    if (g_mod_hub_client_table_test_state.mode == kModHubClientTableTestModeFailFloat)
    {
        return kind == emc::MOD_HUB_CLIENT_SETTING_KIND_FLOAT;
    }
    if (g_mod_hub_client_table_test_state.mode == kModHubClientTableTestModeFailAction)
    {
        return kind == emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION;
    }

    return false;
}

void ApplyModHubClientTableTestModeToRows()
{
    if (g_mod_hub_client_table_test_state.mode == kModHubClientTableTestModeInvalidRowKind)
    {
        g_mod_hub_client_table_test_rows[2].kind = 999;
    }
    else if (g_mod_hub_client_table_test_state.mode == kModHubClientTableTestModeNullRowDef)
    {
        g_mod_hub_client_table_test_rows[2].def = 0;
    }
}

int32_t ModHubClientTableTestExpectedKindAtIndex(int32_t index)
{
    switch (index)
    {
    case 0:
        return emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL;
    case 1:
        return emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND;
    case 2:
        return emc::MOD_HUB_CLIENT_SETTING_KIND_INT;
    case 3:
        return emc::MOD_HUB_CLIENT_SETTING_KIND_FLOAT;
    case 4:
        return emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION;
    default:
        break;
    }

    return -1;
}

void RecordModHubClientTableTestSettingCall(int32_t kind)
{
    const int32_t expected_kind = ModHubClientTableTestExpectedKindAtIndex(
        g_mod_hub_client_table_test_state.next_expected_setting_call_index);
    if (expected_kind < 0 || expected_kind != kind)
    {
        g_mod_hub_client_table_test_state.order_checks_passed = 0;
    }

    g_mod_hub_client_table_test_state.next_expected_setting_call_index += 1;
}

bool StringEquals(const char* actual, const char* expected)
{
    return actual != 0 && expected != 0 && std::strcmp(actual, expected) == 0;
}

EMC_Result __cdecl ModHubClientTableTestRegisterMod(const EMC_ModDescriptorV1* desc, EMC_ModHandle* out_handle)
{
    if (out_handle == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    g_mod_hub_client_table_test_state.register_mod_calls += 1;
    if (desc == 0
        || !StringEquals(desc->namespace_id, kModHubClientTableTestNamespaceId)
        || !StringEquals(desc->namespace_display_name, kModHubClientTableTestNamespaceDisplayName)
        || !StringEquals(desc->mod_id, kModHubClientTableTestModId)
        || !StringEquals(desc->mod_display_name, kModHubClientTableTestModDisplayName)
        || desc->mod_user_data != &g_mod_hub_client_table_test_mod_user_data)
    {
        g_mod_hub_client_table_test_state.descriptor_checks_passed = 0;
        *out_handle = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_handle = GetModHubClientTableTestHandle();
    return EMC_OK;
}

EMC_Result __cdecl ModHubClientTableTestRegisterBool(EMC_ModHandle mod, const EMC_BoolSettingDefV1* def)
{
    g_mod_hub_client_table_test_state.register_bool_calls += 1;
    RecordModHubClientTableTestSettingCall(emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL);

    if (mod != GetModHubClientTableTestHandle()
        || def == 0
        || !StringEquals(def->setting_id, kModHubClientTableTestBoolSettingId)
        || def->get_value != &ModHubClientTableTestGetBool
        || def->set_value != &ModHubClientTableTestSetBool
        || def->user_data != &g_mod_hub_client_table_test_bool_value)
    {
        g_mod_hub_client_table_test_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return ModHubClientTableTestModeFailsKind(emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL)
        ? EMC_ERR_INTERNAL
        : EMC_OK;
}

EMC_Result __cdecl ModHubClientTableTestRegisterKeybind(EMC_ModHandle mod, const EMC_KeybindSettingDefV1* def)
{
    g_mod_hub_client_table_test_state.register_keybind_calls += 1;
    RecordModHubClientTableTestSettingCall(emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND);

    if (mod != GetModHubClientTableTestHandle()
        || def == 0
        || !StringEquals(def->setting_id, kModHubClientTableTestKeybindSettingId)
        || def->get_value != &ModHubClientTableTestGetKeybind
        || def->set_value != &ModHubClientTableTestSetKeybind
        || def->user_data != &g_mod_hub_client_table_test_keybind_value)
    {
        g_mod_hub_client_table_test_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return ModHubClientTableTestModeFailsKind(emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND)
        ? EMC_ERR_INTERNAL
        : EMC_OK;
}

EMC_Result __cdecl ModHubClientTableTestRegisterInt(EMC_ModHandle mod, const EMC_IntSettingDefV1* def)
{
    g_mod_hub_client_table_test_state.register_int_calls += 1;
    RecordModHubClientTableTestSettingCall(emc::MOD_HUB_CLIENT_SETTING_KIND_INT);

    if (mod != GetModHubClientTableTestHandle()
        || def == 0
        || !StringEquals(def->setting_id, kModHubClientTableTestIntSettingId)
        || def->min_value != 0
        || def->max_value != 100
        || def->step != 5
        || def->get_value != &ModHubClientTableTestGetInt
        || def->set_value != &ModHubClientTableTestSetInt
        || def->user_data != &g_mod_hub_client_table_test_int_value)
    {
        g_mod_hub_client_table_test_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return ModHubClientTableTestModeFailsKind(emc::MOD_HUB_CLIENT_SETTING_KIND_INT)
        ? EMC_ERR_INTERNAL
        : EMC_OK;
}

EMC_Result __cdecl ModHubClientTableTestRegisterFloat(EMC_ModHandle mod, const EMC_FloatSettingDefV1* def)
{
    g_mod_hub_client_table_test_state.register_float_calls += 1;
    RecordModHubClientTableTestSettingCall(emc::MOD_HUB_CLIENT_SETTING_KIND_FLOAT);

    if (mod != GetModHubClientTableTestHandle()
        || def == 0
        || !StringEquals(def->setting_id, kModHubClientTableTestFloatSettingId)
        || def->min_value != 0.0f
        || def->max_value != 10.0f
        || def->step != 0.5f
        || def->display_decimals != EMC_FLOAT_DISPLAY_DECIMALS_DEFAULT
        || def->get_value != &ModHubClientTableTestGetFloat
        || def->set_value != &ModHubClientTableTestSetFloat
        || def->user_data != &g_mod_hub_client_table_test_float_value)
    {
        g_mod_hub_client_table_test_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return ModHubClientTableTestModeFailsKind(emc::MOD_HUB_CLIENT_SETTING_KIND_FLOAT)
        ? EMC_ERR_INTERNAL
        : EMC_OK;
}

EMC_Result __cdecl ModHubClientTableTestRegisterAction(EMC_ModHandle mod, const EMC_ActionRowDefV1* def)
{
    g_mod_hub_client_table_test_state.register_action_calls += 1;
    RecordModHubClientTableTestSettingCall(emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION);

    if (mod != GetModHubClientTableTestHandle()
        || def == 0
        || !StringEquals(def->setting_id, kModHubClientTableTestActionSettingId)
        || def->action_flags != EMC_ACTION_FORCE_REFRESH
        || def->on_action != &ModHubClientTableTestAction
        || def->user_data != &g_mod_hub_client_table_test_action_count)
    {
        g_mod_hub_client_table_test_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return ModHubClientTableTestModeFailsKind(emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION)
        ? EMC_ERR_INTERNAL
        : EMC_OK;
}

const EMC_HubApiV1* GetModHubClientTableTestApi()
{
    static const EMC_HubApiV1 kTableApi = {
        EMC_HUB_API_VERSION_1,
        (uint32_t)sizeof(EMC_HubApiV1),
        &ModHubClientTableTestRegisterMod,
        &ModHubClientTableTestRegisterBool,
        &ModHubClientTableTestRegisterKeybind,
        &ModHubClientTableTestRegisterInt,
        &ModHubClientTableTestRegisterFloat,
        &ModHubClientTableTestRegisterAction,
        0,
        0};
    return &kTableApi;
}

EMC_Result __cdecl ModHubClientTableTestGetApi(
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

    if (requested_version != EMC_HUB_API_VERSION_1)
    {
        return EMC_ERR_UNSUPPORTED_VERSION;
    }

    if (caller_api_size < EMC_HUB_API_V1_MIN_SIZE)
    {
        return EMC_ERR_API_SIZE_MISMATCH;
    }

    *out_api = GetModHubClientTableTestApi();
    *out_api_size = (uint32_t)sizeof(EMC_HubApiV1);
    return EMC_OK;
}

void ResetModHubClientTableTestState()
{
    g_mod_hub_client_table_test_state.mode = kModHubClientTableTestModeSuccess;
    ResetModHubClientTableTestRows();
    ResetModHubClientTableTestCapture();

    emc::ModHubClient::Config config;
    config.get_api_fn = &ModHubClientTableTestGetApi;
    config.table_registration = &g_mod_hub_client_table_test_registration;
    g_mod_hub_client_table_test_state.client.SetConfig(config);
    g_mod_hub_client_table_test_state.client.Reset();
}

void EnsureModHubClientTableTestStateInitialized()
{
    if (g_mod_hub_client_table_test_state_initialized)
    {
        return;
    }

    ResetModHubClientTableTestState();
    g_mod_hub_client_table_test_state_initialized = true;
}

void SetModHubClientTableTestMode(int32_t mode)
{
    g_mod_hub_client_table_test_state.mode = mode;
    ResetModHubClientTableTestRows();
    ApplyModHubClientTableTestModeToRows();
    ResetModHubClientTableTestCapture();
    g_mod_hub_client_table_test_state.client.Reset();
}
#endif
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_GetApi(
    uint32_t requested_version,
    uint32_t caller_api_size,
    const EMC_HubApiV1** out_api,
    uint32_t* out_api_size)
{
    if (out_api == nullptr || out_api_size == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_api = nullptr;
    *out_api_size = 0;

    if (requested_version != EMC_HUB_API_VERSION_1)
    {
        return EMC_ERR_UNSUPPORTED_VERSION;
    }

    if (caller_api_size < EMC_HUB_API_V1_MIN_SIZE)
    {
        return EMC_ERR_API_SIZE_MISMATCH;
    }

    *out_api = &kHubApiV1;
    *out_api_size = (uint32_t)sizeof(EMC_HubApiV1);
    return EMC_OK;
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_GetApi_v1_compat(
    uint32_t requested_version,
    uint32_t caller_api_size,
    const EMC_HubApiV1** out_api,
    uint32_t* out_api_size)
{
    LogGetApiAliasDeprecatedOnce(EMC_MOD_HUB_GET_API_COMPAT_EXPORT_NAME);
    return EMC_ModHub_GetApi(requested_version, caller_api_size, out_api, out_api_size);
}

#if defined(EMC_ENABLE_TEST_EXPORTS)
extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_SetRegistrationLocked(int32_t is_locked)
{
    HubRegistry_SetRegistrationLocked(is_locked != 0);
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_SetRegistryAttachEnabled(int32_t is_enabled)
{
    if (is_enabled < 0)
    {
        ClearRegistryAttachOverride();
        return;
    }

    SetRegistryAttachOverride(is_enabled != 0);
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_ResetGetApiAliasWarningCount()
{
    g_get_api_alias_warning_count = 0;
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_GetApiAliasWarningCount()
{
    return g_get_api_alias_warning_count;
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_Menu_SetHubEnabled(int32_t is_enabled)
{
    HubMenuBridge_SetHubEnabled(is_enabled != 0);
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_Menu_OpenOptionsWindow()
{
    HubMenuBridge_OnOptionsWindowInit();
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_GetOptionsWindowInitObserverCount()
{
    return GetActiveOptionsWindowInitObserverCount();
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_Menu_SaveOptionsWindow()
{
    HubMenuBridge_OnOptionsWindowSave();
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_Menu_CloseOptionsWindow()
{
    HubMenuBridge_OnOptionsWindowClose();
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_SetPendingBool(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    int32_t value)
{
    return HubUi_SetPendingBool(namespace_id, mod_id, setting_id, value);
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_AdjustPendingIntStep(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    int32_t step_delta)
{
    return HubUi_AdjustPendingIntStep(namespace_id, mod_id, setting_id, step_delta);
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_SetPendingIntFromText(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    const char* text)
{
    return HubUi_SetPendingIntFromText(namespace_id, mod_id, setting_id, text);
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_NormalizePendingIntText(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id)
{
    return HubUi_NormalizePendingIntText(namespace_id, mod_id, setting_id);
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_GetPendingIntState(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    int32_t* out_value,
    int32_t* out_parse_error,
    char* out_text,
    uint32_t out_text_size)
{
    if (out_value == 0 || out_parse_error == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    HubUiRowView row_view = {};
    if (!TryGetRowViewById(namespace_id, mod_id, setting_id, &row_view)
        || row_view.kind != HUB_UI_ROW_KIND_INT)
    {
        return EMC_ERR_NOT_FOUND;
    }

    *out_value = row_view.pending_int_value;
    *out_parse_error = row_view.int_text_parse_error ? 1 : 0;
    CopyStringToBuffer(row_view.pending_int_text, out_text, out_text_size);
    return EMC_OK;
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_AdjustPendingFloatStep(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    int32_t step_delta)
{
    return HubUi_AdjustPendingFloatStep(namespace_id, mod_id, setting_id, step_delta);
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_SetPendingFloatFromText(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    const char* text)
{
    return HubUi_SetPendingFloatFromText(namespace_id, mod_id, setting_id, text);
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_NormalizePendingFloatText(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id)
{
    return HubUi_NormalizePendingFloatText(namespace_id, mod_id, setting_id);
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_GetPendingFloatState(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    float* out_value,
    int32_t* out_parse_error,
    char* out_text,
    uint32_t out_text_size)
{
    if (out_value == 0 || out_parse_error == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    HubUiRowView row_view = {};
    if (!TryGetRowViewById(namespace_id, mod_id, setting_id, &row_view)
        || row_view.kind != HUB_UI_ROW_KIND_FLOAT)
    {
        return EMC_ERR_NOT_FOUND;
    }

    *out_value = row_view.pending_float_value;
    *out_parse_error = row_view.float_text_parse_error ? 1 : 0;
    CopyStringToBuffer(row_view.pending_float_text, out_text, out_text_size);
    return EMC_OK;
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_BeginKeybindCapture(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id)
{
    return HubUi_BeginKeybindCapture(namespace_id, mod_id, setting_id);
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_CancelKeybindCapture(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id)
{
    return HubUi_CancelKeybindCapture(namespace_id, mod_id, setting_id);
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_ApplyCapturedKeycode(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    int32_t keycode)
{
    return HubUi_ApplyCapturedKeycode(namespace_id, mod_id, setting_id, keycode);
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_ClearPendingKeybind(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id)
{
    return HubUi_ClearPendingKeybind(namespace_id, mod_id, setting_id);
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_InvokeActionRow(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id)
{
    return HubUi_InvokeActionRow(namespace_id, mod_id, setting_id);
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_SetNamespaceSearchQuery(
    const char* namespace_id,
    const char* search_query)
{
    return HubUi_SetNamespaceSearchQuery(namespace_id, search_query);
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_SetModCollapsed(
    const char* namespace_id,
    const char* mod_id,
    int32_t is_collapsed)
{
    return HubUi_SetModCollapsed(namespace_id, mod_id, is_collapsed != 0);
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_GetModCollapsed(
    const char* namespace_id,
    const char* mod_id,
    int32_t* out_is_collapsed)
{
    if (out_is_collapsed == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    bool collapsed = false;
    if (!HubUi_GetModCollapsed(namespace_id, mod_id, &collapsed))
    {
        *out_is_collapsed = 0;
        return EMC_ERR_NOT_FOUND;
    }

    *out_is_collapsed = collapsed ? 1 : 0;
    return EMC_OK;
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_DoesSettingMatchNamespaceSearch(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    int32_t* out_matches)
{
    if (out_matches == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    bool matches = false;
    if (!HubUi_DoesSettingMatchNamespaceSearch(namespace_id, mod_id, setting_id, &matches))
    {
        *out_matches = 0;
        return EMC_ERR_NOT_FOUND;
    }

    *out_matches = matches ? 1 : 0;
    return EMC_OK;
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_UI_CountSettingsForMod(
    const char* namespace_id,
    const char* mod_id)
{
    if (namespace_id == 0 || mod_id == 0 || namespace_id[0] == '\0' || mod_id[0] == '\0')
    {
        return -1;
    }

    CountSettingsForModContext context = {
        namespace_id,
        mod_id,
        0 };
    HubRegistry_ForEachSettingInOrder(&CountSettingsForModVisitor, &context);
    return context.count;
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_Commit_GetLastSummary(
    uint32_t* out_attempted,
    uint32_t* out_succeeded,
    uint32_t* out_failed,
    uint32_t* out_skipped,
    int32_t* out_skip_reason)
{
    if (out_attempted == nullptr
        || out_succeeded == nullptr
        || out_failed == nullptr
        || out_skipped == nullptr
        || out_skip_reason == nullptr)
    {
        return;
    }

    HubCommitSummary summary;
    HubCommit_GetLastSummary(&summary);
    *out_attempted = summary.attempted;
    *out_succeeded = summary.succeeded;
    *out_failed = summary.failed;
    *out_skipped = summary.skipped;
    *out_skip_reason = summary.skip_reason;
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_Client_Reset()
{
    EnsureModHubClientTestStateInitialized();
    ResetModHubClientTestState();
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_Client_SetGetApiMode(int32_t mode)
{
    EnsureModHubClientTestStateInitialized();
    g_mod_hub_client_test_state.get_api_mode = mode;
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_Client_SetGetApiFailureResult(int32_t result)
{
    EnsureModHubClientTestStateInitialized();
    g_mod_hub_client_test_state.get_api_failure_result = (EMC_Result)result;
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_Client_SetRegisterResult(int32_t result)
{
    EnsureModHubClientTestStateInitialized();
    g_mod_hub_client_test_state.register_result = (EMC_Result)result;
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_Client_SetForceAttachFailureMode(int32_t mode)
{
    EnsureModHubClientTestStateInitialized();
    g_mod_hub_client_test_state.force_attach_failure_mode = mode;
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_Client_OnStartup()
{
    EnsureModHubClientTestStateInitialized();
    return (int32_t)g_mod_hub_client_test_state.client.OnStartup();
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_Client_OnOptionsWindowInit()
{
    EnsureModHubClientTestStateInitialized();
    return (int32_t)g_mod_hub_client_test_state.client.OnOptionsWindowInit();
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_Client_UseHubUi()
{
    EnsureModHubClientTestStateInitialized();
    return g_mod_hub_client_test_state.client.UseHubUi() ? 1 : 0;
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_Client_IsAttachRetryPending()
{
    EnsureModHubClientTestStateInitialized();
    return g_mod_hub_client_test_state.client.IsAttachRetryPending() ? 1 : 0;
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_Client_HasAttachRetryAttempted()
{
    EnsureModHubClientTestStateInitialized();
    return g_mod_hub_client_test_state.client.HasAttachRetryAttempted() ? 1 : 0;
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_Client_LastAttemptFailureResult()
{
    EnsureModHubClientTestStateInitialized();
    return (int32_t)g_mod_hub_client_test_state.client.LastAttemptFailureResult();
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_Client_Table_Reset()
{
    EnsureModHubClientTableTestStateInitialized();
    ResetModHubClientTableTestState();
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_Client_Table_SetMode(int32_t mode)
{
    EnsureModHubClientTableTestStateInitialized();
    SetModHubClientTableTestMode(mode);
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_Client_Table_OnStartup()
{
    EnsureModHubClientTableTestStateInitialized();
    return (int32_t)g_mod_hub_client_table_test_state.client.OnStartup();
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_Client_Table_UseHubUi()
{
    EnsureModHubClientTableTestStateInitialized();
    return g_mod_hub_client_table_test_state.client.UseHubUi() ? 1 : 0;
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_Client_Table_LastAttemptFailureResult()
{
    EnsureModHubClientTableTestStateInitialized();
    return (int32_t)g_mod_hub_client_table_test_state.client.LastAttemptFailureResult();
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_Client_Table_GetRegisterModCalls()
{
    EnsureModHubClientTableTestStateInitialized();
    return g_mod_hub_client_table_test_state.register_mod_calls;
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_Client_Table_GetRegisterBoolCalls()
{
    EnsureModHubClientTableTestStateInitialized();
    return g_mod_hub_client_table_test_state.register_bool_calls;
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_Client_Table_GetRegisterKeybindCalls()
{
    EnsureModHubClientTableTestStateInitialized();
    return g_mod_hub_client_table_test_state.register_keybind_calls;
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_Client_Table_GetRegisterIntCalls()
{
    EnsureModHubClientTableTestStateInitialized();
    return g_mod_hub_client_table_test_state.register_int_calls;
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_Client_Table_GetRegisterFloatCalls()
{
    EnsureModHubClientTableTestStateInitialized();
    return g_mod_hub_client_table_test_state.register_float_calls;
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_Client_Table_GetRegisterActionCalls()
{
    EnsureModHubClientTableTestStateInitialized();
    return g_mod_hub_client_table_test_state.register_action_calls;
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_Client_Table_GetOrderChecksPassed()
{
    EnsureModHubClientTableTestStateInitialized();
    return g_mod_hub_client_table_test_state.order_checks_passed;
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_Client_Table_GetDescriptorChecksPassed()
{
    EnsureModHubClientTableTestStateInitialized();
    return g_mod_hub_client_table_test_state.descriptor_checks_passed;
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_DummyConsumer_Reset()
{
    ModHubDummyConsumer_Reset();
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_DummyConsumer_SetMode(int32_t mode)
{
    ModHubDummyConsumer_SetMode(mode);
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_DummyConsumer_OnStartup()
{
    return ModHubDummyConsumer_OnStartup();
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_DummyConsumer_UseHubUi()
{
    return ModHubDummyConsumer_UseHubUi();
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_DummyConsumer_LastAttemptFailureResult()
{
    return ModHubDummyConsumer_LastAttemptFailureResult();
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_DummyConsumer_GetRegisterModCalls()
{
    return ModHubDummyConsumer_GetRegisterModCalls();
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_DummyConsumer_GetRegisterBoolCalls()
{
    return ModHubDummyConsumer_GetRegisterBoolCalls();
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_DummyConsumer_GetRegisterKeybindCalls()
{
    return ModHubDummyConsumer_GetRegisterKeybindCalls();
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_DummyConsumer_GetRegisterIntCalls()
{
    return ModHubDummyConsumer_GetRegisterIntCalls();
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_DummyConsumer_GetRegisterFloatCalls()
{
    return ModHubDummyConsumer_GetRegisterFloatCalls();
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_DummyConsumer_GetRegisterActionCalls()
{
    return ModHubDummyConsumer_GetRegisterActionCalls();
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_DummyConsumer_GetOrderChecksPassed()
{
    return ModHubDummyConsumer_GetOrderChecksPassed();
}

extern "C" EMC_MOD_HUB_API int32_t __cdecl EMC_ModHub_Test_DummyConsumer_GetDescriptorChecksPassed()
{
    return ModHubDummyConsumer_GetDescriptorChecksPassed();
}
#endif
