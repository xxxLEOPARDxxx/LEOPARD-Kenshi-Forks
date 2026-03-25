#pragma once

#include <kenshi/Character.h>

#include <cstddef>
#include <cstdint>
#include <string>

struct PluginConfig
{
    bool enabled;
    bool debugLogTransitions;
    bool enableDeleteAllJobsTopActions;
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
    bool foundDebugLogTransitions;
    bool invalidDebugLogTransitions;
    bool foundEnableDeleteAllJobsTopActions;
    bool invalidEnableDeleteAllJobsTopActions;
    bool usedLegacyEnableDeleteAllJobsTopActionsKey;
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
