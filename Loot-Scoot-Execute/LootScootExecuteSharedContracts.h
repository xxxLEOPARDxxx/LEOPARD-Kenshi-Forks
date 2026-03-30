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
    bool enableExecuteAll;
    bool ignoreExecuteAllianceCheck;
    int executeDistanceMeters;
    int executeAllRadiusUnits;
    int executeButtonWidthPx;
    int executeButtonHeightPx;
    int executeButtonGapPx;
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
    bool foundEnableExecuteAll;
    bool invalidEnableExecuteAll;
    bool foundIgnoreExecuteAllianceCheck;
    bool invalidIgnoreExecuteAllianceCheck;
    bool foundExecuteDistanceMeters;
    bool usedLegacyExecuteDistanceMetersKey;
    bool invalidExecuteDistanceMeters;
    bool clampedExecuteDistanceMeters;
    bool foundExecuteAllRadiusUnits;
    bool invalidExecuteAllRadiusUnits;
    bool clampedExecuteAllRadiusUnits;
    bool foundExecuteButtonWidthPx;
    bool invalidExecuteButtonWidthPx;
    bool clampedExecuteButtonWidthPx;
    bool foundExecuteButtonHeightPx;
    bool invalidExecuteButtonHeightPx;
    bool clampedExecuteButtonHeightPx;
    bool foundExecuteButtonGapPx;
    bool invalidExecuteButtonGapPx;
    bool clampedExecuteButtonGapPx;
    bool foundExecuteButtonOffsetXPx;
    bool invalidExecuteButtonOffsetXPx;
    bool clampedExecuteButtonOffsetXPx;
    bool foundExecuteButtonOffsetYPx;
    bool invalidExecuteButtonOffsetYPx;
    bool clampedExecuteButtonOffsetYPx;
    bool syntaxError;
    size_t syntaxErrorOffset;
};
