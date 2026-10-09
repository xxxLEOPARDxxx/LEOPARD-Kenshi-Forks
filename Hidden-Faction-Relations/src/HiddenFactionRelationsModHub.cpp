// ModConfigMenu: подписи настроек идут через Tr - перевод в locale/<язык>.
#define KLOC_DOMAIN "hidden_faction_relations"
#include <Localization.h>

#include "HiddenFactionRelationsModHub.h"

#include "HiddenFactionRelationsConfig.h"

#include <Debug.h>

#include "emc/mod_hub_client.h"
#define MCM_BRIDGE_OLD_SDK        // старый SDK: строки {kind, def}, без разделов
#include <McmModHubBridge.h>

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

EMC_Result __cdecl GetDebugLoggingSetting(void* user_data, int32_t* out_value)
{
    return GetHubBoolSetting(user_data, out_value, &HiddenFactionRelationsConfigSnapshot::debugLogging);
}

EMC_Result __cdecl SetDebugLoggingSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return SetHubBoolSetting(user_data, value, err_buf, err_buf_size, &HiddenFactionRelationsConfigSnapshot::debugLogging);
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

    // «Подробный журнал» - во всех модах (08.10.2026, leopard).
    static const EMC_BoolSettingDefV1 kDebugLoggingSetting = {
        "debug_logging",
        "Debug logging",
        "Write detailed logs to RE_Kenshi_log.txt",
        &g_modHubClient,
        &GetDebugLoggingSetting,
        &SetDebugLoggingSetting };

    static const emc::ModHubClientSettingRowV1 kModHubRows[] = {
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kAutoFocusSearchOnOpenSetting },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kDebugLoggingSetting },
        // Клавиша открытия (Ctrl+Alt+G) убрана 07.10.2026: список и так на
        // странице MCM, а клавиша у игроков не срабатывала.
    };

    static const emc::ModHubClientTableRegistrationV1 kModHubRegistration = {
        &kModHubDescriptor,
        kModHubRows,
        static_cast<uint32_t>(sizeof(kModHubRows) / sizeof(kModHubRows[0]))
    };

    emc::ModHubClient::Config config;
    config.table_registration = mcm_bridge::Capture(&kModHubRegistration);
    g_modHubClient.SetConfig(config);
    g_modHubClientConfigured = true;
}
}

void HiddenFactionRelationsModHub_OnStartup()
{
    EnsureModHubClientConfigured();

    // Mod Hub из сборки убран: настройки - во вкладке MCM (McmModHubBridge.h),
    // таблица к этому месту уже захвачена. К Mod Hub не подключаемся.
    return;
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

// Страница в ModConfigMenu (вкладка MCM в настройках игры) вместо Mod Hub.
#include "HiddenFactionRelationsPanel.h"

static void __cdecl McmAttachList(void*, void* parent) { HiddenFactionRelationsPanel_McmAttach(parent); }
static void __cdecl McmDetachList(void*) { HiddenFactionRelationsPanel_McmDetach(); }

// Настройки из таблицы - строками MCM; над ними (API v4) - сам список
// отношений со скрытыми фракциями.
extern "C" __declspec(dllexport) void MCM_Describe(MCM_Api* api)
{
    mcm_bridge::Describe(api, "Your relations toward the hidden factions, with search and sorting.");
    if (api != 0 && api->version >= 4)
        api->customArea(api, &McmAttachList, &McmDetachList, 0, 62);
}
