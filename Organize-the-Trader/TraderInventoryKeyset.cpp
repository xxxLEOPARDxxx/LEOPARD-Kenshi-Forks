#include "TraderInventoryBinding.h"

#include "TraderSearchText.h"
#include "TraderWindowDetection.h"

#include <kenshi/Building.h>
#include <kenshi/Character.h>
#include <kenshi/Dialogue.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/Inventory.h>
#include <kenshi/Item.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/RootObject.h>

#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>
#include <mygui/MyGUI_InputManager.h>

#include <algorithm>
#include <sstream>
#include <vector>

// Minimal ownership interface stub to avoid including Platoon.h
// (it conflicts with Building.h enum declarations in this SDK drop).
class Ownerships
{
public:
    void getHomeFurnitureOfType(lektor<Building*>& out, BuildingFunction type);
    void getBuildingsWithFunction(lektor<Building*>& out, BuildingFunction bf);
};

void AddInventoryCandidateUnique(
    std::vector<InventoryCandidateInfo>* candidates,
    Inventory* inventory,
    const std::string& source,
    bool traderPreferred,
    bool visible,
    int priorityBias);

const char* ItemTypeNameForLog(itemType type);

void AddBuildingInventoryCandidate(
    Building* building,
    const char* sourceTag,
    const std::string& traderName,
    bool traderPreferred,
    int priorityBias,
    std::vector<InventoryCandidateInfo>* outCandidates);

#define g_searchQueryNormalized (TraderState().search.g_searchQueryNormalized)
#define g_lastInventoryKeysetSelectionSignature (TraderState().search.g_lastInventoryKeysetSelectionSignature)
#define g_lastInventoryKeysetLowCoverageSignature (TraderState().search.g_lastInventoryKeysetLowCoverageSignature)
#define g_lastCoverageFallbackDecisionSignature (TraderState().search.g_lastCoverageFallbackDecisionSignature)
#define g_lastKeysetLockSignature (TraderState().search.g_lastKeysetLockSignature)
#define g_lockedKeysetTraderParent (TraderState().search.g_lockedKeysetTraderParent)
#define g_lockedKeysetStage (TraderState().search.g_lockedKeysetStage)
#define g_lockedKeysetSourceId (TraderState().search.g_lockedKeysetSourceId)
#define g_lockedKeysetSourcePreview (TraderState().search.g_lockedKeysetSourcePreview)
#define g_lockedKeysetExpectedCount (TraderState().search.g_lockedKeysetExpectedCount)

namespace
{
std::size_t CountNonEmptyKeysLocal(const std::vector<std::string>& keys)
{
    std::size_t count = 0;
    for (std::size_t index = 0; index < keys.size(); ++index)
    {
        if (!keys[index].empty())
        {
            ++count;
        }
    }
    return count;
}
}

bool IsShopCounterCandidateSource(const std::string& sourceLower)
{
    if (sourceLower.empty())
    {
        return false;
    }

    return sourceLower.find("shop counter") != std::string::npos
        || sourceLower.find("owner=shop counter") != std::string::npos
        || sourceLower.find("trader_furniture_shop") != std::string::npos;
}

bool IsTraderAnchoredCandidateSource(const std::string& sourceLower)
{
    if (sourceLower.empty())
    {
        return false;
    }

    if (sourceLower.find("self inventory true") != std::string::npos
        || sourceLower.find("trader home caption") != std::string::npos
        || sourceLower.find("caption trader") != std::string::npos
        || sourceLower.find("dialog target") != std::string::npos
        || sourceLower.find("widget") != std::string::npos)
    {
        return true;
    }

    return sourceLower.find("active char") != std::string::npos
        && sourceLower.find("trader true") != std::string::npos;
}

bool IsRiskyCoverageFallbackSource(const std::string& sourceLower)
{
    if (sourceLower.empty())
    {
        return false;
    }

    if (sourceLower.find("active char") != std::string::npos
        && sourceLower.find("trader false") != std::string::npos)
    {
        return true;
    }

    return sourceLower.find("nearby shop") != std::string::npos
        || sourceLower.find("nearby caption") != std::string::npos
        || sourceLower.find("nearby dialog") != std::string::npos
        || sourceLower.find("nearby world") != std::string::npos
        || sourceLower.find("root candidate") != std::string::npos;
}

