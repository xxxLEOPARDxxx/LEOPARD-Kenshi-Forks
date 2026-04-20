#include "InventoryModHub.h"

#include "InventoryConfig.h"
#include "InventoryCore.h"
#include "emc/mod_hub_client.h"

#include <sstream>

namespace
{
const char* kHubNamespaceId = "emkej.qol";
const char* kHubNamespaceDisplayName = "Emkej QoL";
const char* kHubModId = "organize_the_inventory";
const char* kHubModDisplayName = "Organize the Inventory";
const char* kHubSectionAdvancedId = "advanced";
const char* kHubSectionAdvancedLabel = "Advanced";

typedef bool InventoryConfigSnapshot::*InventoryConfigBoolField;
typedef int InventoryConfigSnapshot::*InventoryConfigIntField;

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

EMC_Result GetHubBoolSetting(void* user_data, int32_t* out_value, InventoryConfigBoolField field)
{
    if (!IsValidHubUserData(user_data) || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const InventoryConfigSnapshot config = CaptureInventoryConfigSnapshot();
    *out_value = (config.*field) ? 1 : 0;
    return EMC_OK;
}

EMC_Result SetHubBoolSetting(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size,
    InventoryConfigBoolField field)
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

    const InventoryConfigSnapshot previous = CaptureInventoryConfigSnapshot();
    InventoryConfigSnapshot updated = previous;
    updated.*field = value != 0;
    NormalizeInventoryConfigSnapshot(&updated);
    ApplyInventoryConfigSnapshot(updated);

    if (!SaveInventoryConfigSnapshot(updated))
    {
        ApplyInventoryConfigSnapshot(previous);
        WriteHubErrorText(err_buf, err_buf_size, "persist_failed");
        return EMC_ERR_INTERNAL;
    }

    WriteHubErrorText(err_buf, err_buf_size, 0);
    return EMC_OK;
}

EMC_Result GetHubIntSetting(void* user_data, int32_t* out_value, InventoryConfigIntField field)
{
    if (!IsValidHubUserData(user_data) || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const InventoryConfigSnapshot config = CaptureInventoryConfigSnapshot();
    *out_value = static_cast<int32_t>(config.*field);
    return EMC_OK;
}

EMC_Result SetHubIntSetting(
    void* user_data,
    int32_t value,
    char* err_buf,
    uint32_t err_buf_size,
    InventoryConfigIntField field)
{
    if (!IsValidHubUserData(user_data))
    {
        WriteHubErrorText(err_buf, err_buf_size, "invalid_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const InventoryConfigSnapshot previous = CaptureInventoryConfigSnapshot();
    InventoryConfigSnapshot updated = previous;
    updated.*field = static_cast<int>(value);
    NormalizeInventoryConfigSnapshot(&updated);
    ApplyInventoryConfigSnapshot(updated);

    if (!SaveInventoryConfigSnapshot(updated))
    {
        ApplyInventoryConfigSnapshot(previous);
        WriteHubErrorText(err_buf, err_buf_size, "persist_failed");
        return EMC_ERR_INTERNAL;
    }

    WriteHubErrorText(err_buf, err_buf_size, 0);
    return EMC_OK;
}

EMC_Result __cdecl GetEnabledSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(user_data, out_value, &InventoryConfigSnapshot::enabled);
}

EMC_Result __cdecl SetEnabledSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return SetHubBoolSetting(
        user_data,
        value,
        err_buf,
        err_buf_size,
        &InventoryConfigSnapshot::enabled);
}

EMC_Result __cdecl GetCreatureSearchEnabledSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(user_data, out_value, &InventoryConfigSnapshot::creatureSearchEnabled);
}

EMC_Result __cdecl SetCreatureSearchEnabledSetting(
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
        &InventoryConfigSnapshot::creatureSearchEnabled);
}

EMC_Result __cdecl GetShowSearchEntryCountSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(
        user_data,
        out_value,
        &InventoryConfigSnapshot::showSearchEntryCount);
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
        &InventoryConfigSnapshot::showSearchEntryCount);
}

EMC_Result __cdecl GetShowSearchQuantityCountSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(
        user_data,
        out_value,
        &InventoryConfigSnapshot::showSearchQuantityCount);
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
        &InventoryConfigSnapshot::showSearchQuantityCount);
}

EMC_Result __cdecl GetShowSearchClearButtonSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(
        user_data,
        out_value,
        &InventoryConfigSnapshot::showSearchClearButton);
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
        &InventoryConfigSnapshot::showSearchClearButton);
}

