#pragma once

#include "vs_types.h"

#include <string>
#include <vector>

class UtilityT;
class PlayerInterface;

struct RuntimeStateView
{
    PluginConfig& config;
    std::string& settingsPath;
    DWORD& lastProbeTickMs;
    void (*&playerInterfaceUpdateUTOrig)(PlayerInterface*);
    std::vector<CachedKoTarget>& koTargetCache;
    std::vector<hand>& visibleKoHandlesScratch;
    std::vector<KoMarkerWidget>& koMarkerWidgets;
    std::vector<std::string>& iconTextureOkLogs;
    std::vector<std::string>& iconTextureWarnLogs;
    UtilityT*& projectionUtility;
    unsigned int& koMarkerWidgetSerial;
    bool& highlightRuntimeActive;
};

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
    bool& highlightRuntimeActive);
