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
const int32_t kModeUseBoolV2 = 10;
const int32_t kModeUseBoolV2LegacyApi = 11;
const int32_t kModeUseKeybindV2 = 12;
const int32_t kModeUseKeybindV2LegacyApi = 13;
const int32_t kModeUseSelectV2 = 14;
const int32_t kModeUseSelectV2LegacyApi = 15;
const int32_t kModeUseTextV2 = 16;
const int32_t kModeUseTextV2LegacyApi = 17;
const int32_t kModeUseActionV2 = 18;
const int32_t kModeUseActionV2LegacyApi = 19;
const int32_t kModeUseBoolConditionRule = 20;
const int32_t kModeUseBoolConditionRuleLegacyApi = 21;

const char* kNamespaceId = "phase8.dummy_consumer";
const char* kNamespaceDisplayName = "Phase8 Dummy Consumer";
const char* kModId = "phase8_dummy_mod";
const char* kModDisplayName = "Phase8 Dummy Mod";

const char* kBoolSettingId = "enabled";
const char* kKeybindSettingId = "hotkey";
const char* kIntSettingId = "count";
const char* kFloatSettingId = "radius";
const char* kSelectSettingId = "palette";
const char* kTextSettingId = "title";
const char* kColorSettingId = "status_color";
const char* kActionSettingId = "refresh_now";
const char* kBoolConditionControllerSettingId = "feature_enabled";
const char* kBoolConditionHiddenSettingId = "feature_hidden";
const char* kBoolConditionDisabledSettingId = "feature_disabled";
const char* kSectionId = "general";
const char* kSectionDisplayName = "General";
const uint32_t kTextMaxLength = 32u;
const uint32_t kColorTextLength = 7u;
const char* kBoolHoverHint = "Toggle the dummy feature.";
const char* kKeybindHoverHint = "Capture the dummy hotkey.";
const char* kSelectHoverHint = "Choose the dummy palette.";
const char* kTextHoverHint = "Edit the dummy title.";
const char* kActionHoverHint = "Refresh the dummy state now.";
const EMC_BoolConditionRuleDefV1 kBoolConditionHideRule = {
    kBoolConditionHiddenSettingId,
    kBoolConditionControllerSettingId,
    EMC_BOOL_CONDITION_EFFECT_HIDE,
    0 };
const EMC_BoolConditionRuleDefV1 kBoolConditionDisableRule = {
    kBoolConditionDisabledSettingId,
    kBoolConditionControllerSettingId,
    EMC_BOOL_CONDITION_EFFECT_DISABLE,
    0 };

int32_t g_mod_user_data = 11;
int32_t g_bool_value = 1;
EMC_KeybindValueV1 g_keybind_value = { 42, 0u };
int32_t g_int_value = 10;
float g_float_value = 2.5f;
int32_t g_select_value = 1;
char g_text_value[kTextMaxLength + 1u] = "Example title";
char g_color_value[kColorTextLength + 1u] = "#FF3333";
int32_t g_action_count = 0;
int32_t g_bool_condition_controller_value = 0;
int32_t g_bool_condition_hidden_value = 1;
int32_t g_bool_condition_disabled_value = 1;

const EMC_SelectOptionV1 kSelectOptions[] = {
    { 0, "Default" },
    { 1, "Warm" },
    { 2, "Cool" } };

const EMC_ColorPresetV1 kColorPresets[] = {
    { "#FF3333", "Enemy" },
    { "#DEE85A", "Ally" },
    { "#40FF40", "Squad" } };

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

EMC_Result __cdecl GetSelect(void* user_data, int32_t* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = *static_cast<int32_t*>(user_data);
    return EMC_OK;
}

