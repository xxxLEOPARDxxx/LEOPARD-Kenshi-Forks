#include "vs_config.h"

#include "vs_log.h"
#include "vs_parse.h"

#include <fstream>
#include <sstream>
#include <string>

namespace vs_config
{
namespace
{
const DWORD kDefaultMarkerTextSizePx = 18;
const DWORD kDefaultBountyTierTrivialMax = 1999;
const DWORD kDefaultBountyTierLowMax = 4999;
const DWORD kDefaultBountyTierModestMax = 9999;
const DWORD kDefaultBountyTierNotableMax = 19999;
const DWORD kDefaultBountyTierHighValueMax = 39999;
const DWORD kDefaultBountyTierEliteMax = 74999;
const DWORD kDefaultBountySymbolLiveAnchorYOffsetCm = 420;

bool ParseUnsignedFromJsonSafe(const std::string& body, const char* keyName, DWORD* valueOut)
{
    try
    {
        return vs_parse::ParseUnsignedFromJson(body, keyName, valueOut);
    }
    catch (...)
    {
        return false;
    }
}

void ApplyDefaultConfig(PluginConfig& config)
{
    config.enabled = true;
    config.updateIntervalMs = 150;
    config.onlyWhenAltHeld = true;
    config.maxHighlightDistanceMeters = 3500;
    config.enableUnconsciousState = true;
    config.enableRecoveryComaState = true;
    config.enableDyingState = true;
    config.enablePlayingDeadState = true;
    config.enableDeadState = true;
    config.unconsciousText = "ZZ";
    config.recoveryComaText = "RC";
    config.dyingText = "DY";
    config.playingDeadText = "PD";
    config.deadText = "DE";
    config.unconsciousTextSizePx = kDefaultMarkerTextSizePx;
    config.recoveryComaTextSizePx = kDefaultMarkerTextSizePx;
    config.dyingTextSizePx = kDefaultMarkerTextSizePx;
    config.playingDeadTextSizePx = kDefaultMarkerTextSizePx;
    config.deadTextSizePx = kDefaultMarkerTextSizePx;
    config.enemyMarkerColour = MyGUI::Colour(1.0f, 0.2f, 0.2f, 1.0f);
    config.allyMarkerColour = MyGUI::Colour(0.87f, 0.91f, 0.35f, 1.0f);
    config.squadMarkerColour = MyGUI::Colour(0.25f, 1.0f, 0.25f, 1.0f);
    config.customUnconsciousIconTexture.clear();
    config.customUnconsciousIconSizePx = 64;
    config.customRecoveryComaIconTexture.clear();
    config.customRecoveryComaIconSizePx = 64;
    config.customDyingIconTexture.clear();
    config.customDyingIconSizePx = 64;
    config.customPlayingDeadIconTexture.clear();
    config.customPlayingDeadIconSizePx = 64;
    config.customDeadIconTexture.clear();
    config.customDeadIconSizePx = 64;
    config.showMarkerIcons = true;
    config.showMarkerText = true;
    config.showBountyGlow = true;
    config.showBountySymbol = true;
    config.debugLogDiagnostics = false;
    config.debugLogTextureInfo = false;
    config.enableCharacterTint = true;
    config.characterTintIncludeBountyOnly = false;
    config.characterTintForceDepthOverride = true;
    config.bountySymbolText = "$";
    config.bountySymbolTextSizePx = kDefaultMarkerTextSizePx;
    config.showBountySymbolOnAllCharacters = true;
    config.placeBountySymbolBeforeStateIcon = true;
    config.bountyTierTrivialMax = kDefaultBountyTierTrivialMax;
    config.bountyTierLowMax = kDefaultBountyTierLowMax;
    config.bountyTierModestMax = kDefaultBountyTierModestMax;
    config.bountyTierNotableMax = kDefaultBountyTierNotableMax;
    config.bountyTierHighValueMax = kDefaultBountyTierHighValueMax;
    config.bountyTierEliteMax = kDefaultBountyTierEliteMax;
    config.bountyTierTrivialColour = MyGUI::Colour(0.690196f, 0.690196f, 0.690196f, 0.784314f);
    config.bountyTierLowColour = MyGUI::Colour(0.788235f, 0.647059f, 0.482353f, 0.831373f);
    config.bountyTierModestColour = MyGUI::Colour(0.623529f, 0.741176f, 0.411765f, 0.878431f);
    config.bountyTierNotableColour = MyGUI::Colour(0.435294f, 0.650980f, 0.850980f, 0.925490f);
    config.bountyTierHighValueColour = MyGUI::Colour(1.000000f, 0.760784f, 0.278431f, 0.960784f);
    config.bountyTierEliteColour = MyGUI::Colour(1.000000f, 0.541176f, 0.168627f, 0.980392f);
    config.bountyTierLegendaryColour = MyGUI::Colour(0.878431f, 0.192157f, 0.192157f, 1.000000f);
    config.bountySymbolLiveAnchorYOffsetCm = kDefaultBountySymbolLiveAnchorYOffsetCm;
}
} // namespace

bool LoadConfigState(RuntimeStateView& state, const char* pluginName)
{
    ApplyDefaultConfig(state.config);

    if (state.settingsPath.empty())
    {
        vs_log::LogWarn(pluginName, "settings path is empty; using defaults");
        return false;
    }

    std::ifstream in(state.settingsPath.c_str(), std::ios::in | std::ios::binary);
    if (!in)
    {
        vs_log::LogWarn(pluginName, "mod-config.json not found; using defaults");
        return true;
    }

    const std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    bool parsedEnabled = true;
    if (!vs_parse::ParseBoolFromJson(body, "enabled", &parsedEnabled))
    {
        vs_log::LogWarn(pluginName, "invalid/missing key \"enabled\"; using default");
        return true;
    }

    state.config.enabled = parsedEnabled;
    DWORD parsedInterval = 0;
    if (ParseUnsignedFromJsonSafe(body, "update_interval_ms", &parsedInterval))
    {
        if (parsedInterval < 50)
        {
            state.config.updateIntervalMs = 50;
            vs_log::LogWarn(pluginName, "update_interval_ms too low; clamped to 50");
        }
        else if (parsedInterval > 2000)
        {
            state.config.updateIntervalMs = 2000;
            vs_log::LogWarn(pluginName, "update_interval_ms too high; clamped to 2000");
        }
        else
        {
            state.config.updateIntervalMs = parsedInterval;
        }
    }

    DWORD parsedDistanceMeters = 0;
    if (ParseUnsignedFromJsonSafe(body, "max_highlight_distance_m", &parsedDistanceMeters))
    {
        if (parsedDistanceMeters < 5)
        {
            state.config.maxHighlightDistanceMeters = 5;
            vs_log::LogWarn(pluginName, "max_highlight_distance_m too low; clamped to 5");
        }
        else if (parsedDistanceMeters > 20000)
        {
            state.config.maxHighlightDistanceMeters = 20000;
            vs_log::LogWarn(pluginName, "max_highlight_distance_m too high; clamped to 20000");
        }
        else
        {
            state.config.maxHighlightDistanceMeters = parsedDistanceMeters;
        }
    }

    bool parsedAltGate = true;
    if (vs_parse::ParseBoolFromJson(body, "only_when_alt_held", &parsedAltGate))
    {
        state.config.onlyWhenAltHeld = parsedAltGate;
    }

    bool parsedShowIcons = true;
    if (vs_parse::ParseBoolFromJson(body, "show_icons", &parsedShowIcons))
    {
        state.config.showMarkerIcons = parsedShowIcons;
    }

    bool parsedShowText = true;
    if (vs_parse::ParseBoolFromJson(body, "show_text", &parsedShowText))
    {
        state.config.showMarkerText = parsedShowText;
    }

    bool parsedShowBountyGlow = true;
    if (vs_parse::ParseBoolFromJson(body, "show_bounty_glow", &parsedShowBountyGlow))
    {
        state.config.showBountyGlow = parsedShowBountyGlow;
    }

    bool parsedShowBountySymbol = true;
    if (vs_parse::ParseBoolFromJson(body, "show_bounty_symbol", &parsedShowBountySymbol))
    {
        state.config.showBountySymbol = parsedShowBountySymbol;
    }

    bool parsedDebugLogDiagnostics = false;
    if (vs_parse::ParseBoolFromJson(body, "debug_log_diagnostics", &parsedDebugLogDiagnostics))
    {
        state.config.debugLogDiagnostics = parsedDebugLogDiagnostics;
    }

    bool parsedDebugLogTextureInfo = false;
    if (vs_parse::ParseBoolFromJson(body, "debug_log_texture_info", &parsedDebugLogTextureInfo))
    {
        state.config.debugLogTextureInfo = parsedDebugLogTextureInfo;
    }

    bool parsedEnableCharacterTint = true;
    if (vs_parse::ParseBoolFromJson(body, "enable_character_tint", &parsedEnableCharacterTint))
    {
        state.config.enableCharacterTint = parsedEnableCharacterTint;
    }

    bool parsedCharacterTintIncludeBountyOnly = false;
    if (vs_parse::ParseBoolFromJson(body, "character_tint_include_bounty_only", &parsedCharacterTintIncludeBountyOnly))
    {
        state.config.characterTintIncludeBountyOnly = parsedCharacterTintIncludeBountyOnly;
    }

    bool parsedCharacterTintForceDepthOverride = true;
    if (vs_parse::ParseBoolFromJson(body, "character_tint_force_depth_override", &parsedCharacterTintForceDepthOverride))
    {
        state.config.characterTintForceDepthOverride = parsedCharacterTintForceDepthOverride;
    }

    bool parsedShowBountySymbolOnAllCharacters = false;
    if (vs_parse::ParseBoolFromJson(body, "show_bounty_symbol_on_all_characters", &parsedShowBountySymbolOnAllCharacters))
    {
        state.config.showBountySymbolOnAllCharacters = parsedShowBountySymbolOnAllCharacters;
    }

    std::string parsedBountySymbol;
    if (vs_parse::ParseStringFromJson(body, "bounty_symbol", &parsedBountySymbol))
    {
        const std::string trimmed = vs_parse::TrimAscii(parsedBountySymbol);
        if (!trimmed.empty())
        {
            state.config.bountySymbolText = trimmed;
        }
        else
        {
            vs_log::LogWarn(pluginName, "bounty_symbol is empty; using default");
        }
    }

    DWORD parsedBountySymbolSize = 0;
    if (ParseUnsignedFromJsonSafe(body, "bounty_symbol_size_px", &parsedBountySymbolSize))
    {
        if (parsedBountySymbolSize == 0)
        {
            state.config.bountySymbolTextSizePx = kDefaultMarkerTextSizePx;
            vs_log::LogWarn(pluginName, "bounty_symbol_size_px=0 deprecated; using default 18");
        }
        else if (parsedBountySymbolSize < 8)
        {
            state.config.bountySymbolTextSizePx = 8;
            vs_log::LogWarn(pluginName, "bounty_symbol_size_px too low; clamped to 8");
        }
        else if (parsedBountySymbolSize > 128)
        {
            state.config.bountySymbolTextSizePx = 128;
            vs_log::LogWarn(pluginName, "bounty_symbol_size_px too high; clamped to 128");
        }
        else
        {
            state.config.bountySymbolTextSizePx = parsedBountySymbolSize;
        }
    }

    std::string parsedBountySymbolPosition;
    if (vs_parse::ParseStringFromJson(body, "bounty_symbol_position", &parsedBountySymbolPosition))
    {
        const std::string lowered = vs_parse::ToLowerAsciiCopy(vs_parse::TrimAscii(parsedBountySymbolPosition));
        if (lowered == "before_state_icon" || lowered == "icon")
        {
            state.config.placeBountySymbolBeforeStateIcon = true;
        }
        else if (lowered == "before_state_text" || lowered == "text")
        {
            state.config.placeBountySymbolBeforeStateIcon = false;
        }
        else
        {
            vs_log::LogWarn(pluginName, "bounty_symbol_position invalid; expected before_state_icon|before_state_text; using default");
        }
    }

    DWORD parsedBountySymbolLiveAnchorYOffsetCm = 0;
    if (ParseUnsignedFromJsonSafe(body, "bounty_symbol_live_anchor_y_offset_cm", &parsedBountySymbolLiveAnchorYOffsetCm))
    {
        if (parsedBountySymbolLiveAnchorYOffsetCm > 20000)
        {
            state.config.bountySymbolLiveAnchorYOffsetCm = 20000;
            vs_log::LogWarn(pluginName, "bounty_symbol_live_anchor_y_offset_cm too high; clamped to 20000");
        }
        else
        {
            state.config.bountySymbolLiveAnchorYOffsetCm = parsedBountySymbolLiveAnchorYOffsetCm;
        }
    }

    bool parsedEnableUnconsciousState = true;
    if (vs_parse::ParseBoolFromJson(body, "enable_unconscious", &parsedEnableUnconsciousState))
    {
        state.config.enableUnconsciousState = parsedEnableUnconsciousState;
    }

    bool parsedEnableRecoveryComaState = true;
    if (vs_parse::ParseBoolFromJson(body, "enable_recovery_coma", &parsedEnableRecoveryComaState))
    {
        state.config.enableRecoveryComaState = parsedEnableRecoveryComaState;
    }

    bool parsedEnableDyingState = true;
    if (vs_parse::ParseBoolFromJson(body, "enable_dying", &parsedEnableDyingState))
    {
        state.config.enableDyingState = parsedEnableDyingState;
    }

    bool parsedEnablePlayingDeadState = true;
    if (vs_parse::ParseBoolFromJson(body, "enable_playing_dead", &parsedEnablePlayingDeadState))
    {
        state.config.enablePlayingDeadState = parsedEnablePlayingDeadState;
    }

    bool parsedEnableDeadState = true;
    if (vs_parse::ParseBoolFromJson(body, "enable_dead", &parsedEnableDeadState))
    {
        state.config.enableDeadState = parsedEnableDeadState;
    }

    std::string parsedUnconsciousText;
    if (vs_parse::ParseStringFromJson(body, "unconscious_text", &parsedUnconsciousText))
    {
        const std::string trimmed = vs_parse::TrimAscii(parsedUnconsciousText);
        if (!trimmed.empty())
        {
            state.config.unconsciousText = trimmed;
        }
        else
        {
            vs_log::LogWarn(pluginName, "unconscious_text is empty; using default");
        }
    }

    std::string parsedDyingText;
    if (vs_parse::ParseStringFromJson(body, "dying_text", &parsedDyingText))
    {
        const std::string trimmed = vs_parse::TrimAscii(parsedDyingText);
        if (!trimmed.empty())
        {
            state.config.dyingText = trimmed;
        }
        else
        {
            vs_log::LogWarn(pluginName, "dying_text is empty; using default");
        }
    }

    std::string parsedRecoveryComaText;
    if (vs_parse::ParseStringFromJson(body, "recovery_coma_text", &parsedRecoveryComaText))
    {
        const std::string trimmed = vs_parse::TrimAscii(parsedRecoveryComaText);
        if (!trimmed.empty())
        {
            state.config.recoveryComaText = trimmed;
        }
        else
        {
            vs_log::LogWarn(pluginName, "recovery_coma_text is empty; using default");
        }
    }

    std::string parsedPlayingDeadText;
    if (vs_parse::ParseStringFromJson(body, "playing_dead_text", &parsedPlayingDeadText))
    {
        const std::string trimmed = vs_parse::TrimAscii(parsedPlayingDeadText);
        if (!trimmed.empty())
        {
            state.config.playingDeadText = trimmed;
        }
        else
        {
            vs_log::LogWarn(pluginName, "playing_dead_text is empty; using default");
        }
    }

    std::string parsedDeadText;
    if (vs_parse::ParseStringFromJson(body, "dead_text", &parsedDeadText))
    {
        const std::string trimmed = vs_parse::TrimAscii(parsedDeadText);
        if (!trimmed.empty())
        {
            state.config.deadText = trimmed;
        }
        else
        {
            vs_log::LogWarn(pluginName, "dead_text is empty; using default");
        }
    }

    DWORD parsedUnconsciousTextSize = 0;
    if (ParseUnsignedFromJsonSafe(body, "unconscious_text_size_px", &parsedUnconsciousTextSize))
    {
        if (parsedUnconsciousTextSize == 0)
        {
            state.config.unconsciousTextSizePx = kDefaultMarkerTextSizePx;
            vs_log::LogWarn(pluginName, "unconscious_text_size_px=0 deprecated; using default 18");
        }
        else if (parsedUnconsciousTextSize < 8)
        {
            state.config.unconsciousTextSizePx = 8;
            vs_log::LogWarn(pluginName, "unconscious_text_size_px too low; clamped to 8");
        }
        else if (parsedUnconsciousTextSize > 128)
        {
            state.config.unconsciousTextSizePx = 128;
            vs_log::LogWarn(pluginName, "unconscious_text_size_px too high; clamped to 128");
        }
        else
        {
            state.config.unconsciousTextSizePx = parsedUnconsciousTextSize;
        }
    }

    DWORD parsedDyingTextSize = 0;
    if (ParseUnsignedFromJsonSafe(body, "dying_text_size_px", &parsedDyingTextSize))
    {
        if (parsedDyingTextSize == 0)
        {
            state.config.dyingTextSizePx = kDefaultMarkerTextSizePx;
            vs_log::LogWarn(pluginName, "dying_text_size_px=0 deprecated; using default 18");
        }
        else if (parsedDyingTextSize < 8)
        {
            state.config.dyingTextSizePx = 8;
            vs_log::LogWarn(pluginName, "dying_text_size_px too low; clamped to 8");
        }
        else if (parsedDyingTextSize > 128)
        {
            state.config.dyingTextSizePx = 128;
            vs_log::LogWarn(pluginName, "dying_text_size_px too high; clamped to 128");
        }
        else
        {
            state.config.dyingTextSizePx = parsedDyingTextSize;
        }
    }

    DWORD parsedRecoveryComaTextSize = 0;
    if (ParseUnsignedFromJsonSafe(body, "recovery_coma_text_size_px", &parsedRecoveryComaTextSize))
    {
        if (parsedRecoveryComaTextSize == 0)
        {
            state.config.recoveryComaTextSizePx = kDefaultMarkerTextSizePx;
            vs_log::LogWarn(pluginName, "recovery_coma_text_size_px=0 deprecated; using default 18");
        }
        else if (parsedRecoveryComaTextSize < 8)
        {
            state.config.recoveryComaTextSizePx = 8;
            vs_log::LogWarn(pluginName, "recovery_coma_text_size_px too low; clamped to 8");
        }
        else if (parsedRecoveryComaTextSize > 128)
        {
            state.config.recoveryComaTextSizePx = 128;
            vs_log::LogWarn(pluginName, "recovery_coma_text_size_px too high; clamped to 128");
        }
        else
        {
            state.config.recoveryComaTextSizePx = parsedRecoveryComaTextSize;
        }
    }

    DWORD parsedPlayingDeadTextSize = 0;
    if (ParseUnsignedFromJsonSafe(body, "playing_dead_text_size_px", &parsedPlayingDeadTextSize))
    {
        if (parsedPlayingDeadTextSize == 0)
        {
            state.config.playingDeadTextSizePx = kDefaultMarkerTextSizePx;
            vs_log::LogWarn(pluginName, "playing_dead_text_size_px=0 deprecated; using default 18");
        }
        else if (parsedPlayingDeadTextSize < 8)
        {
            state.config.playingDeadTextSizePx = 8;
            vs_log::LogWarn(pluginName, "playing_dead_text_size_px too low; clamped to 8");
        }
        else if (parsedPlayingDeadTextSize > 128)
        {
            state.config.playingDeadTextSizePx = 128;
            vs_log::LogWarn(pluginName, "playing_dead_text_size_px too high; clamped to 128");
        }
        else
        {
            state.config.playingDeadTextSizePx = parsedPlayingDeadTextSize;
        }
    }

    DWORD parsedDeadTextSize = 0;
    if (ParseUnsignedFromJsonSafe(body, "dead_text_size_px", &parsedDeadTextSize))
    {
        if (parsedDeadTextSize == 0)
        {
            state.config.deadTextSizePx = kDefaultMarkerTextSizePx;
            vs_log::LogWarn(pluginName, "dead_text_size_px=0 deprecated; using default 18");
        }
        else if (parsedDeadTextSize < 8)
        {
            state.config.deadTextSizePx = 8;
            vs_log::LogWarn(pluginName, "dead_text_size_px too low; clamped to 8");
        }
        else if (parsedDeadTextSize > 128)
        {
            state.config.deadTextSizePx = 128;
            vs_log::LogWarn(pluginName, "dead_text_size_px too high; clamped to 128");
        }
        else
        {
            state.config.deadTextSizePx = parsedDeadTextSize;
        }
    }

    std::string parsedEnemyColorHex;
    if (vs_parse::ParseStringFromJson(body, "enemy_color_hex", &parsedEnemyColorHex))
    {
        MyGUI::Colour parsed = state.config.enemyMarkerColour;
        if (vs_parse::TryParseColourHex(parsedEnemyColorHex, &parsed))
        {
            state.config.enemyMarkerColour = parsed;
        }
        else
        {
            vs_log::LogWarn(pluginName, "enemy_color_hex invalid; expected #RRGGBB or #RRGGBBAA; using default");
        }
    }

    std::string parsedAllyColorHex;
    if (vs_parse::ParseStringFromJson(body, "ally_color_hex", &parsedAllyColorHex))
    {
        MyGUI::Colour parsed = state.config.allyMarkerColour;
        if (vs_parse::TryParseColourHex(parsedAllyColorHex, &parsed))
        {
            state.config.allyMarkerColour = parsed;
        }
        else
        {
            vs_log::LogWarn(pluginName, "ally_color_hex invalid; expected #RRGGBB or #RRGGBBAA; using default");
        }
    }

    std::string parsedSquadColorHex;
    if (vs_parse::ParseStringFromJson(body, "squad_color_hex", &parsedSquadColorHex))
    {
        MyGUI::Colour parsed = state.config.squadMarkerColour;
        if (vs_parse::TryParseColourHex(parsedSquadColorHex, &parsed))
        {
            state.config.squadMarkerColour = parsed;
        }
        else
        {
            vs_log::LogWarn(pluginName, "squad_color_hex invalid; expected #RRGGBB or #RRGGBBAA; using default");
        }
    }

    DWORD parsedBountyTierTrivialMax = state.config.bountyTierTrivialMax;
    DWORD parsedBountyTierLowMax = state.config.bountyTierLowMax;
    DWORD parsedBountyTierModestMax = state.config.bountyTierModestMax;
    DWORD parsedBountyTierNotableMax = state.config.bountyTierNotableMax;
    DWORD parsedBountyTierHighValueMax = state.config.bountyTierHighValueMax;
    DWORD parsedBountyTierEliteMax = state.config.bountyTierEliteMax;

    ParseUnsignedFromJsonSafe(body, "bounty_tier_trivial_max", &parsedBountyTierTrivialMax);
    ParseUnsignedFromJsonSafe(body, "bounty_tier_low_max", &parsedBountyTierLowMax);
    ParseUnsignedFromJsonSafe(body, "bounty_tier_modest_max", &parsedBountyTierModestMax);
    ParseUnsignedFromJsonSafe(body, "bounty_tier_notable_max", &parsedBountyTierNotableMax);
    ParseUnsignedFromJsonSafe(body, "bounty_tier_high_value_max", &parsedBountyTierHighValueMax);
    ParseUnsignedFromJsonSafe(body, "bounty_tier_elite_max", &parsedBountyTierEliteMax);

    if (parsedBountyTierTrivialMax < parsedBountyTierLowMax &&
        parsedBountyTierLowMax < parsedBountyTierModestMax &&
        parsedBountyTierModestMax < parsedBountyTierNotableMax &&
        parsedBountyTierNotableMax < parsedBountyTierHighValueMax &&
        parsedBountyTierHighValueMax < parsedBountyTierEliteMax)
    {
        state.config.bountyTierTrivialMax = parsedBountyTierTrivialMax;
        state.config.bountyTierLowMax = parsedBountyTierLowMax;
        state.config.bountyTierModestMax = parsedBountyTierModestMax;
        state.config.bountyTierNotableMax = parsedBountyTierNotableMax;
        state.config.bountyTierHighValueMax = parsedBountyTierHighValueMax;
        state.config.bountyTierEliteMax = parsedBountyTierEliteMax;
    }
    else
    {
        vs_log::LogWarn(pluginName, "bounty tier max keys invalid; expected strict ascending order; using defaults");
    }

    std::string parsedBountyTrivialColorHex;
    if (vs_parse::ParseStringFromJson(body, "bounty_color_trivial_hex", &parsedBountyTrivialColorHex))
    {
        MyGUI::Colour parsed = state.config.bountyTierTrivialColour;
        if (vs_parse::TryParseColourHex(parsedBountyTrivialColorHex, &parsed))
        {
            state.config.bountyTierTrivialColour = parsed;
        }
        else
        {
            vs_log::LogWarn(pluginName, "bounty_color_trivial_hex invalid; expected #RRGGBB or #RRGGBBAA; using default");
        }
    }

    std::string parsedBountyLowColorHex;
    if (vs_parse::ParseStringFromJson(body, "bounty_color_low_hex", &parsedBountyLowColorHex))
    {
        MyGUI::Colour parsed = state.config.bountyTierLowColour;
        if (vs_parse::TryParseColourHex(parsedBountyLowColorHex, &parsed))
        {
            state.config.bountyTierLowColour = parsed;
        }
        else
        {
            vs_log::LogWarn(pluginName, "bounty_color_low_hex invalid; expected #RRGGBB or #RRGGBBAA; using default");
        }
    }

    std::string parsedBountyModestColorHex;
    if (vs_parse::ParseStringFromJson(body, "bounty_color_modest_hex", &parsedBountyModestColorHex))
    {
        MyGUI::Colour parsed = state.config.bountyTierModestColour;
        if (vs_parse::TryParseColourHex(parsedBountyModestColorHex, &parsed))
        {
            state.config.bountyTierModestColour = parsed;
        }
        else
        {
            vs_log::LogWarn(pluginName, "bounty_color_modest_hex invalid; expected #RRGGBB or #RRGGBBAA; using default");
        }
    }

    std::string parsedBountyNotableColorHex;
    if (vs_parse::ParseStringFromJson(body, "bounty_color_notable_hex", &parsedBountyNotableColorHex))
    {
        MyGUI::Colour parsed = state.config.bountyTierNotableColour;
        if (vs_parse::TryParseColourHex(parsedBountyNotableColorHex, &parsed))
        {
            state.config.bountyTierNotableColour = parsed;
        }
        else
        {
            vs_log::LogWarn(pluginName, "bounty_color_notable_hex invalid; expected #RRGGBB or #RRGGBBAA; using default");
        }
    }

    std::string parsedBountyHighValueColorHex;
    if (vs_parse::ParseStringFromJson(body, "bounty_color_high_value_hex", &parsedBountyHighValueColorHex))
    {
        MyGUI::Colour parsed = state.config.bountyTierHighValueColour;
        if (vs_parse::TryParseColourHex(parsedBountyHighValueColorHex, &parsed))
        {
            state.config.bountyTierHighValueColour = parsed;
        }
        else
        {
            vs_log::LogWarn(pluginName, "bounty_color_high_value_hex invalid; expected #RRGGBB or #RRGGBBAA; using default");
        }
    }

    std::string parsedBountyEliteColorHex;
    if (vs_parse::ParseStringFromJson(body, "bounty_color_elite_hex", &parsedBountyEliteColorHex))
    {
        MyGUI::Colour parsed = state.config.bountyTierEliteColour;
        if (vs_parse::TryParseColourHex(parsedBountyEliteColorHex, &parsed))
        {
            state.config.bountyTierEliteColour = parsed;
        }
        else
        {
            vs_log::LogWarn(pluginName, "bounty_color_elite_hex invalid; expected #RRGGBB or #RRGGBBAA; using default");
        }
    }

    std::string parsedBountyLegendaryColorHex;
    if (vs_parse::ParseStringFromJson(body, "bounty_color_legendary_hex", &parsedBountyLegendaryColorHex))
    {
        MyGUI::Colour parsed = state.config.bountyTierLegendaryColour;
        if (vs_parse::TryParseColourHex(parsedBountyLegendaryColorHex, &parsed))
        {
            state.config.bountyTierLegendaryColour = parsed;
        }
        else
        {
            vs_log::LogWarn(pluginName, "bounty_color_legendary_hex invalid; expected #RRGGBB or #RRGGBBAA; using default");
        }
    }

    std::string parsedUnconsciousIconTexture;
    if (vs_parse::ParseStringFromJson(body, "unconscious_icon_texture", &parsedUnconsciousIconTexture))
    {
        state.config.customUnconsciousIconTexture = vs_parse::TrimAscii(parsedUnconsciousIconTexture);
    }

    DWORD parsedUnconsciousIconSize = 0;
    if (ParseUnsignedFromJsonSafe(body, "unconscious_icon_size_px", &parsedUnconsciousIconSize))
    {
        if (parsedUnconsciousIconSize < 8)
        {
            state.config.customUnconsciousIconSizePx = 8;
            vs_log::LogWarn(pluginName, "unconscious_icon_size_px too low; clamped to 8");
        }
        else if (parsedUnconsciousIconSize > 512)
        {
            state.config.customUnconsciousIconSizePx = 512;
            vs_log::LogWarn(pluginName, "unconscious_icon_size_px too high; clamped to 512");
        }
        else
        {
            state.config.customUnconsciousIconSizePx = parsedUnconsciousIconSize;
        }
    }

    std::string parsedRecoveryComaIconTexture;
    if (vs_parse::ParseStringFromJson(body, "recovery_coma_icon_texture", &parsedRecoveryComaIconTexture))
    {
        state.config.customRecoveryComaIconTexture = vs_parse::TrimAscii(parsedRecoveryComaIconTexture);
    }

    DWORD parsedRecoveryComaIconSize = 0;
    if (ParseUnsignedFromJsonSafe(body, "recovery_coma_icon_size_px", &parsedRecoveryComaIconSize))
    {
        if (parsedRecoveryComaIconSize < 8)
        {
            state.config.customRecoveryComaIconSizePx = 8;
            vs_log::LogWarn(pluginName, "recovery_coma_icon_size_px too low; clamped to 8");
        }
        else if (parsedRecoveryComaIconSize > 512)
        {
            state.config.customRecoveryComaIconSizePx = 512;
            vs_log::LogWarn(pluginName, "recovery_coma_icon_size_px too high; clamped to 512");
        }
        else
        {
            state.config.customRecoveryComaIconSizePx = parsedRecoveryComaIconSize;
        }
    }

    std::string parsedDyingIconTexture;
    if (vs_parse::ParseStringFromJson(body, "dying_icon_texture", &parsedDyingIconTexture))
    {
        state.config.customDyingIconTexture = vs_parse::TrimAscii(parsedDyingIconTexture);
    }

    DWORD parsedDyingIconSize = 0;
    if (ParseUnsignedFromJsonSafe(body, "dying_icon_size_px", &parsedDyingIconSize))
    {
        if (parsedDyingIconSize < 8)
        {
            state.config.customDyingIconSizePx = 8;
            vs_log::LogWarn(pluginName, "dying_icon_size_px too low; clamped to 8");
        }
        else if (parsedDyingIconSize > 512)
        {
            state.config.customDyingIconSizePx = 512;
            vs_log::LogWarn(pluginName, "dying_icon_size_px too high; clamped to 512");
        }
        else
        {
            state.config.customDyingIconSizePx = parsedDyingIconSize;
        }
    }

    std::string parsedPlayingDeadIconTexture;
    if (vs_parse::ParseStringFromJson(body, "playing_dead_icon_texture", &parsedPlayingDeadIconTexture))
    {
        state.config.customPlayingDeadIconTexture = vs_parse::TrimAscii(parsedPlayingDeadIconTexture);
    }

    DWORD parsedPlayingDeadIconSize = 0;
    if (ParseUnsignedFromJsonSafe(body, "playing_dead_icon_size_px", &parsedPlayingDeadIconSize))
    {
        if (parsedPlayingDeadIconSize < 8)
        {
            state.config.customPlayingDeadIconSizePx = 8;
            vs_log::LogWarn(pluginName, "playing_dead_icon_size_px too low; clamped to 8");
        }
        else if (parsedPlayingDeadIconSize > 512)
        {
            state.config.customPlayingDeadIconSizePx = 512;
            vs_log::LogWarn(pluginName, "playing_dead_icon_size_px too high; clamped to 512");
        }
        else
        {
            state.config.customPlayingDeadIconSizePx = parsedPlayingDeadIconSize;
        }
    }

    std::string parsedDeadIconTexture;
    if (vs_parse::ParseStringFromJson(body, "dead_icon_texture", &parsedDeadIconTexture))
    {
        state.config.customDeadIconTexture = vs_parse::TrimAscii(parsedDeadIconTexture);
    }

    DWORD parsedDeadIconSize = 0;
    if (ParseUnsignedFromJsonSafe(body, "dead_icon_size_px", &parsedDeadIconSize))
    {
        if (parsedDeadIconSize < 8)
        {
            state.config.customDeadIconSizePx = 8;
            vs_log::LogWarn(pluginName, "dead_icon_size_px too low; clamped to 8");
        }
        else if (parsedDeadIconSize > 512)
        {
            state.config.customDeadIconSizePx = 512;
            vs_log::LogWarn(pluginName, "dead_icon_size_px too high; clamped to 512");
        }
        else
        {
            state.config.customDeadIconSizePx = parsedDeadIconSize;
        }
    }

    if (state.config.debugLogDiagnostics && !state.config.customUnconsciousIconTexture.empty())
    {
        std::stringstream iconInfo;
        iconInfo << "custom ZZ icon configured texture=" << state.config.customUnconsciousIconTexture
                 << " size=" << state.config.customUnconsciousIconSizePx;
        vs_log::LogInfo(pluginName, iconInfo.str());
    }

    if (state.config.debugLogDiagnostics && !state.config.customDyingIconTexture.empty())
    {
        std::stringstream iconInfo;
        iconInfo << "custom DY icon configured texture=" << state.config.customDyingIconTexture
                 << " size=" << state.config.customDyingIconSizePx;
        vs_log::LogInfo(pluginName, iconInfo.str());
    }

    if (state.config.debugLogDiagnostics && !state.config.customRecoveryComaIconTexture.empty())
    {
        std::stringstream iconInfo;
        iconInfo << "custom RC icon configured texture=" << state.config.customRecoveryComaIconTexture
                 << " size=" << state.config.customRecoveryComaIconSizePx;
        vs_log::LogInfo(pluginName, iconInfo.str());
    }

    if (state.config.debugLogDiagnostics && !state.config.customPlayingDeadIconTexture.empty())
    {
        std::stringstream iconInfo;
        iconInfo << "custom PD icon configured texture=" << state.config.customPlayingDeadIconTexture
                 << " size=" << state.config.customPlayingDeadIconSizePx;
        vs_log::LogInfo(pluginName, iconInfo.str());
    }

    if (state.config.debugLogDiagnostics && !state.config.customDeadIconTexture.empty())
    {
        std::stringstream iconInfo;
        iconInfo << "custom DE icon configured texture=" << state.config.customDeadIconTexture
                 << " size=" << state.config.customDeadIconSizePx;
        vs_log::LogInfo(pluginName, iconInfo.str());
    }

    return true;
}

} // namespace vs_config
