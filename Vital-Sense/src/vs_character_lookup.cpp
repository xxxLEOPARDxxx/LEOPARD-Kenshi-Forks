#include "vs_character_lookup.h"

#include "vs_types.h"

#include <Windows.h>
#include <kenshi/Faction.h>
#include <kenshi/FactionRelations.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/PlayerInterface.h>

namespace vs_character_lookup
{

bool TryReadCharacterHandleSafe(Character* candidate, hand* handleOut)
{
    if (!candidate || !handleOut)
    {
        return false;
    }

    __try
    {
        *handleOut = candidate->getHandle();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return !handleOut->isNull();
}

Character* ResolveCharacterByHandleSafe(const hand& targetHandle)
{
    if (targetHandle.isNull())
    {
        return 0;
    }

    Character* candidate = 0;
    __try
    {
        candidate = targetHandle.getCharacter();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        candidate = 0;
    }

    if (candidate)
    {
        return candidate;
    }

    if (!ou)
    {
        return 0;
    }

    __try
    {
        candidate = ou->getFromDeathParade(targetHandle);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        candidate = 0;
    }

    return candidate;
}

bool IsAnimalCharacterSafe(Character* candidate)
{
    if (!candidate)
    {
        return false;
    }

    bool isAnimalCharacter = false;
    __try
    {
        isAnimalCharacter = (candidate->isAnimal() != 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        isAnimalCharacter = false;
    }

    return isAnimalCharacter;
}

bool IsCharacterInPlayerSquadSafe(Character* candidate)
{
    if (!candidate || !ou || !ou->player)
    {
        return false;
    }

    bool inPlayerSquad = false;
    __try
    {
        // A corpse can outlive its entry in the player's active roster.
        const Faction* playerFaction = ou->player->participant;
        if (playerFaction && candidate->owner == playerFaction)
        {
            return true;
        }

        const lektor<Character*>& playerCharacters = ou->player->playerCharacters;
        if (playerCharacters.valid())
        {
            for (lektor<Character*>::const_iterator it = playerCharacters.begin(); it != playerCharacters.end(); ++it)
            {
                if (*it == candidate)
                {
                    inPlayerSquad = true;
                    break;
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        inPlayerSquad = false;
    }

    return inPlayerSquad;
}

static int ReadFactionRelation(const FactionRelations* relations, Faction* otherFaction)
{
    if (!relations)
    {
        return CachedKoTarget::RELATION_NEUTRAL;
    }

    // Native isEnemy/isAlly can insert a missing diplomatic record.
    // Use their inclusive thresholds and the native default without changing the table.
    const auto entry = relations->_factionRelations.find(otherFaction);
    const bool hasEntry = entry != relations->_factionRelations.end();
    const float standing = hasEntry ? entry->second.relation : relations->defaultFactionRelation;
    if (standing <= -30.0f)
    {
        return CachedKoTarget::RELATION_ENEMY;
    }
    if (standing >= 50.0f || (hasEntry && entry->second.alliance))
    {
        return CachedKoTarget::RELATION_ALLY;
    }
    return CachedKoTarget::RELATION_NEUTRAL;
}

int ResolveMarkerRelationSafe(Character* candidate, bool isPlayerSquadMember)
{
    if (!candidate || !ou)
    {
        return CachedKoTarget::RELATION_NEUTRAL;
    }
    if (isPlayerSquadMember)
    {
        return CachedKoTarget::RELATION_SQUAD;
    }

    __try
    {
        Faction* const faction = candidate->owner;
        Faction* const playerFaction = ou->player ? ou->player->participant : 0;
        if (faction && playerFaction)
        {
            // Relation colours describe faction standing, regardless of temporary combat
            // targets, disguise, or whether the character still has an active AI.
            const int factionRelation = ReadFactionRelation(faction->relations, playerFaction);
            if (factionRelation == CachedKoTarget::RELATION_ENEMY)
            {
                return CachedKoTarget::RELATION_ENEMY;
            }
            const int playerRelation = ReadFactionRelation(playerFaction->relations, faction);
            if (playerRelation == CachedKoTarget::RELATION_ENEMY)
            {
                return CachedKoTarget::RELATION_ENEMY;
            }
            if (factionRelation == CachedKoTarget::RELATION_ALLY
                || playerRelation == CachedKoTarget::RELATION_ALLY)
            {
                return CachedKoTarget::RELATION_ALLY;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return CachedKoTarget::RELATION_NEUTRAL;
    }

    return CachedKoTarget::RELATION_NEUTRAL;
}

} // namespace vs_character_lookup
