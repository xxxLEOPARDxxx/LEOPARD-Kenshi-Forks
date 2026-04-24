#include "vs_mod_hub.h"

#include "emc/mod_hub_client.h"
#include "emc/mod_hub_consumer_helpers.h"

#include "vs_character_tint.h"
#include "vs_config.h"
#include "vs_keybind.h"
#include "vs_log.h"
#include "vs_marker_render.h"
#include "vs_parse.h"
#include "vs_probe_cache.h"
#include "vs_mod_hub_helpers.h"
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
    const char* hoverHint;
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
    const char* hoverHint;
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

struct HubSelectSettingDescriptor
{
    const char* settingId;
    const char* label;
    const char* description;
    const char* hoverHint;
    bool PluginConfig::*field;
    const EMC_SelectOptionV1* options;
    uint32_t optionCount;
};

struct HubTextSettingDescriptor
{
    const char* settingId;
    const char* label;
    const char* description;
    const char* hoverHint;
    std::string PluginConfig::*field;
    uint32_t maxLength;
    bool allowEmpty;
};

EMC_Result __cdecl GetBoolSettingValue(void* user_data, int32_t* out_value);
EMC_Result __cdecl SetBoolSettingValue(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size);
EMC_Result __cdecl GetIntSettingValue(void* user_data, int32_t* out_value);
EMC_Result __cdecl SetIntSettingValue(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size);
EMC_Result __cdecl GetKeybindSettingValue(void* user_data, EMC_KeybindValueV1* out_value);
EMC_Result __cdecl SetKeybindSettingValue(void* user_data, EMC_KeybindValueV1 value, char* err_buf, uint32_t err_buf_size);
EMC_Result __cdecl GetColorSettingValue(void* user_data, char* out_value, uint32_t out_value_size);
EMC_Result __cdecl SetColorSettingValue(void* user_data, const char* value, char* err_buf, uint32_t err_buf_size);
EMC_Result __cdecl GetSelectSettingValue(void* user_data, int32_t* out_value);
EMC_Result __cdecl SetSelectSettingValue(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size);
EMC_Result __cdecl GetTextSettingValue(void* user_data, char* out_value, uint32_t out_value_size);
EMC_Result __cdecl SetTextSettingValue(void* user_data, const char* value, char* err_buf, uint32_t err_buf_size);
EMC_Result __cdecl RegisterModHubSettings(const EMC_HubApiV1* api, void* user_data);

emc::ModHubClient g_modHubClient;
bool g_modHubClientConfigured = false;
bool g_modHubRowsInitialized = false;

HubBoolSettingDescriptor g_boolSettingDescriptors[] = {
    { "enabled", "Enabled", "Enable all Vital Sense overlays and tint behavior", "Turn all Vital Sense overlays and tint effects on or off.", &PluginConfig::enabled },
    { "highlight_key_require_ctrl", "Require Ctrl", "Require Ctrl to be held with the highlight key", 0, &PluginConfig::highlightKeyRequireCtrl },
    { "highlight_key_require_shift", "Require Shift", "Require Shift to be held with the highlight key", 0, &PluginConfig::highlightKeyRequireShift },
    { "highlight_key_require_alt", "Require Alt", "Require Alt to be held with the highlight key", 0, &PluginConfig::highlightKeyRequireAlt },
    { "enable_unconscious", "Show unconscious", "Show markers for unconscious targets", 0, &PluginConfig::enableUnconsciousState },
    { "enable_recovery_coma", "Show recovery coma", "Show markers for recovery coma targets", 0, &PluginConfig::enableRecoveryComaState },
    { "enable_dying", "Show dying", "Show markers for dying targets", 0, &PluginConfig::enableDyingState },
    { "enable_playing_dead", "Show playing dead", "Show markers for playing dead targets", 0, &PluginConfig::enablePlayingDeadState },
    { "enable_dead", "Show dead", "Show markers for dead targets", 0, &PluginConfig::enableDeadState },
    { "show_icons", "Show icons", "Render icon widgets for KO states", 0, &PluginConfig::showMarkerIcons },
    { "show_text", "Show text", "Render short state labels such as ZZ and DY", 0, &PluginConfig::showMarkerText },
    { "show_bounty_glow", "Show bounty glow", "Render bounty glow behind eligible targets", 0, &PluginConfig::showBountyGlow },
    { "show_bounty_symbol", "Show bounty symbol", "Render bounty symbol text using tier colors", "Show or hide the bounty tier symbol next to qualifying targets.", &PluginConfig::showBountySymbol },
    { "enable_character_tint", "Enable character tint", "Tint resolved characters in addition to marker overlays", "Apply Vital Sense relation colors directly to resolved characters.", &PluginConfig::enableCharacterTint },
    { "show_bounty_symbol_on_all_characters", "Show live bounty symbol", "Render bounty symbols for non-downed on-screen bounty targets", "Extend bounty symbols to awake on-screen characters, not just downed targets.", &PluginConfig::showBountySymbolOnAllCharacters }
};

HubKeybindSettingDescriptor g_keybindSettingDescriptors[] = {
    {
        "highlight_key",
        "Highlight key",
        "Primary key that gates KO highlights. Clear to Unbound for always-on highlights.",
        "Capture the primary key for KO highlighting. Modifier requirements stay on the toggle rows below.",
        &PluginConfig::highlightKeyCode
    }
};

