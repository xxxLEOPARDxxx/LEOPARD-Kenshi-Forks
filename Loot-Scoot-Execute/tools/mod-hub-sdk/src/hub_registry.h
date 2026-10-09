#ifndef EMC_HUB_REGISTRY_H
#define EMC_HUB_REGISTRY_H

#include "emc/mod_hub_api.h"

enum HubRegistrySettingKind
{
    HUB_REGISTRY_SETTING_KIND_BOOL = 0,
    HUB_REGISTRY_SETTING_KIND_KEYBIND = 1,
    HUB_REGISTRY_SETTING_KIND_INT = 2,
    HUB_REGISTRY_SETTING_KIND_FLOAT = 3,
    HUB_REGISTRY_SETTING_KIND_ACTION = 4
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

struct HubRegistrySettingView
{
    int32_t kind;
    const char* setting_id;
    const char* label;
    const char* description;
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

    EMC_GetFloatCallback get_float;
    EMC_SetFloatCallback set_float;
    float float_min_value;
    float float_max_value;
    float float_step;
    uint32_t float_display_decimals;

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
EMC_Result __cdecl HubRegistry_RegisterKeybindSetting(EMC_ModHandle mod, const EMC_KeybindSettingDefV1* def);
EMC_Result __cdecl HubRegistry_RegisterIntSetting(EMC_ModHandle mod, const EMC_IntSettingDefV1* def);
EMC_Result __cdecl HubRegistry_RegisterFloatSetting(EMC_ModHandle mod, const EMC_FloatSettingDefV1* def);
EMC_Result __cdecl HubRegistry_RegisterActionRow(EMC_ModHandle mod, const EMC_ActionRowDefV1* def);
void HubRegistry_ForEachSettingInOrder(HubRegistryVisitSettingFn visitor, void* user_data);

void HubRegistry_SetRegistrationLocked(bool is_locked);
bool HubRegistry_IsRegistrationLocked();

#endif
