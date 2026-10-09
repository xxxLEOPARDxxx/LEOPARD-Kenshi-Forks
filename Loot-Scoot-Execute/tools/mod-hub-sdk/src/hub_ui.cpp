#include "hub_ui.h"

#include "hub_registry.h"

#include <Debug.h>

#include <ois/OISKeyboard.h>

#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
const char* kLogNone = "none";
const char* kUnavailableMessage = "Unavailable";
const char* kActionFailedMessage = "action_callback_failed";
const char* kInvalidIntTextMessage = "invalid_int_text";
const char* kInvalidFloatTextMessage = "invalid_float_text";
const float kFloatSnapEpsilon = 1e-6f;

struct HubUiModSection;

struct HubUiSettingRow
{
    int32_t kind;
    std::string namespace_id;
    std::string namespace_display_name;
    std::string mod_id;
    std::string mod_display_name;
    std::string setting_id;
    std::string label;
    std::string description;
    void* user_data;

    EMC_GetBoolCallback get_bool;
    EMC_SetBoolCallback set_bool;
    int32_t canonical_bool_value;
    int32_t pending_bool_value;

    EMC_GetKeybindCallback get_keybind;
    EMC_SetKeybindCallback set_keybind;
    EMC_KeybindValueV1 canonical_keybind_value;
    EMC_KeybindValueV1 pending_keybind_value;

    EMC_GetIntCallback get_int;
    EMC_SetIntCallback set_int;
    int32_t int_min_value;
    int32_t int_max_value;
    int32_t int_step;
    int32_t canonical_int_value;
    int32_t pending_int_value;
    std::string pending_int_text;
    bool int_text_parse_error;

    EMC_GetFloatCallback get_float;
    EMC_SetFloatCallback set_float;
    float float_min_value;
    float float_max_value;
    float float_step;
    uint32_t float_display_decimals;
    float canonical_float_value;
    float pending_float_value;
    std::string pending_float_text;
    bool float_text_parse_error;

    EMC_ActionRowCallback on_action;
    uint32_t action_flags;

    bool dirty;
    bool capture_active;
    std::string inline_error;
    HubUiModSection* parent_mod;
};

struct HubUiModSection
{
    std::string namespace_id;
    std::string mod_id;
    std::string mod_display_name;
    bool collapsed;
    std::vector<HubUiSettingRow*> rows;
};

struct HubUiNamespaceTab
{
    std::string namespace_id;
    std::string namespace_display_name;
    std::string search_query;
    std::vector<HubUiModSection*> mods;
};

bool g_options_window_open = false;
std::vector<HubUiNamespaceTab*> g_tabs_in_order;
std::vector<HubUiSettingRow*> g_rows_in_order;

const char* SafeLogValue(const char* value)
{
    if (value == nullptr || value[0] == '\0')
    {
        return kLogNone;
    }

    return value;
}

bool IsBoolValueValid(int32_t value)
{
    return value == 0 || value == 1;
}

bool KeybindEquals(EMC_KeybindValueV1 lhs, EMC_KeybindValueV1 rhs)
{
    return lhs.keycode == rhs.keycode && lhs.modifiers == rhs.modifiers;
}

bool FloatBitsEqual(float lhs, float rhs)
{
    uint32_t lhs_bits = 0;
    uint32_t rhs_bits = 0;
    std::memcpy(&lhs_bits, &lhs, sizeof(lhs_bits));
    std::memcpy(&rhs_bits, &rhs, sizeof(rhs_bits));
    return lhs_bits == rhs_bits;
}

int32_t ClampIntValue(int32_t value, int32_t min_value, int32_t max_value)
{
    if (value < min_value)
    {
        return min_value;
    }

    if (value > max_value)
    {
        return max_value;
    }

    return value;
}

float ClampFloatValue(float value, float min_value, float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }

    if (value > max_value)
    {
        return max_value;
    }

    return value;
}

std::string FormatIntText(int32_t value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

std::string FormatFloatText(float value, uint32_t display_decimals)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(static_cast<std::streamsize>(display_decimals));
    stream << value;
    return stream.str();
}

bool TryParseIntText(const char* text, int32_t* out_value)
{
    if (text == nullptr || out_value == nullptr)
    {
        return false;
    }

    errno = 0;
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (end == text)
    {
        return false;
    }

    while (end != nullptr && *end != '\0' && std::isspace(static_cast<unsigned char>(*end)))
    {
        ++end;
    }

    if (end != nullptr && *end != '\0')
    {
        return false;
    }

    if (errno == ERANGE
        || parsed < static_cast<long>(std::numeric_limits<int32_t>::min())
        || parsed > static_cast<long>(std::numeric_limits<int32_t>::max()))
    {
        return false;
    }

    *out_value = static_cast<int32_t>(parsed);
    return true;
}

bool TryParseFloatText(const char* text, float* out_value)
{
    if (text == nullptr || out_value == nullptr)
    {
        return false;
    }

    errno = 0;
    char* end = nullptr;
    const double parsed = std::strtod(text, &end);
    if (end == text)
    {
        return false;
    }

    while (end != nullptr && *end != '\0' && std::isspace(static_cast<unsigned char>(*end)))
    {
        ++end;
    }

    if (end != nullptr && *end != '\0')
    {
        return false;
    }

    if (errno == ERANGE
        || parsed != parsed
        || parsed < -static_cast<double>(std::numeric_limits<float>::max())
        || parsed > static_cast<double>(std::numeric_limits<float>::max()))
    {
        return false;
    }

    *out_value = static_cast<float>(parsed);
    return true;
}

