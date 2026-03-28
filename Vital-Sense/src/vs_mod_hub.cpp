#include "vs_mod_hub.h"

#include "emc/mod_hub_client.h"
#include "emc/mod_hub_consumer_helpers.h"

#include "vs_character_tint.h"
#include "vs_config.h"
#include "vs_keybind.h"
#include "vs_log.h"
#include "vs_marker_render.h"
#include "vs_parse.h"
#include "vs_runtime_state.h"

#include <cstring>
#include <sstream>

namespace vs_mod_hub
{
namespace
{
#define VS_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

const char* kPluginName = "Vital-Sense";
const char* kHubNamespaceId = "emkej.qol";
const char* kHubNamespaceDisplayName = "Emkej QoL";
const char* kHubModId = "vital_sense";
const char* kHubModDisplayName = "Vital Sense";

struct HubBoolSettingDescriptor
{
    const char* settingId;
    const char* label;
    const char* description;
    bool PluginConfig::*field;
};

struct HubIntSettingDescriptor
{
    const char* settingId;
    const char* label;
    const char* description;
    DWORD PluginConfig::*field;
    int32_t minValue;
    int32_t maxValue;
    int32_t step;
};

struct HubKeybindSettingDescriptor
{
    const char* settingId;
    const char* label;
    const char* description;
    int32_t PluginConfig::*field;
};

struct HubColorSettingDescriptor
{
    const char* settingId;
    const char* label;
    const char* description;
    MyGUI::Colour PluginConfig::*field;
    uint32_t previewKind;
    bool preserveAlpha;
};

EMC_Result __cdecl GetBoolSettingValue(void* user_data, int32_t* out_value);
EMC_Result __cdecl SetBoolSettingValue(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size);
EMC_Result __cdecl GetIntSettingValue(void* user_data, int32_t* out_value);
EMC_Result __cdecl SetIntSettingValue(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size);
EMC_Result __cdecl GetKeybindSettingValue(void* user_data, EMC_KeybindValueV1* out_value);
EMC_Result __cdecl SetKeybindSettingValue(void* user_data, EMC_KeybindValueV1 value, char* err_buf, uint32_t err_buf_size);
EMC_Result __cdecl GetColorSettingValue(void* user_data, char* out_value, uint32_t out_value_size);
EMC_Result __cdecl SetColorSettingValue(void* user_data, const char* value, char* err_buf, uint32_t err_buf_size);

emc::ModHubClient g_modHubClient;
bool g_modHubClientConfigured = false;
bool g_modHubRowsInitialized = false;

HubBoolSettingDescriptor g_boolSettingDescriptors[] = {
    { "enabled", "Enabled", "Enable all Vital Sense overlays and tint behavior", &PluginConfig::enabled },
    { "highlight_key_require_ctrl", "Require Ctrl", "Require Ctrl to be held with the highlight key", &PluginConfig::highlightKeyRequireCtrl },
    { "highlight_key_require_shift", "Require Shift", "Require Shift to be held with the highlight key", &PluginConfig::highlightKeyRequireShift },
    { "highlight_key_require_alt", "Require Alt", "Require Alt to be held with the highlight key", &PluginConfig::highlightKeyRequireAlt },
    { "enable_unconscious", "Show unconscious", "Show markers for unconscious targets", &PluginConfig::enableUnconsciousState },
    { "enable_recovery_coma", "Show recovery coma", "Show markers for recovery coma targets", &PluginConfig::enableRecoveryComaState },
    { "enable_dying", "Show dying", "Show markers for dying targets", &PluginConfig::enableDyingState },
    { "enable_playing_dead", "Show playing dead", "Show markers for playing dead targets", &PluginConfig::enablePlayingDeadState },
    { "enable_dead", "Show dead", "Show markers for dead targets", &PluginConfig::enableDeadState },
    { "show_icons", "Show icons", "Render icon widgets for KO states", &PluginConfig::showMarkerIcons },
    { "show_text", "Show text", "Render short state labels such as ZZ and DY", &PluginConfig::showMarkerText },
    { "show_bounty_glow", "Show bounty glow", "Render bounty glow behind eligible targets", &PluginConfig::showBountyGlow },
    { "show_bounty_symbol", "Show bounty symbol", "Render bounty symbol text using tier colors", &PluginConfig::showBountySymbol },
    { "enable_character_tint", "Enable character tint", "Tint resolved characters in addition to marker overlays", &PluginConfig::enableCharacterTint },
    { "show_bounty_symbol_on_all_characters", "Show live bounty symbol", "Render bounty symbols for non-downed on-screen bounty targets", &PluginConfig::showBountySymbolOnAllCharacters }
};

HubKeybindSettingDescriptor g_keybindSettingDescriptors[] = {
    { "highlight_key", "Highlight key", "Primary key that gates KO highlights. Clear to Unbound for always-on highlights.", &PluginConfig::highlightKeyCode }
};

HubIntSettingDescriptor g_intSettingDescriptors[] = {
    { "max_highlight_distance_m", "Max highlight distance", "Maximum horizontal distance from camera center for highlights", &PluginConfig::maxHighlightDistanceMeters, 5, 20000, 100 },
    { "bounty_symbol_size_px", "Bounty symbol size", "Base font height for the bounty symbol", &PluginConfig::bountySymbolTextSizePx, 8, 128, 1 },
    { "unconscious_text_size_px", "Unconscious text size", "Font height for the unconscious label", &PluginConfig::unconsciousTextSizePx, 8, 128, 1 },
    { "recovery_coma_text_size_px", "Recovery coma text size", "Font height for the recovery coma label", &PluginConfig::recoveryComaTextSizePx, 8, 128, 1 },
    { "dying_text_size_px", "Dying text size", "Font height for the dying label", &PluginConfig::dyingTextSizePx, 8, 128, 1 },
    { "playing_dead_text_size_px", "Playing dead text size", "Font height for the playing dead label", &PluginConfig::playingDeadTextSizePx, 8, 128, 1 },
    { "dead_text_size_px", "Dead text size", "Font height for the dead label", &PluginConfig::deadTextSizePx, 8, 128, 1 }
};

HubColorSettingDescriptor g_colorSettingDescriptors[] = {
    { "enemy_color_hex", "Enemy color", "Relation color used for enemy markers and tint", &PluginConfig::enemyMarkerColour, EMC_COLOR_PREVIEW_KIND_TEXT, false },
    { "ally_color_hex", "Ally color", "Relation color used for ally markers and tint", &PluginConfig::allyMarkerColour, EMC_COLOR_PREVIEW_KIND_TEXT, false },
    { "squad_color_hex", "Squad color", "Relation color used for squad markers and tint", &PluginConfig::squadMarkerColour, EMC_COLOR_PREVIEW_KIND_TEXT, false },
    { "bounty_color_trivial_hex", "Bounty trivial color", "Bounty symbol text color for the trivial tier; Mod Hub edits RGB only", &PluginConfig::bountyTierTrivialColour, EMC_COLOR_PREVIEW_KIND_TEXT, true },
    { "bounty_color_low_hex", "Bounty low color", "Bounty symbol text color for the low tier; Mod Hub edits RGB only", &PluginConfig::bountyTierLowColour, EMC_COLOR_PREVIEW_KIND_TEXT, true },
    { "bounty_color_modest_hex", "Bounty modest color", "Bounty symbol text color for the modest tier; Mod Hub edits RGB only", &PluginConfig::bountyTierModestColour, EMC_COLOR_PREVIEW_KIND_TEXT, true },
    { "bounty_color_notable_hex", "Bounty notable color", "Bounty symbol text color for the notable tier; Mod Hub edits RGB only", &PluginConfig::bountyTierNotableColour, EMC_COLOR_PREVIEW_KIND_TEXT, true },
    { "bounty_color_high_value_hex", "Bounty high value color", "Bounty symbol text color for the high-value tier; Mod Hub edits RGB only", &PluginConfig::bountyTierHighValueColour, EMC_COLOR_PREVIEW_KIND_TEXT, true },
    { "bounty_color_elite_hex", "Bounty elite color", "Bounty symbol text color for the elite tier; Mod Hub edits RGB only", &PluginConfig::bountyTierEliteColour, EMC_COLOR_PREVIEW_KIND_TEXT, true },
    { "bounty_color_legendary_hex", "Bounty legendary color", "Bounty symbol text color for the legendary tier; Mod Hub edits RGB only", &PluginConfig::bountyTierLegendaryColour, EMC_COLOR_PREVIEW_KIND_TEXT, true }
};

enum
{
    kHubBoolSettingCount = static_cast<int>(VS_ARRAY_COUNT(g_boolSettingDescriptors)),
    kHubKeybindSettingCount = static_cast<int>(VS_ARRAY_COUNT(g_keybindSettingDescriptors)),
    kHubIntSettingCount = static_cast<int>(VS_ARRAY_COUNT(g_intSettingDescriptors)),
    kHubColorSettingCount = static_cast<int>(VS_ARRAY_COUNT(g_colorSettingDescriptors)),
    kHubRowCount = kHubBoolSettingCount + kHubKeybindSettingCount + kHubIntSettingCount + kHubColorSettingCount
};

EMC_BoolSettingDefV1 g_boolSettingDefs[kHubBoolSettingCount];
EMC_KeybindSettingDefV1 g_keybindSettingDefs[kHubKeybindSettingCount];
EMC_IntSettingDefV1 g_intSettingDefs[kHubIntSettingCount];
EMC_ColorSettingDefV1 g_colorSettingDefs[kHubColorSettingCount];
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

void InitializeSettingDefinitions()
{
    if (g_modHubRowsInitialized)
    {
        return;
    }

    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_boolSettingDescriptors); ++i)
    {
        g_boolSettingDefs[i].setting_id = g_boolSettingDescriptors[i].settingId;
        g_boolSettingDefs[i].label = g_boolSettingDescriptors[i].label;
        g_boolSettingDefs[i].description = g_boolSettingDescriptors[i].description;
        g_boolSettingDefs[i].user_data = &g_boolSettingDescriptors[i];
        g_boolSettingDefs[i].get_value = &GetBoolSettingValue;
        g_boolSettingDefs[i].set_value = &SetBoolSettingValue;
    }

    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_keybindSettingDescriptors); ++i)
    {
        g_keybindSettingDefs[i].setting_id = g_keybindSettingDescriptors[i].settingId;
        g_keybindSettingDefs[i].label = g_keybindSettingDescriptors[i].label;
        g_keybindSettingDefs[i].description = g_keybindSettingDescriptors[i].description;
        g_keybindSettingDefs[i].user_data = &g_keybindSettingDescriptors[i];
        g_keybindSettingDefs[i].get_value = &GetKeybindSettingValue;
        g_keybindSettingDefs[i].set_value = &SetKeybindSettingValue;
    }

    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_intSettingDescriptors); ++i)
    {
        g_intSettingDefs[i].setting_id = g_intSettingDescriptors[i].settingId;
        g_intSettingDefs[i].label = g_intSettingDescriptors[i].label;
        g_intSettingDefs[i].description = g_intSettingDescriptors[i].description;
        g_intSettingDefs[i].user_data = &g_intSettingDescriptors[i];
        g_intSettingDefs[i].min_value = g_intSettingDescriptors[i].minValue;
        g_intSettingDefs[i].max_value = g_intSettingDescriptors[i].maxValue;
        g_intSettingDefs[i].step = g_intSettingDescriptors[i].step;
        g_intSettingDefs[i].get_value = &GetIntSettingValue;
        g_intSettingDefs[i].set_value = &SetIntSettingValue;
    }

    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_colorSettingDescriptors); ++i)
    {
        g_colorSettingDefs[i].setting_id = g_colorSettingDescriptors[i].settingId;
        g_colorSettingDefs[i].label = g_colorSettingDescriptors[i].label;
        g_colorSettingDefs[i].description = g_colorSettingDescriptors[i].description;
        g_colorSettingDefs[i].user_data = &g_colorSettingDescriptors[i];
        g_colorSettingDefs[i].preview_kind = g_colorSettingDescriptors[i].previewKind;
        g_colorSettingDefs[i].presets = 0;
        g_colorSettingDefs[i].preset_count = 0u;
        g_colorSettingDefs[i].get_value = &GetColorSettingValue;
        g_colorSettingDefs[i].set_value = &SetColorSettingValue;
    }

    size_t rowIndex = 0u;
    g_modHubRows[rowIndex].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL;
    g_modHubRows[rowIndex].def = &g_boolSettingDefs[0];
    ++rowIndex;

    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_keybindSettingDescriptors); ++i)
    {
        g_modHubRows[rowIndex].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND;
        g_modHubRows[rowIndex].def = &g_keybindSettingDefs[i];
        ++rowIndex;
    }

    for (size_t i = 1u; i < VS_ARRAY_COUNT(g_boolSettingDescriptors); ++i)
    {
        g_modHubRows[rowIndex].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL;
        g_modHubRows[rowIndex].def = &g_boolSettingDefs[i];
        ++rowIndex;
    }

    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_intSettingDescriptors); ++i)
    {
        g_modHubRows[rowIndex].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_INT;
        g_modHubRows[rowIndex].def = &g_intSettingDefs[i];
        ++rowIndex;
    }

    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_colorSettingDescriptors); ++i)
    {
        g_modHubRows[rowIndex].kind = emc::MOD_HUB_CLIENT_SETTING_KIND_COLOR;
        g_modHubRows[rowIndex].def = &g_colorSettingDefs[i];
        ++rowIndex;
    }

    g_modHubRegistration.row_count = static_cast<uint32_t>(rowIndex);
    g_modHubRowsInitialized = true;
}

