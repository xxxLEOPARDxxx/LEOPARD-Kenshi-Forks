#pragma once

#include <kenshi/Character.h>

namespace vs_character_lookup
{

bool TryReadCharacterHandleSafe(Character* candidate, hand* handleOut);
Character* ResolveCharacterByHandleSafe(const hand& targetHandle);
bool IsAnimalCharacterSafe(Character* candidate);
bool IsCharacterInPlayerSquadSafe(Character* candidate);
bool IsEnemyToAnyPlayerCharacterSafe(Character* candidate);
bool IsSameFactionAsPlayerSafe(Character* candidate);

} // namespace vs_character_lookup
