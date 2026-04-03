#pragma once

#include <string>
#include <vector>

class GameWorld;

struct HiddenFactionRelationEntry
{
    int index;
    bool isNullEntry;
    bool isHidden;
    bool isPlayerFaction;
    bool hasPlayerRelation;
    std::string factionId;
    std::string factionName;
    float playerRelation;
};

struct HiddenFactionRelationsSnapshot
{
    std::string playerFactionId;
    std::string playerFactionName;
    int totalFactions;
    int hiddenFactions;
    int visibleFactions;
    int nullEntries;
    int missingRelationEntries;
    std::vector<HiddenFactionRelationEntry> factions;

    HiddenFactionRelationsSnapshot();
};

bool HiddenFactionRelations_TryCollectSnapshot(
    GameWorld* gameWorld,
    HiddenFactionRelationsSnapshot* outSnapshot);

bool HiddenFactionRelations_TryLogAllFactionProbe(GameWorld* gameWorld);
