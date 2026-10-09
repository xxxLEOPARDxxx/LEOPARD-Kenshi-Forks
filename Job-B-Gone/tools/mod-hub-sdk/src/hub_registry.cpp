#include "hub_registry.h"
#include "logging.h"

#include <Debug.h>

#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct EMC_ModHandle_t
{
    uint32_t sentinel;
};

namespace
{
const char* kLogNone = "none";
const uint32_t kMaxIdLength = 64u;
const char* kEnvRegistrationLocked = "EMC_HUB_REGISTRATION_LOCKED";

enum SettingKind
{
    SETTING_KIND_BOOL = 0,
    SETTING_KIND_KEYBIND = 1,
    SETTING_KIND_INT = 2,
    SETTING_KIND_FLOAT = 3,
    SETTING_KIND_ACTION = 4
};

struct SettingEntry
{
    SettingKind kind;
    std::string setting_id;
    std::string label;
    std::string description;
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

    EMC_ActionRowCallback on_action;
    uint32_t action_flags;
};

struct ModEntry
{
    std::string namespace_id;
    std::string mod_id;
    std::string mod_display_name;
    void* mod_user_data;
    EMC_ModHandle handle;

    std::vector<SettingEntry*> settings_in_order;
    std::map<std::string, SettingEntry*> settings_by_id;
};

struct NamespaceEntry
{
    std::string namespace_id;
    std::string namespace_display_name;

