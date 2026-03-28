#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <string>

namespace vr_config
{
struct PluginConfig
{
    PluginConfig();

    bool enabled;
    bool debugLogging;
    bool debugSearchLogging;
    bool debugBindingLogging;
    bool showIcons;
    bool showText;
    DWORD portraitIconDisplaySizePx;
    DWORD portraitTextFontHeightPx;
    int portraitOverlayMarginXPx;
    int portraitOverlayMarginYPx;
    std::string portraitIconAnchor;
    std::string portraitTextAnchor;
    std::string unconsciousIconTexture;
    DWORD unconsciousIconSizePx;
    bool unconsciousIconHasImageCoord;
    int unconsciousIconCoordLeft;
    int unconsciousIconCoordTop;
    int unconsciousIconCoordWidth;
    int unconsciousIconCoordHeight;
    std::string recoveryComaIconTexture;
    DWORD recoveryComaIconSizePx;
    bool recoveryComaIconHasImageCoord;
    int recoveryComaIconCoordLeft;
    int recoveryComaIconCoordTop;
    int recoveryComaIconCoordWidth;
    int recoveryComaIconCoordHeight;
    std::string dyingIconTexture;
    DWORD dyingIconSizePx;
    bool dyingIconHasImageCoord;
    int dyingIconCoordLeft;
    int dyingIconCoordTop;
    int dyingIconCoordWidth;
    int dyingIconCoordHeight;
    std::string playingDeadIconTexture;
    DWORD playingDeadIconSizePx;
    bool playingDeadIconHasImageCoord;
    int playingDeadIconCoordLeft;
    int playingDeadIconCoordTop;
    int playingDeadIconCoordWidth;
    int playingDeadIconCoordHeight;
    std::string starvingIconTexture;
    DWORD starvingIconSizePx;
    bool starvingIconHasImageCoord;
    int starvingIconCoordLeft;
    int starvingIconCoordTop;
    int starvingIconCoordWidth;
    int starvingIconCoordHeight;
    std::string crippledArmIconTexture;
    DWORD crippledArmIconSizePx;
    bool crippledArmIconHasImageCoord;
    int crippledArmIconCoordLeft;
    int crippledArmIconCoordTop;
    int crippledArmIconCoordWidth;
    int crippledArmIconCoordHeight;
    std::string crippledLegIconTexture;
    DWORD crippledLegIconSizePx;
    bool crippledLegIconHasImageCoord;
    int crippledLegIconCoordLeft;
    int crippledLegIconCoordTop;
    int crippledLegIconCoordWidth;
    int crippledLegIconCoordHeight;
};

enum LoadStatus
{
    LOAD_OK = 0,
    LOAD_READ_FAILED
};

LoadStatus LoadFromFile(const std::string& path, PluginConfig* outConfig);
bool SaveToFile(const std::string& path, const PluginConfig& config);
}
