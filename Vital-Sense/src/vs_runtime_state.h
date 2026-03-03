#pragma once

#include "vs_types.h"

#include <string>
#include <vector>

class UtilityT;
class PlayerInterface;
typedef void (*PlayerInterfaceUpdateUTFn)(PlayerInterface*);

struct RuntimeStateView
{
    PluginConfig& config;
    std::string& settingsPath;
    DWORD& lastProbeTickMs;
    PlayerInterfaceUpdateUTFn& playerInterfaceUpdateUTOrig;
    std::vector<CachedKoTarget>& koTargetCache;
    std::vector<hand>& visibleKoHandlesScratch;
    std::vector<KoMarkerWidget>& koMarkerWidgets;
    std::vector<std::string>& iconTextureOkLogs;
    std::vector<std::string>& iconTextureWarnLogs;
    UtilityT*& projectionUtility;
    unsigned int& koMarkerWidgetSerial;
    bool& highlightRuntimeActive;
};

namespace vs_runtime_state
{

RuntimeStateView CreateRuntimeStateView(
    PluginConfig& config,
    std::string& settingsPath,
    DWORD& lastProbeTickMs,
    PlayerInterfaceUpdateUTFn& playerInterfaceUpdateUTOrig,
    std::vector<CachedKoTarget>& koTargetCache,
    std::vector<hand>& visibleKoHandlesScratch,
    std::vector<KoMarkerWidget>& koMarkerWidgets,
    std::vector<std::string>& iconTextureOkLogs,
    std::vector<std::string>& iconTextureWarnLogs,
    UtilityT*& projectionUtility,
    unsigned int& koMarkerWidgetSerial,
    bool& highlightRuntimeActive);

RuntimeStateView GetRuntimeStateView();
PlayerInterfaceUpdateUTFn* GetPlayerInterfaceUpdateUTOrigSlot();

} // namespace vs_runtime_state
