#include "emc/mod_hub_client.h"
#include "emc/mod_hub_consumer_helpers.h"

#include <ois/OISKeyboard.h>

namespace
{
#define JBG_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

const char* kHubNamespaceId = "emkej.qol";
const char* kHubNamespaceDisplayName = "Emkej QoL";
const char* kHubModId = "job_b_gone";
const char* kHubModDisplayName = "Job-B-Gone";

enum HubBoolSettingKind
{
    HubBoolSettingKind_ConfigField = 0,
    HubBoolSettingKind_PanelToggleRequireCtrl = 1,
    HubBoolSettingKind_PanelToggleRequireAlt = 2,
    HubBoolSettingKind_PanelToggleRequireShift = 3
};

struct HubBoolSettingDescriptor
{
    const char* settingId;
    const char* label;
    const char* description;
    HubBoolSettingKind kind;
    bool PluginConfig::*field;
};

struct HubKeybindSettingDescriptor
{
    const char* settingId;
    const char* label;
    const char* description;
};

struct HubKeyNameEntry
{
    const char* token;
    int32_t keyCode;
};

struct PanelVisibilityToggleHubState
{
    bool enabled;
    bool requireCtrl;
    bool requireAlt;
    bool requireShift;
    int32_t keyCode;
};

EMC_Result __cdecl GetBoolSettingValue(void* user_data, int32_t* out_value);
EMC_Result __cdecl SetBoolSettingValue(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size);
EMC_Result __cdecl GetKeybindSettingValue(void* user_data, EMC_KeybindValueV1* out_value);
EMC_Result __cdecl SetKeybindSettingValue(void* user_data, EMC_KeybindValueV1 value, char* err_buf, uint32_t err_buf_size);

emc::ModHubClient g_modHubClient;
bool g_modHubClientConfigured = false;
bool g_modHubRowsInitialized = false;

HubBoolSettingDescriptor g_boolSettingDescriptors[] = {
    { "enabled", "Enabled", "Enable the Job-B-Gone panel and actions", HubBoolSettingKind_ConfigField, &PluginConfig::enabled },
    { "panel_visibility_toggle_require_ctrl", "Require Ctrl", "Require Ctrl to be held with the panel toggle hotkey", HubBoolSettingKind_PanelToggleRequireCtrl, 0 },
    { "panel_visibility_toggle_require_alt", "Require Alt", "Require Alt to be held with the panel toggle hotkey", HubBoolSettingKind_PanelToggleRequireAlt, 0 },
    { "panel_visibility_toggle_require_shift", "Require Shift", "Require Shift to be held with the panel toggle hotkey", HubBoolSettingKind_PanelToggleRequireShift, 0 },
    { "enable_delete_all_jobs_top_actions", "Top actions", "Show the top scoped delete-all buttons", HubBoolSettingKind_ConfigField, &PluginConfig::enableDeleteAllJobsTopActions },
    { "job_b_gone_panel_collapsed", "Panel collapsed", "Persist whether the panel is collapsed", HubBoolSettingKind_ConfigField, &PluginConfig::jobBGonePanelCollapsed },
    { "hide_panel_during_character_creation", "Hide in creation", "Hide the panel while character creation or editing is open", HubBoolSettingKind_ConfigField, &PluginConfig::hidePanelDuringCharacterCreation },
    { "hide_panel_during_inventory_open", "Hide in inventory", "Hide the panel while inventory or trade windows are open", HubBoolSettingKind_ConfigField, &PluginConfig::hidePanelDuringInventoryOpen },
    { "hide_panel_during_character_interaction", "Hide in dialogue", "Hide the panel while characters are in dialogue or interaction", HubBoolSettingKind_ConfigField, &PluginConfig::hidePanelDuringCharacterInteraction },
    { "debug_log_transitions", "Debug transitions", "Enable transition and visibility diagnostics", HubBoolSettingKind_ConfigField, &PluginConfig::debugLogTransitions },
    { "log_selected_member_job_snapshot", "Debug snapshots", "Log selected-member job snapshots for diagnostics", HubBoolSettingKind_ConfigField, &PluginConfig::logSelectedMemberJobSnapshot }
};

HubKeybindSettingDescriptor g_keybindSettingDescriptors[] = {
    { "panel_visibility_toggle_hotkey", "Panel toggle key", "Primary key for the panel visibility toggle. Clear to disable the hotkey.", }
};

const HubKeyNameEntry kHubKeyNameMap[] = {
    { "A", static_cast<int32_t>(OIS::KC_A) }, { "B", static_cast<int32_t>(OIS::KC_B) }, { "C", static_cast<int32_t>(OIS::KC_C) },
    { "D", static_cast<int32_t>(OIS::KC_D) }, { "E", static_cast<int32_t>(OIS::KC_E) }, { "F", static_cast<int32_t>(OIS::KC_F) },
    { "G", static_cast<int32_t>(OIS::KC_G) }, { "H", static_cast<int32_t>(OIS::KC_H) }, { "I", static_cast<int32_t>(OIS::KC_I) },
    { "J", static_cast<int32_t>(OIS::KC_J) }, { "K", static_cast<int32_t>(OIS::KC_K) }, { "L", static_cast<int32_t>(OIS::KC_L) },
    { "M", static_cast<int32_t>(OIS::KC_M) }, { "N", static_cast<int32_t>(OIS::KC_N) }, { "O", static_cast<int32_t>(OIS::KC_O) },
    { "P", static_cast<int32_t>(OIS::KC_P) }, { "Q", static_cast<int32_t>(OIS::KC_Q) }, { "R", static_cast<int32_t>(OIS::KC_R) },
    { "S", static_cast<int32_t>(OIS::KC_S) }, { "T", static_cast<int32_t>(OIS::KC_T) }, { "U", static_cast<int32_t>(OIS::KC_U) },
    { "V", static_cast<int32_t>(OIS::KC_V) }, { "W", static_cast<int32_t>(OIS::KC_W) }, { "X", static_cast<int32_t>(OIS::KC_X) },
    { "Y", static_cast<int32_t>(OIS::KC_Y) }, { "Z", static_cast<int32_t>(OIS::KC_Z) },
    { "0", static_cast<int32_t>(OIS::KC_0) }, { "1", static_cast<int32_t>(OIS::KC_1) }, { "2", static_cast<int32_t>(OIS::KC_2) },
    { "3", static_cast<int32_t>(OIS::KC_3) }, { "4", static_cast<int32_t>(OIS::KC_4) }, { "5", static_cast<int32_t>(OIS::KC_5) },
    { "6", static_cast<int32_t>(OIS::KC_6) }, { "7", static_cast<int32_t>(OIS::KC_7) }, { "8", static_cast<int32_t>(OIS::KC_8) },
    { "9", static_cast<int32_t>(OIS::KC_9) },
    { "SPACE", static_cast<int32_t>(OIS::KC_SPACE) }, { "TAB", static_cast<int32_t>(OIS::KC_TAB) }, { "ENTER", static_cast<int32_t>(OIS::KC_RETURN) },
    { "ESC", static_cast<int32_t>(OIS::KC_ESCAPE) }, { "BACKSPACE", static_cast<int32_t>(OIS::KC_BACK) },
    { "INSERT", static_cast<int32_t>(OIS::KC_INSERT) }, { "DELETE", static_cast<int32_t>(OIS::KC_DELETE) },
    { "HOME", static_cast<int32_t>(OIS::KC_HOME) }, { "END", static_cast<int32_t>(OIS::KC_END) },
    { "PAGEUP", static_cast<int32_t>(OIS::KC_PGUP) }, { "PAGEDOWN", static_cast<int32_t>(OIS::KC_PGDOWN) },
    { "UP", static_cast<int32_t>(OIS::KC_UP) }, { "DOWN", static_cast<int32_t>(OIS::KC_DOWN) },
    { "LEFT", static_cast<int32_t>(OIS::KC_LEFT) }, { "RIGHT", static_cast<int32_t>(OIS::KC_RIGHT) },
    { "F1", static_cast<int32_t>(OIS::KC_F1) }, { "F2", static_cast<int32_t>(OIS::KC_F2) }, { "F3", static_cast<int32_t>(OIS::KC_F3) },
    { "F4", static_cast<int32_t>(OIS::KC_F4) }, { "F5", static_cast<int32_t>(OIS::KC_F5) }, { "F6", static_cast<int32_t>(OIS::KC_F6) },
    { "F7", static_cast<int32_t>(OIS::KC_F7) }, { "F8", static_cast<int32_t>(OIS::KC_F8) }, { "F9", static_cast<int32_t>(OIS::KC_F9) },
    { "F10", static_cast<int32_t>(OIS::KC_F10) }, { "F11", static_cast<int32_t>(OIS::KC_F11) }, { "F12", static_cast<int32_t>(OIS::KC_F12) },
    { "F13", static_cast<int32_t>(OIS::KC_F13) }, { "F14", static_cast<int32_t>(OIS::KC_F14) }, { "F15", static_cast<int32_t>(OIS::KC_F15) }
};

enum
{
    kHubBoolSettingCount = static_cast<int>(JBG_ARRAY_COUNT(g_boolSettingDescriptors)),
    kHubKeybindSettingCount = static_cast<int>(JBG_ARRAY_COUNT(g_keybindSettingDescriptors)),
    kHubRowCount = kHubBoolSettingCount + kHubKeybindSettingCount
};

EMC_BoolSettingDefV1 g_boolSettingDefs[kHubBoolSettingCount];
EMC_KeybindSettingDefV1 g_keybindSettingDefs[kHubKeybindSettingCount];
emc::ModHubClientSettingRowV1 g_modHubRows[kHubRowCount];

const EMC_ModDescriptorV1 kModHubDescriptor = {
    kHubNamespaceId,
    kHubNamespaceDisplayName,
    kHubModId,
    kHubModDisplayName,
    &g_modHubClient
};

emc::ModHubClientTableRegistrationV1 g_modHubRegistration = {
    &kModHubDescriptor,
    g_modHubRows,
    0u
};

bool TryMapPanelVisibilityTokenToHubKeyCode(const std::string& token, int32_t* keyCodeOut)
{
    if (keyCodeOut == 0)
    {
        return false;
    }

    const std::string upper = ToUpperAscii(TrimAscii(token));
    if (upper.empty())
    {
        return false;
    }

    for (size_t i = 0u; i < JBG_ARRAY_COUNT(kHubKeyNameMap); ++i)
    {
        if (upper == kHubKeyNameMap[i].token)
        {
            *keyCodeOut = kHubKeyNameMap[i].keyCode;
            return true;
        }
    }

    return false;
}

bool TryMapHubKeyCodeToPanelVisibilityToken(int32_t keyCode, std::string* tokenOut)
{
    if (tokenOut == 0)
    {
        return false;
    }

    for (size_t i = 0u; i < JBG_ARRAY_COUNT(kHubKeyNameMap); ++i)
    {
        if (keyCode == kHubKeyNameMap[i].keyCode)
        {
            *tokenOut = kHubKeyNameMap[i].token;
            return true;
        }
    }

    return false;
}

bool ValidatePanelVisibilityPrimaryKeyCode(int32_t keyCode, std::string* reasonOut)
{
    if (keyCode == EMC_KEY_UNBOUND)
    {
        if (reasonOut != 0)
        {
            reasonOut->clear();
        }
        return true;
    }

    for (size_t i = 0u; i < JBG_ARRAY_COUNT(kHubKeyNameMap); ++i)
    {
        if (keyCode == kHubKeyNameMap[i].keyCode)
        {
            if (reasonOut != 0)
            {
                reasonOut->clear();
            }
            return true;
        }
    }

    if (reasonOut != 0)
    {
        *reasonOut = "unsupported key";
    }
    return false;
}

bool GetDefaultPanelVisibilityPrimaryKeyCode(int32_t* keyCodeOut)
{
    if (keyCodeOut == 0)
    {
        return false;
    }

    std::string canonical;
    if (!TryNormalizePanelVisibilityToggleHotkey(kPanelVisibilityToggleHotkeyDefault, &canonical, 0, 0, 0, 0, 0))
    {
        return false;
    }

    const size_t plusPos = canonical.find_last_of('+');
    const std::string primaryToken = (plusPos == std::string::npos) ? canonical : canonical.substr(plusPos + 1);
    return TryMapPanelVisibilityTokenToHubKeyCode(primaryToken, keyCodeOut);
}

bool ReadPanelVisibilityToggleHubState(const PluginConfig& config, PanelVisibilityToggleHubState* stateOut)
{
    if (stateOut == 0)
    {
        return false;
    }

    std::string canonical;
    bool enabled = false;
    bool requireCtrl = false;
    bool requireAlt = false;
    bool requireShift = false;
    if (!TryNormalizePanelVisibilityToggleHotkey(
            config.panelVisibilityToggleHotkey,
            &canonical,
            &enabled,
            &requireCtrl,
            &requireAlt,
            &requireShift,
            0))
    {
        return false;
    }

    stateOut->enabled = enabled;
    stateOut->requireCtrl = requireCtrl;
    stateOut->requireAlt = requireAlt;
    stateOut->requireShift = requireShift;
    stateOut->keyCode = EMC_KEY_UNBOUND;

    if (!enabled)
    {
        return true;
    }

    const size_t plusPos = canonical.find_last_of('+');
    const std::string primaryToken = (plusPos == std::string::npos) ? canonical : canonical.substr(plusPos + 1);
    return TryMapPanelVisibilityTokenToHubKeyCode(primaryToken, &stateOut->keyCode);
}

void SeedPanelVisibilityToggleModifierEditState(PanelVisibilityToggleHubState* state)
{
    if (state == 0 || (state->enabled && state->keyCode != EMC_KEY_UNBOUND))
    {
        return;
    }

    int32_t defaultKeyCode = EMC_KEY_UNBOUND;
    if (!GetDefaultPanelVisibilityPrimaryKeyCode(&defaultKeyCode))
    {
        defaultKeyCode = static_cast<int32_t>(OIS::KC_B);
    }

    state->enabled = true;
    state->keyCode = defaultKeyCode;
    state->requireCtrl = false;
    state->requireAlt = false;
    state->requireShift = false;
}

bool TryBuildPanelVisibilityToggleHotkeyValue(
    const PanelVisibilityToggleHubState& state,
    std::string* hotkeyValueOut,
    std::string* reasonOut)
{
    if (hotkeyValueOut == 0)
    {
        if (reasonOut != 0)
        {
            *reasonOut = "invalid_output";
        }
        return false;
    }

    if (!state.enabled || state.keyCode == EMC_KEY_UNBOUND)
    {
        *hotkeyValueOut = kPanelVisibilityToggleHotkeyDisabled;
        if (reasonOut != 0)
        {
            reasonOut->clear();
        }
        return true;
    }

    std::string primaryToken;
    if (!TryMapHubKeyCodeToPanelVisibilityToken(state.keyCode, &primaryToken))
    {
        if (reasonOut != 0)
        {
            *reasonOut = "unsupported key";
        }
        return false;
    }

    std::stringstream rawValue;
    if (state.requireCtrl)
    {
        rawValue << "CTRL+";
    }
    if (state.requireAlt)
    {
        rawValue << "ALT+";
    }
    if (state.requireShift)
    {
        rawValue << "SHIFT+";
    }
    rawValue << primaryToken;

    std::string canonical;
    if (!TryNormalizePanelVisibilityToggleHotkey(rawValue.str(), &canonical, 0, 0, 0, 0, 0))
    {
        if (reasonOut != 0)
        {
            *reasonOut = "invalid_hotkey";
        }
        return false;
    }

    *hotkeyValueOut = canonical;
    if (reasonOut != 0)
    {
        reasonOut->clear();
    }
    return true;
}

void InitializeSettingDefinitions()
{
    if (g_modHubRowsInitialized)
    {
        return;
    }

    for (size_t i = 0u; i < JBG_ARRAY_COUNT(g_boolSettingDescriptors); ++i)
    {
        g_boolSettingDefs[i].setting_id = g_boolSettingDescriptors[i].settingId;
        g_boolSettingDefs[i].label = g_boolSettingDescriptors[i].label;
        g_boolSettingDefs[i].description = g_boolSettingDescriptors[i].description;
        g_boolSettingDefs[i].user_data = &g_boolSettingDescriptors[i];
        g_boolSettingDefs[i].get_value = &GetBoolSettingValue;
        g_boolSettingDefs[i].set_value = &SetBoolSettingValue;

    }

    for (size_t i = 0u; i < JBG_ARRAY_COUNT(g_keybindSettingDescriptors); ++i)
    {
        g_keybindSettingDefs[i].setting_id = g_keybindSettingDescriptors[i].settingId;
        g_keybindSettingDefs[i].label = g_keybindSettingDescriptors[i].label;
        g_keybindSettingDefs[i].description = g_keybindSettingDescriptors[i].description;
        g_keybindSettingDefs[i].user_data = &g_keybindSettingDescriptors[i];
        g_keybindSettingDefs[i].get_value = &GetKeybindSettingValue;
        g_keybindSettingDefs[i].set_value = &SetKeybindSettingValue;
    }

    size_t rowIndex = 0u;
    g_modHubRows[rowIndex].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL;
    g_modHubRows[rowIndex].def = &g_boolSettingDefs[0];
    ++rowIndex;

    for (size_t i = 0u; i < JBG_ARRAY_COUNT(g_keybindSettingDescriptors); ++i)
    {
        g_modHubRows[rowIndex].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND;
        g_modHubRows[rowIndex].def = &g_keybindSettingDefs[i];
        ++rowIndex;
    }

    for (size_t i = 1u; i < JBG_ARRAY_COUNT(g_boolSettingDescriptors); ++i)
    {
        g_modHubRows[rowIndex].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL;
        g_modHubRows[rowIndex].def = &g_boolSettingDefs[i];
        ++rowIndex;
    }

    g_modHubRegistration.row_count = static_cast<uint32_t>(rowIndex);
    g_modHubRowsInitialized = true;
}

EMC_Result ApplyHubConfigUpdate(const PluginConfig& updated, char* err_buf, uint32_t err_buf_size)
{
    const PluginConfig previous = g_config;
    const bool previousCollapsed = g_jobBGonePanelCollapsed;
    const bool previousNeedsWriteBack = g_configNeedsWriteBack;

    g_config = updated;
    g_jobBGonePanelCollapsed = updated.jobBGonePanelCollapsed;
    RefreshPanelVisibilityToggleHotkeyBinding();

    if (!SaveConfigState())
    {
        g_config = previous;
        g_jobBGonePanelCollapsed = previousCollapsed;
        g_configNeedsWriteBack = previousNeedsWriteBack;
        RefreshPanelVisibilityToggleHotkeyBinding();
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "persist_failed");
        return EMC_ERR_INTERNAL;
    }