EMC_Result __cdecl GetAutoFocusSearchInputSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(
        user_data,
        out_value,
        &InventoryConfigSnapshot::autoFocusSearchInput);
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
        &InventoryConfigSnapshot::autoFocusSearchInput);
}

EMC_Result __cdecl GetDebugLoggingSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(user_data, out_value, &InventoryConfigSnapshot::debugLogging);
}

EMC_Result __cdecl SetDebugLoggingSetting(
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
        &InventoryConfigSnapshot::debugLogging);
}

EMC_Result __cdecl GetDebugSearchLoggingSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(user_data, out_value, &InventoryConfigSnapshot::debugSearchLogging);
}

EMC_Result __cdecl SetDebugSearchLoggingSetting(
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
        &InventoryConfigSnapshot::debugSearchLogging);
}

EMC_Result __cdecl GetDebugBindingLoggingSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(user_data, out_value, &InventoryConfigSnapshot::debugBindingLogging);
}

EMC_Result __cdecl SetDebugBindingLoggingSetting(
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
        &InventoryConfigSnapshot::debugBindingLogging);
}

EMC_Result __cdecl GetEnableDebugProbesSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(user_data, out_value, &InventoryConfigSnapshot::enableDebugProbes);
}

EMC_Result __cdecl SetEnableDebugProbesSetting(
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
        &InventoryConfigSnapshot::enableDebugProbes);
}

EMC_Result __cdecl GetSearchInputWidthSetting(void* user_data, int32_t* out_value)
{
    return GetHubIntSetting(user_data, out_value, &InventoryConfigSnapshot::searchInputWidth);
}

EMC_Result __cdecl GetSearchBarWidthSetting(void* user_data, int32_t* out_value)
{
    return GetHubIntSetting(user_data, out_value, &InventoryConfigSnapshot::searchBarWidth);
}

EMC_Result __cdecl SetSearchBarWidthSetting(
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
        &InventoryConfigSnapshot::searchBarWidth);
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
        &InventoryConfigSnapshot::searchInputWidth);
}

EMC_Result __cdecl GetSearchInputHeightSetting(void* user_data, int32_t* out_value)
{
    return GetHubIntSetting(user_data, out_value, &InventoryConfigSnapshot::searchInputHeight);
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
        &InventoryConfigSnapshot::searchInputHeight);
}

EMC_Result __cdecl GetCreatureSearchInputWidthSetting(void* user_data, int32_t* out_value)
{
    return GetHubIntSetting(
        user_data,
        out_value,
        &InventoryConfigSnapshot::creatureSearchInputWidth);
}

EMC_Result __cdecl SetCreatureSearchInputWidthSetting(
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
        &InventoryConfigSnapshot::creatureSearchInputWidth);
}

EMC_Result __cdecl GetCreatureSearchBarWidthSetting(void* user_data, int32_t* out_value)
{
    return GetHubIntSetting(
        user_data,
        out_value,
        &InventoryConfigSnapshot::creatureSearchBarWidth);
}

EMC_Result __cdecl SetCreatureSearchBarWidthSetting(
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
        &InventoryConfigSnapshot::creatureSearchBarWidth);
}

EMC_Result __cdecl GetCreatureSearchInputHeightSetting(void* user_data, int32_t* out_value)
{
    return GetHubIntSetting(
        user_data,
        out_value,
        &InventoryConfigSnapshot::creatureSearchInputHeight);
}

EMC_Result __cdecl SetCreatureSearchInputHeightSetting(
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
        &InventoryConfigSnapshot::creatureSearchInputHeight);
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

