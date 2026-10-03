#include <McmModHubBridge.h>
static void WriteRuntimeApiError(char* err_buf, uint32_t err_buf_size, const char* text)
{
    if (!err_buf || err_buf_size == 0u || !text)
    {
        return;
    }

    const size_t copyLen = static_cast<size_t>(err_buf_size - 1u);
    std::strncpy(err_buf, text, copyLen);
    err_buf[copyLen] = '\0';
}

static bool IsHubUserDataValid(void* user_data)
{
    return user_data == &g_modHubClient;
}

static EMC_Result HubGetBoolSetting(void* user_data, bool value, int32_t* out_value)
{
    if (!IsHubUserDataValid(user_data) || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = value ? 1 : 0;
    return EMC_OK;
}

static EMC_Result HubSetBoolSetting(void* user_data, int32_t value, bool* target, char* err_buf, uint32_t err_buf_size)
{
    if (!IsHubUserDataValid(user_data) || target == 0)
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value != 0 && value != 1)
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "invalid_bool");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const bool previous_value = *target;
    *target = (value != 0);
    if (!SaveConfigState())
    {
        *target = previous_value;
        WriteRuntimeApiError(err_buf, err_buf_size, "persist_failed");
        return EMC_ERR_INTERNAL;
    }

    return EMC_OK;
}

static EMC_Result __cdecl HubGetEnabledSetting(void* user_data, int32_t* out_value)
{
    return HubGetBoolSetting(user_data, g_modEnabled, out_value);
}

static EMC_Result __cdecl HubSetEnabledSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return HubSetBoolSetting(user_data, value, &g_modEnabled, err_buf, err_buf_size);
}

static EMC_Result __cdecl HubGetSleepingBagEnabledSetting(void* user_data, int32_t* out_value)
{
    return HubGetBoolSetting(user_data, g_sleepingBagDismantleEnabled, out_value);
}

static EMC_Result __cdecl HubSetSleepingBagEnabledSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return HubSetBoolSetting(user_data, value, &g_sleepingBagDismantleEnabled, err_buf, err_buf_size);
}

static EMC_Result __cdecl HubGetAnyOwnBuildingSetting(void* user_data, int32_t* out_value)
{
    return HubGetBoolSetting(user_data, g_dismantleAnyOwnBuilding, out_value);
}

static EMC_Result __cdecl HubSetAnyOwnBuildingSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return HubSetBoolSetting(user_data, value, &g_dismantleAnyOwnBuilding, err_buf, err_buf_size);
}

static EMC_Result __cdecl HubGetDebugLoggingSetting(void* user_data, int32_t* out_value)
{
    return HubGetBoolSetting(user_data, g_debugLogging, out_value);
}

static EMC_Result __cdecl HubSetDebugLoggingSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return HubSetBoolSetting(user_data, value, &g_debugLogging, err_buf, err_buf_size);
}

static EMC_Result __cdecl HubGetHotkeyRequireCtrlSetting(void* user_data, int32_t* out_value)
{
    return HubGetBoolSetting(user_data, g_hotkeyRequireCtrl, out_value);
}

static EMC_Result __cdecl HubSetHotkeyRequireCtrlSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    const EMC_Result result = HubSetBoolSetting(user_data, value, &g_hotkeyRequireCtrl, err_buf, err_buf_size);
    if (result == EMC_OK)
    {
        RefreshHotkeyUiWidgets();
    }
    return result;
}

static EMC_Result __cdecl HubGetHotkeyRequireShiftSetting(void* user_data, int32_t* out_value)
{
    return HubGetBoolSetting(user_data, g_hotkeyRequireShift, out_value);
}

static EMC_Result __cdecl HubSetHotkeyRequireShiftSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    const EMC_Result result = HubSetBoolSetting(user_data, value, &g_hotkeyRequireShift, err_buf, err_buf_size);
    if (result == EMC_OK)
    {
        RefreshHotkeyUiWidgets();
    }
    return result;
}

static EMC_Result __cdecl HubGetHotkeyRequireAltSetting(void* user_data, int32_t* out_value)
{
    return HubGetBoolSetting(user_data, g_hotkeyRequireAlt, out_value);
}

