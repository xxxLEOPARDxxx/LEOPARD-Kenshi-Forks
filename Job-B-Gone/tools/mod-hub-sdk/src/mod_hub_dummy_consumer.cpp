#include "mod_hub_dummy_consumer.h"

#if defined(EMC_ENABLE_TEST_EXPORTS)

#include "emc/mod_hub_api.h"
#include "emc/mod_hub_client.h"

#include <cstring>

namespace
{
const int32_t kModeSuccess = 0;
const int32_t kModeFailBool = 1;
const int32_t kModeFailKeybind = 2;
const int32_t kModeFailInt = 3;
const int32_t kModeFailFloat = 4;
const int32_t kModeFailAction = 5;
const int32_t kModeInvalidRowKind = 6;
const int32_t kModeNullRowDef = 7;
const int32_t kModeUseIntV2 = 8;
const int32_t kModeUseIntV2LegacyApi = 9;

const char* kNamespaceId = "phase8.dummy_consumer";
const char* kNamespaceDisplayName = "Phase8 Dummy Consumer";
const char* kModId = "phase8_dummy_mod";
const char* kModDisplayName = "Phase8 Dummy Mod";

const char* kBoolSettingId = "enabled";
const char* kKeybindSettingId = "hotkey";
const char* kIntSettingId = "count";
const char* kFloatSettingId = "radius";
const char* kActionSettingId = "refresh_now";

int32_t g_mod_user_data = 11;
int32_t g_bool_value = 1;
EMC_KeybindValueV1 g_keybind_value = { 42, 0u };
int32_t g_int_value = 10;
float g_float_value = 2.5f;
int32_t g_action_count = 0;

EMC_Result __cdecl GetBool(void* user_data, int32_t* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = *static_cast<int32_t*>(user_data);
    return EMC_OK;
}

EMC_Result __cdecl SetBool(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
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

EMC_Result __cdecl GetKeybind(void* user_data, EMC_KeybindValueV1* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = *static_cast<EMC_KeybindValueV1*>(user_data);
    return EMC_OK;
}

EMC_Result __cdecl SetKeybind(void* user_data, EMC_KeybindValueV1 value, char* err_buf, uint32_t err_buf_size)
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

EMC_Result __cdecl GetInt(void* user_data, int32_t* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = *static_cast<int32_t*>(user_data);
    return EMC_OK;
}

EMC_Result __cdecl SetInt(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
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

EMC_Result __cdecl GetFloat(void* user_data, float* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = *static_cast<float*>(user_data);
    return EMC_OK;
}

EMC_Result __cdecl SetFloat(void* user_data, float value, char* err_buf, uint32_t err_buf_size)
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

EMC_Result __cdecl InvokeAction(void* user_data, char* err_buf, uint32_t err_buf_size)
{
    (void)err_buf;
    (void)err_buf_size;
    if (user_data == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    int32_t* count = static_cast<int32_t*>(user_data);
    *count += 1;
    return EMC_OK;
}

const EMC_ModDescriptorV1 kModDescriptor = {
    kNamespaceId,
    kNamespaceDisplayName,
    kModId,
    kModDisplayName,
    &g_mod_user_data};

const EMC_BoolSettingDefV1 kBoolSettingDef = {
    kBoolSettingId,
    "Enabled",
    "Enable or disable feature",
    &g_bool_value,
    &GetBool,
    &SetBool};

const EMC_KeybindSettingDefV1 kKeybindSettingDef = {
    kKeybindSettingId,
    "Hotkey",
    "Example hotkey",
    &g_keybind_value,
    &GetKeybind,
    &SetKeybind};

const EMC_IntSettingDefV1 kIntSettingDef = {
    kIntSettingId,
    "Count",
    "Example count",
    &g_int_value,
    0,
    100,
    5,
    &GetInt,
    &SetInt};

const EMC_IntSettingDefV2 kIntSettingDefV2 = {
    kIntSettingId,
    "Count",
    "Example count",
    &g_int_value,
    0,
    100,
    5,
    { 10, 5, 0 },
    { 5, 10, 0 },
    &GetInt,
    &SetInt};

const EMC_FloatSettingDefV1 kFloatSettingDef = {
    kFloatSettingId,
    "Radius",
    "Example radius",
    &g_float_value,
    0.0f,
    10.0f,
    0.5f,
    EMC_FLOAT_DISPLAY_DECIMALS_DEFAULT,
    &GetFloat,
    &SetFloat};

const EMC_ActionRowDefV1 kActionSettingDef = {
    kActionSettingId,
    "Refresh now",
    "Example action",
    &g_action_count,
    EMC_ACTION_FORCE_REFRESH,
    &InvokeAction};

struct HandleToken
{
    int32_t value;
};

HandleToken g_handle_token = { 1 };

EMC_ModHandle GetHandle()
{
    return reinterpret_cast<EMC_ModHandle>(&g_handle_token);
}

struct DummyState
{
    emc::ModHubClient client;
    int32_t mode;

    int32_t register_mod_calls;
    int32_t register_bool_calls;
    int32_t register_keybind_calls;
    int32_t register_int_calls;
    int32_t register_int_v2_calls;
    int32_t register_float_calls;
    int32_t register_action_calls;

    int32_t order_checks_passed;
    int32_t descriptor_checks_passed;
    int32_t next_expected_kind_index;
};

DummyState g_state;
bool g_initialized = false;

emc::ModHubClientSettingRowV1 g_rows[5];
emc::ModHubClientTableRegistrationV1 g_table_registration = {
    &kModDescriptor,
    g_rows,
    5u};

void ResetRows()
{
    g_rows[0].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL;
    g_rows[0].def = &kBoolSettingDef;

    g_rows[1].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND;
    g_rows[1].def = &kKeybindSettingDef;

    g_rows[2].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_INT;
    g_rows[2].def = &kIntSettingDef;

    g_rows[3].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_FLOAT;
    g_rows[3].def = &kFloatSettingDef;

    g_rows[4].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION;
    g_rows[4].def = &kActionSettingDef;
}

void ResetCapture()
{
    g_state.register_mod_calls = 0;
    g_state.register_bool_calls = 0;
    g_state.register_keybind_calls = 0;
    g_state.register_int_calls = 0;
    g_state.register_int_v2_calls = 0;
    g_state.register_float_calls = 0;
    g_state.register_action_calls = 0;
    g_state.order_checks_passed = 1;
    g_state.descriptor_checks_passed = 1;
    g_state.next_expected_kind_index = 0;
}

int32_t ExpectedKindForIndex(int32_t index)
{
    switch (index)
    {
    case 0:
        return emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL;
    case 1:
        return emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND;
    case 2:
        return (g_state.mode == kModeUseIntV2 || g_state.mode == kModeUseIntV2LegacyApi)
            ? emc::MOD_HUB_CLIENT_SETTING_KIND_INT_V2
            : emc::MOD_HUB_CLIENT_SETTING_KIND_INT;
    case 3:
        return emc::MOD_HUB_CLIENT_SETTING_KIND_FLOAT;
    case 4:
        return emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION;
    default:
        break;
    }

    return -1;
}

void RecordKind(int32_t kind)
{
    const int32_t expected_kind = ExpectedKindForIndex(g_state.next_expected_kind_index);
    if (expected_kind < 0 || expected_kind != kind)
    {
        g_state.order_checks_passed = 0;
    }

    g_state.next_expected_kind_index += 1;
}

bool StringEquals(const char* a, const char* b)
{
    return a != 0 && b != 0 && std::strcmp(a, b) == 0;
}

bool ShouldFailKind(int32_t kind)
{
    if (g_state.mode == kModeFailBool)
    {
        return kind == emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL;
    }
    if (g_state.mode == kModeFailKeybind)
    {
        return kind == emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND;
    }
    if (g_state.mode == kModeFailInt)
    {
        return kind == emc::MOD_HUB_CLIENT_SETTING_KIND_INT;
    }
    if (g_state.mode == kModeFailFloat)
    {
        return kind == emc::MOD_HUB_CLIENT_SETTING_KIND_FLOAT;
    }
    if (g_state.mode == kModeFailAction)
    {
        return kind == emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION;
    }

    return false;
}

void ApplyMode()
{
    if (g_state.mode == kModeInvalidRowKind)
    {
        g_rows[2].kind = 999;
    }
    else if (g_state.mode == kModeNullRowDef)
    {
        g_rows[2].def = 0;
    }
    else if (g_state.mode == kModeUseIntV2 || g_state.mode == kModeUseIntV2LegacyApi)
    {
        g_rows[2].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_INT_V2;
        g_rows[2].def = &kIntSettingDefV2;
    }
}

EMC_Result __cdecl TestRegisterMod(const EMC_ModDescriptorV1* desc, EMC_ModHandle* out_handle)
{
    if (out_handle == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    g_state.register_mod_calls += 1;
    if (desc == 0
        || !StringEquals(desc->namespace_id, kNamespaceId)
        || !StringEquals(desc->namespace_display_name, kNamespaceDisplayName)
        || !StringEquals(desc->mod_id, kModId)
        || !StringEquals(desc->mod_display_name, kModDisplayName)
        || desc->mod_user_data != &g_mod_user_data)
    {
        g_state.descriptor_checks_passed = 0;
        *out_handle = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_handle = GetHandle();
    return EMC_OK;
}

EMC_Result __cdecl TestRegisterBool(EMC_ModHandle mod, const EMC_BoolSettingDefV1* def)
{
    g_state.register_bool_calls += 1;
    RecordKind(emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL);

    if (mod != GetHandle()
        || def == 0
        || !StringEquals(def->setting_id, kBoolSettingId)
        || def->get_value != &GetBool
        || def->set_value != &SetBool
        || def->user_data != &g_bool_value)
    {
        g_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return ShouldFailKind(emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL) ? EMC_ERR_INTERNAL : EMC_OK;
}

EMC_Result __cdecl TestRegisterKeybind(EMC_ModHandle mod, const EMC_KeybindSettingDefV1* def)
{
    g_state.register_keybind_calls += 1;
    RecordKind(emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND);

    if (mod != GetHandle()
        || def == 0
        || !StringEquals(def->setting_id, kKeybindSettingId)
        || def->get_value != &GetKeybind
        || def->set_value != &SetKeybind
        || def->user_data != &g_keybind_value)
    {
        g_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return ShouldFailKind(emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND) ? EMC_ERR_INTERNAL : EMC_OK;
}

EMC_Result __cdecl TestRegisterInt(EMC_ModHandle mod, const EMC_IntSettingDefV1* def)
{
    g_state.register_int_calls += 1;
    RecordKind(emc::MOD_HUB_CLIENT_SETTING_KIND_INT);

    if (mod != GetHandle()
        || def == 0
        || !StringEquals(def->setting_id, kIntSettingId)
        || def->min_value != 0
        || def->max_value != 100
        || def->step != 5
        || def->get_value != &GetInt
        || def->set_value != &SetInt
        || def->user_data != &g_int_value)
    {
        g_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return ShouldFailKind(emc::MOD_HUB_CLIENT_SETTING_KIND_INT) ? EMC_ERR_INTERNAL : EMC_OK;
}

EMC_Result __cdecl TestRegisterIntV2(EMC_ModHandle mod, const EMC_IntSettingDefV2* def)
{
    g_state.register_int_v2_calls += 1;
    RecordKind(emc::MOD_HUB_CLIENT_SETTING_KIND_INT_V2);

    if (mod != GetHandle()
        || def == 0
        || !StringEquals(def->setting_id, kIntSettingId)
        || def->min_value != 0
        || def->max_value != 100
        || def->step != 5
        || def->dec_button_deltas[0] != 10
        || def->dec_button_deltas[1] != 5
        || def->dec_button_deltas[2] != 0
        || def->inc_button_deltas[0] != 5
        || def->inc_button_deltas[1] != 10
        || def->inc_button_deltas[2] != 0
        || def->get_value != &GetInt
        || def->set_value != &SetInt
        || def->user_data != &g_int_value)
    {
        g_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_OK;
}

EMC_Result __cdecl TestRegisterFloat(EMC_ModHandle mod, const EMC_FloatSettingDefV1* def)
{
    g_state.register_float_calls += 1;
    RecordKind(emc::MOD_HUB_CLIENT_SETTING_KIND_FLOAT);

    if (mod != GetHandle()
        || def == 0
        || !StringEquals(def->setting_id, kFloatSettingId)
        || def->min_value != 0.0f
        || def->max_value != 10.0f
        || def->step != 0.5f
        || def->display_decimals != EMC_FLOAT_DISPLAY_DECIMALS_DEFAULT
        || def->get_value != &GetFloat
        || def->set_value != &SetFloat
        || def->user_data != &g_float_value)
    {
        g_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return ShouldFailKind(emc::MOD_HUB_CLIENT_SETTING_KIND_FLOAT) ? EMC_ERR_INTERNAL : EMC_OK;
}

EMC_Result __cdecl TestRegisterAction(EMC_ModHandle mod, const EMC_ActionRowDefV1* def)
{
    g_state.register_action_calls += 1;
    RecordKind(emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION);

    if (mod != GetHandle()
        || def == 0
        || !StringEquals(def->setting_id, kActionSettingId)
        || def->action_flags != EMC_ACTION_FORCE_REFRESH
        || def->on_action != &InvokeAction
        || def->user_data != &g_action_count)
    {
        g_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return ShouldFailKind(emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION) ? EMC_ERR_INTERNAL : EMC_OK;
}

const EMC_HubApiV1* GetTestApi()
{
    static const EMC_HubApiV1 kApi = {
        EMC_HUB_API_VERSION_1,
        (uint32_t)sizeof(EMC_HubApiV1),
        &TestRegisterMod,
        &TestRegisterBool,
        &TestRegisterKeybind,
        &TestRegisterInt,
        &TestRegisterFloat,
        &TestRegisterAction,
        0,
        0,
        &TestRegisterIntV2};
    return &kApi;
}

EMC_Result __cdecl TestGetApi(
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

    *out_api = GetTestApi();
    *out_api_size = (g_state.mode == kModeUseIntV2LegacyApi)
        ? EMC_HUB_API_V1_OPTIONS_WINDOW_INIT_OBSERVER_MIN_SIZE
        : (uint32_t)sizeof(EMC_HubApiV1);
    return EMC_OK;
}

void EnsureInitialized()
{
    if (g_initialized)
    {
        return;
    }

    g_state.mode = kModeSuccess;
    ResetRows();
    ResetCapture();

    emc::ModHubClient::Config config;
    config.get_api_fn = &TestGetApi;
    config.table_registration = &g_table_registration;
    g_state.client.SetConfig(config);
    g_state.client.Reset();

    g_initialized = true;
}

void ResetState()
{
    g_state.mode = kModeSuccess;
    ResetRows();
    ResetCapture();

    emc::ModHubClient::Config config;
    config.get_api_fn = &TestGetApi;
    config.table_registration = &g_table_registration;
    g_state.client.SetConfig(config);
    g_state.client.Reset();
}
}

void ModHubDummyConsumer_Reset()
{
    EnsureInitialized();
    ResetState();
}

void ModHubDummyConsumer_SetMode(int32_t mode)
{
    EnsureInitialized();
    g_state.mode = mode;
    ResetRows();
    ApplyMode();
    ResetCapture();
    g_state.client.Reset();
}

int32_t ModHubDummyConsumer_OnStartup()
{
    EnsureInitialized();
    return (int32_t)g_state.client.OnStartup();
}

int32_t ModHubDummyConsumer_UseHubUi()
{
    EnsureInitialized();
    return g_state.client.UseHubUi() ? 1 : 0;
}

int32_t ModHubDummyConsumer_LastAttemptFailureResult()
{
    EnsureInitialized();
    return (int32_t)g_state.client.LastAttemptFailureResult();
}

int32_t ModHubDummyConsumer_GetRegisterModCalls()
{
    EnsureInitialized();
    return g_state.register_mod_calls;
}

int32_t ModHubDummyConsumer_GetRegisterBoolCalls()
{
    EnsureInitialized();
    return g_state.register_bool_calls;
}

int32_t ModHubDummyConsumer_GetRegisterKeybindCalls()
{
    EnsureInitialized();
    return g_state.register_keybind_calls;
}

int32_t ModHubDummyConsumer_GetRegisterIntCalls()
{
    EnsureInitialized();
    return g_state.register_int_calls;
}

int32_t ModHubDummyConsumer_GetRegisterIntV2Calls()
{
    EnsureInitialized();
    return g_state.register_int_v2_calls;
}

int32_t ModHubDummyConsumer_GetRegisterFloatCalls()
{
    EnsureInitialized();
    return g_state.register_float_calls;
}

int32_t ModHubDummyConsumer_GetRegisterActionCalls()
{
    EnsureInitialized();
    return g_state.register_action_calls;
}

int32_t ModHubDummyConsumer_GetOrderChecksPassed()
{
    EnsureInitialized();
    return g_state.order_checks_passed;
}

int32_t ModHubDummyConsumer_GetDescriptorChecksPassed()
{
    EnsureInitialized();
    return g_state.descriptor_checks_passed;
}

#endif
