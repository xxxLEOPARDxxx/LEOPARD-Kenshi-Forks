#include <Debug.h>

#include "MapMarkersModHub.h"

#include "emc/mod_hub_client.h"

#include <Windows.h>

#include <sstream>

namespace
{
const char* kHubNamespaceId = "emkej.qol";
const char* kHubNamespaceDisplayName = "Emkej QoL";
const char* kHubModId = "map_markers";
const char* kHubModDisplayName = "Map-markers";

typedef bool MapMarkersModConfigSnapshot::*HubBoolField;
typedef int MapMarkersModConfigSnapshot::*HubIntField;

emc::ModHubClient g_modHubClient;
bool g_modHubClientConfigured = false;

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

EMC_Result GetHubBoolSetting(void* user_data, int32_t* out_value, HubBoolField field)
{
    if (!IsValidHubUserData(user_data) || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const MapMarkersModConfigSnapshot config = MapMarkers_CaptureModConfigSnapshot();
    *out_value = (config.*field) ? 1 : 0;
    return EMC_OK;
}

EMC_Result GetHubIntSetting(void* user_data, int32_t* out_value, HubIntField field)
{
    if (!IsValidHubUserData(user_data) || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const MapMarkersModConfigSnapshot config = MapMarkers_CaptureModConfigSnapshot();
    *out_value = static_cast<int32_t>(config.*field);
    return EMC_OK;
}

EMC_Result SetHubBoolSetting(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size,
    HubBoolField field)
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

    const MapMarkersModConfigSnapshot previous = MapMarkers_CaptureModConfigSnapshot();
    MapMarkersModConfigSnapshot updated = previous;
    updated.*field = value != 0;
    MapMarkers_ApplyModConfigSnapshot(updated);

    if (!MapMarkers_PersistCurrentModConfig(false))
    {
        MapMarkers_ApplyModConfigSnapshot(previous);
        WriteHubErrorText(err_buf, err_buf_size, "persist_failed");
        return EMC_ERR_INTERNAL;
    }

    WriteHubErrorText(err_buf, err_buf_size, 0);
    return EMC_OK;
}

EMC_Result SetHubIntSetting(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size,
    HubIntField field,
    int32_t minValue,
    int32_t maxValue)
{
    if (!IsValidHubUserData(user_data))
    {
        WriteHubErrorText(err_buf, err_buf_size, "invalid_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value < minValue || value > maxValue)
    {
        WriteHubErrorText(err_buf, err_buf_size, "invalid_int_range");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const MapMarkersModConfigSnapshot previous = MapMarkers_CaptureModConfigSnapshot();
    MapMarkersModConfigSnapshot updated = previous;
    updated.*field = static_cast<int>(value);
    MapMarkers_ApplyModConfigSnapshot(updated);

    if (!MapMarkers_PersistCurrentModConfig(false))
    {
        MapMarkers_ApplyModConfigSnapshot(previous);
        WriteHubErrorText(err_buf, err_buf_size, "persist_failed");
        return EMC_ERR_INTERNAL;
    }

    WriteHubErrorText(err_buf, err_buf_size, 0);
    return EMC_OK;
}

EMC_Result __cdecl GetEnabledSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(user_data, out_value, &MapMarkersModConfigSnapshot::enabled);
}

EMC_Result __cdecl SetEnabledSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return SetHubBoolSetting(
        user_data,
        value,
        err_buf,
        err_buf_size,
        &MapMarkersModConfigSnapshot::enabled);
}

EMC_Result __cdecl GetCloseEditorOnMapCloseSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(user_data, out_value, &MapMarkersModConfigSnapshot::closeEditorOnMapClose);
}

EMC_Result __cdecl SetCloseEditorOnMapCloseSetting(
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
        &MapMarkersModConfigSnapshot::closeEditorOnMapClose);
}

EMC_Result __cdecl GetShowHoverLabelsSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(user_data, out_value, &MapMarkersModConfigSnapshot::showHoverLabels);
}

EMC_Result __cdecl SetShowHoverLabelsSetting(
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
        &MapMarkersModConfigSnapshot::showHoverLabels);
}

EMC_Result __cdecl GetDefaultMarkerTypeSetting(void* user_data, int32_t* out_value)
{
    return GetHubIntSetting(user_data, out_value, &MapMarkersModConfigSnapshot::defaultMarkerType);
}

