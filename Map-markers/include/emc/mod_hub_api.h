#ifndef EMC_MOD_HUB_API_H
#define EMC_MOD_HUB_API_H

#include <stddef.h>
#include <stdint.h>

#if !defined(__cdecl)
#define __cdecl
#endif

#if defined(_WIN32) && defined(EMC_MOD_HUB_BUILD_DLL)
#define EMC_MOD_HUB_API __declspec(dllexport)
#else
#define EMC_MOD_HUB_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define EMC_HUB_API_VERSION_1 ((uint32_t)1u)
#define EMC_MOD_HUB_GET_API_EXPORT_NAME "EMC_ModHub_GetApi"
#define EMC_MOD_HUB_GET_API_COMPAT_EXPORT_NAME "EMC_ModHub_GetApi_v1_compat"
#define EMC_MOD_HUB_GET_API_COMPAT_REMOVAL_TARGET "v1.2.0"

typedef int32_t EMC_Result;

#define EMC_OK ((EMC_Result)0)
#define EMC_ERR_INVALID_ARGUMENT ((EMC_Result)1)
#define EMC_ERR_UNSUPPORTED_VERSION ((EMC_Result)2)
#define EMC_ERR_API_SIZE_MISMATCH ((EMC_Result)3)
#define EMC_ERR_CONFLICT ((EMC_Result)4)
#define EMC_ERR_NOT_FOUND ((EMC_Result)5)
#define EMC_ERR_CALLBACK_FAILED ((EMC_Result)6)
#define EMC_ERR_INTERNAL ((EMC_Result)7)

#define EMC_KEY_UNBOUND ((int32_t)-1)

#define EMC_ACTION_FORCE_REFRESH ((uint32_t)(1u << 0))

#define EMC_FLOAT_DISPLAY_DECIMALS_DEFAULT ((uint32_t)3u)

typedef struct EMC_ModHandle_t* EMC_ModHandle;

typedef struct EMC_KeybindValueV1
{
    int32_t keycode;
    uint32_t modifiers;
} EMC_KeybindValueV1;

typedef EMC_Result(__cdecl* EMC_GetBoolCallback)(void* user_data, int32_t* out_value);
typedef EMC_Result(__cdecl* EMC_SetBoolCallback)(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size);

typedef EMC_Result(__cdecl* EMC_GetKeybindCallback)(void* user_data, EMC_KeybindValueV1* out_value);
typedef EMC_Result(__cdecl* EMC_SetKeybindCallback)(void* user_data, EMC_KeybindValueV1 value, char* err_buf, uint32_t err_buf_size);

typedef EMC_Result(__cdecl* EMC_GetIntCallback)(void* user_data, int32_t* out_value);
typedef EMC_Result(__cdecl* EMC_SetIntCallback)(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size);

typedef EMC_Result(__cdecl* EMC_GetFloatCallback)(void* user_data, float* out_value);
typedef EMC_Result(__cdecl* EMC_SetFloatCallback)(void* user_data, float value, char* err_buf, uint32_t err_buf_size);

typedef EMC_Result(__cdecl* EMC_ActionRowCallback)(void* user_data, char* err_buf, uint32_t err_buf_size);
typedef void(__cdecl* EMC_OptionsWindowInitObserverFn)(void* user_data);

typedef struct EMC_ModDescriptorV1
{
    const char* namespace_id;
    const char* namespace_display_name;
    const char* mod_id;
    const char* mod_display_name;
    void* mod_user_data;
} EMC_ModDescriptorV1;

typedef struct EMC_BoolSettingDefV1
{
    const char* setting_id;
    const char* label;
    const char* description;
    void* user_data;
    EMC_GetBoolCallback get_value;
    EMC_SetBoolCallback set_value;
} EMC_BoolSettingDefV1;

typedef struct EMC_KeybindSettingDefV1
{
    const char* setting_id;
    const char* label;
    const char* description;
    void* user_data;
    EMC_GetKeybindCallback get_value;
    EMC_SetKeybindCallback set_value;
} EMC_KeybindSettingDefV1;

typedef struct EMC_IntSettingDefV1
{
    const char* setting_id;
    const char* label;
    const char* description;
    void* user_data;
    int32_t min_value;
    int32_t max_value;
    int32_t step;
    EMC_GetIntCallback get_value;
    EMC_SetIntCallback set_value;
} EMC_IntSettingDefV1;

typedef struct EMC_IntSettingDefV2
{
    const char* setting_id;
    const char* label;
    const char* description;
    void* user_data;
    int32_t min_value;
    int32_t max_value;
    int32_t step;
    int32_t dec_button_deltas[3];
    int32_t inc_button_deltas[3];
    EMC_GetIntCallback get_value;
    EMC_SetIntCallback set_value;
} EMC_IntSettingDefV2;

typedef struct EMC_FloatSettingDefV1
{
    const char* setting_id;
    const char* label;
    const char* description;
    void* user_data;
    float min_value;
    float max_value;
    float step;
    uint32_t display_decimals;
    EMC_GetFloatCallback get_value;
    EMC_SetFloatCallback set_value;
} EMC_FloatSettingDefV1;

