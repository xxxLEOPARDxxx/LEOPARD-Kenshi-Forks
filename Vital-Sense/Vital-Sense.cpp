#include <Debug.h>

#include <core/Functions.h>
#include <kenshi/Character.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/Kenshi.h>
#include <kenshi/PlayerInterface.h>
#include <mygui/MyGUI_Colour.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_ImageBox.h>
#include <mygui/MyGUI_RenderManager.h>
#include <mygui/MyGUI_TextBox.h>

#include "src/vs_config.h"
#include "src/vs_log.h"
#include "src/vs_marker_render.h"
#include "src/vs_parse.h"
#include "src/vs_runtime_state.h"
#include "src/vs_types.h"

#ifndef BOOST_ALL_NO_LIB
#define BOOST_ALL_NO_LIB
#endif

#ifndef BOOST_ERROR_CODE_HEADER_ONLY
#define BOOST_ERROR_CODE_HEADER_ONLY
#endif

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
const char* kPluginName = "Vital-Sense";
const char* kConfigFileName = "mod-config.json";

PluginConfig g_config = {
    true,
    150,
    true,
    3500,
    true,
    true,
    true,
    true,
    true,
    "ZZ",
    "RC",
    "DY",
    "PD",
    "DE",
    18,
    18,
    18,
    18,
    18,
    MyGUI::Colour(1.0f, 0.2f, 0.2f, 1.0f),
    MyGUI::Colour(0.62f, 0.9f, 0.45f, 1.0f),
    MyGUI::Colour(0.25f, 1.0f, 0.25f, 1.0f),
    "",
    64,
    "",
    64,
    "",
    64,
    "",
    64,
    "",
    64,
    true,
    true,
    true,
    true,
    "$",
    18,
    true,
    true,
    1999,
    4999,
    9999,
    19999,
    39999,
    74999,
    MyGUI::Colour(0.690196f, 0.690196f, 0.690196f, 0.784314f),
    MyGUI::Colour(0.788235f, 0.647059f, 0.482353f, 0.831373f),
    MyGUI::Colour(0.623529f, 0.741176f, 0.411765f, 0.878431f),
    MyGUI::Colour(0.435294f, 0.650980f, 0.850980f, 0.925490f),
    MyGUI::Colour(1.000000f, 0.760784f, 0.278431f, 0.960784f),
    MyGUI::Colour(1.000000f, 0.541176f, 0.168627f, 0.980392f),
    MyGUI::Colour(0.878431f, 0.192157f, 0.192157f, 1.000000f),
    420
};
std::string g_settingsPath;
DWORD g_lastProbeTickMs = 0;
void (*PlayerInterface_updateUT_orig)(PlayerInterface*) = 0;
std::vector<CachedKoTarget> g_koTargetCache;
std::vector<hand> g_visibleKoHandlesScratch;
std::vector<KoMarkerWidget> g_koMarkerWidgets;
std::vector<std::string> g_iconTextureOkLogs;
std::vector<std::string> g_iconTextureWarnLogs;
UtilityT* g_projectionUtility = 0;
unsigned int g_koMarkerWidgetSerial = 0;
bool g_highlightRuntimeActive = false;

RuntimeStateView GetRuntimeStateView()
{
    RuntimeStateView state = CreateRuntimeStateView(
        g_config,
        g_settingsPath,
        g_lastProbeTickMs,
        PlayerInterface_updateUT_orig,
        g_koTargetCache,
        g_visibleKoHandlesScratch,
        g_koMarkerWidgets,
        g_iconTextureOkLogs,
        g_iconTextureWarnLogs,
        g_projectionUtility,
        g_koMarkerWidgetSerial,
        g_highlightRuntimeActive);

    return state;
}

const size_t kMaxKoMarkerWidgets = 48;
const float kKoMarkerHeadAnchorYOffset = 2.0f;
const float kProbablyDyingBloodMax = 50.0f;