int32_t SnapIntValueToStep(int32_t value, int32_t min_value, int32_t max_value, int32_t step)
{
    if (step <= 0)
    {
        return ClampIntValue(value, min_value, max_value);
    }

    const int64_t clamped_value = static_cast<int64_t>(ClampIntValue(value, min_value, max_value));
    const int64_t min64 = static_cast<int64_t>(min_value);
    const int64_t max64 = static_cast<int64_t>(max_value);
    const int64_t step64 = static_cast<int64_t>(step);

    const int64_t offset = clamped_value - min64;
    const int64_t lower_index = offset / step64;

    int64_t lower = min64 + (lower_index * step64);
    int64_t upper = lower + step64;
    const bool upper_valid = upper <= max64;

    const int64_t distance_to_lower = clamped_value - lower;
    const int64_t distance_to_upper = upper_valid ? (upper - clamped_value) : std::numeric_limits<int64_t>::max();

    int64_t chosen = lower;
    if (upper_valid && distance_to_upper < distance_to_lower)
    {
        chosen = upper;
    }
    else if (upper_valid && distance_to_upper == distance_to_lower)
    {
        const int64_t abs_lower = lower < 0 ? -lower : lower;
        const int64_t abs_upper = upper < 0 ? -upper : upper;
        chosen = abs_upper < abs_lower ? upper : lower;
    }

    if (chosen < min64)
    {
        chosen = min64;
    }
    if (chosen > max64)
    {
        chosen = max64;
    }

    return static_cast<int32_t>(chosen);
}

float SnapFloatValueToStep(float value, float min_value, float max_value, float step)
{
    if (step <= 0.0f)
    {
        return ClampFloatValue(value, min_value, max_value);
    }

    const double min64 = static_cast<double>(min_value);
    const double max64 = static_cast<double>(max_value);
    const double step64 = static_cast<double>(step);
    const double clamped = static_cast<double>(ClampFloatValue(value, min_value, max_value));

    const double offset = clamped - min64;
    const double ratio = offset / step64;
    const double eps_ratio = static_cast<double>(kFloatSnapEpsilon) / step64;

    const double lower_index = std::floor(ratio);
    double upper_index = lower_index + 1.0;
    if (std::fabs(ratio - lower_index) <= eps_ratio)
    {
        upper_index = lower_index;
    }

    const double lower = min64 + (lower_index * step64);
    const double upper = min64 + (upper_index * step64);
    const bool upper_valid = upper <= (max64 + static_cast<double>(kFloatSnapEpsilon));

    const double distance_to_lower = std::fabs(clamped - lower);
    const double distance_to_upper =
        upper_valid ? std::fabs(upper - clamped) : std::numeric_limits<double>::infinity();

    double chosen = lower;
    if (upper_valid && (distance_to_upper + static_cast<double>(kFloatSnapEpsilon)) < distance_to_lower)
    {
        chosen = upper;
    }
    else if (upper_valid
        && std::fabs(distance_to_upper - distance_to_lower) <= static_cast<double>(kFloatSnapEpsilon))
    {
        const double abs_lower = std::fabs(lower);
        const double abs_upper = std::fabs(upper);
        chosen = (abs_upper + static_cast<double>(kFloatSnapEpsilon)) < abs_lower ? upper : lower;
    }

    if (chosen < min64 && (min64 - chosen) <= static_cast<double>(kFloatSnapEpsilon))
    {
        chosen = min64;
    }
    else if (chosen < min64)
    {
        chosen = min64;
    }
    if (chosen > max64 && (chosen - max64) <= static_cast<double>(kFloatSnapEpsilon))
    {
        chosen = max64;
    }
    else if (chosen > max64)
    {
        chosen = max64;
    }

    return static_cast<float>(chosen);
}

void RecomputeIntDirty(HubUiSettingRow* row)
{
    if (row == nullptr)
    {
        return;
    }

    row->dirty = row->int_text_parse_error || row->pending_int_value != row->canonical_int_value;
}

void RecomputeFloatDirty(HubUiSettingRow* row)
{
    if (row == nullptr)
    {
        return;
    }

    row->dirty = row->float_text_parse_error || !FloatBitsEqual(row->pending_float_value, row->canonical_float_value);
}

unsigned char FoldAsciiForSearch(unsigned char value)
{
    if (value >= 'A' && value <= 'Z')
    {
        return static_cast<unsigned char>(value - 'A' + 'a');
    }

    return value;
}

bool BytesEqualForSearch(unsigned char lhs, unsigned char rhs)
{
    if (lhs < 0x80u && rhs < 0x80u)
    {
        return FoldAsciiForSearch(lhs) == FoldAsciiForSearch(rhs);
    }

    return lhs == rhs;
}

bool ContainsSearchMatch(const char* haystack, const std::string& needle)
{
    if (needle.empty())
    {
        return true;
    }

    if (haystack == nullptr || haystack[0] == '\0')
    {
        return false;
    }

    const size_t haystack_len = std::strlen(haystack);
    const size_t needle_len = needle.size();
    if (needle_len > haystack_len)
    {
        return false;
    }

    for (size_t start = 0; start <= haystack_len - needle_len; ++start)
    {
        bool matched = true;
        for (size_t offset = 0; offset < needle_len; ++offset)
        {
            const unsigned char hay_byte = static_cast<unsigned char>(haystack[start + offset]);
            const unsigned char needle_byte = static_cast<unsigned char>(needle[offset]);
            if (!BytesEqualForSearch(hay_byte, needle_byte))
            {
                matched = false;
                break;
            }
        }

        if (matched)
        {
            return true;
        }
    }

    return false;
}

void LogUiGetFailure(const HubUiSettingRow* row, EMC_Result result, const char* message)
{
    std::ostringstream line;
    line << "event=hub_ui_get_failure"
         << " namespace=" << SafeLogValue(row->namespace_id.c_str())
         << " mod=" << SafeLogValue(row->mod_id.c_str())
         << " setting=" << SafeLogValue(row->setting_id.c_str())
         << " result=" << result
         << " message=" << SafeLogValue(message);
    ErrorLog(line.str().c_str());
}

void LogActionFailure(const HubUiSettingRow* row, EMC_Result result, const char* message)
{
    std::ostringstream line;
    line << "event=hub_action_failure"
         << " namespace=" << SafeLogValue(row->namespace_id.c_str())
         << " mod=" << SafeLogValue(row->mod_id.c_str())
         << " setting=" << SafeLogValue(row->setting_id.c_str())
         << " result=" << result
         << " message=" << SafeLogValue(message);
    ErrorLog(line.str().c_str());
}