HubIntSettingDescriptor g_intSettingDescriptors[] = {
    { "update_interval_ms", "Update interval", "KO target refresh interval in milliseconds", &PluginConfig::updateIntervalMs, 50, 2000, 1 },
    { "max_highlight_distance_m", "Max highlight distance", "Maximum horizontal distance from camera center for highlights", &PluginConfig::maxHighlightDistanceMeters, 5, 20000, 100 },
    { "bounty_symbol_size_px", "Bounty symbol size", "Base font height for the bounty symbol", &PluginConfig::bountySymbolTextSizePx, 8, 128, 1 },
    { "unconscious_text_size_px", "Unconscious text size", "Font height for the unconscious label", &PluginConfig::unconsciousTextSizePx, 8, 128, 1 },
    { "recovery_coma_text_size_px", "Recovery coma text size", "Font height for the recovery coma label", &PluginConfig::recoveryComaTextSizePx, 8, 128, 1 },
    { "dying_text_size_px", "Dying text size", "Font height for the dying label", &PluginConfig::dyingTextSizePx, 8, 128, 1 },
    { "playing_dead_text_size_px", "Playing dead text size", "Font height for the playing dead label", &PluginConfig::playingDeadTextSizePx, 8, 128, 1 },
    { "dead_text_size_px", "Dead text size", "Font height for the dead label", &PluginConfig::deadTextSizePx, 8, 128, 1 }
};

HubBoolSettingDescriptor g_advancedBoolSettingDescriptors[] = {
    { "debug_log_diagnostics", "Debug diagnostics", "Enable extra runtime diagnostics", "Write additional runtime diagnostics to the log.", &PluginConfig::debugLogDiagnostics },
    { "debug_log_texture_info", "Debug texture info", "Log custom icon texture resolution details", "Write extra icon texture lookup details to the log.", &PluginConfig::debugLogTextureInfo }
};

