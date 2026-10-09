#pragma once

#include <string>

struct HiddenFactionRelationsConfigSnapshot
{
    bool enabled;
    bool debugLogging;
    bool debugSearchLogging;
    bool debugBindingLogging;
    bool autoFocusSearchOnOpen;

    HiddenFactionRelationsConfigSnapshot();
};

void HiddenFactionRelationsConfig_SetConfigPath(const std::string& configPath);

bool HiddenFactionRelationsConfig_Load(std::string* outError);
bool HiddenFactionRelationsConfig_Save(
    const HiddenFactionRelationsConfigSnapshot& snapshot,
    std::string* outError);

HiddenFactionRelationsConfigSnapshot HiddenFactionRelationsConfig_Capture();
void HiddenFactionRelationsConfig_Apply(const HiddenFactionRelationsConfigSnapshot& snapshot);
void HiddenFactionRelationsConfig_Normalize(HiddenFactionRelationsConfigSnapshot* snapshot);

bool HiddenFactionRelationsConfig_IsDebugLoggingEnabled();
bool HiddenFactionRelationsConfig_IsDebugSearchLoggingEnabled();
bool HiddenFactionRelationsConfig_IsDebugBindingLoggingEnabled();
bool HiddenFactionRelationsConfig_ShouldAutoFocusSearchOnOpen();

const char* HiddenFactionRelationsConfig_BoolToString(bool value);