void LogModHubRegistrationFailureTrace()
{
    if (!g_modHubClient.HasLastRegistrationFailureTrace())
    {
        return;
    }

    std::stringstream line;
    line << "event=mod_hub_registration_failure"
         << " stage=" << (g_modHubClient.LastRegistrationFailureStage() != 0
                 ? g_modHubClient.LastRegistrationFailureStage()
                 : "unknown")
         << " row_index=" << g_modHubClient.LastRegistrationFailureRowIndex()
         << " setting_id=" << (g_modHubClient.LastRegistrationFailureSettingId() != 0
                 ? g_modHubClient.LastRegistrationFailureSettingId()
                 : "null")
         << " section_id=" << (g_modHubClient.LastRegistrationFailureSectionId() != 0
                 ? g_modHubClient.LastRegistrationFailureSectionId()
                 : "null")
         << " visible_when_setting_id=" << (g_modHubClient.LastRegistrationFailureVisibleWhenSettingId() != 0
                 ? g_modHubClient.LastRegistrationFailureVisibleWhenSettingId()
                 : "null")
         << " result=" << g_modHubClient.LastRegistrationFailureTraceResult();
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
        "Enable Organize the Inventory search controls",
        &g_modHubClient,
        &GetEnabledSetting,
        &SetEnabledSetting };

    static const EMC_BoolSettingDefV1 kCreatureSearchEnabledSetting = {
        "creature_search_enabled",
        "Creature search",
        "Show search controls when the active target is a creature or pack animal inventory",
        &g_modHubClient,
        &GetCreatureSearchEnabledSetting,
        &SetCreatureSearchEnabledSetting };

    static const EMC_BoolSettingDefV1 kShowSearchEntryCountSetting = {
        "show_search_entry_count",
        "Show entry count",
        "Show visible and total entry counts in the inventory search bar",
        &g_modHubClient,
        &GetShowSearchEntryCountSetting,
        &SetShowSearchEntryCountSetting };

    static const EMC_BoolSettingDefV1 kShowSearchQuantityCountSetting = {
        "show_search_quantity_count",
        "Show quantity count",
        "Show visible stack quantity in the inventory search bar",
        &g_modHubClient,
        &GetShowSearchQuantityCountSetting,
        &SetShowSearchQuantityCountSetting };

    static const EMC_BoolSettingDefV1 kShowSearchClearButtonSetting = {
        "show_search_clear_button",
        "Show clear button",
        "Show the clear button inside the inventory search bar",
        &g_modHubClient,
        &GetShowSearchClearButtonSetting,
        &SetShowSearchClearButtonSetting };

    static const EMC_BoolSettingDefV1 kAutoFocusSearchInputSetting = {
        "auto_focus_search_input",
        "Auto-focus search",
        "Focus the inventory search input automatically when controls are injected",
        &g_modHubClient,
        &GetAutoFocusSearchInputSetting,
        &SetAutoFocusSearchInputSetting };

    static const EMC_BoolSettingDefV1 kDebugLoggingSetting = {
        "debug_logging",
        "Debug logging",
        "Enable baseline debug logging for inventory UI attachment",
        &g_modHubClient,
        &GetDebugLoggingSetting,
        &SetDebugLoggingSetting };

    static const EMC_BoolSettingDefV1 kDebugSearchLoggingSetting = {
        "debug_search_logging",
        "Debug search logging",
        "Enable search-specific debug logging when debug logging is enabled",
        &g_modHubClient,
        &GetDebugSearchLoggingSetting,
        &SetDebugSearchLoggingSetting };

    static const EMC_BoolSettingDefV1 kDebugBindingLoggingSetting = {
        "debug_binding_logging",
        "Debug binding logging",
        "Enable inventory binding diagnostics when debug logging is enabled",
        &g_modHubClient,
        &GetDebugBindingLoggingSetting,
        &SetDebugBindingLoggingSetting };

    static const EMC_BoolSettingDefV1 kEnableDebugProbesSetting = {
        "enable_debug_probes",
        "Enable debug probes",
        "Enable extra inventory UI probe diagnostics and hot-path dumps",
        &g_modHubClient,
        &GetEnableDebugProbesSetting,
        &SetEnableDebugProbesSetting };

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

    static const EMC_IntSettingDefV1 kSearchBarWidthSetting = {
        "search_bar_width",
        "Search bar width",
        "Desired total search bar width in pixels",
        &g_modHubClient,
        static_cast<int32_t>(kSearchBarConfiguredWidthMin),
        static_cast<int32_t>(kSearchBarConfiguredWidthMax),
        1,
        &GetSearchBarWidthSetting,
        &SetSearchBarWidthSetting };

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

    static const EMC_IntSettingDefV1 kCreatureSearchInputWidthSetting = {
        "creature_search_input_width",
        "Creature search input width",
        "Desired creature search input width in pixels",
        &g_modHubClient,
        static_cast<int32_t>(kSearchInputConfiguredWidthMin),
        static_cast<int32_t>(kSearchInputConfiguredWidthMax),
        1,
        &GetCreatureSearchInputWidthSetting,
        &SetCreatureSearchInputWidthSetting };

    static const EMC_IntSettingDefV1 kCreatureSearchBarWidthSetting = {
        "creature_search_bar_width",
        "Creature search bar width",
        "Desired total creature search bar width in pixels",
        &g_modHubClient,
        static_cast<int32_t>(kSearchBarConfiguredWidthMin),
        static_cast<int32_t>(kSearchBarConfiguredWidthMax),
        1,
        &GetCreatureSearchBarWidthSetting,
        &SetCreatureSearchBarWidthSetting };

    static const EMC_IntSettingDefV1 kCreatureSearchInputHeightSetting = {
        "creature_search_input_height",
        "Creature search input height",
        "Desired creature search input height in pixels",
        &g_modHubClient,
        static_cast<int32_t>(kSearchInputConfiguredHeightMin),
        static_cast<int32_t>(kSearchInputConfiguredHeightMax),
        1,
        &GetCreatureSearchInputHeightSetting,
        &SetCreatureSearchInputHeightSetting };

    static const emc::ModHubClientSettingRowV2 kModHubRows[] = {
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, "enabled", &kEnabledSetting, 0, 0, 0, 1 },
        {
            emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL,
            "creature_search_enabled",
            &kCreatureSearchEnabledSetting,
            0,
            0,
            0,
            1
        },
        {
            emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL,
            "show_search_entry_count",
            &kShowSearchEntryCountSetting,
            0,
            0,
            0,
            1
        },
        {
            emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL,
            "show_search_quantity_count",
            &kShowSearchQuantityCountSetting,
            0,
            0,
            0,
            1
        },
        {
            emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL,
            "show_search_clear_button",
            &kShowSearchClearButtonSetting,
            0,
            0,
            0,
            1
        },
        {
            emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL,
            "auto_focus_search_input",
            &kAutoFocusSearchInputSetting,
            0,
            0,
            0,
            1
        },
        {
            emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL,
            "debug_logging",
            &kDebugLoggingSetting,
            kHubSectionAdvancedId,
            kHubSectionAdvancedLabel,
            0,
            1
        },
        {
            emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL,
            "debug_search_logging",
            &kDebugSearchLoggingSetting,
            kHubSectionAdvancedId,
            kHubSectionAdvancedLabel,
            0,
            1
        },
        {
            emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL,
            "debug_binding_logging",
            &kDebugBindingLoggingSetting,
            kHubSectionAdvancedId,
            kHubSectionAdvancedLabel,
            0,
            1
        },
        {
            emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL,
            "enable_debug_probes",
            &kEnableDebugProbesSetting,
            kHubSectionAdvancedId,
            kHubSectionAdvancedLabel,
            0,
            1
        },
        {
            emc::MOD_HUB_CLIENT_SETTING_KIND_INT,
            "search_bar_width",
            &kSearchBarWidthSetting,
            0,
            0,
            0,
            1
        },
        {
            emc::MOD_HUB_CLIENT_SETTING_KIND_INT,
            "search_input_width",
            &kSearchInputWidthSetting,
            0,
            0,
            0,
            1
        },
        {
            emc::MOD_HUB_CLIENT_SETTING_KIND_INT,
            "search_input_height",
            &kSearchInputHeightSetting,
            0,
            0,
            0,
            1
        },
        {
            emc::MOD_HUB_CLIENT_SETTING_KIND_INT,
            "creature_search_bar_width",
            &kCreatureSearchBarWidthSetting,
            0,
            0,
            "creature_search_enabled",
            1
        },
        {
            emc::MOD_HUB_CLIENT_SETTING_KIND_INT,
            "creature_search_input_width",
            &kCreatureSearchInputWidthSetting,
            0,
            0,
            "creature_search_enabled",
            1
        },
        {
            emc::MOD_HUB_CLIENT_SETTING_KIND_INT,
            "creature_search_input_height",
            &kCreatureSearchInputHeightSetting,
            0,
            0,
            "creature_search_enabled",
            1
        }
    };

    static const emc::ModHubClientTableRegistrationV2 kModHubRegistration = {
        &kModHubDescriptor,
        kModHubRows,
        static_cast<uint32_t>(sizeof(kModHubRows) / sizeof(kModHubRows[0])) };

    emc::ModHubClient::Config config;
    config.table_registration_v2 = &kModHubRegistration;
    g_modHubClient.SetConfig(config);
    g_modHubClientConfigured = true;
}
}

void InventoryModHub_OnStartup()
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
        LogModHubRegistrationFailureTrace();
        LogModHubFallback("register_mod_or_setting_failed");
        return;
    }

    LogModHubFallback("invalid_client_configuration");
}
