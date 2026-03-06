#include "vs_runtime_state.h"

namespace
{
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
    false,
    false,
    true,
    false,
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
PlayerInterfaceUpdateUTFn g_playerInterfaceUpdateUTOrig = 0;
std::vector<CachedKoTarget> g_koTargetCache;
std::vector<CharacterTintEntry> g_characterTintEntries;
std::vector<hand> g_visibleKoHandlesScratch;
std::vector<KoMarkerWidget> g_koMarkerWidgets;
std::vector<std::string> g_iconTextureOkLogs;
std::vector<std::string> g_iconTextureWarnLogs;
UtilityT* g_projectionUtility = 0;
unsigned int g_koMarkerWidgetSerial = 0;
bool g_highlightRuntimeActive = false;
} // namespace

namespace vs_runtime_state
{

RuntimeStateView CreateRuntimeStateView(
    PluginConfig& config,
    std::string& settingsPath,
    DWORD& lastProbeTickMs,
    PlayerInterfaceUpdateUTFn& playerInterfaceUpdateUTOrig,
    std::vector<CachedKoTarget>& koTargetCache,
    std::vector<CharacterTintEntry>& characterTintEntries,
    std::vector<hand>& visibleKoHandlesScratch,
    std::vector<KoMarkerWidget>& koMarkerWidgets,
    std::vector<std::string>& iconTextureOkLogs,
    std::vector<std::string>& iconTextureWarnLogs,
    UtilityT*& projectionUtility,
    unsigned int& koMarkerWidgetSerial,
    bool& highlightRuntimeActive)
{
    RuntimeStateView state = {
        config,
        settingsPath,
        lastProbeTickMs,
        playerInterfaceUpdateUTOrig,
        koTargetCache,
        characterTintEntries,
        visibleKoHandlesScratch,
        koMarkerWidgets,
        iconTextureOkLogs,
        iconTextureWarnLogs,
        projectionUtility,
        koMarkerWidgetSerial,
        highlightRuntimeActive
    };

    return state;
}

RuntimeStateView GetRuntimeStateView()
{
    return CreateRuntimeStateView(
        g_config,
        g_settingsPath,
        g_lastProbeTickMs,
        g_playerInterfaceUpdateUTOrig,
        g_koTargetCache,
        g_characterTintEntries,
        g_visibleKoHandlesScratch,
        g_koMarkerWidgets,
        g_iconTextureOkLogs,
        g_iconTextureWarnLogs,
        g_projectionUtility,
        g_koMarkerWidgetSerial,
        g_highlightRuntimeActive);
}

PlayerInterfaceUpdateUTFn* GetPlayerInterfaceUpdateUTOrigSlot()
{
    return &g_playerInterfaceUpdateUTOrig;
}

} // namespace vs_runtime_state