std::string StripInventorySourceDiagnostics(const std::string& source)
{
    static const char* kSuffixTokens[] =
    {
        " aligned_matches=",
        " query_matches=",
        " coverage_fallback=",
        " non_empty=",
        " nearby_shop_candidates=",
        " nearby_shop_scanned=",
        " nearby_shop_with_inventory=",
        " nearby_focused=",
        " nearby_candidates=",
        " nearby_scanned=",
        " nearby_with_inventory="
    };

    std::size_t cut = source.size();
    for (std::size_t i = 0; i < sizeof(kSuffixTokens) / sizeof(kSuffixTokens[0]); ++i)
    {
        const std::size_t tokenPos = source.find(kSuffixTokens[i]);
        if (tokenPos != std::string::npos && tokenPos < cut)
        {
            cut = tokenPos;
        }
    }

    while (cut > 0 && source[cut - 1] == ' ')
    {
        --cut;
    }
    return source.substr(0, cut);
}

std::string BuildKeysetSourceId(const std::string& source)
{
    return NormalizeSearchText(StripInventorySourceDiagnostics(source));
}

void AddCandidateRootObjectUnique(std::vector<RootObject*>* candidates, RootObject* candidate)
{
    if (candidates == 0 || candidate == 0)
    {
        return;
    }

    for (std::size_t index = 0; index < candidates->size(); ++index)
    {
        if ((*candidates)[index] == candidate)
        {
            return;
        }
    }

    candidates->push_back(candidate);
}

void ClearLockedKeysetSource()
{
    g_lockedKeysetTraderParent = 0;
    g_lockedKeysetStage.clear();
    g_lockedKeysetSourceId.clear();
    g_lockedKeysetSourcePreview.clear();
    g_lockedKeysetExpectedCount = 0;
    g_lastKeysetLockSignature.clear();
}

