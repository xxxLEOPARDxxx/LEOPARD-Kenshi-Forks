#ifndef EMC_HUB_REGISTRY_H
#define EMC_HUB_REGISTRY_H

#include "emc/mod_hub_api.h"

enum HubRegistrySettingKind
{
    HUB_REGISTRY_SETTING_KIND_BOOL = 0,
    HUB_REGISTRY_SETTING_KIND_KEYBIND = 1,
    HUB_REGISTRY_SETTING_KIND_INT = 2,
    HUB_REGISTRY_SETTING_KIND_FLOAT = 3,
    HUB_REGISTRY_SETTING_KIND_ACTION = 4,
    HUB_REGISTRY_SETTING_KIND_SELECT = 5,
    HUB_REGISTRY_SETTING_KIND_TEXT = 6,
    HUB_REGISTRY_SETTING_KIND_COLOR = 7
};

struct HubRegistryNamespaceView
{
    const char* namespace_id;
    const char* namespace_display_name;
};

struct HubRegistryModView
{
    const char* namespace_id;
    const char* namespace_display_name;
    const char* mod_id;
    const char* mod_display_name;
    void* mod_user_data;
    EMC_ModHandle handle;
};

enum HubRegistryBoolConditionEffect
{
    HUB_REGISTRY_BOOL_CONDITION_EFFECT_HIDE = 0,
    HUB_REGISTRY_BOOL_CONDITION_EFFECT_DISABLE = 1
};

struct HubRegistryBoolConditionRuleView
{
    const char* target_setting_id;
    const char* controller_setting_id;
    uint32_t effect;
    int32_t expected_bool_value;
};

struct HubRegistrySettingView
{
    int32_t kind;
    const char* setting_id;
    const char* label;
    const char* description;
    const char* hover_hint;
    const char* section_id;
    const char* section_display_name;
    void* user_data;

    EMC_GetBoolCallback get_bool;
    EMC_SetBoolCallback set_bool;

    EMC_GetKeybindCallback get_keybind;
    EMC_SetKeybindCallback set_keybind;

    EMC_GetIntCallback get_int;
    EMC_SetIntCallback set_int;
    int32_t int_min_value;
    int32_t int_max_value;
    int32_t int_step;
    bool int_use_custom_buttons;
    int32_t int_dec_button_deltas[3];
    int32_t int_inc_button_deltas[3];

    EMC_GetFloatCallback get_float;
    EMC_SetFloatCallback set_float;
    float float_min_value;
    float float_max_value;
    float float_step;
    uint32_t float_display_decimals;

    EMC_GetSelectCallback get_select;
    EMC_SetSelectCallback set_select;
    const EMC_SelectOptionV1* select_options;
    uint32_t select_option_count;

    EMC_GetTextCallback get_text;
    EMC_SetTextCallback set_text;
    uint32_t text_max_length;

    uint32_t color_preview_kind;
    const EMC_ColorPresetV1* color_presets;
    uint32_t color_preset_count;

    EMC_ActionRowCallback on_action;
    uint32_t action_flags;
};

typedef void(__cdecl* HubRegistryVisitSettingFn)(
    const HubRegistryNamespaceView* namespace_view,
    const HubRegistryModView* mod_view,
    const HubRegistrySettingView* setting_view,
    void* user_data);

EMC_Result __cdecl HubRegistry_RegisterMod(const EMC_ModDescriptorV1* desc, EMC_ModHandle* out_handle);
EMC_Result __cdecl HubRegistry_RegisterBoolSetting(EMC_ModHandle mod, const EMC_BoolSettingDefV1* def);
EMC_Result __cdecl HubRegistry_RegisterBoolSettingV2(EMC_ModHandle mod, const EMC_BoolSettingDefV2* def);
EMC_Result __cdecl HubRegistry_RegisterKeybindSetting(EMC_ModHandle mod, const EMC_KeybindSettingDefV1* def);
EMC_Result __cdecl HubRegistry_RegisterKeybindSettingV2(EMC_ModHandle mod, const EMC_KeybindSettingDefV2* def);
EMC_Result __cdecl HubRegistry_RegisterIntSetting(EMC_ModHandle mod, const EMC_IntSettingDefV1* def);
EMC_Result __cdecl HubRegistry_RegisterIntSettingV2(EMC_ModHandle mod, const EMC_IntSettingDefV2* def);
EMC_Result __cdecl HubRegistry_RegisterFloatSetting(EMC_ModHandle mod, const EMC_FloatSettingDefV1* def);
EMC_Result __cdecl HubRegistry_RegisterSelectSetting(EMC_ModHandle mod, const EMC_SelectSettingDefV1* def);
EMC_Result __cdecl HubRegistry_RegisterSelectSettingV2(EMC_ModHandle mod, const EMC_SelectSettingDefV2* def);
EMC_Result __cdecl HubRegistry_RegisterTextSetting(EMC_ModHandle mod, const EMC_TextSettingDefV1* def);
EMC_Result __cdecl HubRegistry_RegisterTextSettingV2(EMC_ModHandle mod, const EMC_TextSettingDefV2* def);
EMC_Result __cdecl HubRegistry_RegisterColorSetting(EMC_ModHandle mod, const EMC_ColorSettingDefV1* def);
EMC_Result __cdecl HubRegistry_RegisterSettingSection(EMC_ModHandle mod, const EMC_SettingSectionDefV1* def);
EMC_Result __cdecl HubRegistry_RegisterActionRow(EMC_ModHandle mod, const EMC_ActionRowDefV1* def);
EMC_Result __cdecl HubRegistry_RegisterActionRowV2(EMC_ModHandle mod, const EMC_ActionRowDefV2* def);
EMC_Result __cdecl HubRegistry_RegisterBoolConditionRule(EMC_ModHandle mod, const EMC_BoolConditionRuleDefV1* def);
bool HubRegistry_GetBoolConditionRuleView(
    EMC_ModHandle mod,
    const char* target_setting_id,
    HubRegistryBoolConditionRuleView* out_view);
void HubRegistry_ForEachSettingInOrder(HubRegistryVisitSettingFn visitor, void* user_data);

void HubRegistry_SetRegistrationLocked(bool is_locked);
bool HubRegistry_IsRegistrationLocked();

#endif
