#include "HiddenFactionRelationsUiModel.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace
{
const float kZeroRelationEpsilon = 0.001f;

bool IsEffectivelyZero(float value)
{
    return std::fabs(value) <= kZeroRelationEpsilon;
}

std::string FormatRelationValue(float value)
{
    const int wholeValue = static_cast<int>(value);
    if (std::fabs(value - static_cast<float>(wholeValue)) <= kZeroRelationEpsilon)
    {
        std::stringstream integerText;
        integerText << wholeValue;
        return integerText.str();
    }

    std::stringstream decimalText;
    decimalText << std::fixed << std::setprecision(1) << value;
    return decimalText.str();
}

std::string GetRelationBadgeText(float value)
{
    if (value <= -75.0f)
    {
        return "HATED";
    }

    if (value < 0.0f)
    {
        return "HOSTILE";
    }

    if (value >= 75.0f)
    {
        return "ALLIED";
    }

    if (value > 0.0f)
    {
        return "FRIENDLY";
    }

    return "NEUTRAL";
}

int GetSummaryBucket(float value)
{
    if (value < 0.0f)
    {
        return -1;
    }

    if (value > 0.0f)
    {
        return 1;
    }

    return 0;
}

struct RelationAscendingComparator
{
    bool operator()(
        const HiddenFactionRelationsUiRow& left,
        const HiddenFactionRelationsUiRow& right) const
    {
        if (left.relationValue != right.relationValue)
        {
            return left.relationValue < right.relationValue;
        }

        return left.factionName < right.factionName;
    }
};
}

HiddenFactionRelationsUiSummary::HiddenFactionRelationsUiSummary()
    : shownHiddenFactions(0)
    , scannedHiddenFactions(0)
    , hostileCount(0)
    , friendlyCount(0)
    , neutralCount(0)
{
}

bool HiddenFactionRelationsUiModel_BuildDefaultView(
    const HiddenFactionRelationsSnapshot& snapshot,
    HiddenFactionRelationsUiView* outView)
{
    if (outView == 0)
    {
        return false;
    }

    *outView = HiddenFactionRelationsUiView();
    outView->playerFactionName = snapshot.playerFactionName;
    outView->playerFactionId = snapshot.playerFactionId;
    outView->summary.scannedHiddenFactions = snapshot.hiddenFactions;

    for (size_t i = 0; i < snapshot.factions.size(); ++i)
    {
        const HiddenFactionRelationEntry& entry = snapshot.factions[i];
        if (entry.isNullEntry || !entry.isHidden || !entry.hasPlayerRelation)
        {
            continue;
        }

        if (IsEffectivelyZero(entry.playerRelation))
        {
            continue;
        }

        HiddenFactionRelationsUiRow row;
        row.factionName = entry.factionName;
        row.relationValueText = FormatRelationValue(entry.playerRelation);
        row.relationBadgeText = GetRelationBadgeText(entry.playerRelation);
        row.relationValue = entry.playerRelation;
        row.hasPlayerRelation = entry.hasPlayerRelation;
        outView->rows.push_back(row);

        const int bucket = GetSummaryBucket(entry.playerRelation);
        if (bucket < 0)
        {
            ++outView->summary.hostileCount;
        }
        else if (bucket > 0)
        {
            ++outView->summary.friendlyCount;
        }
        else
        {
            ++outView->summary.neutralCount;
        }
    }

    std::stable_sort(outView->rows.begin(), outView->rows.end(), RelationAscendingComparator());
    outView->summary.shownHiddenFactions = static_cast<int>(outView->rows.size());
    return true;
}
