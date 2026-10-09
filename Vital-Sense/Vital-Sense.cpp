#include <Debug.h>

#include <core/Functions.h>
#include <kenshi/Kenshi.h>
#include <kenshi/GameWorld.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/SaveInfo.h>
#include <kenshi/SaveManager.h>

#include "src/vs_config.h"
#include "src/vs_character_tint.h"
#include "src/vs_log.h"
#include "src/vs_keybind.h"
#include "src/vs_marker_render.h"
#include "src/vs_parse.h"
#include "src/vs_probe.h"
#include "src/vs_probe_cache.h"
#include "src/vs_runtime_state.h"
#include "src/vs_mod_hub.h"
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
const DWORD kSaveLoadHighlightCooldownMs = 5000;
void (*GameWorld_mainLoopGPUSensitiveStuffOrig)(GameWorld* thisptr, float time) = 0;
void (*SaveManager_loadByNameOrig)(SaveManager* thisptr, const std::string& name) = 0;
void (*SaveManager_loadByInfoOrig)(SaveManager* thisptr, const SaveInfo& saveInfo, bool resetPos) = 0;
DWORD gSaveLoadHighlightCooldownUntilMs = 0;

bool IsSupportedVersion(KenshiLib::BinaryVersion versionInfo)
{
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    return platform != KenshiLib::BinaryVersion::UNKNOWN
        && (version == "1.0.65" || version == "1.0.68");
}

std::string ColourToLogToken(const MyGUI::Colour& colour)
{
    std::string token = vs_parse::ColourToHexRgb(colour);
    if (!token.empty() && token[0] == '#')
    {
        token[0] = '@';
    }
    return token;
}

bool IsSaveLoadHighlightCooldownActive()
{
    return gSaveLoadHighlightCooldownUntilMs != 0
        && GetTickCount() < gSaveLoadHighlightCooldownUntilMs;
}

void BeginSaveLoadHighlightCooldown()
{
    RuntimeStateView state = vs_runtime_state::GetRuntimeStateView();
    state.highlightRuntimeActive = false;
    state.lastProbeTickMs = 0;
    vs_probe_cache::Reset(state);
    vs_character_tint::ResetKoCharacterTintRuntime(state);
    gSaveLoadHighlightCooldownUntilMs = GetTickCount() + kSaveLoadHighlightCooldownMs;
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
    if (IsSaveLoadHighlightCooldownActive())
    {
        state.highlightRuntimeActive = false;
        state.lastProbeTickMs = 0;
        vs_probe_cache::Reset(state);
        vs_character_tint::ResetKoCharacterTintRuntime(state);
        if (anyMarkerOverlayVisualEnabled)
        {
            vs_marker_render::HideAllKoMarkerWidgets(state, kPluginName);
        }
        return;
    }

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
    if (IsSaveLoadHighlightCooldownActive())
    {
        state.highlightRuntimeActive = false;
        vs_character_tint::ResetKoCharacterTintRuntime(state);
        if (GameWorld_mainLoopGPUSensitiveStuffOrig)
        {
            GameWorld_mainLoopGPUSensitiveStuffOrig(thisptr, time);
        }
        return;
    }

    vs_character_tint::TickKoCharacterTintRuntime(state);

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

void SaveManager_loadByName_hook(SaveManager* thisptr, const std::string& name)
{
    BeginSaveLoadHighlightCooldown();
    if (SaveManager_loadByNameOrig)
    {
        SaveManager_loadByNameOrig(thisptr, name);
    }
    BeginSaveLoadHighlightCooldown();
}

void SaveManager_loadByInfo_hook(SaveManager* thisptr, const SaveInfo& saveInfo, bool resetPos)
{
    BeginSaveLoadHighlightCooldown();
    if (SaveManager_loadByInfoOrig)
    {
        SaveManager_loadByInfoOrig(thisptr, saveInfo, resetPos);
    }
    BeginSaveLoadHighlightCooldown();
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
    ++state.configRevision;
    state.lastTintConfigRevision = 0;
    const PluginConfig& config = state.config;

    std::stringstream info;
    info << "loaded (enabled=" << (config.enabled ? "true" : "false")
         << ", update_interval_ms=" << config.updateIntervalMs
         << ", highlight_key=" << vs_keybind::FormatKeybind(config)
         << ", show_icons=" << (config.showMarkerIcons ? "true" : "false")
         << ", show_text=" << (config.showMarkerText ? "true" : "false")
         << ", show_bounty_symbol=" << (config.showBountySymbol ? "true" : "false")
         << ", show_bounty_glow=" << (config.showBountyGlow ? "true" : "false")
         << ", enable_character_tint=" << (config.enableCharacterTint ? "true" : "false")
         << ", character_tint_include_squad=" << (config.characterTintIncludeSquad ? "true" : "false")
         << ", character_tint_force_depth_override=" << (config.characterTintForceDepthOverride ? "true" : "false")
         << ", debug_log_diagnostics=" << (config.debugLogDiagnostics ? "true" : "false")
         << ", debug_log_texture_info=" << (config.debugLogTextureInfo ? "true" : "false")
         << ", enemy_color_hex=" << ColourToLogToken(config.enemyMarkerColour)
         << ", ally_color_hex=" << ColourToLogToken(config.allyMarkerColour)
         << ", squad_color_hex=" << ColourToLogToken(config.squadMarkerColour)
         << ", max_highlight_distance_m=" << config.maxHighlightDistanceMeters
         << ")";
    vs_log::LogInfo(kPluginName, info.str());

    state.koTargetCache.reserve(128);
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

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(static_cast<void (SaveManager::*)(const std::string&)>(&SaveManager::load)),
        SaveManager_loadByName_hook,
        &SaveManager_loadByNameOrig))
    {
        vs_log::LogError(kPluginName, "could not hook SaveManager::load(name)");
        return;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(static_cast<void (SaveManager::*)(const SaveInfo&, bool)>(&SaveManager::load)),
        SaveManager_loadByInfo_hook,
        &SaveManager_loadByInfoOrig))
    {
        vs_log::LogError(kPluginName, "could not hook SaveManager::load(info)");
        return;
    }

    vs_log::LogInfo(kPluginName, "update, gpu, and save-load hooks installed");
    vs_mod_hub::OnStartup();
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
