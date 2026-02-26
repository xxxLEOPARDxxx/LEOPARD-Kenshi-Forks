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

#ifndef BOOST_ALL_NO_LIB
#define BOOST_ALL_NO_LIB
#endif

#ifndef BOOST_ERROR_CODE_HEADER_ONLY
#define BOOST_ERROR_CODE_HEADER_ONLY
#endif

#include "../_deps/KenshiExtensionPlugin/KenshiExtensionPlugin/include/extern/DatapanelGUI.h"
#include "../_deps/KenshiExtensionPlugin/KenshiExtensionPlugin/include/extern/ForgottenGUI.h"
#include "../_deps/KenshiExtensionPlugin/KenshiExtensionPlugin/include/extern/StateBroadcastData.h"

#include <Windows.h>

#include <cctype>
#include <cstring>
#include <fstream>
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

struct PluginConfig
{
    bool enabled;
    DWORD updateIntervalMs;
    bool onlyWhenAltHeld;
    DWORD maxHighlightDistanceMeters;
    std::string customDyingIconTexture;
    DWORD customDyingIconSizePx;
    bool showMarkerIcons;
    bool showMarkerText;
};

PluginConfig g_config = { true, 150, true, 3500, "", 64, true, true };
std::string g_settingsPath;
DWORD g_lastProbeTickMs = 0;
void (*PlayerInterface_updateUT_orig)(PlayerInterface*) = 0;

