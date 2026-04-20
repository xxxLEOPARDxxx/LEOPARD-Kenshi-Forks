#ifndef EMC_MOD_HUB_CLIENT_H
#define EMC_MOD_HUB_CLIENT_H

#include "emc/mod_hub_api.h"

namespace emc
{
typedef EMC_Result(__cdecl* ModHubClientGetApiFn)(
    uint32_t requested_version,
    uint32_t caller_api_size,
    const EMC_HubApiV1** out_api,
    uint32_t* out_api_size);

typedef EMC_Result(__cdecl* ModHubClientRegisterFn)(const EMC_HubApiV1* api, void* user_data);

typedef bool(__cdecl* ModHubClientForceAttachFailureFn)(
    void* user_data,
    bool is_retry,
    EMC_Result* out_result);

enum ModHubClientSettingKind
{
    MOD_HUB_CLIENT_SETTING_KIND_BOOL = 0,
    MOD_HUB_CLIENT_SETTING_KIND_KEYBIND = 1,
    MOD_HUB_CLIENT_SETTING_KIND_INT = 2,
    MOD_HUB_CLIENT_SETTING_KIND_FLOAT = 3,
    MOD_HUB_CLIENT_SETTING_KIND_ACTION = 4,
    MOD_HUB_CLIENT_SETTING_KIND_INT_V2 = 5,
    MOD_HUB_CLIENT_SETTING_KIND_SELECT = 6,
    MOD_HUB_CLIENT_SETTING_KIND_TEXT = 7,
    MOD_HUB_CLIENT_SETTING_KIND_COLOR = 8,
    MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2 = 9,
    MOD_HUB_CLIENT_SETTING_KIND_KEYBIND_V2 = 10,
    MOD_HUB_CLIENT_SETTING_KIND_SELECT_V2 = 11,
    MOD_HUB_CLIENT_SETTING_KIND_TEXT_V2 = 12,
    MOD_HUB_CLIENT_SETTING_KIND_ACTION_V2 = 13
};

struct ModHubClientSettingRowV1
{
    int32_t kind;
    const char* setting_id;
    const void* def;
    const char* section_id;
    const char* section_display_name;
};

struct ModHubClientTableRegistrationV1
{
    const EMC_ModDescriptorV1* mod_desc;
    const ModHubClientSettingRowV1* rows;
    uint32_t row_count;
    const EMC_BoolConditionRuleDefV1* bool_condition_rules;
    uint32_t bool_condition_rule_count;
};

EMC_Result RegisterSettingsTableV1(
    const EMC_HubApiV1* api,
    const ModHubClientTableRegistrationV1* table_registration);

EMC_Result RegisterSettingsTableWithApiSizeV1(
    const EMC_HubApiV1* api,
    uint32_t api_size,
    const ModHubClientTableRegistrationV1* table_registration);

EMC_Result RegisterBoolConditionRuleV1(
    const EMC_HubApiV1* api,
    EMC_ModHandle mod,
    const EMC_BoolConditionRuleDefV1* def);

EMC_Result RegisterBoolConditionRuleWithApiSizeV1(
    const EMC_HubApiV1* api,
    uint32_t api_size,
    EMC_ModHandle mod,
    const EMC_BoolConditionRuleDefV1* def);

class ModHubClient
{
public:
    struct Config
    {
        ModHubClientGetApiFn get_api_fn;
        ModHubClientRegisterFn register_fn;
        void* register_user_data;
        const ModHubClientTableRegistrationV1* table_registration;
        ModHubClientForceAttachFailureFn should_force_attach_failure_fn;
        void* attach_failure_user_data;
        uint32_t expected_sdk_api_version;
        uint32_t expected_sdk_min_api_size;

        Config();
    };

    enum AttemptResult
    {
        ATTACH_SUCCESS = 0,
        ATTACH_FAILED = 1,
        REGISTRATION_FAILED = 2,
        INVALID_CONFIGURATION = 3
    };

    ModHubClient();
    explicit ModHubClient(const Config& config);
    ~ModHubClient();

    void SetConfig(const Config& config);
    const Config& GetConfig() const;

    void Reset();

    AttemptResult OnStartup();
    AttemptResult OnOptionsWindowInit();
    bool UseHubUi() const;
    bool IsAttachRetryPending() const;
    bool HasAttachRetryAttempted() const;
    EMC_Result LastAttemptFailureResult() const;

private:
    void RegisterOptionsWindowInitObserverIfAvailable(const EMC_HubApiV1* api, uint32_t api_size);
    void UnregisterOptionsWindowInitObserverIfNeeded();
    static void __cdecl OnOptionsWindowInitObserverThunk(void* user_data);

    AttemptResult AttemptAttachAndRegister(bool is_retry);

    Config config_;
    bool use_hub_ui_;
    bool attach_retry_pending_;
    bool attach_retry_attempted_;
    EMC_Result last_attempt_failure_result_;
    const EMC_HubApiV1* observer_api_;
    bool options_window_init_observer_registered_;
    bool sdk_stamp_warning_emitted_;
};
}

#endif