typedef struct EMC_ActionRowDefV1
{
    const char* setting_id;
    const char* label;
    const char* description;
    void* user_data;
    uint32_t action_flags;
    EMC_ActionRowCallback on_action;
} EMC_ActionRowDefV1;

typedef struct EMC_HubApiV1
{
    uint32_t api_version;
    uint32_t api_size;
    EMC_Result(__cdecl* register_mod)(const EMC_ModDescriptorV1* desc, EMC_ModHandle* out_handle);
    EMC_Result(__cdecl* register_bool_setting)(EMC_ModHandle mod, const EMC_BoolSettingDefV1* def);
    EMC_Result(__cdecl* register_keybind_setting)(EMC_ModHandle mod, const EMC_KeybindSettingDefV1* def);
    EMC_Result(__cdecl* register_int_setting)(EMC_ModHandle mod, const EMC_IntSettingDefV1* def);
    EMC_Result(__cdecl* register_float_setting)(EMC_ModHandle mod, const EMC_FloatSettingDefV1* def);
    EMC_Result(__cdecl* register_action_row)(EMC_ModHandle mod, const EMC_ActionRowDefV1* def);
    EMC_Result(__cdecl* register_options_window_init_observer)(EMC_OptionsWindowInitObserverFn observer_fn, void* user_data);
    EMC_Result(__cdecl* unregister_options_window_init_observer)(EMC_OptionsWindowInitObserverFn observer_fn, void* user_data);
    EMC_Result(__cdecl* register_int_setting_v2)(EMC_ModHandle mod, const EMC_IntSettingDefV2* def);
} EMC_HubApiV1;

#define EMC_HUB_API_V1_MIN_SIZE ((uint32_t)56u)
#define EMC_HUB_API_V1_OPTIONS_WINDOW_INIT_OBSERVER_MIN_SIZE \
    ((uint32_t)(offsetof(EMC_HubApiV1, unregister_options_window_init_observer) + sizeof(void*)))
#define EMC_HUB_API_V1_INT_SETTING_V2_MIN_SIZE \
    ((uint32_t)(offsetof(EMC_HubApiV1, register_int_setting_v2) + sizeof(void*)))

EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_GetApi(
    uint32_t requested_version,
    uint32_t caller_api_size,
    const EMC_HubApiV1** out_api,
    uint32_t* out_api_size);

/* Temporary compatibility alias. Scheduled for removal after EMC_MOD_HUB_GET_API_COMPAT_REMOVAL_TARGET. */
EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_GetApi_v1_compat(
    uint32_t requested_version,
    uint32_t caller_api_size,
    const EMC_HubApiV1** out_api,
    uint32_t* out_api_size);

#ifdef __cplusplus
}
#endif

#if defined(__cplusplus)
#define EMC_ABI_STATIC_ASSERT(expr, msg) static_assert((expr), msg)
#else
#define EMC_ABI_STATIC_ASSERT(expr, msg) _Static_assert((expr), msg)
#endif

#define EMC_ABI_ASSERT_SIZE(type_name, expected_size) \
    EMC_ABI_STATIC_ASSERT(sizeof(type_name) == (expected_size), #type_name " has unexpected size")

#define EMC_ABI_ASSERT_OFFSET(type_name, field_name, expected_offset) \
    EMC_ABI_STATIC_ASSERT(offsetof(type_name, field_name) == (expected_offset), #type_name "." #field_name " has unexpected offset")

EMC_ABI_STATIC_ASSERT(sizeof(void*) == 8, "EMC SDK v1 requires 64-bit builds.");

EMC_ABI_ASSERT_SIZE(EMC_KeybindValueV1, 8);
EMC_ABI_ASSERT_OFFSET(EMC_KeybindValueV1, keycode, 0);
EMC_ABI_ASSERT_OFFSET(EMC_KeybindValueV1, modifiers, 4);

EMC_ABI_ASSERT_SIZE(EMC_ModDescriptorV1, 40);
EMC_ABI_ASSERT_OFFSET(EMC_ModDescriptorV1, namespace_id, 0);
EMC_ABI_ASSERT_OFFSET(EMC_ModDescriptorV1, namespace_display_name, 8);
EMC_ABI_ASSERT_OFFSET(EMC_ModDescriptorV1, mod_id, 16);
EMC_ABI_ASSERT_OFFSET(EMC_ModDescriptorV1, mod_display_name, 24);
EMC_ABI_ASSERT_OFFSET(EMC_ModDescriptorV1, mod_user_data, 32);

EMC_ABI_ASSERT_SIZE(EMC_BoolSettingDefV1, 48);
EMC_ABI_ASSERT_OFFSET(EMC_BoolSettingDefV1, setting_id, 0);
EMC_ABI_ASSERT_OFFSET(EMC_BoolSettingDefV1, label, 8);
EMC_ABI_ASSERT_OFFSET(EMC_BoolSettingDefV1, description, 16);
EMC_ABI_ASSERT_OFFSET(EMC_BoolSettingDefV1, user_data, 24);
EMC_ABI_ASSERT_OFFSET(EMC_BoolSettingDefV1, get_value, 32);
EMC_ABI_ASSERT_OFFSET(EMC_BoolSettingDefV1, set_value, 40);

EMC_ABI_ASSERT_SIZE(EMC_KeybindSettingDefV1, 48);
EMC_ABI_ASSERT_OFFSET(EMC_KeybindSettingDefV1, setting_id, 0);
EMC_ABI_ASSERT_OFFSET(EMC_KeybindSettingDefV1, label, 8);
EMC_ABI_ASSERT_OFFSET(EMC_KeybindSettingDefV1, description, 16);
EMC_ABI_ASSERT_OFFSET(EMC_KeybindSettingDefV1, user_data, 24);
EMC_ABI_ASSERT_OFFSET(EMC_KeybindSettingDefV1, get_value, 32);
EMC_ABI_ASSERT_OFFSET(EMC_KeybindSettingDefV1, set_value, 40);

EMC_ABI_ASSERT_SIZE(EMC_IntSettingDefV1, 64);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV1, setting_id, 0);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV1, label, 8);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV1, description, 16);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV1, user_data, 24);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV1, min_value, 32);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV1, max_value, 36);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV1, step, 40);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV1, get_value, 48);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV1, set_value, 56);

