#include "vs_runtime_state.h"

RuntimeStateView CreateRuntimeStateView(
    PluginConfig& config,
    std::string& settingsPath,
    DWORD& lastProbeTickMs,
    void (*&playerInterfaceUpdateUTOrig)(PlayerInterface*),
    std::vector<CachedKoTarget>& koTargetCache,
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
