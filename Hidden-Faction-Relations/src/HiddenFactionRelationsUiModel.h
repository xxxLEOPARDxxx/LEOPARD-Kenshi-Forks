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
    std::string originText;
    float relationValue;
    bool hasPlayerRelation;
    HiddenFactionRelationsUiTone relationTone;
};

struct HiddenFactionRelationsUiSummary
{
    int shownFactionCount;
    int scannedFactionCount;
    int shownHiddenFactionCount;
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

enum HiddenFactionRelationsUiScopeMode
{
    HiddenFactionRelationsUiScope_HiddenOnly = 0,
    HiddenFactionRelationsUiScope_AllFactions
};

struct HiddenFactionRelationsUiOptions
{
    std::string searchText;
    HiddenFactionRelationsUiSortMode sortMode;
    HiddenFactionRelationsUiScopeMode scopeMode;
    bool nonZeroOnly;

    HiddenFactionRelationsUiOptions();
};

struct HiddenFactionRelationsUiView
{
    std::string playerFactionName;
    std::string playerFactionId;
    bool showingAllFactions;
    HiddenFactionRelationsUiSummary summary;
    std::vector<HiddenFactionRelationsUiRow> rows;
};

bool HiddenFactionRelationsUiModel_BuildView(
    const HiddenFactionRelationsSnapshot& snapshot,
    const HiddenFactionRelationsUiOptions& options,
    HiddenFactionRelationsUiView* outView);
