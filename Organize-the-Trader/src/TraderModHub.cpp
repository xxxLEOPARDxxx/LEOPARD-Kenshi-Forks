#include "TraderModHub.h"

#include "TraderCore.h"
#include "TraderSearchUi.h"
#include "emc/mod_hub_client.h"

#include <sstream>

namespace
{
const char* kHubNamespaceId = "emkej.qol";
const char* kHubNamespaceDisplayName = "Emkej QoL";
const char* kHubModId = "organize_the_trader";
const char* kHubModDisplayName = "Organize the Trader";

typedef bool TraderConfigSnapshot::*TraderConfigBoolField;
typedef int TraderConfigSnapshot::*TraderConfigIntField;

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

EMC_Result GetHubBoolSetting(void* user_data, int32_t* out_value, TraderConfigBoolField field)
{
    if (!IsValidHubUserData(user_data) || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const TraderConfigSnapshot config = CaptureTraderConfigSnapshot();
    *out_value = (config.*field) ? 1 : 0;
    return EMC_OK;
}

EMC_Result SetHubBoolSetting(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size,
    TraderConfigBoolField field)
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

    const TraderConfigSnapshot previous = CaptureTraderConfigSnapshot();
    TraderConfigSnapshot updated = previous;
    updated.*field = value != 0;
    NormalizeTraderConfigSnapshot(&updated);
    ApplyTraderConfigSnapshot(updated);

    if (!SaveTraderConfigSnapshot(updated))
    {
        ApplyTraderConfigSnapshot(previous);
        WriteHubErrorText(err_buf, err_buf_size, "persist_failed");
        return EMC_ERR_INTERNAL;
    }

    ApplyRuntimeSearchUiConfig();
    WriteHubErrorText(err_buf, err_buf_size, 0);
    return EMC_OK;
}

EMC_Result GetHubIntSetting(void* user_data, int32_t* out_value, TraderConfigIntField field)
{
    if (!IsValidHubUserData(user_data) || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const TraderConfigSnapshot config = CaptureTraderConfigSnapshot();
    *out_value = static_cast<int32_t>(config.*field);
    return EMC_OK;
}

EMC_Result SetHubIntSetting(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size,
    TraderConfigIntField field)
{
    if (!IsValidHubUserData(user_data))
    {
        WriteHubErrorText(err_buf, err_buf_size, "invalid_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const TraderConfigSnapshot previous = CaptureTraderConfigSnapshot();
    TraderConfigSnapshot updated = previous;
    updated.*field = static_cast<int>(value);
    NormalizeTraderConfigSnapshot(&updated);
    ApplyTraderConfigSnapshot(updated);

    if (!SaveTraderConfigSnapshot(updated))
    {
        ApplyTraderConfigSnapshot(previous);
        WriteHubErrorText(err_buf, err_buf_size, "persist_failed");
        return EMC_ERR_INTERNAL;
    }

    ApplyRuntimeSearchUiConfig();
    WriteHubErrorText(err_buf, err_buf_size, 0);
    return EMC_OK;
}

EMC_Result __cdecl GetEnabledSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(user_data, out_value, &TraderConfigSnapshot::enabled);
}

EMC_Result __cdecl SetEnabledSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return SetHubBoolSetting(
        user_data,
        value,
        err_buf,
        err_buf_size,
        &TraderConfigSnapshot::enabled);
}

EMC_Result __cdecl GetShowSearchEntryCountSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(user_data, out_value, &TraderConfigSnapshot::showSearchEntryCount);
}

EMC_Result __cdecl SetShowSearchEntryCountSetting(
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
        &TraderConfigSnapshot::showSearchEntryCount);
}

EMC_Result __cdecl GetShowSearchQuantityCountSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(user_data, out_value, &TraderConfigSnapshot::showSearchQuantityCount);
}

EMC_Result __cdecl SetShowSearchQuantityCountSetting(
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
        &TraderConfigSnapshot::showSearchQuantityCount);
}

EMC_Result __cdecl GetShowSearchClearButtonSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(user_data, out_value, &TraderConfigSnapshot::showSearchClearButton);
}

EMC_Result __cdecl SetShowSearchClearButtonSetting(
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
        &TraderConfigSnapshot::showSearchClearButton);
}

EMC_Result __cdecl GetAutoFocusSearchInputSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(user_data, out_value, &TraderConfigSnapshot::autoFocusSearchInput);
}

EMC_Result __cdecl SetAutoFocusSearchInputSetting(
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
        &TraderConfigSnapshot::autoFocusSearchInput);
}

EMC_Result __cdecl GetSearchInputWidthSetting(void* user_data, int32_t* out_value)
{
    return GetHubIntSetting(user_data, out_value, &TraderConfigSnapshot::searchInputWidth);
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
        &TraderConfigSnapshot::searchInputWidth);
}

EMC_Result __cdecl GetSearchInputHeightSetting(void* user_data, int32_t* out_value)
{
    return GetHubIntSetting(user_data, out_value, &TraderConfigSnapshot::searchInputHeight);
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
        &TraderConfigSnapshot::searchInputHeight);
}