bool TryParseRgbColorHex(const char* raw_value, MyGUI::Colour* out_value)
{
    if (raw_value == 0 || out_value == 0)
    {
        return false;
    }

    std::string value = vs_parse::TrimAscii(raw_value);
    if (value.empty())
    {
        return false;
    }

    if (!value.empty() && value[0] == '#')
    {
        value.erase(0, 1);
    }

    if (value.size() != 6u)
    {
        return false;
    }

    return vs_parse::TryParseColourHex(value, out_value);
}

EMC_Result ApplyHubConfigUpdate(const PluginConfig& updated, char* err_buf, uint32_t err_buf_size)
{
    RuntimeStateView state = vs_runtime_state::GetRuntimeStateView();
    const PluginConfig previous = state.config;
    state.config = updated;

    if (!vs_config::SaveConfigState(state, kPluginName))
    {
        state.config = previous;
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "persist_failed");
        return EMC_ERR_INTERNAL;
    }

    state.lastProbeTickMs = 0;

    if (!updated.enabled)
    {
        state.koTargetCache.clear();
        state.visibleKoHandlesScratch.clear();
        state.highlightRuntimeActive = false;
    }

    if (!updated.enabled || (!updated.showMarkerIcons && !updated.showMarkerText && !updated.showBountySymbol))
    {
        vs_marker_render::HideAllKoMarkerWidgets(state, kPluginName);
    }

    if (!updated.enabled || !updated.enableCharacterTint)
    {
        vs_character_tint::ClearKoCharacterTint(state, kPluginName);
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
    RuntimeStateView state = vs_runtime_state::GetRuntimeStateView();
    *out_value = (state.config.*(descriptor->field)) ? 1 : 0;
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
    RuntimeStateView state = vs_runtime_state::GetRuntimeStateView();
    PluginConfig updated = state.config;
    updated.*(descriptor->field) = value != 0;
    return ApplyHubConfigUpdate(updated, err_buf, err_buf_size);
}

