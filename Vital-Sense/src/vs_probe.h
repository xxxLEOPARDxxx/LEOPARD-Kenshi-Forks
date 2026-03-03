#pragma once

#include "vs_runtime_state.h"

namespace vs_probe
{

bool IsCharacterValidSafe(Character* candidate);
bool TryReadCharacterSnapshotSafe(Character* candidate, bool& isOnScreen, Ogre::Vector3& candidatePos, hand& targetHandle);
int ResolveMarkerRelationSafe(RuntimeStateView& state, Character* candidate);
Character* ResolveDeathParadeCandidateSafe(hand targetHandle, Character* fallbackCandidate);
void ProcessMarkerCandidate(RuntimeStateView& state, Character* candidate, const Ogre::Vector3& cameraCenter, DWORD nowMs);
void TickKoProbe(RuntimeStateView& state, const char* pluginName);

} // namespace vs_probe
