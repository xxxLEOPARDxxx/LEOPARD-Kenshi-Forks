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
    MOD_HUB_CLIENT_SETTING_KIND_ACTION = 4
};

struct ModHubClientSettingRowV1
{
    int32_t kind;
    const void* def;
};

struct ModHubClientTableRegistrationV1
{
    const EMC_ModDescriptorV1* mod_desc;
    const ModHubClientSettingRowV1* rows;
    uint32_t row_count;
};

EMC_Result RegisterSettingsTableV1(
    const EMC_HubApiV1* api,
    const ModHubClientTableRegistrationV1* table_registration);

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
};
}

#endif
