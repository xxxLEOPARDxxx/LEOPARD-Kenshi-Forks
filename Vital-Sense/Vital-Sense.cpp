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
#include "src/vs_runtime_state.h"
#include "src/vs_log.h"
#include "src/vs_parse.h"
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
const float kProbablyDyingBloodMax = 50.0f;
const bool kEnableTextureInfoLogs = false;

void LogInfo(const std::string& message);
void LogWarn(const std::string& message);
bool IsMarkerStateEnabled(int markerState);
void LogIconTextureOnce(std::vector<std::string>& sink, const std::string& message, const char* textureName, bool warn);
int ResolveBountySymbolFontHeightPx(int totalBounty);
int ResolveBountySymbolWidthPx(int totalBounty);

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
        const hand& value = handles[i];
        if (HandlesEqualByKey(value, targetHandle))
        {
            return true;
        }
    }
    return false;
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

void LogIconTextureOnce(std::vector<std::string>& sink, const std::string& message, const char* textureName, bool warn)
{
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
        LogWarn(ss.str());
    }
    else
    {
        LogInfo(ss.str());
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

bool SetKoMarkerIconTexture(MyGUI::ImageBox* marker, const char* texture)
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
        addCandidateWithSeparatorVariants(std::string("mods/") + kPluginName + "/icons/" + requestedTexture);
        addCandidateWithSeparatorVariants(std::string("mods/") + kPluginName + "/gui/gfx/" + requestedTexture);
        addCandidateWithSeparatorVariants(std::string("mods/") + kPluginName + "/" + requestedTexture);
    }
    else if (!isAbsolutePath && !isModQualified)
    {
        addCandidateWithSeparatorVariants(std::string("mods/") + kPluginName + "/" + requestedTexture);

        const size_t fileNameStart = requestedTexture.find_last_of("/\\");
        if (fileNameStart != std::string::npos && (fileNameStart + 1) < requestedTexture.size())
        {
            const std::string fileName = requestedTexture.substr(fileNameStart + 1);
            addCandidateWithSeparatorVariants(fileName);
            addCandidateWithSeparatorVariants(std::string("icons/") + fileName);
            addCandidateWithSeparatorVariants(std::string("gui/gfx/") + fileName);
            addCandidateWithSeparatorVariants(std::string("mods/") + kPluginName + "/icons/" + fileName);
            addCandidateWithSeparatorVariants(std::string("mods/") + kPluginName + "/gui/gfx/" + fileName);
            addCandidateWithSeparatorVariants(std::string("mods/") + kPluginName + "/" + fileName);
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
                    LogIconTextureOnce(g_iconTextureWarnLogs, ss.str(), candidate.c_str(), true);
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
                LogIconTextureOnce(g_iconTextureOkLogs, "icon texture ready", candidate.c_str(), false);
                if (candidate != texture)
                {
                    LogIconTextureOnce(g_iconTextureOkLogs, "icon texture resolved alias", texture, false);
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
        LogIconTextureOnce(g_iconTextureWarnLogs, "icon texture fallback engaged", texture, true);
    }
    catch (...)
    {
        LogIconTextureOnce(g_iconTextureWarnLogs, "icon fallback texture apply failed", fallbackTexture, true);
        return false;
    }

    try
    {
        const MyGUI::IntSize fallbackSize = marker->getImageSize();
        if (fallbackSize.width > 0 && fallbackSize.height > 0)
        {
            LogIconTextureOnce(g_iconTextureOkLogs, "icon fallback ready", fallbackTexture, false);
        }
        else
        {
            LogIconTextureOnce(g_iconTextureWarnLogs, "icon fallback has zero size", fallbackTexture, true);
        }
    }
    catch (...)
    {
        LogIconTextureOnce(g_iconTextureWarnLogs, "icon fallback size probe failed", fallbackTexture, true);
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

void SetKoMarkerPosition(
    KoMarkerWidget& marker,
    int left,
    int top,
    int totalBounty,
    bool showStateIcon,
    bool showStateText,
    bool showBountySymbol,
    bool isBountyOnlyState)
{
    const bool placeSymbolBeforeIcon = showBountySymbol && showStateIcon && g_config.placeBountySymbolBeforeStateIcon;
    const int symbolWidth = showBountySymbol ? ResolveBountySymbolWidthPx(totalBounty) : kKoBountySymbolMinWidthPx;
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

void SetKoMarkerVisible(KoMarkerWidget& marker, bool visible, bool showBountyGlow, bool showStateIcon, bool showStateText, bool showBountySymbol)
{
    SetWidgetVisible(marker.bountyGlow, visible && g_config.showMarkerIcons && showBountyGlow);
    SetWidgetVisible(marker.icon, visible && showStateIcon);
    SetWidgetVisible(marker.bountySymbol, visible && showBountySymbol);
    SetWidgetVisible(marker.fallbackText, visible && showStateText);
}

const char* ResolveMarkerCaption(int markerState)
{
    if (markerState == CachedKoTarget::STATE_BOUNTY_ONLY)
    {
        return "";
    }
    if (markerState == CachedKoTarget::STATE_DEAD)
    {
        return g_config.deadText.empty() ? "DE" : g_config.deadText.c_str();
    }
    if (markerState == CachedKoTarget::STATE_RECOVERY_COMA)
    {
        return g_config.recoveryComaText.empty() ? "RC" : g_config.recoveryComaText.c_str();
    }
    if (markerState == CachedKoTarget::STATE_DYING)
    {
        return g_config.dyingText.empty() ? "DY" : g_config.dyingText.c_str();
    }
    if (markerState == CachedKoTarget::STATE_PLAYING_DEAD)
    {
        return g_config.playingDeadText.empty() ? "PD" : g_config.playingDeadText.c_str();
    }
    return g_config.unconsciousText.empty() ? "ZZ" : g_config.unconsciousText.c_str();
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
        // pic_PointerInvalid from data/gui/images/kenshi_images.xml.
        return MyGUI::IntCoord(108, 49, 34, 34);
    }
    if (markerState == CachedKoTarget::STATE_DYING)
    {
        // pic_PointerInvalid from data/gui/images/kenshi_images.xml.
        // This has a stronger silhouette than map markers against sandy backgrounds.
        return MyGUI::IntCoord(108, 49, 34, 34);
    }
    if (markerState == CachedKoTarget::STATE_RECOVERY_COMA)
    {
        // pic_PointerKnockout (32x32 @ 45,122).
        return MyGUI::IntCoord(45, 122, 32, 32);
    }
    if (markerState == CachedKoTarget::STATE_PLAYING_DEAD)
    {
        // Kenshi_CharacterNameTags::Stealth (38x27 @ 80,126).
        return MyGUI::IntCoord(80, 126, 38, 27);
    }

    // pic_PointerKnockout (32x32 @ 45,122).
    return MyGUI::IntCoord(45, 122, 32, 32);
}

MyGUI::Colour ResolveMarkerColour(int markerState, int markerRelation)
{
    (void)markerState;
    if (markerRelation == CachedKoTarget::RELATION_ENEMY)
    {
        return g_config.enemyMarkerColour;
    }

    if (markerRelation == CachedKoTarget::RELATION_SQUAD)
    {
        return g_config.squadMarkerColour;
    }

    return g_config.allyMarkerColour;
}

bool IsBountyOnlyMarkerState(int markerState)
{
    return markerState == CachedKoTarget::STATE_BOUNTY_ONLY;
}

int ResolveBountyTierIndex(int totalBounty)
{
    if (totalBounty <= 0)
    {
        return -1;
    }

    const DWORD bounty = static_cast<DWORD>(totalBounty);
    if (bounty <= g_config.bountyTierTrivialMax)
    {
        return 0;
    }
    if (bounty <= g_config.bountyTierLowMax)
    {
        return 1;
    }
    if (bounty <= g_config.bountyTierModestMax)
    {
        return 2;
    }
    if (bounty <= g_config.bountyTierNotableMax)
    {
        return 3;
    }
    if (bounty <= g_config.bountyTierHighValueMax)
    {
        return 4;
    }
    if (bounty <= g_config.bountyTierEliteMax)
    {
        return 5;
    }
    return 6;
}

MyGUI::Colour ResolveBountyTierColour(int totalBounty)
{
    const int tier = ResolveBountyTierIndex(totalBounty);
    if (tier < 0)
    {
        return MyGUI::Colour(0.0f, 0.0f, 0.0f, 0.0f);
    }

    if (tier == 0)
    {
        return g_config.bountyTierTrivialColour;
    }
    if (tier == 1)
    {
        return g_config.bountyTierLowColour;
    }
    if (tier == 2)
    {
        return g_config.bountyTierModestColour;
    }
    if (tier == 3)
    {
        return g_config.bountyTierNotableColour;
    }
    if (tier == 4)
    {
        return g_config.bountyTierHighValueColour;
    }
    if (tier == 5)
    {
        return g_config.bountyTierEliteColour;
    }
    return g_config.bountyTierLegendaryColour;
}

int ResolveBountySymbolFontHeightPx(int totalBounty)
{
    int fontHeight = static_cast<int>(g_config.bountySymbolTextSizePx);
    if (g_config.bountySymbolText == "$")
    {
        // '$' is visually narrow in Kenshi fonts; bump slightly for readability.
        fontHeight += 4;
    }

    const int tier = ResolveBountyTierIndex(totalBounty);
    if (tier >= 0)
    {
        // Escalate prominence with bounty value.
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

int ResolveBountySymbolWidthPx(int totalBounty)
{
    int width = (ResolveBountySymbolFontHeightPx(totalBounty) * 3) / 4 + 6;
    if (g_config.bountySymbolText == "$" && width < 20)
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

const std::string* ResolveCustomMarkerIconTexture(int markerState)
{
    if (markerState == CachedKoTarget::STATE_DEAD)
    {
        return &g_config.customDeadIconTexture;
    }
    if (markerState == CachedKoTarget::STATE_RECOVERY_COMA)
    {
        return &g_config.customRecoveryComaIconTexture;
    }
    if (markerState == CachedKoTarget::STATE_DYING)
    {
        return &g_config.customDyingIconTexture;
    }
    if (markerState == CachedKoTarget::STATE_PLAYING_DEAD)
    {
        return &g_config.customPlayingDeadIconTexture;
    }
    return &g_config.customUnconsciousIconTexture;
}

DWORD ResolveCustomMarkerIconSizePx(int markerState)
{
    if (markerState == CachedKoTarget::STATE_DEAD)
    {
        return g_config.customDeadIconSizePx;
    }
    if (markerState == CachedKoTarget::STATE_RECOVERY_COMA)
    {
        return g_config.customRecoveryComaIconSizePx;
    }
    if (markerState == CachedKoTarget::STATE_DYING)
    {
        return g_config.customDyingIconSizePx;
    }
    if (markerState == CachedKoTarget::STATE_PLAYING_DEAD)
    {
        return g_config.customPlayingDeadIconSizePx;
    }
    return g_config.customUnconsciousIconSizePx;
}

DWORD ResolveMarkerTextSizePx(int markerState)
{
    if (markerState == CachedKoTarget::STATE_DEAD)
    {
        return g_config.deadTextSizePx;
    }
    if (markerState == CachedKoTarget::STATE_RECOVERY_COMA)
    {
        return g_config.recoveryComaTextSizePx;
    }
    if (markerState == CachedKoTarget::STATE_DYING)
    {
        return g_config.dyingTextSizePx;
    }
    if (markerState == CachedKoTarget::STATE_PLAYING_DEAD)
    {
        return g_config.playingDeadTextSizePx;
    }
    return g_config.unconsciousTextSizePx;
}

bool HasCustomMarkerIcon(int markerState)
{
    const std::string* texture = ResolveCustomMarkerIconTexture(markerState);
    return texture && !texture->empty();
}

void ApplyKoMarkerVisualState(KoMarkerWidget& marker, int markerState, int markerRelation, int totalBounty, bool showBountyGlow, bool showBountySymbol)
{
    const bool isBountyOnlyState = IsBountyOnlyMarkerState(markerState);
    const MyGUI::Colour colour = ResolveMarkerColour(markerState, markerRelation);
    const bool wantsCustomIcon = !isBountyOnlyState && HasCustomMarkerIcon(markerState);
    const std::string* customTexture = ResolveCustomMarkerIconTexture(markerState);
    const int customIconSize = static_cast<int>(ResolveCustomMarkerIconSizePx(markerState));

    if (marker.icon && !isBountyOnlyState)
    {
        if (g_config.showMarkerIcons)
        {
            bool customIconReady = false;
            if (wantsCustomIcon && customTexture)
            {
                customIconReady = SetKoMarkerIconTexture(marker.icon, customTexture->c_str());
                if (customIconReady)
                {
                    // Preserve custom icon authoring colors (no relation tint).
                    SetKoMarkerIconCoord(marker.icon, ResolveCustomIconCoordFromImageSize(marker.icon, customIconSize));
                    SetKoMarkerIconColour(marker.icon, MyGUI::Colour(1.0f, 1.0f, 1.0f, 1.0f));
                }
                else
                {
                    SetKoMarkerIconTexture(marker.icon, ResolveMarkerIconTexture(markerState, markerRelation));
                    SetKoMarkerIconCoord(marker.icon, ResolveMarkerIconCoord(markerState, markerRelation));
                    SetKoMarkerIconColour(marker.icon, colour);
                }
            }
            else
            {
                SetKoMarkerIconTexture(marker.icon, ResolveMarkerIconTexture(markerState, markerRelation));
                SetKoMarkerIconCoord(marker.icon, ResolveMarkerIconCoord(markerState, markerRelation));
                SetKoMarkerIconColour(marker.icon, colour);
            }
        }
    }

    if (marker.bountyGlow && g_config.showMarkerIcons && showBountyGlow)
    {
        const bool glowTextureReady = SetKoMarkerIconTexture(marker.bountyGlow, kKoBountyGlowTexture);
        if (glowTextureReady)
        {
            SetKoMarkerIconCoord(marker.bountyGlow, ResolveCustomIconCoordFromImageSize(marker.bountyGlow, kKoBountyGlowTextureSizePx));
            SetKoMarkerIconColour(marker.bountyGlow, kKoBountyGlowColour);
        }
        else
        {
            // Never fallback to state icons for bounty glow; that can look like a red outline.
            SetKoMarkerIconCoord(marker.bountyGlow, MyGUI::IntCoord(0, 0, 0, 0));
            SetKoMarkerIconColour(marker.bountyGlow, MyGUI::Colour(1.0f, 1.0f, 1.0f, 0.0f));
        }
    }

    if (marker.fallbackText && g_config.showMarkerText)
    {
        if (isBountyOnlyState)
        {
            SetKoMarkerCaption(marker.fallbackText, "");
        }
        else
        {
            const int fontHeight = static_cast<int>(ResolveMarkerTextSizePx(markerState));
            SetKoMarkerTextFontHeight(marker.fallbackText, fontHeight);
            SetKoMarkerCaption(marker.fallbackText, ResolveMarkerCaption(markerState));
            SetKoMarkerTextColour(marker.fallbackText, colour);
        }
    }

    if (marker.bountySymbol)
    {
        if (showBountySymbol)
        {
            const int symbolFontHeight = ResolveBountySymbolFontHeightPx(totalBounty);
            const MyGUI::Colour bountyColour = ResolveBountyTierColour(totalBounty);
            SetKoMarkerTextFontHeight(marker.bountySymbol, symbolFontHeight);
            SetKoMarkerCaption(marker.bountySymbol, g_config.bountySymbolText.c_str());
            SetKoMarkerTextColour(marker.bountySymbol, bountyColour);
        }
        else
        {
            SetKoMarkerCaption(marker.bountySymbol, "");
        }
    }
}

void HideAllKoMarkerWidgets()
{
    for (size_t i = 0; i < g_koMarkerWidgets.size(); ++i)
    {
        SetKoMarkerVisible(g_koMarkerWidgets[i], false, false, false, false, false);
    }
}

bool CreateKoMarkerWidgetAt(size_t index)
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (!gui)
    {
        return false;
    }

    try
    {
        std::stringstream name;
        name << "VS_KOMarker_" << index << "_" << g_koMarkerWidgetSerial++;

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
            LogWarn("ImageBox marker unavailable; using text fallback");
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
            bountySymbol->setCaption(g_config.bountySymbolText.c_str());
            bountySymbol->setTextAlign(MyGUI::Align::Center);
            bountySymbol->setTextColour(g_config.bountyTierNotableColour);
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

        if (index >= g_koMarkerWidgets.size())
        {
            g_koMarkerWidgets.push_back(marker);
        }
        else
        {
            g_koMarkerWidgets[index] = marker;
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool EnsureKoMarkerPool(size_t requiredCount)
{
    if (requiredCount > kMaxKoMarkerWidgets)
    {
        requiredCount = kMaxKoMarkerWidgets;
    }

    while (g_koMarkerWidgets.size() < requiredCount)
    {
        if (!CreateKoMarkerWidgetAt(g_koMarkerWidgets.size()))
        {
            return false;
        }
    }

    for (size_t i = 0; i < requiredCount; ++i)
    {
        if (!g_koMarkerWidgets[i].bountyGlow &&
            !g_koMarkerWidgets[i].icon &&
            !g_koMarkerWidgets[i].bountySymbol &&
            !g_koMarkerWidgets[i].fallbackText &&
            !CreateKoMarkerWidgetAt(i))
        {
            return false;
        }
    }

    return true;
}

bool EnsureProjectionUtility()
{
    if (g_projectionUtility)
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

    g_projectionUtility = reinterpret_cast<UtilityT*>(baseAddress + utilityOffset);
    return g_projectionUtility != 0;
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

bool TryProjectWorldToScreenPx(const Ogre::Vector3& worldPos, float* xOut, float* yOut)
{
    if (!xOut || !yOut)
    {
        return false;
    }

    if (!EnsureProjectionUtility())
    {
        return false;
    }

    float x = 0.0f;
    float y = 0.0f;
    bool projected = false;
    __try
    {
        projected = g_projectionUtility->worldToScreenPX(worldPos, x, y);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_projectionUtility = 0;
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

void TickKoMarkerRender()
{
    if (!g_config.enabled)
    {
        HideAllKoMarkerWidgets();
        return;
    }

    if (!g_config.showMarkerIcons && !g_config.showMarkerText && !g_config.showBountySymbol)
    {
        HideAllKoMarkerWidgets();
        return;
    }

    size_t renderableTargetCount = g_koTargetCache.size();
    if (renderableTargetCount > kMaxKoMarkerWidgets)
    {
        renderableTargetCount = kMaxKoMarkerWidgets;
    }

    if (renderableTargetCount == 0)
    {
        HideAllKoMarkerWidgets();
        return;
    }

    if (!EnsureKoMarkerPool(renderableTargetCount))
    {
        HideAllKoMarkerWidgets();
        return;
    }

    int viewWidth = 0;
    int viewHeight = 0;
    const bool hasViewSize = TryGetViewSize(&viewWidth, &viewHeight);

    size_t visibleMarkerCount = 0;
    for (size_t i = 0; i < renderableTargetCount; ++i)
    {
        const CachedKoTarget& cached = g_koTargetCache[i];
        const bool isBountyOnlyState = IsBountyOnlyMarkerState(cached.markerState);

        Ogre::Vector3 characterWorldPos = cached.worldPos;
        Character* targetCharacter = cached.targetHandle.getCharacter();
        if (targetCharacter && targetCharacter->isValid())
        {
            characterWorldPos = targetCharacter->getPosition();
        }

        Ogre::Vector3 markerAnchor = characterWorldPos;
        const float liveAnchorYOffsetWorld = isBountyOnlyState
            ? (static_cast<float>(g_config.bountySymbolLiveAnchorYOffsetCm) * kKoCentimetersToWorldUnits)
            : 0.0f;
        const float markerAnchorYOffsetWorld = kKoMarkerHeadAnchorYOffset + liveAnchorYOffsetWorld;
        markerAnchor.y += markerAnchorYOffsetWorld;

        float screenX = 0.0f;
        float screenY = 0.0f;
        if (!TryProjectWorldToScreenPx(markerAnchor, &screenX, &screenY))
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

        if (visibleMarkerCount >= g_koMarkerWidgets.size())
        {
            break;
        }

        const int markerYOffsetPx = kKoMarkerYOffsetPx;
        int markerLeft = static_cast<int>(pixelX) - (kKoMarkerWidthPx / 2);
        int markerTop = static_cast<int>(pixelY) - markerYOffsetPx;
        const int markerLeftPreClamp = markerLeft;
        const int markerTopPreClamp = markerTop;
        if (hasViewSize)
        {
            const int maxLeft = (viewWidth > kKoMarkerWidthPx) ? (viewWidth - kKoMarkerWidthPx) : 0;
            const int maxTop = (viewHeight > kKoMarkerHeightPx) ? (viewHeight - kKoMarkerHeightPx) : 0;
            const int clampedLeft = ClampInt(markerLeft, 0, maxLeft);
            const int clampedTop = ClampInt(markerTop, 0, maxTop);
            markerLeft = clampedLeft;
            markerTop = clampedTop;
        }

        KoMarkerWidget& marker = g_koMarkerWidgets[visibleMarkerCount];
        const bool showStateIcon = g_config.showMarkerIcons && !isBountyOnlyState;
        const bool showStateText = g_config.showMarkerText && !isBountyOnlyState;
        const bool showBountySymbol = g_config.showBountySymbol && cached.totalBounty > 0;
        const bool showBountyGlow = g_config.showBountyGlow
            && cached.totalBounty > 0
            && !isBountyOnlyState
            && cached.markerRelation != CachedKoTarget::RELATION_SQUAD;
        ApplyKoMarkerVisualState(marker, cached.markerState, cached.markerRelation, cached.totalBounty, showBountyGlow, showBountySymbol);
        SetKoMarkerPosition(marker, markerLeft, markerTop, cached.totalBounty, showStateIcon, showStateText, showBountySymbol, isBountyOnlyState);
        SetKoMarkerVisible(marker, true, showBountyGlow, showStateIcon, showStateText, showBountySymbol);

        ++visibleMarkerCount;
    }

    for (size_t i = visibleMarkerCount; i < g_koMarkerWidgets.size(); ++i)
    {
        SetKoMarkerVisible(g_koMarkerWidgets[i], false, false, false, false, false);
    }
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
