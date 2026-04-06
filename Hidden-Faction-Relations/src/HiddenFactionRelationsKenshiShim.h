#pragma once

#include <kenshi/util/lektor.h>

#include <string>

class GameDataContainer;

class GameData
{
public:
    virtual ~GameData();

    int validity;
    GameDataContainer* sourceContainer;
    bool isStandalone;
    char _pad18[3];
    int id;
    bool readOnly;
    char _pad21[7];
    std::string name;
    int type;
    char _pad54[4];
    std::string stringID;
};

class Faction;

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
    virtual void vfunc0x18(Faction* faction);
    virtual void addRelation(Faction* target, float amount, float mult);
    virtual void vfunc0x28(Faction* target, unsigned int type, float amount);
    virtual void addTrust(Faction* target, float amount, float mult);
    virtual void endWar(Faction* target);
    virtual void declareWar(Faction* target);
    virtual void addReputation(Faction* target, float amount);
    virtual RelationData* getRelationData(Faction* target);
};

class Faction
{
public:
    const std::string& getName();
    GameData* getData() const;
    bool isThePlayer() const;
    bool isNotARealFaction() const;

    char _pad00[0x78];
    FactionRelations* relations;
};

class PlayerInterface
{
public:
    Faction* getFaction() const;
};

class FactionManager
{
public:
    const lektor<Faction*>* getAllFactions();
};

class GameWorld
{
public:
    char _pad00[0xC];
    bool initialized;
    char _pad0D[0x49B];
    FactionManager* factionMgr;
    char _pad4B0[0xD0];
    PlayerInterface* player;
};
