#pragma once

#include <kenshi/Character.h>

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <string>

struct PluginConfig
{
    bool enabled;
    DWORD pauseDebounceMs;
    bool debugLogTransitions;
    bool enableDeleteAllJobsSelectedMemberAction;
    bool enableExperimentalSingleJobDelete;
    bool logSelectedMemberJobSnapshot;
    bool hidePanelDuringCharacterCreation;
    bool hidePanelDuringInventoryOpen;
    bool hidePanelDuringCharacterInteraction;
    bool jobBGonePanelCollapsed;
    bool jobBGonePanelHasCustomPosition;
    int jobBGonePanelPosX;
    int jobBGonePanelPosY;
    std::string panelVisibilityToggleHotkey;
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

struct JobRowModel
{
    int slot;
    TaskType taskType;
    std::string taskName;
    uintptr_t taskDataPtr;
    std::string jobKeyBase;
    int duplicateOrdinal;
    std::string jobKey;
};

enum JobDeleteScope
{
    JobDeleteScope_SelectedMember = 0,
    JobDeleteScope_SelectedMembers = 1,
    JobDeleteScope_WholeSquad = 2,
    JobDeleteScope_EveryoneGlobal = 3
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
    bool foundEnableDeleteAllJobsSelectedMemberAction;
    bool invalidEnableDeleteAllJobsSelectedMemberAction;
    bool foundEnableExperimentalSingleJobDelete;
    bool invalidEnableExperimentalSingleJobDelete;
    bool foundLogSelectedMemberJobSnapshot;
    bool invalidLogSelectedMemberJobSnapshot;
    bool foundHidePanelDuringCharacterCreation;
    bool invalidHidePanelDuringCharacterCreation;
    bool foundHidePanelDuringInventoryOpen;
    bool invalidHidePanelDuringInventoryOpen;
    bool foundHidePanelDuringCharacterInteraction;
    bool invalidHidePanelDuringCharacterInteraction;
    bool foundJobBGonePanelCollapsed;
    bool invalidJobBGonePanelCollapsed;
    bool foundJobBGonePanelHasCustomPosition;
    bool invalidJobBGonePanelHasCustomPosition;
    bool foundJobBGonePanelPosX;
    bool invalidJobBGonePanelPosX;
    bool clampedJobBGonePanelPosX;
    bool foundJobBGonePanelPosY;
    bool invalidJobBGonePanelPosY;
    bool clampedJobBGonePanelPosY;
    bool foundPanelVisibilityToggleHotkey;
    bool invalidPanelVisibilityToggleHotkey;
    bool syntaxError;
    size_t syntaxErrorOffset;
};