void LogActionRefreshGetFailure(const HubUiSettingRow* row, EMC_Result result, const char* message)
{
    std::ostringstream line;
    line << "event=hub_action_refresh_get_failure"
         << " namespace=" << SafeLogValue(row->namespace_id.c_str())
         << " mod=" << SafeLogValue(row->mod_id.c_str())
         << " setting=" << SafeLogValue(row->setting_id.c_str())
         << " result=" << result
         << " message=" << SafeLogValue(message);
    ErrorLog(line.str().c_str());
}

const char* ResolveErrorMessage(const char* err_buf, const char* fallback)
{
    if (err_buf != nullptr && err_buf[0] != '\0')
    {
        return err_buf;
    }

    return fallback;
}

bool TryReadBool(HubUiSettingRow* row, int32_t* out_value, EMC_Result* out_result, const char** out_message)
{
    if (row->get_bool == nullptr)
    {
        if (out_result != nullptr)
        {
            *out_result = EMC_ERR_INVALID_ARGUMENT;
        }
        if (out_message != nullptr)
        {
            *out_message = "missing_get_callback";
        }
        return false;
    }

    int32_t value = 0;
    EMC_Result result = row->get_bool(row->user_data, &value);
    if (result != EMC_OK)
    {
        if (out_result != nullptr)
        {
            *out_result = result;
        }
        if (out_message != nullptr)
        {
            *out_message = "get_callback_failed";
        }
        return false;
    }

    if (!IsBoolValueValid(value))
    {
        if (out_result != nullptr)
        {
            *out_result = EMC_ERR_CALLBACK_FAILED;
        }
        if (out_message != nullptr)
        {
            *out_message = "invalid_bool_value";
        }
        return false;
    }

    if (out_value != nullptr)
    {
        *out_value = value;
    }

    return true;
}

bool TryReadKeybind(HubUiSettingRow* row, EMC_KeybindValueV1* out_value, EMC_Result* out_result, const char** out_message)
{
    if (row->get_keybind == nullptr)
    {
        if (out_result != nullptr)
        {
            *out_result = EMC_ERR_INVALID_ARGUMENT;
        }
        if (out_message != nullptr)
        {
            *out_message = "missing_get_callback";
        }
        return false;
    }

    EMC_KeybindValueV1 value;
    value.keycode = EMC_KEY_UNBOUND;
    value.modifiers = 0;
    EMC_Result result = row->get_keybind(row->user_data, &value);
    if (result != EMC_OK)
    {
        if (out_result != nullptr)
        {
            *out_result = result;
        }
        if (out_message != nullptr)
        {
            *out_message = "get_callback_failed";
        }
        return false;
    }

    if (out_value != nullptr)
    {
        *out_value = value;
    }

    return true;
}

bool TryReadInt(HubUiSettingRow* row, int32_t* out_value, EMC_Result* out_result, const char** out_message)
{
    if (row->get_int == nullptr)
    {
        if (out_result != nullptr)
        {
            *out_result = EMC_ERR_INVALID_ARGUMENT;
        }
        if (out_message != nullptr)
        {
            *out_message = "missing_get_callback";
        }
        return false;
    }

    int32_t value = 0;
    const EMC_Result result = row->get_int(row->user_data, &value);
    if (result != EMC_OK)
    {
        if (out_result != nullptr)
        {
            *out_result = result;
        }
        if (out_message != nullptr)
        {
            *out_message = "get_callback_failed";
        }
        return false;
    }

    if (out_value != nullptr)
    {
        *out_value = value;
    }

    return true;
}

bool TryReadFloat(HubUiSettingRow* row, float* out_value, EMC_Result* out_result, const char** out_message)
{
    if (row->get_float == nullptr)
    {
        if (out_result != nullptr)
        {
            *out_result = EMC_ERR_INVALID_ARGUMENT;
        }
        if (out_message != nullptr)
        {
            *out_message = "missing_get_callback";
        }
        return false;
    }

    float value = 0.0f;
    const EMC_Result result = row->get_float(row->user_data, &value);
    if (result != EMC_OK)
    {
        if (out_result != nullptr)
        {
            *out_result = result;
        }
        if (out_message != nullptr)
        {
            *out_message = "get_callback_failed";
        }
        return false;
    }

    if (out_value != nullptr)
    {
        *out_value = value;
    }

    return true;
}

void ClearTabsAndRows()
{
    for (size_t tab_index = 0; tab_index < g_tabs_in_order.size(); ++tab_index)
    {
        HubUiNamespaceTab* tab = g_tabs_in_order[tab_index];
        for (size_t mod_index = 0; mod_index < tab->mods.size(); ++mod_index)
        {
            HubUiModSection* mod = tab->mods[mod_index];
            for (size_t row_index = 0; row_index < mod->rows.size(); ++row_index)
            {
                delete mod->rows[row_index];
            }
            mod->rows.clear();
            delete mod;
        }
        tab->mods.clear();
        delete tab;
    }

    g_tabs_in_order.clear();
    g_rows_in_order.clear();
}

HubUiSettingRow* FindRow(const char* namespace_id, const char* mod_id, const char* setting_id)
{
    if (namespace_id == nullptr || mod_id == nullptr || setting_id == nullptr)
    {
        return nullptr;
    }

    for (size_t index = 0; index < g_rows_in_order.size(); ++index)
    {
        HubUiSettingRow* row = g_rows_in_order[index];
        if (row->namespace_id == namespace_id && row->mod_id == mod_id && row->setting_id == setting_id)
        {
            return row;
        }
    }

    return nullptr;
}

HubUiModSection* FindModSection(const char* namespace_id, const char* mod_id)
{
    if (namespace_id == nullptr || mod_id == nullptr)
    {
        return nullptr;
    }

    for (size_t tab_index = 0; tab_index < g_tabs_in_order.size(); ++tab_index)
    {
        HubUiNamespaceTab* tab = g_tabs_in_order[tab_index];
        if (tab->namespace_id != namespace_id)
        {
            continue;
        }

        for (size_t mod_index = 0; mod_index < tab->mods.size(); ++mod_index)
        {
            HubUiModSection* mod = tab->mods[mod_index];
            if (mod->mod_id == mod_id)
            {
                return mod;
            }
        }

        return nullptr;
    }

    return nullptr;
}

