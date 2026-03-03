#include "vs_probe.h"

#include "vs_marker_render.h"

#include <core/Functions.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/PlayerInterface.h>

#include <Windows.h>

#include <string>
#include <vector>

class UtilityT
{
public:
    UtilityT();
    bool worldToScreenPX(const Ogre::Vector3& pos, float& x, float& y);
};

namespace
{
const float kKoMarkerHeadAnchorYOffset = 2.0f;
const float kProbablyDyingBloodMax = 50.0f;
}

namespace vs_probe
{

int FindCachedKoTargetIndex(RuntimeStateView& state, const hand& targetHandle)
{
    for (size_t i = 0; i < state.koTargetCache.size(); ++i)
    {
        const hand& cached = state.koTargetCache[i].targetHandle;
        if (cached.type == targetHandle.type
            && cached.index == targetHandle.index
            && cached.serial == targetHandle.serial)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool HandlesEqualByKey(const hand& a, const hand& b)
{
    return a.type == b.type
        && a.index == b.index
        && a.serial == b.serial;
}

bool HandleVectorContains(const std::vector<hand>& handles, const hand& targetHandle)
{
    for (size_t i = 0; i < handles.size(); ++i)
    {
        if (HandlesEqualByKey(handles[i], targetHandle))
        {
            return true;
        }
    }
    return false;
}

bool VisibleHandleListContains(RuntimeStateView& state, const hand& targetHandle)
{
    return HandleVectorContains(state.visibleKoHandlesScratch, targetHandle);
}

bool StringListContains(const std::vector<std::string>& values, const std::string& needle)
{
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (values[i] == needle)
        {
            return true;
        }
    }
    return false;
}

bool IsMarkerStateEnabled(RuntimeStateView& state, int markerState)
{
    if (markerState == CachedKoTarget::STATE_BOUNTY_ONLY)
    {
        return state.config.showBountySymbolOnAllCharacters;
    }
    if (markerState == CachedKoTarget::STATE_DEAD)
    {
        return state.config.enableDeadState;
    }
    if (markerState == CachedKoTarget::STATE_RECOVERY_COMA)
    {
        return state.config.enableRecoveryComaState;
    }
    if (markerState == CachedKoTarget::STATE_DYING)
    {
        return state.config.enableDyingState;
    }
    if (markerState == CachedKoTarget::STATE_PLAYING_DEAD)
    {
        return state.config.enablePlayingDeadState;
    }
    return state.config.enableUnconsciousState;
}

bool TryResolveMarkerState(RuntimeStateView& state, Character* candidate, int* markerStateOut)
{
    if (!candidate || !markerStateOut)
    {
        return false;
    }

    bool isDead = false;
    bool isUnconscious = false;
    bool isRecoveryComa = false;
    bool isDying = false;
    bool isPlayingDead = false;
    bool isProbablyDying = false;
    bool dyingByProbablyLowBlood = false;
    bool dyingByProbablySub50Ko = false;
    bool dyingByActiveBleed = false;
    bool dyingByTrauma = false;
    bool dyingByBloodThreshold = false;
    bool recoveryComaByCannotWake = false;
    bool medicalSub50KoFlag = false;
    float currentBleedRate = 0.0f;
    float bloodLevel = 0.0f;
    float pointOfNoReturn = 0.0f;

    __try
    {
        const bool isDeadByCharacter = candidate->isDead();
        const bool isDeadByMedicalMethod = candidate->medical.isDead();
        const bool isDeadByMedicalFlag = candidate->medical.dead;
        isDead = (isDeadByCharacter || isDeadByMedicalMethod || isDeadByMedicalFlag);
        isUnconscious = candidate->isUnconcious();
        if (isUnconscious)
        {
            isPlayingDead = (candidate->_currentProneState == PS_PLAYING_DEAD);
            isProbablyDying = candidate->medical.isProbablyDying();
            dyingByTrauma = candidate->medical.isInBloodlossTrauma();
            medicalSub50KoFlag = candidate->medical.sub50KO;
            currentBleedRate = candidate->medical.currentBleedRate;
            bloodLevel = candidate->medical.blood;
            pointOfNoReturn = candidate->medical.pointOfNoReturn();
            dyingByBloodThreshold = (bloodLevel <= pointOfNoReturn);
            dyingByProbablyLowBlood = (isProbablyDying && bloodLevel <= kProbablyDyingBloodMax);
            dyingByProbablySub50Ko = medicalSub50KoFlag;
            dyingByActiveBleed = (isProbablyDying && (currentBleedRate > 0.0f || candidate->medical.extraBloodLossFromBodyparts > 0.0f));
            recoveryComaByCannotWake = (!candidate->medical.canGetUpWakeUp() && medicalSub50KoFlag);

            const bool knockoutTimerElapsed = (candidate->medical.knockoutTimer <= 0.0f);
            isRecoveryComa = recoveryComaByCannotWake
                && knockoutTimerElapsed
                && !isProbablyDying
                && !dyingByBloodThreshold
                && !dyingByTrauma
                && !dyingByActiveBleed
                && !dyingByProbablyLowBlood;
            isDying = dyingByBloodThreshold || dyingByProbablySub50Ko;
            if (isRecoveryComa)
            {
                isDying = false;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    if (isDead)
    {
        *markerStateOut = CachedKoTarget::STATE_DEAD;
        return IsMarkerStateEnabled(state, CachedKoTarget::STATE_DEAD);
    }

    if (isUnconscious)
    {
        if (isPlayingDead)
        {
            *markerStateOut = CachedKoTarget::STATE_PLAYING_DEAD;
            return IsMarkerStateEnabled(state, CachedKoTarget::STATE_PLAYING_DEAD);
        }
        if (isDying)
        {
            *markerStateOut = CachedKoTarget::STATE_DYING;
            return IsMarkerStateEnabled(state, CachedKoTarget::STATE_DYING);
        }
        if (isRecoveryComa)
        {
            *markerStateOut = CachedKoTarget::STATE_RECOVERY_COMA;
            return IsMarkerStateEnabled(state, CachedKoTarget::STATE_RECOVERY_COMA);
        }
        *markerStateOut = CachedKoTarget::STATE_UNCONSCIOUS;
        return IsMarkerStateEnabled(state, CachedKoTarget::STATE_UNCONSCIOUS);
    }

    return false;
}

bool IsHighlightGateOpen(RuntimeStateView& state)
{
    if (!state.config.onlyWhenAltHeld)
    {
        return true;
    }

    return (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
}

bool IsGamePausedSafe()
{
    if (!ou)
    {
        return false;
    }

    bool paused = false;
    __try
    {
        paused = ou->isPaused();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return paused;
}

bool IsPlayerSquadMember(Character* candidate)
{
    if (!candidate || !ou || !ou->player)
    {
        return false;
    }

    const lektor<Character*>& playerCharacters = ou->player->playerCharacters;
    if (!playerCharacters.valid())
    {
        return false;
    }

    for (lektor<Character*>::const_iterator it = playerCharacters.begin(); it != playerCharacters.end(); ++it)
    {
        if (*it == candidate)
        {
            return true;
        }
    }

    return false;
}

bool IsSameFactionAsPlayer(Character* candidate)
{
    if (!candidate || !ou || !ou->player)
    {
        return false;
    }

    Faction* playerFaction = ou->player->participant;
    if (!playerFaction)
    {
        return false;
    }

    return candidate->owner == playerFaction;
}

int ResolveMarkerRelation(Character* candidate)
{
    if (IsPlayerSquadMember(candidate))
    {
        return CachedKoTarget::RELATION_SQUAD;
    }

    if (IsSameFactionAsPlayer(candidate))
    {
        return CachedKoTarget::RELATION_ALLY;
    }

    return CachedKoTarget::RELATION_ENEMY;
}

int ResolveTotalBounty(Character* candidate)
{
    if (!candidate)
    {
        return 0;
    }

    int totalBounty = 0;
    __try
    {
        totalBounty = candidate->crimes.getTotalBounty();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }

    if (totalBounty < 0)
    {
        return 0;
    }

    return totalBounty;
}

bool IsWithinHighlightRange(RuntimeStateView& state, const Ogre::Vector3& sourcePos, const Ogre::Vector3& targetPos)
{
    if (state.config.maxHighlightDistanceMeters == 0)
    {
        return true;
    }

    const float maxDistance = static_cast<float>(state.config.maxHighlightDistanceMeters);
    const float maxDistanceSq = maxDistance * maxDistance;
    const float dx = targetPos.x - sourcePos.x;
    const float dz = targetPos.z - sourcePos.z;
    return (dx * dx + dz * dz) <= maxDistanceSq;
}

bool IsCharacterValidSafe(Character* candidate)
{
    bool candidateValid = false;
    __try
    {
        candidateValid = candidate->isValid();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    return candidateValid;
}

bool TryReadCharacterSnapshotSafe(Character* candidate, bool& isOnScreen, Ogre::Vector3& candidatePos, hand& targetHandle)
{
    __try
    {
        isOnScreen = candidate->isOnScreen;
        candidatePos = candidate->getPosition();
        targetHandle = candidate->getHandle();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    return true;
}

int ResolveMarkerRelationSafe(RuntimeStateView& state, Character* candidate)
{
    int markerRelation = CachedKoTarget::RELATION_ENEMY;
    __try
    {
        markerRelation = ResolveMarkerRelation(candidate);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        markerRelation = CachedKoTarget::RELATION_ENEMY;
    }

    (void)state;
    return markerRelation;
}

Character* ResolveDeathParadeCandidateSafe(hand targetHandle, Character* fallbackCandidate)
{
    Character* deathParadeCandidate = 0;
    __try
    {
        deathParadeCandidate = ou->getFromDeathParade(targetHandle);
        if (!deathParadeCandidate)
        {
            deathParadeCandidate = fallbackCandidate;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        deathParadeCandidate = 0;
    }
    return deathParadeCandidate;
}

void ProcessMarkerCandidate(RuntimeStateView& state, Character* candidate, const Ogre::Vector3& cameraCenter, DWORD nowMs)
{
    if (!candidate)
    {
        return;
    }

    if (!IsCharacterValidSafe(candidate))
    {
        return;
    }

    const int totalBounty = ResolveTotalBounty(candidate);
    int markerState = CachedKoTarget::STATE_UNCONSCIOUS;
    const bool isDownedState = TryResolveMarkerState(state, candidate, &markerState);
    if (!isDownedState)
    {
        if (!state.config.showBountySymbol || !state.config.showBountySymbolOnAllCharacters || totalBounty <= 0)
        {
            return;
        }
        markerState = CachedKoTarget::STATE_BOUNTY_ONLY;
    }

    bool isOnScreen = false;
    Ogre::Vector3 candidatePos;
    hand targetHandle;
    if (!TryReadCharacterSnapshotSafe(candidate, isOnScreen, candidatePos, targetHandle))
    {
        return;
    }

    const bool isDeadState = (markerState == CachedKoTarget::STATE_DEAD);
    if (!isDeadState && !isOnScreen)
    {
        return;
    }

    if (!IsWithinHighlightRange(state, cameraCenter, candidatePos))
    {
        return;
    }

    if (isDeadState && !isOnScreen)
    {
        if (!vs_marker_render::EnsureProjectionUtility(state))
        {
            return;
        }

        float probeX = 0.0f;
        float probeY = 0.0f;
        const Ogre::Vector3 anchorPos = candidatePos + Ogre::Vector3(0, kKoMarkerHeadAnchorYOffset, 0);
        if (!state.projectionUtility->worldToScreenPX(anchorPos, probeX, probeY))
        {
            return;
        }
    }

    if (targetHandle.isNull())
    {
        return;
    }

    const int markerRelation = ResolveMarkerRelationSafe(state, candidate);

    if (!VisibleHandleListContains(state, targetHandle))
    {
        state.visibleKoHandlesScratch.push_back(targetHandle);
    }

    const int existingIndex = FindCachedKoTargetIndex(state, targetHandle);
    if (existingIndex >= 0)
    {
        CachedKoTarget& existing = state.koTargetCache[existingIndex];
        existing.worldPos = candidatePos;
        existing.lastSeenMs = nowMs;
        existing.markerState = markerState;
        existing.markerRelation = markerRelation;
        existing.totalBounty = totalBounty;
    }
    else
    {
        CachedKoTarget created = {
            targetHandle,
            candidatePos,
            nowMs,
            markerState,
            markerRelation,
            totalBounty
        };
        state.koTargetCache.push_back(created);
    }
}

void TickKoProbe(RuntimeStateView& state, const char* pluginName)
{
    const bool anyMarkerVisualEnabled = (state.config.showMarkerIcons || state.config.showMarkerText || state.config.showBountySymbol);
    const bool canRun = state.config.enabled && anyMarkerVisualEnabled && IsHighlightGateOpen(state) && ou;
    if (!canRun)
    {
        if (state.highlightRuntimeActive)
        {
            state.koTargetCache.clear();
            vs_marker_render::HideAllKoMarkerWidgets(state, pluginName);
            state.highlightRuntimeActive = false;
        }
        return;
    }
    state.highlightRuntimeActive = true;

    const DWORD nowMs = GetTickCount();

    if (state.lastProbeTickMs != 0 && (nowMs - state.lastProbeTickMs) < state.config.updateIntervalMs)
    {
        vs_marker_render::TickKoMarkerRender(state, pluginName);
        return;
    }
    state.lastProbeTickMs = nowMs;

    const Ogre::Vector3 cameraCenter = ou->getCameraCenter();

    const ogre_unordered_set<Character*>::type& activeCharacters = ou->getCharacterUpdateList();
    const ogre_unordered_map<hand, Character*>::type& deathParadeCharacters = ou->deathParade;
    state.visibleKoHandlesScratch.clear();

    for (auto iter = activeCharacters.begin(); iter != activeCharacters.end(); ++iter)
    {
        ProcessMarkerCandidate(state, *iter, cameraCenter, nowMs);
    }

    for (auto iter = deathParadeCharacters.begin(); iter != deathParadeCharacters.end(); ++iter)
    {
        Character* deathParadeCandidate = ResolveDeathParadeCandidateSafe(iter->first, iter->second);
        ProcessMarkerCandidate(state, deathParadeCandidate, cameraCenter, nowMs);
    }

    for (int i = static_cast<int>(state.koTargetCache.size()) - 1; i >= 0; --i)
    {
        if (!VisibleHandleListContains(state, state.koTargetCache[static_cast<size_t>(i)].targetHandle))
        {
            state.koTargetCache.erase(state.koTargetCache.begin() + i);
        }
    }
    vs_marker_render::TickKoMarkerRender(state, pluginName);
}

} // namespace vs_probe