EMC_Result __cdecl GetSortPanelWidthSetting(void* user_data, int32_t* out_value)
{
    return GetHubIntSetting(user_data, out_value, &TraderConfigSnapshot::sortPanelWidth);
}

EMC_Result __cdecl SetSortPanelWidthSetting(
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
        &TraderConfigSnapshot::sortPanelWidth);
}

EMC_Result __cdecl GetSortPanelHeightSetting(void* user_data, int32_t* out_value)
{
    return GetHubIntSetting(user_data, out_value, &TraderConfigSnapshot::sortPanelHeight);
}

EMC_Result __cdecl SetSortPanelHeightSetting(
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
        &TraderConfigSnapshot::sortPanelHeight);
}

void LogModHubFallback(const char* reason)
{
    std::stringstream line;
    line << "event=mod_hub_fallback"
         << " reason=" << (reason != 0 ? reason : "unknown")
         << " result=" << g_modHubClient.LastAttemptFailureResult()
         << " use_hub_ui=0";

    if (g_modHubClient.LastAttemptFailureResult() == EMC_ERR_NOT_FOUND)
    {
        LogDebugLine(line.str());
        return;
    }

    LogWarnLine(line.str());
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
        "Enable Organize the Trader search controls",
        &g_modHubClient,
        &GetEnabledSetting,
        &SetEnabledSetting };

    static const EMC_BoolSettingDefV1 kShowSearchEntryCountSetting = {
        "show_search_entry_count",
        "Show entry count",
        "Show visible and total entry counts in the search bar",
        &g_modHubClient,
        &GetShowSearchEntryCountSetting,
        &SetShowSearchEntryCountSetting };

    static const EMC_BoolSettingDefV1 kShowSearchQuantityCountSetting = {
        "show_search_quantity_count",
        "Show quantity count",
        "Show visible stack quantity in the search bar",
        &g_modHubClient,
        &GetShowSearchQuantityCountSetting,
        &SetShowSearchQuantityCountSetting };

    static const EMC_BoolSettingDefV1 kShowSearchClearButtonSetting = {
        "show_search_clear_button",
        "Show clear button",
        "Show the clear button inside the search bar",
        &g_modHubClient,
        &GetShowSearchClearButtonSetting,
        &SetShowSearchClearButtonSetting };

    static const EMC_BoolSettingDefV1 kAutoFocusSearchInputSetting = {
        "auto_focus_search_input",
        "Auto-focus search",
        "Focus the trader search input automatically when controls are injected",
        &g_modHubClient,
        &GetAutoFocusSearchInputSetting,
        &SetAutoFocusSearchInputSetting };

    static const EMC_IntSettingDefV1 kSearchInputWidthSetting = {
        "search_input_width",
        "Search input width",
        "Desired search input width in pixels",
        &g_modHubClient,
        static_cast<int32_t>(kSearchInputConfiguredWidthMin),
        static_cast<int32_t>(kSearchInputConfiguredWidthMax),
        1,
        &GetSearchInputWidthSetting,
        &SetSearchInputWidthSetting };

    static const EMC_IntSettingDefV1 kSearchInputHeightSetting = {
        "search_input_height",
        "Search input height",
        "Desired search input height in pixels",
        &g_modHubClient,
        static_cast<int32_t>(kSearchInputConfiguredHeightMin),
        static_cast<int32_t>(kSearchInputConfiguredHeightMax),
        1,
        &GetSearchInputHeightSetting,
        &SetSearchInputHeightSetting };

    static const EMC_IntSettingDefV1 kSortPanelWidthSetting = {
        "sort_panel_width",
        "Sort panel width",
        "Desired sort panel width in pixels",
        &g_modHubClient,
        static_cast<int32_t>(kSortPanelConfiguredWidthMin),
        static_cast<int32_t>(kSortPanelConfiguredWidthMax),
        1,
        &GetSortPanelWidthSetting,
        &SetSortPanelWidthSetting };

    static const EMC_IntSettingDefV1 kSortPanelHeightSetting = {
        "sort_panel_height",
        "Sort panel height",
        "Desired sort panel height in pixels",
        &g_modHubClient,
        static_cast<int32_t>(kSortPanelConfiguredHeightMin),
        static_cast<int32_t>(kSortPanelConfiguredHeightMax),
        1,
        &GetSortPanelHeightSetting,
        &SetSortPanelHeightSetting };

    static const emc::ModHubClientSettingRowV1 kModHubRows[] = {
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kEnabledSetting },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kShowSearchEntryCountSetting },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kShowSearchQuantityCountSetting },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kShowSearchClearButtonSetting },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kAutoFocusSearchInputSetting },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &kSearchInputWidthSetting },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &kSearchInputHeightSetting },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &kSortPanelWidthSetting },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &kSortPanelHeightSetting }
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

void TraderModHub_OnStartup()
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

        LogModHubFallback("get_api_failed");
        return;
    }

    if (result == emc::ModHubClient::REGISTRATION_FAILED)
    {
        LogModHubFallback("register_mod_or_setting_failed");
        return;
    }

    LogModHubFallback("invalid_client_configuration");
}