static EMC_Result __cdecl HubSetHotkeyRequireAltSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    const EMC_Result result = HubSetBoolSetting(user_data, value, &g_hotkeyRequireAlt, err_buf, err_buf_size);
    if (result == EMC_OK)
    {
        RefreshHotkeyUiWidgets();
    }
    return result;
}

static const char* kHubSectionAdvancedId = "advanced";
static const char* kHubSectionAdvancedLabel = "Advanced";
static const EMC_SettingSectionDefV1 kDebugLoggingSectionDef = {
    "debug_logging",
    kHubSectionAdvancedId,
    kHubSectionAdvancedLabel };

static uint32_t GetCurrentHotkeyModifierBits()
{
    uint32_t modifiers = 0u;
    if (g_hotkeyRequireCtrl)
    {
        modifiers |= kHotkeyModifierCtrlMask;
    }
    if (g_hotkeyRequireShift)
    {
        modifiers |= kHotkeyModifierShiftMask;
    }
    if (g_hotkeyRequireAlt)
    {
        modifiers |= kHotkeyModifierAltMask;
    }
    return modifiers;
}

static EMC_Result __cdecl HubGetDismantleHotkeySetting(void* user_data, EMC_KeybindValueV1* out_value)
{
    if (!IsHubUserDataValid(user_data) || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EnsureRuntimeHotkeyValid();
    out_value->keycode = static_cast<int32_t>(g_hotkeyPrimary);
    out_value->modifiers = GetCurrentHotkeyModifierBits();
    return EMC_OK;
}

static EMC_Result __cdecl HubSetDismantleHotkeySetting(
    void* user_data,
    EMC_KeybindValueV1 value,
    char* err_buf,
    uint32_t err_buf_size)
{
    if (!IsHubUserDataValid(user_data))
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const uint32_t supportedModifierMask = kHotkeyModifierCtrlMask | kHotkeyModifierShiftMask | kHotkeyModifierAltMask;
    if ((value.modifiers & ~supportedModifierMask) != 0u)
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "invalid_modifiers");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const OIS::KeyCode requestedHotkey = static_cast<OIS::KeyCode>(value.keycode);
    std::string validationReason;
    if (ValidateHotkey(requestedHotkey, &validationReason) != HotkeyValidation_Ok)
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "invalid_hotkey");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const OIS::KeyCode previous_hotkey = g_hotkeyPrimary;
    const bool previous_hotkey_require_ctrl = g_hotkeyRequireCtrl;
    const bool previous_hotkey_require_shift = g_hotkeyRequireShift;
    const bool previous_hotkey_require_alt = g_hotkeyRequireAlt;
    g_hotkeyPrimary = requestedHotkey;
    g_pendingHotkeyPrimary = requestedHotkey;
    g_hotkeyRequireCtrl = (value.modifiers & kHotkeyModifierCtrlMask) != 0u;
    g_hotkeyRequireShift = (value.modifiers & kHotkeyModifierShiftMask) != 0u;
    g_hotkeyRequireAlt = (value.modifiers & kHotkeyModifierAltMask) != 0u;
    SyncNativeBindingFromHotkey();
    RefreshHotkeyUiWidgets();

    if (!SaveConfigState())
    {
        g_hotkeyPrimary = previous_hotkey;
        g_pendingHotkeyPrimary = previous_hotkey;
        g_hotkeyRequireCtrl = previous_hotkey_require_ctrl;
        g_hotkeyRequireShift = previous_hotkey_require_shift;
        g_hotkeyRequireAlt = previous_hotkey_require_alt;
        SyncNativeBindingFromHotkey();
        RefreshHotkeyUiWidgets();
        WriteRuntimeApiError(err_buf, err_buf_size, "persist_failed");
        return EMC_ERR_INTERNAL;
    }

    return EMC_OK;
}