HubBoolSettingDescriptor g_tintBoolSettingDescriptors[] = {
    { "character_tint_include_squad", "Tint squad", "Tint conscious squadmates when character tint is enabled", "Include conscious squadmates in the tint pass when character tint is enabled.", &PluginConfig::characterTintIncludeSquad },
    { "character_tint_include_bounty_only", "Tint bounty-only", "Tint non-downed bounty-only targets when character tint is enabled", "Also tint awake bounty-only targets when character tint is enabled.", &PluginConfig::characterTintIncludeBountyOnly },
    { "character_tint_force_depth_override", "Force depth override", "Force tint to render through depth when supported by the shader", "Keep tint visible through world depth when the active shader path supports it.", &PluginConfig::characterTintForceDepthOverride }
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

HubIntSettingDescriptor g_stateIconSizeSettingDescriptors[] = {
    { "unconscious_icon_size_px", "Unconscious icon size", "Pixel size for the unconscious icon", &PluginConfig::customUnconsciousIconSizePx, 8, 512, 1 },
    { "recovery_coma_icon_size_px", "Recovery coma icon size", "Pixel size for the recovery coma icon", &PluginConfig::customRecoveryComaIconSizePx, 8, 512, 1 },
    { "dying_icon_size_px", "Dying icon size", "Pixel size for the dying icon", &PluginConfig::customDyingIconSizePx, 8, 512, 1 },
    { "playing_dead_icon_size_px", "Playing dead icon size", "Pixel size for the playing dead icon", &PluginConfig::customPlayingDeadIconSizePx, 8, 512, 1 },
    { "dead_icon_size_px", "Dead icon size", "Pixel size for the dead icon", &PluginConfig::customDeadIconSizePx, 8, 512, 1 }
};

HubIntSettingDescriptor g_bountyIntSettingDescriptors[] = {
    { "bounty_symbol_live_anchor_y_offset_cm", "Live symbol anchor offset", "Vertical world-anchor offset for live bounty symbols", &PluginConfig::bountySymbolLiveAnchorYOffsetCm, 0, 20000, 10 },
    { "bounty_tier_trivial_max", "Bounty trivial max", "Upper bound for the trivial bounty tier", &PluginConfig::bountyTierTrivialMax, 0, 1000000, 1 },
    { "bounty_tier_low_max", "Bounty low max", "Upper bound for the low bounty tier", &PluginConfig::bountyTierLowMax, 0, 1000000, 1 },
    { "bounty_tier_modest_max", "Bounty modest max", "Upper bound for the modest bounty tier", &PluginConfig::bountyTierModestMax, 0, 1000000, 1 },
    { "bounty_tier_notable_max", "Bounty notable max", "Upper bound for the notable bounty tier", &PluginConfig::bountyTierNotableMax, 0, 1000000, 1 },
    { "bounty_tier_high_value_max", "Bounty high value max", "Upper bound for the high-value bounty tier", &PluginConfig::bountyTierHighValueMax, 0, 1000000, 1 },
    { "bounty_tier_elite_max", "Bounty elite max", "Upper bound for the elite bounty tier", &PluginConfig::bountyTierEliteMax, 0, 1000000, 1 }
};

const int32_t kBountySymbolPositionBeforeStateIcon = 0;
const int32_t kBountySymbolPositionBeforeStateText = 1;
const uint32_t kHubShortTextMaxLength = 32u;
const uint32_t kHubPathTextMaxLength = 128u;
const char* kHubSectionCoreId = "core";
const char* kHubSectionCoreLabel = "Core";
const char* kHubSectionStatesId = "states";
const char* kHubSectionStatesLabel = "States";
const char* kHubSectionBountyId = "bounty";
const char* kHubSectionBountyLabel = "Bounty";
const char* kHubSectionTintId = "tint_relation_colors";
const char* kHubSectionTintLabel = "Tint & relation colors";
const char* kHubSectionAdvancedId = "advanced";
const char* kHubSectionAdvancedLabel = "Advanced";

const EMC_SelectOptionV1 g_bountySymbolPositionOptions[] = {
    { kBountySymbolPositionBeforeStateIcon, "Before state icon" },
    { kBountySymbolPositionBeforeStateText, "Before state text" }
};

HubSelectSettingDescriptor g_selectSettingDescriptors[] = {
    {
        "bounty_symbol_position",
        "Bounty symbol position",
        "Choose whether the bounty symbol renders before the state icon or before the state text",
        "Choose whether the bounty symbol is placed before the icon or before the text label.",
        &PluginConfig::placeBountySymbolBeforeStateIcon,
        g_bountySymbolPositionOptions,
        static_cast<uint32_t>(VS_ARRAY_COUNT(g_bountySymbolPositionOptions))
    }
};

HubTextSettingDescriptor g_textSettingDescriptors[] = {
    {
        "bounty_symbol",
        "Bounty symbol",
        "Short bounty symbol text such as $ or B",
        "Edit the short symbol shown for bounty targets, such as $ or B.",
        &PluginConfig::bountySymbolText,
        kHubShortTextMaxLength,
        false
    },
    {
        "unconscious_text",
        "Unconscious text",
        "Short label for the unconscious marker",
        "Edit the short marker label shown for unconscious targets.",
        &PluginConfig::unconsciousText,
        kHubShortTextMaxLength,
        false
    },
    {
        "recovery_coma_text",
        "Recovery coma text",
        "Short label for the recovery coma marker",
        "Edit the short marker label shown for recovery coma targets.",
        &PluginConfig::recoveryComaText,
        kHubShortTextMaxLength,
        false
    },
    {
        "dying_text",
        "Dying text",
        "Short label for the dying marker",
        "Edit the short marker label shown for dying targets.",
        &PluginConfig::dyingText,
        kHubShortTextMaxLength,
        false
    },
    {
        "playing_dead_text",
        "Playing dead text",
        "Short label for the playing dead marker",
        "Edit the short marker label shown for playing dead targets.",
        &PluginConfig::playingDeadText,
        kHubShortTextMaxLength,
        false
    },
    {
        "dead_text",
        "Dead text",
        "Short label for the dead marker",
        "Edit the short marker label shown for dead targets.",
        &PluginConfig::deadText,
        kHubShortTextMaxLength,
        false
    }
};

HubTextSettingDescriptor g_stateIconTextureSettingDescriptors[] = {
    {
        "unconscious_icon_texture",
        "Unconscious icon texture",
        "Short path for the unconscious icon texture",
        "Edit the icon texture used for unconscious targets.",
        &PluginConfig::customUnconsciousIconTexture,
        kHubPathTextMaxLength,
        true
    },
    {
        "recovery_coma_icon_texture",
        "Recovery coma icon texture",
        "Short path for the recovery coma icon texture",
        "Edit the icon texture used for recovery coma targets.",
        &PluginConfig::customRecoveryComaIconTexture,
        kHubPathTextMaxLength,
        true
    },
    {
        "dying_icon_texture",
        "Dying icon texture",
        "Short path for the dying icon texture",
        "Edit the icon texture used for dying targets.",
        &PluginConfig::customDyingIconTexture,
        kHubPathTextMaxLength,
        true
    },
    {
        "playing_dead_icon_texture",
        "Playing dead icon texture",
        "Short path for the playing dead icon texture",
        "Edit the icon texture used for playing dead targets.",
        &PluginConfig::customPlayingDeadIconTexture,
        kHubPathTextMaxLength,
        true
    },
    {
        "dead_icon_texture",
        "Dead icon texture",
        "Short path for the dead icon texture",
        "Edit the icon texture used for dead targets.",
        &PluginConfig::customDeadIconTexture,
        kHubPathTextMaxLength,
        true
    }
};

enum
{
    kHubBoolSettingCount = static_cast<int>(VS_ARRAY_COUNT(g_boolSettingDescriptors)),
    kHubKeybindSettingCount = static_cast<int>(VS_ARRAY_COUNT(g_keybindSettingDescriptors)),
    kHubIntSettingCount = static_cast<int>(VS_ARRAY_COUNT(g_intSettingDescriptors)),
    kHubAdvancedBoolSettingCount = static_cast<int>(VS_ARRAY_COUNT(g_advancedBoolSettingDescriptors)),
    kHubTintBoolSettingCount = static_cast<int>(VS_ARRAY_COUNT(g_tintBoolSettingDescriptors)),
    kHubSelectSettingCount = static_cast<int>(VS_ARRAY_COUNT(g_selectSettingDescriptors)),
    kHubTextSettingCount = static_cast<int>(VS_ARRAY_COUNT(g_textSettingDescriptors)),
    kHubStateIconTextureSettingCount = static_cast<int>(VS_ARRAY_COUNT(g_stateIconTextureSettingDescriptors)),
    kHubStateIconSizeSettingCount = static_cast<int>(VS_ARRAY_COUNT(g_stateIconSizeSettingDescriptors)),
    kHubBountyIntSettingCount = static_cast<int>(VS_ARRAY_COUNT(g_bountyIntSettingDescriptors)),
    kHubColorSettingCount = static_cast<int>(VS_ARRAY_COUNT(g_colorSettingDescriptors)),
    kHubRowCount = kHubBoolSettingCount + kHubKeybindSettingCount + kHubIntSettingCount
        + kHubAdvancedBoolSettingCount + kHubTintBoolSettingCount + kHubSelectSettingCount
        + kHubTextSettingCount + kHubStateIconTextureSettingCount + kHubStateIconSizeSettingCount
        + kHubBountyIntSettingCount + kHubColorSettingCount
};

EMC_BoolSettingDefV1 g_boolSettingDefs[kHubBoolSettingCount];
EMC_BoolSettingDefV2 g_boolSettingDefsV2[kHubBoolSettingCount];
EMC_KeybindSettingDefV1 g_keybindSettingDefs[kHubKeybindSettingCount];
EMC_KeybindSettingDefV2 g_keybindSettingDefsV2[kHubKeybindSettingCount];
EMC_IntSettingDefV1 g_intSettingDefs[kHubIntSettingCount];
EMC_BoolSettingDefV1 g_advancedBoolSettingDefs[kHubAdvancedBoolSettingCount];
EMC_BoolSettingDefV2 g_advancedBoolSettingDefsV2[kHubAdvancedBoolSettingCount];
EMC_BoolSettingDefV1 g_tintBoolSettingDefs[kHubTintBoolSettingCount];
EMC_BoolSettingDefV2 g_tintBoolSettingDefsV2[kHubTintBoolSettingCount];
EMC_SelectSettingDefV1 g_selectSettingDefs[kHubSelectSettingCount];
EMC_SelectSettingDefV2 g_selectSettingDefsV2[kHubSelectSettingCount];
EMC_TextSettingDefV1 g_textSettingDefs[kHubTextSettingCount];
EMC_TextSettingDefV2 g_textSettingDefsV2[kHubTextSettingCount];
EMC_TextSettingDefV1 g_stateIconTextureSettingDefs[kHubStateIconTextureSettingCount];
EMC_TextSettingDefV2 g_stateIconTextureSettingDefsV2[kHubStateIconTextureSettingCount];
EMC_IntSettingDefV1 g_stateIconSizeSettingDefs[kHubStateIconSizeSettingCount];
EMC_IntSettingDefV1 g_bountyIntSettingDefs[kHubBountyIntSettingCount];
EMC_ColorSettingDefV1 g_colorSettingDefs[kHubColorSettingCount];
emc::ModHubClientSettingRowV1 g_modHubRows[kHubRowCount];
emc::ModHubClientSettingRowV1 g_modHubRowsV2[kHubRowCount];

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
emc::ModHubClientTableRegistrationV1 g_modHubRegistrationV2 = {
    &kModHubDescriptor,
    g_modHubRowsV2,
    0u
};

void SetHubRow(
    emc::ModHubClientSettingRowV1* rows,
    size_t row_index,
    int32_t kind,
    const char* setting_id,
    const void* def,
    const char* section_id,
    const char* section_display_name)
{
    rows[row_index].kind = kind;
    rows[row_index].setting_id = setting_id;
    rows[row_index].def = def;
    rows[row_index].section_id = section_id;
    rows[row_index].section_display_name = section_display_name;
}

void SetHubRows(
    size_t row_index,
    int32_t kind_v1,
    const char* setting_id,
    const void* def_v1,
    int32_t kind_v2,
    const void* def_v2,
    const char* section_id,
    const char* section_display_name)
{
    SetHubRow(g_modHubRows, row_index, kind_v1, setting_id, def_v1, section_id, section_display_name);
    SetHubRow(g_modHubRowsV2, row_index, kind_v2, setting_id, def_v2, section_id, section_display_name);
}

bool SupportsHoverHintSettingRows(const EMC_HubApiV1* api)
{
    if (api == 0)
    {
        return false;
    }

    const uint32_t api_size = api->api_size;
    return api_size >= EMC_HUB_API_V1_TEXT_SETTING_V2_MIN_SIZE
        && api->register_bool_setting_v2 != 0
        && api->register_keybind_setting_v2 != 0
        && api->register_select_setting_v2 != 0
        && api->register_text_setting_v2 != 0;
}

const emc::ModHubClientTableRegistrationV1* GetModHubRegistrationForApi(const EMC_HubApiV1* api)
{
    return SupportsHoverHintSettingRows(api) ? &g_modHubRegistrationV2 : &g_modHubRegistration;
}

bool IsBountyTierOrderValid(const PluginConfig& config)
{
    return config.bountyTierTrivialMax < config.bountyTierLowMax
        && config.bountyTierLowMax < config.bountyTierModestMax
        && config.bountyTierModestMax < config.bountyTierNotableMax
        && config.bountyTierNotableMax < config.bountyTierHighValueMax
        && config.bountyTierHighValueMax < config.bountyTierEliteMax;
}

bool IsBountyTierSetting(const HubIntSettingDescriptor* descriptor)
{
    return descriptor != 0
        && descriptor->settingId != 0
        && std::strncmp(descriptor->settingId, "bounty_tier_", 12u) == 0;
}

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

        g_boolSettingDefsV2[i].setting_id = g_boolSettingDescriptors[i].settingId;
        g_boolSettingDefsV2[i].label = g_boolSettingDescriptors[i].label;
        g_boolSettingDefsV2[i].description = g_boolSettingDescriptors[i].description;
        g_boolSettingDefsV2[i].user_data = &g_boolSettingDescriptors[i];
        g_boolSettingDefsV2[i].get_value = &GetBoolSettingValue;
        g_boolSettingDefsV2[i].set_value = &SetBoolSettingValue;
        g_boolSettingDefsV2[i].hover_hint = g_boolSettingDescriptors[i].hoverHint;
    }

    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_keybindSettingDescriptors); ++i)
    {
        g_keybindSettingDefs[i].setting_id = g_keybindSettingDescriptors[i].settingId;
        g_keybindSettingDefs[i].label = g_keybindSettingDescriptors[i].label;
        g_keybindSettingDefs[i].description = g_keybindSettingDescriptors[i].description;
        g_keybindSettingDefs[i].user_data = &g_keybindSettingDescriptors[i];
        g_keybindSettingDefs[i].get_value = &GetKeybindSettingValue;
        g_keybindSettingDefs[i].set_value = &SetKeybindSettingValue;

        g_keybindSettingDefsV2[i].setting_id = g_keybindSettingDescriptors[i].settingId;
        g_keybindSettingDefsV2[i].label = g_keybindSettingDescriptors[i].label;
        g_keybindSettingDefsV2[i].description = g_keybindSettingDescriptors[i].description;
        g_keybindSettingDefsV2[i].user_data = &g_keybindSettingDescriptors[i];
        g_keybindSettingDefsV2[i].get_value = &GetKeybindSettingValue;
        g_keybindSettingDefsV2[i].set_value = &SetKeybindSettingValue;
        g_keybindSettingDefsV2[i].hover_hint = g_keybindSettingDescriptors[i].hoverHint;
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

    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_advancedBoolSettingDescriptors); ++i)
    {
        g_advancedBoolSettingDefs[i].setting_id = g_advancedBoolSettingDescriptors[i].settingId;
        g_advancedBoolSettingDefs[i].label = g_advancedBoolSettingDescriptors[i].label;
        g_advancedBoolSettingDefs[i].description = g_advancedBoolSettingDescriptors[i].description;
        g_advancedBoolSettingDefs[i].user_data = &g_advancedBoolSettingDescriptors[i];
        g_advancedBoolSettingDefs[i].get_value = &GetBoolSettingValue;
        g_advancedBoolSettingDefs[i].set_value = &SetBoolSettingValue;

        g_advancedBoolSettingDefsV2[i].setting_id = g_advancedBoolSettingDescriptors[i].settingId;
        g_advancedBoolSettingDefsV2[i].label = g_advancedBoolSettingDescriptors[i].label;
        g_advancedBoolSettingDefsV2[i].description = g_advancedBoolSettingDescriptors[i].description;
        g_advancedBoolSettingDefsV2[i].user_data = &g_advancedBoolSettingDescriptors[i];
        g_advancedBoolSettingDefsV2[i].get_value = &GetBoolSettingValue;
        g_advancedBoolSettingDefsV2[i].set_value = &SetBoolSettingValue;
        g_advancedBoolSettingDefsV2[i].hover_hint = g_advancedBoolSettingDescriptors[i].hoverHint;
    }

    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_tintBoolSettingDescriptors); ++i)
    {
        g_tintBoolSettingDefs[i].setting_id = g_tintBoolSettingDescriptors[i].settingId;
        g_tintBoolSettingDefs[i].label = g_tintBoolSettingDescriptors[i].label;
        g_tintBoolSettingDefs[i].description = g_tintBoolSettingDescriptors[i].description;
        g_tintBoolSettingDefs[i].user_data = &g_tintBoolSettingDescriptors[i];
        g_tintBoolSettingDefs[i].get_value = &GetBoolSettingValue;
        g_tintBoolSettingDefs[i].set_value = &SetBoolSettingValue;

        g_tintBoolSettingDefsV2[i].setting_id = g_tintBoolSettingDescriptors[i].settingId;
        g_tintBoolSettingDefsV2[i].label = g_tintBoolSettingDescriptors[i].label;
        g_tintBoolSettingDefsV2[i].description = g_tintBoolSettingDescriptors[i].description;
        g_tintBoolSettingDefsV2[i].user_data = &g_tintBoolSettingDescriptors[i];
        g_tintBoolSettingDefsV2[i].get_value = &GetBoolSettingValue;
        g_tintBoolSettingDefsV2[i].set_value = &SetBoolSettingValue;
        g_tintBoolSettingDefsV2[i].hover_hint = g_tintBoolSettingDescriptors[i].hoverHint;
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

    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_selectSettingDescriptors); ++i)
    {
        g_selectSettingDefs[i].setting_id = g_selectSettingDescriptors[i].settingId;
        g_selectSettingDefs[i].label = g_selectSettingDescriptors[i].label;
        g_selectSettingDefs[i].description = g_selectSettingDescriptors[i].description;
        g_selectSettingDefs[i].user_data = &g_selectSettingDescriptors[i];
        g_selectSettingDefs[i].options = g_selectSettingDescriptors[i].options;
        g_selectSettingDefs[i].option_count = g_selectSettingDescriptors[i].optionCount;
        g_selectSettingDefs[i].get_value = &GetSelectSettingValue;
        g_selectSettingDefs[i].set_value = &SetSelectSettingValue;

        g_selectSettingDefsV2[i].setting_id = g_selectSettingDescriptors[i].settingId;
        g_selectSettingDefsV2[i].label = g_selectSettingDescriptors[i].label;
        g_selectSettingDefsV2[i].description = g_selectSettingDescriptors[i].description;
        g_selectSettingDefsV2[i].user_data = &g_selectSettingDescriptors[i];
        g_selectSettingDefsV2[i].options = g_selectSettingDescriptors[i].options;
        g_selectSettingDefsV2[i].option_count = g_selectSettingDescriptors[i].optionCount;
        g_selectSettingDefsV2[i].get_value = &GetSelectSettingValue;
        g_selectSettingDefsV2[i].set_value = &SetSelectSettingValue;
        g_selectSettingDefsV2[i].hover_hint = g_selectSettingDescriptors[i].hoverHint;
    }

    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_textSettingDescriptors); ++i)
    {
        g_textSettingDefs[i].setting_id = g_textSettingDescriptors[i].settingId;
        g_textSettingDefs[i].label = g_textSettingDescriptors[i].label;
        g_textSettingDefs[i].description = g_textSettingDescriptors[i].description;
        g_textSettingDefs[i].user_data = &g_textSettingDescriptors[i];
        g_textSettingDefs[i].max_length = g_textSettingDescriptors[i].maxLength;
        g_textSettingDefs[i].get_value = &GetTextSettingValue;
        g_textSettingDefs[i].set_value = &SetTextSettingValue;

        g_textSettingDefsV2[i].setting_id = g_textSettingDescriptors[i].settingId;
        g_textSettingDefsV2[i].label = g_textSettingDescriptors[i].label;
        g_textSettingDefsV2[i].description = g_textSettingDescriptors[i].description;
        g_textSettingDefsV2[i].user_data = &g_textSettingDescriptors[i];
        g_textSettingDefsV2[i].max_length = g_textSettingDescriptors[i].maxLength;
        g_textSettingDefsV2[i].get_value = &GetTextSettingValue;
        g_textSettingDefsV2[i].set_value = &SetTextSettingValue;
        g_textSettingDefsV2[i].hover_hint = g_textSettingDescriptors[i].hoverHint;
    }

    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_stateIconTextureSettingDescriptors); ++i)
    {
        g_stateIconTextureSettingDefs[i].setting_id = g_stateIconTextureSettingDescriptors[i].settingId;
        g_stateIconTextureSettingDefs[i].label = g_stateIconTextureSettingDescriptors[i].label;
        g_stateIconTextureSettingDefs[i].description = g_stateIconTextureSettingDescriptors[i].description;
        g_stateIconTextureSettingDefs[i].user_data = &g_stateIconTextureSettingDescriptors[i];
        g_stateIconTextureSettingDefs[i].max_length = g_stateIconTextureSettingDescriptors[i].maxLength;
        g_stateIconTextureSettingDefs[i].get_value = &GetTextSettingValue;
        g_stateIconTextureSettingDefs[i].set_value = &SetTextSettingValue;

        g_stateIconTextureSettingDefsV2[i].setting_id = g_stateIconTextureSettingDescriptors[i].settingId;
        g_stateIconTextureSettingDefsV2[i].label = g_stateIconTextureSettingDescriptors[i].label;
        g_stateIconTextureSettingDefsV2[i].description = g_stateIconTextureSettingDescriptors[i].description;
        g_stateIconTextureSettingDefsV2[i].user_data = &g_stateIconTextureSettingDescriptors[i];
        g_stateIconTextureSettingDefsV2[i].max_length = g_stateIconTextureSettingDescriptors[i].maxLength;
        g_stateIconTextureSettingDefsV2[i].get_value = &GetTextSettingValue;
        g_stateIconTextureSettingDefsV2[i].set_value = &SetTextSettingValue;
        g_stateIconTextureSettingDefsV2[i].hover_hint = g_stateIconTextureSettingDescriptors[i].hoverHint;
    }

    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_stateIconSizeSettingDescriptors); ++i)
    {
        g_stateIconSizeSettingDefs[i].setting_id = g_stateIconSizeSettingDescriptors[i].settingId;
        g_stateIconSizeSettingDefs[i].label = g_stateIconSizeSettingDescriptors[i].label;
        g_stateIconSizeSettingDefs[i].description = g_stateIconSizeSettingDescriptors[i].description;
        g_stateIconSizeSettingDefs[i].user_data = &g_stateIconSizeSettingDescriptors[i];
        g_stateIconSizeSettingDefs[i].min_value = g_stateIconSizeSettingDescriptors[i].minValue;
        g_stateIconSizeSettingDefs[i].max_value = g_stateIconSizeSettingDescriptors[i].maxValue;
        g_stateIconSizeSettingDefs[i].step = g_stateIconSizeSettingDescriptors[i].step;
        g_stateIconSizeSettingDefs[i].get_value = &GetIntSettingValue;
        g_stateIconSizeSettingDefs[i].set_value = &SetIntSettingValue;
    }

    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_bountyIntSettingDescriptors); ++i)
    {
        g_bountyIntSettingDefs[i].setting_id = g_bountyIntSettingDescriptors[i].settingId;
        g_bountyIntSettingDefs[i].label = g_bountyIntSettingDescriptors[i].label;
        g_bountyIntSettingDefs[i].description = g_bountyIntSettingDescriptors[i].description;
        g_bountyIntSettingDefs[i].user_data = &g_bountyIntSettingDescriptors[i];
        g_bountyIntSettingDefs[i].min_value = g_bountyIntSettingDescriptors[i].minValue;
        g_bountyIntSettingDefs[i].max_value = g_bountyIntSettingDescriptors[i].maxValue;
        g_bountyIntSettingDefs[i].step = g_bountyIntSettingDescriptors[i].step;
        g_bountyIntSettingDefs[i].get_value = &GetIntSettingValue;
        g_bountyIntSettingDefs[i].set_value = &SetIntSettingValue;
    }

    size_t rowIndex = 0u;

    // Core section.
    SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, g_boolSettingDefs[0].setting_id, &g_boolSettingDefs[0], emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2, &g_boolSettingDefsV2[0], kHubSectionCoreId, kHubSectionCoreLabel);
    SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND, g_keybindSettingDefs[0].setting_id, &g_keybindSettingDefs[0], emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND_V2, &g_keybindSettingDefsV2[0], kHubSectionCoreId, kHubSectionCoreLabel);
    SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, g_boolSettingDefs[1].setting_id, &g_boolSettingDefs[1], emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2, &g_boolSettingDefsV2[1], kHubSectionCoreId, kHubSectionCoreLabel);
    SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, g_boolSettingDefs[2].setting_id, &g_boolSettingDefs[2], emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2, &g_boolSettingDefsV2[2], kHubSectionCoreId, kHubSectionCoreLabel);
    SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, g_boolSettingDefs[3].setting_id, &g_boolSettingDefs[3], emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2, &g_boolSettingDefsV2[3], kHubSectionCoreId, kHubSectionCoreLabel);
    SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_INT, g_intSettingDefs[1].setting_id, &g_intSettingDefs[1], emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &g_intSettingDefs[1], kHubSectionCoreId, kHubSectionCoreLabel);
    SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, g_boolSettingDefs[9].setting_id, &g_boolSettingDefs[9], emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2, &g_boolSettingDefsV2[9], kHubSectionCoreId, kHubSectionCoreLabel);
    SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, g_boolSettingDefs[10].setting_id, &g_boolSettingDefs[10], emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2, &g_boolSettingDefsV2[10], kHubSectionCoreId, kHubSectionCoreLabel);

    // States section.
    for (size_t i = 4u; i <= 8u; ++i)
    {
        SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, g_boolSettingDefs[i].setting_id, &g_boolSettingDefs[i], emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2, &g_boolSettingDefsV2[i], kHubSectionStatesId, kHubSectionStatesLabel);
    }
    for (size_t i = 1u; i < VS_ARRAY_COUNT(g_textSettingDescriptors); ++i)
    {
        SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_TEXT, g_textSettingDefs[i].setting_id, &g_textSettingDefs[i], emc::MOD_HUB_CLIENT_SETTING_KIND_TEXT_V2, &g_textSettingDefsV2[i], kHubSectionStatesId, kHubSectionStatesLabel);
    }
    for (size_t i = 3u; i < VS_ARRAY_COUNT(g_intSettingDescriptors); ++i)
    {
        SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_INT, g_intSettingDefs[i].setting_id, &g_intSettingDefs[i], emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &g_intSettingDefs[i], kHubSectionStatesId, kHubSectionStatesLabel);
    }
    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_stateIconTextureSettingDescriptors); ++i)
    {
        SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_TEXT, g_stateIconTextureSettingDefs[i].setting_id, &g_stateIconTextureSettingDefs[i], emc::MOD_HUB_CLIENT_SETTING_KIND_TEXT_V2, &g_stateIconTextureSettingDefsV2[i], kHubSectionStatesId, kHubSectionStatesLabel);
    }
    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_stateIconSizeSettingDefs); ++i)
    {
        SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_INT, g_stateIconSizeSettingDefs[i].setting_id, &g_stateIconSizeSettingDefs[i], emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &g_stateIconSizeSettingDefs[i], kHubSectionStatesId, kHubSectionStatesLabel);
    }

    // Bounty section.
    SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, g_boolSettingDefs[11].setting_id, &g_boolSettingDefs[11], emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2, &g_boolSettingDefsV2[11], kHubSectionBountyId, kHubSectionBountyLabel);
    SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, g_boolSettingDefs[12].setting_id, &g_boolSettingDefs[12], emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2, &g_boolSettingDefsV2[12], kHubSectionBountyId, kHubSectionBountyLabel);
    SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, g_boolSettingDefs[14].setting_id, &g_boolSettingDefs[14], emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2, &g_boolSettingDefsV2[14], kHubSectionBountyId, kHubSectionBountyLabel);
    SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_TEXT, g_textSettingDefs[0].setting_id, &g_textSettingDefs[0], emc::MOD_HUB_CLIENT_SETTING_KIND_TEXT_V2, &g_textSettingDefsV2[0], kHubSectionBountyId, kHubSectionBountyLabel);
    SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_INT, g_intSettingDefs[2].setting_id, &g_intSettingDefs[2], emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &g_intSettingDefs[2], kHubSectionBountyId, kHubSectionBountyLabel);
    SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_SELECT, g_selectSettingDefs[0].setting_id, &g_selectSettingDefs[0], emc::MOD_HUB_CLIENT_SETTING_KIND_SELECT_V2, &g_selectSettingDefsV2[0], kHubSectionBountyId, kHubSectionBountyLabel);
    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_bountyIntSettingDefs); ++i)
    {
        SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_INT, g_bountyIntSettingDefs[i].setting_id, &g_bountyIntSettingDefs[i], emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &g_bountyIntSettingDefs[i], kHubSectionBountyId, kHubSectionBountyLabel);
    }
    for (size_t i = 3u; i < VS_ARRAY_COUNT(g_colorSettingDefs); ++i)
    {
        SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_COLOR, g_colorSettingDefs[i].setting_id, &g_colorSettingDefs[i], emc::MOD_HUB_CLIENT_SETTING_KIND_COLOR, &g_colorSettingDefs[i], kHubSectionBountyId, kHubSectionBountyLabel);
    }

    // Tint & relation colors section.
    SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, g_boolSettingDefs[13].setting_id, &g_boolSettingDefs[13], emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2, &g_boolSettingDefsV2[13], kHubSectionTintId, kHubSectionTintLabel);
    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_tintBoolSettingDefs); ++i)
    {
        SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, g_tintBoolSettingDefs[i].setting_id, &g_tintBoolSettingDefs[i], emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2, &g_tintBoolSettingDefsV2[i], kHubSectionTintId, kHubSectionTintLabel);
    }
    for (size_t i = 0u; i < 3u; ++i)
    {
        SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_COLOR, g_colorSettingDefs[i].setting_id, &g_colorSettingDefs[i], emc::MOD_HUB_CLIENT_SETTING_KIND_COLOR, &g_colorSettingDefs[i], kHubSectionTintId, kHubSectionTintLabel);
    }

    // Advanced section.
    SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_INT, g_intSettingDefs[0].setting_id, &g_intSettingDefs[0], emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &g_intSettingDefs[0], kHubSectionAdvancedId, kHubSectionAdvancedLabel);
    for (size_t i = 0u; i < VS_ARRAY_COUNT(g_advancedBoolSettingDefs); ++i)
    {
        SetHubRows(rowIndex++, emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, g_advancedBoolSettingDefs[i].setting_id, &g_advancedBoolSettingDefs[i], emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL_V2, &g_advancedBoolSettingDefsV2[i], kHubSectionAdvancedId, kHubSectionAdvancedLabel);
    }

    g_modHubRegistration.row_count = static_cast<uint32_t>(rowIndex);
    g_modHubRegistrationV2.row_count = static_cast<uint32_t>(rowIndex);
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

    ++state.configRevision;
    state.lastProbeTickMs = 0;

    if (!updated.enabled)
    {
        vs_probe_cache::Reset(state);
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

    if (IsBountyTierSetting(descriptor) && !IsBountyTierOrderValid(updated))
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_bounty_tier_order");
        return EMC_ERR_INVALID_ARGUMENT;
    }

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