EMC_ABI_ASSERT_SIZE(EMC_IntSettingDefV2, 88);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV2, setting_id, 0);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV2, label, 8);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV2, description, 16);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV2, user_data, 24);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV2, min_value, 32);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV2, max_value, 36);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV2, step, 40);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV2, dec_button_deltas, 44);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV2, inc_button_deltas, 56);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV2, get_value, 72);
EMC_ABI_ASSERT_OFFSET(EMC_IntSettingDefV2, set_value, 80);

EMC_ABI_ASSERT_SIZE(EMC_FloatSettingDefV1, 64);
EMC_ABI_ASSERT_OFFSET(EMC_FloatSettingDefV1, setting_id, 0);
EMC_ABI_ASSERT_OFFSET(EMC_FloatSettingDefV1, label, 8);
EMC_ABI_ASSERT_OFFSET(EMC_FloatSettingDefV1, description, 16);
EMC_ABI_ASSERT_OFFSET(EMC_FloatSettingDefV1, user_data, 24);
EMC_ABI_ASSERT_OFFSET(EMC_FloatSettingDefV1, min_value, 32);
EMC_ABI_ASSERT_OFFSET(EMC_FloatSettingDefV1, max_value, 36);
EMC_ABI_ASSERT_OFFSET(EMC_FloatSettingDefV1, step, 40);
EMC_ABI_ASSERT_OFFSET(EMC_FloatSettingDefV1, display_decimals, 44);
EMC_ABI_ASSERT_OFFSET(EMC_FloatSettingDefV1, get_value, 48);
EMC_ABI_ASSERT_OFFSET(EMC_FloatSettingDefV1, set_value, 56);

EMC_ABI_ASSERT_SIZE(EMC_ActionRowDefV1, 48);
EMC_ABI_ASSERT_OFFSET(EMC_ActionRowDefV1, setting_id, 0);
EMC_ABI_ASSERT_OFFSET(EMC_ActionRowDefV1, label, 8);
EMC_ABI_ASSERT_OFFSET(EMC_ActionRowDefV1, description, 16);
EMC_ABI_ASSERT_OFFSET(EMC_ActionRowDefV1, user_data, 24);
EMC_ABI_ASSERT_OFFSET(EMC_ActionRowDefV1, action_flags, 32);
EMC_ABI_ASSERT_OFFSET(EMC_ActionRowDefV1, on_action, 40);

EMC_ABI_ASSERT_SIZE(EMC_HubApiV1, 80);
EMC_ABI_ASSERT_OFFSET(EMC_HubApiV1, api_version, 0);
EMC_ABI_ASSERT_OFFSET(EMC_HubApiV1, api_size, 4);
EMC_ABI_ASSERT_OFFSET(EMC_HubApiV1, register_mod, 8);
EMC_ABI_ASSERT_OFFSET(EMC_HubApiV1, register_bool_setting, 16);
EMC_ABI_ASSERT_OFFSET(EMC_HubApiV1, register_keybind_setting, 24);
EMC_ABI_ASSERT_OFFSET(EMC_HubApiV1, register_int_setting, 32);
EMC_ABI_ASSERT_OFFSET(EMC_HubApiV1, register_float_setting, 40);
EMC_ABI_ASSERT_OFFSET(EMC_HubApiV1, register_action_row, 48);
EMC_ABI_ASSERT_OFFSET(EMC_HubApiV1, register_options_window_init_observer, 56);
EMC_ABI_ASSERT_OFFSET(EMC_HubApiV1, unregister_options_window_init_observer, 64);
EMC_ABI_ASSERT_OFFSET(EMC_HubApiV1, register_int_setting_v2, 72);

#undef EMC_ABI_ASSERT_OFFSET
#undef EMC_ABI_ASSERT_SIZE
#undef EMC_ABI_STATIC_ASSERT

#endif