struct CachedKoTarget
{
    enum MarkerState
    {
        STATE_UNCONSCIOUS = 0,
        STATE_DYING = 1,
        STATE_PLAYING_DEAD = 2
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
};

struct MarkerStateDebugInfo
{
    bool isUnconscious;
    bool isPlayingDead;
    bool isLiteral;
    bool isProbablyDying;
    bool dyingByProbablyLiteral;
    bool dyingByProbablyLowBlood;
    bool dyingByProbablySub50Ko;
    bool dyingByActiveBleed;
    bool dyingByTrauma;
    bool dyingByBloodThreshold;
    bool medicalUnconsciousFlag;
    bool medicalSub50KoFlag;
    bool medicalBloodlossTraumaFlag;
    float currentBleedRate;
    int proneState;
    bool hasSleepState;
    int sleepState;
    bool hasSlaveState;
    int slaveState;
    float bloodLevel;
    float pointOfNoReturn;
};

std::vector<CachedKoTarget> g_koTargetCache;
std::vector<hand> g_visibleKoHandlesScratch;
struct KoMarkerWidget
{
    MyGUI::ImageBox* beacon;
    MyGUI::ImageBox* icon;
    MyGUI::TextBox* fallbackText;
};
std::vector<KoMarkerWidget> g_koMarkerWidgets;
std::vector<std::string> g_iconTextureOkLogs;
std::vector<std::string> g_iconTextureWarnLogs;
unsigned int g_markerDebugLogCount = 0;
const unsigned int kMarkerDebugLogMaxPerSession = 120;
bool g_loggedDyingByTrauma = false;
bool g_loggedDyingByBloodThreshold = false;
bool g_loggedDyingBySub50Ko = false;
bool g_loggedDyingByProbably = false;
bool g_loggedDyingByProbablyLiteral = false;
bool g_loggedDyingByProbablyLowBlood = false;
hand g_lastSelectedDebugHandle;
int g_lastSelectedDebugState = -1;
int g_lastSelectedDebugRelation = -1;
DWORD g_lastSelectedDebugTickMs = 0;
hand g_lastSelectedDeepDumpHandle;
DWORD g_lastSelectedDeepDumpTickMs = 0;
hand g_lastSelectedPanelDumpHandle;
DWORD g_lastSelectedPanelDumpTickMs = 0;
hand g_lastSelectionSnapshotCharacter;
hand g_lastSelectionSnapshotObject;
size_t g_lastSelectionSnapshotCount = 0;
DWORD g_lastSelectionSnapshotTickMs = 0;
DWORD g_lastSelectionNoMatchTickMs = 0;
DWORD g_lastHighlightGateOpenTickMs = 0;
UtilityT* g_projectionUtility = 0;
unsigned int g_koMarkerWidgetSerial = 0;
bool g_highlightRuntimeActive = false;

const size_t kMaxKoMarkerWidgets = 48;
const int kKoMarkerWidthPx = 64;
const int kKoMarkerHeightPx = 18;
const int kKoMarkerYOffsetPx = 24;
const float kKoMarkerHeadAnchorYOffset = 2.0f;
const int kKoBeaconSizePx = 34;
const float kKoBeaconAlpha = 0.80f;
const float kProbablyDyingBloodMax = 50.0f;
const DWORD kHighlightGateReleaseDebounceMs = 250;
const bool kEnableUnsafePanelProbe = false;
const bool kEnableUiBeaconOverlay = false;
const bool kEnableVerboseRuntimeLogs = false;
const bool kEnableTextureInfoLogs = false;

void LogInfo(const std::string& message);
void LogWarn(const std::string& message);

void LogWithPrefix(void (*sink)(const char*), const char* level, const std::string& message)
{
    if (!sink)
    {
        return;
    }

    std::stringstream line;
    line << kPluginName << " " << level << ": " << message;
    sink(line.str().c_str());
}

void LogInfo(const std::string& message)
{
    LogWithPrefix(&DebugLog, "INFO", message);
}

void LogWarn(const std::string& message)
{
    LogWithPrefix(&ErrorLog, "WARN", message);
}

void LogError(const std::string& message)
{
    LogWithPrefix(&ErrorLog, "ERROR", message);
}

void LogDyingDetection(const char* trigger, float bloodLevel, float pointOfNoReturn)
{
    if (!kEnableVerboseRuntimeLogs)
    {
        return;
    }
    std::stringstream info;
    info << "DY detected via " << trigger << " (blood=" << bloodLevel
         << ", point_of_no_return=" << pointOfNoReturn << ")";
    LogInfo(info.str());
}

const char* MarkerStateName(int markerState)
{
    if (markerState == CachedKoTarget::STATE_DYING)
    {
        return "DY";
    }
    if (markerState == CachedKoTarget::STATE_PLAYING_DEAD)
    {
        return "PD";
    }
    return "ZZ";
}

const char* MarkerRelationName(int markerRelation)
{
    if (markerRelation == CachedKoTarget::RELATION_SQUAD)
    {
        return "SQUAD";
    }
    if (markerRelation == CachedKoTarget::RELATION_ALLY)
    {
        return "ALLY";
    }
    return "ENEMY";
}

bool HandsEqualExact(const hand& a, const hand& b)
{
    return a.type == b.type &&
        a.container == b.container &&
        a.containerSerial == b.containerSerial &&
        a.index == b.index &&
        a.serial == b.serial;
}

bool HandsEqualWithFallback(const hand& a, const hand& b)
{
    if (HandsEqualExact(a, b))
    {
        return true;
    }
    if (a.isNull() || b.isNull())
    {
        return false;
    }
    return a.toString() == b.toString();
}

std::string ClipForLog(const std::string& value, size_t maxLen)
{
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
    {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }

    std::string trimmed = value.substr(start, end - start);
    if (trimmed.size() <= maxLen)
    {
        return trimmed;
    }

    if (maxLen < 4)
    {
        return trimmed.substr(0, maxLen);
    }
    return trimmed.substr(0, maxLen - 3) + "...";
}

std::string ToLowerAsciiCopy(const std::string& value)
{
    std::string lowered = value;
    for (size_t i = 0; i < lowered.size(); ++i)
    {
        lowered[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
    }
    return lowered;
}

bool ContainsStatusToken(const std::string& loweredText)
{
    return loweredText.find("state") != std::string::npos ||
        loweredText.find("status") != std::string::npos ||
        loweredText.find("dying") != std::string::npos ||
        loweredText.find("unconc") != std::string::npos ||
        loweredText.find("playing dead") != std::string::npos ||
        loweredText.find("ko") != std::string::npos;
}

bool TryGetKenshiGuiOffset(uintptr_t* offsetOut)
{
    if (!offsetOut)
    {
        return false;
    }

    *offsetOut = 0;
    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    if (platform == KenshiLib::BinaryVersion::STEAM)
    {
        if (version == "1.0.65")
        {
            *offsetOut = 0x02132750;
            return true;
        }
        if (version == "1.0.68")
        {
            *offsetOut = 0x021337b0;
            return true;
        }
        return false;
    }

    if (platform == KenshiLib::BinaryVersion::GOG)
    {
        if (version == "1.0.65")
        {
            *offsetOut = 0x021306c0;
            return true;
        }
        if (version == "1.0.68")
        {
            *offsetOut = 0x021326e0;
            return true;
        }
    }

    return false;
}

ForgottenGUI* ResolveKenshiGui()
{
    uintptr_t guiOffset = 0;
    if (!TryGetKenshiGuiOffset(&guiOffset))
    {
        return 0;
    }

    HMODULE exeHandle = GetModuleHandleA(0);
    if (!exeHandle)
    {
        return 0;
    }

    const uintptr_t baseAddress = reinterpret_cast<uintptr_t>(exeHandle);
    if (!baseAddress)
    {
        return 0;
    }

    return reinterpret_cast<ForgottenGUI*>(baseAddress + guiOffset);
}

hand GetPrimarySelectionHandle()
{
    if (!ou || !ou->player)
    {
        return hand();
    }

    if (!ou->player->selectedObject.isNull())
    {
        return ou->player->selectedObject;
    }

    return ou->player->selectedCharacter;
}

void LogMarkerStateDecision(const char* reason, const hand& targetHandle, int markerState, int markerRelation, const MarkerStateDebugInfo& debugInfo, bool isSelected)
{
    if (!kEnableVerboseRuntimeLogs)
    {
        return;
    }

    if (!reason)
    {
        return;
    }

    const bool isSelectedProbe = (std::strcmp(reason, "selected_probe") == 0);
    if (!isSelectedProbe && g_markerDebugLogCount >= kMarkerDebugLogMaxPerSession)
    {
        return;
    }
    if (!isSelectedProbe)
    {
        ++g_markerDebugLogCount;
    }

    std::stringstream info;
    info << "marker_state reason=" << reason
         << " handle=" << targetHandle.toString()
         << " state=" << MarkerStateName(markerState)
         << " relation=" << MarkerRelationName(markerRelation)
         << " selected=" << (isSelected ? "true" : "false")
         << " unconscious=" << (debugInfo.isUnconscious ? "true" : "false")
         << " playing_dead=" << (debugInfo.isPlayingDead ? "true" : "false")
         << " literal_ko=" << (debugInfo.isLiteral ? "true" : "false")
         << " probably_dying=" << (debugInfo.isProbablyDying ? "true" : "false")
         << " probably_literal=" << (debugInfo.dyingByProbablyLiteral ? "true" : "false")
         << " probably_low_blood=" << (debugInfo.dyingByProbablyLowBlood ? "true" : "false")
         << " probably_sub50_ko=" << (debugInfo.dyingByProbablySub50Ko ? "true" : "false")
         << " active_bleed_dying=" << (debugInfo.dyingByActiveBleed ? "true" : "false")
         << " bloodloss_trauma=" << (debugInfo.dyingByTrauma ? "true" : "false")
         << " blood_threshold=" << (debugInfo.dyingByBloodThreshold ? "true" : "false")
         << " medical_unconcious_flag=" << (debugInfo.medicalUnconsciousFlag ? "true" : "false")
         << " medical_sub50_ko_flag=" << (debugInfo.medicalSub50KoFlag ? "true" : "false")
         << " medical_bloodloss_trauma_flag=" << (debugInfo.medicalBloodlossTraumaFlag ? "true" : "false")
         << " bleed_rate=" << debugInfo.currentBleedRate
         << " prone_state=" << debugInfo.proneState
         << " has_sleep_state=" << (debugInfo.hasSleepState ? "true" : "false")
         << " sleep_state=" << debugInfo.sleepState
         << " has_slave_state=" << (debugInfo.hasSlaveState ? "true" : "false")
         << " slave_state=" << debugInfo.slaveState
         << " blood=" << debugInfo.bloodLevel
         << " point_of_no_return=" << debugInfo.pointOfNoReturn;
    LogInfo(info.str());
}

std::string TrimAscii(const std::string& value)
{
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
    {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }

    return value.substr(start, end - start);
}

void SkipWhitespace(const std::string& body, size_t* pos)
{
    if (!pos)
    {
        return;
    }

    while (*pos < body.size() && std::isspace(static_cast<unsigned char>(body[*pos])) != 0)
    {
        ++(*pos);
    }
}

bool ParseBoolFromJson(const std::string& body, const char* keyName, bool* valueOut)
{
    if (!keyName || !valueOut)
    {
        return false;
    }

    const std::string key = std::string("\"") + keyName + "\"";
    const size_t keyPos = body.find(key);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    size_t pos = keyPos + key.size();
    SkipWhitespace(body, &pos);
    if (pos >= body.size() || body[pos] != ':')
    {
        return false;
    }

    ++pos;
    SkipWhitespace(body, &pos);

    if (pos + 4 <= body.size() && body.compare(pos, 4, "true") == 0)
    {
        *valueOut = true;
        return true;
    }

    if (pos + 5 <= body.size() && body.compare(pos, 5, "false") == 0)
    {
        *valueOut = false;
        return true;
    }

    return false;
}

bool ParseUnsignedFromJson(const std::string& body, const char* keyName, DWORD* valueOut)
{
    if (!keyName || !valueOut)
    {
        return false;
    }

    const std::string key = std::string("\"") + keyName + "\"";
    const size_t keyPos = body.find(key);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    size_t pos = keyPos + key.size();
    SkipWhitespace(body, &pos);
    if (pos >= body.size() || body[pos] != ':')
    {
        return false;
    }

    ++pos;
    SkipWhitespace(body, &pos);
    if (pos >= body.size() || std::isdigit(static_cast<unsigned char>(body[pos])) == 0)
    {
        return false;
    }

    size_t end = pos;
    while (end < body.size() && std::isdigit(static_cast<unsigned char>(body[end])) != 0)
    {
        ++end;
    }

    DWORD parsed = 0;
    try
    {
        parsed = static_cast<DWORD>(std::stoul(body.substr(pos, end - pos)));
    }
    catch (...)
    {
        return false;
    }

    *valueOut = parsed;
    return true;
}

bool ParseStringFromJson(const std::string& body, const char* keyName, std::string* valueOut)
{
    if (!keyName || !valueOut)
    {
        return false;
    }

    const std::string key = std::string("\"") + keyName + "\"";
    const size_t keyPos = body.find(key);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    size_t pos = keyPos + key.size();
    SkipWhitespace(body, &pos);
    if (pos >= body.size() || body[pos] != ':')
    {
        return false;
    }

    ++pos;
    SkipWhitespace(body, &pos);
    if (pos >= body.size() || body[pos] != '"')
    {
        return false;
    }

    ++pos;
    std::string parsed;
    while (pos < body.size())
    {
        const char ch = body[pos];
        if (ch == '"')
        {
            *valueOut = parsed;
            return true;
        }
        if (ch == '\\')
        {
            ++pos;
            if (pos >= body.size())
            {
                return false;
            }
            const char esc = body[pos];
            if (esc == '"' || esc == '\\' || esc == '/')
            {
                parsed.push_back(esc);
            }
            else if (esc == 'b')
            {
                parsed.push_back('\b');
            }
            else if (esc == 'f')
            {
                parsed.push_back('\f');
            }
            else if (esc == 'n')
            {
                parsed.push_back('\n');
            }
            else if (esc == 'r')
            {
                parsed.push_back('\r');
            }
            else if (esc == 't')
            {
                parsed.push_back('\t');
            }
            else
            {
                return false;
            }
        }
        else
        {
            parsed.push_back(ch);
        }
        ++pos;
    }

    return false;
}

bool LoadConfigState()
{
    g_config.enabled = true;
    g_config.updateIntervalMs = 150;
    g_config.onlyWhenAltHeld = true;
    g_config.maxHighlightDistanceMeters = 3500;
    g_config.customDyingIconTexture.clear();
    g_config.customDyingIconSizePx = 64;
    g_config.showMarkerIcons = true;
    g_config.showMarkerText = true;

    if (g_settingsPath.empty())
    {
        LogWarn("settings path is empty; using defaults");
        return false;
    }

    std::ifstream in(g_settingsPath.c_str(), std::ios::in | std::ios::binary);
    if (!in)
    {
        LogWarn("mod-config.json not found; using defaults");
        return true;
    }

    const std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    bool parsedEnabled = true;
    if (!ParseBoolFromJson(body, "enabled", &parsedEnabled))
    {
        LogWarn("invalid/missing key \"enabled\"; using default");
        return true;
    }

    g_config.enabled = parsedEnabled;
    DWORD parsedInterval = 0;
    if (ParseUnsignedFromJson(body, "update_interval_ms", &parsedInterval))
    {
        if (parsedInterval < 50)
        {
            g_config.updateIntervalMs = 50;
            LogWarn("update_interval_ms too low; clamped to 50");
        }
        else if (parsedInterval > 2000)
        {
            g_config.updateIntervalMs = 2000;
            LogWarn("update_interval_ms too high; clamped to 2000");
        }
        else
        {
            g_config.updateIntervalMs = parsedInterval;
        }
    }

    DWORD parsedDistanceMeters = 0;
    if (ParseUnsignedFromJson(body, "max_highlight_distance_m", &parsedDistanceMeters))
    {
        if (parsedDistanceMeters < 5)
        {
            g_config.maxHighlightDistanceMeters = 5;
            LogWarn("max_highlight_distance_m too low; clamped to 5");
        }
        else if (parsedDistanceMeters > 20000)
        {
            g_config.maxHighlightDistanceMeters = 20000;
            LogWarn("max_highlight_distance_m too high; clamped to 20000");
        }
        else
        {
            g_config.maxHighlightDistanceMeters = parsedDistanceMeters;
        }
    }

    bool parsedAltGate = true;
    if (ParseBoolFromJson(body, "only_when_alt_held", &parsedAltGate))
    {
        g_config.onlyWhenAltHeld = parsedAltGate;
    }

    bool parsedShowIcons = true;
    if (ParseBoolFromJson(body, "show_icons", &parsedShowIcons))
    {
        g_config.showMarkerIcons = parsedShowIcons;
    }

    bool parsedShowText = true;
    if (ParseBoolFromJson(body, "show_text", &parsedShowText))
    {
        g_config.showMarkerText = parsedShowText;
    }

    std::string parsedDyingIconTexture;
    if (ParseStringFromJson(body, "dying_icon_texture", &parsedDyingIconTexture))
    {
        g_config.customDyingIconTexture = TrimAscii(parsedDyingIconTexture);
    }

    DWORD parsedDyingIconSize = 0;
    if (ParseUnsignedFromJson(body, "dying_icon_size_px", &parsedDyingIconSize))
    {
        if (parsedDyingIconSize < 8)
        {
            g_config.customDyingIconSizePx = 8;
            LogWarn("dying_icon_size_px too low; clamped to 8");
        }
        else if (parsedDyingIconSize > 512)
        {
            g_config.customDyingIconSizePx = 512;
            LogWarn("dying_icon_size_px too high; clamped to 512");
        }
        else
        {
            g_config.customDyingIconSizePx = parsedDyingIconSize;
        }
    }

    if (!g_config.customDyingIconTexture.empty())
    {
        std::stringstream iconInfo;
        iconInfo << "custom DY icon configured texture=" << g_config.customDyingIconTexture
            << " size=" << g_config.customDyingIconSizePx;
        LogInfo(iconInfo.str());
    }

    return true;
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
    textureCandidates.push_back(texture);
    const bool hasPathSeparator = (std::strchr(texture, '/') != 0 || std::strchr(texture, '\\') != 0);
    if (!hasPathSeparator)
    {
        textureCandidates.push_back(std::string("gui/gfx/") + texture);
        textureCandidates.push_back(std::string("mods/") + kPluginName + "/gui/gfx/" + texture);
        textureCandidates.push_back(std::string("mods/") + kPluginName + "/" + texture);
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
            LogIconTextureOnce(g_iconTextureWarnLogs, "icon texture apply threw exception", candidate.c_str(), true);
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
                LogIconTextureOnce(g_iconTextureWarnLogs, "icon texture applied with zero size", candidate.c_str(), true);
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
            LogIconTextureOnce(g_iconTextureWarnLogs, "icon texture size probe failed", candidate.c_str(), true);
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

void SetKoMarkerPosition(KoMarkerWidget& marker, int left, int top)
{
    if (marker.beacon)
    {
        try
        {
            const int centerX = left + (kKoMarkerWidthPx / 2);
            const int centerY = top + (kKoMarkerHeightPx / 2);
            marker.beacon->setCoord(
                centerX - (kKoBeaconSizePx / 2),
                centerY - (kKoBeaconSizePx / 2),
                kKoBeaconSizePx,
                kKoBeaconSizePx);
        }
        catch (...)
        {
        }
    }

    if (marker.icon)
    {
        try
        {
            marker.icon->setCoord(left, top, kKoMarkerHeightPx, kKoMarkerHeightPx);
        }
        catch (...)
        {
        }
    }

    if (marker.fallbackText)
    {
        try
        {
            if (marker.icon && g_config.showMarkerIcons)
            {
                marker.fallbackText->setCoord(left + kKoMarkerHeightPx + 1, top, kKoMarkerWidthPx - (kKoMarkerHeightPx + 1), kKoMarkerHeightPx);
            }
            else
            {
                marker.fallbackText->setCoord(left, top, kKoMarkerWidthPx, kKoMarkerHeightPx);
            }
        }
        catch (...)
        {
        }
    }
}

void SetKoMarkerVisible(KoMarkerWidget& marker, bool visible)
{
    SetWidgetVisible(marker.beacon, visible && kEnableUiBeaconOverlay);
    SetWidgetVisible(marker.icon, visible && g_config.showMarkerIcons);
    SetWidgetVisible(marker.fallbackText, visible && g_config.showMarkerText);
}

const char* ResolveMarkerCaption(int markerState)
{
    if (markerState == CachedKoTarget::STATE_DYING)
    {
        return "DY";
    }
    if (markerState == CachedKoTarget::STATE_PLAYING_DEAD)
    {
        return "PD";
    }
    return "ZZ";
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
    if (markerState == CachedKoTarget::STATE_DYING)
    {
        // pic_PointerInvalid from data/gui/images/kenshi_images.xml.
        // This has a stronger silhouette than map markers against sandy backgrounds.
        return MyGUI::IntCoord(108, 49, 34, 34);
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
        return MyGUI::Colour(1.0f, 0.2f, 0.2f, 1.0f);
    }

    if (markerRelation == CachedKoTarget::RELATION_SQUAD)
    {
        return MyGUI::Colour(0.25f, 1.0f, 0.25f, 1.0f);
    }

    return MyGUI::Colour(0.62f, 0.9f, 0.45f, 1.0f);
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

void ApplyKoMarkerVisualState(KoMarkerWidget& marker, int markerState, int markerRelation)
{
    const MyGUI::Colour colour = ResolveMarkerColour(markerState, markerRelation);
    const MyGUI::Colour beaconColour(colour.red, colour.green, colour.blue, kKoBeaconAlpha);

    if (marker.beacon && kEnableUiBeaconOverlay)
    {
        const bool wantsCustomDyingIcon = (markerState == CachedKoTarget::STATE_DYING && !g_config.customDyingIconTexture.empty());
        bool customDyingReady = false;
        if (wantsCustomDyingIcon)
        {
            customDyingReady = SetKoMarkerIconTexture(marker.beacon, g_config.customDyingIconTexture.c_str());
            const int fallbackSize = static_cast<int>(g_config.customDyingIconSizePx);
            if (customDyingReady)
            {
                SetKoMarkerIconCoord(marker.beacon, ResolveCustomIconCoordFromImageSize(marker.beacon, fallbackSize));
                SetKoMarkerIconColour(marker.beacon, MyGUI::Colour(1.0f, 1.0f, 1.0f, kKoBeaconAlpha));
            }
            else
            {
                SetKoMarkerIconTexture(marker.beacon, ResolveMarkerIconTexture(markerState, markerRelation));
                SetKoMarkerIconCoord(marker.beacon, ResolveMarkerIconCoord(markerState, markerRelation));
                SetKoMarkerIconColour(marker.beacon, beaconColour);
            }
        }
        else
        {
            SetKoMarkerIconTexture(marker.beacon, ResolveMarkerIconTexture(markerState, markerRelation));
            SetKoMarkerIconCoord(marker.beacon, ResolveMarkerIconCoord(markerState, markerRelation));
            SetKoMarkerIconColour(marker.beacon, beaconColour);
        }
    }

    if (marker.icon)
    {
        if (g_config.showMarkerIcons)
        {
            const bool wantsCustomDyingIcon = (markerState == CachedKoTarget::STATE_DYING && !g_config.customDyingIconTexture.empty());
            bool customDyingReady = false;
            if (wantsCustomDyingIcon)
            {
                customDyingReady = SetKoMarkerIconTexture(marker.icon, g_config.customDyingIconTexture.c_str());
                const int fallbackSize = static_cast<int>(g_config.customDyingIconSizePx);
                if (customDyingReady)
                {
                    // Preserve custom icon authoring colors (no relation tint).
                    SetKoMarkerIconCoord(marker.icon, ResolveCustomIconCoordFromImageSize(marker.icon, fallbackSize));
                    SetKoMarkerIconColour(marker.icon, MyGUI::Colour(1.0f, 1.0f, 1.0f, 1.0f));
                }
                else
                {
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

    if (marker.fallbackText && g_config.showMarkerText)
    {
        SetKoMarkerCaption(marker.fallbackText, ResolveMarkerCaption(markerState));
        SetKoMarkerTextColour(marker.fallbackText, colour);
    }
}

void HideAllKoMarkerWidgets()
{
    for (size_t i = 0; i < g_koMarkerWidgets.size(); ++i)
    {
        SetKoMarkerVisible(g_koMarkerWidgets[i], false);
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

        MyGUI::ImageBox* beacon = gui->createWidget<MyGUI::ImageBox>(
            "ImageBox",
            MyGUI::IntCoord(0, 0, kKoBeaconSizePx, kKoBeaconSizePx),
            MyGUI::Align::Default,
            "Top",
            name.str() + "_beacon");

        MyGUI::ImageBox* icon = gui->createWidget<MyGUI::ImageBox>(
            "ImageBox",
            MyGUI::IntCoord(0, 0, kKoMarkerHeightPx, kKoMarkerHeightPx),
            MyGUI::Align::Default,
            "Top",
            name.str() + "_icon");

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
        if (!beacon && !icon && !fallbackText)
        {
            return false;
        }
        if (!icon && fallbackText)
        {
            LogWarn("ImageBox marker unavailable; using text fallback");
        }

        if (beacon)
        {
            beacon->setNeedMouseFocus(false);
            beacon->setImageTexture("default_icon.png");
            beacon->setVisible(false);
        }

        if (icon)
        {
            icon->setNeedMouseFocus(false);
            icon->setImageTexture("default_icon.png");
            icon->setVisible(false);
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

        KoMarkerWidget marker = { beacon, icon, fallbackText };

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
        if (!g_koMarkerWidgets[i].beacon && !g_koMarkerWidgets[i].icon && !g_koMarkerWidgets[i].fallbackText && !CreateKoMarkerWidgetAt(i))
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

    if (!g_config.showMarkerIcons && !g_config.showMarkerText)
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

        Ogre::Vector3 characterWorldPos = cached.worldPos;
        Character* targetCharacter = cached.targetHandle.getCharacter();
        if (targetCharacter && targetCharacter->isValid())
        {
            characterWorldPos = targetCharacter->getPosition();
        }

        Ogre::Vector3 markerAnchor = characterWorldPos;
        markerAnchor.y += kKoMarkerHeadAnchorYOffset;

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

        int markerLeft = static_cast<int>(pixelX) - (kKoMarkerWidthPx / 2);
        int markerTop = static_cast<int>(pixelY) - kKoMarkerYOffsetPx;
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
        ApplyKoMarkerVisualState(marker, cached.markerState, cached.markerRelation);
        SetKoMarkerPosition(marker, markerLeft, markerTop);
        SetKoMarkerVisible(marker, true);

        ++visibleMarkerCount;
    }

    for (size_t i = visibleMarkerCount; i < g_koMarkerWidgets.size(); ++i)
    {
        SetKoMarkerVisible(g_koMarkerWidgets[i], false);
    }
}

bool TryResolveMarkerState(Character* candidate, int* markerStateOut, MarkerStateDebugInfo* debugInfoOut)
{
    if (!candidate || !markerStateOut)
    {
        return false;
    }

    bool isUnconscious = false;
    bool isDying = false;
    bool isPlayingDead = false;
    bool isLiteral = false;
    bool isProbablyDying = false;
    bool dyingByProbablyLiteral = false;
    bool dyingByProbablyLowBlood = false;
    bool dyingByProbablySub50Ko = false;
    bool dyingByActiveBleed = false;
    bool dyingByTrauma = false;
    bool dyingByBloodThreshold = false;
    bool medicalUnconsciousFlag = false;
    bool medicalSub50KoFlag = false;
    bool medicalBloodlossTraumaFlag = false;
    float currentBleedRate = 0.0f;
    int proneState = -1;
    bool hasSleepState = false;
    int sleepState = -1;
    bool hasSlaveState = false;
    int slaveState = -1;
    float bloodLevel = 0.0f;
    float pointOfNoReturn = 0.0f;

    if (debugInfoOut)
    {
        debugInfoOut->isUnconscious = false;
        debugInfoOut->isPlayingDead = false;
        debugInfoOut->isLiteral = false;
        debugInfoOut->isProbablyDying = false;
        debugInfoOut->dyingByProbablyLiteral = false;
        debugInfoOut->dyingByProbablyLowBlood = false;
        debugInfoOut->dyingByProbablySub50Ko = false;
        debugInfoOut->dyingByActiveBleed = false;
        debugInfoOut->dyingByTrauma = false;
        debugInfoOut->dyingByBloodThreshold = false;
        debugInfoOut->medicalUnconsciousFlag = false;
        debugInfoOut->medicalSub50KoFlag = false;
        debugInfoOut->medicalBloodlossTraumaFlag = false;
        debugInfoOut->currentBleedRate = 0.0f;
        debugInfoOut->proneState = -1;
        debugInfoOut->hasSleepState = false;
        debugInfoOut->sleepState = -1;
        debugInfoOut->hasSlaveState = false;
        debugInfoOut->slaveState = -1;
        debugInfoOut->bloodLevel = 0.0f;
        debugInfoOut->pointOfNoReturn = 0.0f;
    }

    __try
    {
        isUnconscious = candidate->isUnconcious();
        if (isUnconscious)
        {
            proneState = static_cast<int>(candidate->_currentProneState);
            isPlayingDead = (candidate->_currentProneState == PS_PLAYING_DEAD);
            isLiteral = candidate->isLiterallyUnconciousNotPretending();
            isProbablyDying = candidate->medical.isProbablyDying();
            dyingByTrauma = candidate->medical.isInBloodlossTrauma();
            medicalUnconsciousFlag = candidate->medical.unconcious;
            medicalSub50KoFlag = candidate->medical.sub50KO;
            medicalBloodlossTraumaFlag = candidate->medical.bloodlossTrauma;
            currentBleedRate = candidate->medical.currentBleedRate;
            bloodLevel = candidate->medical.blood;
            pointOfNoReturn = candidate->medical.pointOfNoReturn();
            dyingByBloodThreshold = (bloodLevel <= pointOfNoReturn);
            dyingByProbablyLiteral = (isProbablyDying && isLiteral);
            dyingByProbablyLowBlood = (isProbablyDying && bloodLevel <= kProbablyDyingBloodMax);
            dyingByProbablySub50Ko = medicalSub50KoFlag;
            dyingByActiveBleed = (isProbablyDying && (currentBleedRate > 0.0f || candidate->medical.extraBloodLossFromBodyparts > 0.0f));

            StateBroadcastData* stateBroadcast = candidate->getStateBroadcast();
            if (stateBroadcast)
            {
                sleepState = stateBroadcast->sleepState;
                hasSleepState = true;
                slaveState = static_cast<int>(stateBroadcast->slaveState);
                hasSlaveState = true;
            }

            // Match in-game DY closer: sub50 KO and no-return blood are the stable signals.
            isDying = dyingByProbablySub50Ko || dyingByBloodThreshold;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    if (debugInfoOut)
    {
        debugInfoOut->isUnconscious = isUnconscious;
        debugInfoOut->isPlayingDead = isPlayingDead;
        debugInfoOut->isLiteral = isLiteral;
        debugInfoOut->isProbablyDying = isProbablyDying;
        debugInfoOut->dyingByProbablyLiteral = dyingByProbablyLiteral;
        debugInfoOut->dyingByProbablyLowBlood = dyingByProbablyLowBlood;
        debugInfoOut->dyingByProbablySub50Ko = dyingByProbablySub50Ko;
        debugInfoOut->dyingByActiveBleed = dyingByActiveBleed;
        debugInfoOut->dyingByTrauma = dyingByTrauma;
        debugInfoOut->dyingByBloodThreshold = dyingByBloodThreshold;
        debugInfoOut->medicalUnconsciousFlag = medicalUnconsciousFlag;
        debugInfoOut->medicalSub50KoFlag = medicalSub50KoFlag;
        debugInfoOut->medicalBloodlossTraumaFlag = medicalBloodlossTraumaFlag;
        debugInfoOut->currentBleedRate = currentBleedRate;
        debugInfoOut->proneState = proneState;
        debugInfoOut->hasSleepState = hasSleepState;
        debugInfoOut->sleepState = sleepState;
        debugInfoOut->hasSlaveState = hasSlaveState;
        debugInfoOut->slaveState = slaveState;
        debugInfoOut->bloodLevel = bloodLevel;
        debugInfoOut->pointOfNoReturn = pointOfNoReturn;
    }

    if (isUnconscious && isDying)
    {
        if (isProbablyDying && !g_loggedDyingByProbably)
        {
            LogDyingDetection("isProbablyDying", bloodLevel, pointOfNoReturn);
            g_loggedDyingByProbably = true;
        }
        if (dyingByProbablyLiteral && !g_loggedDyingByProbablyLiteral)
        {
            LogDyingDetection("isProbablyDying+literal", bloodLevel, pointOfNoReturn);
            g_loggedDyingByProbablyLiteral = true;
        }
        if (dyingByProbablyLowBlood && !g_loggedDyingByProbablyLowBlood)
        {
            LogDyingDetection("isProbablyDying+low_blood", bloodLevel, pointOfNoReturn);
            g_loggedDyingByProbablyLowBlood = true;
        }
        if (dyingByProbablySub50Ko && !g_loggedDyingBySub50Ko)
        {
            LogDyingDetection("sub50KO", bloodLevel, pointOfNoReturn);
            g_loggedDyingBySub50Ko = true;
        }
        if (dyingByTrauma && !g_loggedDyingByTrauma)
        {
            LogDyingDetection("bloodloss trauma", bloodLevel, pointOfNoReturn);
            g_loggedDyingByTrauma = true;
        }
        if (dyingByBloodThreshold && !g_loggedDyingByBloodThreshold)
        {
            LogDyingDetection("blood threshold", bloodLevel, pointOfNoReturn);
            g_loggedDyingByBloodThreshold = true;
        }
    }

    if (isUnconscious)
    {
        if (isPlayingDead)
        {
            *markerStateOut = CachedKoTarget::STATE_PLAYING_DEAD;
            return true;
        }
        if (isDying)
        {
            *markerStateOut = CachedKoTarget::STATE_DYING;
            return true;
        }
        *markerStateOut = CachedKoTarget::STATE_UNCONSCIOUS;
        return true;
    }

    return false;
}

bool IsHighlightGateOpen()
{
    if (!g_config.onlyWhenAltHeld)
    {
        g_lastHighlightGateOpenTickMs = GetTickCount();
        return true;
    }

    const bool rawOpen = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    const DWORD nowMs = GetTickCount();
    if (rawOpen)
    {
        g_lastHighlightGateOpenTickMs = nowMs;
        return true;
    }

    if (g_lastHighlightGateOpenTickMs != 0 && (nowMs - g_lastHighlightGateOpenTickMs) <= kHighlightGateReleaseDebounceMs)
    {
        return true;
    }

    return false;
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

bool IsTargetSelected(const hand& targetHandle)
{
    if (targetHandle.isNull() || !ou || !ou->player)
    {
        return false;
    }

    const hand selectedCharacter = ou->player->selectedCharacter;
    if (HandsEqualExact(selectedCharacter, targetHandle))
    {
        return true;
    }
    if (!selectedCharacter.isNull() && selectedCharacter.toString() == targetHandle.toString())
    {
        return true;
    }

    const hand selectedObject = ou->player->selectedObject;
    if (HandsEqualExact(selectedObject, targetHandle))
    {
        return true;
    }
    if (!selectedObject.isNull() && selectedObject.toString() == targetHandle.toString())
    {
        return true;
    }

    const ogre_unordered_set<hand>::type& selectedCharacters = ou->player->selectedCharacters;
    const std::string targetText = targetHandle.toString();
    for (ogre_unordered_set<hand>::type::const_iterator it = selectedCharacters.begin(); it != selectedCharacters.end(); ++it)
    {
        if (HandsEqualExact(*it, targetHandle))
        {
            return true;
        }
        if (!it->isNull() && it->toString() == targetText)
        {
            return true;
        }
    }

    return false;
}

bool HasAnySelection()
{
    if (!ou || !ou->player)
    {
        return false;
    }

    if (!ou->player->selectedCharacter.isNull())
    {
        return true;
    }

    if (!ou->player->selectedObject.isNull())
    {
        return true;
    }

    return !ou->player->selectedCharacters.empty();
}

void LogSelectionSnapshot(DWORD nowMs)
{
    if (!kEnableVerboseRuntimeLogs)
    {
        return;
    }

    if (!ou || !ou->player)
    {
        return;
    }

    const hand selectedCharacter = ou->player->selectedCharacter;
    const hand selectedObject = ou->player->selectedObject;
    const ogre_unordered_set<hand>::type& selectedCharacters = ou->player->selectedCharacters;
    const size_t selectedCount = selectedCharacters.size();

    const bool selectionChanged = (selectedCharacter != g_lastSelectionSnapshotCharacter) ||
        (selectedObject != g_lastSelectionSnapshotObject) ||
        (selectedCount != g_lastSelectionSnapshotCount);
    if (!selectionChanged && g_lastSelectionSnapshotTickMs != 0 && (nowMs - g_lastSelectionSnapshotTickMs) < 1500)
    {
        return;
    }

    g_lastSelectionSnapshotCharacter = selectedCharacter;
    g_lastSelectionSnapshotObject = selectedObject;
    g_lastSelectionSnapshotCount = selectedCount;
    g_lastSelectionSnapshotTickMs = nowMs;

    std::stringstream info;
    info << "selection_snapshot selected_character="
         << (selectedCharacter.isNull() ? "null" : selectedCharacter.toString())
         << " selected_object="
         << (selectedObject.isNull() ? "null" : selectedObject.toString())
         << " selected_set_count=" << selectedCount;
    if (selectedCount > 0)
    {
        info << " selected_set=";
        size_t emitted = 0;
        for (ogre_unordered_set<hand>::type::const_iterator it = selectedCharacters.begin(); it != selectedCharacters.end() && emitted < 4; ++it)
        {
            if (emitted > 0)
            {
                info << ",";
            }
            info << it->toString();
            ++emitted;
        }
        if (selectedCount > 4)
        {
            info << ",...";
        }
    }
    LogInfo(info.str());
}

void MaybeLogSelectionNoMatch(DWORD nowMs)
{
    if (!kEnableVerboseRuntimeLogs)
    {
        return;
    }

    if (!HasAnySelection())
    {
        return;
    }

    if (g_lastSelectionNoMatchTickMs != 0 && (nowMs - g_lastSelectionNoMatchTickMs) < 1500)
    {
        return;
    }
    g_lastSelectionNoMatchTickMs = nowMs;
    LogInfo("selection_probe no selected handles matched visible KO marker targets");
}

void LogSelectedDeepDebug(Character* candidate, const hand& targetHandle, int markerState, DWORD nowMs)
{
    if (!kEnableVerboseRuntimeLogs)
    {
        return;
    }

    if (!candidate || !candidate->isValid())
    {
        return;
    }

    bool sameHandle = HandsEqualExact(g_lastSelectedDeepDumpHandle, targetHandle);
    if (!sameHandle && !g_lastSelectedDeepDumpHandle.isNull())
    {
        sameHandle = (g_lastSelectedDeepDumpHandle.toString() == targetHandle.toString());
    }
    if (sameHandle && g_lastSelectedDeepDumpTickMs != 0 && (nowMs - g_lastSelectedDeepDumpTickMs) < 1200)
    {
        return;
    }

    g_lastSelectedDeepDumpHandle = targetHandle;
    g_lastSelectedDeepDumpTickMs = nowMs;

    bool charIsUnconscious = false;
    bool charIsDead = false;
    bool charLiteralKo = false;
    int charProneState = -1;
    bool charOnScreen = false;
    bool charVisibleNear = false;
    int charInSomething = -1;
    std::string charInWhat = "null";
    bool charBeingCarried = false;
    std::string charCarryingObject = "null";

    bool medMethodUnconscious = false;
    bool medMethodDead = false;
    bool medCanGetUp = false;
    bool medHungerKo = false;
    bool medProbablyDying = false;
    bool medMethodTrauma = false;
    bool medFlagUnconscious = false;
    bool medFlagSub50Ko = false;
    bool medFlagTrauma = false;
    bool medFlagDead = false;
    bool medFlagCrippled = false;
    float medHunger = 0.0f;
    float medFed = 0.0f;
    float medBlood = 0.0f;
    float medMaxBlood = 0.0f;
    float medBleedRate = 0.0f;
    float medExtraBloodLoss = 0.0f;
    float medKnockoutTimer = 0.0f;
    float medNextKoTime = 0.0f;
    float medPointCollapse = 0.0f;
    float medPointNoReturn = 0.0f;
    float medRestedState = 0.0f;
    float medWorstDamage = 0.0f;
    float medDazedOrAlert = 0.0f;

    bool sbPresent = false;
    int sbSleepState = -1;
    int sbSlaveState = -1;
    bool sbUnavailable = false;
    double sbSsct = 0.0;
    float sbStrong = 0.0f;
    float sbMoveSpeed = 0.0f;
    int sbPersonality = -1;
    int sbNpcClass = -1;
    bool sbEscap = false;
    bool sbKidn = false;
    bool sbTn = false;
    float sbSlaveness = 0.0f;
    float sbDisguise = 0.0f;
    float sbDisguiseBlown = 0.0f;
    bool sbUnprovoked = false;

    charIsUnconscious = candidate->isUnconcious();
    charIsDead = candidate->isDead();
    charLiteralKo = candidate->isLiterallyUnconciousNotPretending();
    charProneState = static_cast<int>(candidate->_currentProneState);
    charOnScreen = candidate->isOnScreen;
    charVisibleNear = candidate->isVisibleAndNear;
    charInSomething = static_cast<int>(candidate->inSomething);
    if (!candidate->inWhat.isNull())
    {
        charInWhat = candidate->inWhat.toString();
    }
    charBeingCarried = candidate->isBeingCarried();
    const hand carryingObject = candidate->getCarryingObject();
    if (!carryingObject.isNull())
    {
        charCarryingObject = carryingObject.toString();
    }

    medMethodUnconscious = candidate->medical.isUnconcious();
    medMethodDead = candidate->medical.isDead();
    medCanGetUp = candidate->medical.canGetUpWakeUp();
    medHungerKo = candidate->medical.isHungerKO();
    medProbablyDying = candidate->medical.isProbablyDying();
    medMethodTrauma = candidate->medical.isInBloodlossTrauma();
    medFlagUnconscious = candidate->medical.unconcious;
    medFlagSub50Ko = candidate->medical.sub50KO;
    medFlagTrauma = candidate->medical.bloodlossTrauma;
    medFlagDead = candidate->medical.dead;
    medFlagCrippled = candidate->medical.crippled;
    medHunger = candidate->medical.hunger;
    medFed = candidate->medical.fed;
    medBlood = candidate->medical.blood;
    medMaxBlood = candidate->medical.getMaxBlood();
    medBleedRate = candidate->medical.currentBleedRate;
    medExtraBloodLoss = candidate->medical.extraBloodLossFromBodyparts;
    medKnockoutTimer = candidate->medical.knockoutTimer;
    medNextKoTime = candidate->medical.nextKOTime;
    medPointCollapse = candidate->medical.pointOfCollapseBloodloss();
    medPointNoReturn = candidate->medical.pointOfNoReturn();
    medRestedState = candidate->medical.restedState;
    medWorstDamage = candidate->medical.worstDamage;
    medDazedOrAlert = candidate->medical.dazedOrAlert;

    StateBroadcastData* stateBroadcast = candidate->getStateBroadcast();
    if (stateBroadcast)
    {
        sbPresent = true;
        sbSleepState = stateBroadcast->sleepState;
        sbSlaveState = static_cast<int>(stateBroadcast->slaveState);
        sbUnavailable = stateBroadcast->unavailble;
        sbSsct = stateBroadcast->ssct;
        sbStrong = stateBroadcast->strong;
        sbMoveSpeed = stateBroadcast->moveSpeed;
        sbPersonality = static_cast<int>(stateBroadcast->personality);
        sbNpcClass = static_cast<int>(stateBroadcast->npcClass);
        sbEscap = stateBroadcast->escap;
        sbKidn = stateBroadcast->kidn;
        sbTn = stateBroadcast->tn;
        sbSlaveness = stateBroadcast->slaveness;
        sbDisguise = stateBroadcast->disguise;
        sbDisguiseBlown = stateBroadcast->disguiseblown;
        sbUnprovoked = stateBroadcast->unprovoked;
    }

    std::stringstream info;
    info << "selected_deep_dump handle=" << targetHandle.toString()
         << " marker_state=" << MarkerStateName(markerState)
         << " char_unconcious=" << (charIsUnconscious ? "true" : "false")
         << " char_dead=" << (charIsDead ? "true" : "false")
         << " char_literal_ko=" << (charLiteralKo ? "true" : "false")
         << " char_prone_state=" << charProneState
         << " char_on_screen=" << (charOnScreen ? "true" : "false")
         << " char_visible_near=" << (charVisibleNear ? "true" : "false")
         << " char_in_something=" << charInSomething
         << " char_in_what=" << charInWhat
         << " char_being_carried=" << (charBeingCarried ? "true" : "false")
         << " char_carrying_object=" << charCarryingObject
         << " med_unconcious_method=" << (medMethodUnconscious ? "true" : "false")
         << " med_dead_method=" << (medMethodDead ? "true" : "false")
         << " med_can_get_up=" << (medCanGetUp ? "true" : "false")
         << " med_hunger_ko=" << (medHungerKo ? "true" : "false")
         << " med_probably_dying=" << (medProbablyDying ? "true" : "false")
         << " med_trauma_method=" << (medMethodTrauma ? "true" : "false")
         << " med_unconcious_flag=" << (medFlagUnconscious ? "true" : "false")
         << " med_sub50_ko_flag=" << (medFlagSub50Ko ? "true" : "false")
         << " med_trauma_flag=" << (medFlagTrauma ? "true" : "false")
         << " med_dead_flag=" << (medFlagDead ? "true" : "false")
         << " med_crippled_flag=" << (medFlagCrippled ? "true" : "false")
         << " med_hunger=" << medHunger
         << " med_fed=" << medFed
         << " med_blood=" << medBlood
         << " med_max_blood=" << medMaxBlood
         << " med_bleed_rate=" << medBleedRate
         << " med_extra_blood_loss=" << medExtraBloodLoss
         << " med_knockout_timer=" << medKnockoutTimer
         << " med_next_ko_time=" << medNextKoTime
         << " med_point_of_collapse=" << medPointCollapse
         << " med_point_of_no_return=" << medPointNoReturn
         << " med_rested_state=" << medRestedState
         << " med_worst_damage=" << medWorstDamage
         << " med_dazed_or_alert=" << medDazedOrAlert
         << " sb_present=" << (sbPresent ? "true" : "false")
         << " sb_sleep_state=" << sbSleepState
         << " sb_slave_state=" << sbSlaveState
         << " sb_unavailable=" << (sbUnavailable ? "true" : "false")
         << " sb_ssct=" << sbSsct
         << " sb_strong=" << sbStrong
         << " sb_move_speed=" << sbMoveSpeed
         << " sb_personality=" << sbPersonality
         << " sb_npc_class=" << sbNpcClass
         << " sb_escap=" << (sbEscap ? "true" : "false")
         << " sb_kidn=" << (sbKidn ? "true" : "false")
         << " sb_tn=" << (sbTn ? "true" : "false")
         << " sb_slaveness=" << sbSlaveness
         << " sb_disguise=" << sbDisguise
         << " sb_disguise_blown=" << sbDisguiseBlown
         << " sb_unprovoked=" << (sbUnprovoked ? "true" : "false");
    LogInfo(info.str());
}

void MaybeLogSelectedPanelStatus(const hand& targetHandle, DWORD nowMs)
{
    if (!kEnableVerboseRuntimeLogs)
    {
        return;
    }

    if (!kEnableUnsafePanelProbe)
    {
        return;
    }

    const hand primarySelection = GetPrimarySelectionHandle();
    if (primarySelection.isNull() || !HandsEqualWithFallback(primarySelection, targetHandle))
    {
        return;
    }

    const bool sameHandle = HandsEqualWithFallback(g_lastSelectedPanelDumpHandle, targetHandle);
    if (sameHandle && g_lastSelectedPanelDumpTickMs != 0 && (nowMs - g_lastSelectedPanelDumpTickMs) < 1200)
    {
        return;
    }

    g_lastSelectedPanelDumpHandle = targetHandle;
    g_lastSelectedPanelDumpTickMs = nowMs;

    ForgottenGUI* gui = ResolveKenshiGui();
    if (!gui)
    {
        LogWarn("selected_panel_probe gui_unavailable=true");
        return;
    }

    DatapanelGUI* panel = gui->_0x18;
    if (!panel)
    {
        LogWarn("selected_panel_probe panel_unavailable=true");
        return;
    }

    std::stringstream header;
    header << "selected_panel_probe handle=" << targetHandle.toString()
           << " gui_display_handle=" << (ou && !ou->guiDisplayObject.isNull() ? ou->guiDisplayObject.toString() : "null")
           << " category_count=" << panel->_0x60.size()
           << " flat_line_count=" << panel->_0x88.size();
    LogInfo(header.str());

    size_t statusLineCount = 0;
    size_t previewCount = 0;
    std::vector<std::string> previewLines;
    previewLines.reserve(12);

    for (Ogre::map<int, Ogre::map<std::string, DataPanelLine*>::type>::type::const_iterator catIt = panel->_0x60.begin(); catIt != panel->_0x60.end(); ++catIt)
    {
        const int categoryId = catIt->first;
        const Ogre::map<std::string, DataPanelLine*>::type& lineMap = catIt->second;
        for (Ogre::map<std::string, DataPanelLine*>::type::const_iterator lineIt = lineMap.begin(); lineIt != lineMap.end(); ++lineIt)
        {
            DataPanelLine* line = lineIt->second;
            if (!line)
            {
                continue;
            }

            const std::string keyText = ClipForLog(lineIt->first, 48);
            const std::string s1 = ClipForLog(line->_0x28, 64);
            const std::string s2 = ClipForLog(line->_0x50, 64);
            const std::string s3 = ClipForLog(line->_0x78, 64);
            const std::string s4 = ClipForLog(line->_0xa8, 64);
            const std::string s5 = ClipForLog(line->_0xd0, 64);

            std::stringstream lineInfo;
            lineInfo << "selected_panel_line cat=" << categoryId
                     << " key=" << keyText
                     << " id=" << line->id
                     << " type=" << line->type
                     << " s1=" << s1
                     << " s2=" << s2
                     << " s3=" << s3
                     << " s4=" << s4
                     << " s5=" << s5;

            if (previewCount < 12)
            {
                previewLines.push_back(lineInfo.str());
                ++previewCount;
            }

            const std::string joined = ToLowerAsciiCopy(keyText + " " + s1 + " " + s2 + " " + s3 + " " + s4 + " " + s5);
            if (ContainsStatusToken(joined))
            {
                ++statusLineCount;
                LogInfo(lineInfo.str());
            }
        }
    }

    if (statusLineCount == 0)
    {
        LogInfo("selected_panel_probe status_line_count=0 preview=true");
        for (size_t i = 0; i < previewLines.size(); ++i)
        {
            LogInfo(previewLines[i]);
        }
    }
}

void MaybeLogSelectedMarkerState(Character* candidate, const hand& targetHandle, int markerState, int markerRelation, const MarkerStateDebugInfo& debugInfo, DWORD nowMs)
{
    if (!kEnableVerboseRuntimeLogs)
    {
        return;
    }

    if (!IsTargetSelected(targetHandle))
    {
        return;
    }

    bool sameHandle = HandsEqualExact(g_lastSelectedDebugHandle, targetHandle);
    if (!sameHandle && !g_lastSelectedDebugHandle.isNull())
    {
        sameHandle = (g_lastSelectedDebugHandle.toString() == targetHandle.toString());
    }
    const bool sameState = (g_lastSelectedDebugState == markerState && g_lastSelectedDebugRelation == markerRelation);
    if (sameHandle && sameState && g_lastSelectedDebugTickMs != 0 && (nowMs - g_lastSelectedDebugTickMs) < 1500)
    {
        return;
    }

    g_lastSelectedDebugHandle = targetHandle;
    g_lastSelectedDebugState = markerState;
    g_lastSelectedDebugRelation = markerRelation;
    g_lastSelectedDebugTickMs = nowMs;
    LogMarkerStateDecision("selected_probe", targetHandle, markerState, markerRelation, debugInfo, true);
    LogSelectedDeepDebug(candidate, targetHandle, markerState, nowMs);
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

void TickKoProbe()
{
    const bool anyMarkerVisualEnabled = (g_config.showMarkerIcons || g_config.showMarkerText);
    const bool canRun = g_config.enabled && anyMarkerVisualEnabled && IsHighlightGateOpen() && ou;
    if (!canRun)
    {
        g_lastHighlightGateOpenTickMs = 0;
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
    LogSelectionSnapshot(nowMs);

    if (g_lastProbeTickMs != 0 && (nowMs - g_lastProbeTickMs) < g_config.updateIntervalMs)
    {
        TickKoMarkerRender();
        return;
    }
    g_lastProbeTickMs = nowMs;

    const Ogre::Vector3 cameraCenter = ou->getCameraCenter();

    const ogre_unordered_set<Character*>::type& activeCharacters = ou->getCharacterUpdateList();
    g_visibleKoHandlesScratch.clear();
    bool anySelectedMarkerMatched = false;

    for (auto iter = activeCharacters.begin(); iter != activeCharacters.end(); ++iter)
    {
        Character* candidate = *iter;
        if (!candidate || !candidate->isValid())
        {
            continue;
        }

        if (!candidate->isOnScreen)
        {
            continue;
        }

        if (candidate->isDead())
        {
            continue;
        }

        if (!IsWithinHighlightRange(cameraCenter, candidate->getPosition()))
        {
            continue;
        }

        int markerState = CachedKoTarget::STATE_UNCONSCIOUS;
        MarkerStateDebugInfo markerDebugInfo = {};
        if (TryResolveMarkerState(candidate, &markerState, &markerDebugInfo))
        {
            const hand targetHandle = candidate->getHandle();
            if (targetHandle.isNull())
            {
                continue;
            }

            const int markerRelation = ResolveMarkerRelation(candidate);
            const bool isSelected = IsTargetSelected(targetHandle);
            if (isSelected)
            {
                anySelectedMarkerMatched = true;
            }
            MaybeLogSelectedMarkerState(candidate, targetHandle, markerState, markerRelation, markerDebugInfo, nowMs);

            if (!VisibleHandleListContains(targetHandle))
            {
                g_visibleKoHandlesScratch.push_back(targetHandle);
            }

            const int existingIndex = FindCachedKoTargetIndex(targetHandle);
            if (existingIndex >= 0)
            {
                CachedKoTarget& existing = g_koTargetCache[existingIndex];
                if (existing.markerState != markerState || existing.markerRelation != markerRelation)
                {
                    LogMarkerStateDecision("state_changed", targetHandle, markerState, markerRelation, markerDebugInfo, isSelected);
                }
                existing.worldPos = candidate->getPosition();
                existing.lastSeenMs = nowMs;
                existing.markerState = markerState;
                existing.markerRelation = markerRelation;
            }
            else
            {
                CachedKoTarget created = {
                    targetHandle,
                    candidate->getPosition(),
                    nowMs,
                    markerState,
                    markerRelation
                };
                g_koTargetCache.push_back(created);
                if (isSelected || markerState == CachedKoTarget::STATE_UNCONSCIOUS)
                {
                    LogMarkerStateDecision("first_seen", targetHandle, markerState, markerRelation, markerDebugInfo, isSelected);
                }
            }
        }
    }

    if (!anySelectedMarkerMatched)
    {
        MaybeLogSelectionNoMatch(nowMs);
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
         << ", show_icons=" << (g_config.showMarkerIcons ? "true" : "false")
         << ", show_text=" << (g_config.showMarkerText ? "true" : "false")
         << ", alt_release_debounce_ms=" << kHighlightGateReleaseDebounceMs
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