static EMC_Result __cdecl HubResetHotkeyDefaultAction(void* user_data, char* err_buf, uint32_t err_buf_size)
{
    if (!IsHubUserDataValid(user_data))
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const OIS::KeyCode previous_hotkey = g_hotkeyPrimary;
    const bool previous_hotkey_require_ctrl = g_hotkeyRequireCtrl;
    const bool previous_hotkey_require_shift = g_hotkeyRequireShift;
    const bool previous_hotkey_require_alt = g_hotkeyRequireAlt;
    g_hotkeyPrimary = kDefaultHotkey;
    g_pendingHotkeyPrimary = kDefaultHotkey;
    g_hotkeyRequireCtrl = kDefaultHotkeyRequireCtrl;
    g_hotkeyRequireShift = kDefaultHotkeyRequireShift;
    g_hotkeyRequireAlt = kDefaultHotkeyRequireAlt;
    SyncNativeBindingFromHotkey();
    RefreshHotkeyUiWidgets();

    if (!SaveConfigState())
    {
        g_hotkeyPrimary = previous_hotkey;
        g_pendingHotkeyPrimary = previous_hotkey;
        g_hotkeyRequireCtrl = previous_hotkey_require_ctrl;
        g_hotkeyRequireShift = previous_hotkey_require_shift;
        g_hotkeyRequireAlt = previous_hotkey_require_alt;
        SyncNativeBindingFromHotkey();
        RefreshHotkeyUiWidgets();
        WriteRuntimeApiError(err_buf, err_buf_size, "persist_failed");
        return EMC_ERR_INTERNAL;
    }

    return EMC_OK;
}

static const emc::ModHubClientTableRegistrationV1* GetModHubTableRegistration()
{
    static const EMC_ModDescriptorV1 kModDescriptor = {
        kHubNamespaceId,
        kHubNamespaceDisplayName,
        kHubModId,
        kHubModDisplayName,
        &g_modHubClient };

    static const EMC_BoolSettingDefV1 kEnabledSettingDef = {
        kHubSettingEnabledId,
        "Enabled",
        "Enable Wall-B-Gone features",
        &g_modHubClient,
        &HubGetEnabledSetting,
        &HubSetEnabledSetting };

    static const EMC_BoolSettingDefV1 kSleepingBagEnabledSettingDef = {
        kHubSettingSleepingBagEnabledId,
        "Allow bed and furniture dismantle",
        "Allow dismantle behavior for beds and common furniture",
        &g_modHubClient,
        &HubGetSleepingBagEnabledSetting,
        &HubSetSleepingBagEnabledSetting };

    static const EMC_BoolSettingDefV1 kAnyOwnBuildingSettingDef = {
        kHubSettingAnyOwnBuildingId,
        "Dismantle any own building",
        "Dismantle any selected building of yours, not only walls and furniture; storage must be empty and nobody inside",
        &g_modHubClient,
        &HubGetAnyOwnBuildingSetting,
        &HubSetAnyOwnBuildingSetting };

    static const EMC_BoolSettingDefV1 kDebugLoggingSettingDef = {
        "debug_logging",
        "Debug logging",
        "Write extra runtime diagnostics to the RE_Kenshi log",
        &g_modHubClient,
        &HubGetDebugLoggingSetting,
        &HubSetDebugLoggingSetting };

    static const EMC_KeybindSettingDefV2 kHotkeySettingDef = {
        kHubSettingHotkeyId,
        "Dismantle hotkey",
        "Primary key used to dismantle the selected wall, bed, or furniture. Hold Ctrl, Shift, or Alt while capturing to store a combo.",
        &g_modHubClient,
        &HubGetDismantleHotkeySetting,
        &HubSetDismantleHotkeySetting,
        "Capture the dismantle shortcut with optional Ctrl/Shift/Alt modifiers." };

    static const EMC_ActionRowDefV2 kResetHotkeyActionDef = {
        kHubActionResetHotkeyId,
        "Reset hotkey default",
        "Reset dismantle hotkey and modifier requirements to defaults",
        &g_modHubClient,
        EMC_ACTION_FORCE_REFRESH,
        &HubResetHotkeyDefaultAction,
        "Restore the dismantle shortcut and modifier toggles to Wall-B-Gone defaults." };

    static const emc::ModHubClientSettingRowV1 kRows[] = {
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, kHubSettingEnabledId, &kEnabledSettingDef, 0, 0 },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, kHubSettingSleepingBagEnabledId, &kSleepingBagEnabledSettingDef, 0, 0 },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, kHubSettingAnyOwnBuildingId, &kAnyOwnBuildingSettingDef, 0, 0 },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND_V2, kHubSettingHotkeyId, &kHotkeySettingDef, 0, 0 },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION_V2, kHubActionResetHotkeyId, &kResetHotkeyActionDef, 0, 0 },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, kDebugLoggingSettingDef.setting_id, &kDebugLoggingSettingDef, kHubSectionAdvancedId, kHubSectionAdvancedLabel }
    };

    static const emc::ModHubClientTableRegistrationV1 kRegistration = {
        &kModDescriptor,
        kRows,
        static_cast<uint32_t>(sizeof(kRows) / sizeof(kRows[0])) };

    return &kRegistration;
}

