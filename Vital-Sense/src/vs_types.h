#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <kenshi/Character.h>
#include <mygui/MyGUI_Colour.h>

#include <stddef.h>
#include <stdint.h>
#include <map>
#include <string>

namespace MyGUI
{
class ImageBox;
class TextBox;
}

struct PluginConfig
{
    bool enabled;
    DWORD updateIntervalMs;
    int32_t highlightKeyCode;
    bool highlightKeyRequireCtrl;
    bool highlightKeyRequireShift;
    bool highlightKeyRequireAlt;
    DWORD maxHighlightDistanceMeters;
    bool enableUnconsciousState;
    bool enableRecoveryComaState;
    bool enableDyingState;
    bool enablePlayingDeadState;
    bool enableDeadState;
    std::string unconsciousText;
    std::string recoveryComaText;
    std::string dyingText;
    std::string playingDeadText;
    std::string deadText;
    DWORD unconsciousTextSizePx;
    DWORD recoveryComaTextSizePx;
    DWORD dyingTextSizePx;
    DWORD playingDeadTextSizePx;
    DWORD deadTextSizePx;
    MyGUI::Colour enemyMarkerColour;
    MyGUI::Colour allyMarkerColour;
    MyGUI::Colour squadMarkerColour;
    std::string customUnconsciousIconTexture;
    DWORD customUnconsciousIconSizePx;
    std::string customRecoveryComaIconTexture;
    DWORD customRecoveryComaIconSizePx;
    std::string customDyingIconTexture;
    DWORD customDyingIconSizePx;
    std::string customPlayingDeadIconTexture;
    DWORD customPlayingDeadIconSizePx;
    std::string customDeadIconTexture;
    DWORD customDeadIconSizePx;
    bool showMarkerIcons;
    bool showMarkerText;
    bool showBountyGlow;
    bool showBountySymbol;
    bool debugLogDiagnostics;
    bool debugLogTextureInfo;
    bool enableCharacterTint;
    bool characterTintIncludeSquad;
    bool characterTintIncludeBountyOnly;
    bool characterTintForceDepthOverride;
    std::string bountySymbolText;
    DWORD bountySymbolTextSizePx;
    bool showBountySymbolOnAllCharacters;
    bool placeBountySymbolBeforeStateIcon;
    DWORD bountyTierTrivialMax;
    DWORD bountyTierLowMax;
    DWORD bountyTierModestMax;
    DWORD bountyTierNotableMax;
    DWORD bountyTierHighValueMax;
    DWORD bountyTierEliteMax;
    MyGUI::Colour bountyTierTrivialColour;
    MyGUI::Colour bountyTierLowColour;
    MyGUI::Colour bountyTierModestColour;
    MyGUI::Colour bountyTierNotableColour;
    MyGUI::Colour bountyTierHighValueColour;
    MyGUI::Colour bountyTierEliteColour;
    MyGUI::Colour bountyTierLegendaryColour;
    DWORD bountySymbolLiveAnchorYOffsetCm;
};

struct CachedKoTarget
{
    enum MarkerState
    {
        STATE_UNCONSCIOUS = 0,
        STATE_RECOVERY_COMA = 1,
        STATE_DYING = 2,
        STATE_PLAYING_DEAD = 3,
        STATE_DEAD = 4,
        STATE_BOUNTY_ONLY = 5
    };

    enum MarkerRelation
    {
        RELATION_SQUAD = 0,
        RELATION_ALLY = 1,
        RELATION_ENEMY = 2
    };

    hand targetHandle;
    Ogre::Vector3 worldPos;
    DWORD lastSeenMs;
    int markerState;
    int markerRelation;
    int totalBounty;
    unsigned int lastSeenGeneration;
};

struct CharacterTintEntry
{
    hand targetHandle;
    int markerRelation;
};

struct ProbeDeferredCandidateState
{
    ProbeDeferredCandidateState()
        : nextEligibleProbeMs(0)
    {
    }

    DWORD nextEligibleProbeMs;
};

struct ProbeHandleKey
{
    ProbeHandleKey()
        : type(0)
        , index(0)
        , serial(0)
    {
    }

    unsigned int type;
    unsigned int index;
    unsigned int serial;

    bool operator<(const ProbeHandleKey& other) const
    {
        if (type != other.type)
        {
            return type < other.type;
        }
        if (index != other.index)
        {
            return index < other.index;
        }
        return serial < other.serial;
    }
};

struct ProbeRuntimeCaches
{
    ProbeRuntimeCaches()
        : generation(0)
        , currentProbeIntervalMs(0)
        , lastProbeCandidateCount(0)
    {
    }

    std::map<ProbeHandleKey, size_t> koTargetIndexByHandle;
    std::map<ProbeHandleKey, ProbeDeferredCandidateState> notDownedByHandle;
    unsigned int generation;
    DWORD currentProbeIntervalMs;
    unsigned int lastProbeCandidateCount;
};

struct KoMarkerVisualState
{
    bool valid;
    unsigned int configRevision;
    int markerState;
    int markerRelation;
    int totalBounty;
    bool showStateIcon;
    bool showStateText;
    bool showBountyGlow;
    bool showBountySymbol;
    bool visible;
    bool positionValid;
    int positionLeft;
    int positionTop;
    int positionTotalBounty;
    bool positionShowStateIcon;
    bool positionShowStateText;
    bool positionShowBountySymbol;
    bool positionIsBountyOnlyState;
};

struct KoMarkerWidget
{
    MyGUI::ImageBox* bountyGlow;
    MyGUI::ImageBox* icon;
    MyGUI::TextBox* bountySymbol;
    MyGUI::TextBox* fallbackTextOutline[8];
    MyGUI::TextBox* fallbackText;
    KoMarkerVisualState visualState;
};