HubUiNamespaceTab* FindNamespaceTab(const char* namespace_id)
{
    if (namespace_id == nullptr)
    {
        return nullptr;
    }

    for (size_t tab_index = 0; tab_index < g_tabs_in_order.size(); ++tab_index)
    {
        HubUiNamespaceTab* tab = g_tabs_in_order[tab_index];
        if (tab->namespace_id == namespace_id)
        {
            return tab;
        }
    }

    return nullptr;
}

void RefreshValueRowsForMod(HubUiModSection* mod, bool force_refresh)
{
    if (mod == nullptr)
    {
        return;
    }

    if (force_refresh)
    {
        for (size_t row_index = 0; row_index < mod->rows.size(); ++row_index)
        {
            HubUiSettingRow* row = mod->rows[row_index];
            if (row->kind == HUB_UI_ROW_KIND_BOOL
                || row->kind == HUB_UI_ROW_KIND_KEYBIND
                || row->kind == HUB_UI_ROW_KIND_INT
                || row->kind == HUB_UI_ROW_KIND_FLOAT)
            {
                row->dirty = false;
                row->capture_active = false;
                row->int_text_parse_error = false;
                row->float_text_parse_error = false;
            }
        }
    }

    for (size_t row_index = 0; row_index < mod->rows.size(); ++row_index)
    {
        HubUiSettingRow* row = mod->rows[row_index];
        if (row->kind != HUB_UI_ROW_KIND_BOOL
            && row->kind != HUB_UI_ROW_KIND_KEYBIND
            && row->kind != HUB_UI_ROW_KIND_INT
            && row->kind != HUB_UI_ROW_KIND_FLOAT)
        {
            continue;
        }

        if (!force_refresh && row->dirty)
        {
            continue;
        }

        if (row->kind == HUB_UI_ROW_KIND_BOOL)
        {
            int32_t value = 0;
            EMC_Result get_result = EMC_OK;
            const char* message = kLogNone;
            if (!TryReadBool(row, &value, &get_result, &message))
            {
                LogActionRefreshGetFailure(row, get_result, message);
                continue;
            }

            row->canonical_bool_value = value;
            row->pending_bool_value = value;
            row->dirty = false;
            row->inline_error.clear();
            continue;
        }

        if (row->kind == HUB_UI_ROW_KIND_KEYBIND)
        {
            EMC_KeybindValueV1 value;
            value.keycode = EMC_KEY_UNBOUND;
            value.modifiers = 0;
            EMC_Result get_result = EMC_OK;
            const char* message = kLogNone;
            if (!TryReadKeybind(row, &value, &get_result, &message))
            {
                LogActionRefreshGetFailure(row, get_result, message);
                continue;
            }

            row->canonical_keybind_value = value;
            row->pending_keybind_value = value;
            row->dirty = false;
            row->inline_error.clear();
            continue;
        }

        if (row->kind == HUB_UI_ROW_KIND_INT)
        {
            int32_t value = 0;
            EMC_Result get_result = EMC_OK;
            const char* message = kLogNone;
            if (!TryReadInt(row, &value, &get_result, &message))
            {
                LogActionRefreshGetFailure(row, get_result, message);
                continue;
            }

            row->canonical_int_value = value;
            row->pending_int_value = value;
            row->pending_int_text = FormatIntText(value);
            row->int_text_parse_error = false;
            row->dirty = false;
            row->inline_error.clear();
            continue;
        }

        float value = 0.0f;
        EMC_Result get_result = EMC_OK;
        const char* message = kLogNone;
        if (!TryReadFloat(row, &value, &get_result, &message))
        {
            LogActionRefreshGetFailure(row, get_result, message);
            continue;
        }

        row->canonical_float_value = value;
        row->pending_float_value = value;
        row->pending_float_text = FormatFloatText(value, row->float_display_decimals);
        row->float_text_parse_error = false;
        row->dirty = false;
        row->inline_error.clear();
    }
}

