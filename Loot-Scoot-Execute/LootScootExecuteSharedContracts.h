#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <string>

struct PluginConfig
{
    bool enabled;
    DWORD pauseDebounceMs;
    bool debugLogTransitions;
    bool enableContextMenuProbe;
    bool debugContextMenu;
    bool enableDebugDirectDamageFallback;
    bool enableExecuteKillSound;
};

struct RuntimeState
{
    bool loadInProgress;
    bool pauseArmed;
    bool loadSignalSeenAfterArm;
    DWORD armTimestampMs;
    DWORD lastPauseMs;
    DWORD lastTickAliveLogMs;
    bool loggedWorldUnavailable;
};

struct ConfigParseDiagnostics
{
    bool foundEnabled;
    bool invalidEnabled;
    bool foundPauseDebounceMs;
    bool invalidPauseDebounceMs;
    bool clampedPauseDebounceMs;
    bool foundDebugLogTransitions;
    bool invalidDebugLogTransitions;
    bool foundEnableContextMenuProbe;
    bool invalidEnableContextMenuProbe;
    bool foundDebugContextMenu;
    bool invalidDebugContextMenu;
    bool foundEnableDebugDirectDamageFallback;
    bool invalidEnableDebugDirectDamageFallback;
    bool foundEnableExecuteKillSound;
    bool invalidEnableExecuteKillSound;
    bool syntaxError;
    size_t syntaxErrorOffset;
};
