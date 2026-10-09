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
    std::vector<CharacterTintEntry>& characterTintEntries;
    ProbeRuntimeCaches& probeRuntimeCaches;
    std::vector<KoMarkerWidget>& koMarkerWidgets;
    std::vector<std::string>& iconTextureOkLogs;
    std::vector<std::string>& iconTextureWarnLogs;
    UtilityT*& projectionUtility;
    unsigned int& koMarkerWidgetSerial;
    bool& highlightRuntimeActive;
    unsigned int& configRevision;
    unsigned int& lastTintConfigRevision;
};

namespace vs_runtime_state
{

RuntimeStateView CreateRuntimeStateView(
    PluginConfig& config,
    std::string& settingsPath,
    DWORD& lastProbeTickMs,
    PlayerInterfaceUpdateUTFn& playerInterfaceUpdateUTOrig,
    std::vector<CachedKoTarget>& koTargetCache,
    std::vector<CharacterTintEntry>& characterTintEntries,
    ProbeRuntimeCaches& probeRuntimeCaches,
    std::vector<KoMarkerWidget>& koMarkerWidgets,
    std::vector<std::string>& iconTextureOkLogs,
    std::vector<std::string>& iconTextureWarnLogs,
    UtilityT*& projectionUtility,
    unsigned int& koMarkerWidgetSerial,
    bool& highlightRuntimeActive,
    unsigned int& configRevision,
    unsigned int& lastTintConfigRevision);

RuntimeStateView GetRuntimeStateView();
PlayerInterfaceUpdateUTFn* GetPlayerInterfaceUpdateUTOrigSlot();

} // namespace vs_runtime_state