void __cdecl BuildSessionRow(
    const HubRegistryNamespaceView* namespace_view,
    const HubRegistryModView* mod_view,
    const HubRegistrySettingView* setting_view,
    void* user_data)
{
    if (namespace_view == nullptr || mod_view == nullptr || setting_view == nullptr || user_data == nullptr)
    {
        return;
    }

    std::map<std::string, HubUiNamespaceTab*>* tabs_by_id =
        static_cast<std::map<std::string, HubUiNamespaceTab*>*>(user_data);
    HubUiNamespaceTab* tab = nullptr;

    std::map<std::string, HubUiNamespaceTab*>::const_iterator tab_it = tabs_by_id->find(namespace_view->namespace_id);
    if (tab_it == tabs_by_id->end())
    {
        tab = new HubUiNamespaceTab();
        tab->namespace_id = namespace_view->namespace_id;
        tab->namespace_display_name = namespace_view->namespace_display_name;
        tab->search_query.clear();
        g_tabs_in_order.push_back(tab);
        (*tabs_by_id)[tab->namespace_id] = tab;
    }
    else
    {
        tab = tab_it->second;
    }

    HubUiModSection* mod = nullptr;
    for (size_t mod_index = 0; mod_index < tab->mods.size(); ++mod_index)
    {
        HubUiModSection* existing_mod = tab->mods[mod_index];
        if (existing_mod->namespace_id == mod_view->namespace_id && existing_mod->mod_id == mod_view->mod_id)
        {
            mod = existing_mod;
            break;
        }
    }

    if (mod == nullptr)
    {
        mod = new HubUiModSection();
        mod->namespace_id = mod_view->namespace_id;
        mod->mod_id = mod_view->mod_id;
        mod->mod_display_name = mod_view->mod_display_name;
        mod->collapsed = false;
        tab->mods.push_back(mod);
    }

    if (setting_view->kind != HUB_REGISTRY_SETTING_KIND_BOOL
        && setting_view->kind != HUB_REGISTRY_SETTING_KIND_KEYBIND
        && setting_view->kind != HUB_REGISTRY_SETTING_KIND_INT
        && setting_view->kind != HUB_REGISTRY_SETTING_KIND_FLOAT
        && setting_view->kind != HUB_REGISTRY_SETTING_KIND_ACTION)
    {
        return;
    }

    HubUiSettingRow* row = new HubUiSettingRow();
    if (setting_view->kind == HUB_REGISTRY_SETTING_KIND_BOOL)
    {
        row->kind = HUB_UI_ROW_KIND_BOOL;
    }
    else if (setting_view->kind == HUB_REGISTRY_SETTING_KIND_KEYBIND)
    {
        row->kind = HUB_UI_ROW_KIND_KEYBIND;
    }
    else if (setting_view->kind == HUB_REGISTRY_SETTING_KIND_INT)
    {
        row->kind = HUB_UI_ROW_KIND_INT;
    }
    else if (setting_view->kind == HUB_REGISTRY_SETTING_KIND_FLOAT)
    {
        row->kind = HUB_UI_ROW_KIND_FLOAT;
    }
    else
    {
        row->kind = HUB_UI_ROW_KIND_ACTION;
    }
    row->namespace_id = namespace_view->namespace_id;
    row->namespace_display_name = namespace_view->namespace_display_name;
    row->mod_id = mod_view->mod_id;
    row->mod_display_name = mod_view->mod_display_name;
    row->setting_id = setting_view->setting_id;
    row->label = setting_view->label;
    row->description = setting_view->description;
    row->user_data = setting_view->user_data;
    row->get_bool = setting_view->get_bool;
    row->set_bool = setting_view->set_bool;
    row->canonical_bool_value = 0;
    row->pending_bool_value = 0;
    row->get_keybind = setting_view->get_keybind;
    row->set_keybind = setting_view->set_keybind;
    row->canonical_keybind_value.keycode = EMC_KEY_UNBOUND;
    row->canonical_keybind_value.modifiers = 0;
    row->pending_keybind_value.keycode = EMC_KEY_UNBOUND;
    row->pending_keybind_value.modifiers = 0;
    row->get_int = setting_view->get_int;
    row->set_int = setting_view->set_int;
    row->int_min_value = setting_view->int_min_value;
    row->int_max_value = setting_view->int_max_value;
    row->int_step = setting_view->int_step;
    row->canonical_int_value = 0;
    row->pending_int_value = 0;
    row->pending_int_text = "0";
    row->int_text_parse_error = false;
    row->get_float = setting_view->get_float;
    row->set_float = setting_view->set_float;
    row->float_min_value = setting_view->float_min_value;
    row->float_max_value = setting_view->float_max_value;
    row->float_step = setting_view->float_step;
    row->float_display_decimals = setting_view->float_display_decimals;
    row->canonical_float_value = 0.0f;
    row->pending_float_value = 0.0f;
    row->pending_float_text = FormatFloatText(0.0f, row->float_display_decimals);
    row->float_text_parse_error = false;
    row->on_action = setting_view->on_action;
    row->action_flags = setting_view->action_flags;
    row->dirty = false;
    row->capture_active = false;
    row->inline_error.clear();
    row->parent_mod = mod;

    mod->rows.push_back(row);
    g_rows_in_order.push_back(row);
}
}

void HubUi_SetOptionsWindowOpen(bool is_open)
{
    g_options_window_open = is_open;
}

bool HubUi_IsOptionsWindowOpen()
{
    return g_options_window_open;
}

void HubUi_ClearSessionModel()
{
    ClearTabsAndRows();
}

void HubUi_RebuildSessionModelFromRegistry()
{
    ClearTabsAndRows();

    std::map<std::string, HubUiNamespaceTab*> tabs_by_id;
    HubRegistry_ForEachSettingInOrder(&BuildSessionRow, &tabs_by_id);
}

void HubUi_PerformInitialSync()
{
    for (size_t row_index = 0; row_index < g_rows_in_order.size(); ++row_index)
    {
        HubUiSettingRow* row = g_rows_in_order[row_index];
        row->capture_active = false;

        if (row->kind == HUB_UI_ROW_KIND_BOOL)
        {
            int32_t value = 0;
            EMC_Result get_result = EMC_OK;
            const char* message = kLogNone;
            if (!TryReadBool(row, &value, &get_result, &message))
            {
                row->dirty = false;
                row->inline_error = kUnavailableMessage;
                LogUiGetFailure(row, get_result, message);
                continue;
            }

            row->canonical_bool_value = value;
            row->pending_bool_value = value;
            row->dirty = false;
            row->inline_error.clear();
            continue;
        }

        if (row->kind == HUB_UI_ROW_KIND_KEYBIND)
        {
            EMC_KeybindValueV1 value;
            value.keycode = EMC_KEY_UNBOUND;
            value.modifiers = 0;
            EMC_Result get_result = EMC_OK;
            const char* message = kLogNone;
            if (!TryReadKeybind(row, &value, &get_result, &message))
            {
                row->dirty = false;
                row->inline_error = kUnavailableMessage;
                LogUiGetFailure(row, get_result, message);
                continue;
            }

            row->canonical_keybind_value = value;
            row->pending_keybind_value = value;
            row->dirty = false;
            row->inline_error.clear();
            continue;
        }

        if (row->kind == HUB_UI_ROW_KIND_INT)
        {
            int32_t value = 0;
            EMC_Result get_result = EMC_OK;
            const char* message = kLogNone;
            if (!TryReadInt(row, &value, &get_result, &message))
            {
                row->dirty = false;
                row->int_text_parse_error = false;
                row->pending_int_text.clear();
                row->inline_error = kUnavailableMessage;
                LogUiGetFailure(row, get_result, message);
                continue;
            }

            row->canonical_int_value = value;
            row->pending_int_value = value;
            row->pending_int_text = FormatIntText(value);
            row->int_text_parse_error = false;
            row->dirty = false;
            row->inline_error.clear();
            continue;
        }

        if (row->kind == HUB_UI_ROW_KIND_FLOAT)
        {
            float value = 0.0f;
            EMC_Result get_result = EMC_OK;
            const char* message = kLogNone;
            if (!TryReadFloat(row, &value, &get_result, &message))
            {
                row->dirty = false;
                row->float_text_parse_error = false;
                row->pending_float_text.clear();
                row->inline_error = kUnavailableMessage;
                LogUiGetFailure(row, get_result, message);
                continue;
            }

            row->canonical_float_value = value;
            row->pending_float_value = value;
            row->pending_float_text = FormatFloatText(value, row->float_display_decimals);
            row->float_text_parse_error = false;
            row->dirty = false;
            row->inline_error.clear();
            continue;
        }
    }
}

