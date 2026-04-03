#include "HiddenFactionRelationsUiModel.h"

#include <algorithm>
#include <cmath>
#include <cctype>
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

struct RelationDescendingComparator
{
    bool operator()(
        const HiddenFactionRelationsUiRow& left,
        const HiddenFactionRelationsUiRow& right) const
    {
        if (left.relationValue != right.relationValue)
        {
            return left.relationValue > right.relationValue;
        }

        return left.factionName < right.factionName;
    }
};

struct NameAscendingComparator
{
    bool operator()(
        const HiddenFactionRelationsUiRow& left,
        const HiddenFactionRelationsUiRow& right) const
    {
        return left.factionName < right.factionName;
    }
};

struct NameDescendingComparator
{
    bool operator()(
        const HiddenFactionRelationsUiRow& left,
        const HiddenFactionRelationsUiRow& right) const
    {
        return left.factionName > right.factionName;
    }
};

std::string ToAsciiLower(const std::string& text)
{
    std::string lowered = text;
    for (size_t i = 0; i < lowered.size(); ++i)
    {
        lowered[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
    }
    return lowered;
}

bool MatchesSearch(const std::string& query, const std::string& value)
{
    if (query.empty())
    {
        return true;
    }

    return ToAsciiLower(value).find(query) != std::string::npos;
}
}

HiddenFactionRelationsUiSummary::HiddenFactionRelationsUiSummary()
    : shownHiddenFactions(0)
    , scannedHiddenFactions(0)
    , hostileCount(0)
    , friendlyCount(0)
    , neutralCount(0)
{
}

HiddenFactionRelationsUiOptions::HiddenFactionRelationsUiOptions()
    : sortMode(HiddenFactionRelationsUiSort_RelationAscending)
    , nonZeroOnly(true)
{
}

bool HiddenFactionRelationsUiModel_BuildView(
    const HiddenFactionRelationsSnapshot& snapshot,
    const HiddenFactionRelationsUiOptions& options,
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
    const std::string searchQuery = ToAsciiLower(options.searchText);

    for (size_t i = 0; i < snapshot.factions.size(); ++i)
    {
        const HiddenFactionRelationEntry& entry = snapshot.factions[i];
        if (entry.isNullEntry || !entry.isHidden || !entry.hasPlayerRelation)
        {
            continue;
        }

        if (options.nonZeroOnly && IsEffectivelyZero(entry.playerRelation))
        {
            continue;
        }

        if (!MatchesSearch(searchQuery, entry.factionName))
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

    switch (options.sortMode)
    {
    case HiddenFactionRelationsUiSort_RelationDescending:
        std::stable_sort(outView->rows.begin(), outView->rows.end(), RelationDescendingComparator());
        break;
    case HiddenFactionRelationsUiSort_NameAscending:
        std::stable_sort(outView->rows.begin(), outView->rows.end(), NameAscendingComparator());
        break;
    case HiddenFactionRelationsUiSort_NameDescending:
        std::stable_sort(outView->rows.begin(), outView->rows.end(), NameDescendingComparator());
        break;
    case HiddenFactionRelationsUiSort_RelationAscending:
    default:
        std::stable_sort(outView->rows.begin(), outView->rows.end(), RelationAscendingComparator());
        break;
    }
    outView->summary.shownHiddenFactions = static_cast<int>(outView->rows.size());
    return true;
}
