#include <Debug.h>

#include <core/Functions.h>
#include <kenshi/Kenshi.h>
#include <kenshi/GameWorld.h>
#include <kenshi/PlayerInterface.h>

#include "src/vs_config.h"
#include "src/vs_character_tint.h"
#include "src/vs_log.h"
#include "src/vs_marker_render.h"
#include "src/vs_parse.h"
#include "src/vs_probe.h"
#include "src/vs_runtime_state.h"
#include "src/vs_types.h"

#ifndef BOOST_ALL_NO_LIB
#define BOOST_ALL_NO_LIB
#endif

#ifndef BOOST_ERROR_CODE_HEADER_ONLY
#define BOOST_ERROR_CODE_HEADER_ONLY
#endif

#include <Windows.h>

#include <sstream>
#include <string>

namespace
{
const char* kPluginName = "Vital-Sense";
const char* kConfigFileName = "mod-config.json";

const size_t kMaxKoMarkerWidgets = 48;
void (*GameWorld_mainLoopGPUSensitiveStuffOrig)(GameWorld* thisptr, float time) = 0;

bool IsSupportedVersion(KenshiLib::BinaryVersion versionInfo)
{
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    return platform != KenshiLib::BinaryVersion::UNKNOWN
        && (version == "1.0.65" || version == "1.0.68");
}

void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    PlayerInterfaceUpdateUTFn* updateUTOrigSlot = vs_runtime_state::GetPlayerInterfaceUpdateUTOrigSlot();
    if (updateUTOrigSlot && *updateUTOrigSlot)
    {
        (*updateUTOrigSlot)(thisptr);
    }

    RuntimeStateView state = vs_runtime_state::GetRuntimeStateView();
    const bool anyMarkerOverlayVisualEnabled =
        state.config.showMarkerIcons
        || state.config.showMarkerText
        || state.config.showBountySymbol;
    const vs_probe::ProbeRenderDirective renderDirective = vs_probe::TickKoProbe(state, kPluginName);
    if (renderDirective == vs_probe::PROBE_RENDER_HIDE_ALL)
    {
        if (anyMarkerOverlayVisualEnabled)
        {
            vs_marker_render::HideAllKoMarkerWidgets(state, kPluginName);
        }
    }
    else if (renderDirective == vs_probe::PROBE_RENDER_TICK)
    {
        if (anyMarkerOverlayVisualEnabled)
        {
            vs_marker_render::TickKoMarkerRender(state, kPluginName);
        }
    }
}

void GameWorld_mainLoopGPUSensitiveStuff_hook(GameWorld* thisptr, float time)
{
    RuntimeStateView state = vs_runtime_state::GetRuntimeStateView();

    if (state.highlightRuntimeActive)
    {
        vs_character_tint::SyncKoCharacterTint(state, kPluginName);
    }
    else
    {
        vs_character_tint::ClearKoCharacterTint(state, kPluginName);
    }

    if (GameWorld_mainLoopGPUSensitiveStuffOrig)
    {
        GameWorld_mainLoopGPUSensitiveStuffOrig(thisptr, time);
    }
}
}