bool HubUi_IsAnyKeybindCaptureActive()
{
    for (size_t row_index = 0; row_index < g_rows_in_order.size(); ++row_index)
    {
        HubUiSettingRow* row = g_rows_in_order[row_index];
        if (row->kind == HUB_UI_ROW_KIND_KEYBIND && row->capture_active)
        {
            return true;
        }
    }

    return false;
}

EMC_Result HubUi_SetModCollapsed(const char* namespace_id, const char* mod_id, bool is_collapsed)
{
    HubUiModSection* mod = FindModSection(namespace_id, mod_id);
    if (mod == nullptr)
    {
        return EMC_ERR_NOT_FOUND;
    }

    mod->collapsed = is_collapsed;
    return EMC_OK;
}

bool HubUi_GetModCollapsed(const char* namespace_id, const char* mod_id, bool* out_is_collapsed)
{
    if (out_is_collapsed == nullptr)
    {
        return false;
    }

    HubUiModSection* mod = FindModSection(namespace_id, mod_id);
    if (mod == nullptr)
    {
        return false;
    }

    *out_is_collapsed = mod->collapsed;
    return true;
}

EMC_Result HubUi_SetNamespaceSearchQuery(const char* namespace_id, const char* search_query)
{
    HubUiNamespaceTab* tab = FindNamespaceTab(namespace_id);
    if (tab == nullptr)
    {
        return EMC_ERR_NOT_FOUND;
    }

    tab->search_query = search_query != nullptr ? search_query : "";
    return EMC_OK;
}

bool HubUi_GetNamespaceSearchQuery(const char* namespace_id, const char** out_search_query)
{
    if (out_search_query == nullptr)
    {
        return false;
    }

    *out_search_query = "";

    HubUiNamespaceTab* tab = FindNamespaceTab(namespace_id);
    if (tab == nullptr)
    {
        return false;
    }

    *out_search_query = tab->search_query.c_str();
    return true;
}

bool HubUi_DoesRowMatchNamespaceSearch(const HubUiRowView* row)
{
    if (row == nullptr || row->namespace_id == nullptr)
    {
        return false;
    }

    HubUiNamespaceTab* tab = FindNamespaceTab(row->namespace_id);
    if (tab == nullptr)
    {
        return false;
    }

    if (tab->search_query.empty())
    {
        return true;
    }

    return ContainsSearchMatch(row->mod_display_name, tab->search_query)
        || ContainsSearchMatch(row->label, tab->search_query)
        || ContainsSearchMatch(row->description, tab->search_query);
}

bool HubUi_DoesSettingMatchNamespaceSearch(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    bool* out_matches)
{
    if (out_matches == nullptr)
    {
        return false;
    }

    *out_matches = false;

    HubUiSettingRow* row = FindRow(namespace_id, mod_id, setting_id);
    if (row == nullptr)
    {
        return false;
    }

    HubUiRowView view;
    view.namespace_id = row->namespace_id.c_str();
    view.mod_display_name = row->mod_display_name.c_str();
    view.label = row->label.c_str();
    view.description = row->description.c_str();
    *out_matches = HubUi_DoesRowMatchNamespaceSearch(&view);
    return true;
}

EMC_Result HubUi_SetPendingBool(const char* namespace_id, const char* mod_id, const char* setting_id, int32_t value)
{
    if (!IsBoolValueValid(value))
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    HubUiSettingRow* row = FindRow(namespace_id, mod_id, setting_id);
    if (row == nullptr || row->kind != HUB_UI_ROW_KIND_BOOL)
    {
        return EMC_ERR_NOT_FOUND;
    }

    row->pending_bool_value = value;
    row->dirty = row->pending_bool_value != row->canonical_bool_value;
    return EMC_OK;
}

EMC_Result HubUi_AdjustPendingIntStep(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    int32_t step_delta)
{
    HubUiSettingRow* row = FindRow(namespace_id, mod_id, setting_id);
    if (row == nullptr || row->kind != HUB_UI_ROW_KIND_INT)
    {
        return EMC_ERR_NOT_FOUND;
    }

    const int64_t next_value = static_cast<int64_t>(row->pending_int_value)
        + (static_cast<int64_t>(step_delta) * static_cast<int64_t>(row->int_step));
    int32_t clamped = row->pending_int_value;
    if (next_value < static_cast<int64_t>(std::numeric_limits<int32_t>::min()))
    {
        clamped = std::numeric_limits<int32_t>::min();
    }
    else if (next_value > static_cast<int64_t>(std::numeric_limits<int32_t>::max()))
    {
        clamped = std::numeric_limits<int32_t>::max();
    }
    else
    {
        clamped = static_cast<int32_t>(next_value);
    }

    row->pending_int_value = ClampIntValue(clamped, row->int_min_value, row->int_max_value);
    row->pending_int_text = FormatIntText(row->pending_int_value);
    row->int_text_parse_error = false;
    row->inline_error.clear();
    RecomputeIntDirty(row);
    return EMC_OK;
}