EMC_Result __cdecl GetSelectSettingValue(void* user_data, int32_t* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HubSelectSettingDescriptor* descriptor = static_cast<const HubSelectSettingDescriptor*>(user_data);
    RuntimeStateView state = vs_runtime_state::GetRuntimeStateView();
    return vs_mod_hub_helpers::GetBoolSelectionFieldValue(
        &state.config,
        out_value,
        descriptor->field,
        kBountySymbolPositionBeforeStateText,
        kBountySymbolPositionBeforeStateIcon);
}

EMC_Result __cdecl SetSelectSettingValue(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    if (user_data == 0)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_setting_context");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HubSelectSettingDescriptor* descriptor = static_cast<const HubSelectSettingDescriptor*>(user_data);
    bool placeBeforeStateIcon = false;
    const EMC_Result selectValidation = vs_mod_hub_helpers::NormalizeBoolSelectionValue(
        value,
        kBountySymbolPositionBeforeStateText,
        kBountySymbolPositionBeforeStateIcon,
        &placeBeforeStateIcon,
        err_buf,
        err_buf_size);
    if (selectValidation != EMC_OK)
    {
        return selectValidation;
    }

    RuntimeStateView state = vs_runtime_state::GetRuntimeStateView();
    PluginConfig updated = state.config;
    updated.*(descriptor->field) = placeBeforeStateIcon;
    return ApplyHubConfigUpdate(updated, err_buf, err_buf_size);
}