EMC_Result __cdecl GetIntSettingValue(void* user_data, int32_t* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HubIntSettingDescriptor* descriptor = static_cast<const HubIntSettingDescriptor*>(user_data);
    RuntimeStateView state = vs_runtime_state::GetRuntimeStateView();
    *out_value = static_cast<int32_t>(state.config.*(descriptor->field));
    return EMC_OK;
}

EMC_Result __cdecl SetIntSettingValue(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    if (user_data == 0)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_setting_context");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HubIntSettingDescriptor* descriptor = static_cast<const HubIntSettingDescriptor*>(user_data);
    if (value < descriptor->minValue || value > descriptor->maxValue)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "value_out_of_range");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    RuntimeStateView state = vs_runtime_state::GetRuntimeStateView();
    PluginConfig updated = state.config;
    updated.*(descriptor->field) = static_cast<DWORD>(value);

    return ApplyHubConfigUpdate(updated, err_buf, err_buf_size);
}

EMC_Result __cdecl GetKeybindSettingValue(void* user_data, EMC_KeybindValueV1* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HubKeybindSettingDescriptor* descriptor = static_cast<const HubKeybindSettingDescriptor*>(user_data);
    RuntimeStateView state = vs_runtime_state::GetRuntimeStateView();
    out_value->keycode = state.config.*(descriptor->field);
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
    if (!vs_keybind::ValidatePrimaryKeyCode(value.keycode, &validationReason))
    {
        emc::consumer::WriteErrorMessage(
            err_buf,
            err_buf_size,
            validationReason.empty() ? "invalid_keybind" : validationReason.c_str());
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HubKeybindSettingDescriptor* descriptor = static_cast<const HubKeybindSettingDescriptor*>(user_data);
    RuntimeStateView state = vs_runtime_state::GetRuntimeStateView();
    PluginConfig updated = state.config;
    updated.*(descriptor->field) = value.keycode;
    return ApplyHubConfigUpdate(updated, err_buf, err_buf_size);
}

EMC_Result __cdecl GetColorSettingValue(void* user_data, char* out_value, uint32_t out_value_size)
{
    if (user_data == 0 || out_value == 0 || out_value_size == 0u)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HubColorSettingDescriptor* descriptor = static_cast<const HubColorSettingDescriptor*>(user_data);
    RuntimeStateView state = vs_runtime_state::GetRuntimeStateView();
    const std::string value = vs_parse::ColourToHexRgb(state.config.*(descriptor->field));
    if (value.size() + 1u > out_value_size)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    std::memcpy(out_value, value.c_str(), value.size() + 1u);
    return EMC_OK;
}

EMC_Result __cdecl SetColorSettingValue(void* user_data, const char* value, char* err_buf, uint32_t err_buf_size)
{
    if (user_data == 0)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_setting_context");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    MyGUI::Colour parsed;
    if (!TryParseRgbColorHex(value, &parsed))
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "expected_rgb_hex");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HubColorSettingDescriptor* descriptor = static_cast<const HubColorSettingDescriptor*>(user_data);
    RuntimeStateView state = vs_runtime_state::GetRuntimeStateView();
    PluginConfig updated = state.config;
    const float alpha = descriptor->preserveAlpha ? (state.config.*(descriptor->field)).alpha : 1.0f;
    updated.*(descriptor->field) = MyGUI::Colour(parsed.red, parsed.green, parsed.blue, alpha);
    return ApplyHubConfigUpdate(updated, err_buf, err_buf_size);
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
        vs_log::LogInfo(kPluginName, line.str());
        return;
    }

    vs_log::LogWarn(kPluginName, line.str());
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

#undef VS_ARRAY_COUNT
} // namespace

void OnStartup()
{
    EnsureModHubClientConfigured();

    const emc::ModHubClient::AttemptResult result = g_modHubClient.OnStartup();
    if (result == emc::ModHubClient::ATTACH_SUCCESS)
    {
        vs_log::LogInfo(kPluginName, "event=mod_hub_attached use_hub_ui=1");
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

} // namespace vs_mod_hub