EMC_Result HubUi_SetPendingIntFromText(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    const char* text)
{
    if (text == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    HubUiSettingRow* row = FindRow(namespace_id, mod_id, setting_id);
    if (row == nullptr || row->kind != HUB_UI_ROW_KIND_INT)
    {
        return EMC_ERR_NOT_FOUND;
    }

    row->pending_int_text = text;

    int32_t parsed_value = 0;
    if (!TryParseIntText(text, &parsed_value))
    {
        row->int_text_parse_error = true;
        row->inline_error = kInvalidIntTextMessage;
        RecomputeIntDirty(row);
        return EMC_OK;
    }

    row->pending_int_value = SnapIntValueToStep(parsed_value, row->int_min_value, row->int_max_value, row->int_step);
    row->int_text_parse_error = false;
    row->inline_error.clear();
    RecomputeIntDirty(row);
    return EMC_OK;
}

EMC_Result HubUi_NormalizePendingIntText(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id)
{
    HubUiSettingRow* row = FindRow(namespace_id, mod_id, setting_id);
    if (row == nullptr || row->kind != HUB_UI_ROW_KIND_INT)
    {
        return EMC_ERR_NOT_FOUND;
    }

    row->pending_int_text = FormatIntText(row->pending_int_value);
    row->int_text_parse_error = false;
    row->inline_error.clear();
    RecomputeIntDirty(row);
    return EMC_OK;
}

EMC_Result HubUi_AdjustPendingFloatStep(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    int32_t step_delta)
{
    HubUiSettingRow* row = FindRow(namespace_id, mod_id, setting_id);
    if (row == nullptr || row->kind != HUB_UI_ROW_KIND_FLOAT)
    {
        return EMC_ERR_NOT_FOUND;
    }

    const float delta = static_cast<float>(step_delta) * row->float_step;
    row->pending_float_value = SnapFloatValueToStep(
        row->pending_float_value + delta,
        row->float_min_value,
        row->float_max_value,
        row->float_step);
    row->pending_float_text = FormatFloatText(row->pending_float_value, row->float_display_decimals);
    row->float_text_parse_error = false;
    row->inline_error.clear();
    RecomputeFloatDirty(row);
    return EMC_OK;
}

EMC_Result HubUi_SetPendingFloatFromText(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    const char* text)
{
    if (text == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    HubUiSettingRow* row = FindRow(namespace_id, mod_id, setting_id);
    if (row == nullptr || row->kind != HUB_UI_ROW_KIND_FLOAT)
    {
        return EMC_ERR_NOT_FOUND;
    }

    row->pending_float_text = text;

    float parsed_value = 0.0f;
    if (!TryParseFloatText(text, &parsed_value))
    {
        row->float_text_parse_error = true;
        row->inline_error = kInvalidFloatTextMessage;
        RecomputeFloatDirty(row);
        return EMC_OK;
    }

    row->pending_float_value = SnapFloatValueToStep(parsed_value, row->float_min_value, row->float_max_value, row->float_step);
    row->float_text_parse_error = false;
    row->inline_error.clear();
    RecomputeFloatDirty(row);
    return EMC_OK;
}

EMC_Result HubUi_NormalizePendingFloatText(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id)
{
    HubUiSettingRow* row = FindRow(namespace_id, mod_id, setting_id);
    if (row == nullptr || row->kind != HUB_UI_ROW_KIND_FLOAT)
    {
        return EMC_ERR_NOT_FOUND;
    }

    row->pending_float_text = FormatFloatText(row->pending_float_value, row->float_display_decimals);
    row->float_text_parse_error = false;
    row->inline_error.clear();
    RecomputeFloatDirty(row);
    return EMC_OK;
}

EMC_Result HubUi_BeginKeybindCapture(const char* namespace_id, const char* mod_id, const char* setting_id)
{
    HubUiSettingRow* row = FindRow(namespace_id, mod_id, setting_id);
    if (row == nullptr || row->kind != HUB_UI_ROW_KIND_KEYBIND)
    {
        return EMC_ERR_NOT_FOUND;
    }

    for (size_t row_index = 0; row_index < g_rows_in_order.size(); ++row_index)
    {
        HubUiSettingRow* candidate = g_rows_in_order[row_index];
        if (candidate->kind == HUB_UI_ROW_KIND_KEYBIND)
        {
            candidate->capture_active = false;
        }
    }

    row->capture_active = true;
    return EMC_OK;
}

EMC_Result HubUi_CancelKeybindCapture(const char* namespace_id, const char* mod_id, const char* setting_id)
{
    HubUiSettingRow* row = FindRow(namespace_id, mod_id, setting_id);
    if (row == nullptr || row->kind != HUB_UI_ROW_KIND_KEYBIND)
    {
        return EMC_ERR_NOT_FOUND;
    }

    row->capture_active = false;
    return EMC_OK;
}

EMC_Result HubUi_ApplyCapturedKeycode(const char* namespace_id, const char* mod_id, const char* setting_id, int32_t keycode)
{
    HubUiSettingRow* row = FindRow(namespace_id, mod_id, setting_id);
    if (row == nullptr || row->kind != HUB_UI_ROW_KIND_KEYBIND)
    {
        return EMC_ERR_NOT_FOUND;
    }

    const int32_t escape_keycode = static_cast<int32_t>(OIS::KC_ESCAPE);
    const int32_t backspace_keycode = static_cast<int32_t>(OIS::KC_BACK);

    if (keycode == 27 || keycode == escape_keycode)
    {
        row->capture_active = false;
        return EMC_OK;
    }

    if (keycode == 8 || keycode == backspace_keycode)
    {
        row->pending_keybind_value.keycode = EMC_KEY_UNBOUND;
        row->pending_keybind_value.modifiers = 0;
        row->capture_active = false;
        row->dirty = !KeybindEquals(row->pending_keybind_value, row->canonical_keybind_value);
        return EMC_OK;
    }

    row->pending_keybind_value.keycode = keycode;
    row->pending_keybind_value.modifiers = 0;
    row->capture_active = false;
    row->dirty = !KeybindEquals(row->pending_keybind_value, row->canonical_keybind_value);
    return EMC_OK;
}

EMC_Result HubUi_ApplyCapturedKeycodeToActiveRow(int32_t keycode)
{
    for (size_t row_index = 0; row_index < g_rows_in_order.size(); ++row_index)
    {
        HubUiSettingRow* row = g_rows_in_order[row_index];
        if (row->kind != HUB_UI_ROW_KIND_KEYBIND || !row->capture_active)
        {
            continue;
        }

        return HubUi_ApplyCapturedKeycode(row->namespace_id.c_str(), row->mod_id.c_str(), row->setting_id.c_str(), keycode);
    }

    return EMC_ERR_NOT_FOUND;
}

EMC_Result HubUi_ClearPendingKeybind(const char* namespace_id, const char* mod_id, const char* setting_id)
{
    HubUiSettingRow* row = FindRow(namespace_id, mod_id, setting_id);
    if (row == nullptr || row->kind != HUB_UI_ROW_KIND_KEYBIND)
    {
        return EMC_ERR_NOT_FOUND;
    }

    row->pending_keybind_value.keycode = EMC_KEY_UNBOUND;
    row->pending_keybind_value.modifiers = 0;
    row->capture_active = false;
    row->dirty = !KeybindEquals(row->pending_keybind_value, row->canonical_keybind_value);
    return EMC_OK;
}

EMC_Result HubUi_InvokeActionRow(const char* namespace_id, const char* mod_id, const char* setting_id)
{
    HubUiSettingRow* row = FindRow(namespace_id, mod_id, setting_id);
    if (row == nullptr || row->kind != HUB_UI_ROW_KIND_ACTION)
    {
        return EMC_ERR_NOT_FOUND;
    }

    if (row->on_action == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    char err_buf[256];
    std::memset(err_buf, 0, sizeof(err_buf));
    const EMC_Result callback_result = row->on_action(row->user_data, err_buf, (uint32_t)sizeof(err_buf));
    if (callback_result != EMC_OK)
    {
        const char* message = ResolveErrorMessage(err_buf, kActionFailedMessage);
        row->inline_error = message;
        LogActionFailure(row, callback_result, message);
        return callback_result;
    }

    row->inline_error.clear();
    const bool force_refresh = (row->action_flags & EMC_ACTION_FORCE_REFRESH) != 0u;
    RefreshValueRowsForMod(row->parent_mod, force_refresh);
    return EMC_OK;
}

uint32_t HubUi_GetRowCount()
{
    return static_cast<uint32_t>(g_rows_in_order.size());
}

bool HubUi_GetRowViewByIndex(uint32_t index, HubUiRowView* out_view)
{
    if (out_view == nullptr)
    {
        return false;
    }

    if (index >= g_rows_in_order.size())
    {
        return false;
    }

    HubUiSettingRow* row = g_rows_in_order[index];
    out_view->token = row;
    out_view->kind = row->kind;
    out_view->dirty = row->dirty;
    out_view->capture_active = row->capture_active;
    out_view->namespace_id = row->namespace_id.c_str();
    out_view->namespace_display_name = row->namespace_display_name.c_str();
    out_view->mod_id = row->mod_id.c_str();
    out_view->mod_display_name = row->mod_display_name.c_str();
    out_view->setting_id = row->setting_id.c_str();
    out_view->label = row->label.c_str();
    out_view->description = row->description.c_str();
    out_view->inline_error = row->inline_error.c_str();
    out_view->user_data = row->user_data;
    out_view->get_bool = row->get_bool;
    out_view->set_bool = row->set_bool;
    out_view->pending_bool_value = row->pending_bool_value;
    out_view->get_keybind = row->get_keybind;
    out_view->set_keybind = row->set_keybind;
    out_view->pending_keybind_value = row->pending_keybind_value;
    out_view->get_int = row->get_int;
    out_view->set_int = row->set_int;
    out_view->int_min_value = row->int_min_value;
    out_view->int_max_value = row->int_max_value;
    out_view->int_step = row->int_step;
    out_view->pending_int_value = row->pending_int_value;
    out_view->pending_int_text = row->pending_int_text.c_str();
    out_view->int_text_parse_error = row->int_text_parse_error;
    out_view->get_float = row->get_float;
    out_view->set_float = row->set_float;
    out_view->float_min_value = row->float_min_value;
    out_view->float_max_value = row->float_max_value;
    out_view->float_step = row->float_step;
    out_view->float_display_decimals = row->float_display_decimals;
    out_view->pending_float_value = row->pending_float_value;
    out_view->pending_float_text = row->pending_float_text.c_str();
    out_view->float_text_parse_error = row->float_text_parse_error;
    return true;
}

void HubUi_OnCommitSetFailure(void* token, const char* message)
{
    if (token == nullptr)
    {
        return;
    }

    HubUiSettingRow* row = static_cast<HubUiSettingRow*>(token);
    row->inline_error = SafeLogValue(message);
}

void HubUi_OnCommitSyncBool(void* token, int32_t canonical_value)
{
    if (token == nullptr)
    {
        return;
    }

    HubUiSettingRow* row = static_cast<HubUiSettingRow*>(token);
    if (row->kind != HUB_UI_ROW_KIND_BOOL)
    {
        return;
    }

    row->canonical_bool_value = IsBoolValueValid(canonical_value) ? canonical_value : 0;
    row->pending_bool_value = row->canonical_bool_value;
    row->dirty = false;
    row->inline_error.clear();
}

void HubUi_OnCommitSyncKeybind(void* token, EMC_KeybindValueV1 canonical_value)
{
    if (token == nullptr)
    {
        return;
    }

    HubUiSettingRow* row = static_cast<HubUiSettingRow*>(token);
    if (row->kind != HUB_UI_ROW_KIND_KEYBIND)
    {
        return;
    }

    row->canonical_keybind_value = canonical_value;
    row->pending_keybind_value = canonical_value;
    row->dirty = false;
    row->capture_active = false;
    row->inline_error.clear();
}

void HubUi_OnCommitSyncInt(void* token, int32_t canonical_value)
{
    if (token == nullptr)
    {
        return;
    }

    HubUiSettingRow* row = static_cast<HubUiSettingRow*>(token);
    if (row->kind != HUB_UI_ROW_KIND_INT)
    {
        return;
    }

    row->canonical_int_value = canonical_value;
    row->pending_int_value = canonical_value;
    row->pending_int_text = FormatIntText(canonical_value);
    row->int_text_parse_error = false;
    row->dirty = false;
    row->inline_error.clear();
}

void HubUi_OnCommitSyncFloat(void* token, float canonical_value)
{
    if (token == nullptr)
    {
        return;
    }

    HubUiSettingRow* row = static_cast<HubUiSettingRow*>(token);
    if (row->kind != HUB_UI_ROW_KIND_FLOAT)
    {
        return;
    }

    row->canonical_float_value = canonical_value;
    row->pending_float_value = canonical_value;
    row->pending_float_text = FormatFloatText(canonical_value, row->float_display_decimals);
    row->float_text_parse_error = false;
    row->dirty = false;
    row->inline_error.clear();
}