bool TryResolveTraderInventoryNameKeys(
    MyGUI::Widget* traderParent,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys,
    bool preferCoverageFallbackWhenWidgetOpaque)
{
    if (traderParent == 0 || outKeys == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    outKeys->clear();
    if (outSource != 0)
    {
        outSource->clear();
    }
    if (outQuantityKeys != 0)
    {
        outQuantityKeys->clear();
    }

    if (g_lockedKeysetTraderParent != 0 && g_lockedKeysetTraderParent != traderParent)
    {
        ClearLockedKeysetSource();
    }

    std::vector<std::string> bestKeys;
    std::vector<QuantityNameKey> bestQuantityKeys;
    std::string bestSource;
    std::string bestStage;
    int bestScore = -1000000;
    std::vector<std::string> bestCoverageKeys;
    std::vector<QuantityNameKey> bestCoverageQuantityKeys;
    std::string bestCoverageSource;
    std::string bestCoverageStage;
    int bestCoverageScore = -1000000;
    int bestQueryMatchCount = -1;
    int bestCoverageQueryMatchCount = -1;
    bool usedCoverageFallback = false;
    std::vector<std::string> candidateDiagnostics;

    const bool hasLockedSourceForQuery =
        !g_searchQueryNormalized.empty()
        && g_lockedKeysetTraderParent == traderParent
        && !g_lockedKeysetSourceId.empty()
        && g_lockedKeysetExpectedCount > 0
        && expectedEntryCount > 0
        && AbsoluteDiffSize(g_lockedKeysetExpectedCount, expectedEntryCount) <= 6;

    struct ResolvedKeySetCandidate
    {
        const char* stage;
        std::vector<std::string> keys;
        std::vector<QuantityNameKey> quantityKeys;
        std::string source;
    };

    ResolvedKeySetCandidate stageCandidate;

    auto considerCandidate = [&](const ResolvedKeySetCandidate& candidate)
    {
        if (candidate.keys.empty())
        {
            return;
        }

        const int keyCount = static_cast<int>(candidate.keys.size());
        int nonEmptyKeyCount = 0;
        for (std::size_t keyIndex = 0; keyIndex < candidate.keys.size(); ++keyIndex)
        {
            if (!candidate.keys[keyIndex].empty())
            {
                ++nonEmptyKeyCount;
            }
        }
        const int expected = static_cast<int>(expectedEntryCount);
        const int diff = expected > 0
            ? (nonEmptyKeyCount > expected ? nonEmptyKeyCount - expected : expected - nonEmptyKeyCount)
            : 0;
        const bool lowCoverage = expected >= 8 && nonEmptyKeyCount * 2 < expected;
        const int emptyKeyCount = keyCount - nonEmptyKeyCount;
        const int sourceQueryMatches = ExtractTaggedIntValue(candidate.source, "query_matches=");
        int keyQueryMatches = 0;
        if (!g_searchQueryNormalized.empty())
        {
            for (std::size_t keyIndex = 0; keyIndex < candidate.keys.size(); ++keyIndex)
            {
                if (!candidate.keys[keyIndex].empty()
                    && candidate.keys[keyIndex].find(g_searchQueryNormalized) != std::string::npos)
                {
                    ++keyQueryMatches;
                }
            }
        }
        const int effectiveQueryMatches =
            sourceQueryMatches >= 0 ? sourceQueryMatches : keyQueryMatches;
        int sourceAlignedMatches = -1;
        int sourceAlignedTotal = -1;
        const bool hasSourceAlignedMatches =
            TryExtractTaggedFraction(
                candidate.source,
                "aligned_matches=",
                &sourceAlignedMatches,
                &sourceAlignedTotal);

        int score = 0;
        score += nonEmptyKeyCount * 18;
        score -= emptyKeyCount * 110;
        score -= diff * 64;

        const std::string stageName = candidate.stage == 0 ? "" : candidate.stage;
        if (stageName == "widget")
        {
            score += 1800;
        }
        else if (stageName == "section_widget")
        {
            score += 5200;
        }
        else if (stageName == "recent_refresh")
        {
            score += 4200;
        }
        else if (stageName == "hovered_widget")
        {
            score += 3200;
        }
        else if (stageName == "ownership")
        {
            score += 900;
        }
        else if (stageName == "nearby_shop_counter")
        {
            score += 700;
        }
        else if (stageName == "nearby")
        {
            score -= 700;
        }

        if (expected > 0 && nonEmptyKeyCount == expected)
        {
            score += 1500;
        }
        else if (diff <= 1)
        {
            score += 520;
        }
        else if (lowCoverage)
        {
            score -= 3200;
        }

        if (!g_searchQueryNormalized.empty() && g_searchQueryNormalized.size() >= 3)
        {
            if (effectiveQueryMatches > 0)
            {
                score += 2600;
                score += effectiveQueryMatches * 760;
                if (effectiveQueryMatches >= 2)
                {
                    score += 620;
                }
            }
            else if (effectiveQueryMatches == 0)
            {
                score -= 3200;
                if (stageName == "nearby_shop_counter")
                {
                    score -= 4200;
                }
            }

            if (hasSourceAlignedMatches
                && sourceAlignedTotal >= 10
                && sourceAlignedMatches * 3 < sourceAlignedTotal)
            {
                score -= 1400;
                if (stageName == "nearby_shop_counter")
                {
                    score -= 1800;
                }
            }
        }

        if (hasSourceAlignedMatches && sourceAlignedTotal >= 10)
        {
            if (sourceAlignedMatches * 3 < sourceAlignedTotal)
            {
                score -= 2600;
                if (stageName == "nearby_shop_counter")
                {
                    score -= 6200;
                }
                else if (stageName == "nearby")
                {
                    score -= 2600;
                }
            }
            else if (sourceAlignedMatches * 2 >= sourceAlignedTotal)
            {
                score += 900;
            }
        }

        const std::string sourceLower = NormalizeSearchText(candidate.source);
        const std::string sourceId = BuildKeysetSourceId(candidate.source);
        const bool sourceIsActiveNonTrader =
            sourceLower.find("active char") != std::string::npos
            && sourceLower.find("trader false") != std::string::npos;

        if (preferCoverageFallbackWhenWidgetOpaque && sourceIsActiveNonTrader)
        {
            return;
        }
        if (!sourceLower.empty())
        {
            if (sourceLower.find("self inventory true") != std::string::npos)
            {
                if (expected >= 8 && nonEmptyKeyCount * 3 < expected * 2)
                {
                    score -= 2800;
                }
                else
                {
                    score += 700;
                }
            }
            if (IsShopCounterCandidateSource(sourceLower))
            {
                score += 520;
                if (expected >= 8)
                {
                    if (diff <= 2)
                    {
                        score += 360;
                    }
                    else if (diff <= 5)
                    {
                        score += 220;
                    }
                }
            }
            if (sourceLower.find("trader furniture shop") != std::string::npos)
            {
                score += 900;
            }
            if (sourceLower.find("trader furniture storage") != std::string::npos)
            {
                score -= 360;
            }
            if (sourceLower.find("trader furniture resource") != std::string::npos)
            {
                score -= 520;
            }
            if (sourceLower.find("nearby world") != std::string::npos)
            {
                score -= 1800;
            }
            if (sourceLower.find("nearby caption") != std::string::npos)
            {
                score += 280;
            }
            if (sourceLower.find("nearby dialog") != std::string::npos)
            {
                score += 360;
            }
            if (sourceLower.find("caption trader") != std::string::npos)
            {
                score += 160;
            }
            if (sourceLower.find("dialog target") != std::string::npos
                && sourceLower.find("trader true") != std::string::npos)
            {
                score += 260;
            }
            if (sourceLower.find("active char") != std::string::npos
                && sourceLower.find("trader true") != std::string::npos)
            {
                score += 140;
            }
            if (sourceIsActiveNonTrader)
            {
                score -= 5200;
            }
            if (sourceLower.find("selected item") != std::string::npos)
            {
                score += 520;
            }
            if (sourceLower.find("widget") != std::string::npos)
            {
                score += 880;
            }
            if (sourceLower.find("section widget map") != std::string::npos)
            {
                score += 1800;
            }
            if (sourceLower.find("recent refresh") != std::string::npos)
            {
                score += 1400;
            }
            if (sourceLower.find("visible true") != std::string::npos)
            {
                score += 120;
            }
            if (sourceLower.find("nearby") != std::string::npos)
            {
                score += 100;
            }
            if (sourceLower.find("shop counter focus true") != std::string::npos)
            {
                score += 300;
            }
            if (sourceLower.find("root candidate") != std::string::npos)
            {
                score -= 180;
            }
            if (sourceLower.find("selected true") != std::string::npos
                && sourceLower.find("trader true") == std::string::npos)
            {
                score -= 120;
            }
        }

        int lockBoost = 0;
        if (hasLockedSourceForQuery)
        {
            const bool stageMatchesLock = !g_lockedKeysetStage.empty() && stageName == g_lockedKeysetStage;
            const bool sourceMatchesLock = !sourceId.empty() && sourceId == g_lockedKeysetSourceId;

            if (sourceMatchesLock && stageMatchesLock)
            {
                lockBoost = 6200;
            }
            else if (sourceMatchesLock)
            {
                lockBoost = 4200;
            }
            else if (stageMatchesLock)
            {
                lockBoost = 1200;
            }

            if (lockBoost > 0)
            {
                score += lockBoost;
            }
            else if (IsRiskyCoverageFallbackSource(sourceLower))
            {
                score -= 1600;
            }
        }

        {
            std::stringstream diag;
            diag << "inventory keyset candidate stage=" << candidate.stage
                 << " key_count=" << candidate.keys.size()
                 << " non_empty=" << nonEmptyKeyCount
                 << " expected=" << expectedEntryCount
                 << " score=" << score
                 << " lock_boost=" << lockBoost
                 << " source=\"" << TruncateForLog(candidate.source, 220) << "\"";
            candidateDiagnostics.push_back(diag.str());
        }

        if (score > bestScore)
        {
            bestScore = score;
            bestKeys = candidate.keys;
            bestQuantityKeys = candidate.quantityKeys;
            bestSource = candidate.source;
            bestStage = stageName;
            bestQueryMatchCount = effectiveQueryMatches;
        }

        if (!lowCoverage && score > bestCoverageScore)
        {
            bestCoverageScore = score;
            bestCoverageKeys = candidate.keys;
            bestCoverageQuantityKeys = candidate.quantityKeys;
            bestCoverageSource = candidate.source;
            bestCoverageStage = stageName;
            bestCoverageQueryMatchCount = effectiveQueryMatches;
        }
    };

    stageCandidate.stage = "recent_refresh";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromRecentRefreshedInventories(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "section_widget";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromSectionWidgetMap(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "widget";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromWidgetBindings(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "hovered_widget";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromHoveredWidget(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "caption";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromWindowCaption(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "dialogue";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromDialogue(
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "active";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromActiveCharacters(
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "nearby_shop_counter";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromNearbyShopCounters(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "ownership";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromTraderOwnership(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "selected_item";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromSelectedItemHandles(
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "nearby";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromNearbyObjects(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        if (g_searchQueryNormalized.empty())
        {
            considerCandidate(stageCandidate);
        }
    }

    std::vector<RootObject*> candidates;
    AddCandidateRootObjectUnique(&candidates, ou->guiDisplayObject.getRootObject());
    AddCandidateRootObjectUnique(&candidates, ou->player->selectedObject.getRootObject());
    AddCandidateRootObjectUnique(&candidates, ou->player->selectedCharacter.getRootObject());
    AddCandidateRootObjectUnique(&candidates, ou->player->mouseRightTarget);

    std::vector<InventoryCandidateInfo> inventoryCandidates;
    for (std::size_t candidateIndex = 0; candidateIndex < candidates.size(); ++candidateIndex)
    {
        RootObject* owner = candidates[candidateIndex];
        if (owner == 0)
        {
            continue;
        }

        Inventory* inventory = owner->getInventory();
        if (inventory == 0)
        {
            continue;
        }

        std::stringstream src;
        src << "root_candidate:" << RootObjectDisplayNameForLog(owner)
            << " visible=" << (inventory->isVisible() ? "true" : "false");
        AddInventoryCandidateUnique(
            &inventoryCandidates,
            inventory,
            src.str(),
            false,
            inventory->isVisible());
    }

    stageCandidate.stage = "root";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveInventoryNameKeysFromCandidates(
            inventoryCandidates,
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    if (bestKeys.empty())
    {
        return false;
    }

    if (expectedEntryCount >= 8
        && bestKeys.size() * 2 < expectedEntryCount
        && !bestCoverageKeys.empty())
    {
        bool allowCoverageFallback = true;
        std::string fallbackSkipReason;
        const int currentMatches = bestQueryMatchCount < 0 ? 0 : bestQueryMatchCount;
        const int coverageMatches = bestCoverageQueryMatchCount < 0 ? 0 : bestCoverageQueryMatchCount;
        if (!g_searchQueryNormalized.empty())
        {
            if (coverageMatches < currentMatches)
            {
                allowCoverageFallback = false;
                fallbackSkipReason = "query_match_regression";
            }
        }

        const std::string bestSourceLower = NormalizeSearchText(bestSource);
        const std::string bestCoverageSourceLower = NormalizeSearchText(bestCoverageSource);
        const std::string bestSourceId = BuildKeysetSourceId(bestSource);
        const std::string bestCoverageSourceId = BuildKeysetSourceId(bestCoverageSource);
        const bool currentSourceTraderAnchored = IsTraderAnchoredCandidateSource(bestSourceLower);
        const bool coverageSourceRisky = IsRiskyCoverageFallbackSource(bestCoverageSourceLower);
        const bool coverageSourceNonTraderActive =
            bestCoverageSourceLower.find("active char") != std::string::npos
            && bestCoverageSourceLower.find("trader false") != std::string::npos;
        const bool currentSourceMatchesLock =
            hasLockedSourceForQuery
            && !bestSourceId.empty()
            && bestSourceId == g_lockedKeysetSourceId;
        const bool coverageSourceMatchesLock =
            hasLockedSourceForQuery
            && !bestCoverageSourceId.empty()
            && bestCoverageSourceId == g_lockedKeysetSourceId;
        int currentAlignedMatches = -1;
        int currentAlignedTotal = -1;
        const bool hasCurrentAlignedMatches =
            TryExtractTaggedFraction(
                bestSource,
                "aligned_matches=",
                &currentAlignedMatches,
                &currentAlignedTotal);

        const bool strongQueryEvidence =
            !g_searchQueryNormalized.empty()
            && g_searchQueryNormalized.size() >= 4
            && coverageMatches >= 3
            && coverageMatches >= currentMatches + 2;

        if (allowCoverageFallback && coverageSourceNonTraderActive && !strongQueryEvidence)
        {
            allowCoverageFallback = false;
            fallbackSkipReason = "coverage_non_trader_active";
        }

        if (allowCoverageFallback
            && currentSourceMatchesLock
            && !coverageSourceMatchesLock
            && !strongQueryEvidence)
        {
            allowCoverageFallback = false;
            fallbackSkipReason = "locked_source_preserved";
        }

        if (allowCoverageFallback && currentSourceTraderAnchored && coverageSourceRisky && !strongQueryEvidence)
        {
            allowCoverageFallback = false;
            fallbackSkipReason = "coverage_source_risky_for_trader_anchor";
        }

        int coverageAlignedMatches = -1;
        int coverageAlignedTotal = -1;
        const bool hasCoverageAlignedMatches =
            TryExtractTaggedFraction(
                bestCoverageSource,
                "aligned_matches=",
                &coverageAlignedMatches,
                &coverageAlignedTotal);
        if (allowCoverageFallback
            && hasCoverageAlignedMatches
            && coverageAlignedTotal >= 10
            && coverageAlignedMatches * 2 < coverageAlignedTotal
            && !(strongQueryEvidence && coverageMatches >= 4))
        {
            allowCoverageFallback = false;
            fallbackSkipReason = "coverage_alignment_too_low";
        }

        if (!allowCoverageFallback
            && preferCoverageFallbackWhenWidgetOpaque
            && coverageSourceRisky
            && !coverageSourceNonTraderActive
            && expectedEntryCount >= 8)
        {
            const bool coverageHasStrongCount =
                bestCoverageKeys.size() * 3 >= expectedEntryCount * 2;
            const bool coverageQueryNotWorse = coverageMatches >= currentMatches;
            const bool coverageAlignmentNotWorse =
                !hasCoverageAlignedMatches
                || !hasCurrentAlignedMatches
                || currentAlignedTotal <= 0
                || coverageAlignedTotal <= 0
                || (coverageAlignedMatches * currentAlignedTotal
                    >= currentAlignedMatches * coverageAlignedTotal);
            if (coverageHasStrongCount && coverageQueryNotWorse && coverageAlignmentNotWorse)
            {
                allowCoverageFallback = true;
                fallbackSkipReason.clear();
            }
        }

        std::stringstream decisionSignature;
        decisionSignature
            << (allowCoverageFallback ? "allow" : "skip")
            << "|" << bestKeys.size()
            << "|" << bestCoverageKeys.size()
            << "|" << currentMatches
            << "|" << coverageMatches
            << "|" << bestSource
            << "|" << bestCoverageSource
            << "|" << g_searchQueryNormalized;
        const bool logDecision =
            decisionSignature.str() != g_lastCoverageFallbackDecisionSignature;
        g_lastCoverageFallbackDecisionSignature = decisionSignature.str();

        if (allowCoverageFallback)
        {
            if (logDecision)
            {
                std::stringstream line;
                line << "inventory keyset fallback selected key_count=" << bestCoverageKeys.size()
                     << " expected=" << expectedEntryCount
                     << " current_query_matches=" << currentMatches
                     << " coverage_query_matches=" << coverageMatches
                     << " opaque_prefer_mode=" << (preferCoverageFallbackWhenWidgetOpaque ? "true" : "false")
                     << " replacing_source=\"" << TruncateForLog(bestSource, 220) << "\""
                     << " with_source=\"" << TruncateForLog(bestCoverageSource, 220) << "\"";
                LogWarnLine(line.str());
            }

            bestKeys.swap(bestCoverageKeys);
            bestQuantityKeys.swap(bestCoverageQuantityKeys);
            bestSource = bestCoverageSource;
            bestStage = bestCoverageStage;
            bestScore = bestCoverageScore;
            bestQueryMatchCount = bestCoverageQueryMatchCount;
            usedCoverageFallback = true;
        }
        else
        {
            if (logDecision)
            {
                std::stringstream line;
                line << "inventory keyset fallback skipped reason="
                     << (fallbackSkipReason.empty() ? "unknown" : fallbackSkipReason)
                     << " current_query_matches=" << currentMatches
                     << " coverage_query_matches=" << coverageMatches
                     << " opaque_prefer_mode=" << (preferCoverageFallbackWhenWidgetOpaque ? "true" : "false")
                     << " current_source=\"" << TruncateForLog(bestSource, 160) << "\""
                     << " coverage_source=\"" << TruncateForLog(bestCoverageSource, 160) << "\"";
                LogWarnLine(line.str());
            }
        }
    }
    else
    {
        g_lastCoverageFallbackDecisionSignature.clear();
    }

    if (expectedEntryCount >= 8 && bestKeys.size() * 2 < expectedEntryCount)
    {
        std::stringstream signature;
        signature << bestKeys.size() << "|" << expectedEntryCount << "|"
                  << bestSource << "|" << g_searchQueryNormalized;
        if (signature.str() != g_lastInventoryKeysetLowCoverageSignature)
        {
            std::stringstream line;
            line << "inventory keyset low-coverage key_count=" << bestKeys.size()
                 << " expected=" << expectedEntryCount
                 << " source=\"" << TruncateForLog(bestSource, 220) << "\""
                 << " continuing_with_partial_keys=true";
            LogWarnLine(line.str());
            g_lastInventoryKeysetLowCoverageSignature = signature.str();
        }
    }
    else
    {
        g_lastInventoryKeysetLowCoverageSignature.clear();
    }

    outKeys->swap(bestKeys);
    if (outSource != 0)
    {
        *outSource = bestSource;
    }
    if (outQuantityKeys != 0)
    {
        outQuantityKeys->swap(bestQuantityKeys);
    }

    const std::size_t selectedNonEmptyKeyCount = CountNonEmptyKeysLocal(*outKeys);
    const bool selectedLowCoverage =
        expectedEntryCount >= 8 && selectedNonEmptyKeyCount * 2 < expectedEntryCount;
    const std::string selectedSourceId = BuildKeysetSourceId(bestSource);

    if (g_searchQueryNormalized.empty())
    {
        if (!selectedSourceId.empty() && expectedEntryCount > 0 && !selectedLowCoverage)
        {
            const bool lockChanged =
                g_lockedKeysetTraderParent != traderParent
                || g_lockedKeysetStage != bestStage
                || g_lockedKeysetSourceId != selectedSourceId
                || g_lockedKeysetExpectedCount != expectedEntryCount;

            g_lockedKeysetTraderParent = traderParent;
            g_lockedKeysetStage = bestStage;
            g_lockedKeysetSourceId = selectedSourceId;
            g_lockedKeysetSourcePreview = StripInventorySourceDiagnostics(bestSource);
            g_lockedKeysetExpectedCount = expectedEntryCount;

            if (lockChanged)
            {
                std::stringstream line;
                line << "inventory keyset lock updated"
                     << " stage=" << (bestStage.empty() ? "<unknown>" : bestStage)
                     << " expected=" << expectedEntryCount
                     << " source=\"" << TruncateForLog(g_lockedKeysetSourcePreview, 220) << "\"";
                LogInfoLine(line.str());
            }
        }
    }
    else if (hasLockedSourceForQuery
             && !selectedSourceId.empty()
             && selectedSourceId != g_lockedKeysetSourceId)
    {
        std::stringstream lockSignature;
        lockSignature << "deviate|" << g_searchQueryNormalized
                      << "|" << selectedSourceId
                      << "|" << g_lockedKeysetSourceId
                      << "|" << expectedEntryCount;
        if (lockSignature.str() != g_lastKeysetLockSignature)
        {
            std::stringstream line;
            line << "inventory keyset lock deviation"
                 << " query=\"" << TruncateForLog(g_searchQueryNormalized, 64) << "\""
                 << " locked_stage=" << (g_lockedKeysetStage.empty() ? "<unknown>" : g_lockedKeysetStage)
                 << " selected_stage=" << (bestStage.empty() ? "<unknown>" : bestStage)
                 << " selected_source=\"" << TruncateForLog(StripInventorySourceDiagnostics(bestSource), 200) << "\""
                 << " locked_source=\"" << TruncateForLog(g_lockedKeysetSourcePreview, 200) << "\"";
            LogWarnLine(line.str());
            g_lastKeysetLockSignature = lockSignature.str();
        }
    }

    const std::size_t selectedKeyCount = outKeys->size();
    std::stringstream signature;
    signature << selectedKeyCount << "|" << expectedEntryCount << "|" << bestSource
              << "|" << (usedCoverageFallback ? "coverage_fallback" : "direct");
    if (signature.str() != g_lastInventoryKeysetSelectionSignature)
    {
        for (std::size_t index = 0; index < candidateDiagnostics.size(); ++index)
        {
            LogInfoLine(candidateDiagnostics[index]);
        }

        std::stringstream line;
        line << "inventory keyset selected key_count=" << selectedKeyCount
             << " expected=" << expectedEntryCount
             << " best_score=" << bestScore
             << " source=\"" << TruncateForLog(bestSource, 220) << "\"";
        LogInfoLine(line.str());

        std::stringstream previewLine;
        previewLine << "inventory keyset preview " << BuildKeyPreviewForLog(*outKeys, 14);
        LogInfoLine(previewLine.str());
        g_lastInventoryKeysetSelectionSignature = signature.str();
    }
    return true;
}