EMC_Result __cdecl GetTextSettingValue(void* user_data, char* out_value, uint32_t out_value_size)
{
    if (user_data == 0 || out_value == 0 || out_value_size == 0u)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HubTextSettingDescriptor* descriptor = static_cast<const HubTextSettingDescriptor*>(user_data);
    RuntimeStateView state = vs_runtime_state::GetRuntimeStateView();
    return vs_mod_hub_helpers::GetStringFieldValue(&state.config, out_value, out_value_size, descriptor->field);
}

EMC_Result __cdecl SetTextSettingValue(void* user_data, const char* value, char* err_buf, uint32_t err_buf_size)
{
    if (user_data == 0 || value == 0)
    {
        emc::consumer::WriteErrorMessage(err_buf, err_buf_size, "invalid_setting_context");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const HubTextSettingDescriptor* descriptor = static_cast<const HubTextSettingDescriptor*>(user_data);
    std::string normalized;
    const EMC_Result textValidation = vs_mod_hub_helpers::NormalizeTextValue(
        value,
        descriptor->maxLength,
        normalized,
        err_buf,
        err_buf_size,
        true,
        descriptor->allowEmpty);
    if (textValidation != EMC_OK)
    {
        return textValidation;
    }

    RuntimeStateView state = vs_runtime_state::GetRuntimeStateView();
    PluginConfig updated = state.config;
    updated.*(descriptor->field) = normalized;
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
    config.register_fn = &RegisterModHubSettings;
    g_modHubClient.SetConfig(config);
    g_modHubClientConfigured = true;
}

EMC_Result __cdecl RegisterModHubSettings(const EMC_HubApiV1* api, void* user_data)
{
    (void)user_data;

    if (api == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return emc::RegisterSettingsTableWithApiSizeV1(
        api,
        api->api_size,
        GetModHubRegistrationForApi(api));
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
