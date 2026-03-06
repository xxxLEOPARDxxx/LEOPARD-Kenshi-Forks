#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <kenshi/Character.h>
#include <mygui/MyGUI_Colour.h>

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
    bool onlyWhenAltHeld;
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
};

struct CharacterTintEntry
{
    hand targetHandle;
    int markerRelation;
};

struct KoMarkerWidget
{
    MyGUI::ImageBox* bountyGlow;
    MyGUI::ImageBox* icon;
    MyGUI::TextBox* bountySymbol;
    MyGUI::TextBox* fallbackText;
};
