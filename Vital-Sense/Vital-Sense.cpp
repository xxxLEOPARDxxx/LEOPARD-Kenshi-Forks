#include <Debug.h>

#include <core/Functions.h>
#include <kenshi/Character.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/Kenshi.h>
#include <kenshi/PlayerInterface.h>
#include <mygui/MyGUI_Colour.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_ImageBox.h>
#include <mygui/MyGUI_RenderManager.h>
#include <mygui/MyGUI_TextBox.h>

#include "src/vs_config.h"
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

#include <cctype>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

class UtilityT
{
public:
    UtilityT();
    bool worldToScreenPX(const Ogre::Vector3& pos, float& x, float& y);
};

namespace
{
const char* kPluginName = "Vital-Sense";
const char* kConfigFileName = "mod-config.json";

PluginConfig g_config = {
    true,
    150,
    true,
    3500,
    true,
    true,
    true,
    true,
    true,
    "ZZ",
    "RC",
    "DY",
    "PD",
    "DE",
    18,
    18,
    18,
    18,
    18,
    MyGUI::Colour(1.0f, 0.2f, 0.2f, 1.0f),
    MyGUI::Colour(0.62f, 0.9f, 0.45f, 1.0f),
    MyGUI::Colour(0.25f, 1.0f, 0.25f, 1.0f),
    "",
    64,
    "",
    64,
    "",
    64,
    "",
    64,
    "",
    64,
    true,
    true,
    true,
    true,
    "$",
    18,
    true,
    true,
    1999,
    4999,
    9999,
    19999,
    39999,
    74999,
    MyGUI::Colour(0.690196f, 0.690196f, 0.690196f, 0.784314f),
    MyGUI::Colour(0.788235f, 0.647059f, 0.482353f, 0.831373f),
    MyGUI::Colour(0.623529f, 0.741176f, 0.411765f, 0.878431f),
    MyGUI::Colour(0.435294f, 0.650980f, 0.850980f, 0.925490f),
    MyGUI::Colour(1.000000f, 0.760784f, 0.278431f, 0.960784f),
    MyGUI::Colour(1.000000f, 0.541176f, 0.168627f, 0.980392f),
    MyGUI::Colour(0.878431f, 0.192157f, 0.192157f, 1.000000f),
    420
};
std::string g_settingsPath;
DWORD g_lastProbeTickMs = 0;
void (*PlayerInterface_updateUT_orig)(PlayerInterface*) = 0;
std::vector<CachedKoTarget> g_koTargetCache;
std::vector<hand> g_visibleKoHandlesScratch;
std::vector<KoMarkerWidget> g_koMarkerWidgets;
std::vector<std::string> g_iconTextureOkLogs;
std::vector<std::string> g_iconTextureWarnLogs;
UtilityT* g_projectionUtility = 0;
unsigned int g_koMarkerWidgetSerial = 0;
bool g_highlightRuntimeActive = false;

RuntimeStateView GetRuntimeStateView()
{
    RuntimeStateView state = CreateRuntimeStateView(
        g_config,
        g_settingsPath,
        g_lastProbeTickMs,
        PlayerInterface_updateUT_orig,
        g_koTargetCache,
        g_visibleKoHandlesScratch,
        g_koMarkerWidgets,
        g_iconTextureOkLogs,
        g_iconTextureWarnLogs,
        g_projectionUtility,
        g_koMarkerWidgetSerial,
        g_highlightRuntimeActive);

    return state;
}

const size_t kMaxKoMarkerWidgets = 48;

void LogInfo(const std::string& message);
void LogWarn(const std::string& message);

// REFACTOR_WRAPPER_PHASE6_REMOVE
void LogWithPrefix(void (*sink)(const char*), const char* level, const std::string& message)
{
    vs_log::LogWithPrefix(kPluginName, sink, level, message);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
void LogInfo(const std::string& message)
{
    vs_log::LogInfo(kPluginName, message);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
void LogWarn(const std::string& message)
{
    vs_log::LogWarn(kPluginName, message);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
void LogError(const std::string& message)
{
    vs_log::LogError(kPluginName, message);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
std::string ToLowerAsciiCopy(const std::string& value)
{
    return vs_parse::ToLowerAsciiCopy(value);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
std::string TrimAscii(const std::string& value)
{
    return vs_parse::TrimAscii(value);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
void SkipWhitespace(const std::string& body, size_t* pos)
{
    vs_parse::SkipWhitespace(body, pos);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
bool ParseBoolFromJson(const std::string& body, const char* keyName, bool* valueOut)
{
    return vs_parse::ParseBoolFromJson(body, keyName, valueOut);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
bool ParseUnsignedFromJson(const std::string& body, const char* keyName, DWORD* valueOut)
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

// REFACTOR_WRAPPER_PHASE6_REMOVE
bool ParseStringFromJson(const std::string& body, const char* keyName, std::string* valueOut)
{
    return vs_parse::ParseStringFromJson(body, keyName, valueOut);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
bool TryParseHexNibble(char value, unsigned int* nibbleOut)
{
    return vs_parse::TryParseHexNibble(value, nibbleOut);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
bool TryParseHexByte(const std::string& value, size_t pos, unsigned int* byteOut)
{
    return vs_parse::TryParseHexByte(value, pos, byteOut);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
bool TryParseColourHex(const std::string& rawValue, MyGUI::Colour* colourOut)
{
    return vs_parse::TryParseColourHex(rawValue, colourOut);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
std::string ColourToHexRgb(const MyGUI::Colour& colour)
{
    return vs_parse::ColourToHexRgb(colour);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
bool LoadConfigState()
{
    RuntimeStateView state = GetRuntimeStateView();
    return vs_config::LoadConfigState(state, kPluginName);
}

bool IsSupportedVersion(KenshiLib::BinaryVersion versionInfo)
{
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    return platform != KenshiLib::BinaryVersion::UNKNOWN
        && (version == "1.0.65" || version == "1.0.68");
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
bool EnsureProjectionUtility()
{
    RuntimeStateView state = GetRuntimeStateView();
    return vs_marker_render::EnsureProjectionUtility(state);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
void HideAllKoMarkerWidgets()
{
    RuntimeStateView state = GetRuntimeStateView();
    vs_marker_render::HideAllKoMarkerWidgets(state, kPluginName);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
void TickKoMarkerRender()
{
    RuntimeStateView state = GetRuntimeStateView();
    vs_marker_render::TickKoMarkerRender(state, kPluginName);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
bool IsCharacterValidSafe(Character* candidate)
{
    return vs_probe::IsCharacterValidSafe(candidate);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
bool TryReadCharacterSnapshotSafe(Character* candidate, bool& isOnScreen, Ogre::Vector3& candidatePos, hand& targetHandle)
{
    return vs_probe::TryReadCharacterSnapshotSafe(candidate, isOnScreen, candidatePos, targetHandle);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
int ResolveMarkerRelationSafe(Character* candidate)
{
    RuntimeStateView state = GetRuntimeStateView();
    return vs_probe::ResolveMarkerRelationSafe(state, candidate);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
Character* ResolveDeathParadeCandidateSafe(hand targetHandle, Character* fallbackCandidate)
{
    return vs_probe::ResolveDeathParadeCandidateSafe(targetHandle, fallbackCandidate);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
void ProcessMarkerCandidate(Character* candidate, const Ogre::Vector3& cameraCenter, DWORD nowMs)
{
    RuntimeStateView state = GetRuntimeStateView();
    vs_probe::ProcessMarkerCandidate(state, candidate, cameraCenter, nowMs);
}

void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    if (PlayerInterface_updateUT_orig)
    {
        PlayerInterface_updateUT_orig(thisptr);
    }
    RuntimeStateView state = GetRuntimeStateView();
    vs_probe::TickKoProbe(state, kPluginName);
}
}

__declspec(dllexport) void startPlugin()
{
    LogInfo("startPlugin()");

    const KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    if (!IsSupportedVersion(versionInfo))
    {
        LogError("unsupported Kenshi version/platform");
        return;
    }

    LoadConfigState();

    std::stringstream info;
    info << "loaded (enabled=" << (g_config.enabled ? "true" : "false")
         << ", update_interval_ms=" << g_config.updateIntervalMs
         << ", only_when_alt_held=" << (g_config.onlyWhenAltHeld ? "true" : "false")
         << ", enable_unconscious=" << (g_config.enableUnconsciousState ? "true" : "false")
         << ", enable_recovery_coma=" << (g_config.enableRecoveryComaState ? "true" : "false")
         << ", enable_dying=" << (g_config.enableDyingState ? "true" : "false")
         << ", enable_playing_dead=" << (g_config.enablePlayingDeadState ? "true" : "false")
         << ", enable_dead=" << (g_config.enableDeadState ? "true" : "false")
         << ", show_icons=" << (g_config.showMarkerIcons ? "true" : "false")
         << ", show_text=" << (g_config.showMarkerText ? "true" : "false")
         << ", show_bounty_glow=" << (g_config.showBountyGlow ? "true" : "false")
         << ", show_bounty_symbol=" << (g_config.showBountySymbol ? "true" : "false")
         << ", show_bounty_symbol_on_all_characters=" << (g_config.showBountySymbolOnAllCharacters ? "true" : "false")
         << ", bounty_symbol=" << g_config.bountySymbolText
         << ", bounty_symbol_size_px=" << g_config.bountySymbolTextSizePx
         << ", bounty_symbol_position=" << (g_config.placeBountySymbolBeforeStateIcon ? "before_state_icon" : "before_state_text")
         << ", bounty_symbol_live_anchor_y_offset_cm=" << g_config.bountySymbolLiveAnchorYOffsetCm
         << ", unconscious_text=" << g_config.unconsciousText
         << ", recovery_coma_text=" << g_config.recoveryComaText
         << ", dying_text=" << g_config.dyingText
         << ", playing_dead_text=" << g_config.playingDeadText
         << ", dead_text=" << g_config.deadText
         << ", unconscious_text_size_px=" << g_config.unconsciousTextSizePx
         << ", recovery_coma_text_size_px=" << g_config.recoveryComaTextSizePx
         << ", dying_text_size_px=" << g_config.dyingTextSizePx
         << ", playing_dead_text_size_px=" << g_config.playingDeadTextSizePx
         << ", dead_text_size_px=" << g_config.deadTextSizePx
         << ", enemy_color_hex=" << ColourToHexRgb(g_config.enemyMarkerColour)
         << ", ally_color_hex=" << ColourToHexRgb(g_config.allyMarkerColour)
         << ", squad_color_hex=" << ColourToHexRgb(g_config.squadMarkerColour)
         << ", bounty_tier_trivial_max=" << g_config.bountyTierTrivialMax
         << ", bounty_tier_low_max=" << g_config.bountyTierLowMax
         << ", bounty_tier_modest_max=" << g_config.bountyTierModestMax
         << ", bounty_tier_notable_max=" << g_config.bountyTierNotableMax
         << ", bounty_tier_high_value_max=" << g_config.bountyTierHighValueMax
         << ", bounty_tier_elite_max=" << g_config.bountyTierEliteMax
         << ", bounty_color_trivial_hex=" << ColourToHexRgb(g_config.bountyTierTrivialColour)
         << ", bounty_color_low_hex=" << ColourToHexRgb(g_config.bountyTierLowColour)
         << ", bounty_color_modest_hex=" << ColourToHexRgb(g_config.bountyTierModestColour)
         << ", bounty_color_notable_hex=" << ColourToHexRgb(g_config.bountyTierNotableColour)
         << ", bounty_color_high_value_hex=" << ColourToHexRgb(g_config.bountyTierHighValueColour)
         << ", bounty_color_elite_hex=" << ColourToHexRgb(g_config.bountyTierEliteColour)
         << ", bounty_color_legendary_hex=" << ColourToHexRgb(g_config.bountyTierLegendaryColour)
         << ", max_highlight_distance_m=" << g_config.maxHighlightDistanceMeters
         << ")";
    LogInfo(info.str());

    g_koTargetCache.reserve(128);
    g_visibleKoHandlesScratch.reserve(128);
    g_koMarkerWidgets.reserve(kMaxKoMarkerWidgets);

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        &PlayerInterface_updateUT_orig))
    {
        LogError("could not hook PlayerInterface::updateUT");
        return;
    }

    LogInfo("update hook installed");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        char dllPath[_MAX_PATH] = { 0 };
        if (GetModuleFileNameA(hModule, dllPath, _MAX_PATH) > 0)
        {
            const std::string fullPath = TrimAscii(std::string(dllPath));
            const size_t sep = fullPath.find_last_of("\\/");
            if (sep != std::string::npos)
            {
                const std::string pluginDir = fullPath.substr(0, sep);
                g_settingsPath = pluginDir + "\\" + kConfigFileName;
            }
        }
    }

    return TRUE;
}