static EMC_Result RegisterHubSettingRow(
    const EMC_HubApiV1* api,
    uint32_t api_size,
    EMC_ModHandle mod_handle,
    const emc::ModHubClientSettingRowV1& row)
{
    if (api == 0 || mod_handle == 0 || row.def == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    switch (row.kind)
    {
    case emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL:
        if (api->register_bool_setting == 0)
        {
            return EMC_ERR_INTERNAL;
        }
        return api->register_bool_setting(mod_handle, static_cast<const EMC_BoolSettingDefV1*>(row.def));

    case emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND_V2:
        if (api_size < EMC_HUB_API_V1_KEYBIND_SETTING_V2_MIN_SIZE || api->register_keybind_setting_v2 == 0)
        {
            return EMC_ERR_API_SIZE_MISMATCH;
        }
        return api->register_keybind_setting_v2(mod_handle, static_cast<const EMC_KeybindSettingDefV2*>(row.def));

    case emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION_V2:
        if (api_size < EMC_HUB_API_V1_ACTION_ROW_V2_MIN_SIZE || api->register_action_row_v2 == 0)
        {
            return EMC_ERR_API_SIZE_MISMATCH;
        }
        return api->register_action_row_v2(mod_handle, static_cast<const EMC_ActionRowDefV2*>(row.def));

    default:
        return EMC_ERR_INVALID_ARGUMENT;
    }
}

static bool ShouldForceHubAttachFailure(bool is_retry)
{
    if (g_modHubAttachFailureMode == kHubAttachFailureModeAlways)
    {
        return true;
    }

    return g_modHubAttachFailureMode == kHubAttachFailureModeStartupOnly && !is_retry;
}

static bool __cdecl ShouldForceHubAttachFailureForClient(void* user_data, bool is_retry, EMC_Result* out_result)
{
    (void)user_data;
    if (!ShouldForceHubAttachFailure(is_retry))
    {
        return false;
    }

    if (out_result != 0)
    {
        *out_result = EMC_ERR_INTERNAL;
    }

    return true;
}

static EMC_Result __cdecl RegisterHubSettingsForClient(const EMC_HubApiV1* api, void* user_data)
{
    (void)user_data;
    if (g_modHubRegisterMode == kHubRegisterModeFail)
    {
        return EMC_ERR_INTERNAL;
    }

    const emc::ModHubClientTableRegistrationV1* registration = GetModHubTableRegistration();
    if (api == 0 || registration == 0 || registration->mod_desc == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (api->register_mod == 0)
    {
        return EMC_ERR_INTERNAL;
    }

    if (registration->row_count > 0u && registration->rows == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EMC_ModHandle mod_handle = 0;
    EMC_Result result = api->register_mod(registration->mod_desc, &mod_handle);
    if (result != EMC_OK)
    {
        return result;
    }

    if (mod_handle == 0)
    {
        return EMC_ERR_INTERNAL;
    }

    const uint32_t api_size = api->api_size;
    for (uint32_t row_index = 0u; row_index < registration->row_count; ++row_index)
    {
        result = RegisterHubSettingRow(api, api_size, mod_handle, registration->rows[row_index]);
        if (result != EMC_OK)
        {
            return result;
        }
    }

    if (api_size >= EMC_HUB_API_V1_SETTING_SECTION_MIN_SIZE && api->register_setting_section != 0)
    {
        result = api->register_setting_section(mod_handle, &kDebugLoggingSectionDef);
        if (result != EMC_OK)
        {
            return result;
        }
    }

    static const EMC_BoolConditionRuleDefV1 kHiddenWhenDisabledRules[] = {
        {
            kHubSettingSleepingBagEnabledId,
            kHubSettingEnabledId,
            EMC_BOOL_CONDITION_EFFECT_HIDE,
            0 },
        {
            kHubSettingHotkeyId,
            kHubSettingEnabledId,
            EMC_BOOL_CONDITION_EFFECT_HIDE,
            0 },
        {
            kHubActionResetHotkeyId,
            kHubSettingEnabledId,
            EMC_BOOL_CONDITION_EFFECT_HIDE,
            0 }
    };

    for (size_t rule_index = 0u; rule_index < sizeof(kHiddenWhenDisabledRules) / sizeof(kHiddenWhenDisabledRules[0]); ++rule_index)
    {
        result = emc::RegisterBoolConditionRuleV1(api, mod_handle, &kHiddenWhenDisabledRules[rule_index]);
        if (result != EMC_OK)
        {
            return result;
        }
    }

    return EMC_OK;
}

static void ConfigureModHubClient()
{
    emc::ModHubClient::Config config;
    config.register_fn = &RegisterHubSettingsForClient;
    config.register_user_data = 0;
    config.should_force_attach_failure_fn = &ShouldForceHubAttachFailureForClient;
    config.attach_failure_user_data = 0;
    g_modHubClient.SetConfig(config);
}

static void StartModHubClient()
{
    // Mod Hub из сборки убран: настройки - во вкладке MCM (McmModHubBridge.h),
    // таблица к этому месту уже захвачена. К Mod Hub не подключаемся.
    return;
    const emc::ModHubClient::AttemptResult result = g_modHubClient.OnStartup();
    if (result == emc::ModHubClient::ATTACH_SUCCESS)
    {
        WallBGoneDebugLog("Wall-B-Gone INFO: event=mod_hub_attached use_hub_ui=1");
        return;
    }

    if (result == emc::ModHubClient::ATTACH_FAILED)
    {
        if (g_modHubClient.LastAttemptFailureResult() == EMC_ERR_NOT_FOUND)
        {
            return;
        }

        ErrorLog("Wall-B-Gone WARN: event=mod_hub_fallback reason=get_api_failed use_hub_ui=0");
    }
    else if (result == emc::ModHubClient::REGISTRATION_FAILED)
    {
        ErrorLog("Wall-B-Gone WARN: event=mod_hub_fallback reason=register_mod_or_setting_failed use_hub_ui=0");
    }
    else
    {
        ErrorLog("Wall-B-Gone WARN: event=mod_hub_fallback reason=invalid_client_configuration use_hub_ui=0");
    }
}

static void OnOptionsWindowInitForModHub()
{
    g_modHubClient.OnOptionsWindowInit();
}

static bool ShouldUseHubUiFromModHub()
{
    return g_modHubClient.UseHubUi();
}

extern "C" __declspec(dllexport) void __cdecl WallBGone_Test_ModHub_SetAttachFailureMode(int32_t mode)
{
    if (mode < kHubAttachFailureModeNone || mode > kHubAttachFailureModeAlways)
    {
        mode = kHubAttachFailureModeNone;
    }

    g_modHubAttachFailureMode = mode;
    ConfigureModHubClient();
}

extern "C" __declspec(dllexport) void __cdecl WallBGone_Test_ModHub_SetRegisterMode(int32_t mode)
{
    if (mode < kHubRegisterModeNormal || mode > kHubRegisterModeFail)
    {
        mode = kHubRegisterModeNormal;
    }

    g_modHubRegisterMode = mode;
    ConfigureModHubClient();
}

extern "C" __declspec(dllexport) void __cdecl WallBGone_Test_ModHub_ResetClientState()
{
    g_modEnabled = true;
    g_sleepingBagDismantleEnabled = true;
    g_hotkeyPrimary = kDefaultHotkey;
    g_pendingHotkeyPrimary = kDefaultHotkey;
    SyncNativeBindingFromHotkey();
    RefreshHotkeyUiWidgets();

    g_modHubAttachFailureMode = kHubAttachFailureModeNone;
    g_modHubRegisterMode = kHubRegisterModeNormal;
    g_modHubClient.Reset();
    ConfigureModHubClient();
}

extern "C" __declspec(dllexport) void __cdecl WallBGone_Test_ModHub_RunStartupAttach()
{
    g_modHubClient.Reset();
    ConfigureModHubClient();
    StartModHubClient();
}

extern "C" __declspec(dllexport) void __cdecl WallBGone_Test_ModHub_OnOptionsWindowInit()
{
    OnOptionsWindowInitForModHub();
}

extern "C" __declspec(dllexport) int32_t __cdecl WallBGone_Test_ModHub_UseHubUi()
{
    return g_modHubClient.UseHubUi() ? 1 : 0;
}

extern "C" __declspec(dllexport) int32_t __cdecl WallBGone_Test_ModHub_IsAttachRetryPending()
{
    return g_modHubClient.IsAttachRetryPending() ? 1 : 0;
}

extern "C" __declspec(dllexport) int32_t __cdecl WallBGone_Test_ModHub_HasAttachRetryAttempted()
{
    return g_modHubClient.HasAttachRetryAttempted() ? 1 : 0;
}

extern "C" __declspec(dllexport) int __cdecl WallBGone_GetRuntimeStateV1(WallBGoneRuntimeStateV1* out_state)
{
    if (!out_state)
    {
        return 1;
    }

    EnsureRuntimeHotkeyValid();
    out_state->enabled = g_modEnabled ? 1 : 0;
    out_state->sleeping_bag_dismantle_enabled = g_sleepingBagDismantleEnabled ? 1 : 0;
    out_state->hotkey_keycode = static_cast<int32_t>(g_hotkeyPrimary);
    out_state->hotkey_modifiers = 0u;
    if (g_hotkeyRequireCtrl)
    {
        out_state->hotkey_modifiers |= kHotkeyModifierCtrlMask;
    }
    if (g_hotkeyRequireShift)
    {
        out_state->hotkey_modifiers |= kHotkeyModifierShiftMask;
    }
    if (g_hotkeyRequireAlt)
    {
        out_state->hotkey_modifiers |= kHotkeyModifierAltMask;
    }
    return 0;
}

extern "C" __declspec(dllexport) int __cdecl WallBGone_SetRuntimeStateV1(
    const WallBGoneRuntimeStateV1* state,
    char* err_buf,
    uint32_t err_buf_size)
{
    if (!state)
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "missing_state");
        return 1;
    }

    if ((state->enabled != 0 && state->enabled != 1)
        || (state->sleeping_bag_dismantle_enabled != 0 && state->sleeping_bag_dismantle_enabled != 1))
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "invalid_bool");
        return 1;
    }

    const uint32_t supportedModifierMask = kHotkeyModifierCtrlMask | kHotkeyModifierShiftMask | kHotkeyModifierAltMask;
    if ((state->hotkey_modifiers & ~supportedModifierMask) != 0u)
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "invalid_modifiers");
        return 1;
    }

    const OIS::KeyCode requestedHotkey = static_cast<OIS::KeyCode>(state->hotkey_keycode);
    std::string validationReason;
    if (ValidateHotkey(requestedHotkey, &validationReason) != HotkeyValidation_Ok)
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "invalid_hotkey");
        return 1;
    }

    g_modEnabled = state->enabled != 0;
    g_sleepingBagDismantleEnabled = state->sleeping_bag_dismantle_enabled != 0;
    g_hotkeyPrimary = requestedHotkey;
    g_pendingHotkeyPrimary = requestedHotkey;
    g_hotkeyRequireCtrl = (state->hotkey_modifiers & kHotkeyModifierCtrlMask) != 0u;
    g_hotkeyRequireShift = (state->hotkey_modifiers & kHotkeyModifierShiftMask) != 0u;
    g_hotkeyRequireAlt = (state->hotkey_modifiers & kHotkeyModifierAltMask) != 0u;
    SyncNativeBindingFromHotkey();
    RefreshHotkeyUiWidgets();

    if (!SaveConfigState())
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "persist_failed");
        return 2;
    }

    return 0;
}

// Страница в ModConfigMenu (вкладка MCM в настройках игры) вместо Mod Hub.
MCM_MODHUB_BRIDGE_TABLE(GetModHubTableRegistration(), "Dismantles the selected wall, bed or furniture with a hotkey and returns the materials.")
