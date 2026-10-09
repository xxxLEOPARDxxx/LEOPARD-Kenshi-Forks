#pragma once

#include "vs_runtime_state.h"

namespace vs_probe_cache
{

void Reset(RuntimeStateView& state);
void BeginWindow(RuntimeStateView& state);
bool ShouldDeferNotDownedCandidate(RuntimeStateView& state, const hand& targetHandle, DWORD nowMs);
bool RecordNotDownedCandidate(RuntimeStateView& state, const hand& targetHandle, DWORD nowMs);
void ClearNotDownedCandidate(RuntimeStateView& state, const hand& targetHandle);
bool UpsertCachedKoTarget(RuntimeStateView& state, const CachedKoTarget& cachedTarget);
unsigned int PruneUnseenCachedTargets(RuntimeStateView& state);

} // namespace vs_probe_cache