EMC_Result __cdecl SetDefaultMarkerTypeSetting(
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
        &MapMarkersModConfigSnapshot::defaultMarkerType,
        0,
        9);
}

void LogModHubMessage(const char* event, const char* reason)
{
    std::stringstream line;
    line << "mod_hub event=" << (event != 0 ? event : "unknown");
    if (reason != 0 && *reason != '\0')
    {
        line << " reason=" << reason;
    }
    line << " result=" << g_modHubClient.LastAttemptFailureResult()
         << " use_hub_ui=" << (g_modHubClient.UseHubUi() ? "1" : "0");
    MapMarkers_LogProbeMessage(line.str().c_str());
}

void LogModHubStartupMessage(bool warn, const char* message)
{
    std::stringstream line;
    line << "Map-markers " << (warn ? "WARN: " : "INFO: ")
         << (message != 0 ? message : "Mod Hub startup state changed")
         << " result=" << g_modHubClient.LastAttemptFailureResult();
    if (warn)
    {
        ErrorLog(line.str().c_str());
        return;
    }

    DebugLog(line.str().c_str());
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
        &g_modHubClient };

    static const EMC_BoolSettingDefV1 kEnabledSetting = {
        "enabled",
        "Enabled",
        "Enable Map-markers map UI and runtime behavior",
        &g_modHubClient,
        &GetEnabledSetting,
        &SetEnabledSetting };

    static const EMC_BoolSettingDefV1 kCloseEditorOnMapCloseSetting = {
        "close_editor_on_map_close",
        "Close editor on map close",
        "Clear selection and close the marker editor when the world map closes",
        &g_modHubClient,
        &GetCloseEditorOnMapCloseSetting,
        &SetCloseEditorOnMapCloseSetting };

    static const EMC_BoolSettingDefV1 kShowHoverLabelsSetting = {
        "show_hover_labels",
        "Show hover labels",
        "Show marker type and label text when hovering a marker on the world map",
        &g_modHubClient,
        &GetShowHoverLabelsSetting,
        &SetShowHoverLabelsSetting };

    static const EMC_IntSettingDefV2 kDefaultMarkerTypeSetting = {
        "default_marker_type",
        "Default marker type",
        "0 Note | 1 Danger | 2 Stash | 3 Ruin | 4 Mine | 5 Base | 6 Trader | 7 Safe Spot | 8 Quest | 9 Todo",
        &g_modHubClient,
        0,
        9,
        1,
        { 1, 0, 0 },
        { 1, 0, 0 },
        &GetDefaultMarkerTypeSetting,
        &SetDefaultMarkerTypeSetting };

    static const emc::ModHubClientSettingRowV1 kModHubRows[] = {
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kEnabledSetting },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kCloseEditorOnMapCloseSetting },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kShowHoverLabelsSetting },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_INT_V2, &kDefaultMarkerTypeSetting }
    };

    static const emc::ModHubClientTableRegistrationV1 kModHubRegistration = {
        &kModHubDescriptor,
        kModHubRows,
        static_cast<uint32_t>(sizeof(kModHubRows) / sizeof(kModHubRows[0])) };

    emc::ModHubClient::Config config;
    config.table_registration = &kModHubRegistration;
    g_modHubClient.SetConfig(config);
    g_modHubClientConfigured = true;
}
}

void MapMarkersModHub_OnStartup()
{
    EnsureModHubClientConfigured();

    const emc::ModHubClient::AttemptResult result = g_modHubClient.OnStartup();
    if (result == emc::ModHubClient::ATTACH_SUCCESS)
    {
        LogModHubMessage("attached", 0);
        return;
    }

    if (result == emc::ModHubClient::ATTACH_FAILED)
    {
        LogModHubStartupMessage(false, "Mod Hub unavailable or inactive; continuing with file-only settings");
        LogModHubMessage("fallback", "get_api_failed");
        return;
    }

    if (result == emc::ModHubClient::REGISTRATION_FAILED)
    {
        LogModHubStartupMessage(true, "Mod Hub registration failed; continuing with file-only settings");
        LogModHubMessage("fallback", "register_mod_or_setting_failed");
        return;
    }

    LogModHubStartupMessage(true, "Mod Hub client configuration invalid; continuing with file-only settings");
    LogModHubMessage("fallback", "invalid_client_configuration");
}
