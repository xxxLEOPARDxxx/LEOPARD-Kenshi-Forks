#include "vs_probe_cache.h"

namespace
{

ProbeHandleKey MakeProbeHandleKey(const hand& source)
{
    ProbeHandleKey key;
    key.type = static_cast<unsigned int>(source.type);
    key.index = static_cast<unsigned int>(source.index);
    key.serial = static_cast<unsigned int>(source.serial);
    return key;
}

bool HandlesEqualByKey(const hand& a, const ProbeHandleKey& b)
{
    return static_cast<unsigned int>(a.type) == b.type
        && static_cast<unsigned int>(a.index) == b.index
        && static_cast<unsigned int>(a.serial) == b.serial;
}

DWORD ComputeNotDownedProbeCooldownMs(const RuntimeStateView& state)
{
    DWORD cooldownMs = state.config.updateIntervalMs * 3;
    if (cooldownMs < 200)
    {
        cooldownMs = 200;
    }
    else if (cooldownMs > 500)
    {
        cooldownMs = 500;
    }
    return cooldownMs;
}

} // namespace

namespace vs_probe_cache
{

void Reset(RuntimeStateView& state)
{
    state.koTargetCache.clear();
    state.probeRuntimeCaches.koTargetIndexByHandle.clear();
    state.probeRuntimeCaches.notDownedByHandle.clear();
    state.probeRuntimeCaches.generation = 0;
}

void BeginWindow(RuntimeStateView& state)
{
    ++state.probeRuntimeCaches.generation;
    if (state.probeRuntimeCaches.generation == 0)
    {
        state.probeRuntimeCaches.generation = 1;
    }
}

bool ShouldDeferNotDownedCandidate(RuntimeStateView& state, const hand& targetHandle, DWORD nowMs)
{
    if (targetHandle.isNull())
    {
        return false;
    }

    std::map<ProbeHandleKey, ProbeDeferredCandidateState>& deferredByHandle = state.probeRuntimeCaches.notDownedByHandle;
    const ProbeHandleKey targetKey = MakeProbeHandleKey(targetHandle);
    std::map<ProbeHandleKey, ProbeDeferredCandidateState>::iterator it = deferredByHandle.find(targetKey);
    if (it == deferredByHandle.end())
    {
        return false;
    }

    if (nowMs < it->second.nextEligibleProbeMs)
    {
        return true;
    }

    deferredByHandle.erase(it);
    return false;
}

bool RecordNotDownedCandidate(RuntimeStateView& state, const hand& targetHandle, DWORD nowMs)
{
    if (targetHandle.isNull())
    {
        return false;
    }

    ProbeDeferredCandidateState deferredState;
    deferredState.nextEligibleProbeMs = nowMs + ComputeNotDownedProbeCooldownMs(state);
    state.probeRuntimeCaches.notDownedByHandle[MakeProbeHandleKey(targetHandle)] = deferredState;
    return true;
}

void ClearNotDownedCandidate(RuntimeStateView& state, const hand& targetHandle)
{
    if (targetHandle.isNull())
    {
        return;
    }

    state.probeRuntimeCaches.notDownedByHandle.erase(MakeProbeHandleKey(targetHandle));
}

bool UpsertCachedKoTarget(RuntimeStateView& state, const CachedKoTarget& cachedTarget)
{
    std::map<ProbeHandleKey, size_t>& targetIndexByHandle = state.probeRuntimeCaches.koTargetIndexByHandle;
    const ProbeHandleKey targetKey = MakeProbeHandleKey(cachedTarget.targetHandle);
    std::map<ProbeHandleKey, size_t>::iterator existing = targetIndexByHandle.find(targetKey);
    if (existing != targetIndexByHandle.end())
    {
        const size_t index = existing->second;
        if (index < state.koTargetCache.size() && HandlesEqualByKey(state.koTargetCache[index].targetHandle, targetKey))
        {
            state.koTargetCache[index] = cachedTarget;
            return true;
        }

        targetIndexByHandle.erase(existing);
    }

    state.koTargetCache.push_back(cachedTarget);
    targetIndexByHandle[targetKey] = state.koTargetCache.size() - 1;
    return false;
}

unsigned int PruneUnseenCachedTargets(RuntimeStateView& state)
{
    unsigned int prunedCount = 0;
    std::map<ProbeHandleKey, size_t>& targetIndexByHandle = state.probeRuntimeCaches.koTargetIndexByHandle;
    std::vector<CachedKoTarget>& koTargetCache = state.koTargetCache;

    size_t index = 0;
    while (index < koTargetCache.size())
    {
        if (koTargetCache[index].lastSeenGeneration == state.probeRuntimeCaches.generation)
        {
            ++index;
            continue;
        }

        targetIndexByHandle.erase(MakeProbeHandleKey(koTargetCache[index].targetHandle));
        const size_t lastIndex = koTargetCache.size() - 1;
        if (index != lastIndex)
        {
            koTargetCache[index] = koTargetCache[lastIndex];
            targetIndexByHandle[MakeProbeHandleKey(koTargetCache[index].targetHandle)] = index;
        }
        koTargetCache.pop_back();
        ++prunedCount;
    }

    return prunedCount;
}

} // namespace vs_probe_cache
