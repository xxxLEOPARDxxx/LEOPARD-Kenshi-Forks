#pragma once

#include <string>

struct HiddenFactionRelationsConfigSnapshot
{
    bool enabled;
    bool debugLogging;
    bool debugSearchLogging;
    bool debugBindingLogging;
    bool autoFocusSearchOnOpen;
    bool openMenuRequireCtrl;
    bool openMenuRequireShift;
    bool openMenuRequireAlt;
    int searchInputWidth;
    int searchInputHeight;
    int openMenuKeycode;

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
bool HiddenFactionRelationsConfig_ShouldRequireCtrlForOpenMenu();
bool HiddenFactionRelationsConfig_ShouldRequireShiftForOpenMenu();
bool HiddenFactionRelationsConfig_ShouldRequireAltForOpenMenu();
int HiddenFactionRelationsConfig_GetSearchInputWidth();
int HiddenFactionRelationsConfig_GetSearchInputHeight();
int HiddenFactionRelationsConfig_GetOpenMenuKeycode();

const char* HiddenFactionRelationsConfig_BoolToString(bool value);