EMC_Result __cdecl SetSelect(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
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

EMC_Result __cdecl GetText(void* user_data, char* out_value, uint32_t out_value_size)
{
    if (user_data == 0 || out_value == 0 || out_value_size == 0u)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const char* value = static_cast<const char*>(user_data);
    const size_t length = std::strlen(value);
    if (length + 1u > out_value_size)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    std::memcpy(out_value, value, length + 1u);
    return EMC_OK;
}

EMC_Result __cdecl SetText(void* user_data, const char* value, char* err_buf, uint32_t err_buf_size)
{
    (void)err_buf;
    (void)err_buf_size;
    if (user_data == 0 || value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const size_t length = std::strlen(value);
    if (length > kTextMaxLength)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    std::memcpy(static_cast<char*>(user_data), value, length + 1u);
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

const EMC_BoolSettingDefV2 kBoolSettingDefV2 = {
    kBoolSettingId,
    "Enabled",
    "Enable or disable feature",
    &g_bool_value,
    &GetBool,
    &SetBool,
    kBoolHoverHint};

const EMC_KeybindSettingDefV1 kKeybindSettingDef = {
    kKeybindSettingId,
    "Hotkey",
    "Example hotkey",
    &g_keybind_value,
    &GetKeybind,
    &SetKeybind};

const EMC_KeybindSettingDefV2 kKeybindSettingDefV2 = {
    kKeybindSettingId,
    "Hotkey",
    "Example hotkey",
    &g_keybind_value,
    &GetKeybind,
    &SetKeybind,
    kKeybindHoverHint};

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

const EMC_SelectSettingDefV1 kSelectSettingDef = {
    kSelectSettingId,
    "Palette",
    "Example select",
    &g_select_value,
    kSelectOptions,
    (uint32_t)(sizeof(kSelectOptions) / sizeof(kSelectOptions[0])),
    &GetSelect,
    &SetSelect};

const EMC_SelectSettingDefV2 kSelectSettingDefV2 = {
    kSelectSettingId,
    "Palette",
    "Example select",
    &g_select_value,
    kSelectOptions,
    (uint32_t)(sizeof(kSelectOptions) / sizeof(kSelectOptions[0])),
    &GetSelect,
    &SetSelect,
    kSelectHoverHint};

const EMC_TextSettingDefV1 kTextSettingDef = {
    kTextSettingId,
    "Title",
    "Example text",
    g_text_value,
    kTextMaxLength,
    &GetText,
    &SetText};

const EMC_TextSettingDefV2 kTextSettingDefV2 = {
    kTextSettingId,
    "Title",
    "Example text",
    g_text_value,
    kTextMaxLength,
    &GetText,
    &SetText,
    kTextHoverHint};

const EMC_ColorSettingDefV1 kColorSettingDef = {
    kColorSettingId,
    "Status color",
    "Example color",
    g_color_value,
    EMC_COLOR_PREVIEW_KIND_SWATCH,
    kColorPresets,
    (uint32_t)(sizeof(kColorPresets) / sizeof(kColorPresets[0])),
    &GetText,
    &SetText};

const EMC_ActionRowDefV1 kActionSettingDef = {
    kActionSettingId,
    "Refresh now",
    "Example action",
    &g_action_count,
    EMC_ACTION_FORCE_REFRESH,
    &InvokeAction};

const EMC_ActionRowDefV2 kActionSettingDefV2 = {
    kActionSettingId,
    "Refresh now",
    "Example action",
    &g_action_count,
    EMC_ACTION_FORCE_REFRESH,
    &InvokeAction,
    kActionHoverHint};

const EMC_BoolSettingDefV1 kBoolConditionControllerSettingDef = {
    kBoolConditionControllerSettingId,
    "Feature enabled",
    "Toggle the conditional rows",
    &g_bool_condition_controller_value,
    &GetBool,
    &SetBool};

const EMC_BoolSettingDefV1 kBoolConditionHiddenSettingDef = {
    kBoolConditionHiddenSettingId,
    "Feature hidden",
    "Hidden when the controller is off",
    &g_bool_condition_hidden_value,
    &GetBool,
    &SetBool};

const EMC_BoolSettingDefV1 kBoolConditionDisabledSettingDef = {
    kBoolConditionDisabledSettingId,
    "Feature disabled",
    "Disabled when the controller is off",
    &g_bool_condition_disabled_value,
    &GetBool,
    &SetBool};

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
    int32_t register_bool_v2_calls;
    int32_t register_keybind_calls;
    int32_t register_keybind_v2_calls;
    int32_t register_int_calls;
    int32_t register_int_v2_calls;
    int32_t register_float_calls;
    int32_t register_select_calls;
    int32_t register_select_v2_calls;
    int32_t register_text_calls;
    int32_t register_text_v2_calls;
    int32_t register_color_calls;
    int32_t register_section_calls;
    int32_t register_action_calls;
    int32_t register_action_v2_calls;
    int32_t register_bool_condition_rule_calls;

    int32_t order_checks_passed;
    int32_t descriptor_checks_passed;
    int32_t next_expected_kind_index;
};

DummyState g_state;
bool g_initialized = false;

emc::ModHubClientSettingRowV1 g_rows[8];
emc::ModHubClientTableRegistrationV1 g_table_registration = {
    &kModDescriptor,
    g_rows,
    8u};

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

    g_rows[4].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_SELECT;
    g_rows[4].def = &kSelectSettingDef;

    g_rows[5].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_TEXT;
    g_rows[5].def = &kTextSettingDef;

    g_rows[6].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_COLOR;
    g_rows[6].def = &kColorSettingDef;

    g_rows[7].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION;
    g_rows[7].def = &kActionSettingDef;
}

void ResetCapture()
{
    g_state.register_mod_calls = 0;
    g_state.register_bool_calls = 0;
    g_state.register_bool_v2_calls = 0;
    g_state.register_keybind_calls = 0;
    g_state.register_keybind_v2_calls = 0;
    g_state.register_int_calls = 0;
    g_state.register_int_v2_calls = 0;
    g_state.register_float_calls = 0;
    g_state.register_select_calls = 0;
    g_state.register_select_v2_calls = 0;
    g_state.register_text_calls = 0;
    g_state.register_text_v2_calls = 0;
    g_state.register_color_calls = 0;
    g_state.register_section_calls = 0;
    g_state.register_action_calls = 0;
    g_state.register_action_v2_calls = 0;
    g_state.register_bool_condition_rule_calls = 0;
    g_state.order_checks_passed = 1;
    g_state.descriptor_checks_passed = 1;
    g_state.next_expected_kind_index = 0;
}

int32_t ExpectedKindForIndex(int32_t index)
{
    switch (index)
    {
    case 0:
        return (g_state.mode == kModeUseBoolV2 || g_state.mode == kModeUseBoolV2LegacyApi)
            ? emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2
            : emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL;
    case 1:
        return (g_state.mode == kModeUseKeybindV2 || g_state.mode == kModeUseKeybindV2LegacyApi)
            ? emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND_V2
            : emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND;
    case 2:
        return (g_state.mode == kModeUseIntV2 || g_state.mode == kModeUseIntV2LegacyApi)
            ? emc::MOD_HUB_CLIENT_SETTING_KIND_INT_V2
            : emc::MOD_HUB_CLIENT_SETTING_KIND_INT;
    case 3:
        return emc::MOD_HUB_CLIENT_SETTING_KIND_FLOAT;
    case 4:
        return (g_state.mode == kModeUseSelectV2 || g_state.mode == kModeUseSelectV2LegacyApi)
            ? emc::MOD_HUB_CLIENT_SETTING_KIND_SELECT_V2
            : emc::MOD_HUB_CLIENT_SETTING_KIND_SELECT;
    case 5:
        return (g_state.mode == kModeUseTextV2 || g_state.mode == kModeUseTextV2LegacyApi)
            ? emc::MOD_HUB_CLIENT_SETTING_KIND_TEXT_V2
            : emc::MOD_HUB_CLIENT_SETTING_KIND_TEXT;
    case 6:
        return emc::MOD_HUB_CLIENT_SETTING_KIND_COLOR;
    case 7:
        return (g_state.mode == kModeUseActionV2 || g_state.mode == kModeUseActionV2LegacyApi)
            ? emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION_V2
            : emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION;
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

bool IsLegacyApiMode()
{
    return g_state.mode == kModeUseIntV2LegacyApi
        || g_state.mode == kModeUseBoolV2LegacyApi
        || g_state.mode == kModeUseKeybindV2LegacyApi
        || g_state.mode == kModeUseSelectV2LegacyApi
        || g_state.mode == kModeUseTextV2LegacyApi
        || g_state.mode == kModeUseActionV2LegacyApi
        || g_state.mode == kModeUseBoolConditionRuleLegacyApi;
}

uint32_t ResolveLegacyApiSize()
{
    if (g_state.mode == kModeUseBoolConditionRuleLegacyApi)
    {
        return EMC_HUB_API_V1_BOOL_CONDITION_RULE_MIN_SIZE - 1u;
    }

    if (g_state.mode == kModeUseIntV2LegacyApi)
    {
        return EMC_HUB_API_V1_OPTIONS_WINDOW_INIT_OBSERVER_MIN_SIZE;
    }

    return EMC_HUB_API_V1_COLOR_SETTING_MIN_SIZE;
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

    if (g_state.mode == kModeUseBoolV2 || g_state.mode == kModeUseBoolV2LegacyApi)
    {
        g_rows[0].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2;
        g_rows[0].def = &kBoolSettingDefV2;
    }

    if (g_state.mode == kModeUseKeybindV2 || g_state.mode == kModeUseKeybindV2LegacyApi)
    {
        g_rows[1].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND_V2;
        g_rows[1].def = &kKeybindSettingDefV2;
    }

    if (g_state.mode == kModeUseSelectV2 || g_state.mode == kModeUseSelectV2LegacyApi)
    {
        g_rows[4].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_SELECT_V2;
        g_rows[4].def = &kSelectSettingDefV2;
    }

    if (g_state.mode == kModeUseTextV2 || g_state.mode == kModeUseTextV2LegacyApi)
    {
        g_rows[5].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_TEXT_V2;
        g_rows[5].def = &kTextSettingDefV2;
    }

    if (g_state.mode == kModeUseActionV2 || g_state.mode == kModeUseActionV2LegacyApi)
    {
        g_rows[7].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION_V2;
        g_rows[7].def = &kActionSettingDefV2;
    }
}

EMC_Result __cdecl TestRegisterBoolConditionRules(const EMC_HubApiV1* api, void* user_data);
EMC_Result __cdecl TestGetApi(
    uint32_t requested_version,
    uint32_t caller_api_size,
    const EMC_HubApiV1** out_api,
    uint32_t* out_api_size);

void ConfigureClientForMode()
{
    emc::ModHubClient::Config config;
    config.get_api_fn = &TestGetApi;
    if (g_state.mode == kModeUseBoolConditionRule || g_state.mode == kModeUseBoolConditionRuleLegacyApi)
    {
        config.register_fn = &TestRegisterBoolConditionRules;
        config.table_registration = 0;
    }
    else
    {
        config.table_registration = &g_table_registration;
    }

    g_state.client.SetConfig(config);
    g_state.client.Reset();
}

EMC_Result __cdecl TestRegisterBoolConditionRules(const EMC_HubApiV1* api, void* user_data)
{
    (void)user_data;

    if (api == 0 || api->register_mod == 0 || api->register_bool_setting == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EMC_ModHandle mod = 0;
    EMC_Result result = api->register_mod(&kModDescriptor, &mod);
    if (result != EMC_OK)
    {
        return result;
    }

    result = api->register_bool_setting(mod, &kBoolConditionControllerSettingDef);
    if (result != EMC_OK)
    {
        return result;
    }

    result = api->register_bool_setting(mod, &kBoolConditionHiddenSettingDef);
    if (result != EMC_OK)
    {
        return result;
    }

    result = api->register_bool_setting(mod, &kBoolConditionDisabledSettingDef);
    if (result != EMC_OK)
    {
        return result;
    }

    result = emc::RegisterBoolConditionRuleWithApiSizeV1(api, api->api_size, mod, &kBoolConditionHideRule);
    if (result != EMC_OK)
    {
        return result;
    }

    result = emc::RegisterBoolConditionRuleWithApiSizeV1(api, api->api_size, mod, &kBoolConditionDisableRule);
    if (result != EMC_OK)
    {
        return result;
    }
    return EMC_OK;
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
    if (g_state.mode != kModeUseBoolConditionRule && g_state.mode != kModeUseBoolConditionRuleLegacyApi)
    {
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

    const int32_t call_index = g_state.register_bool_calls - 1;
    const char* expected_setting_id = nullptr;
    void* expected_user_data = nullptr;
    if (call_index == 0)
    {
        expected_setting_id = kBoolConditionControllerSettingId;
        expected_user_data = &g_bool_condition_controller_value;
    }
    else if (call_index == 1)
    {
        expected_setting_id = kBoolConditionHiddenSettingId;
        expected_user_data = &g_bool_condition_hidden_value;
    }
    else
    {
        expected_setting_id = kBoolConditionDisabledSettingId;
        expected_user_data = &g_bool_condition_disabled_value;
    }

    if (mod != GetHandle()
        || def == 0
        || !StringEquals(def->setting_id, expected_setting_id)
        || def->get_value != &GetBool
        || def->set_value != &SetBool
        || def->user_data != expected_user_data)
    {
        g_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_OK;
}

EMC_Result __cdecl TestRegisterBoolV2(EMC_ModHandle mod, const EMC_BoolSettingDefV2* def)
{
    g_state.register_bool_v2_calls += 1;
    RecordKind(emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2);

    if (mod != GetHandle()
        || def == 0
        || !StringEquals(def->setting_id, kBoolSettingId)
        || def->get_value != &GetBool
        || def->set_value != &SetBool
        || def->user_data != &g_bool_value
        || !StringEquals(def->hover_hint, kBoolHoverHint))
    {
        g_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_OK;
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

EMC_Result __cdecl TestRegisterKeybindV2(EMC_ModHandle mod, const EMC_KeybindSettingDefV2* def)
{
    g_state.register_keybind_v2_calls += 1;
    RecordKind(emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND_V2);

    if (mod != GetHandle()
        || def == 0
        || !StringEquals(def->setting_id, kKeybindSettingId)
        || def->get_value != &GetKeybind
        || def->set_value != &SetKeybind
        || def->user_data != &g_keybind_value
        || !StringEquals(def->hover_hint, kKeybindHoverHint))
    {
        g_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_OK;
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

EMC_Result __cdecl TestRegisterSelect(EMC_ModHandle mod, const EMC_SelectSettingDefV1* def)
{
    g_state.register_select_calls += 1;
    RecordKind(emc::MOD_HUB_CLIENT_SETTING_KIND_SELECT);

    if (mod != GetHandle()
        || def == 0
        || !StringEquals(def->setting_id, kSelectSettingId)
        || def->option_count != (uint32_t)(sizeof(kSelectOptions) / sizeof(kSelectOptions[0]))
        || def->options == 0
        || def->options[0].value != 0
        || !StringEquals(def->options[0].label, "Default")
        || def->options[1].value != 1
        || !StringEquals(def->options[1].label, "Warm")
        || def->options[2].value != 2
        || !StringEquals(def->options[2].label, "Cool")
        || def->get_value != &GetSelect
        || def->set_value != &SetSelect
        || def->user_data != &g_select_value)
    {
        g_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_OK;
}

EMC_Result __cdecl TestRegisterSelectV2(EMC_ModHandle mod, const EMC_SelectSettingDefV2* def)
{
    g_state.register_select_v2_calls += 1;
    RecordKind(emc::MOD_HUB_CLIENT_SETTING_KIND_SELECT_V2);

    if (mod != GetHandle()
        || def == 0
        || !StringEquals(def->setting_id, kSelectSettingId)
        || def->option_count != (uint32_t)(sizeof(kSelectOptions) / sizeof(kSelectOptions[0]))
        || def->options == 0
        || def->options[0].value != 0
        || !StringEquals(def->options[0].label, "Default")
        || def->options[1].value != 1
        || !StringEquals(def->options[1].label, "Warm")
        || def->options[2].value != 2
        || !StringEquals(def->options[2].label, "Cool")
        || def->get_value != &GetSelect
        || def->set_value != &SetSelect
        || def->user_data != &g_select_value
        || !StringEquals(def->hover_hint, kSelectHoverHint))
    {
        g_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_OK;
}

EMC_Result __cdecl TestRegisterText(EMC_ModHandle mod, const EMC_TextSettingDefV1* def)
{
    g_state.register_text_calls += 1;
    RecordKind(emc::MOD_HUB_CLIENT_SETTING_KIND_TEXT);

    if (mod != GetHandle()
        || def == 0
        || !StringEquals(def->setting_id, kTextSettingId)
        || def->max_length != kTextMaxLength
        || def->get_value != &GetText
        || def->set_value != &SetText
        || def->user_data != g_text_value)
    {
        g_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_OK;
}

EMC_Result __cdecl TestRegisterTextV2(EMC_ModHandle mod, const EMC_TextSettingDefV2* def)
{
    g_state.register_text_v2_calls += 1;
    RecordKind(emc::MOD_HUB_CLIENT_SETTING_KIND_TEXT_V2);

    if (mod != GetHandle()
        || def == 0
        || !StringEquals(def->setting_id, kTextSettingId)
        || def->max_length != kTextMaxLength
        || def->get_value != &GetText
        || def->set_value != &SetText
        || def->user_data != g_text_value
        || !StringEquals(def->hover_hint, kTextHoverHint))
    {
        g_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_OK;
}

EMC_Result __cdecl TestRegisterColor(EMC_ModHandle mod, const EMC_ColorSettingDefV1* def)
{
    g_state.register_color_calls += 1;
    RecordKind(emc::MOD_HUB_CLIENT_SETTING_KIND_COLOR);

    if (mod != GetHandle()
        || def == 0
        || !StringEquals(def->setting_id, kColorSettingId)
        || def->preview_kind != EMC_COLOR_PREVIEW_KIND_SWATCH
        || def->preset_count != (uint32_t)(sizeof(kColorPresets) / sizeof(kColorPresets[0]))
        || def->presets == 0
        || !StringEquals(def->presets[0].value_hex, "#FF3333")
        || !StringEquals(def->presets[1].value_hex, "#DEE85A")
        || !StringEquals(def->presets[2].value_hex, "#40FF40")
        || def->get_value != &GetText
        || def->set_value != &SetText
        || def->user_data != g_color_value)
    {
        g_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_OK;
}

EMC_Result __cdecl TestRegisterSection(EMC_ModHandle mod, const EMC_SettingSectionDefV1* def)
{
    g_state.register_section_calls += 1;

    if (mod != GetHandle()
        || def == 0
        || !StringEquals(def->setting_id, kBoolSettingId)
        || !StringEquals(def->section_id, kSectionId)
        || !StringEquals(def->section_display_name, kSectionDisplayName))
    {
        g_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_OK;
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

EMC_Result __cdecl TestRegisterActionV2(EMC_ModHandle mod, const EMC_ActionRowDefV2* def)
{
    g_state.register_action_v2_calls += 1;
    RecordKind(emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION_V2);

    if (mod != GetHandle()
        || def == 0
        || !StringEquals(def->setting_id, kActionSettingId)
        || def->action_flags != EMC_ACTION_FORCE_REFRESH
        || def->on_action != &InvokeAction
        || def->user_data != &g_action_count
        || !StringEquals(def->hover_hint, kActionHoverHint))
    {
        g_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_OK;
}

EMC_Result __cdecl TestRegisterBoolConditionRule(EMC_ModHandle mod, const EMC_BoolConditionRuleDefV1* def)
{
    g_state.register_bool_condition_rule_calls += 1;

    const int32_t call_index = g_state.register_bool_condition_rule_calls - 1;
    const char* expected_target_id =
        call_index == 0 ? kBoolConditionHiddenSettingId : kBoolConditionDisabledSettingId;
    const uint32_t expected_effect =
        call_index == 0 ? EMC_BOOL_CONDITION_EFFECT_HIDE : EMC_BOOL_CONDITION_EFFECT_DISABLE;

    if (mod != GetHandle()
        || def == 0
        || !StringEquals(def->target_setting_id, expected_target_id)
        || !StringEquals(def->controller_setting_id, kBoolConditionControllerSettingId)
        || def->effect != expected_effect
        || def->expected_bool_value != 0)
    {
        g_state.descriptor_checks_passed = 0;
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_OK;
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
        &TestRegisterIntV2,
        &TestRegisterSelect,
        &TestRegisterText,
        &TestRegisterColor,
        &TestRegisterSection,
        &TestRegisterBoolV2,
        &TestRegisterKeybindV2,
        &TestRegisterSelectV2,
        &TestRegisterTextV2,
        &TestRegisterActionV2,
        &TestRegisterBoolConditionRule};
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

    if (IsLegacyApiMode())
    {
        static EMC_HubApiV1 legacy_api;
        legacy_api = *GetTestApi();
        legacy_api.api_size = ResolveLegacyApiSize();
        *out_api = &legacy_api;
        *out_api_size = legacy_api.api_size;
    }
    else
    {
        *out_api = GetTestApi();
        *out_api_size = (uint32_t)sizeof(EMC_HubApiV1);
    }
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
    ConfigureClientForMode();

    g_initialized = true;
}

void ResetState()
{
    ResetRows();
    ApplyMode();
    ResetCapture();
    ConfigureClientForMode();
}
}

void ModHubDummyConsumer_Reset()
{
    EnsureInitialized();
    g_state.mode = kModeSuccess;
    ResetState();
}

void ModHubDummyConsumer_SetMode(int32_t mode)
{
    EnsureInitialized();
    g_state.mode = mode;
    ResetState();
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

int32_t ModHubDummyConsumer_GetRegisterBoolV2Calls()
{
    EnsureInitialized();
    return g_state.register_bool_v2_calls;
}

int32_t ModHubDummyConsumer_GetRegisterKeybindCalls()
{
    EnsureInitialized();
    return g_state.register_keybind_calls;
}

int32_t ModHubDummyConsumer_GetRegisterKeybindV2Calls()
{
    EnsureInitialized();
    return g_state.register_keybind_v2_calls;
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

int32_t ModHubDummyConsumer_GetRegisterSelectCalls()
{
    EnsureInitialized();
    return g_state.register_select_calls;
}

int32_t ModHubDummyConsumer_GetRegisterSelectV2Calls()
{
    EnsureInitialized();
    return g_state.register_select_v2_calls;
}

int32_t ModHubDummyConsumer_GetRegisterTextCalls()
{
    EnsureInitialized();
    return g_state.register_text_calls;
}

int32_t ModHubDummyConsumer_GetRegisterTextV2Calls()
{
    EnsureInitialized();
    return g_state.register_text_v2_calls;
}

int32_t ModHubDummyConsumer_GetRegisterColorCalls()
{
    EnsureInitialized();
    return g_state.register_color_calls;
}

int32_t ModHubDummyConsumer_GetRegisterSectionCalls()
{
    EnsureInitialized();
    return g_state.register_section_calls;
}

int32_t ModHubDummyConsumer_GetRegisterActionCalls()
{
    EnsureInitialized();
    return g_state.register_action_calls;
}

int32_t ModHubDummyConsumer_GetRegisterActionV2Calls()
{
    EnsureInitialized();
    return g_state.register_action_v2_calls;
}

int32_t ModHubDummyConsumer_GetRegisterBoolConditionRuleCalls()
{
    EnsureInitialized();
    return g_state.register_bool_condition_rule_calls;
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
