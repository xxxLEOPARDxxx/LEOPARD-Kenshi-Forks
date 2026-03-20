#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <string>

struct PluginConfig
{
    bool enabled;
    bool enableExecuteKillSound;
    bool debugExecuteLogging;
    bool ignoreExecuteAllianceCheck;
    int executeButtonWidthPx;
    int executeButtonHeightPx;
    int executeButtonOffsetXPx;
    int executeButtonOffsetYPx;
};

struct ConfigParseDiagnostics
{
    bool foundEnabled;
    bool invalidEnabled;
    bool foundEnableExecuteKillSound;
    bool invalidEnableExecuteKillSound;
    bool foundDebugExecuteLogging;
    bool invalidDebugExecuteLogging;
    bool foundIgnoreExecuteAllianceCheck;
    bool invalidIgnoreExecuteAllianceCheck;
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
