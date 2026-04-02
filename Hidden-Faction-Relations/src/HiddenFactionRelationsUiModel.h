#pragma once

#include "HiddenFactionRelations.h"

#include <string>
#include <vector>

struct HiddenFactionRelationsUiRow
{
    std::string factionName;
    std::string relationValueText;
    std::string relationBadgeText;
    float relationValue;
    bool hasPlayerRelation;
};

struct HiddenFactionRelationsUiSummary
{
    int shownHiddenFactions;
    int scannedHiddenFactions;
    int hostileCount;
    int friendlyCount;
    int neutralCount;

    HiddenFactionRelationsUiSummary();
};

struct HiddenFactionRelationsUiView
{
    std::string playerFactionName;
    std::string playerFactionId;
    HiddenFactionRelationsUiSummary summary;
    std::vector<HiddenFactionRelationsUiRow> rows;
};

bool HiddenFactionRelationsUiModel_BuildDefaultView(
    const HiddenFactionRelationsSnapshot& snapshot,
    HiddenFactionRelationsUiView* outView);
