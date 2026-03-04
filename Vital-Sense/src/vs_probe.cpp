#include "vs_probe.h"

#include "vs_log.h"

#include <core/Functions.h>
#include <kenshi/GameData.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/InputHandler.h>
#include <kenshi/Kenshi.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/RaceData.h>

#include <Windows.h>

#include <cctype>
#include <cstring>
#include <sstream>
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
const DWORD kProbeDiagLogIntervalMs = 2000;
const unsigned int kProbeAnimalSampleMaxPerWindow = 10;

struct CandidateSpeciesInfo
{
    bool classified;
    bool isAnimal;
    bool isLikelySpider;
    char raceId[64];
};

struct ProbeDiagCounters
{
    unsigned int ticks;
    unsigned int candidatesSeen;
    unsigned int animalsSeen;
    unsigned int spidersSeen;
    unsigned int downedAnimals;
    unsigned int downedSpiders;
    unsigned int accepted;
    unsigned int acceptedAnimals;
    unsigned int acceptedSpiders;
    unsigned int rejectInvalid;
    unsigned int rejectNotDowned;
    unsigned int rejectNotDownedAnimals;
    unsigned int rejectNotDownedSpiders;
    unsigned int rejectSnapshot;
    unsigned int rejectSnapshotAnimals;
    unsigned int rejectOffscreen;
    unsigned int rejectOffscreenAnimals;
    unsigned int rejectRange;
    unsigned int rejectRangeAnimals;
    unsigned int rejectProjection;
    unsigned int rejectProjectionAnimals;
    unsigned int rejectNullHandle;
    unsigned int rejectNullHandleAnimals;
};

DWORD gProbeDiagLastLogMs = 0;
unsigned int gProbeAnimalSamplesLogged = 0;
ProbeDiagCounters gProbeDiag;

bool ContainsCaseInsensitiveToken(const char* haystack, const char* needle)
{
    if (!haystack || !needle || needle[0] == '\0')
    {
        return false;
    }

    const size_t needleLen = std::strlen(needle);
    if (needleLen == 0)
    {
        return false;
    }

    for (const char* scan = haystack; *scan != '\0'; ++scan)
    {
        size_t matched = 0;
        while (matched < needleLen && scan[matched] != '\0')
        {
            const unsigned char a = static_cast<unsigned char>(scan[matched]);
            const unsigned char b = static_cast<unsigned char>(needle[matched]);
            if (std::tolower(a) != std::tolower(b))
            {
                break;
            }
            ++matched;
        }

        if (matched == needleLen)
        {
            return true;
        }
    }

    return false;
}

bool TryReadAnimalRaceIdSafe(Character* candidate, char* raceIdOut, size_t raceIdOutLen)
{
    if (!candidate || !raceIdOut || raceIdOutLen == 0)
    {
        return false;
    }

    raceIdOut[0] = '\0';

    __try
    {
        RaceData* race = candidate->getRace();
        if (!race || !race->data)
        {
            return false;
        }

        const std::string* raceString = 0;
        if (!race->data->stringID.empty())
        {
            raceString = &race->data->stringID;
        }
        else if (!race->data->name.empty())
        {
            raceString = &race->data->name;
        }

        if (!raceString)
        {
            return false;
        }

        const char* source = raceString->c_str();
        const size_t sourceLen = raceString->size();
        const size_t copyLen = (sourceLen < (raceIdOutLen - 1)) ? sourceLen : (raceIdOutLen - 1);
        std::memcpy(raceIdOut, source, copyLen);
        raceIdOut[copyLen] = '\0';
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        raceIdOut[0] = '\0';
        return false;
    }

    return raceIdOut[0] != '\0';
}

