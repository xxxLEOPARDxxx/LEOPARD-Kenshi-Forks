#include "HiddenFactionRelations.h"

#include <Debug.h>

#include <kenshi/Faction.h>
#include <kenshi/GameData.h>
#include <kenshi/GameWorld.h>
#include <kenshi/PlayerInterface.h>

#include <kenshi/util/lektor.h>

#include <cctype>
#include <sstream>

class FactionRelations
{
public:
    class RelationData
    {
    public:
        bool own;
        bool _0x1;
        bool _0x2;
        bool isCoexistence;
        float relation;
        float trust;
        float trustNeg;
        int _0x10;
    };

    virtual void saveState(GameData* state);
    virtual void loadState(GameData* state, bool isImport);
    virtual void setOwnFactionRelation();
    virtual void vfunc0x18(Faction*);
    virtual void addRelation(Faction* target, float amount, float mult);
    virtual void vfunc0x28(Faction* target, unsigned int type, float amount);
    virtual void addTrust(Faction* target, float amount, float mult);
    virtual void endWar(Faction* target);
    virtual void declareWar(Faction* target);
    virtual void addReputation(Faction* target, float amount);
    virtual RelationData* getRelationData(Faction* target);
};

namespace
{
const char* kPluginName = "Hidden-Faction-Relations";

void LogInvestigateLine(const std::string& message)
{
    std::stringstream line;
    line << kPluginName << " INFO: [investigate][relations] " << message;
    DebugLog(line.str().c_str());
}

bool IsBaseGameFactionId(const std::string& factionId)
{
    const std::string suffix = "gamedata.base";
    if (factionId.size() < suffix.size())
    {
        return false;
    }

    return factionId.compare(factionId.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string GetFactionDisplayName(Faction* faction)
{
    if (faction == 0)
    {
        return "<null>";
    }

    const std::string& factionName = faction->getName();
    if (!factionName.empty())
    {
        return factionName;
    }

    GameData* data = faction->getData();
    if (data != 0 && !data->name.empty())
    {
        return data->name;
    }

    return "<unnamed>";
}

std::string GetFactionStringId(Faction* faction)
{
    if (faction == 0)
    {
        return "<null>";
    }

    GameData* data = faction->getData();
    if (data != 0 && !data->stringID.empty())
    {
        return data->stringID;
    }

    return "<missing>";
}
}

HiddenFactionRelationsSnapshot::HiddenFactionRelationsSnapshot()
    : totalFactions(0)
    , hiddenFactions(0)
    , visibleFactions(0)
    , nullEntries(0)
    , missingRelationEntries(0)
{
}

bool HiddenFactionRelations_TryCollectSnapshot(
    GameWorld* gameWorld,
    HiddenFactionRelationsSnapshot* outSnapshot)
{
    if (outSnapshot == 0)
    {
        return false;
    }

    *outSnapshot = HiddenFactionRelationsSnapshot();

    if (gameWorld == 0 || !gameWorld->initialized || gameWorld->factionMgr == 0 || gameWorld->player == 0)
    {
        return false;
    }

    Faction* playerFaction = gameWorld->player->getFaction();
    if (playerFaction == 0)
    {
        return false;
    }

    const lektor<Faction*>* factions = gameWorld->factionMgr->getAllFactions();
    if (factions == 0 || !factions->valid())
    {
        return false;
    }

    outSnapshot->playerFactionId = GetFactionStringId(playerFaction);
    outSnapshot->playerFactionName = GetFactionDisplayName(playerFaction);
    outSnapshot->totalFactions = static_cast<int>(factions->size());
    outSnapshot->factions.reserve(factions->size());

    for (unsigned int i = 0; i < factions->size(); ++i)
    {
        HiddenFactionRelationEntry entry;
        entry.index = static_cast<int>(i);
        entry.isNullEntry = false;
        entry.isHidden = false;
        entry.isPlayerFaction = false;
        entry.hasPlayerRelation = false;
        entry.playerRelation = 0.0f;
        entry.factionId.clear();
        entry.factionName.clear();

        Faction* faction = (*factions)[i];
        if (faction == 0)
        {
            entry.isNullEntry = true;
            entry.factionId = "<null>";
            entry.factionName = "<null>";
            ++outSnapshot->nullEntries;
            outSnapshot->factions.push_back(entry);
            continue;
        }

        entry.factionId = GetFactionStringId(faction);
        entry.factionName = GetFactionDisplayName(faction);
        entry.isHidden = faction->isNotARealFaction();
        entry.isPlayerFaction = (faction == playerFaction || faction->isThePlayer());

        if (entry.isHidden)
        {
            ++outSnapshot->hiddenFactions;
        }
        else
        {
            ++outSnapshot->visibleFactions;
        }

        if (faction->relations != 0)
        {
            FactionRelations::RelationData* relationData = faction->relations->getRelationData(playerFaction);
            if (relationData != 0)
            {
                entry.hasPlayerRelation = true;
                entry.playerRelation = relationData->relation;
            }
            else
            {
                ++outSnapshot->missingRelationEntries;
            }
        }
        else
        {
            ++outSnapshot->missingRelationEntries;
        }

        outSnapshot->factions.push_back(entry);
    }

    return true;
}


