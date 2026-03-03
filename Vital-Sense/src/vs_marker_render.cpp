#include "vs_marker_render.h"

#include "vs_log.h"

#include <kenshi/Character.h>
#include <kenshi/Kenshi.h>
#include <mygui/MyGUI_Colour.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_ImageBox.h>
#include <mygui/MyGUI_RenderManager.h>
#include <mygui/MyGUI_TextBox.h>

#include <Windows.h>

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

namespace vs_marker_render
{
namespace
{
const size_t kMaxKoMarkerWidgets = 48;
const int kKoMarkerWidthPx = 64;
const int kKoMarkerHeightPx = 18;
const int kKoBountySymbolMinWidthPx = 12;
const int kKoBountySymbolGapPx = 1;
const int kKoMarkerYOffsetPx = 24;
const float kKoMarkerHeadAnchorYOffset = 2.0f;
const float kKoCentimetersToWorldUnits = 0.01f;
const int kKoBountyGlowPaddingPx = 3;
const int kKoBountyGlowOffsetXPx = -kKoBountyGlowPaddingPx;
const int kKoBountyGlowOffsetYPx = -kKoBountyGlowPaddingPx;
const float kKoBountyGlowAlpha = 0.90f;
const MyGUI::Colour kKoBountyGlowColour(1.0f, 0.93f, 0.28f, kKoBountyGlowAlpha);
const char* kKoBountyGlowTexture = "gui/gfx/bounty_glow_64px.png";
const int kKoBountyGlowTextureSizePx = 64;
const bool kEnableTextureInfoLogs = false;

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

void LogIconTextureOnce(
    RuntimeStateView& state,
    const char* pluginName,
    std::vector<std::string>& sink,
    const std::string& message,
    const char* textureName,
    bool warn)
{
    (void)state;

    if (!warn && !kEnableTextureInfoLogs)
    {
        return;
    }

    if (!textureName || !*textureName)
    {
        return;
    }

    const std::string texture(textureName);
    if (StringListContains(sink, texture))
    {
        return;
    }
    sink.push_back(texture);

    std::stringstream ss;
    ss << message << " texture=" << texture;
    if (warn)
    {
        vs_log::LogWarn(pluginName, ss.str());
    }
    else
    {
        vs_log::LogInfo(pluginName, ss.str());
    }
}

void SetWidgetVisible(MyGUI::Widget* widget, bool visible)
{
    if (!widget)
    {
        return;
    }

    __try
    {
        widget->setVisible(visible);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

void SetKoMarkerCaption(MyGUI::TextBox* marker, const char* caption)
{
    if (!marker || !caption)
    {
        return;
    }

    try
    {
        marker->setCaption(caption);
    }
    catch (...)
    {
    }
}

void SetKoMarkerTextColour(MyGUI::TextBox* marker, const MyGUI::Colour& colour)
{
    if (!marker)
    {
        return;
    }

    try
    {
        marker->setTextColour(colour);
    }
    catch (...)
    {
    }
}

void SetKoMarkerTextFontHeight(MyGUI::TextBox* marker, int fontHeight)
{
    if (!marker || fontHeight <= 0)
    {
        return;
    }

    try
    {
        marker->setFontHeight(fontHeight);
    }
    catch (...)
    {
    }
}

void SetKoMarkerIconColour(MyGUI::ImageBox* marker, const MyGUI::Colour& colour)
{
    if (!marker)
    {
        return;
    }

    try
    {
        marker->setColour(colour);
    }
    catch (...)
    {
    }
}

bool SetKoMarkerIconTexture(RuntimeStateView& state, const char* pluginName, MyGUI::ImageBox* marker, const char* texture)
{
    if (!marker || !texture)
    {
        return false;
    }

    const char* fallbackTexture = "Kenshi_UI.png";
    const bool requestIsFallbackTexture = (std::strcmp(texture, fallbackTexture) == 0);
    std::vector<std::string> textureCandidates;
    const std::string requestedTexture(texture);

    const auto addCandidate = [&textureCandidates](const std::string& candidate)
    {
        if (!candidate.empty() && !StringListContains(textureCandidates, candidate))
        {
            textureCandidates.push_back(candidate);
        }
    };

    const auto addCandidateWithSeparatorVariants = [&addCandidate](const std::string& candidate)
    {
        if (candidate.empty())
        {
            return;
        }

        addCandidate(candidate);

        std::string withForwardSlashes = candidate;
        bool changedToForward = false;
        for (size_t i = 0; i < withForwardSlashes.size(); ++i)
        {
            if (withForwardSlashes[i] == '\\')
            {
                withForwardSlashes[i] = '/';
                changedToForward = true;
            }
        }
        if (changedToForward)
        {
            addCandidate(withForwardSlashes);
        }

        std::string withBackSlashes = candidate;
        bool changedToBack = false;
        for (size_t i = 0; i < withBackSlashes.size(); ++i)
        {
            if (withBackSlashes[i] == '/')
            {
                withBackSlashes[i] = '\\';
                changedToBack = true;
            }
        }
        if (changedToBack)
        {
            addCandidate(withBackSlashes);
        }
    };

    addCandidateWithSeparatorVariants(requestedTexture);

    const bool hasPathSeparator = (std::strchr(texture, '/') != 0 || std::strchr(texture, '\\') != 0);
    const bool isAbsolutePath = (requestedTexture.size() >= 2 && requestedTexture[1] == ':') ||
        (!requestedTexture.empty() && (requestedTexture[0] == '/' || requestedTexture[0] == '\\'));
    const bool isModQualified = (requestedTexture.rfind("mods/", 0) == 0 || requestedTexture.rfind("mods\\", 0) == 0);

    if (!hasPathSeparator)
    {
        addCandidateWithSeparatorVariants(std::string("icons/") + requestedTexture);
        addCandidateWithSeparatorVariants(std::string("gui/gfx/") + requestedTexture);
        addCandidateWithSeparatorVariants(std::string("mods/") + pluginName + "/icons/" + requestedTexture);
        addCandidateWithSeparatorVariants(std::string("mods/") + pluginName + "/gui/gfx/" + requestedTexture);
        addCandidateWithSeparatorVariants(std::string("mods/") + pluginName + "/" + requestedTexture);
    }
    else if (!isAbsolutePath && !isModQualified)
    {
        addCandidateWithSeparatorVariants(std::string("mods/") + pluginName + "/" + requestedTexture);

        const size_t fileNameStart = requestedTexture.find_last_of("/\\");
        if (fileNameStart != std::string::npos && (fileNameStart + 1) < requestedTexture.size())
        {
            const std::string fileName = requestedTexture.substr(fileNameStart + 1);
            addCandidateWithSeparatorVariants(fileName);
            addCandidateWithSeparatorVariants(std::string("icons/") + fileName);
            addCandidateWithSeparatorVariants(std::string("gui/gfx/") + fileName);
            addCandidateWithSeparatorVariants(std::string("mods/") + pluginName + "/icons/" + fileName);
            addCandidateWithSeparatorVariants(std::string("mods/") + pluginName + "/gui/gfx/" + fileName);
            addCandidateWithSeparatorVariants(std::string("mods/") + pluginName + "/" + fileName);
        }
    }

    for (size_t i = 0; i < textureCandidates.size(); ++i)
    {
        const std::string& candidate = textureCandidates[i];
        bool applied = false;
        try
        {
            marker->setImageTexture(candidate);
            applied = true;
        }
        catch (...)
        {
        }

        if (!applied)
        {
            continue;
        }

        bool validSize = false;
        try
        {
            const MyGUI::IntSize size = marker->getImageSize();
            if (size.width > 0 && size.height > 0)
            {
                const bool suspiciousOversize = (!requestIsFallbackTexture && (size.width > 512 || size.height > 512));
                if (suspiciousOversize)
                {
                    std::stringstream ss;
                    ss << "icon texture suspicious size " << size.width << "x" << size.height;
                    LogIconTextureOnce(state, pluginName, state.iconTextureWarnLogs, ss.str(), candidate.c_str(), true);
                }
                else
                {
                    validSize = true;
                }
            }
            else
            {
            }

            if (validSize)
            {
                LogIconTextureOnce(state, pluginName, state.iconTextureOkLogs, "icon texture ready", candidate.c_str(), false);
                if (candidate != texture)
                {
                    LogIconTextureOnce(state, pluginName, state.iconTextureOkLogs, "icon texture resolved alias", texture, false);
                }
                if (candidate != fallbackTexture)
                {
                    return true;
                }
            }
        }
        catch (...)
        {
        }

        if (validSize)
        {
            return false;
        }
    }

    try
    {
        marker->setImageTexture(fallbackTexture);
        LogIconTextureOnce(state, pluginName, state.iconTextureWarnLogs, "icon texture fallback engaged", texture, true);
    }
    catch (...)
    {
        LogIconTextureOnce(state, pluginName, state.iconTextureWarnLogs, "icon fallback texture apply failed", fallbackTexture, true);
        return false;
    }

    try
    {
        const MyGUI::IntSize fallbackSize = marker->getImageSize();
        if (fallbackSize.width > 0 && fallbackSize.height > 0)
        {
            LogIconTextureOnce(state, pluginName, state.iconTextureOkLogs, "icon fallback ready", fallbackTexture, false);
        }
        else
        {
            LogIconTextureOnce(state, pluginName, state.iconTextureWarnLogs, "icon fallback has zero size", fallbackTexture, true);
        }
    }
    catch (...)
    {
        LogIconTextureOnce(state, pluginName, state.iconTextureWarnLogs, "icon fallback size probe failed", fallbackTexture, true);
    }

    return false;
}

void SetKoMarkerIconCoord(MyGUI::ImageBox* marker, const MyGUI::IntCoord& coord)
{
    if (!marker)
    {
        return;
    }

    try
    {
        marker->setImageCoord(coord);
        marker->setImageTile(MyGUI::IntSize(coord.width, coord.height));
    }
    catch (...)
    {
    }
}

int ResolveBountyTierIndex(RuntimeStateView& state, int totalBounty)
{
    if (totalBounty <= 0)
    {
        return -1;
    }

    const DWORD bounty = static_cast<DWORD>(totalBounty);
    if (bounty <= state.config.bountyTierTrivialMax)
    {
        return 0;
    }
    if (bounty <= state.config.bountyTierLowMax)
    {
        return 1;
    }
    if (bounty <= state.config.bountyTierModestMax)
    {
        return 2;
    }
    if (bounty <= state.config.bountyTierNotableMax)
    {
        return 3;
    }
    if (bounty <= state.config.bountyTierHighValueMax)
    {
        return 4;
    }
    if (bounty <= state.config.bountyTierEliteMax)
    {
        return 5;
    }
    return 6;
}

int ResolveBountySymbolFontHeightPx(RuntimeStateView& state, int totalBounty)
{
    int fontHeight = static_cast<int>(state.config.bountySymbolTextSizePx);
    if (state.config.bountySymbolText == "$")
    {
        fontHeight += 4;
    }

    const int tier = ResolveBountyTierIndex(state, totalBounty);
    if (tier >= 0)
    {
        static const int kTierBonusPx[7] = { 0, 0, 1, 2, 3, 4, 6 };
        fontHeight += kTierBonusPx[tier];
    }

    if (fontHeight < 8)
    {
        fontHeight = 8;
    }
    if (fontHeight > 128)
    {
        fontHeight = 128;
    }
    return fontHeight;
}

int ResolveBountySymbolWidthPx(RuntimeStateView& state, int totalBounty)
{
    int width = (ResolveBountySymbolFontHeightPx(state, totalBounty) * 3) / 4 + 6;
    if (state.config.bountySymbolText == "$" && width < 20)
    {
        width = 20;
    }
    if (width < kKoBountySymbolMinWidthPx)
    {
        width = kKoBountySymbolMinWidthPx;
    }
    if (width > 32)
    {
        width = 32;
    }
    return width;
}

void SetKoMarkerPosition(
    RuntimeStateView& state,
    KoMarkerWidget& marker,
    int left,
    int top,
    int totalBounty,
    bool showStateIcon,
    bool showStateText,
    bool showBountySymbol,
    bool isBountyOnlyState)
{
    const bool placeSymbolBeforeIcon = showBountySymbol && showStateIcon && state.config.placeBountySymbolBeforeStateIcon;
    const int symbolWidth = showBountySymbol ? ResolveBountySymbolWidthPx(state, totalBounty) : kKoBountySymbolMinWidthPx;
    int layoutLeft = left;
    int iconLeft = left;
    int symbolLeft = left;
    bool hasSymbolPlacement = false;

    if (showBountySymbol && isBountyOnlyState && !showStateIcon && !showStateText)
    {
        symbolLeft = left + ((kKoMarkerWidthPx - symbolWidth) / 2);
        hasSymbolPlacement = true;
    }

    if (!isBountyOnlyState && placeSymbolBeforeIcon)
    {
        symbolLeft = layoutLeft;
        hasSymbolPlacement = true;
        layoutLeft += symbolWidth + kKoBountySymbolGapPx;
    }

    if (showStateIcon)
    {
        iconLeft = layoutLeft;
        layoutLeft += kKoMarkerHeightPx + 1;
    }

    if (!isBountyOnlyState && showBountySymbol && !placeSymbolBeforeIcon)
    {
        symbolLeft = layoutLeft;
        hasSymbolPlacement = true;
        layoutLeft += symbolWidth + kKoBountySymbolGapPx;
    }

    if (marker.icon)
    {
        try
        {
            marker.icon->setCoord(iconLeft, top, kKoMarkerHeightPx, kKoMarkerHeightPx);
        }
        catch (...)
        {
        }
    }

    if (marker.bountySymbol)
    {
        try
        {
            marker.bountySymbol->setCoord(symbolLeft, top, symbolWidth, kKoMarkerHeightPx);
        }
        catch (...)
        {
        }
    }

    if (marker.bountyGlow)
    {
        try
        {
            int glowAnchorLeft = left;
            if (showStateIcon)
            {
                glowAnchorLeft = iconLeft;
            }
            else if (hasSymbolPlacement)
            {
                glowAnchorLeft = symbolLeft;
            }
            marker.bountyGlow->setCoord(
                glowAnchorLeft + kKoBountyGlowOffsetXPx,
                top + kKoBountyGlowOffsetYPx,
                kKoMarkerHeightPx + (kKoBountyGlowPaddingPx * 2),
                kKoMarkerHeightPx + (kKoBountyGlowPaddingPx * 2));
        }
        catch (...)
        {
        }
    }

    if (marker.fallbackText)
    {
        try
        {
            int textWidth = kKoMarkerWidthPx - (layoutLeft - left);
            if (textWidth < 0)
            {
                textWidth = 0;
            }
            if (textWidth > kKoMarkerWidthPx)
            {
                textWidth = kKoMarkerWidthPx;
            }
            if (showStateText)
            {
                marker.fallbackText->setCoord(layoutLeft, top, textWidth, kKoMarkerHeightPx);
            }
            else
            {
                marker.fallbackText->setCoord(layoutLeft, top, 0, kKoMarkerHeightPx);
            }
        }
        catch (...)
        {
        }
    }
}

void SetKoMarkerVisible(
    RuntimeStateView& state,
    KoMarkerWidget& marker,
    bool visible,
    bool showBountyGlow,
    bool showStateIcon,
    bool showStateText,
    bool showBountySymbol)
{
    SetWidgetVisible(marker.bountyGlow, visible && state.config.showMarkerIcons && showBountyGlow);
    SetWidgetVisible(marker.icon, visible && showStateIcon);
    SetWidgetVisible(marker.bountySymbol, visible && showBountySymbol);
    SetWidgetVisible(marker.fallbackText, visible && showStateText);
}

const char* ResolveMarkerCaption(RuntimeStateView& state, int markerState)
{
    if (markerState == CachedKoTarget::STATE_BOUNTY_ONLY)
    {
        return "";
    }
    if (markerState == CachedKoTarget::STATE_DEAD)
    {
        return state.config.deadText.empty() ? "DE" : state.config.deadText.c_str();
    }
    if (markerState == CachedKoTarget::STATE_RECOVERY_COMA)
    {
        return state.config.recoveryComaText.empty() ? "RC" : state.config.recoveryComaText.c_str();
    }
    if (markerState == CachedKoTarget::STATE_DYING)
    {
        return state.config.dyingText.empty() ? "DY" : state.config.dyingText.c_str();
    }
    if (markerState == CachedKoTarget::STATE_PLAYING_DEAD)
    {
        return state.config.playingDeadText.empty() ? "PD" : state.config.playingDeadText.c_str();
    }
    return state.config.unconsciousText.empty() ? "ZZ" : state.config.unconsciousText.c_str();
}

const char* ResolveMarkerIconTexture(int markerState, int markerRelation)
{
    (void)markerState;
    (void)markerRelation;
    return "Kenshi_UI.png";
}

MyGUI::IntCoord ResolveMarkerIconCoord(int markerState, int markerRelation)
{
    (void)markerRelation;
    if (markerState == CachedKoTarget::STATE_DEAD)
    {
        return MyGUI::IntCoord(108, 49, 34, 34);
    }
    if (markerState == CachedKoTarget::STATE_DYING)
    {
        return MyGUI::IntCoord(108, 49, 34, 34);
    }
    if (markerState == CachedKoTarget::STATE_RECOVERY_COMA)
    {
        return MyGUI::IntCoord(45, 122, 32, 32);
    }
    if (markerState == CachedKoTarget::STATE_PLAYING_DEAD)
    {
        return MyGUI::IntCoord(80, 126, 38, 27);
    }

    return MyGUI::IntCoord(45, 122, 32, 32);
}

MyGUI::Colour ResolveMarkerColour(RuntimeStateView& state, int markerState, int markerRelation)
{
    (void)markerState;
    if (markerRelation == CachedKoTarget::RELATION_ENEMY)
    {
        return state.config.enemyMarkerColour;
    }

    if (markerRelation == CachedKoTarget::RELATION_SQUAD)
    {
        return state.config.squadMarkerColour;
    }

    return state.config.allyMarkerColour;
}

bool IsBountyOnlyMarkerState(int markerState)
{
    return markerState == CachedKoTarget::STATE_BOUNTY_ONLY;
}

MyGUI::Colour ResolveBountyTierColour(RuntimeStateView& state, int totalBounty)
{
    const int tier = ResolveBountyTierIndex(state, totalBounty);
    if (tier < 0)
    {
        return MyGUI::Colour(0.0f, 0.0f, 0.0f, 0.0f);
    }

    if (tier == 0)
    {
        return state.config.bountyTierTrivialColour;
    }
    if (tier == 1)
    {
        return state.config.bountyTierLowColour;
    }
    if (tier == 2)
    {
        return state.config.bountyTierModestColour;
    }
    if (tier == 3)
    {
        return state.config.bountyTierNotableColour;
    }
    if (tier == 4)
    {
        return state.config.bountyTierHighValueColour;
    }
    if (tier == 5)
    {
        return state.config.bountyTierEliteColour;
    }
    return state.config.bountyTierLegendaryColour;
}

MyGUI::IntCoord ResolveCustomIconCoordFromImageSize(MyGUI::ImageBox* marker, int fallbackSize)
{
    (void)marker;
    int size = fallbackSize;
    if (size <= 0)
    {
        size = 64;
    }
    return MyGUI::IntCoord(0, 0, size, size);
}

const std::string* ResolveCustomMarkerIconTexture(RuntimeStateView& state, int markerState)
{
    if (markerState == CachedKoTarget::STATE_DEAD)
    {
        return &state.config.customDeadIconTexture;
    }
    if (markerState == CachedKoTarget::STATE_RECOVERY_COMA)
    {
        return &state.config.customRecoveryComaIconTexture;
    }
    if (markerState == CachedKoTarget::STATE_DYING)
    {
        return &state.config.customDyingIconTexture;
    }
    if (markerState == CachedKoTarget::STATE_PLAYING_DEAD)
    {
        return &state.config.customPlayingDeadIconTexture;
    }
    return &state.config.customUnconsciousIconTexture;
}

DWORD ResolveCustomMarkerIconSizePx(RuntimeStateView& state, int markerState)
{
    if (markerState == CachedKoTarget::STATE_DEAD)
    {
        return state.config.customDeadIconSizePx;
    }
    if (markerState == CachedKoTarget::STATE_RECOVERY_COMA)
    {
        return state.config.customRecoveryComaIconSizePx;
    }
    if (markerState == CachedKoTarget::STATE_DYING)
    {
        return state.config.customDyingIconSizePx;
    }
    if (markerState == CachedKoTarget::STATE_PLAYING_DEAD)
    {
        return state.config.customPlayingDeadIconSizePx;
    }
    return state.config.customUnconsciousIconSizePx;
}

DWORD ResolveMarkerTextSizePx(RuntimeStateView& state, int markerState)
{
    if (markerState == CachedKoTarget::STATE_DEAD)
    {
        return state.config.deadTextSizePx;
    }
    if (markerState == CachedKoTarget::STATE_RECOVERY_COMA)
    {
        return state.config.recoveryComaTextSizePx;
    }
    if (markerState == CachedKoTarget::STATE_DYING)
    {
        return state.config.dyingTextSizePx;
    }
    if (markerState == CachedKoTarget::STATE_PLAYING_DEAD)
    {
        return state.config.playingDeadTextSizePx;
    }
    return state.config.unconsciousTextSizePx;
}

bool HasCustomMarkerIcon(RuntimeStateView& state, int markerState)
{
    const std::string* texture = ResolveCustomMarkerIconTexture(state, markerState);
    return texture && !texture->empty();
}

void ApplyKoMarkerVisualState(
    RuntimeStateView& state,
    const char* pluginName,
    KoMarkerWidget& marker,
    int markerState,
    int markerRelation,
    int totalBounty,
    bool showBountyGlow,
    bool showBountySymbol)
{
    const bool isBountyOnlyState = IsBountyOnlyMarkerState(markerState);
    const MyGUI::Colour colour = ResolveMarkerColour(state, markerState, markerRelation);
    const bool wantsCustomIcon = !isBountyOnlyState && HasCustomMarkerIcon(state, markerState);
    const std::string* customTexture = ResolveCustomMarkerIconTexture(state, markerState);
    const int customIconSize = static_cast<int>(ResolveCustomMarkerIconSizePx(state, markerState));

    if (marker.icon && !isBountyOnlyState)
    {
        if (state.config.showMarkerIcons)
        {
            bool customIconReady = false;
            if (wantsCustomIcon && customTexture)
            {
                customIconReady = SetKoMarkerIconTexture(state, pluginName, marker.icon, customTexture->c_str());
                if (customIconReady)
                {
                    SetKoMarkerIconCoord(marker.icon, ResolveCustomIconCoordFromImageSize(marker.icon, customIconSize));
                    SetKoMarkerIconColour(marker.icon, MyGUI::Colour(1.0f, 1.0f, 1.0f, 1.0f));
                }
                else
                {
                    SetKoMarkerIconTexture(state, pluginName, marker.icon, ResolveMarkerIconTexture(markerState, markerRelation));
                    SetKoMarkerIconCoord(marker.icon, ResolveMarkerIconCoord(markerState, markerRelation));
                    SetKoMarkerIconColour(marker.icon, colour);
                }
            }
            else
            {
                SetKoMarkerIconTexture(state, pluginName, marker.icon, ResolveMarkerIconTexture(markerState, markerRelation));
                SetKoMarkerIconCoord(marker.icon, ResolveMarkerIconCoord(markerState, markerRelation));
                SetKoMarkerIconColour(marker.icon, colour);
            }
        }
    }

    if (marker.bountyGlow && state.config.showMarkerIcons && showBountyGlow)
    {
        const bool glowTextureReady = SetKoMarkerIconTexture(state, pluginName, marker.bountyGlow, kKoBountyGlowTexture);
        if (glowTextureReady)
        {
            SetKoMarkerIconCoord(marker.bountyGlow, ResolveCustomIconCoordFromImageSize(marker.bountyGlow, kKoBountyGlowTextureSizePx));
            SetKoMarkerIconColour(marker.bountyGlow, kKoBountyGlowColour);
        }
        else
        {
            SetKoMarkerIconCoord(marker.bountyGlow, MyGUI::IntCoord(0, 0, 0, 0));
            SetKoMarkerIconColour(marker.bountyGlow, MyGUI::Colour(1.0f, 1.0f, 1.0f, 0.0f));
        }
    }

    if (marker.fallbackText && state.config.showMarkerText)
    {
        if (isBountyOnlyState)
        {
            SetKoMarkerCaption(marker.fallbackText, "");
        }
        else
        {
            const int fontHeight = static_cast<int>(ResolveMarkerTextSizePx(state, markerState));
            SetKoMarkerTextFontHeight(marker.fallbackText, fontHeight);
            SetKoMarkerCaption(marker.fallbackText, ResolveMarkerCaption(state, markerState));
            SetKoMarkerTextColour(marker.fallbackText, colour);
        }
    }

    if (marker.bountySymbol)
    {
        if (showBountySymbol)
        {
            const int symbolFontHeight = ResolveBountySymbolFontHeightPx(state, totalBounty);
            const MyGUI::Colour bountyColour = ResolveBountyTierColour(state, totalBounty);
            SetKoMarkerTextFontHeight(marker.bountySymbol, symbolFontHeight);
            SetKoMarkerCaption(marker.bountySymbol, state.config.bountySymbolText.c_str());
            SetKoMarkerTextColour(marker.bountySymbol, bountyColour);
        }
        else
        {
            SetKoMarkerCaption(marker.bountySymbol, "");
        }
    }
}

void HideAllKoMarkerWidgetsInternal(RuntimeStateView& state)
{
    for (size_t i = 0; i < state.koMarkerWidgets.size(); ++i)
    {
        SetKoMarkerVisible(state, state.koMarkerWidgets[i], false, false, false, false, false);
    }
}

bool CreateKoMarkerWidgetAt(RuntimeStateView& state, size_t index, const char* pluginName)
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (!gui)
    {
        return false;
    }

    try
    {
        std::stringstream name;
        name << "VS_KOMarker_" << index << "_" << state.koMarkerWidgetSerial++;

        MyGUI::ImageBox* bountyGlow = gui->createWidget<MyGUI::ImageBox>(
            "ImageBox",
            MyGUI::IntCoord(0, 0, kKoMarkerHeightPx + (kKoBountyGlowPaddingPx * 2), kKoMarkerHeightPx + (kKoBountyGlowPaddingPx * 2)),
            MyGUI::Align::Default,
            "Top",
            name.str() + "_bounty_glow");

        MyGUI::ImageBox* icon = gui->createWidget<MyGUI::ImageBox>(
            "ImageBox",
            MyGUI::IntCoord(0, 0, kKoMarkerHeightPx, kKoMarkerHeightPx),
            MyGUI::Align::Default,
            "Top",
            name.str() + "_icon");

        MyGUI::TextBox* bountySymbol = gui->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(0, 0, kKoBountySymbolMinWidthPx, kKoMarkerHeightPx),
            MyGUI::Align::Default,
            "Top",
            name.str() + "_bounty_symbol");
        if (!bountySymbol)
        {
            bountySymbol = gui->createWidget<MyGUI::TextBox>(
                "TextBox",
                MyGUI::IntCoord(0, 0, kKoBountySymbolMinWidthPx, kKoMarkerHeightPx),
                MyGUI::Align::Default,
                "Top",
                name.str() + "_bounty_symbol_fallback");
        }

        MyGUI::TextBox* fallbackText = gui->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(kKoMarkerHeightPx + 1, 0, kKoMarkerWidthPx - (kKoMarkerHeightPx + 1), kKoMarkerHeightPx),
            MyGUI::Align::Default,
            "Top",
            name.str());
        if (!fallbackText)
        {
            fallbackText = gui->createWidget<MyGUI::TextBox>(
                "TextBox",
                MyGUI::IntCoord(kKoMarkerHeightPx + 1, 0, kKoMarkerWidthPx - (kKoMarkerHeightPx + 1), kKoMarkerHeightPx),
                MyGUI::Align::Default,
                "Top",
                name.str() + "_fallback");
        }
        if (!bountyGlow && !icon && !bountySymbol && !fallbackText)
        {
            return false;
        }
        if (!icon && fallbackText)
        {
            vs_log::LogWarn(pluginName, "ImageBox marker unavailable; using text fallback");
        }

        if (icon)
        {
            icon->setNeedMouseFocus(false);
            icon->setImageTexture("default_icon.png");
            icon->setVisible(false);
        }

        if (bountyGlow)
        {
            bountyGlow->setNeedMouseFocus(false);
            bountyGlow->setImageTexture("default_icon.png");
            bountyGlow->setVisible(false);
        }

        if (bountySymbol)
        {
            bountySymbol->setNeedMouseFocus(false);
            bountySymbol->setCaption(state.config.bountySymbolText.c_str());
            bountySymbol->setTextAlign(MyGUI::Align::Center);
            bountySymbol->setTextColour(state.config.bountyTierNotableColour);
            bountySymbol->setTextShadow(true);
            bountySymbol->setVisible(false);
        }

        if (fallbackText)
        {
            fallbackText->setNeedMouseFocus(false);
            fallbackText->setCaption("ZZ");
            fallbackText->setTextAlign(MyGUI::Align::Left);
            fallbackText->setTextColour(MyGUI::Colour(1.0f, 0.2f, 0.2f, 1.0f));
            fallbackText->setTextShadow(true);
            fallbackText->setVisible(false);
        }

        KoMarkerWidget marker = { bountyGlow, icon, bountySymbol, fallbackText };

        if (index >= state.koMarkerWidgets.size())
        {
            state.koMarkerWidgets.push_back(marker);
        }
        else
        {
            state.koMarkerWidgets[index] = marker;
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool EnsureKoMarkerPool(RuntimeStateView& state, size_t requiredCount, const char* pluginName)
{
    if (requiredCount > kMaxKoMarkerWidgets)
    {
        requiredCount = kMaxKoMarkerWidgets;
    }

    while (state.koMarkerWidgets.size() < requiredCount)
    {
        if (!CreateKoMarkerWidgetAt(state, state.koMarkerWidgets.size(), pluginName))
        {
            return false;
        }
    }

    for (size_t i = 0; i < requiredCount; ++i)
    {
        if (!state.koMarkerWidgets[i].bountyGlow &&
            !state.koMarkerWidgets[i].icon &&
            !state.koMarkerWidgets[i].bountySymbol &&
            !state.koMarkerWidgets[i].fallbackText &&
            !CreateKoMarkerWidgetAt(state, i, pluginName))
        {
            return false;
        }
    }

    return true;
}

int ClampInt(int value, int minValue, int maxValue)
{
    if (value < minValue)
    {
        return minValue;
    }
    if (value > maxValue)
    {
        return maxValue;
    }
    return value;
}

bool TryGetViewSize(int* widthOut, int* heightOut)
{
    if (!widthOut || !heightOut)
    {
        return false;
    }

    MyGUI::RenderManager* renderManager = MyGUI::RenderManager::getInstancePtr();
    if (!renderManager)
    {
        return false;
    }

    const MyGUI::IntSize& viewSize = renderManager->getViewSize();
    if (viewSize.width <= 0 || viewSize.height <= 0)
    {
        return false;
    }

    *widthOut = viewSize.width;
    *heightOut = viewSize.height;
    return true;
}

bool ConvertProjectionToPixels(float rawX, float rawY, int viewWidth, int viewHeight, float* pixelXOut, float* pixelYOut, bool* normalizedUsedOut)
{
    if (!pixelXOut || !pixelYOut || viewWidth <= 0 || viewHeight <= 0)
    {
        return false;
    }

    if (normalizedUsedOut)
    {
        *normalizedUsedOut = false;
    }

    if (rawX >= 0.0f && rawX <= 1.0f && rawY >= 0.0f && rawY <= 1.0f)
    {
        *pixelXOut = rawX * static_cast<float>(viewWidth);
        *pixelYOut = rawY * static_cast<float>(viewHeight);
        if (normalizedUsedOut)
        {
            *normalizedUsedOut = true;
        }
        return true;
    }

    if (rawX >= -1.0f && rawX <= 1.0f && rawY >= -1.0f && rawY <= 1.0f)
    {
        *pixelXOut = (rawX * 0.5f + 0.5f) * static_cast<float>(viewWidth);
        *pixelYOut = (rawY * 0.5f + 0.5f) * static_cast<float>(viewHeight);
        if (normalizedUsedOut)
        {
            *normalizedUsedOut = true;
        }
        return true;
    }

    *pixelXOut = rawX;
    *pixelYOut = rawY;
    return true;
}

bool TryProjectWorldToScreenPx(RuntimeStateView& state, const Ogre::Vector3& worldPos, float* xOut, float* yOut)
{
    if (!xOut || !yOut)
    {
        return false;
    }

    if (!EnsureProjectionUtility(state))
    {
        return false;
    }

    float x = 0.0f;
    float y = 0.0f;
    bool projected = false;
    __try
    {
        projected = state.projectionUtility->worldToScreenPX(worldPos, x, y);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        state.projectionUtility = 0;
        return false;
    }

    if (!projected)
    {
        return false;
    }

    *xOut = x;
    *yOut = y;
    return true;
}
} // namespace

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

void HideAllKoMarkerWidgets(RuntimeStateView& state, const char* pluginName)
{
    (void)pluginName;
    HideAllKoMarkerWidgetsInternal(state);
}

void TickKoMarkerRender(RuntimeStateView& state, const char* pluginName)
{
    if (!state.config.enabled)
    {
        HideAllKoMarkerWidgetsInternal(state);
        return;
    }

    if (!state.config.showMarkerIcons && !state.config.showMarkerText && !state.config.showBountySymbol)
    {
        HideAllKoMarkerWidgetsInternal(state);
        return;
    }

    size_t renderableTargetCount = state.koTargetCache.size();
    if (renderableTargetCount > kMaxKoMarkerWidgets)
    {
        renderableTargetCount = kMaxKoMarkerWidgets;
    }

    if (renderableTargetCount == 0)
    {
        HideAllKoMarkerWidgetsInternal(state);
        return;
    }

    if (!EnsureKoMarkerPool(state, renderableTargetCount, pluginName))
    {
        HideAllKoMarkerWidgetsInternal(state);
        return;
    }

    int viewWidth = 0;
    int viewHeight = 0;
    const bool hasViewSize = TryGetViewSize(&viewWidth, &viewHeight);

    size_t visibleMarkerCount = 0;
    for (size_t i = 0; i < renderableTargetCount; ++i)
    {
        const CachedKoTarget& cached = state.koTargetCache[i];
        const bool isBountyOnlyState = IsBountyOnlyMarkerState(cached.markerState);

        Ogre::Vector3 characterWorldPos = cached.worldPos;
        Character* targetCharacter = cached.targetHandle.getCharacter();
        if (targetCharacter && targetCharacter->isValid())
        {
            characterWorldPos = targetCharacter->getPosition();
        }

        Ogre::Vector3 markerAnchor = characterWorldPos;
        const float liveAnchorYOffsetWorld = isBountyOnlyState
            ? (static_cast<float>(state.config.bountySymbolLiveAnchorYOffsetCm) * kKoCentimetersToWorldUnits)
            : 0.0f;
        const float markerAnchorYOffsetWorld = kKoMarkerHeadAnchorYOffset + liveAnchorYOffsetWorld;
        markerAnchor.y += markerAnchorYOffsetWorld;

        float screenX = 0.0f;
        float screenY = 0.0f;
        if (!TryProjectWorldToScreenPx(state, markerAnchor, &screenX, &screenY))
        {
            continue;
        }

        float pixelX = screenX;
        float pixelY = screenY;
        if (hasViewSize)
        {
            if (!ConvertProjectionToPixels(screenX, screenY, viewWidth, viewHeight, &pixelX, &pixelY, 0))
            {
                continue;
            }
        }

        if (visibleMarkerCount >= state.koMarkerWidgets.size())
        {
            break;
        }

        int markerLeft = static_cast<int>(pixelX) - (kKoMarkerWidthPx / 2);
        int markerTop = static_cast<int>(pixelY) - kKoMarkerYOffsetPx;
        if (hasViewSize)
        {
            const int maxLeft = (viewWidth > kKoMarkerWidthPx) ? (viewWidth - kKoMarkerWidthPx) : 0;
            const int maxTop = (viewHeight > kKoMarkerHeightPx) ? (viewHeight - kKoMarkerHeightPx) : 0;
            markerLeft = ClampInt(markerLeft, 0, maxLeft);
            markerTop = ClampInt(markerTop, 0, maxTop);
        }

        KoMarkerWidget& marker = state.koMarkerWidgets[visibleMarkerCount];
        const bool showStateIcon = state.config.showMarkerIcons && !isBountyOnlyState;
        const bool showStateText = state.config.showMarkerText && !isBountyOnlyState;
        const bool showBountySymbol = state.config.showBountySymbol && cached.totalBounty > 0;
        const bool showBountyGlow = state.config.showBountyGlow
            && cached.totalBounty > 0
            && !isBountyOnlyState
            && cached.markerRelation != CachedKoTarget::RELATION_SQUAD;
        ApplyKoMarkerVisualState(state, pluginName, marker, cached.markerState, cached.markerRelation, cached.totalBounty, showBountyGlow, showBountySymbol);
        SetKoMarkerPosition(state, marker, markerLeft, markerTop, cached.totalBounty, showStateIcon, showStateText, showBountySymbol, isBountyOnlyState);
        SetKoMarkerVisible(state, marker, true, showBountyGlow, showStateIcon, showStateText, showBountySymbol);

        ++visibleMarkerCount;
    }

    for (size_t i = visibleMarkerCount; i < state.koMarkerWidgets.size(); ++i)
    {
        SetKoMarkerVisible(state, state.koMarkerWidgets[i], false, false, false, false, false);
    }
}

} // namespace vs_marker_render
