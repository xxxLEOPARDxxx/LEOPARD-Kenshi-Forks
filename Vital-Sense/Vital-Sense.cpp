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
};

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
    true
};
std::string g_settingsPath;
DWORD g_lastProbeTickMs = 0;
void (*PlayerInterface_updateUT_orig)(PlayerInterface*) = 0;

struct CachedKoTarget
{
    enum MarkerState
    {
        STATE_UNCONSCIOUS = 0,
        STATE_RECOVERY_COMA = 1,
        STATE_DYING = 2,
        STATE_PLAYING_DEAD = 3,
        STATE_DEAD = 4
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
    bool isRecoveryComa;
    bool isPlayingDead;
    bool isLiteral;
    bool isProbablyDying;
    bool dyingByProbablyLiteral;
    bool dyingByProbablyLowBlood;
    bool dyingByProbablySub50Ko;
    bool dyingByActiveBleed;
    bool dyingByTrauma;
    bool dyingByBloodThreshold;
    bool recoveryComaBySub50Ko;
    bool recoveryComaByCannotWake;
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
const DWORD kDefaultMarkerTextSizePx = 18;
const bool kEnableUnsafePanelProbe = false;
const bool kEnableUiBeaconOverlay = false;
const bool kEnableVerboseRuntimeLogs = false;
const bool kEnableTextureInfoLogs = false;

void LogInfo(const std::string& message);
void LogWarn(const std::string& message);
bool IsMarkerStateEnabled(int markerState);
void LogIconTextureOnce(std::vector<std::string>& sink, const std::string& message, const char* textureName, bool warn);

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
    if (markerState == CachedKoTarget::STATE_DEAD)
    {
        return "DE";
    }
    if (markerState == CachedKoTarget::STATE_RECOVERY_COMA)
    {
        return "RC";
    }
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
         << " recovery_coma=" << (debugInfo.isRecoveryComa ? "true" : "false")
         << " playing_dead=" << (debugInfo.isPlayingDead ? "true" : "false")
         << " literal_ko=" << (debugInfo.isLiteral ? "true" : "false")
         << " probably_dying=" << (debugInfo.isProbablyDying ? "true" : "false")
         << " probably_literal=" << (debugInfo.dyingByProbablyLiteral ? "true" : "false")
         << " probably_low_blood=" << (debugInfo.dyingByProbablyLowBlood ? "true" : "false")
         << " probably_sub50_ko=" << (debugInfo.dyingByProbablySub50Ko ? "true" : "false")
         << " active_bleed_dying=" << (debugInfo.dyingByActiveBleed ? "true" : "false")
         << " bloodloss_trauma=" << (debugInfo.dyingByTrauma ? "true" : "false")
         << " blood_threshold=" << (debugInfo.dyingByBloodThreshold ? "true" : "false")
         << " recovery_coma_sub50_ko=" << (debugInfo.recoveryComaBySub50Ko ? "true" : "false")
         << " recovery_coma_cannot_wake=" << (debugInfo.recoveryComaByCannotWake ? "true" : "false")
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

bool TryParseHexNibble(char value, unsigned int* nibbleOut)
{
    if (!nibbleOut)
    {
        return false;
    }

    if (value >= '0' && value <= '9')
    {
        *nibbleOut = static_cast<unsigned int>(value - '0');
        return true;
    }
    if (value >= 'a' && value <= 'f')
    {
        *nibbleOut = static_cast<unsigned int>(10 + (value - 'a'));
        return true;
    }
    if (value >= 'A' && value <= 'F')
    {
        *nibbleOut = static_cast<unsigned int>(10 + (value - 'A'));
        return true;
    }
    return false;
}

bool TryParseHexByte(const std::string& value, size_t pos, unsigned int* byteOut)
{
    if (!byteOut || pos + 1 >= value.size())
    {
        return false;
    }

    unsigned int hi = 0;
    unsigned int lo = 0;
    if (!TryParseHexNibble(value[pos], &hi) || !TryParseHexNibble(value[pos + 1], &lo))
    {
        return false;
    }

    *byteOut = (hi << 4) | lo;
    return true;
}

bool TryParseColourHex(const std::string& rawValue, MyGUI::Colour* colourOut)
{
    if (!colourOut)
    {
        return false;
    }

    std::string value = TrimAscii(rawValue);
    if (value.empty())
    {
        return false;
    }

    if (!value.empty() && value[0] == '#')
    {
        value.erase(0, 1);
    }

    if (value.size() != 6 && value.size() != 8)
    {
        return false;
    }

    unsigned int r = 0;
    unsigned int g = 0;
    unsigned int b = 0;
    unsigned int a = 255;
    if (!TryParseHexByte(value, 0, &r) ||
        !TryParseHexByte(value, 2, &g) ||
        !TryParseHexByte(value, 4, &b))
    {
        return false;
    }
    if (value.size() == 8 && !TryParseHexByte(value, 6, &a))
    {
        return false;
    }

    *colourOut = MyGUI::Colour(
        static_cast<float>(r) / 255.0f,
        static_cast<float>(g) / 255.0f,
        static_cast<float>(b) / 255.0f,
        static_cast<float>(a) / 255.0f);
    return true;
}

std::string ColourToHexRgb(const MyGUI::Colour& colour)
{
    auto toByte = [](float channel) -> unsigned int
    {
        if (channel < 0.0f)
        {
            channel = 0.0f;
        }
        if (channel > 1.0f)
        {
            channel = 1.0f;
        }
        return static_cast<unsigned int>(channel * 255.0f + 0.5f);
    };

    const unsigned int r = toByte(colour.red);
    const unsigned int g = toByte(colour.green);
    const unsigned int b = toByte(colour.blue);

    std::stringstream ss;
    ss << "#";
    const char* kHex = "0123456789ABCDEF";
    ss << kHex[(r >> 4) & 0xF] << kHex[r & 0xF]
       << kHex[(g >> 4) & 0xF] << kHex[g & 0xF]
       << kHex[(b >> 4) & 0xF] << kHex[b & 0xF];
    return ss.str();
}

bool LoadConfigState()
{
    g_config.enabled = true;
    g_config.updateIntervalMs = 150;
    g_config.onlyWhenAltHeld = true;
    g_config.maxHighlightDistanceMeters = 3500;
    g_config.enableUnconsciousState = true;
    g_config.enableRecoveryComaState = true;
    g_config.enableDyingState = true;
    g_config.enablePlayingDeadState = true;
    g_config.enableDeadState = true;
    g_config.unconsciousText = "ZZ";
    g_config.recoveryComaText = "RC";
    g_config.dyingText = "DY";
    g_config.playingDeadText = "PD";
    g_config.deadText = "DE";
    g_config.unconsciousTextSizePx = kDefaultMarkerTextSizePx;
    g_config.recoveryComaTextSizePx = kDefaultMarkerTextSizePx;
    g_config.dyingTextSizePx = kDefaultMarkerTextSizePx;
    g_config.playingDeadTextSizePx = kDefaultMarkerTextSizePx;
    g_config.deadTextSizePx = kDefaultMarkerTextSizePx;
    g_config.enemyMarkerColour = MyGUI::Colour(1.0f, 0.2f, 0.2f, 1.0f);
    g_config.allyMarkerColour = MyGUI::Colour(0.62f, 0.9f, 0.45f, 1.0f);
    g_config.squadMarkerColour = MyGUI::Colour(0.25f, 1.0f, 0.25f, 1.0f);
    g_config.customUnconsciousIconTexture.clear();
    g_config.customUnconsciousIconSizePx = 64;
    g_config.customRecoveryComaIconTexture.clear();
    g_config.customRecoveryComaIconSizePx = 64;
    g_config.customDyingIconTexture.clear();
    g_config.customDyingIconSizePx = 64;
    g_config.customPlayingDeadIconTexture.clear();
    g_config.customPlayingDeadIconSizePx = 64;
    g_config.customDeadIconTexture.clear();
    g_config.customDeadIconSizePx = 64;
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

    bool parsedEnableUnconsciousState = true;
    if (ParseBoolFromJson(body, "enable_unconscious", &parsedEnableUnconsciousState))
    {
        g_config.enableUnconsciousState = parsedEnableUnconsciousState;
    }

    bool parsedEnableRecoveryComaState = true;
    if (ParseBoolFromJson(body, "enable_recovery_coma", &parsedEnableRecoveryComaState))
    {
        g_config.enableRecoveryComaState = parsedEnableRecoveryComaState;
    }

    bool parsedEnableDyingState = true;
    if (ParseBoolFromJson(body, "enable_dying", &parsedEnableDyingState))
    {
        g_config.enableDyingState = parsedEnableDyingState;
    }

    bool parsedEnablePlayingDeadState = true;
    if (ParseBoolFromJson(body, "enable_playing_dead", &parsedEnablePlayingDeadState))
    {
        g_config.enablePlayingDeadState = parsedEnablePlayingDeadState;
    }

    bool parsedEnableDeadState = true;
    if (ParseBoolFromJson(body, "enable_dead", &parsedEnableDeadState))
    {
        g_config.enableDeadState = parsedEnableDeadState;
    }

    std::string parsedUnconsciousText;
    if (ParseStringFromJson(body, "unconscious_text", &parsedUnconsciousText))
    {
        const std::string trimmed = TrimAscii(parsedUnconsciousText);
        if (!trimmed.empty())
        {
            g_config.unconsciousText = trimmed;
        }
        else
        {
            LogWarn("unconscious_text is empty; using default");
        }
    }

    std::string parsedDyingText;
    if (ParseStringFromJson(body, "dying_text", &parsedDyingText))
    {
        const std::string trimmed = TrimAscii(parsedDyingText);
        if (!trimmed.empty())
        {
            g_config.dyingText = trimmed;
        }
        else
        {
            LogWarn("dying_text is empty; using default");
        }
    }

    std::string parsedRecoveryComaText;
    if (ParseStringFromJson(body, "recovery_coma_text", &parsedRecoveryComaText))
    {
        const std::string trimmed = TrimAscii(parsedRecoveryComaText);
        if (!trimmed.empty())
        {
            g_config.recoveryComaText = trimmed;
        }
        else
        {
            LogWarn("recovery_coma_text is empty; using default");
        }
    }

    std::string parsedPlayingDeadText;
    if (ParseStringFromJson(body, "playing_dead_text", &parsedPlayingDeadText))
    {
        const std::string trimmed = TrimAscii(parsedPlayingDeadText);
        if (!trimmed.empty())
        {
            g_config.playingDeadText = trimmed;
        }
        else
        {
            LogWarn("playing_dead_text is empty; using default");
        }
    }

    std::string parsedDeadText;
    if (ParseStringFromJson(body, "dead_text", &parsedDeadText))
    {
        const std::string trimmed = TrimAscii(parsedDeadText);
        if (!trimmed.empty())
        {
            g_config.deadText = trimmed;
        }
        else
        {
            LogWarn("dead_text is empty; using default");
        }
    }

    DWORD parsedUnconsciousTextSize = 0;
    if (ParseUnsignedFromJson(body, "unconscious_text_size_px", &parsedUnconsciousTextSize))
    {
        if (parsedUnconsciousTextSize == 0)
        {
            g_config.unconsciousTextSizePx = kDefaultMarkerTextSizePx;
            LogWarn("unconscious_text_size_px=0 deprecated; using default 18");
        }
        else if (parsedUnconsciousTextSize < 8)
        {
            g_config.unconsciousTextSizePx = 8;
            LogWarn("unconscious_text_size_px too low; clamped to 8");
        }
        else if (parsedUnconsciousTextSize > 128)
        {
            g_config.unconsciousTextSizePx = 128;
            LogWarn("unconscious_text_size_px too high; clamped to 128");
        }
        else
        {
            g_config.unconsciousTextSizePx = parsedUnconsciousTextSize;
        }
    }

    DWORD parsedDyingTextSize = 0;
    if (ParseUnsignedFromJson(body, "dying_text_size_px", &parsedDyingTextSize))
    {
        if (parsedDyingTextSize == 0)
        {
            g_config.dyingTextSizePx = kDefaultMarkerTextSizePx;
            LogWarn("dying_text_size_px=0 deprecated; using default 18");
        }
        else if (parsedDyingTextSize < 8)
        {
            g_config.dyingTextSizePx = 8;
            LogWarn("dying_text_size_px too low; clamped to 8");
        }
        else if (parsedDyingTextSize > 128)
        {
            g_config.dyingTextSizePx = 128;
            LogWarn("dying_text_size_px too high; clamped to 128");
        }
        else
        {
            g_config.dyingTextSizePx = parsedDyingTextSize;
        }
    }

    DWORD parsedRecoveryComaTextSize = 0;
    if (ParseUnsignedFromJson(body, "recovery_coma_text_size_px", &parsedRecoveryComaTextSize))
    {
        if (parsedRecoveryComaTextSize == 0)
        {
            g_config.recoveryComaTextSizePx = kDefaultMarkerTextSizePx;
            LogWarn("recovery_coma_text_size_px=0 deprecated; using default 18");
        }
        else if (parsedRecoveryComaTextSize < 8)
        {
            g_config.recoveryComaTextSizePx = 8;
            LogWarn("recovery_coma_text_size_px too low; clamped to 8");
        }
        else if (parsedRecoveryComaTextSize > 128)
        {
            g_config.recoveryComaTextSizePx = 128;
            LogWarn("recovery_coma_text_size_px too high; clamped to 128");
        }
        else
        {
            g_config.recoveryComaTextSizePx = parsedRecoveryComaTextSize;
        }
    }

    DWORD parsedPlayingDeadTextSize = 0;
    if (ParseUnsignedFromJson(body, "playing_dead_text_size_px", &parsedPlayingDeadTextSize))
    {
        if (parsedPlayingDeadTextSize == 0)
        {
            g_config.playingDeadTextSizePx = kDefaultMarkerTextSizePx;
            LogWarn("playing_dead_text_size_px=0 deprecated; using default 18");
        }
        else if (parsedPlayingDeadTextSize < 8)
        {
            g_config.playingDeadTextSizePx = 8;
            LogWarn("playing_dead_text_size_px too low; clamped to 8");
        }
        else if (parsedPlayingDeadTextSize > 128)
        {
            g_config.playingDeadTextSizePx = 128;
            LogWarn("playing_dead_text_size_px too high; clamped to 128");
        }
        else
        {
            g_config.playingDeadTextSizePx = parsedPlayingDeadTextSize;
        }
    }

    DWORD parsedDeadTextSize = 0;
    if (ParseUnsignedFromJson(body, "dead_text_size_px", &parsedDeadTextSize))
    {
        if (parsedDeadTextSize == 0)
        {
            g_config.deadTextSizePx = kDefaultMarkerTextSizePx;
            LogWarn("dead_text_size_px=0 deprecated; using default 18");
        }
        else if (parsedDeadTextSize < 8)
        {
            g_config.deadTextSizePx = 8;
            LogWarn("dead_text_size_px too low; clamped to 8");
        }
        else if (parsedDeadTextSize > 128)
        {
            g_config.deadTextSizePx = 128;
            LogWarn("dead_text_size_px too high; clamped to 128");
        }
        else
        {
            g_config.deadTextSizePx = parsedDeadTextSize;
        }
    }

    std::string parsedEnemyColorHex;
    if (ParseStringFromJson(body, "enemy_color_hex", &parsedEnemyColorHex))
    {
        MyGUI::Colour parsed = g_config.enemyMarkerColour;
        if (TryParseColourHex(parsedEnemyColorHex, &parsed))
        {
            g_config.enemyMarkerColour = parsed;
        }
        else
        {
            LogWarn("enemy_color_hex invalid; expected #RRGGBB or #RRGGBBAA; using default");
        }
    }

    std::string parsedAllyColorHex;
    if (ParseStringFromJson(body, "ally_color_hex", &parsedAllyColorHex))
    {
        MyGUI::Colour parsed = g_config.allyMarkerColour;
        if (TryParseColourHex(parsedAllyColorHex, &parsed))
        {
            g_config.allyMarkerColour = parsed;
        }
        else
        {
            LogWarn("ally_color_hex invalid; expected #RRGGBB or #RRGGBBAA; using default");
        }
    }

    std::string parsedSquadColorHex;
    if (ParseStringFromJson(body, "squad_color_hex", &parsedSquadColorHex))
    {
        MyGUI::Colour parsed = g_config.squadMarkerColour;
        if (TryParseColourHex(parsedSquadColorHex, &parsed))
        {
            g_config.squadMarkerColour = parsed;
        }
        else
        {
            LogWarn("squad_color_hex invalid; expected #RRGGBB or #RRGGBBAA; using default");
        }
    }

    std::string parsedUnconsciousIconTexture;
    if (ParseStringFromJson(body, "unconscious_icon_texture", &parsedUnconsciousIconTexture))
    {
        g_config.customUnconsciousIconTexture = TrimAscii(parsedUnconsciousIconTexture);
    }

    DWORD parsedUnconsciousIconSize = 0;
    if (ParseUnsignedFromJson(body, "unconscious_icon_size_px", &parsedUnconsciousIconSize))
    {
        if (parsedUnconsciousIconSize < 8)
        {
            g_config.customUnconsciousIconSizePx = 8;
            LogWarn("unconscious_icon_size_px too low; clamped to 8");
        }
        else if (parsedUnconsciousIconSize > 512)
        {
            g_config.customUnconsciousIconSizePx = 512;
            LogWarn("unconscious_icon_size_px too high; clamped to 512");
        }
        else
        {
            g_config.customUnconsciousIconSizePx = parsedUnconsciousIconSize;
        }
    }

    std::string parsedRecoveryComaIconTexture;
    if (ParseStringFromJson(body, "recovery_coma_icon_texture", &parsedRecoveryComaIconTexture))
    {
        g_config.customRecoveryComaIconTexture = TrimAscii(parsedRecoveryComaIconTexture);
    }

    DWORD parsedRecoveryComaIconSize = 0;
    if (ParseUnsignedFromJson(body, "recovery_coma_icon_size_px", &parsedRecoveryComaIconSize))
    {
        if (parsedRecoveryComaIconSize < 8)
        {
            g_config.customRecoveryComaIconSizePx = 8;
            LogWarn("recovery_coma_icon_size_px too low; clamped to 8");
        }
        else if (parsedRecoveryComaIconSize > 512)
        {
            g_config.customRecoveryComaIconSizePx = 512;
            LogWarn("recovery_coma_icon_size_px too high; clamped to 512");
        }
        else
        {
            g_config.customRecoveryComaIconSizePx = parsedRecoveryComaIconSize;
        }
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

    std::string parsedPlayingDeadIconTexture;
    if (ParseStringFromJson(body, "playing_dead_icon_texture", &parsedPlayingDeadIconTexture))
    {
        g_config.customPlayingDeadIconTexture = TrimAscii(parsedPlayingDeadIconTexture);
    }

    DWORD parsedPlayingDeadIconSize = 0;
    if (ParseUnsignedFromJson(body, "playing_dead_icon_size_px", &parsedPlayingDeadIconSize))
    {
        if (parsedPlayingDeadIconSize < 8)
        {
            g_config.customPlayingDeadIconSizePx = 8;
            LogWarn("playing_dead_icon_size_px too low; clamped to 8");
        }
        else if (parsedPlayingDeadIconSize > 512)
        {
            g_config.customPlayingDeadIconSizePx = 512;
            LogWarn("playing_dead_icon_size_px too high; clamped to 512");
        }
        else
        {
            g_config.customPlayingDeadIconSizePx = parsedPlayingDeadIconSize;
        }
    }

    std::string parsedDeadIconTexture;
    if (ParseStringFromJson(body, "dead_icon_texture", &parsedDeadIconTexture))
    {
        g_config.customDeadIconTexture = TrimAscii(parsedDeadIconTexture);
    }

    DWORD parsedDeadIconSize = 0;
    if (ParseUnsignedFromJson(body, "dead_icon_size_px", &parsedDeadIconSize))
    {
        if (parsedDeadIconSize < 8)
        {
            g_config.customDeadIconSizePx = 8;
            LogWarn("dead_icon_size_px too low; clamped to 8");
        }
        else if (parsedDeadIconSize > 512)
        {
            g_config.customDeadIconSizePx = 512;
            LogWarn("dead_icon_size_px too high; clamped to 512");
        }
        else
        {
            g_config.customDeadIconSizePx = parsedDeadIconSize;
        }
    }

    if (!g_config.customUnconsciousIconTexture.empty())
    {
        std::stringstream iconInfo;
        iconInfo << "custom ZZ icon configured texture=" << g_config.customUnconsciousIconTexture
            << " size=" << g_config.customUnconsciousIconSizePx;
        LogInfo(iconInfo.str());
    }

    if (!g_config.customDyingIconTexture.empty())
    {
        std::stringstream iconInfo;
        iconInfo << "custom DY icon configured texture=" << g_config.customDyingIconTexture
            << " size=" << g_config.customDyingIconSizePx;
        LogInfo(iconInfo.str());
    }

    if (!g_config.customRecoveryComaIconTexture.empty())
    {
        std::stringstream iconInfo;
        iconInfo << "custom RC icon configured texture=" << g_config.customRecoveryComaIconTexture
            << " size=" << g_config.customRecoveryComaIconSizePx;
        LogInfo(iconInfo.str());
    }

    if (!g_config.customPlayingDeadIconTexture.empty())
    {
        std::stringstream iconInfo;
        iconInfo << "custom PD icon configured texture=" << g_config.customPlayingDeadIconTexture
            << " size=" << g_config.customPlayingDeadIconSizePx;
        LogInfo(iconInfo.str());
    }

    if (!g_config.customDeadIconTexture.empty())
    {
        std::stringstream iconInfo;
        iconInfo << "custom DE icon configured texture=" << g_config.customDeadIconTexture
            << " size=" << g_config.customDeadIconSizePx;
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

void ApplyKoMarkerVisualState(KoMarkerWidget& marker, int markerState, int markerRelation)
{
    const MyGUI::Colour colour = ResolveMarkerColour(markerState, markerRelation);
    const MyGUI::Colour beaconColour(colour.red, colour.green, colour.blue, kKoBeaconAlpha);
    const bool wantsCustomIcon = HasCustomMarkerIcon(markerState);
    const std::string* customTexture = ResolveCustomMarkerIconTexture(markerState);
    const int customIconSize = static_cast<int>(ResolveCustomMarkerIconSizePx(markerState));

    if (marker.beacon && kEnableUiBeaconOverlay)
    {
        bool customIconReady = false;
        if (wantsCustomIcon && customTexture)
        {
            customIconReady = SetKoMarkerIconTexture(marker.beacon, customTexture->c_str());
            if (customIconReady)
            {
                SetKoMarkerIconCoord(marker.beacon, ResolveCustomIconCoordFromImageSize(marker.beacon, customIconSize));
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

    if (marker.fallbackText && g_config.showMarkerText)
    {
        const int fontHeight = static_cast<int>(ResolveMarkerTextSizePx(markerState));
        SetKoMarkerTextFontHeight(marker.fallbackText, fontHeight);
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

    bool isDead = false;
    bool isUnconscious = false;
    bool isRecoveryComa = false;
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
    bool recoveryComaBySub50Ko = false;
    bool recoveryComaByCannotWake = false;
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
        debugInfoOut->isRecoveryComa = false;
        debugInfoOut->isPlayingDead = false;
        debugInfoOut->isLiteral = false;
        debugInfoOut->isProbablyDying = false;
        debugInfoOut->dyingByProbablyLiteral = false;
        debugInfoOut->dyingByProbablyLowBlood = false;
        debugInfoOut->dyingByProbablySub50Ko = false;
        debugInfoOut->dyingByActiveBleed = false;
        debugInfoOut->dyingByTrauma = false;
        debugInfoOut->dyingByBloodThreshold = false;
        debugInfoOut->recoveryComaBySub50Ko = false;
        debugInfoOut->recoveryComaByCannotWake = false;
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
        const bool isDeadByCharacter = candidate->isDead();
        const bool isDeadByMedicalMethod = candidate->medical.isDead();
        const bool isDeadByMedicalFlag = candidate->medical.dead;
        isDead = (isDeadByCharacter || isDeadByMedicalMethod || isDeadByMedicalFlag);
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
            recoveryComaBySub50Ko = medicalSub50KoFlag;
            recoveryComaByCannotWake = (!candidate->medical.canGetUpWakeUp() && medicalSub50KoFlag);

            StateBroadcastData* stateBroadcast = candidate->getStateBroadcast();
            if (stateBroadcast)
            {
                sleepState = stateBroadcast->sleepState;
                hasSleepState = true;
                slaveState = static_cast<int>(stateBroadcast->slaveState);
                hasSlaveState = true;
            }

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

    if (debugInfoOut)
    {
        debugInfoOut->isUnconscious = isUnconscious;
        debugInfoOut->isRecoveryComa = isRecoveryComa;
        debugInfoOut->isPlayingDead = isPlayingDead;
        debugInfoOut->isLiteral = isLiteral;
        debugInfoOut->isProbablyDying = isProbablyDying;
        debugInfoOut->dyingByProbablyLiteral = dyingByProbablyLiteral;
        debugInfoOut->dyingByProbablyLowBlood = dyingByProbablyLowBlood;
        debugInfoOut->dyingByProbablySub50Ko = dyingByProbablySub50Ko;
        debugInfoOut->dyingByActiveBleed = dyingByActiveBleed;
        debugInfoOut->dyingByTrauma = dyingByTrauma;
        debugInfoOut->dyingByBloodThreshold = dyingByBloodThreshold;
        debugInfoOut->recoveryComaBySub50Ko = recoveryComaBySub50Ko;
        debugInfoOut->recoveryComaByCannotWake = recoveryComaByCannotWake;
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

bool IsMarkerStateEnabled(int markerState)
{
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

void TickKoProbe()
{
    const bool anyMarkerVisualEnabled = (g_config.showMarkerIcons || g_config.showMarkerText);
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
    LogSelectionSnapshot(nowMs);

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
    bool anySelectedMarkerMatched = false;

    const auto processMarkerCandidate = [&](Character* candidate)
    {
        if (!candidate)
        {
            return;
        }

        bool candidateValid = false;
        __try
        {
            candidateValid = candidate->isValid();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return;
        }
        if (!candidateValid)
        {
            return;
        }

        int markerState = CachedKoTarget::STATE_UNCONSCIOUS;
        MarkerStateDebugInfo markerDebugInfo = {};
        if (!TryResolveMarkerState(candidate, &markerState, &markerDebugInfo))
        {
            return;
        }

        bool isOnScreen = false;
        Ogre::Vector3 candidatePos;
        hand targetHandle;
        __try
        {
            isOnScreen = candidate->isOnScreen;
            candidatePos = candidate->getPosition();
            targetHandle = candidate->getHandle();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
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
            if (!g_projectionUtility)
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

        int markerRelation = CachedKoTarget::RELATION_ENEMY;
        __try
        {
            markerRelation = ResolveMarkerRelation(candidate);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            markerRelation = CachedKoTarget::RELATION_ENEMY;
        }

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
            existing.worldPos = candidatePos;
            existing.lastSeenMs = nowMs;
            existing.markerState = markerState;
            existing.markerRelation = markerRelation;
        }
        else
        {
            CachedKoTarget created = {
                targetHandle,
                candidatePos,
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
    };

    for (auto iter = activeCharacters.begin(); iter != activeCharacters.end(); ++iter)
    {
        processMarkerCandidate(*iter);
    }

    for (auto iter = deathParadeCharacters.begin(); iter != deathParadeCharacters.end(); ++iter)
    {
        Character* deathParadeCandidate = 0;
        __try
        {
            deathParadeCandidate = ou->getFromDeathParade(iter->first);
            if (!deathParadeCandidate)
            {
                deathParadeCandidate = iter->second;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            deathParadeCandidate = 0;
        }
        processMarkerCandidate(deathParadeCandidate);
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
         << ", enable_unconscious=" << (g_config.enableUnconsciousState ? "true" : "false")
         << ", enable_recovery_coma=" << (g_config.enableRecoveryComaState ? "true" : "false")
         << ", enable_dying=" << (g_config.enableDyingState ? "true" : "false")
         << ", enable_playing_dead=" << (g_config.enablePlayingDeadState ? "true" : "false")
         << ", enable_dead=" << (g_config.enableDeadState ? "true" : "false")
         << ", show_icons=" << (g_config.showMarkerIcons ? "true" : "false")
         << ", show_text=" << (g_config.showMarkerText ? "true" : "false")
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