void LogInfo(const std::string& message);
void LogWarn(const std::string& message);
bool IsMarkerStateEnabled(int markerState);

// REFACTOR_WRAPPER_PHASE6_REMOVE
void LogWithPrefix(void (*sink)(const char*), const char* level, const std::string& message)
{
    vs_log::LogWithPrefix(kPluginName, sink, level, message);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
void LogInfo(const std::string& message)
{
    vs_log::LogInfo(kPluginName, message);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
void LogWarn(const std::string& message)
{
    vs_log::LogWarn(kPluginName, message);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
void LogError(const std::string& message)
{
    vs_log::LogError(kPluginName, message);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
std::string ToLowerAsciiCopy(const std::string& value)
{
    return vs_parse::ToLowerAsciiCopy(value);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
std::string TrimAscii(const std::string& value)
{
    return vs_parse::TrimAscii(value);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
void SkipWhitespace(const std::string& body, size_t* pos)
{
    vs_parse::SkipWhitespace(body, pos);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
bool ParseBoolFromJson(const std::string& body, const char* keyName, bool* valueOut)
{
    return vs_parse::ParseBoolFromJson(body, keyName, valueOut);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
bool ParseUnsignedFromJson(const std::string& body, const char* keyName, DWORD* valueOut)
{
    try
    {
        return vs_parse::ParseUnsignedFromJson(body, keyName, valueOut);
    }
    catch (...)
    {
        return false;
    }
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
bool ParseStringFromJson(const std::string& body, const char* keyName, std::string* valueOut)
{
    return vs_parse::ParseStringFromJson(body, keyName, valueOut);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
bool TryParseHexNibble(char value, unsigned int* nibbleOut)
{
    return vs_parse::TryParseHexNibble(value, nibbleOut);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
bool TryParseHexByte(const std::string& value, size_t pos, unsigned int* byteOut)
{
    return vs_parse::TryParseHexByte(value, pos, byteOut);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
bool TryParseColourHex(const std::string& rawValue, MyGUI::Colour* colourOut)
{
    return vs_parse::TryParseColourHex(rawValue, colourOut);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
std::string ColourToHexRgb(const MyGUI::Colour& colour)
{
    return vs_parse::ColourToHexRgb(colour);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
bool LoadConfigState()
{
    RuntimeStateView state = GetRuntimeStateView();
    return vs_config::LoadConfigState(state, kPluginName);
}

bool IsSupportedVersion(KenshiLib::BinaryVersion versionInfo)
{
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    return platform != KenshiLib::BinaryVersion::UNKNOWN
        && (version == "1.0.65" || version == "1.0.68");
}

int FindCachedKoTargetIndex(const hand& targetHandle)
{
    for (size_t i = 0; i < g_koTargetCache.size(); ++i)
    {
        const hand& cached = g_koTargetCache[i].targetHandle;
        if (cached.type == targetHandle.type
            && cached.index == targetHandle.index
            && cached.serial == targetHandle.serial)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool VisibleHandleListContains(const hand& targetHandle)
{
    for (size_t i = 0; i < g_visibleKoHandlesScratch.size(); ++i)
    {
        const hand& visible = g_visibleKoHandlesScratch[i];
        if (visible.type == targetHandle.type
            && visible.index == targetHandle.index
            && visible.serial == targetHandle.serial)
        {
            return true;
        }
    }
    return false;
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
bool EnsureProjectionUtility()
{
    RuntimeStateView state = GetRuntimeStateView();
    return vs_marker_render::EnsureProjectionUtility(state);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
void HideAllKoMarkerWidgets()
{
    RuntimeStateView state = GetRuntimeStateView();
    vs_marker_render::HideAllKoMarkerWidgets(state, kPluginName);
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
void TickKoMarkerRender()
{
    RuntimeStateView state = GetRuntimeStateView();
    vs_marker_render::TickKoMarkerRender(state, kPluginName);
}

bool TryResolveMarkerState(Character* candidate, int* markerStateOut)
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

            // Preserve prior stable DY behavior (sub50 KO + no-return blood), then carve out RC
            // only for stable, non-dying coma cases.
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
        return IsMarkerStateEnabled(CachedKoTarget::STATE_DEAD);
    }

    if (isUnconscious)
    {
        if (isPlayingDead)
        {
            *markerStateOut = CachedKoTarget::STATE_PLAYING_DEAD;
            return IsMarkerStateEnabled(CachedKoTarget::STATE_PLAYING_DEAD);
        }
        if (isDying)
        {
            *markerStateOut = CachedKoTarget::STATE_DYING;
            return IsMarkerStateEnabled(CachedKoTarget::STATE_DYING);
        }
        if (isRecoveryComa)
        {
            *markerStateOut = CachedKoTarget::STATE_RECOVERY_COMA;
            return IsMarkerStateEnabled(CachedKoTarget::STATE_RECOVERY_COMA);
        }
        *markerStateOut = CachedKoTarget::STATE_UNCONSCIOUS;
        return IsMarkerStateEnabled(CachedKoTarget::STATE_UNCONSCIOUS);
    }

    return false;
}

bool IsHighlightGateOpen()
{
    if (!g_config.onlyWhenAltHeld)
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

bool IsMarkerStateEnabled(int markerState)
{
    if (markerState == CachedKoTarget::STATE_BOUNTY_ONLY)
    {
        return g_config.showBountySymbolOnAllCharacters;
    }
    if (markerState == CachedKoTarget::STATE_DEAD)
    {
        return g_config.enableDeadState;
    }
    if (markerState == CachedKoTarget::STATE_RECOVERY_COMA)
    {
        return g_config.enableRecoveryComaState;
    }
    if (markerState == CachedKoTarget::STATE_DYING)
    {
        return g_config.enableDyingState;
    }
    if (markerState == CachedKoTarget::STATE_PLAYING_DEAD)
    {
        return g_config.enablePlayingDeadState;
    }
    return g_config.enableUnconsciousState;
}

bool IsWithinHighlightRange(const Ogre::Vector3& sourcePos, const Ogre::Vector3& targetPos)
{
    if (g_config.maxHighlightDistanceMeters == 0)
    {
        return true;
    }

    const float maxDistance = static_cast<float>(g_config.maxHighlightDistanceMeters);
    const float maxDistanceSq = maxDistance * maxDistance;
    const float dx = targetPos.x - sourcePos.x;
    const float dz = targetPos.z - sourcePos.z;
    return (dx * dx + dz * dz) <= maxDistanceSq;
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
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

// REFACTOR_WRAPPER_PHASE6_REMOVE
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

// REFACTOR_WRAPPER_PHASE6_REMOVE
int ResolveMarkerRelationSafe(Character* candidate)
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
    return markerRelation;
}

// REFACTOR_WRAPPER_PHASE6_REMOVE
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

// REFACTOR_WRAPPER_PHASE6_REMOVE
void ProcessMarkerCandidate(Character* candidate, const Ogre::Vector3& cameraCenter, DWORD nowMs)
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
    const bool isDownedState = TryResolveMarkerState(candidate, &markerState);
    if (!isDownedState)
    {
        if (!g_config.showBountySymbol || !g_config.showBountySymbolOnAllCharacters || totalBounty <= 0)
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

    if (!IsWithinHighlightRange(cameraCenter, candidatePos))
    {
        return;
    }

    if (isDeadState && !isOnScreen)
    {
        if (!EnsureProjectionUtility())
        {
            return;
        }

        float probeX = 0.0f;
        float probeY = 0.0f;
        const Ogre::Vector3 anchorPos = candidatePos + Ogre::Vector3(0, kKoMarkerHeadAnchorYOffset, 0);
        if (!g_projectionUtility->worldToScreenPX(anchorPos, probeX, probeY))
        {
            return;
        }
    }

    if (targetHandle.isNull())
    {
        return;
    }

    const int markerRelation = ResolveMarkerRelationSafe(candidate);

    if (!VisibleHandleListContains(targetHandle))
    {
        g_visibleKoHandlesScratch.push_back(targetHandle);
    }

    const int existingIndex = FindCachedKoTargetIndex(targetHandle);
    if (existingIndex >= 0)
    {
        CachedKoTarget& existing = g_koTargetCache[existingIndex];
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
        g_koTargetCache.push_back(created);
    }
}

void TickKoProbe()
{
    const bool anyMarkerVisualEnabled = (g_config.showMarkerIcons || g_config.showMarkerText || g_config.showBountySymbol);
    const bool canRun = g_config.enabled && anyMarkerVisualEnabled && IsHighlightGateOpen() && ou;
    if (!canRun)
    {
        if (g_highlightRuntimeActive)
        {
            g_koTargetCache.clear();
            HideAllKoMarkerWidgets();
            g_highlightRuntimeActive = false;
        }
        return;
    }
    g_highlightRuntimeActive = true;

    const DWORD nowMs = GetTickCount();

    if (g_lastProbeTickMs != 0 && (nowMs - g_lastProbeTickMs) < g_config.updateIntervalMs)
    {
        TickKoMarkerRender();
        return;
    }
    g_lastProbeTickMs = nowMs;

    const Ogre::Vector3 cameraCenter = ou->getCameraCenter();

    const ogre_unordered_set<Character*>::type& activeCharacters = ou->getCharacterUpdateList();
    const ogre_unordered_map<hand, Character*>::type& deathParadeCharacters = ou->deathParade;
    g_visibleKoHandlesScratch.clear();

    for (auto iter = activeCharacters.begin(); iter != activeCharacters.end(); ++iter)
    {
        ProcessMarkerCandidate(*iter, cameraCenter, nowMs);
    }

    for (auto iter = deathParadeCharacters.begin(); iter != deathParadeCharacters.end(); ++iter)
    {
        Character* deathParadeCandidate = ResolveDeathParadeCandidateSafe(iter->first, iter->second);
        ProcessMarkerCandidate(deathParadeCandidate, cameraCenter, nowMs);
    }

    for (int i = static_cast<int>(g_koTargetCache.size()) - 1; i >= 0; --i)
    {
        if (!VisibleHandleListContains(g_koTargetCache[static_cast<size_t>(i)].targetHandle))
        {
            g_koTargetCache.erase(g_koTargetCache.begin() + i);
        }
    }
    TickKoMarkerRender();
}

void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    if (PlayerInterface_updateUT_orig)
    {
        PlayerInterface_updateUT_orig(thisptr);
    }
    TickKoProbe();
}
}

__declspec(dllexport) void startPlugin()
{
    LogInfo("startPlugin()");

    const KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    if (!IsSupportedVersion(versionInfo))
    {
        LogError("unsupported Kenshi version/platform");
        return;
    }

    LoadConfigState();

    std::stringstream info;
    info << "loaded (enabled=" << (g_config.enabled ? "true" : "false")
         << ", update_interval_ms=" << g_config.updateIntervalMs
         << ", only_when_alt_held=" << (g_config.onlyWhenAltHeld ? "true" : "false")
         << ", enable_unconscious=" << (g_config.enableUnconsciousState ? "true" : "false")
         << ", enable_recovery_coma=" << (g_config.enableRecoveryComaState ? "true" : "false")
         << ", enable_dying=" << (g_config.enableDyingState ? "true" : "false")
         << ", enable_playing_dead=" << (g_config.enablePlayingDeadState ? "true" : "false")
         << ", enable_dead=" << (g_config.enableDeadState ? "true" : "false")
         << ", show_icons=" << (g_config.showMarkerIcons ? "true" : "false")
         << ", show_text=" << (g_config.showMarkerText ? "true" : "false")
         << ", show_bounty_glow=" << (g_config.showBountyGlow ? "true" : "false")
         << ", show_bounty_symbol=" << (g_config.showBountySymbol ? "true" : "false")
         << ", show_bounty_symbol_on_all_characters=" << (g_config.showBountySymbolOnAllCharacters ? "true" : "false")
         << ", bounty_symbol=" << g_config.bountySymbolText
         << ", bounty_symbol_size_px=" << g_config.bountySymbolTextSizePx
         << ", bounty_symbol_position=" << (g_config.placeBountySymbolBeforeStateIcon ? "before_state_icon" : "before_state_text")
         << ", bounty_symbol_live_anchor_y_offset_cm=" << g_config.bountySymbolLiveAnchorYOffsetCm
         << ", unconscious_text=" << g_config.unconsciousText
         << ", recovery_coma_text=" << g_config.recoveryComaText
         << ", dying_text=" << g_config.dyingText
         << ", playing_dead_text=" << g_config.playingDeadText
         << ", dead_text=" << g_config.deadText
         << ", unconscious_text_size_px=" << g_config.unconsciousTextSizePx
         << ", recovery_coma_text_size_px=" << g_config.recoveryComaTextSizePx
         << ", dying_text_size_px=" << g_config.dyingTextSizePx
         << ", playing_dead_text_size_px=" << g_config.playingDeadTextSizePx
         << ", dead_text_size_px=" << g_config.deadTextSizePx
         << ", enemy_color_hex=" << ColourToHexRgb(g_config.enemyMarkerColour)
         << ", ally_color_hex=" << ColourToHexRgb(g_config.allyMarkerColour)
         << ", squad_color_hex=" << ColourToHexRgb(g_config.squadMarkerColour)
         << ", bounty_tier_trivial_max=" << g_config.bountyTierTrivialMax
         << ", bounty_tier_low_max=" << g_config.bountyTierLowMax
         << ", bounty_tier_modest_max=" << g_config.bountyTierModestMax
         << ", bounty_tier_notable_max=" << g_config.bountyTierNotableMax
         << ", bounty_tier_high_value_max=" << g_config.bountyTierHighValueMax
         << ", bounty_tier_elite_max=" << g_config.bountyTierEliteMax
         << ", bounty_color_trivial_hex=" << ColourToHexRgb(g_config.bountyTierTrivialColour)
         << ", bounty_color_low_hex=" << ColourToHexRgb(g_config.bountyTierLowColour)
         << ", bounty_color_modest_hex=" << ColourToHexRgb(g_config.bountyTierModestColour)
         << ", bounty_color_notable_hex=" << ColourToHexRgb(g_config.bountyTierNotableColour)
         << ", bounty_color_high_value_hex=" << ColourToHexRgb(g_config.bountyTierHighValueColour)
         << ", bounty_color_elite_hex=" << ColourToHexRgb(g_config.bountyTierEliteColour)
         << ", bounty_color_legendary_hex=" << ColourToHexRgb(g_config.bountyTierLegendaryColour)
         << ", max_highlight_distance_m=" << g_config.maxHighlightDistanceMeters
         << ")";
    LogInfo(info.str());

    g_koTargetCache.reserve(128);
    g_visibleKoHandlesScratch.reserve(128);
    g_koMarkerWidgets.reserve(kMaxKoMarkerWidgets);

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        &PlayerInterface_updateUT_orig))
    {
        LogError("could not hook PlayerInterface::updateUT");
        return;
    }

    LogInfo("update hook installed");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        char dllPath[_MAX_PATH] = { 0 };
        if (GetModuleFileNameA(hModule, dllPath, _MAX_PATH) > 0)
        {
            const std::string fullPath = TrimAscii(std::string(dllPath));
            const size_t sep = fullPath.find_last_of("\\/");
            if (sep != std::string::npos)
            {
                const std::string pluginDir = fullPath.substr(0, sep);
                g_settingsPath = pluginDir + "\\" + kConfigFileName;
            }
        }
    }

    return TRUE;
}
