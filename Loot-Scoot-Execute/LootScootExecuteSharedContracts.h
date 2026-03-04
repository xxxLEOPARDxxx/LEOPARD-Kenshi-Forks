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
    int executeButtonWidthPx;
    int executeButtonHeightPx;
    int executeButtonOffsetXPx;
    int executeButtonOffsetYPx;
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
    bool foundExecuteButtonWidthPx;
    bool invalidExecuteButtonWidthPx;
    bool clampedExecuteButtonWidthPx;
    bool foundExecuteButtonHeightPx;
    bool invalidExecuteButtonHeightPx;
    bool clampedExecuteButtonHeightPx;
    bool foundExecuteButtonOffsetXPx;
    bool invalidExecuteButtonOffsetXPx;
    bool clampedExecuteButtonOffsetXPx;
    bool foundExecuteButtonOffsetYPx;
    bool invalidExecuteButtonOffsetYPx;
    bool clampedExecuteButtonOffsetYPx;
    bool syntaxError;
    size_t syntaxErrorOffset;
};
