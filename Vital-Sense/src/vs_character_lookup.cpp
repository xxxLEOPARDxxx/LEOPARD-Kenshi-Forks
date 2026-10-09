#include "vs_character_lookup.h"

#include <Windows.h>
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

bool IsEnemyToAnyPlayerCharacterSafe(Character* candidate)
{
    if (!candidate || !ou || !ou->player)
    {
        return false;
    }

    bool isEnemy = false;
    __try
    {
        const lektor<Character*>& playerCharacters = ou->player->playerCharacters;
        if (!playerCharacters.valid())
        {
            return false;
        }

        for (lektor<Character*>::const_iterator it = playerCharacters.begin(); it != playerCharacters.end(); ++it)
        {
            Character* playerCharacter = *it;
            if (!playerCharacter || playerCharacter == candidate)
            {
                continue;
            }

            bool hostileToPlayerCharacter = false;
            __try
            {
                hostileToPlayerCharacter = candidate->isEnemy(playerCharacter, true);
                if (!hostileToPlayerCharacter)
                {
                    hostileToPlayerCharacter = candidate->shouldIScrewThisGuyOver(playerCharacter);
                }
                if (!hostileToPlayerCharacter)
                {
                    hostileToPlayerCharacter = candidate->areYouGonnaGetMe(playerCharacter);
                }
                if (!hostileToPlayerCharacter)
                {
                    hostileToPlayerCharacter = playerCharacter->areYouGonnaGetMe(candidate);
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                hostileToPlayerCharacter = false;
            }

            if (hostileToPlayerCharacter)
            {
                isEnemy = true;
                break;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        isEnemy = false;
    }

    return isEnemy;
}

bool IsSameFactionAsPlayerSafe(Character* candidate)
{
    if (!candidate || !ou || !ou->player)
    {
        return false;
    }

    bool sameFaction = false;
    __try
    {
        Faction* playerFaction = ou->player->participant;
        sameFaction = (playerFaction && candidate->owner == playerFaction);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sameFaction = false;
    }

    return sameFaction;
}

} // namespace vs_character_lookup