    std::vector<ModEntry*> mods_in_order;
    std::map<std::string, ModEntry*> mods_by_id;
};

bool g_registration_locked = false;
std::vector<NamespaceEntry*> g_namespaces_in_order;
std::map<std::string, NamespaceEntry*> g_namespaces_by_id;
std::map<EMC_ModHandle, ModEntry*> g_mods_by_handle;
const int32_t kDefaultIntDecButtonDeltas[3] = { 10, 5, 1 };
const int32_t kDefaultIntIncButtonDeltas[3] = { 1, 5, 10 };

const char* SafeLogValue(const char* value)
{
    if (value == nullptr)
    {
        return kLogNone;
    }

    if (value[0] == '\0')
    {
        return kLogNone;
    }

    return value;
}

void LogRegistrationWarning(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    const char* field,
    const char* message)
{
    std::ostringstream line;
    line << "event=hub_registration_warning"
         << " namespace=" << SafeLogValue(namespace_id)
         << " mod=" << SafeLogValue(mod_id)
         << " setting=" << SafeLogValue(setting_id)
         << " field=" << SafeLogValue(field)
         << " message=" << SafeLogValue(message);
    LogDebugLine(line.str());
}

void LogSettingRegistrationConflict(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    EMC_Result result,
    const char* message)
{
    std::ostringstream line;
    line << "event=hub_setting_registration_conflict"
         << " namespace=" << SafeLogValue(namespace_id)
         << " mod=" << SafeLogValue(mod_id)
         << " setting=" << SafeLogValue(setting_id)
         << " result=" << result
         << " message=" << SafeLogValue(message);
    ErrorLog(line.str().c_str());
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

bool IsAllowedIdChar(char c)
{
    if (c >= 'a' && c <= 'z')
    {
        return true;
    }

    if (c >= '0' && c <= '9')
    {
        return true;
    }

    return c == '_' || c == '.' || c == '-';
}

bool IsValidId(const char* value)
{
    if (value == nullptr)
    {
        return false;
    }

    const size_t length = std::strlen(value);
    if (length == 0 || length > kMaxIdLength)
    {
        return false;
    }

    for (size_t i = 0; i < length; ++i)
    {
        if (!IsAllowedIdChar(value[i]))
        {
            return false;
        }
    }

    return true;
}

bool StringEquals(const std::string& stored_value, const char* incoming_value)
{
    if (incoming_value == nullptr)
    {
        return false;
    }

    return stored_value == incoming_value;
}

bool FloatBitsEqual(float lhs, float rhs)
{
    uint32_t lhs_bits = 0;
    uint32_t rhs_bits = 0;
    std::memcpy(&lhs_bits, &lhs, sizeof(lhs_bits));
    std::memcpy(&rhs_bits, &rhs, sizeof(rhs_bits));
    return lhs_bits == rhs_bits;
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

bool IsRegistrationLockForcedByEnv()
{
    return IsEnvTruthy(std::getenv(kEnvRegistrationLocked));
}

bool RejectIfRegistrationLocked(const char* api_name)
{
    if (!g_registration_locked && !IsRegistrationLockForcedByEnv())
    {
        return false;
    }

    LogRegistrationRejected(
        api_name,
        "options_window_open",
        EMC_ERR_INVALID_ARGUMENT,
        "registration_while_options_window_open");
    return true;
}

ModEntry* FindModByHandle(EMC_ModHandle handle)
{
    std::map<EMC_ModHandle, ModEntry*>::const_iterator it = g_mods_by_handle.find(handle);
    if (it == g_mods_by_handle.end())
    {
        return nullptr;
    }

    return it->second;
}

bool ValidateCommonSettingStrings(const char* setting_id, const char* label, const char* description)
{
    return IsValidId(setting_id) && label != nullptr && description != nullptr;
}

void LogSettingWarning(
    const ModEntry* mod,
    const char* setting_id,
    const char* field,
    const char* message)
{
    LogRegistrationWarning(
        mod->namespace_id.c_str(),
        mod->mod_id.c_str(),
        setting_id,
        field,
        message);
}

EMC_Result ValidateSettingRegistrationCall(
    EMC_ModHandle mod,
    const void* def,
    const char* api_name,
    ModEntry** out_mod)
{
    if (mod == nullptr || def == nullptr || out_mod == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (RejectIfRegistrationLocked(api_name))
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    ModEntry* mod_entry = FindModByHandle(mod);
    if (mod_entry == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_mod = mod_entry;
    return EMC_OK;
}

void InitializeSettingDefaults(SettingEntry* setting)
{
    setting->user_data = nullptr;
    setting->get_bool = nullptr;
    setting->set_bool = nullptr;
    setting->get_keybind = nullptr;
    setting->set_keybind = nullptr;
    setting->get_int = nullptr;
    setting->set_int = nullptr;
    setting->int_min_value = 0;
    setting->int_max_value = 0;
    setting->int_step = 0;
    setting->int_use_custom_buttons = false;
    std::memcpy(setting->int_dec_button_deltas, kDefaultIntDecButtonDeltas, sizeof(setting->int_dec_button_deltas));
    std::memcpy(setting->int_inc_button_deltas, kDefaultIntIncButtonDeltas, sizeof(setting->int_inc_button_deltas));
    setting->get_float = nullptr;
    setting->set_float = nullptr;
    setting->float_min_value = 0.0f;
    setting->float_max_value = 0.0f;
    setting->float_step = 0.0f;
    setting->float_display_decimals = 0;
    setting->on_action = nullptr;
    setting->action_flags = 0;
}

bool IntButtonLayoutsEqual(const int32_t* lhs, const int32_t* rhs)
{
    return std::memcmp(lhs, rhs, sizeof(int32_t) * 3u) == 0;
}

bool ValidateCustomIntButtonSide(const int32_t* deltas, int32_t step, bool is_decrement_side)
{
    int32_t previous_non_zero = 0;
    for (int32_t index = 0; index < 3; ++index)
    {
        const int32_t delta = deltas[index];
        if (delta == 0)
        {
            continue;
        }

        if (delta < 0 || step < 1 || (delta % step) != 0)
        {
            return false;
        }

        for (int32_t previous_index = 0; previous_index < index; ++previous_index)
        {
            if (deltas[previous_index] == delta)
            {
                return false;
            }
        }

        if (previous_non_zero != 0)
        {
            if (is_decrement_side)
            {
                if (delta >= previous_non_zero)
                {
                    return false;
                }
            }
            else if (delta <= previous_non_zero)
            {
                return false;
            }
        }

        previous_non_zero = delta;
    }

    return true;
}

bool ValidateCustomIntButtonLayout(const EMC_IntSettingDefV2* def)
{
    if (def == 0 || def->step < 1)
    {
        return false;
    }

    return ValidateCustomIntButtonSide(def->dec_button_deltas, def->step, true)
        && ValidateCustomIntButtonSide(def->inc_button_deltas, def->step, false);
}

bool EmitCommonSettingDriftWarnings(
    const ModEntry* mod,
    const SettingEntry* existing,
    const char* label,
    const char* description,
    void* user_data)
{
    bool exact_match = true;

    if (!StringEquals(existing->label, label))
    {
        LogSettingWarning(mod, existing->setting_id.c_str(), "label", "label_drift_ignored_using_canonical");
        exact_match = false;
    }

    if (!StringEquals(existing->description, description))
    {
        LogSettingWarning(mod, existing->setting_id.c_str(), "description", "description_drift_ignored_using_canonical");
        exact_match = false;
    }

    if (existing->user_data != user_data)
    {
        LogSettingWarning(mod, existing->setting_id.c_str(), "user_data", "user_data_drift_ignored_using_canonical");
        exact_match = false;
    }

    return exact_match;
}

void AppendNewSetting(ModEntry* mod, SettingEntry* setting)
{
    mod->settings_in_order.push_back(setting);
    mod->settings_by_id[setting->setting_id] = setting;
}

void PopulateSettingView(const SettingEntry* setting, HubRegistrySettingView* out_view)
{
    std::memset(out_view, 0, sizeof(HubRegistrySettingView));

    out_view->kind = static_cast<int32_t>(setting->kind);
    out_view->setting_id = setting->setting_id.c_str();
    out_view->label = setting->label.c_str();
    out_view->description = setting->description.c_str();
    out_view->user_data = setting->user_data;

    out_view->get_bool = setting->get_bool;
    out_view->set_bool = setting->set_bool;

    out_view->get_keybind = setting->get_keybind;
    out_view->set_keybind = setting->set_keybind;

    out_view->get_int = setting->get_int;
    out_view->set_int = setting->set_int;
    out_view->int_min_value = setting->int_min_value;
    out_view->int_max_value = setting->int_max_value;
    out_view->int_step = setting->int_step;
    out_view->int_use_custom_buttons = setting->int_use_custom_buttons;
    std::memcpy(out_view->int_dec_button_deltas, setting->int_dec_button_deltas, sizeof(out_view->int_dec_button_deltas));
    std::memcpy(out_view->int_inc_button_deltas, setting->int_inc_button_deltas, sizeof(out_view->int_inc_button_deltas));

    out_view->get_float = setting->get_float;
    out_view->set_float = setting->set_float;
    out_view->float_min_value = setting->float_min_value;
    out_view->float_max_value = setting->float_max_value;
    out_view->float_step = setting->float_step;
    out_view->float_display_decimals = setting->float_display_decimals;

    out_view->on_action = setting->on_action;
    out_view->action_flags = setting->action_flags;
}
}

void HubRegistry_SetRegistrationLocked(bool is_locked)
{
    g_registration_locked = is_locked;
}

bool HubRegistry_IsRegistrationLocked()
{
    return g_registration_locked;
}

EMC_Result __cdecl HubRegistry_RegisterMod(const EMC_ModDescriptorV1* desc, EMC_ModHandle* out_handle)
{
    if (out_handle != nullptr)
    {
        *out_handle = nullptr;
    }

    if (desc == nullptr || out_handle == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (RejectIfRegistrationLocked("register_mod"))
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (!IsValidId(desc->namespace_id) || !IsValidId(desc->mod_id))
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (desc->namespace_display_name == nullptr || desc->mod_display_name == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    NamespaceEntry* namespace_entry = nullptr;
    std::map<std::string, NamespaceEntry*>::const_iterator ns_it = g_namespaces_by_id.find(desc->namespace_id);
    if (ns_it == g_namespaces_by_id.end())
    {
        namespace_entry = new NamespaceEntry();
        namespace_entry->namespace_id = desc->namespace_id;
        namespace_entry->namespace_display_name = desc->namespace_display_name;
        g_namespaces_in_order.push_back(namespace_entry);
        g_namespaces_by_id[namespace_entry->namespace_id] = namespace_entry;
    }
    else
    {
        namespace_entry = ns_it->second;
        if (!StringEquals(namespace_entry->namespace_display_name, desc->namespace_display_name))
        {
            LogRegistrationWarning(
                desc->namespace_id,
                desc->mod_id,
                nullptr,
                "namespace_display_name",
                "namespace_display_name_drift_ignored_using_canonical");
        }
    }

    std::map<std::string, ModEntry*>::const_iterator mod_it = namespace_entry->mods_by_id.find(desc->mod_id);
    if (mod_it != namespace_entry->mods_by_id.end())
    {
        ModEntry* existing_mod = mod_it->second;
        if (!StringEquals(existing_mod->mod_display_name, desc->mod_display_name))
        {
            LogRegistrationWarning(
                existing_mod->namespace_id.c_str(),
                existing_mod->mod_id.c_str(),
                nullptr,
                "mod_display_name",
                "mod_display_name_drift_ignored_using_canonical");
        }

        if (existing_mod->mod_user_data != desc->mod_user_data)
        {
            LogRegistrationWarning(
                existing_mod->namespace_id.c_str(),
                existing_mod->mod_id.c_str(),
                nullptr,
                "mod_user_data",
                "mod_user_data_drift_ignored_using_canonical");
        }

        *out_handle = existing_mod->handle;
        return EMC_OK;
    }

    ModEntry* new_mod = new ModEntry();
    new_mod->namespace_id = namespace_entry->namespace_id;
    new_mod->mod_id = desc->mod_id;
    new_mod->mod_display_name = desc->mod_display_name;
    new_mod->mod_user_data = desc->mod_user_data;
    new_mod->handle = new EMC_ModHandle_t();

    namespace_entry->mods_in_order.push_back(new_mod);
    namespace_entry->mods_by_id[new_mod->mod_id] = new_mod;
    g_mods_by_handle[new_mod->handle] = new_mod;

    *out_handle = new_mod->handle;
    return EMC_OK;
}

EMC_Result __cdecl HubRegistry_RegisterBoolSetting(EMC_ModHandle mod, const EMC_BoolSettingDefV1* def)
{
    ModEntry* mod_entry = nullptr;
    EMC_Result validation_result = ValidateSettingRegistrationCall(mod, def, "register_bool_setting", &mod_entry);
    if (validation_result != EMC_OK)
    {
        return validation_result;
    }

    if (!ValidateCommonSettingStrings(def->setting_id, def->label, def->description))
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (def->get_value == nullptr || def->set_value == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    std::map<std::string, SettingEntry*>::const_iterator it = mod_entry->settings_by_id.find(def->setting_id);
    if (it != mod_entry->settings_by_id.end())
    {
        SettingEntry* existing = it->second;
        if (existing->kind != SETTING_KIND_BOOL)
        {
            LogSettingRegistrationConflict(
                mod_entry->namespace_id.c_str(),
                mod_entry->mod_id.c_str(),
                def->setting_id,
                EMC_ERR_CONFLICT,
                "setting_id_already_registered_with_different_kind");
            return EMC_ERR_CONFLICT;
        }

        bool exact_match = EmitCommonSettingDriftWarnings(mod_entry, existing, def->label, def->description, def->user_data);
        if (existing->get_bool != def->get_value || existing->set_bool != def->set_value)
        {
            LogSettingWarning(mod_entry, def->setting_id, "callback", "callback_drift_ignored_using_canonical");
            exact_match = false;
        }

        (void)exact_match;
        return EMC_OK;
    }

    SettingEntry* setting = new SettingEntry();
    InitializeSettingDefaults(setting);
    setting->kind = SETTING_KIND_BOOL;
    setting->setting_id = def->setting_id;
    setting->label = def->label;
    setting->description = def->description;
    setting->user_data = def->user_data;
    setting->get_bool = def->get_value;
    setting->set_bool = def->set_value;
    AppendNewSetting(mod_entry, setting);
    return EMC_OK;
}

EMC_Result __cdecl HubRegistry_RegisterKeybindSetting(EMC_ModHandle mod, const EMC_KeybindSettingDefV1* def)
{
    ModEntry* mod_entry = nullptr;
    EMC_Result validation_result = ValidateSettingRegistrationCall(mod, def, "register_keybind_setting", &mod_entry);
    if (validation_result != EMC_OK)
    {
        return validation_result;
    }

    if (!ValidateCommonSettingStrings(def->setting_id, def->label, def->description))
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (def->get_value == nullptr || def->set_value == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    std::map<std::string, SettingEntry*>::const_iterator it = mod_entry->settings_by_id.find(def->setting_id);
    if (it != mod_entry->settings_by_id.end())
    {
        SettingEntry* existing = it->second;
        if (existing->kind != SETTING_KIND_KEYBIND)
        {
            LogSettingRegistrationConflict(
                mod_entry->namespace_id.c_str(),
                mod_entry->mod_id.c_str(),
                def->setting_id,
                EMC_ERR_CONFLICT,
                "setting_id_already_registered_with_different_kind");
            return EMC_ERR_CONFLICT;
        }

        bool exact_match = EmitCommonSettingDriftWarnings(mod_entry, existing, def->label, def->description, def->user_data);
        if (existing->get_keybind != def->get_value || existing->set_keybind != def->set_value)
        {
            LogSettingWarning(mod_entry, def->setting_id, "callback", "callback_drift_ignored_using_canonical");
            exact_match = false;
        }

        (void)exact_match;
        return EMC_OK;
    }

    SettingEntry* setting = new SettingEntry();
    InitializeSettingDefaults(setting);
    setting->kind = SETTING_KIND_KEYBIND;
    setting->setting_id = def->setting_id;
    setting->label = def->label;
    setting->description = def->description;
    setting->user_data = def->user_data;
    setting->get_keybind = def->get_value;
    setting->set_keybind = def->set_value;
    AppendNewSetting(mod_entry, setting);
    return EMC_OK;
}

EMC_Result __cdecl HubRegistry_RegisterIntSetting(EMC_ModHandle mod, const EMC_IntSettingDefV1* def)
{
    ModEntry* mod_entry = nullptr;
    EMC_Result validation_result = ValidateSettingRegistrationCall(mod, def, "register_int_setting", &mod_entry);
    if (validation_result != EMC_OK)
    {
        return validation_result;
    }

    if (!ValidateCommonSettingStrings(def->setting_id, def->label, def->description))
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (def->get_value == nullptr || def->set_value == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (def->step < 1 || def->min_value > def->max_value)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    std::map<std::string, SettingEntry*>::const_iterator it = mod_entry->settings_by_id.find(def->setting_id);
    if (it != mod_entry->settings_by_id.end())
    {
        SettingEntry* existing = it->second;
        if (existing->kind != SETTING_KIND_INT)
        {
            LogSettingRegistrationConflict(
                mod_entry->namespace_id.c_str(),
                mod_entry->mod_id.c_str(),
                def->setting_id,
                EMC_ERR_CONFLICT,
                "setting_id_already_registered_with_different_kind");
            return EMC_ERR_CONFLICT;
        }

        bool exact_match = EmitCommonSettingDriftWarnings(mod_entry, existing, def->label, def->description, def->user_data);
        if (existing->get_int != def->get_value || existing->set_int != def->set_value)
        {
            LogSettingWarning(mod_entry, def->setting_id, "callback", "callback_drift_ignored_using_canonical");
            exact_match = false;
        }

        if (existing->int_min_value != def->min_value
            || existing->int_max_value != def->max_value
            || existing->int_step != def->step
            || existing->int_use_custom_buttons
            || !IntButtonLayoutsEqual(existing->int_dec_button_deltas, kDefaultIntDecButtonDeltas)
            || !IntButtonLayoutsEqual(existing->int_inc_button_deltas, kDefaultIntIncButtonDeltas))
        {
            LogSettingWarning(
                mod_entry,
                def->setting_id,
                "description",
                "numeric_metadata_drift_ignored_using_canonical");
            exact_match = false;
        }

        (void)exact_match;
        return EMC_OK;
    }

    SettingEntry* setting = new SettingEntry();
    InitializeSettingDefaults(setting);
    setting->kind = SETTING_KIND_INT;
    setting->setting_id = def->setting_id;
    setting->label = def->label;
    setting->description = def->description;
    setting->user_data = def->user_data;
    setting->get_int = def->get_value;
    setting->set_int = def->set_value;
    setting->int_min_value = def->min_value;
    setting->int_max_value = def->max_value;
    setting->int_step = def->step;
    setting->int_use_custom_buttons = false;
    std::memcpy(setting->int_dec_button_deltas, kDefaultIntDecButtonDeltas, sizeof(setting->int_dec_button_deltas));
    std::memcpy(setting->int_inc_button_deltas, kDefaultIntIncButtonDeltas, sizeof(setting->int_inc_button_deltas));
    AppendNewSetting(mod_entry, setting);
    return EMC_OK;
}

EMC_Result __cdecl HubRegistry_RegisterIntSettingV2(EMC_ModHandle mod, const EMC_IntSettingDefV2* def)
{
    ModEntry* mod_entry = nullptr;
    EMC_Result validation_result = ValidateSettingRegistrationCall(mod, def, "register_int_setting_v2", &mod_entry);
    if (validation_result != EMC_OK)
    {
        return validation_result;
    }

    if (!ValidateCommonSettingStrings(def->setting_id, def->label, def->description))
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (def->get_value == nullptr || def->set_value == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (def->step < 1 || def->min_value > def->max_value || !ValidateCustomIntButtonLayout(def))
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    std::map<std::string, SettingEntry*>::const_iterator it = mod_entry->settings_by_id.find(def->setting_id);
    if (it != mod_entry->settings_by_id.end())
    {
        SettingEntry* existing = it->second;
        if (existing->kind != SETTING_KIND_INT)
        {
            LogSettingRegistrationConflict(
                mod_entry->namespace_id.c_str(),
                mod_entry->mod_id.c_str(),
                def->setting_id,
                EMC_ERR_CONFLICT,
                "setting_id_already_registered_with_different_kind");
            return EMC_ERR_CONFLICT;
        }

        bool exact_match = EmitCommonSettingDriftWarnings(mod_entry, existing, def->label, def->description, def->user_data);
        if (existing->get_int != def->get_value || existing->set_int != def->set_value)
        {
            LogSettingWarning(mod_entry, def->setting_id, "callback", "callback_drift_ignored_using_canonical");
            exact_match = false;
        }

        if (existing->int_min_value != def->min_value
            || existing->int_max_value != def->max_value
            || existing->int_step != def->step
            || !existing->int_use_custom_buttons
            || !IntButtonLayoutsEqual(existing->int_dec_button_deltas, def->dec_button_deltas)
            || !IntButtonLayoutsEqual(existing->int_inc_button_deltas, def->inc_button_deltas))
        {
            LogSettingWarning(
                mod_entry,
                def->setting_id,
                "description",
                "numeric_metadata_drift_ignored_using_canonical");
            exact_match = false;
        }

        (void)exact_match;
        return EMC_OK;
    }

    SettingEntry* setting = new SettingEntry();
    InitializeSettingDefaults(setting);
    setting->kind = SETTING_KIND_INT;
    setting->setting_id = def->setting_id;
    setting->label = def->label;
    setting->description = def->description;
    setting->user_data = def->user_data;
    setting->get_int = def->get_value;
    setting->set_int = def->set_value;
    setting->int_min_value = def->min_value;
    setting->int_max_value = def->max_value;
    setting->int_step = def->step;
    setting->int_use_custom_buttons = true;
    std::memcpy(setting->int_dec_button_deltas, def->dec_button_deltas, sizeof(setting->int_dec_button_deltas));
    std::memcpy(setting->int_inc_button_deltas, def->inc_button_deltas, sizeof(setting->int_inc_button_deltas));
    AppendNewSetting(mod_entry, setting);
    return EMC_OK;
}

EMC_Result __cdecl HubRegistry_RegisterFloatSetting(EMC_ModHandle mod, const EMC_FloatSettingDefV1* def)
{
    ModEntry* mod_entry = nullptr;
    EMC_Result validation_result = ValidateSettingRegistrationCall(mod, def, "register_float_setting", &mod_entry);
    if (validation_result != EMC_OK)
    {
        return validation_result;
    }

    if (!ValidateCommonSettingStrings(def->setting_id, def->label, def->description))
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (def->get_value == nullptr || def->set_value == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (def->step <= 0.0f || def->min_value > def->max_value || def->display_decimals > 3u)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    std::map<std::string, SettingEntry*>::const_iterator it = mod_entry->settings_by_id.find(def->setting_id);
    if (it != mod_entry->settings_by_id.end())
    {
        SettingEntry* existing = it->second;
        if (existing->kind != SETTING_KIND_FLOAT)
        {
            LogSettingRegistrationConflict(
                mod_entry->namespace_id.c_str(),
                mod_entry->mod_id.c_str(),
                def->setting_id,
                EMC_ERR_CONFLICT,
                "setting_id_already_registered_with_different_kind");
            return EMC_ERR_CONFLICT;
        }

        bool exact_match = EmitCommonSettingDriftWarnings(mod_entry, existing, def->label, def->description, def->user_data);
        if (existing->get_float != def->get_value || existing->set_float != def->set_value)
        {
            LogSettingWarning(mod_entry, def->setting_id, "callback", "callback_drift_ignored_using_canonical");
            exact_match = false;
        }

        if (!FloatBitsEqual(existing->float_min_value, def->min_value)
            || !FloatBitsEqual(existing->float_max_value, def->max_value)
            || !FloatBitsEqual(existing->float_step, def->step)
            || existing->float_display_decimals != def->display_decimals)
        {
            LogSettingWarning(
                mod_entry,
                def->setting_id,
                "description",
                "numeric_metadata_drift_ignored_using_canonical");
            exact_match = false;
        }

        (void)exact_match;
        return EMC_OK;
    }

    SettingEntry* setting = new SettingEntry();
    InitializeSettingDefaults(setting);
    setting->kind = SETTING_KIND_FLOAT;
    setting->setting_id = def->setting_id;
    setting->label = def->label;
    setting->description = def->description;
    setting->user_data = def->user_data;
    setting->get_float = def->get_value;
    setting->set_float = def->set_value;
    setting->float_min_value = def->min_value;
    setting->float_max_value = def->max_value;
    setting->float_step = def->step;
    setting->float_display_decimals = def->display_decimals;
    AppendNewSetting(mod_entry, setting);
    return EMC_OK;
}

EMC_Result __cdecl HubRegistry_RegisterActionRow(EMC_ModHandle mod, const EMC_ActionRowDefV1* def)
{
    ModEntry* mod_entry = nullptr;
    EMC_Result validation_result = ValidateSettingRegistrationCall(mod, def, "register_action_row", &mod_entry);
    if (validation_result != EMC_OK)
    {
        return validation_result;
    }

    if (!ValidateCommonSettingStrings(def->setting_id, def->label, def->description))
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (def->on_action == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    std::map<std::string, SettingEntry*>::const_iterator it = mod_entry->settings_by_id.find(def->setting_id);
    if (it != mod_entry->settings_by_id.end())
    {
        SettingEntry* existing = it->second;
        if (existing->kind != SETTING_KIND_ACTION)
        {
            LogSettingRegistrationConflict(
                mod_entry->namespace_id.c_str(),
                mod_entry->mod_id.c_str(),
                def->setting_id,
                EMC_ERR_CONFLICT,
                "setting_id_already_registered_with_different_kind");
            return EMC_ERR_CONFLICT;
        }

        bool exact_match = EmitCommonSettingDriftWarnings(mod_entry, existing, def->label, def->description, def->user_data);
        if (existing->on_action != def->on_action)
        {
            LogSettingWarning(mod_entry, def->setting_id, "callback", "callback_drift_ignored_using_canonical");
            exact_match = false;
        }

        if (existing->action_flags != def->action_flags)
        {
            LogSettingWarning(
                mod_entry,
                def->setting_id,
                "description",
                "action_flags_drift_ignored_using_canonical");
            exact_match = false;
        }

        (void)exact_match;
        return EMC_OK;
    }

    SettingEntry* setting = new SettingEntry();
    InitializeSettingDefaults(setting);
    setting->kind = SETTING_KIND_ACTION;
    setting->setting_id = def->setting_id;
    setting->label = def->label;
    setting->description = def->description;
    setting->user_data = def->user_data;
    setting->on_action = def->on_action;
    setting->action_flags = def->action_flags;
    AppendNewSetting(mod_entry, setting);
    return EMC_OK;
}

void HubRegistry_ForEachSettingInOrder(HubRegistryVisitSettingFn visitor, void* user_data)
{
    if (visitor == nullptr)
    {
        return;
    }

    for (size_t namespace_index = 0; namespace_index < g_namespaces_in_order.size(); ++namespace_index)
    {
        const NamespaceEntry* namespace_entry = g_namespaces_in_order[namespace_index];

        HubRegistryNamespaceView namespace_view;
        namespace_view.namespace_id = namespace_entry->namespace_id.c_str();
        namespace_view.namespace_display_name = namespace_entry->namespace_display_name.c_str();

        for (size_t mod_index = 0; mod_index < namespace_entry->mods_in_order.size(); ++mod_index)
        {
            const ModEntry* mod_entry = namespace_entry->mods_in_order[mod_index];

            HubRegistryModView mod_view;
            mod_view.namespace_id = namespace_entry->namespace_id.c_str();
            mod_view.namespace_display_name = namespace_entry->namespace_display_name.c_str();
            mod_view.mod_id = mod_entry->mod_id.c_str();
            mod_view.mod_display_name = mod_entry->mod_display_name.c_str();
            mod_view.mod_user_data = mod_entry->mod_user_data;
            mod_view.handle = mod_entry->handle;

            for (size_t setting_index = 0; setting_index < mod_entry->settings_in_order.size(); ++setting_index)
            {
                const SettingEntry* setting_entry = mod_entry->settings_in_order[setting_index];
                HubRegistrySettingView setting_view;
                PopulateSettingView(setting_entry, &setting_view);
                visitor(&namespace_view, &mod_view, &setting_view, user_data);
            }
        }
    }
}