bool TryClassifySpeciesSafe(Character* candidate, CandidateSpeciesInfo* speciesInfoOut)
{
    if (!candidate || !speciesInfoOut)
    {
        return false;
    }

    bool isAnimal = false;
    char raceId[64];
    raceId[0] = '\0';

    __try
    {
        isAnimal = (candidate->isAnimal() != 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    if (isAnimal)
    {
        TryReadAnimalRaceIdSafe(candidate, raceId, sizeof(raceId));
    }

    speciesInfoOut->classified = true;
    speciesInfoOut->isAnimal = isAnimal;
    speciesInfoOut->isLikelySpider = ContainsCaseInsensitiveToken(raceId, "spider");
    std::memset(speciesInfoOut->raceId, 0, sizeof(speciesInfoOut->raceId));
    if (raceId[0] != '\0')
    {
        const size_t copyLen = std::strlen(raceId);
        std::memcpy(speciesInfoOut->raceId, raceId, copyLen);
        speciesInfoOut->raceId[copyLen] = '\0';
    }
    return true;
}

struct AnimalStateSnapshotData
{
    bool isUnconscious;
    bool isDeadByCharacter;
    bool isDeadByMedical;
    bool deadFlag;
    bool sub50Ko;
    bool probablyDying;
    bool canWake;
    ProneState proneState;
    float knockoutTimer;
    float blood;
    float pointOfNoReturn;
    float bleedRate;
};

bool TryReadAnimalStateSnapshotSafe(Character* candidate, AnimalStateSnapshotData* snapshotOut)
{
    if (!candidate || !snapshotOut)
    {
        return false;
    }

    AnimalStateSnapshotData snapshot;
    std::memset(&snapshot, 0, sizeof(snapshot));

    __try
    {
        snapshot.proneState = candidate->_currentProneState;
        snapshot.isUnconscious = candidate->isUnconcious();
        snapshot.isDeadByCharacter = candidate->isDead();
        snapshot.isDeadByMedical = candidate->medical.isDead();
        snapshot.deadFlag = candidate->medical.dead;
        snapshot.sub50Ko = candidate->medical.sub50KO;
        snapshot.probablyDying = candidate->medical.isProbablyDying();
        snapshot.canWake = candidate->medical.canGetUpWakeUp();
        snapshot.knockoutTimer = candidate->medical.knockoutTimer;
        snapshot.blood = candidate->medical.blood;
        snapshot.pointOfNoReturn = candidate->medical.pointOfNoReturn();
        snapshot.bleedRate = candidate->medical.currentBleedRate;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    *snapshotOut = snapshot;
    return true;
}

std::string BuildAnimalStateSnapshotSafe(Character* candidate)
{
    AnimalStateSnapshotData snapshot;
    std::memset(&snapshot, 0, sizeof(snapshot));
    if (!TryReadAnimalStateSnapshotSafe(candidate, &snapshot))
    {
        return "state=read_exception";
    }

    std::stringstream ss;
    ss << "prone=" << static_cast<int>(snapshot.proneState)
       << " is_unconscious=" << (snapshot.isUnconscious ? "true" : "false")
       << " is_dead_char=" << (snapshot.isDeadByCharacter ? "true" : "false")
       << " is_dead_med=" << (snapshot.isDeadByMedical ? "true" : "false")
       << " dead_flag=" << (snapshot.deadFlag ? "true" : "false")
       << " sub50ko=" << (snapshot.sub50Ko ? "true" : "false")
       << " can_wake=" << (snapshot.canWake ? "true" : "false")
       << " probably_dying=" << (snapshot.probablyDying ? "true" : "false")
       << " ko_timer=" << snapshot.knockoutTimer
       << " blood=" << snapshot.blood
       << " ponr=" << snapshot.pointOfNoReturn
       << " bleed_rate=" << snapshot.bleedRate;
    return ss.str();
}

void MaybeLogAnimalProbeSample(
    const char* pluginName,
    const CandidateSpeciesInfo& speciesInfo,
    const hand* handleOrNull,
    const char* reason,
    const std::string& details)
{
    if (!pluginName || !speciesInfo.isAnimal)
    {
        return;
    }
    if (gProbeAnimalSamplesLogged >= kProbeAnimalSampleMaxPerWindow)
    {
        return;
    }
    ++gProbeAnimalSamplesLogged;

    std::stringstream ss;
    ss << "probe animal sample reason=" << (reason ? reason : "unknown")
       << " animal=true"
       << " spider=" << (speciesInfo.isLikelySpider ? "true" : "false")
       << " race_id=" << (speciesInfo.raceId[0] != '\0' ? speciesInfo.raceId : "unknown");
    if (handleOrNull)
    {
        ss << " handle_type=" << handleOrNull->type
           << " handle_index=" << handleOrNull->index
           << " handle_serial=" << handleOrNull->serial;
    }
    if (!details.empty())
    {
        ss << " " << details;
    }
    vs_log::LogInfo(pluginName, ss.str());
}

void EmitProbeDiagLogIfDue(const char* pluginName)
{
    if (!pluginName)
    {
        return;
    }

    const DWORD nowMs = GetTickCount();
    if (gProbeDiagLastLogMs != 0 && (nowMs - gProbeDiagLastLogMs) < kProbeDiagLogIntervalMs)
    {
        return;
    }
    gProbeDiagLastLogMs = nowMs;

    if (gProbeDiag.ticks == 0 && gProbeDiag.candidatesSeen == 0)
    {
        gProbeAnimalSamplesLogged = 0;
        return;
    }

    std::stringstream ss;
    ss << "probe diag ticks=" << gProbeDiag.ticks
       << " candidates=" << gProbeDiag.candidatesSeen
       << " animals=" << gProbeDiag.animalsSeen
       << " spiders=" << gProbeDiag.spidersSeen
       << " downed_animals=" << gProbeDiag.downedAnimals
       << " downed_spiders=" << gProbeDiag.downedSpiders
       << " accepted=" << gProbeDiag.accepted
       << " accepted_animals=" << gProbeDiag.acceptedAnimals
       << " accepted_spiders=" << gProbeDiag.acceptedSpiders
       << " reject_invalid=" << gProbeDiag.rejectInvalid
       << " reject_not_downed=" << gProbeDiag.rejectNotDowned
       << " reject_not_downed_animals=" << gProbeDiag.rejectNotDownedAnimals
       << " reject_not_downed_spiders=" << gProbeDiag.rejectNotDownedSpiders
       << " reject_snapshot=" << gProbeDiag.rejectSnapshot
       << " reject_snapshot_animals=" << gProbeDiag.rejectSnapshotAnimals
       << " reject_offscreen=" << gProbeDiag.rejectOffscreen
       << " reject_offscreen_animals=" << gProbeDiag.rejectOffscreenAnimals
       << " reject_range=" << gProbeDiag.rejectRange
       << " reject_range_animals=" << gProbeDiag.rejectRangeAnimals
       << " reject_projection=" << gProbeDiag.rejectProjection
       << " reject_projection_animals=" << gProbeDiag.rejectProjectionAnimals
       << " reject_null_handle=" << gProbeDiag.rejectNullHandle
       << " reject_null_handle_animals=" << gProbeDiag.rejectNullHandleAnimals;
    vs_log::LogInfo(pluginName, ss.str());

    std::memset(&gProbeDiag, 0, sizeof(gProbeDiag));
    gProbeAnimalSamplesLogged = 0;
}

bool EnsureProjectionUtility(RuntimeStateView& state)
{
    if (state.projectionUtility)
    {
        return true;
    }

    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    uintptr_t utilityOffset = 0;
    if (platform == KenshiLib::BinaryVersion::STEAM)
    {
        if (version == "1.0.65")
        {
            utilityOffset = 0x02134b10;
        }
        else if (version == "1.0.68")
        {
            utilityOffset = 0x02135b70;
        }
    }
    else if (platform == KenshiLib::BinaryVersion::GOG)
    {
        if (version == "1.0.65")
        {
            utilityOffset = 0x02132a80;
        }
        else if (version == "1.0.68")
        {
            utilityOffset = 0x02134aa0;
        }
    }

    if (utilityOffset == 0)
    {
        return false;
    }

    HMODULE exeHandle = GetModuleHandleA(0);
    if (!exeHandle)
    {
        return false;
    }

    const uintptr_t baseAddress = reinterpret_cast<uintptr_t>(exeHandle);
    if (!baseAddress)
    {
        return false;
    }

    state.projectionUtility = reinterpret_cast<UtilityT*>(baseAddress + utilityOffset);
    return state.projectionUtility != 0;
}
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
    ProneState proneState = PS_NORMAL;
    bool isKoProne = false;
    float currentBleedRate = 0.0f;
    float bloodLevel = 0.0f;
    float pointOfNoReturn = 0.0f;

    __try
    {
        const bool isDeadByCharacter = candidate->isDead();
        const bool isDeadByMedicalMethod = candidate->medical.isDead();
        const bool isDeadByMedicalFlag = candidate->medical.dead;
        isDead = (isDeadByCharacter || isDeadByMedicalMethod || isDeadByMedicalFlag);
        proneState = candidate->_currentProneState;
        isKoProne = (proneState == PS_KO);
        isUnconscious = candidate->isUnconcious() || isKoProne;
        if (isUnconscious)
        {
            isPlayingDead = (proneState == PS_PLAYING_DEAD);
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

    bool gameHighlightKeyHeld = false;
    if (key)
    {
        __try
        {
            gameHighlightKeyHeld = key->highlight;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            gameHighlightKeyHeld = false;
        }
    }

    if (gameHighlightKeyHeld)
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

bool IsEnemyToAnyPlayerCharacter(Character* candidate, const lektor<Character*>& playerCharacters)
{
    if (!candidate)
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

        bool isEnemy = false;
        __try
        {
            isEnemy = candidate->isEnemy(playerCharacter, true);
            if (!isEnemy)
            {
                isEnemy = playerCharacter->areYouGonnaGetMe(candidate);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            isEnemy = false;
        }

        if (isEnemy)
        {
            return true;
        }
    }

    return false;
}

bool IsAllyToAllPlayerCharacters(Character* candidate, const lektor<Character*>& playerCharacters)
{
    if (!candidate)
    {
        return false;
    }

    bool hasComparablePlayerCharacter = false;
    for (lektor<Character*>::const_iterator it = playerCharacters.begin(); it != playerCharacters.end(); ++it)
    {
        Character* playerCharacter = *it;
        if (!playerCharacter || playerCharacter == candidate)
        {
            continue;
        }
        hasComparablePlayerCharacter = true;

        bool isAlly = false;
        __try
        {
            isAlly = candidate->isAlly(playerCharacter, true);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }

        if (!isAlly)
        {
            return false;
        }
    }

    return hasComparablePlayerCharacter;
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

bool IsAnimalCandidateSafe(Character* candidate)
{
    if (!candidate)
    {
        return false;
    }

    bool isAnimal = false;
    __try
    {
        isAnimal = (candidate->isAnimal() != 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        isAnimal = false;
    }
    return isAnimal;
}

int ResolveMarkerRelation(Character* candidate)
{
    if (!candidate || !ou || !ou->player)
    {
        return CachedKoTarget::RELATION_ENEMY;
    }

    if (IsPlayerSquadMember(candidate))
    {
        return CachedKoTarget::RELATION_SQUAD;
    }

    const bool candidateIsAnimal = IsAnimalCandidateSafe(candidate);
    const lektor<Character*>& playerCharacters = ou->player->playerCharacters;
    if (playerCharacters.valid())
    {
        if (IsEnemyToAnyPlayerCharacter(candidate, playerCharacters))
        {
            return CachedKoTarget::RELATION_ENEMY;
        }

        if (candidateIsAnimal)
        {
            // Keep non-squad fauna from being misclassified as ally; hostile animals must render as enemies.
            return IsSameFactionAsPlayer(candidate)
                ? CachedKoTarget::RELATION_ALLY
                : CachedKoTarget::RELATION_ENEMY;
        }

        if (IsAllyToAllPlayerCharacters(candidate, playerCharacters))
        {
            return CachedKoTarget::RELATION_ALLY;
        }
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

void ProcessMarkerCandidate(
    RuntimeStateView& state,
    Character* candidate,
    const Ogre::Vector3& cameraCenter,
    DWORD nowMs,
    const char* pluginName)
{
    ++gProbeDiag.candidatesSeen;

    if (!candidate)
    {
        return;
    }

    if (!IsCharacterValidSafe(candidate))
    {
        ++gProbeDiag.rejectInvalid;
        return;
    }

    CandidateSpeciesInfo speciesInfo;
    speciesInfo.classified = false;
    speciesInfo.isAnimal = false;
    speciesInfo.isLikelySpider = false;
    std::memset(speciesInfo.raceId, 0, sizeof(speciesInfo.raceId));
    const bool speciesClassified = TryClassifySpeciesSafe(candidate, &speciesInfo);
    if (speciesClassified && speciesInfo.isAnimal)
    {
        ++gProbeDiag.animalsSeen;
        if (speciesInfo.isLikelySpider)
        {
            ++gProbeDiag.spidersSeen;
        }
    }

    const int totalBounty = ResolveTotalBounty(candidate);
    int markerState = CachedKoTarget::STATE_UNCONSCIOUS;
    const bool isDownedState = TryResolveMarkerState(state, candidate, &markerState);
    if (isDownedState && speciesClassified && speciesInfo.isAnimal)
    {
        ++gProbeDiag.downedAnimals;
        if (speciesInfo.isLikelySpider)
        {
            ++gProbeDiag.downedSpiders;
        }
    }
    if (!isDownedState)
    {
        if (!state.config.showBountySymbol || !state.config.showBountySymbolOnAllCharacters || totalBounty <= 0)
        {
            ++gProbeDiag.rejectNotDowned;
            if (speciesClassified && speciesInfo.isAnimal)
            {
                ++gProbeDiag.rejectNotDownedAnimals;
                if (speciesInfo.isLikelySpider)
                {
                    ++gProbeDiag.rejectNotDownedSpiders;
                }
                const std::string animalState = BuildAnimalStateSnapshotSafe(candidate);
                MaybeLogAnimalProbeSample(pluginName, speciesInfo, 0, "not_downed", animalState);
            }
            return;
        }
        markerState = CachedKoTarget::STATE_BOUNTY_ONLY;
    }

    bool isOnScreen = false;
    Ogre::Vector3 candidatePos;
    hand targetHandle;
    if (!TryReadCharacterSnapshotSafe(candidate, isOnScreen, candidatePos, targetHandle))
    {
        ++gProbeDiag.rejectSnapshot;
        if (speciesClassified && speciesInfo.isAnimal)
        {
            ++gProbeDiag.rejectSnapshotAnimals;
            MaybeLogAnimalProbeSample(pluginName, speciesInfo, 0, "snapshot_failed", "");
        }
        return;
    }

    const bool isDeadState = (markerState == CachedKoTarget::STATE_DEAD);
    if (!isDeadState && !isOnScreen)
    {
        ++gProbeDiag.rejectOffscreen;
        if (speciesClassified && speciesInfo.isAnimal)
        {
            ++gProbeDiag.rejectOffscreenAnimals;
            MaybeLogAnimalProbeSample(pluginName, speciesInfo, &targetHandle, "offscreen", "");
        }
        return;
    }

    if (!IsWithinHighlightRange(state, cameraCenter, candidatePos))
    {
        ++gProbeDiag.rejectRange;
        if (speciesClassified && speciesInfo.isAnimal)
        {
            ++gProbeDiag.rejectRangeAnimals;
            MaybeLogAnimalProbeSample(pluginName, speciesInfo, &targetHandle, "out_of_range", "");
        }
        return;
    }

    if (isDeadState && !isOnScreen)
    {
        if (!EnsureProjectionUtility(state))
        {
            ++gProbeDiag.rejectProjection;
            if (speciesClassified && speciesInfo.isAnimal)
            {
                ++gProbeDiag.rejectProjectionAnimals;
                MaybeLogAnimalProbeSample(pluginName, speciesInfo, &targetHandle, "projection_utility_missing", "");
            }
            return;
        }

        float probeX = 0.0f;
        float probeY = 0.0f;
        const Ogre::Vector3 anchorPos = candidatePos + Ogre::Vector3(0, kKoMarkerHeadAnchorYOffset, 0);
        if (!state.projectionUtility->worldToScreenPX(anchorPos, probeX, probeY))
        {
            ++gProbeDiag.rejectProjection;
            if (speciesClassified && speciesInfo.isAnimal)
            {
                ++gProbeDiag.rejectProjectionAnimals;
                MaybeLogAnimalProbeSample(pluginName, speciesInfo, &targetHandle, "projection_failed", "");
            }
            return;
        }
    }

    if (targetHandle.isNull())
    {
        ++gProbeDiag.rejectNullHandle;
        if (speciesClassified && speciesInfo.isAnimal)
        {
            ++gProbeDiag.rejectNullHandleAnimals;
            MaybeLogAnimalProbeSample(pluginName, speciesInfo, &targetHandle, "null_handle", "");
        }
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

    ++gProbeDiag.accepted;
    if (speciesClassified && speciesInfo.isAnimal)
    {
        ++gProbeDiag.acceptedAnimals;
        if (speciesInfo.isLikelySpider)
        {
            ++gProbeDiag.acceptedSpiders;
        }
        MaybeLogAnimalProbeSample(pluginName, speciesInfo, &targetHandle, "accepted_animal", "");
    }
}

ProbeRenderDirective TickKoProbe(RuntimeStateView& state, const char* pluginName)
{
    EmitProbeDiagLogIfDue(pluginName);

    const bool anyHighlightVisualEnabled =
        state.config.showMarkerIcons
        || state.config.showMarkerText
        || state.config.showBountySymbol
        || state.config.enableCharacterTint;
    const bool canRun = state.config.enabled && anyHighlightVisualEnabled && IsHighlightGateOpen(state) && ou;
    if (!canRun)
    {
        if (state.highlightRuntimeActive)
        {
            state.koTargetCache.clear();
            state.highlightRuntimeActive = false;
            return PROBE_RENDER_HIDE_ALL;
        }
        return PROBE_RENDER_NONE;
    }
    state.highlightRuntimeActive = true;
    ++gProbeDiag.ticks;

    const DWORD nowMs = GetTickCount();

    if (state.lastProbeTickMs != 0 && (nowMs - state.lastProbeTickMs) < state.config.updateIntervalMs)
    {
        return PROBE_RENDER_TICK;
    }
    state.lastProbeTickMs = nowMs;

    const Ogre::Vector3 cameraCenter = ou->getCameraCenter();

    const ogre_unordered_set<Character*>::type& activeCharacters = ou->getCharacterUpdateList();
    const ogre_unordered_map<hand, Character*>::type& deathParadeCharacters = ou->deathParade;
    state.visibleKoHandlesScratch.clear();

    for (auto iter = activeCharacters.begin(); iter != activeCharacters.end(); ++iter)
    {
        ProcessMarkerCandidate(state, *iter, cameraCenter, nowMs, pluginName);
    }

    for (auto iter = deathParadeCharacters.begin(); iter != deathParadeCharacters.end(); ++iter)
    {
        Character* deathParadeCandidate = ResolveDeathParadeCandidateSafe(iter->first, iter->second);
        ProcessMarkerCandidate(state, deathParadeCandidate, cameraCenter, nowMs, pluginName);
    }

    for (int i = static_cast<int>(state.koTargetCache.size()) - 1; i >= 0; --i)
    {
        if (!VisibleHandleListContains(state, state.koTargetCache[static_cast<size_t>(i)].targetHandle))
        {
            state.koTargetCache.erase(state.koTargetCache.begin() + i);
        }
    }

    EmitProbeDiagLogIfDue(pluginName);
    return PROBE_RENDER_TICK;
}

} // namespace vs_probe