__declspec(dllexport) void startPlugin()
{
    vs_log::LogInfo(kPluginName, "startPlugin()");

    const KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    if (!IsSupportedVersion(versionInfo))
    {
        vs_log::LogError(kPluginName, "unsupported Kenshi version/platform");
        return;
    }

    RuntimeStateView state = vs_runtime_state::GetRuntimeStateView();
    vs_config::LoadConfigState(state, kPluginName);
    const PluginConfig& config = state.config;

    std::stringstream info;
    info << "loaded (enabled=" << (config.enabled ? "true" : "false")
         << ", update_interval_ms=" << config.updateIntervalMs
         << ", only_when_alt_held=" << (config.onlyWhenAltHeld ? "true" : "false")
         << ", enable_unconscious=" << (config.enableUnconsciousState ? "true" : "false")
         << ", enable_recovery_coma=" << (config.enableRecoveryComaState ? "true" : "false")
         << ", enable_dying=" << (config.enableDyingState ? "true" : "false")
         << ", enable_playing_dead=" << (config.enablePlayingDeadState ? "true" : "false")
         << ", enable_dead=" << (config.enableDeadState ? "true" : "false")
         << ", show_icons=" << (config.showMarkerIcons ? "true" : "false")
         << ", show_text=" << (config.showMarkerText ? "true" : "false")
         << ", show_bounty_glow=" << (config.showBountyGlow ? "true" : "false")
         << ", show_bounty_symbol=" << (config.showBountySymbol ? "true" : "false")
         << ", enable_character_tint=" << (config.enableCharacterTint ? "true" : "false")
         << ", character_tint_include_bounty_only=" << (config.characterTintIncludeBountyOnly ? "true" : "false")
         << ", character_tint_force_depth_override=" << (config.characterTintForceDepthOverride ? "true" : "false")
         << ", show_bounty_symbol_on_all_characters=" << (config.showBountySymbolOnAllCharacters ? "true" : "false")
         << ", bounty_symbol=" << config.bountySymbolText
         << ", bounty_symbol_size_px=" << config.bountySymbolTextSizePx
         << ", bounty_symbol_position=" << (config.placeBountySymbolBeforeStateIcon ? "before_state_icon" : "before_state_text")
         << ", bounty_symbol_live_anchor_y_offset_cm=" << config.bountySymbolLiveAnchorYOffsetCm
         << ", unconscious_text=" << config.unconsciousText
         << ", recovery_coma_text=" << config.recoveryComaText
         << ", dying_text=" << config.dyingText
         << ", playing_dead_text=" << config.playingDeadText
         << ", dead_text=" << config.deadText
         << ", unconscious_text_size_px=" << config.unconsciousTextSizePx
         << ", recovery_coma_text_size_px=" << config.recoveryComaTextSizePx
         << ", dying_text_size_px=" << config.dyingTextSizePx
         << ", playing_dead_text_size_px=" << config.playingDeadTextSizePx
         << ", dead_text_size_px=" << config.deadTextSizePx
         << ", enemy_color_hex=" << vs_parse::ColourToHexRgb(config.enemyMarkerColour)
         << ", ally_color_hex=" << vs_parse::ColourToHexRgb(config.allyMarkerColour)
         << ", squad_color_hex=" << vs_parse::ColourToHexRgb(config.squadMarkerColour)
         << ", bounty_tier_trivial_max=" << config.bountyTierTrivialMax
         << ", bounty_tier_low_max=" << config.bountyTierLowMax
         << ", bounty_tier_modest_max=" << config.bountyTierModestMax
         << ", bounty_tier_notable_max=" << config.bountyTierNotableMax
         << ", bounty_tier_high_value_max=" << config.bountyTierHighValueMax
         << ", bounty_tier_elite_max=" << config.bountyTierEliteMax
         << ", bounty_color_trivial_hex=" << vs_parse::ColourToHexRgb(config.bountyTierTrivialColour)
         << ", bounty_color_low_hex=" << vs_parse::ColourToHexRgb(config.bountyTierLowColour)
         << ", bounty_color_modest_hex=" << vs_parse::ColourToHexRgb(config.bountyTierModestColour)
         << ", bounty_color_notable_hex=" << vs_parse::ColourToHexRgb(config.bountyTierNotableColour)
         << ", bounty_color_high_value_hex=" << vs_parse::ColourToHexRgb(config.bountyTierHighValueColour)
         << ", bounty_color_elite_hex=" << vs_parse::ColourToHexRgb(config.bountyTierEliteColour)
         << ", bounty_color_legendary_hex=" << vs_parse::ColourToHexRgb(config.bountyTierLegendaryColour)
         << ", max_highlight_distance_m=" << config.maxHighlightDistanceMeters
         << ")";
    vs_log::LogInfo(kPluginName, info.str());

    state.koTargetCache.reserve(128);
    state.visibleKoHandlesScratch.reserve(128);
    state.koMarkerWidgets.reserve(kMaxKoMarkerWidgets);

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        vs_runtime_state::GetPlayerInterfaceUpdateUTOrigSlot()))
    {
        vs_log::LogError(kPluginName, "could not hook PlayerInterface::updateUT");
        return;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&GameWorld::_NV_mainLoop_GPUSensitiveStuff),
        GameWorld_mainLoopGPUSensitiveStuff_hook,
        &GameWorld_mainLoopGPUSensitiveStuffOrig))
    {
        vs_log::LogError(kPluginName, "could not hook GameWorld::mainLoop_GPUSensitiveStuff");
        return;
    }

    vs_log::LogInfo(kPluginName, "update and gpu hooks installed");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        char dllPath[_MAX_PATH] = { 0 };
        if (GetModuleFileNameA(hModule, dllPath, _MAX_PATH) > 0)
        {
            const std::string fullPath = vs_parse::TrimAscii(std::string(dllPath));
            const size_t sep = fullPath.find_last_of("\\/");
            if (sep != std::string::npos)
            {
                const std::string pluginDir = fullPath.substr(0, sep);
                RuntimeStateView state = vs_runtime_state::GetRuntimeStateView();
                state.settingsPath = pluginDir + "\\" + kConfigFileName;
            }
        }
    }

    return TRUE;
}
