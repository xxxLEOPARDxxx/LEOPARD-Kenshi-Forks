#pragma once

#include <kenshi/Character.h>

namespace vs_character_lookup
{

bool TryReadCharacterHandleSafe(Character* candidate, hand* handleOut);
Character* ResolveCharacterByHandleSafe(const hand& targetHandle);
bool IsAnimalCharacterSafe(Character* candidate);
bool IsCharacterInPlayerSquadSafe(Character* candidate);
int ResolveMarkerRelationSafe(Character* candidate, bool isPlayerSquadMember);

} // namespace vs_character_lookup