    g_configNeedsWriteBack = false;

    if (!updated.enabled)
    {
        DestroySelectedMemberJobPanelButton();
    }
    else if (!updated.enableDeleteAllJobsTopActions || updated.jobBGonePanelCollapsed)
    {
        HideConfirmationOverlay();
    }

    emc::consumer::WriteErrorMessage(err_buf, err_buf_size, 0);
    return EMC_OK;
}

EMC_Result __cdecl GetBoolSettingValue(void* user_data, int32_t* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HubBoolSettingDescriptor* descriptor = static_cast<const HubBoolSettingDescriptor*>(user_data);
    if (descriptor->kind == HubBoolSettingKind_PanelToggleRequireCtrl
        || descriptor->kind == HubBoolSettingKind_PanelToggleRequireAlt
        || descriptor->kind == HubBoolSettingKind_PanelToggleRequireShift)
    {
        PanelVisibilityToggleHubState state;
        if (!ReadPanelVisibilityToggleHubState(g_config, &state))
        {
            return EMC_ERR_INTERNAL;
        }

        if (descriptor->kind == HubBoolSettingKind_PanelToggleRequireCtrl)
        {
            *out_value = state.requireCtrl ? 1 : 0;
            return EMC_OK;
        }

        if (descriptor->kind == HubBoolSettingKind_PanelToggleRequireAlt)
        {
            *out_value = state.requireAlt ? 1 : 0;
            return EMC_OK;
        }

        *out_value = state.requireShift ? 1 : 0;
        return EMC_OK;
    }

    if (descriptor->field == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = (g_config.*(descriptor->field)) ? 1 : 0;
    return EMC_OK;
}

EMC_Result __cdecl SetBoolSettingValue(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    if (user_data == 0)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_setting_context");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value != 0 && value != 1)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_bool");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HubBoolSettingDescriptor* descriptor = static_cast<const HubBoolSettingDescriptor*>(user_data);
    if (descriptor->kind == HubBoolSettingKind_PanelToggleRequireCtrl
        || descriptor->kind == HubBoolSettingKind_PanelToggleRequireAlt
        || descriptor->kind == HubBoolSettingKind_PanelToggleRequireShift)
    {
        PanelVisibilityToggleHubState state;
        if (!ReadPanelVisibilityToggleHubState(g_config, &state))
        {
            emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_hotkey");
            return EMC_ERR_INTERNAL;
        }

        SeedPanelVisibilityToggleModifierEditState(&state);

        if (descriptor->kind == HubBoolSettingKind_PanelToggleRequireCtrl)
        {
            state.requireCtrl = value != 0;
        }
        else if (descriptor->kind == HubBoolSettingKind_PanelToggleRequireAlt)
        {
            state.requireAlt = value != 0;
        }
        else
        {
            state.requireShift = value != 0;
        }

        std::string hotkeyValue;
        std::string reason;
        if (!TryBuildPanelVisibilityToggleHotkeyValue(state, &hotkeyValue, &reason))
        {
            emc::consumer::WriteErrorMessage(
                err_buf,
                err_buf_size,
                reason.empty() ? "invalid_hotkey" : reason.c_str());
            return EMC_ERR_INVALID_ARGUMENT;
        }

        PluginConfig updated = g_config;
        updated.panelVisibilityToggleHotkey = hotkeyValue;
        return ApplyHubConfigUpdate(updated, err_buf, err_buf_size);
    }

    if (descriptor->field == 0)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_setting_context");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    PluginConfig updated = g_config;
    updated.*(descriptor->field) = value != 0;
    return ApplyHubConfigUpdate(updated, err_buf, err_buf_size);
}

