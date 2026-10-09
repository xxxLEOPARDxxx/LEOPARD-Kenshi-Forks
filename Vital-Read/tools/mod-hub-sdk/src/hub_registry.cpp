#include "hub_registry.h"
#include "hub_color.h"
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
    SETTING_KIND_ACTION = 4,
    SETTING_KIND_SELECT = 5,
    SETTING_KIND_TEXT = 6,
    SETTING_KIND_COLOR = 7
};

struct SelectOptionEntry
{
    int32_t value;
    std::string label;
};

struct ColorPresetEntry
{
    std::string value_hex;
    std::string label;
};

struct BoolConditionRuleEntry
{
    std::string target_setting_id;
    std::string controller_setting_id;
    uint32_t effect;
    int32_t expected_bool_value;
};

struct SettingEntry
{
    SettingKind kind;
    std::string setting_id;
    std::string label;
    std::string description;
    std::string hover_hint;
    std::string section_id;
    std::string section_display_name;
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
    std::vector<SelectOptionEntry> select_options_storage;
    std::vector<EMC_SelectOptionV1> select_options_view;

    EMC_GetTextCallback get_text;
    EMC_SetTextCallback set_text;
    uint32_t text_max_length;

    uint32_t color_preview_kind;
    std::vector<ColorPresetEntry> color_presets_storage;
    std::vector<EMC_ColorPresetV1> color_presets_view;

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
    std::map<std::string, BoolConditionRuleEntry> bool_condition_rules_by_target;
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
const uint32_t kMaxTextSettingLength = 256u;
const uint32_t kColorTextLength = 7u;

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

const char* NormalizeOptionalHoverHint(const char* hover_hint)
{
    return (hover_hint != nullptr && hover_hint[0] != '\0') ? hover_hint : nullptr;
}

bool HoverHintEquals(const std::string& stored_value, const char* incoming_value)
{
    const char* normalized_incoming = NormalizeOptionalHoverHint(incoming_value);
    if (normalized_incoming == nullptr)
    {
        return stored_value.empty();
    }

    return stored_value == normalized_incoming;
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

bool ValidateBoolConditionRuleStrings(const EMC_BoolConditionRuleDefV1* def)
{
    if (def == nullptr)
    {
        return false;
    }

    if (!IsValidId(def->target_setting_id) || !IsValidId(def->controller_setting_id))
    {
        return false;
    }

    if (std::strcmp(def->target_setting_id, def->controller_setting_id) == 0)
    {
        return false;
    }

    return def->effect == EMC_BOOL_CONDITION_EFFECT_HIDE
        || def->effect == EMC_BOOL_CONDITION_EFFECT_DISABLE;
}

bool BoolConditionRuleEquals(const BoolConditionRuleEntry& existing, const EMC_BoolConditionRuleDefV1* def)
{
    if (def == nullptr)
    {
        return false;
    }

    return existing.target_setting_id == def->target_setting_id
        && existing.controller_setting_id == def->controller_setting_id
        && existing.effect == def->effect
        && existing.expected_bool_value == def->expected_bool_value;
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
    setting->hover_hint.clear();
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
    setting->get_select = nullptr;
    setting->set_select = nullptr;
    setting->select_options_storage.clear();
    setting->select_options_view.clear();
    setting->get_text = nullptr;
    setting->set_text = nullptr;
    setting->text_max_length = 0u;
    setting->color_preview_kind = EMC_COLOR_PREVIEW_KIND_SWATCH;
    setting->color_presets_storage.clear();
    setting->color_presets_view.clear();
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

void RefreshSelectOptionViews(SettingEntry* setting)
{
    if (setting == nullptr)
    {
        return;
    }

    setting->select_options_view.clear();
    setting->select_options_view.reserve(setting->select_options_storage.size());
    for (size_t option_index = 0; option_index < setting->select_options_storage.size(); ++option_index)
    {
        EMC_SelectOptionV1 option_view = {};
        option_view.value = setting->select_options_storage[option_index].value;
        option_view.label = setting->select_options_storage[option_index].label.c_str();
        setting->select_options_view.push_back(option_view);
    }
}

bool SelectOptionsEqual(const SettingEntry* setting, const EMC_SelectOptionV1* options, uint32_t option_count)
{
    if (setting == nullptr)
    {
        return false;
    }

    if (setting->select_options_storage.size() != option_count)
    {
        return false;
    }

    for (uint32_t option_index = 0u; option_index < option_count; ++option_index)
    {
        const SelectOptionEntry& existing = setting->select_options_storage[option_index];
        const EMC_SelectOptionV1& incoming = options[option_index];
        if (existing.value != incoming.value || !StringEquals(existing.label, incoming.label))
        {
            return false;
        }
    }

    return true;
}

bool ValidateSelectOptions(const EMC_SelectOptionV1* options, uint32_t option_count)
{
    if (options == nullptr || option_count == 0u)
    {
        return false;
    }

    for (uint32_t option_index = 0u; option_index < option_count; ++option_index)
    {
        if (options[option_index].label == nullptr || options[option_index].label[0] == '\0')
        {
            return false;
        }

        for (uint32_t previous_index = 0u; previous_index < option_index; ++previous_index)
        {
            if (options[previous_index].value == options[option_index].value)
            {
                return false;
            }
        }
    }

    return true;
}

void RefreshColorPresetViews(SettingEntry* setting)
{
    if (setting == nullptr)
    {
        return;
    }

    setting->color_presets_view.clear();
    setting->color_presets_view.reserve(setting->color_presets_storage.size());
    for (size_t preset_index = 0u; preset_index < setting->color_presets_storage.size(); ++preset_index)
    {
        EMC_ColorPresetV1 preset_view = {};
        preset_view.value_hex = setting->color_presets_storage[preset_index].value_hex.c_str();
        preset_view.label = setting->color_presets_storage[preset_index].label.empty()
            ? nullptr
            : setting->color_presets_storage[preset_index].label.c_str();
        setting->color_presets_view.push_back(preset_view);
    }
}

bool ColorPresetsEqual(const SettingEntry* setting, const EMC_ColorPresetV1* presets, uint32_t preset_count)
{
    if (setting == nullptr)
    {
        return false;
    }

    const EMC_ColorPresetV1* source_presets = presets;
    uint32_t source_preset_count = preset_count;
    if (source_presets == nullptr && source_preset_count == 0u)
    {
        source_presets = hub_color::GetDefaultPalette(&source_preset_count);
    }
    else if (source_presets == nullptr || source_preset_count == 0u)
    {
        return false;
    }

    if (setting->color_presets_storage.size() != source_preset_count)
    {
        return false;
    }

    for (uint32_t preset_index = 0u; preset_index < source_preset_count; ++preset_index)
    {
        std::string normalized_value;
        if (!hub_color::TryNormalizeColorHex(source_presets[preset_index].value_hex, &normalized_value))
        {
            return false;
        }

        const ColorPresetEntry& existing = setting->color_presets_storage[preset_index];
        const char* incoming_label = source_presets[preset_index].label != nullptr ? source_presets[preset_index].label : "";
        if (existing.value_hex != normalized_value || existing.label != incoming_label)
        {
            return false;
        }
    }

    return true;
}

bool ValidateAndCopyColorPresets(
    const EMC_ColorPresetV1* incoming_presets,
    uint32_t incoming_preset_count,
    std::vector<ColorPresetEntry>* out_presets)
{
    if (out_presets == nullptr)
    {
        return false;
    }

    out_presets->clear();

    const EMC_ColorPresetV1* source_presets = incoming_presets;
    uint32_t source_preset_count = incoming_preset_count;
    if (source_presets == nullptr && source_preset_count == 0u)
    {
        source_presets = hub_color::GetDefaultPalette(&source_preset_count);
    }
    else if (source_presets == nullptr || source_preset_count == 0u)
    {
        return false;
    }

    out_presets->reserve(source_preset_count);
    for (uint32_t preset_index = 0u; preset_index < source_preset_count; ++preset_index)
    {
        std::string normalized_value;
        if (!hub_color::TryNormalizeColorHex(source_presets[preset_index].value_hex, &normalized_value))
        {
            return false;
        }

        for (uint32_t previous_index = 0u; previous_index < preset_index; ++previous_index)
        {
            if ((*out_presets)[previous_index].value_hex == normalized_value)
            {
                return false;
            }
        }

        ColorPresetEntry preset = {};
        preset.value_hex = normalized_value;
        if (source_presets[preset_index].label != nullptr)
        {
            preset.label = source_presets[preset_index].label;
        }
        out_presets->push_back(preset);
    }

    return !out_presets->empty();
}

bool EmitCommonSettingDriftWarnings(
    const ModEntry* mod,
    const SettingEntry* existing,
    const char* label,
    const char* description,
    const char* hover_hint,
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

    if (!HoverHintEquals(existing->hover_hint, hover_hint))
    {
        LogSettingWarning(mod, existing->setting_id.c_str(), "hover_hint", "hover_hint_drift_ignored_using_canonical");
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
    out_view->hover_hint = setting->hover_hint.empty() ? nullptr : setting->hover_hint.c_str();
    out_view->section_id = setting->section_id.empty() ? nullptr : setting->section_id.c_str();
    out_view->section_display_name = setting->section_display_name.empty() ? nullptr : setting->section_display_name.c_str();
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

    out_view->get_select = setting->get_select;
    out_view->set_select = setting->set_select;
    out_view->select_options = setting->select_options_view.empty() ? nullptr : &setting->select_options_view[0];
    out_view->select_option_count = static_cast<uint32_t>(setting->select_options_view.size());

    out_view->get_text = setting->get_text;
    out_view->set_text = setting->set_text;
    out_view->text_max_length = setting->text_max_length;
    out_view->color_preview_kind = setting->color_preview_kind;
    out_view->color_presets = setting->color_presets_view.empty() ? nullptr : &setting->color_presets_view[0];
    out_view->color_preset_count = static_cast<uint32_t>(setting->color_presets_view.size());

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
    if (def == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const EMC_BoolSettingDefV2 def_v2 = {
        def->setting_id,
        def->label,
        def->description,
        def->user_data,
        def->get_value,
        def->set_value,
        nullptr };
    return HubRegistry_RegisterBoolSettingV2(mod, &def_v2);
}

EMC_Result __cdecl HubRegistry_RegisterBoolSettingV2(EMC_ModHandle mod, const EMC_BoolSettingDefV2* def)
{
    ModEntry* mod_entry = nullptr;
    EMC_Result validation_result = ValidateSettingRegistrationCall(mod, def, "register_bool_setting_v2", &mod_entry);
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

        bool exact_match = EmitCommonSettingDriftWarnings(mod_entry, existing, def->label, def->description, def->hover_hint, def->user_data);
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
    const char* normalized_hover_hint = NormalizeOptionalHoverHint(def->hover_hint);
    if (normalized_hover_hint != nullptr)
    {
        setting->hover_hint = normalized_hover_hint;
    }
    setting->user_data = def->user_data;
    setting->get_bool = def->get_value;
    setting->set_bool = def->set_value;
    AppendNewSetting(mod_entry, setting);
    return EMC_OK;
}

EMC_Result __cdecl HubRegistry_RegisterKeybindSetting(EMC_ModHandle mod, const EMC_KeybindSettingDefV1* def)
{
    if (def == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const EMC_KeybindSettingDefV2 def_v2 = {
        def->setting_id,
        def->label,
        def->description,
        def->user_data,
        def->get_value,
        def->set_value,
        nullptr };
    return HubRegistry_RegisterKeybindSettingV2(mod, &def_v2);
}

EMC_Result __cdecl HubRegistry_RegisterKeybindSettingV2(EMC_ModHandle mod, const EMC_KeybindSettingDefV2* def)
{
    ModEntry* mod_entry = nullptr;
    EMC_Result validation_result = ValidateSettingRegistrationCall(mod, def, "register_keybind_setting_v2", &mod_entry);
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

        bool exact_match = EmitCommonSettingDriftWarnings(mod_entry, existing, def->label, def->description, def->hover_hint, def->user_data);
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
    const char* normalized_hover_hint = NormalizeOptionalHoverHint(def->hover_hint);
    if (normalized_hover_hint != nullptr)
    {
        setting->hover_hint = normalized_hover_hint;
    }
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

        bool exact_match = EmitCommonSettingDriftWarnings(mod_entry, existing, def->label, def->description, nullptr, def->user_data);
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

        bool exact_match = EmitCommonSettingDriftWarnings(mod_entry, existing, def->label, def->description, nullptr, def->user_data);
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

        bool exact_match = EmitCommonSettingDriftWarnings(mod_entry, existing, def->label, def->description, nullptr, def->user_data);
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

EMC_Result __cdecl HubRegistry_RegisterSelectSetting(EMC_ModHandle mod, const EMC_SelectSettingDefV1* def)
{
    if (def == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const EMC_SelectSettingDefV2 def_v2 = {
        def->setting_id,
        def->label,
        def->description,
        def->user_data,
        def->options,
        def->option_count,
        def->get_value,
        def->set_value,
        nullptr };
    return HubRegistry_RegisterSelectSettingV2(mod, &def_v2);
}

EMC_Result __cdecl HubRegistry_RegisterSelectSettingV2(EMC_ModHandle mod, const EMC_SelectSettingDefV2* def)
{
    ModEntry* mod_entry = nullptr;
    EMC_Result validation_result = ValidateSettingRegistrationCall(mod, def, "register_select_setting_v2", &mod_entry);
    if (validation_result != EMC_OK)
    {
        return validation_result;
    }

    if (!ValidateCommonSettingStrings(def->setting_id, def->label, def->description))
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (def->get_value == nullptr || def->set_value == nullptr || !ValidateSelectOptions(def->options, def->option_count))
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    std::map<std::string, SettingEntry*>::const_iterator it = mod_entry->settings_by_id.find(def->setting_id);
    if (it != mod_entry->settings_by_id.end())
    {
        SettingEntry* existing = it->second;
        if (existing->kind != SETTING_KIND_SELECT)
        {
            LogSettingRegistrationConflict(
                mod_entry->namespace_id.c_str(),
                mod_entry->mod_id.c_str(),
                def->setting_id,
                EMC_ERR_CONFLICT,
                "setting_id_already_registered_with_different_kind");
            return EMC_ERR_CONFLICT;
        }

        bool exact_match = EmitCommonSettingDriftWarnings(mod_entry, existing, def->label, def->description, def->hover_hint, def->user_data);
        if (existing->get_select != def->get_value || existing->set_select != def->set_value)
        {
            LogSettingWarning(mod_entry, def->setting_id, "callback", "callback_drift_ignored_using_canonical");
            exact_match = false;
        }

        if (!SelectOptionsEqual(existing, def->options, def->option_count))
        {
            LogSettingWarning(mod_entry, def->setting_id, "options", "options_drift_ignored_using_canonical");
            exact_match = false;
        }

        (void)exact_match;
        return EMC_OK;
    }

    SettingEntry* setting = new SettingEntry();
    InitializeSettingDefaults(setting);
    setting->kind = SETTING_KIND_SELECT;
    setting->setting_id = def->setting_id;
    setting->label = def->label;
    setting->description = def->description;
    const char* normalized_hover_hint = NormalizeOptionalHoverHint(def->hover_hint);
    if (normalized_hover_hint != nullptr)
    {
        setting->hover_hint = normalized_hover_hint;
    }
    setting->user_data = def->user_data;
    setting->get_select = def->get_value;
    setting->set_select = def->set_value;
    setting->select_options_storage.reserve(def->option_count);
    for (uint32_t option_index = 0u; option_index < def->option_count; ++option_index)
    {
        SelectOptionEntry option = {};
        option.value = def->options[option_index].value;
        option.label = def->options[option_index].label;
        setting->select_options_storage.push_back(option);
    }
    RefreshSelectOptionViews(setting);
    AppendNewSetting(mod_entry, setting);
    return EMC_OK;
}

EMC_Result __cdecl HubRegistry_RegisterTextSetting(EMC_ModHandle mod, const EMC_TextSettingDefV1* def)
{
    if (def == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const EMC_TextSettingDefV2 def_v2 = {
        def->setting_id,
        def->label,
        def->description,
        def->user_data,
        def->max_length,
        def->get_value,
        def->set_value,
        nullptr };
    return HubRegistry_RegisterTextSettingV2(mod, &def_v2);
}

EMC_Result __cdecl HubRegistry_RegisterTextSettingV2(EMC_ModHandle mod, const EMC_TextSettingDefV2* def)
{
    ModEntry* mod_entry = nullptr;
    EMC_Result validation_result = ValidateSettingRegistrationCall(mod, def, "register_text_setting_v2", &mod_entry);
    if (validation_result != EMC_OK)
    {
        return validation_result;
    }

    if (!ValidateCommonSettingStrings(def->setting_id, def->label, def->description))
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (def->get_value == nullptr || def->set_value == nullptr || def->max_length == 0u || def->max_length > kMaxTextSettingLength)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    std::map<std::string, SettingEntry*>::const_iterator it = mod_entry->settings_by_id.find(def->setting_id);
    if (it != mod_entry->settings_by_id.end())
    {
        SettingEntry* existing = it->second;
        if (existing->kind != SETTING_KIND_TEXT)
        {
            LogSettingRegistrationConflict(
                mod_entry->namespace_id.c_str(),
                mod_entry->mod_id.c_str(),
                def->setting_id,
                EMC_ERR_CONFLICT,
                "setting_id_already_registered_with_different_kind");
            return EMC_ERR_CONFLICT;
        }

        bool exact_match = EmitCommonSettingDriftWarnings(mod_entry, existing, def->label, def->description, def->hover_hint, def->user_data);
        if (existing->get_text != def->get_value || existing->set_text != def->set_value)
        {
            LogSettingWarning(mod_entry, def->setting_id, "callback", "callback_drift_ignored_using_canonical");
            exact_match = false;
        }

        if (existing->text_max_length != def->max_length)
        {
            LogSettingWarning(mod_entry, def->setting_id, "max_length", "max_length_drift_ignored_using_canonical");
            exact_match = false;
        }

        (void)exact_match;
        return EMC_OK;
    }

    SettingEntry* setting = new SettingEntry();
    InitializeSettingDefaults(setting);
    setting->kind = SETTING_KIND_TEXT;
    setting->setting_id = def->setting_id;
    setting->label = def->label;
    setting->description = def->description;
    const char* normalized_hover_hint = NormalizeOptionalHoverHint(def->hover_hint);
    if (normalized_hover_hint != nullptr)
    {
        setting->hover_hint = normalized_hover_hint;
    }
    setting->user_data = def->user_data;
    setting->get_text = def->get_value;
    setting->set_text = def->set_value;
    setting->text_max_length = def->max_length;
    AppendNewSetting(mod_entry, setting);
    return EMC_OK;
}

EMC_Result __cdecl HubRegistry_RegisterColorSetting(EMC_ModHandle mod, const EMC_ColorSettingDefV1* def)
{
    ModEntry* mod_entry = nullptr;
    EMC_Result validation_result = ValidateSettingRegistrationCall(mod, def, "register_color_setting", &mod_entry);
    if (validation_result != EMC_OK)
    {
        return validation_result;
    }

    if (!ValidateCommonSettingStrings(def->setting_id, def->label, def->description))
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (def->get_value == nullptr
        || def->set_value == nullptr
        || !hub_color::IsValidPreviewKind(def->preview_kind))
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    std::vector<ColorPresetEntry> normalized_presets;
    if (!ValidateAndCopyColorPresets(def->presets, def->preset_count, &normalized_presets))
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    std::map<std::string, SettingEntry*>::const_iterator it = mod_entry->settings_by_id.find(def->setting_id);
    if (it != mod_entry->settings_by_id.end())
    {
        SettingEntry* existing = it->second;
        if (existing->kind != SETTING_KIND_COLOR)
        {
            LogSettingRegistrationConflict(
                mod_entry->namespace_id.c_str(),
                mod_entry->mod_id.c_str(),
                def->setting_id,
                EMC_ERR_CONFLICT,
                "setting_id_already_registered_with_different_kind");
            return EMC_ERR_CONFLICT;
        }

        bool exact_match = EmitCommonSettingDriftWarnings(mod_entry, existing, def->label, def->description, nullptr, def->user_data);
        if (existing->get_text != def->get_value || existing->set_text != def->set_value)
        {
            LogSettingWarning(mod_entry, def->setting_id, "callback", "callback_drift_ignored_using_canonical");
            exact_match = false;
        }

        if (existing->color_preview_kind != def->preview_kind)
        {
            LogSettingWarning(mod_entry, def->setting_id, "preview_kind", "preview_kind_drift_ignored_using_canonical");
            exact_match = false;
        }

        if (!ColorPresetsEqual(existing, def->presets, def->preset_count))
        {
            LogSettingWarning(mod_entry, def->setting_id, "presets", "presets_drift_ignored_using_canonical");
            exact_match = false;
        }

        (void)exact_match;
        return EMC_OK;
    }

    SettingEntry* setting = new SettingEntry();
    InitializeSettingDefaults(setting);
    setting->kind = SETTING_KIND_COLOR;
    setting->setting_id = def->setting_id;
    setting->label = def->label;
    setting->description = def->description;
    setting->user_data = def->user_data;
    setting->get_text = def->get_value;
    setting->set_text = def->set_value;
    setting->text_max_length = kColorTextLength;
    setting->color_preview_kind = def->preview_kind;
    setting->color_presets_storage.swap(normalized_presets);
    RefreshColorPresetViews(setting);
    AppendNewSetting(mod_entry, setting);
    return EMC_OK;
}

EMC_Result __cdecl HubRegistry_RegisterSettingSection(EMC_ModHandle mod, const EMC_SettingSectionDefV1* def)
{
    ModEntry* mod_entry = nullptr;
    EMC_Result validation_result = ValidateSettingRegistrationCall(mod, def, "register_setting_section", &mod_entry);
    if (validation_result != EMC_OK)
    {
        return validation_result;
    }

    if (!ValidateCommonSettingStrings(def->setting_id, def->section_id, def->section_display_name))
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    std::map<std::string, SettingEntry*>::const_iterator it = mod_entry->settings_by_id.find(def->setting_id);
    if (it == mod_entry->settings_by_id.end())
    {
        return EMC_ERR_NOT_FOUND;
    }

    SettingEntry* setting = it->second;
    if (!setting->section_id.empty()
        && (setting->section_id != def->section_id
            || setting->section_display_name != def->section_display_name))
    {
        LogSettingRegistrationConflict(
            mod_entry->namespace_id.c_str(),
            mod_entry->mod_id.c_str(),
            def->setting_id,
            EMC_ERR_CONFLICT,
            "setting_section_drift_ignored_using_canonical");
        return EMC_ERR_CONFLICT;
    }

    setting->section_id = def->section_id;
    setting->section_display_name = def->section_display_name;
    return EMC_OK;
}

EMC_Result __cdecl HubRegistry_RegisterBoolConditionRule(EMC_ModHandle mod, const EMC_BoolConditionRuleDefV1* def)
{
    ModEntry* mod_entry = nullptr;
    EMC_Result validation_result = ValidateSettingRegistrationCall(mod, def, "register_bool_condition_rule", &mod_entry);
    if (validation_result != EMC_OK)
    {
        return validation_result;
    }

    if (!ValidateBoolConditionRuleStrings(def) || def->expected_bool_value < 0 || def->expected_bool_value > 1)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    std::map<std::string, SettingEntry*>::const_iterator target_it = mod_entry->settings_by_id.find(def->target_setting_id);
    if (target_it == mod_entry->settings_by_id.end())
    {
        return EMC_ERR_NOT_FOUND;
    }

    std::map<std::string, SettingEntry*>::const_iterator controller_it = mod_entry->settings_by_id.find(def->controller_setting_id);
    if (controller_it == mod_entry->settings_by_id.end())
    {
        return EMC_ERR_NOT_FOUND;
    }

    if (controller_it->second->kind != SETTING_KIND_BOOL)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    std::map<std::string, BoolConditionRuleEntry>::iterator existing_rule_it =
        mod_entry->bool_condition_rules_by_target.find(def->target_setting_id);
    if (existing_rule_it != mod_entry->bool_condition_rules_by_target.end())
    {
        if (!BoolConditionRuleEquals(existing_rule_it->second, def))
        {
            LogSettingRegistrationConflict(
                mod_entry->namespace_id.c_str(),
                mod_entry->mod_id.c_str(),
                def->target_setting_id,
                EMC_ERR_CONFLICT,
                "bool_condition_rule_drift_ignored_using_canonical");
            return EMC_ERR_CONFLICT;
        }

        return EMC_OK;
    }

    BoolConditionRuleEntry rule;
    rule.target_setting_id = def->target_setting_id;
    rule.controller_setting_id = def->controller_setting_id;
    rule.effect = def->effect;
    rule.expected_bool_value = def->expected_bool_value;
    mod_entry->bool_condition_rules_by_target[rule.target_setting_id] = rule;
    return EMC_OK;
}

bool HubRegistry_GetBoolConditionRuleView(
    EMC_ModHandle mod,
    const char* target_setting_id,
    HubRegistryBoolConditionRuleView* out_view)
{
    if (out_view == nullptr || mod == nullptr || !IsValidId(target_setting_id))
    {
        return false;
    }

    ModEntry* mod_entry = FindModByHandle(mod);
    if (mod_entry == nullptr)
    {
        return false;
    }

    std::map<std::string, BoolConditionRuleEntry>::const_iterator it =
        mod_entry->bool_condition_rules_by_target.find(target_setting_id);
    if (it == mod_entry->bool_condition_rules_by_target.end())
    {
        return false;
    }

    const BoolConditionRuleEntry& rule = it->second;
    out_view->target_setting_id = rule.target_setting_id.c_str();
    out_view->controller_setting_id = rule.controller_setting_id.c_str();
    out_view->effect = rule.effect;
    out_view->expected_bool_value = rule.expected_bool_value;
    return true;
}

EMC_Result __cdecl HubRegistry_RegisterActionRow(EMC_ModHandle mod, const EMC_ActionRowDefV1* def)
{
    if (def == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const EMC_ActionRowDefV2 def_v2 = {
        def->setting_id,
        def->label,
        def->description,
        def->user_data,
        def->action_flags,
        def->on_action,
        nullptr };
    return HubRegistry_RegisterActionRowV2(mod, &def_v2);
}

EMC_Result __cdecl HubRegistry_RegisterActionRowV2(EMC_ModHandle mod, const EMC_ActionRowDefV2* def)
{
    ModEntry* mod_entry = nullptr;
    EMC_Result validation_result = ValidateSettingRegistrationCall(mod, def, "register_action_row_v2", &mod_entry);
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

        bool exact_match = EmitCommonSettingDriftWarnings(mod_entry, existing, def->label, def->description, def->hover_hint, def->user_data);
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
    const char* normalized_hover_hint = NormalizeOptionalHoverHint(def->hover_hint);
    if (normalized_hover_hint != nullptr)
    {
        setting->hover_hint = normalized_hover_hint;
    }
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
