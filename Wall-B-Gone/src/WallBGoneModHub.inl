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

static EMC_Result __cdecl HubGetDismantleHotkeySetting(void* user_data, EMC_KeybindValueV1* out_value)
{
    if (!IsHubUserDataValid(user_data) || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    EnsureRuntimeHotkeyValid();
    out_value->keycode = static_cast<int32_t>(g_hotkeyPrimary);
    out_value->modifiers = 0u;
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

    if (value.modifiers != 0u)
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "use_modifier_toggles");
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
    g_hotkeyPrimary = requestedHotkey;
    g_pendingHotkeyPrimary = requestedHotkey;
    SyncNativeBindingFromHotkey();
    RefreshHotkeyUiWidgets();

    if (!SaveConfigState())
    {
        g_hotkeyPrimary = previous_hotkey;
        g_pendingHotkeyPrimary = previous_hotkey;
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
        "Allow sleeping bag dismantle",
        "Allow dismantle behavior for sleeping bags",
        &g_modHubClient,
        &HubGetSleepingBagEnabledSetting,
        &HubSetSleepingBagEnabledSetting };

    static const EMC_KeybindSettingDefV1 kHotkeySettingDef = {
        kHubSettingHotkeyId,
        "Dismantle hotkey",
        "Primary key used to dismantle the selected wall or sleeping bag. Use the modifier toggles below for combos.",
        &g_modHubClient,
        &HubGetDismantleHotkeySetting,
        &HubSetDismantleHotkeySetting };

    static const EMC_BoolSettingDefV1 kHotkeyRequireCtrlSettingDef = {
        kHubSettingHotkeyRequireCtrlId,
        "Require Ctrl",
        "Require Ctrl to be held with the dismantle hotkey",
        &g_modHubClient,
        &HubGetHotkeyRequireCtrlSetting,
        &HubSetHotkeyRequireCtrlSetting };

    static const EMC_BoolSettingDefV1 kHotkeyRequireShiftSettingDef = {
        kHubSettingHotkeyRequireShiftId,
        "Require Shift",
        "Require Shift to be held with the dismantle hotkey",
        &g_modHubClient,
        &HubGetHotkeyRequireShiftSetting,
        &HubSetHotkeyRequireShiftSetting };

    static const EMC_BoolSettingDefV1 kHotkeyRequireAltSettingDef = {
        kHubSettingHotkeyRequireAltId,
        "Require Alt",
        "Require Alt to be held with the dismantle hotkey",
        &g_modHubClient,
        &HubGetHotkeyRequireAltSetting,
        &HubSetHotkeyRequireAltSetting };

    static const EMC_ActionRowDefV1 kResetHotkeyActionDef = {
        kHubActionResetHotkeyId,
        "Reset hotkey default",
        "Reset dismantle hotkey and modifier requirements to defaults",
        &g_modHubClient,
        EMC_ACTION_FORCE_REFRESH,
        &HubResetHotkeyDefaultAction };

    static const emc::ModHubClientSettingRowV1 kRows[] = {
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kEnabledSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kSleepingBagEnabledSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND, &kHotkeySettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kHotkeyRequireCtrlSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kHotkeyRequireShiftSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kHotkeyRequireAltSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION, &kResetHotkeyActionDef }
    };

    static const emc::ModHubClientTableRegistrationV1 kRegistration = {
        &kModDescriptor,
        kRows,
        static_cast<uint32_t>(sizeof(kRows) / sizeof(kRows[0])) };

    return &kRegistration;
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

    return emc::RegisterSettingsTableV1(api, GetModHubTableRegistration());
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
    const emc::ModHubClient::AttemptResult result = g_modHubClient.OnStartup();
    if (result == emc::ModHubClient::ATTACH_SUCCESS)
    {
        DebugLog("Wall-B-Gone INFO: event=mod_hub_attached use_hub_ui=1");
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