EMC_Result __cdecl GetKeybindSettingValue(void* user_data, EMC_KeybindValueV1* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    PanelVisibilityToggleHubState state;
    if (!ReadPanelVisibilityToggleHubState(g_config, &state))
    {
        return EMC_ERR_INTERNAL;
    }

    out_value->keycode = state.enabled ? state.keyCode : EMC_KEY_UNBOUND;
    out_value->modifiers = 0u;
    return EMC_OK;
}

EMC_Result __cdecl SetKeybindSettingValue(
    void* user_data,
    EMC_KeybindValueV1 value,
    char* err_buf,
    uint32_t err_buf_size)
{
    if (user_data == 0)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_setting_context");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value.modifiers != 0u)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "use_modifier_toggles");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    std::string validationReason;
    if (!ValidatePanelVisibilityPrimaryKeyCode(value.keycode, &validationReason))
    {
        emc::consumer::WriteErrorMessage(
            err_buf,
            err_buf_size,
            validationReason.empty() ? "invalid_hotkey" : validationReason.c_str());
        return EMC_ERR_INVALID_ARGUMENT;
    }

    PanelVisibilityToggleHubState state;
    if (!ReadPanelVisibilityToggleHubState(g_config, &state))
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_hotkey");
        return EMC_ERR_INTERNAL;
    }

    state.enabled = value.keycode != EMC_KEY_UNBOUND;
    state.keyCode = value.keycode;
    if (!state.enabled)
    {
        state.requireCtrl = false;
        state.requireAlt = false;
        state.requireShift = false;
    }

    std::string hotkeyValue;
    std::string reason;
    if (!TryBuildPanelVisibilityToggleHotkeyValue(state, &hotkeyValue, &reason))
    {
        emc::consumer::WriteErrorMessage(
            err_buf,
            err_buf_size,
            reason.empty() ? "invalid_hotkey" : reason.c_str());
        return EMC_ERR_INVALID_ARGUMENT;
    }

    PluginConfig updated = g_config;
    updated.panelVisibilityToggleHotkey = hotkeyValue;
    return ApplyHubConfigUpdate(updated, err_buf, err_buf_size);
}

