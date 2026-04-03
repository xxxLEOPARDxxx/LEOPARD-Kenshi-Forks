#pragma once

#include "HiddenFactionRelations.h"

#include <string>
#include <vector>

enum HiddenFactionRelationsUiTone
{
    HiddenFactionRelationsUiTone_Default = 0,
    HiddenFactionRelationsUiTone_Hostile,
    HiddenFactionRelationsUiTone_Friendly,
    HiddenFactionRelationsUiTone_Neutral
};

struct HiddenFactionRelationsUiRow
{
    std::string factionName;
    std::string relationValueText;
    std::string relationBadgeText;
    float relationValue;
    bool hasPlayerRelation;
    HiddenFactionRelationsUiTone relationTone;
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

enum HiddenFactionRelationsUiSortMode
{
    HiddenFactionRelationsUiSort_RelationAscending = 0,
    HiddenFactionRelationsUiSort_RelationDescending,
    HiddenFactionRelationsUiSort_NameAscending,
    HiddenFactionRelationsUiSort_NameDescending
};

struct HiddenFactionRelationsUiOptions
{
    std::string searchText;
    HiddenFactionRelationsUiSortMode sortMode;
    bool nonZeroOnly;

    HiddenFactionRelationsUiOptions();
};

struct HiddenFactionRelationsUiView
{
    std::string playerFactionName;
    std::string playerFactionId;
    HiddenFactionRelationsUiSummary summary;
    std::vector<HiddenFactionRelationsUiRow> rows;
};

bool HiddenFactionRelationsUiModel_BuildView(
    const HiddenFactionRelationsSnapshot& snapshot,
    const HiddenFactionRelationsUiOptions& options,
    HiddenFactionRelationsUiView* outView);
