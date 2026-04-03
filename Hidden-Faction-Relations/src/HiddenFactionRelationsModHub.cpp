#include "HiddenFactionRelationsModHub.h"

#include "HiddenFactionRelationsConfig.h"

#include <Debug.h>

#include "emc/mod_hub_client.h"

#include <ois/OISKeyboard.h>
#include <sstream>

namespace
{
const char* kPluginName = "Hidden-Faction-Relations";
const char* kHubNamespaceId = "emkej.qol";
const char* kHubNamespaceDisplayName = "Emkej QoL";
const char* kHubModId = "hidden_faction_relations";
const char* kHubModDisplayName = "Hidden Faction Relations";

typedef bool HiddenFactionRelationsConfigSnapshot::*ConfigBoolField;
typedef int HiddenFactionRelationsConfigSnapshot::*ConfigIntField;

emc::ModHubClient g_modHubClient;
bool g_modHubClientConfigured = false;

void LogInfoLine(const std::string& message)
{
    std::stringstream line;
    line << kPluginName << " INFO: " << message;
    DebugLog(line.str().c_str());
}

void LogWarnLine(const std::string& message)
{
    std::stringstream line;
    line << kPluginName << " WARN: " << message;
    ErrorLog(line.str().c_str());
}

void WriteHubErrorText(char* err_buf, uint32_t err_buf_size, const char* text)
{
    if (err_buf == 0 || err_buf_size == 0u)
    {
        return;
    }

    if (text == 0)
    {
        err_buf[0] = '\0';
        return;
    }

    uint32_t index = 0u;
    while (index + 1u < err_buf_size && text[index] != '\0')
    {
        err_buf[index] = text[index];
        ++index;
    }

    err_buf[index] = '\0';
}

bool IsValidHubUserData(void* user_data)
{
    return user_data == &g_modHubClient;
}

EMC_Result GetHubBoolSetting(void* user_data, int32_t* out_value, ConfigBoolField field)
{
    if (!IsValidHubUserData(user_data) || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HiddenFactionRelationsConfigSnapshot config = HiddenFactionRelationsConfig_Capture();
    *out_value = (config.*field) ? 1 : 0;
    return EMC_OK;
}

EMC_Result SetHubBoolSetting(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size,
    ConfigBoolField field)
{
    if (!IsValidHubUserData(user_data))
    {
        WriteHubErrorText(err_buf, err_buf_size, "invalid_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value != 0 && value != 1)
    {
        WriteHubErrorText(err_buf, err_buf_size, "invalid_bool");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HiddenFactionRelationsConfigSnapshot previous = HiddenFactionRelationsConfig_Capture();
    HiddenFactionRelationsConfigSnapshot updated = previous;
    updated.*field = value != 0;
    HiddenFactionRelationsConfig_Normalize(&updated);
    HiddenFactionRelationsConfig_Apply(updated);

    std::string saveError;
    if (!HiddenFactionRelationsConfig_Save(updated, &saveError))
    {
        HiddenFactionRelationsConfig_Apply(previous);
        WriteHubErrorText(err_buf, err_buf_size, saveError.c_str());
        return EMC_ERR_INTERNAL;
    }

    WriteHubErrorText(err_buf, err_buf_size, 0);
    return EMC_OK;
}

EMC_Result GetHubIntSetting(void* user_data, int32_t* out_value, ConfigIntField field)
{
    if (!IsValidHubUserData(user_data) || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HiddenFactionRelationsConfigSnapshot config = HiddenFactionRelationsConfig_Capture();
    *out_value = static_cast<int32_t>(config.*field);
    return EMC_OK;
}

EMC_Result SetHubIntSetting(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size,
    ConfigIntField field)
{
    if (!IsValidHubUserData(user_data))
    {
        WriteHubErrorText(err_buf, err_buf_size, "invalid_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HiddenFactionRelationsConfigSnapshot previous = HiddenFactionRelationsConfig_Capture();
    HiddenFactionRelationsConfigSnapshot updated = previous;
    updated.*field = static_cast<int>(value);
    HiddenFactionRelationsConfig_Normalize(&updated);
    HiddenFactionRelationsConfig_Apply(updated);

    std::string saveError;
    if (!HiddenFactionRelationsConfig_Save(updated, &saveError))
    {
        HiddenFactionRelationsConfig_Apply(previous);
        WriteHubErrorText(err_buf, err_buf_size, saveError.c_str());
        return EMC_ERR_INTERNAL;
    }

    WriteHubErrorText(err_buf, err_buf_size, 0);
    return EMC_OK;
}

EMC_Result __cdecl GetAutoFocusSearchOnOpenSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(
        user_data,
        out_value,
        &HiddenFactionRelationsConfigSnapshot::autoFocusSearchOnOpen);
}

EMC_Result __cdecl SetAutoFocusSearchOnOpenSetting(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size)
{
    return SetHubBoolSetting(
        user_data,
        value,
        err_buf,
        err_buf_size,
        &HiddenFactionRelationsConfigSnapshot::autoFocusSearchOnOpen);
}

bool IsValidOpenMenuKeycode(int32_t keycode)
{
    return keycode == EMC_KEY_UNBOUND || (keycode >= 0 && keycode <= 255);
}

EMC_Result __cdecl GetOpenMenuKeybindSetting(void* user_data, EMC_KeybindValueV1* out_value)
{
    if (!IsValidHubUserData(user_data) || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HiddenFactionRelationsConfigSnapshot config = HiddenFactionRelationsConfig_Capture();
    out_value->keycode = static_cast<int32_t>(config.openMenuKeycode);
    out_value->modifiers = 0u;
    return EMC_OK;
}

EMC_Result __cdecl SetOpenMenuKeybindSetting(
    void* user_data,
    EMC_KeybindValueV1 value,
    char* err_buf,
    uint32_t err_buf_size)
{
    if (!IsValidHubUserData(user_data))
    {
        WriteHubErrorText(err_buf, err_buf_size, "invalid_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value.modifiers != 0u)
    {
        WriteHubErrorText(err_buf, err_buf_size, "use_modifier_toggles");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (!IsValidOpenMenuKeycode(value.keycode))
    {
        WriteHubErrorText(err_buf, err_buf_size, "invalid_keybind");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HiddenFactionRelationsConfigSnapshot previous = HiddenFactionRelationsConfig_Capture();
    HiddenFactionRelationsConfigSnapshot updated = previous;
    updated.openMenuKeycode = static_cast<int>(value.keycode);
    HiddenFactionRelationsConfig_Normalize(&updated);
    HiddenFactionRelationsConfig_Apply(updated);

    std::string saveError;
    if (!HiddenFactionRelationsConfig_Save(updated, &saveError))
    {
        HiddenFactionRelationsConfig_Apply(previous);
        WriteHubErrorText(err_buf, err_buf_size, saveError.c_str());
        return EMC_ERR_INTERNAL;
    }

    WriteHubErrorText(err_buf, err_buf_size, 0);
    return EMC_OK;
}

EMC_Result __cdecl GetOpenMenuRequireCtrlSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(
        user_data,
        out_value,
        &HiddenFactionRelationsConfigSnapshot::openMenuRequireCtrl);
}

EMC_Result __cdecl SetOpenMenuRequireCtrlSetting(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size)
{
    return SetHubBoolSetting(
        user_data,
        value,
        err_buf,
        err_buf_size,
        &HiddenFactionRelationsConfigSnapshot::openMenuRequireCtrl);
}

EMC_Result __cdecl GetOpenMenuRequireShiftSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(
        user_data,
        out_value,
        &HiddenFactionRelationsConfigSnapshot::openMenuRequireShift);
}

EMC_Result __cdecl SetOpenMenuRequireShiftSetting(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size)
{
    return SetHubBoolSetting(
        user_data,
        value,
        err_buf,
        err_buf_size,
        &HiddenFactionRelationsConfigSnapshot::openMenuRequireShift);
}

EMC_Result __cdecl GetOpenMenuRequireAltSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(
        user_data,
        out_value,
        &HiddenFactionRelationsConfigSnapshot::openMenuRequireAlt);
}

EMC_Result __cdecl SetOpenMenuRequireAltSetting(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size)
{
    return SetHubBoolSetting(
        user_data,
        value,
        err_buf,
        err_buf_size,
        &HiddenFactionRelationsConfigSnapshot::openMenuRequireAlt);
}

EMC_Result __cdecl GetSearchInputWidthSetting(void* user_data, int32_t* out_value)
{
    return GetHubIntSetting(
        user_data,
        out_value,
        &HiddenFactionRelationsConfigSnapshot::searchInputWidth);
}

EMC_Result __cdecl SetSearchInputWidthSetting(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size)
{
    return SetHubIntSetting(
        user_data,
        value,
        err_buf,
        err_buf_size,
        &HiddenFactionRelationsConfigSnapshot::searchInputWidth);
}

EMC_Result __cdecl GetSearchInputHeightSetting(void* user_data, int32_t* out_value)
{
    return GetHubIntSetting(
        user_data,
        out_value,
        &HiddenFactionRelationsConfigSnapshot::searchInputHeight);
}

EMC_Result __cdecl SetSearchInputHeightSetting(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size)
{
    return SetHubIntSetting(
        user_data,
        value,
        err_buf,
        err_buf_size,
        &HiddenFactionRelationsConfigSnapshot::searchInputHeight);
}

void EnsureModHubClientConfigured()
{
    if (g_modHubClientConfigured)
    {
        return;
    }

    static const EMC_ModDescriptorV1 kModHubDescriptor = {
        kHubNamespaceId,
        kHubNamespaceDisplayName,
        kHubModId,
        kHubModDisplayName,
        &g_modHubClient
    };

    static const EMC_BoolSettingDefV1 kAutoFocusSearchOnOpenSetting = {
        "auto_focus_search_on_open",
        "Auto-focus search on open",
        "Focus the Hidden Factions search box when the tab opens",
        &g_modHubClient,
        &GetAutoFocusSearchOnOpenSetting,
        &SetAutoFocusSearchOnOpenSetting
    };

    static const EMC_KeybindSettingDefV1 kOpenMenuKeybindSetting = {
        "open_hidden_factions_key",
        "Open Hidden Factions key",
        "Gameplay shortcut key that opens Options on the Hidden Factions tab",
        &g_modHubClient,
        &GetOpenMenuKeybindSetting,
        &SetOpenMenuKeybindSetting
    };

    static const EMC_BoolSettingDefV1 kOpenMenuRequireCtrlSetting = {
        "open_hidden_factions_require_ctrl",
        "Open shortcut requires Ctrl",
        "Require Ctrl for the gameplay Hidden Factions shortcut",
        &g_modHubClient,
        &GetOpenMenuRequireCtrlSetting,
        &SetOpenMenuRequireCtrlSetting
    };

    static const EMC_BoolSettingDefV1 kOpenMenuRequireShiftSetting = {
        "open_hidden_factions_require_shift",
        "Open shortcut requires Shift",
        "Require Shift for the gameplay Hidden Factions shortcut",
        &g_modHubClient,
        &GetOpenMenuRequireShiftSetting,
        &SetOpenMenuRequireShiftSetting
    };

    static const EMC_BoolSettingDefV1 kOpenMenuRequireAltSetting = {
        "open_hidden_factions_require_alt",
        "Open shortcut requires Alt",
        "Require Alt for the gameplay Hidden Factions shortcut",
        &g_modHubClient,
        &GetOpenMenuRequireAltSetting,
        &SetOpenMenuRequireAltSetting
    };

    static const EMC_IntSettingDefV1 kSearchInputWidthSetting = {
        "search_input_width",
        "Search width",
        "Preferred Hidden Factions search box width in pixels",
        &g_modHubClient,
        120,
        720,
        1,
        &GetSearchInputWidthSetting,
        &SetSearchInputWidthSetting
    };

    static const EMC_IntSettingDefV1 kSearchInputHeightSetting = {
        "search_input_height",
        "Search height",
        "Preferred Hidden Factions control height in pixels",
        &g_modHubClient,
        22,
        48,
        1,
        &GetSearchInputHeightSetting,
        &SetSearchInputHeightSetting
    };

    static const emc::ModHubClientSettingRowV1 kModHubRows[] = {
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kAutoFocusSearchOnOpenSetting },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND, &kOpenMenuKeybindSetting },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kOpenMenuRequireCtrlSetting },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kOpenMenuRequireShiftSetting },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kOpenMenuRequireAltSetting },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &kSearchInputWidthSetting },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &kSearchInputHeightSetting }
    };

    static const emc::ModHubClientTableRegistrationV1 kModHubRegistration = {
        &kModHubDescriptor,
        kModHubRows,
        static_cast<uint32_t>(sizeof(kModHubRows) / sizeof(kModHubRows[0]))
    };

    emc::ModHubClient::Config config;
    config.table_registration = &kModHubRegistration;
    g_modHubClient.SetConfig(config);
    g_modHubClientConfigured = true;
}
}

void HiddenFactionRelationsModHub_OnStartup()
{
    EnsureModHubClientConfigured();

    const emc::ModHubClient::AttemptResult result = g_modHubClient.OnStartup();
    if (result == emc::ModHubClient::ATTACH_SUCCESS)
    {
        LogInfoLine("event=mod_hub_attached use_hub_ui=1");
        return;
    }

    if (result == emc::ModHubClient::ATTACH_FAILED)
    {
        if (g_modHubClient.IsAttachRetryPending())
        {
            LogInfoLine("event=mod_hub_attach_retry_pending use_hub_ui=0");
            return;
        }

        LogWarnLine("event=mod_hub_fallback reason=get_api_failed use_hub_ui=0");
        return;
    }

    if (result == emc::ModHubClient::REGISTRATION_FAILED)
    {
        LogWarnLine("event=mod_hub_fallback reason=register_mod_or_setting_failed use_hub_ui=0");
        return;
    }

    LogWarnLine("event=mod_hub_fallback reason=invalid_client_configuration use_hub_ui=0");
}