void LogModHubFallback(const char* reason)
{
    const bool modHubMissing = g_modHubClient.LastAttemptFailureResult() == EMC_ERR_NOT_FOUND;
    std::stringstream line;
    line << (modHubMissing ? "Job-B-Gone INFO: mod_hub_fallback" : "Job-B-Gone WARN: mod_hub_fallback")
         << " reason=" << (reason != 0 ? reason : "unknown")
         << " result=" << g_modHubClient.LastAttemptFailureResult()
         << " use_hub_ui=0";

    if (modHubMissing)
    {
        DebugLog(line.str().c_str());
        return;
    }

    ErrorLog(line.str().c_str());
}

void EnsureModHubClientConfigured()
{
    if (g_modHubClientConfigured)
    {
        return;
    }

    InitializeSettingDefinitions();

    emc::ModHubClient::Config config;
    config.table_registration = &g_modHubRegistration;
    g_modHubClient.SetConfig(config);
    g_modHubClientConfigured = true;
}

void StartModHubClient()
{
    EnsureModHubClientConfigured();

    const emc::ModHubClient::AttemptResult result = g_modHubClient.OnStartup();
    if (result == emc::ModHubClient::ATTACH_SUCCESS)
    {
        DebugLog("Job-B-Gone INFO: mod_hub_attached use_hub_ui=1");
        return;
    }

    if (result == emc::ModHubClient::ATTACH_FAILED)
    {
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

#undef JBG_ARRAY_COUNT
} // namespace
