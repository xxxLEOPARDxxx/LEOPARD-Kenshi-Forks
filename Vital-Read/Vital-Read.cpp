#include <Debug.h>

#include "vr_config.h"
#include "vr_marker_ui.h"

#include <core/Functions.h>
#include <emc/mod_hub_client.h>
#include <emc/mod_hub_consumer_helpers.h>
#include <kenshi/Character.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/InputHandler.h>
#include <kenshi/Kenshi.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/Platoon.h>

#include <mygui/MyGUI_EditBox.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_RenderManager.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>

#include <ois/OISKeyboard.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace
{
const char* kPluginName = "Vital-Read";
const char* kProbeMarkerWidgetName = "VitalRead_Phase1HoveredPortraitMarker";
const char* kUnconsciousOverlayWidgetNamePrefix = "VitalRead_UnconsciousOverlayMarker_";
const char* kRecoveryComaOverlayWidgetNamePrefix = "VitalRead_RecoveryComaOverlayMarker_";
const char* kDyingOverlayWidgetNamePrefix = "VitalRead_DyingOverlayMarker_";
const char* kProbeMarkerSkin = "Kenshi_GenericTextBoxFlatSkin";
const char* kUnconsciousOverlaySkin = "Kenshi_Button1";

const OIS::KeyCode kRunMappingProbeHotkey = OIS::KC_F4;
const OIS::KeyCode kProbeNewSessionHotkey = OIS::KC_F6;
const OIS::KeyCode kMarkStateMatchedPortraitHotkey = OIS::KC_F5;
const OIS::KeyCode kDumpHoveredWidgetHotkey = OIS::KC_F7;
const OIS::KeyCode kDumpPortraitBarTreeHotkey = OIS::KC_F8;
const OIS::KeyCode kDumpPortraitCandidatesHotkey = OIS::KC_F9;
const OIS::KeyCode kMarkHoveredPortraitHotkey = OIS::KC_F10;
const OIS::KeyCode kDumpSelectedSquadMembersHotkey = OIS::KC_F11;
const OIS::KeyCode kDumpMemberStatesHotkey = OIS::KC_F12;

const DWORD kHoveredMarkerLifetimeMs = 1500;
const DWORD kPortraitOverlayRefreshIntervalMs = 250;
const int kHoveredMarkerInsetPx = 2;
const int kHoveredMarkerMinSizePx = 10;
const int kHoveredMarkerMaxSizePx = 18;
const int kPortraitOverlayInsetPx = 1;
const int kPortraitOverlayMinSizePx = 14;
const int kPortraitOverlayMaxSizePx = 20;
const size_t kHoveredChainDepthLimit = 8u;
const size_t kPortraitTreeDepthLimit = 4u;
const size_t kPortraitTreeNodeLimit = 160u;
const size_t kPortraitCandidateScanLimit = 512u;
const size_t kPortraitCandidateLogLimit = 24u;
const size_t kHoveredPortraitDescendantDepthLimit = 3u;
const size_t kHoveredPortraitDescendantNodeLimit = 24u;
const float kPortraitCandidateLogThreshold = 0.20f;
const float kHoveredPortraitMarkThreshold = 0.25f;

void (*PlayerInterface_updateUT_orig)(PlayerInterface*) = 0;
void (*InputHandler_keyDownEvent_orig)(InputHandler*, OIS::KeyCode) = 0;

std::string g_configPath;
vr_config::PluginConfig g_config;
bool& g_enabled = g_config.enabled;
bool& g_debugLogging = g_config.debugLogging;
bool& g_debugSearchLogging = g_config.debugSearchLogging;
bool& g_debugBindingLogging = g_config.debugBindingLogging;
std::string& g_unconsciousIconTexture = g_config.unconsciousIconTexture;
DWORD& g_unconsciousIconSizePx = g_config.unconsciousIconSizePx;
std::string& g_recoveryComaIconTexture = g_config.recoveryComaIconTexture;
DWORD& g_recoveryComaIconSizePx = g_config.recoveryComaIconSizePx;
std::string& g_dyingIconTexture = g_config.dyingIconTexture;
DWORD& g_dyingIconSizePx = g_config.dyingIconSizePx;
unsigned int g_probeLogScopeDepth = 0u;
unsigned int g_nextProbeSessionId = 1u;
unsigned int g_activeProbeSessionId = 0u;
unsigned int g_activeProbeSequence = 0u;
DWORD g_hoveredMarkerExpireTick = 0u;
MyGUI::Widget* g_hoveredMarkerWidget = 0;

struct PortraitOverlayRuntime
{
    PortraitOverlayRuntime()
        : nextRefreshTick(0u)
    {
    }

    DWORD nextRefreshTick;
    std::vector<MyGUI::Widget*> widgets;
};

PortraitOverlayRuntime g_unconsciousOverlayRuntime;
PortraitOverlayRuntime g_recoveryComaOverlayRuntime;
PortraitOverlayRuntime g_dyingOverlayRuntime;

struct HoverContext
{
    HoverContext()
        : available(false)
        , hoveredWidget(0)
        , mouse(0, 0)
    {
    }

    bool available;
    MyGUI::Widget* hoveredWidget;
    MyGUI::IntPoint mouse;
};

struct PortraitCandidateRecord
{
    PortraitCandidateRecord()
        : widget(0)
        , parent(0)
        , absoluteCoord(0, 0, 0, 0)
        , score(0.0f)
        , visible(false)
        , inheritedVisible(false)
        , childCount(0u)
    {
    }

    MyGUI::Widget* widget;
    MyGUI::Widget* parent;
    MyGUI::IntCoord absoluteCoord;
    float score;
    std::string reason;
    std::string typeName;
    std::string name;
    std::string caption;
    bool visible;
    bool inheritedVisible;
    size_t childCount;
};

struct SquadProbeScope
{
    SquadProbeScope()
        : player(0)
        , selectedCharacter(0)
        , targetPlatoon(0)
        , targetActivePlatoon(0)
        , allPlayerCharacters(0)
        , scope("current_platoon")
    {
    }

    PlayerInterface* player;
    Character* selectedCharacter;
    Platoon* targetPlatoon;
    ActivePlatoon* targetActivePlatoon;
    const lektor<Character*>* allPlayerCharacters;
    const char* scope;
};

struct MemberStateSnapshot
{
    MemberStateSnapshot()
        : proneState(PS_NORMAL)
        , dead(false)
        , unconscious(false)
        , playingDead(false)
        , dying(false)
        , recoveryComa(false)
        , medicalUnconcious(false)
        , koProne(false)
        , probablyDying(false)
        , bloodlossTrauma(false)
        , sub50KO(false)
        , canGetUpWakeUp(false)
        , blood(0.0f)
        , pointOfNoReturn(0.0f)
        , currentBleedRate(0.0f)
        , extraBloodLoss(0.0f)
        , knockoutTimer(0.0f)
        , stateLabel("awake")
    {
    }

    ProneState proneState;
    bool dead;
    bool unconscious;
    bool playingDead;
    bool dying;
    bool recoveryComa;
    bool medicalUnconcious;
    bool koProne;
    bool probablyDying;
    bool bloodlossTrauma;
    bool sub50KO;
    bool canGetUpWakeUp;
    float blood;
    float pointOfNoReturn;
    float currentBleedRate;
    float extraBloodLoss;
    float knockoutTimer;
    const char* stateLabel;
};

enum PortraitOverlayState
{
    PORTRAIT_OVERLAY_STATE_STRICT_UNCONSCIOUS = 0,
    PORTRAIT_OVERLAY_STATE_RECOVERY_COMA,
    PORTRAIT_OVERLAY_STATE_DYING
};

enum PortraitOverlayMatchStatus
{
    PORTRAIT_OVERLAY_MATCH_OK = 0,
    PORTRAIT_OVERLAY_MATCH_NO_PLAYER_INTERFACE,
    PORTRAIT_OVERLAY_MATCH_NO_STATE,
    PORTRAIT_OVERLAY_MATCH_NO_GUI,
    PORTRAIT_OVERLAY_MATCH_NO_PORTRAIT
};

struct PortraitOverlayMatch
{
    PortraitOverlayMatch()
        : overlayState(PORTRAIT_OVERLAY_STATE_STRICT_UNCONSCIOUS)
        , status(PORTRAIT_OVERLAY_MATCH_NO_PLAYER_INTERFACE)
        , character(0)
        , displaySlotIndex(-1)
        , mappingKey("unresolved")
        , displayPortraitCount(0u)
        , visibleRootCount(0u)
        , scannedNodes(0u)
    {
    }

    PortraitOverlayState overlayState;
    PortraitOverlayMatchStatus status;
    Character* character;
    MemberStateSnapshot state;
    PortraitCandidateRecord target;
    int displaySlotIndex;
    const char* mappingKey;
    size_t displayPortraitCount;
    size_t visibleRootCount;
    size_t scannedNodes;
};

bool EnsureHoveredMarkerWidget();
void HideHoveredMarker();
void ResetStateOverlays();

bool IsSupportedVersion(KenshiLib::BinaryVersion& versionInfo)
{
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    return platform != KenshiLib::BinaryVersion::UNKNOWN
        && (version == "1.0.65" || version == "1.0.68");
}

void LogInfoLine(const std::string& message)
{
    std::stringstream line;
    line << kPluginName << " INFO: " << message;
    DebugLog(line.str().c_str());
}

void LogWarnLine(const std::string& message)
{
    std::stringstream line;
    line << kPluginName << " WARN: " << message;
    ErrorLog(line.str().c_str());
}

void LogErrorLine(const std::string& message)
{
    std::stringstream line;
    line << kPluginName << " ERROR: " << message;
    ErrorLog(line.str().c_str());
}

bool ShouldCompileVerboseDiagnostics()
{
#if defined(PLUGIN_ENABLE_VERBOSE_DIAGNOSTICS)
    return true;
#else
    return false;
#endif
}

bool ShouldLogDebug()
{
    return g_debugLogging;
}

bool ShouldLogSearchDebug()
{
    return g_debugLogging && g_debugSearchLogging;
}

bool ShouldLogBindingDebug()
{
    return g_debugLogging && g_debugBindingLogging;
}

void LogDebugLine(const std::string& message)
{
    if (ShouldLogDebug())
    {
        LogInfoLine(message);
    }
}

void LogSearchDebugLine(const std::string& message)
{
    if (ShouldLogSearchDebug())
    {
        LogInfoLine(message);
    }
}

void LogBindingDebugLine(const std::string& message)
{
    if (ShouldLogBindingDebug())
    {
        LogInfoLine(message);
    }
}

bool TryResolveModConfigPath(std::string* outPath)
{
    if (outPath == 0 || g_configPath.empty())
    {
        return false;
    }

    *outPath = g_configPath;
    return true;
}

void LoadLoggingConfig()
{
    g_config = vr_config::PluginConfig();

    std::string configPath;
    if (!TryResolveModConfigPath(&configPath))
    {
        LogWarnLine("mod config load skipped: could not resolve plugin directory (using quiet logging defaults)");
        return;
    }

    if (vr_config::LoadFromFile(configPath, &g_config) != vr_config::LOAD_OK)
    {
        std::stringstream line;
        line << "mod config load skipped: could not read " << configPath
             << " (using quiet logging defaults)";
        LogWarnLine(line.str());
        return;
    }

    LogInfoLine("mod config loaded");

    if (ShouldLogDebug())
    {
        std::stringstream line;
        line << "logging flags enabled=" << (g_enabled ? "true" : "false")
             << " debugLogging=" << (g_debugLogging ? "true" : "false")
             << " debugSearchLogging=" << (g_debugSearchLogging ? "true" : "false")
             << " debugBindingLogging=" << (g_debugBindingLogging ? "true" : "false")
             << " unconsciousIconConfigured=" << (!g_unconsciousIconTexture.empty() ? "true" : "false")
             << " unconsciousIconSizePx=" << g_unconsciousIconSizePx
             << " recoveryComaIconConfigured=" << (!g_recoveryComaIconTexture.empty() ? "true" : "false")
             << " recoveryComaIconSizePx=" << g_recoveryComaIconSizePx
             << " dyingIconConfigured=" << (!g_dyingIconTexture.empty() ? "true" : "false")
             << " dyingIconSizePx=" << g_dyingIconSizePx
             << " verboseDiagnostics=" << (ShouldCompileVerboseDiagnostics() ? "true" : "false");
        LogDebugLine(line.str());
    }
}

bool SaveConfigState()
{
    std::string configPath;
    if (!TryResolveModConfigPath(&configPath))
    {
        LogErrorLine("settings path is empty; cannot save mod-config.json");
        return false;
    }

    if (!vr_config::SaveToFile(configPath, g_config))
    {
        std::stringstream line;
        line << "failed to save mod-config.json at " << configPath;
        LogErrorLine(line.str());
        return false;
    }

    return true;
}

std::string ToLowerAscii(const std::string& value)
{
    std::string lowered(value);
    for (std::string::size_type index = 0; index < lowered.size(); ++index)
    {
        lowered[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[index])));
    }
    return lowered;
}

bool ContainsAsciiCaseInsensitive(const std::string& haystack, const char* needle)
{
    if (needle == 0 || *needle == '\0')
    {
        return false;
    }

    return ToLowerAscii(haystack).find(ToLowerAscii(needle)) != std::string::npos;
}

int ClampInt(int value, int minimum, int maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

float ClampUnitFloat(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    if (value > 1.0f)
    {
        return 1.0f;
    }
    return value;
}

int AbsoluteInt(int value)
{
    return value < 0 ? -value : value;
}

std::string EscapeForLog(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size() + 8u);

    for (std::string::size_type index = 0; index < value.size(); ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(value[index]);
        switch (ch)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\r':
        case '\n':
        case '\t':
            escaped.push_back(' ');
            break;
        default:
            if (ch >= 32u)
            {
                escaped.push_back(static_cast<char>(ch));
            }
            break;
        }
    }

    return escaped;
}

std::string QuoteForLog(const std::string& value)
{
    return std::string("\"") + EscapeForLog(value) + "\"";
}

std::string FormatBool(bool value)
{
    return value ? "true" : "false";
}

std::string FormatPointer(const void* pointer)
{
    std::stringstream line;
    line << "0x" << std::hex << std::uppercase << reinterpret_cast<size_t>(pointer);
    return line.str();
}

std::string FormatCoord(const MyGUI::IntCoord& coord)
{
    std::stringstream line;
    line << "(" << coord.left
         << "," << coord.top
         << "," << coord.width
         << "," << coord.height << ")";
    return line.str();
}

std::string FormatPoint(const MyGUI::IntPoint& point)
{
    std::stringstream line;
    line << "(" << point.left << "," << point.top << ")";
    return line.str();
}

std::string FormatFloat2(float value)
{
    std::ostringstream line;
    line << std::fixed << std::setprecision(2) << value;
    return line.str();
}

std::string SafeCharacterName(Character* character)
{
    if (character == 0)
    {
        return "";
    }

    if (!character->displayName.empty())
    {
        return character->displayName;
    }

    return character->getName();
}

std::string SafePlatoonName(Platoon* platoon)
{
    if (platoon == 0)
    {
        return "";
    }

    return platoon->getName();
}

std::string SafeHandleString(const hand& value)
{
    if (!value.isValid())
    {
        return "";
    }

    return value.toString();
}

bool TryIsCharacterSelected(PlayerInterface* player, Character* character)
{
    if (player == 0 || character == 0)
    {
        return false;
    }

    return player->isObjectSelected(character);
}

bool TryResolveSquadProbeScope(SquadProbeScope* outScope)
{
    if (outScope == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    SquadProbeScope scope;
    scope.player = ou->player;
    scope.selectedCharacter = scope.player->selectedCharacter.getCharacter();
    scope.targetPlatoon = scope.player->getCurrentPlatoon();
    scope.targetActivePlatoon = scope.targetPlatoon == 0 ? 0 : scope.targetPlatoon->activePlatoon;
    scope.scope = "current_platoon";

    if (scope.targetPlatoon == 0 && scope.selectedCharacter != 0 && scope.selectedCharacter->platoon != 0)
    {
        scope.targetActivePlatoon = scope.selectedCharacter->platoon;
        scope.targetPlatoon = scope.targetActivePlatoon == 0 ? 0 : scope.targetActivePlatoon->me;
        scope.scope = "selected_character_platoon";
    }

    if (scope.targetPlatoon == 0)
    {
        scope.scope = "all_player_characters";
    }

    scope.allPlayerCharacters = &scope.player->getAllPlayerCharacters();
    *outScope = scope;
    return true;
}

bool CharacterMatchesSquadProbeScope(const SquadProbeScope& scope, Character* candidate)
{
    if (candidate == 0)
    {
        return false;
    }

    Platoon* candidatePlatoon = candidate->platoon == 0 ? 0 : candidate->platoon->me;
    return scope.targetPlatoon == 0 || candidatePlatoon == scope.targetPlatoon;
}

size_t CountScopedSquadMembers(const SquadProbeScope& scope, int* selectedScopeIndexOut)
{
    if (selectedScopeIndexOut != 0)
    {
        *selectedScopeIndexOut = -1;
    }

    if (scope.allPlayerCharacters == 0)
    {
        return 0u;
    }

    size_t scopedCount = 0u;
    for (uint32_t rawIndex = 0u; rawIndex < scope.allPlayerCharacters->size(); ++rawIndex)
    {
        Character* candidate = (*scope.allPlayerCharacters)[rawIndex];
        if (!CharacterMatchesSquadProbeScope(scope, candidate))
        {
            continue;
        }

        if (selectedScopeIndexOut != 0 && candidate == scope.selectedCharacter)
        {
            *selectedScopeIndexOut = static_cast<int>(scopedCount);
        }

        ++scopedCount;
    }

    return scopedCount;
}

bool TryResolveMemberStateSnapshot(Character* candidate, MemberStateSnapshot* outSnapshot)
{
    if (candidate == 0 || outSnapshot == 0)
    {
        return false;
    }

    MemberStateSnapshot snapshot;
    snapshot.proneState = candidate->_currentProneState;
    snapshot.dead = candidate->isDead() || candidate->medical.isDead() || candidate->medical.dead;
    snapshot.medicalUnconcious = candidate->medical.unconcious;
    snapshot.koProne = (snapshot.proneState == PS_KO);
    snapshot.unconscious = candidate->isUnconcious() || snapshot.medicalUnconcious || snapshot.koProne;
    snapshot.playingDead = (snapshot.proneState == PS_PLAYING_DEAD);
    snapshot.probablyDying = candidate->medical.isProbablyDying();
    snapshot.bloodlossTrauma = candidate->medical.isInBloodlossTrauma();
    snapshot.sub50KO = candidate->medical.sub50KO;
    snapshot.blood = candidate->medical.blood;
    snapshot.pointOfNoReturn = candidate->medical.pointOfNoReturn();
    snapshot.currentBleedRate = candidate->medical.currentBleedRate;
    snapshot.extraBloodLoss = candidate->medical.extraBloodLossFromBodyparts;
    snapshot.knockoutTimer = candidate->medical.knockoutTimer;
    snapshot.canGetUpWakeUp = candidate->medical.canGetUpWakeUp();

    const bool dyingByBloodThreshold = (snapshot.blood <= snapshot.pointOfNoReturn);
    const bool dyingByActiveBleed =
        snapshot.probablyDying
        && (snapshot.currentBleedRate > 0.0f || snapshot.extraBloodLoss > 0.0f);
    snapshot.recoveryComa =
        !snapshot.canGetUpWakeUp
        && snapshot.sub50KO
        && snapshot.knockoutTimer <= 0.0f
        && !snapshot.probablyDying
        && !dyingByBloodThreshold
        && !snapshot.bloodlossTrauma
        && !dyingByActiveBleed;

    snapshot.dying =
        !snapshot.dead
        && snapshot.unconscious
        && !snapshot.playingDead
        && !snapshot.recoveryComa
        && (dyingByBloodThreshold || snapshot.sub50KO);

    if (snapshot.dead)
    {
        snapshot.stateLabel = "dead";
    }
    else if (snapshot.dying)
    {
        snapshot.stateLabel = "dying";
    }
    else if (snapshot.playingDead)
    {
        snapshot.stateLabel = "playing_dead";
    }
    else if (snapshot.unconscious)
    {
        snapshot.stateLabel = snapshot.recoveryComa ? "recovery_coma" : "unconscious";
    }

    *outSnapshot = snapshot;
    return true;
}

const char* GetPortraitOverlayStateLabel(const PortraitOverlayState overlayState)
{
    switch (overlayState)
    {
    case PORTRAIT_OVERLAY_STATE_DYING:
        return "dying";
    case PORTRAIT_OVERLAY_STATE_RECOVERY_COMA:
        return "recovery_coma";
    case PORTRAIT_OVERLAY_STATE_STRICT_UNCONSCIOUS:
    default:
        return "unconscious";
    }
}

bool SnapshotMatchesPortraitOverlayState(
    const MemberStateSnapshot& snapshot,
    const PortraitOverlayState overlayState)
{
    if (snapshot.dead || snapshot.playingDead)
    {
        return false;
    }

    switch (overlayState)
    {
    case PORTRAIT_OVERLAY_STATE_DYING:
        return snapshot.dying;
    case PORTRAIT_OVERLAY_STATE_RECOVERY_COMA:
        return snapshot.recoveryComa;
    case PORTRAIT_OVERLAY_STATE_STRICT_UNCONSCIOUS:
    default:
        return snapshot.unconscious && !snapshot.recoveryComa && !snapshot.dying;
    }
}

std::string SafeWidgetName(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "";
    }

    return widget->getName();
}

std::string SafeWidgetType(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "";
    }

    return widget->getTypeName();
}

std::string SafeWidgetCaption(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "";
    }

    if (MyGUI::Window* window = widget->castType<MyGUI::Window>(false))
    {
        return window->getCaption().asUTF8();
    }

    if (MyGUI::TextBox* textBox = widget->castType<MyGUI::TextBox>(false))
    {
        return textBox->getCaption().asUTF8();
    }

    if (MyGUI::EditBox* editBox = widget->castType<MyGUI::EditBox>(false))
    {
        return editBox->getCaption().asUTF8();
    }

    return "";
}

std::string SafeWidgetUserStrings(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "";
    }

    const MyGUI::MapString& userStrings = widget->getUserStrings();
    if (userStrings.empty())
    {
        return "";
    }

    std::stringstream line;
    size_t emitted = 0u;
    for (MyGUI::MapString::const_iterator it = userStrings.begin();
         it != userStrings.end() && emitted < 3u;
         ++it, ++emitted)
    {
        if (emitted != 0u)
        {
            line << "|";
        }
        line << EscapeForLog(it->first) << ":" << EscapeForLog(it->second);
    }

    if (userStrings.size() > emitted)
    {
        line << "|...";
    }

    return line.str();
}

bool TryGetViewSize(MyGUI::IntSize* outViewSize)
{
    if (outViewSize == 0)
    {
        return false;
    }

    MyGUI::RenderManager* renderManager = MyGUI::RenderManager::getInstancePtr();
    if (renderManager == 0)
    {
        return false;
    }

    *outViewSize = renderManager->getViewSize();
    return true;
}

bool ShouldEmitProbeLogs()
{
    return g_probeLogScopeDepth != 0u;
}

void BeginProbeLogging()
{
    ++g_probeLogScopeDepth;
}

void EndProbeLogging()
{
    if (g_probeLogScopeDepth != 0u)
    {
        --g_probeLogScopeDepth;
    }
}

void StartNewProbeSession(const char* reason)
{
    g_activeProbeSessionId = g_nextProbeSessionId++;
    g_activeProbeSequence = 0u;
    BeginProbeLogging();

    std::stringstream payload;
    payload << "reason=" << QuoteForLog(reason == 0 ? "manual" : reason);

    std::stringstream line;
    line << kPluginName
         << " PROBE: session_id=" << g_activeProbeSessionId
         << " probe=session"
         << " seq=" << (++g_activeProbeSequence)
         << " timestamp_ms=" << GetTickCount()
         << " event=session_started"
         << " " << payload.str();
    DebugLog(line.str().c_str());
    EndProbeLogging();
}

void EnsureActiveProbeSession(const char* autoReason)
{
    if (g_activeProbeSessionId == 0u)
    {
        StartNewProbeSession(autoReason == 0 ? "auto" : autoReason);
    }
}

void LogProbeRecord(const char* probe, const char* eventName, const std::string& payload)
{
    if (!ShouldEmitProbeLogs())
    {
        return;
    }

    EnsureActiveProbeSession("auto");

    std::stringstream line;
    line << kPluginName
         << " PROBE: session_id=" << g_activeProbeSessionId
         << " probe=" << (probe == 0 ? "unknown" : probe)
         << " seq=" << (++g_activeProbeSequence)
         << " timestamp_ms=" << GetTickCount()
         << " event=" << (eventName == 0 ? "record" : eventName);

    if (!payload.empty())
    {
        line << " " << payload;
    }

    DebugLog(line.str().c_str());
}

bool TryGetHoverContext(HoverContext* contextOut)
{
    if (contextOut == 0)
    {
        return false;
    }

    *contextOut = HoverContext();

    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        return false;
    }

    contextOut->available = true;
    contextOut->hoveredWidget = inputManager->getMouseFocusWidget();
    contextOut->mouse = inputManager->getMousePosition();
    return true;
}

MyGUI::Widget* GetTopRootWidget(MyGUI::Widget* widget)
{
    MyGUI::Widget* current = widget;
    while (current != 0 && current->getParent() != 0)
    {
        current = current->getParent();
    }

    return current;
}

size_t CountWidgetChainDepth(MyGUI::Widget* widget, size_t maxDepth)
{
    size_t depth = 0u;
    for (MyGUI::Widget* current = widget; current != 0 && depth < maxDepth; current = current->getParent())
    {
        ++depth;
    }
    return depth;
}

int FindChildIndex(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return -1;
    }

    MyGUI::Widget* parent = widget->getParent();
    if (parent == 0)
    {
        return -1;
    }

    const size_t childCount = parent->getChildCount();
    for (size_t index = 0u; index < childCount; ++index)
    {
        if (parent->getChildAt(index) == widget)
        {
            return static_cast<int>(index);
        }
    }

    return -1;
}

bool IsWidgetInParentChain(MyGUI::Widget* possibleAncestor, MyGUI::Widget* widget)
{
    for (MyGUI::Widget* current = widget; current != 0; current = current->getParent())
    {
        if (current == possibleAncestor)
        {
            return true;
        }
    }

    return false;
}

int CountSimilarVisibleSiblings(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    MyGUI::Widget* parent = widget->getParent();
    if (parent == 0)
    {
        return widget->getInheritedVisible() ? 1 : 0;
    }

    const MyGUI::IntCoord targetCoord = widget->getAbsoluteCoord();
    const size_t childCount = parent->getChildCount();
    int similar = 0;
    for (size_t index = 0u; index < childCount; ++index)
    {
        MyGUI::Widget* sibling = parent->getChildAt(index);
        if (sibling == 0 || !sibling->getInheritedVisible())
        {
            continue;
        }

        const MyGUI::IntCoord siblingCoord = sibling->getAbsoluteCoord();
        if (AbsoluteInt(siblingCoord.width - targetCoord.width) <= 6
            && AbsoluteInt(siblingCoord.height - targetCoord.height) <= 6)
        {
            ++similar;
        }
    }

    return similar;
}

bool IsPortraitSizedCoord(const MyGUI::IntCoord& coord)
{
    return coord.width >= 28 && coord.width <= 220
        && coord.height >= 28 && coord.height <= 220;
}

bool WidgetHasPortraitIdentity(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return false;
    }

    return ContainsAsciiCaseInsensitive(SafeWidgetName(widget), "portrait")
        || ContainsAsciiCaseInsensitive(SafeWidgetCaption(widget), "portrait")
        || ContainsAsciiCaseInsensitive(SafeWidgetType(widget), "portrait")
        || ContainsAsciiCaseInsensitive(SafeWidgetUserStrings(widget), "portrait");
}

bool HasPortraitDescendantRecursive(
    MyGUI::Widget* widget,
    size_t depth,
    size_t* visitedNodes)
{
    if (widget == 0
        || visitedNodes == 0
        || *visitedNodes >= kHoveredPortraitDescendantNodeLimit
        || depth > kHoveredPortraitDescendantDepthLimit)
    {
        return false;
    }

    ++(*visitedNodes);

    if (depth != 0u && WidgetHasPortraitIdentity(widget) && IsPortraitSizedCoord(widget->getAbsoluteCoord()))
    {
        return true;
    }

    const size_t childCount = widget->getChildCount();
    for (size_t index = 0u; index < childCount; ++index)
    {
        if (HasPortraitDescendantRecursive(widget->getChildAt(index), depth + 1u, visitedNodes))
        {
            return true;
        }
    }

    return false;
}

bool IsAcceptableHoveredPortraitCandidate(const PortraitCandidateRecord& candidate)
{
    if (candidate.widget == 0 || candidate.score < kHoveredPortraitMarkThreshold)
    {
        return false;
    }

    if (!IsPortraitSizedCoord(candidate.absoluteCoord))
    {
        return false;
    }

    if (WidgetHasPortraitIdentity(candidate.widget))
    {
        return true;
    }

    size_t visitedNodes = 0u;
    return HasPortraitDescendantRecursive(candidate.widget, 0u, &visitedNodes);
}

void AppendScoreReason(bool condition, float delta, const char* token, float* score, std::string* reason)
{
    if (!condition || score == 0 || reason == 0 || token == 0 || *token == '\0')
    {
        return;
    }

    *score += delta;
    if (!reason->empty())
    {
        *reason += ",";
    }
    *reason += token;
}

PortraitCandidateRecord BuildPortraitCandidateRecord(MyGUI::Widget* widget, MyGUI::Widget* hoveredWidget)
{
    PortraitCandidateRecord record;
    record.widget = widget;
    record.parent = widget == 0 ? 0 : widget->getParent();
    record.typeName = SafeWidgetType(widget);
    record.name = SafeWidgetName(widget);
    record.caption = SafeWidgetCaption(widget);

    if (widget == 0)
    {
        record.reason = "null_widget";
        return record;
    }

    record.absoluteCoord = widget->getAbsoluteCoord();
    record.visible = widget->getVisible();
    record.inheritedVisible = widget->getInheritedVisible();
    record.childCount = widget->getChildCount();

    float score = 0.0f;
    std::string reason;

    const bool validSize = record.absoluteCoord.width > 0 && record.absoluteCoord.height > 0;
    const bool portraitSize =
        record.absoluteCoord.width >= 28 && record.absoluteCoord.width <= 220
        && record.absoluteCoord.height >= 28 && record.absoluteCoord.height <= 220;
    const float aspectRatio = record.absoluteCoord.height == 0
        ? 0.0f
        : static_cast<float>(record.absoluteCoord.width) / static_cast<float>(record.absoluteCoord.height);

    AppendScoreReason(record.inheritedVisible, 0.05f, "visible", &score, &reason);
    AppendScoreReason(portraitSize, 0.15f, "size", &score, &reason);
    AppendScoreReason(validSize && aspectRatio >= 0.55f && aspectRatio <= 1.85f, 0.10f, "aspect", &score, &reason);
    AppendScoreReason(record.typeName == "ImageBox", 0.05f, "image_box", &score, &reason);
    AppendScoreReason(widget->getNeedMouseFocus(), 0.05f, "mouse_focus", &score, &reason);
    AppendScoreReason(IsWidgetInParentChain(widget, hoveredWidget), 0.10f, "hover_chain", &score, &reason);
    AppendScoreReason(CountSimilarVisibleSiblings(widget) >= 3, 0.20f, "repeated_siblings", &score, &reason);
    AppendScoreReason(ContainsAsciiCaseInsensitive(record.name, "portrait"), 0.55f, "portrait_token", &score, &reason);
    AppendScoreReason(ContainsAsciiCaseInsensitive(record.name, "squad"), 0.20f, "squad_token", &score, &reason);
    AppendScoreReason(ContainsAsciiCaseInsensitive(record.name, "member"), 0.15f, "member_token", &score, &reason);
    AppendScoreReason(ContainsAsciiCaseInsensitive(record.name, "character"), 0.15f, "character_token", &score, &reason);
    AppendScoreReason(ContainsAsciiCaseInsensitive(record.name, "slot"), 0.08f, "slot_token", &score, &reason);
    AppendScoreReason(ContainsAsciiCaseInsensitive(record.caption, "portrait"), 0.30f, "caption_portrait", &score, &reason);
    AppendScoreReason(ContainsAsciiCaseInsensitive(record.caption, "squad"), 0.15f, "caption_squad", &score, &reason);
    AppendScoreReason(ContainsAsciiCaseInsensitive(record.typeName, "portrait"), 0.25f, "type_portrait", &score, &reason);
    AppendScoreReason(ContainsAsciiCaseInsensitive(record.typeName, "button"), 0.05f, "button_type", &score, &reason);

    const std::string userStrings = SafeWidgetUserStrings(widget);
    AppendScoreReason(ContainsAsciiCaseInsensitive(userStrings, "portrait"), 0.20f, "user_portrait", &score, &reason);
    AppendScoreReason(ContainsAsciiCaseInsensitive(userStrings, "squad"), 0.10f, "user_squad", &score, &reason);

    record.score = ClampUnitFloat(score);
    record.reason = reason.empty() ? "non_match" : reason;
    return record;
}

void LogWidgetRecord(const char* probe, const char* eventName, int depth, int siblingIndex, MyGUI::Widget* widget)
{
    std::stringstream payload;
    payload << "depth=" << depth
            << " sibling_index=" << siblingIndex
            << " pointer=" << QuoteForLog(FormatPointer(widget))
            << " parent_pointer=" << QuoteForLog(FormatPointer(widget == 0 ? 0 : widget->getParent()))
            << " type=" << QuoteForLog(SafeWidgetType(widget))
            << " name=" << QuoteForLog(SafeWidgetName(widget))
            << " caption=" << QuoteForLog(SafeWidgetCaption(widget))
            << " local_coord=" << QuoteForLog(widget == 0 ? "" : FormatCoord(widget->getCoord()))
            << " abs_coord=" << QuoteForLog(widget == 0 ? "" : FormatCoord(widget->getAbsoluteCoord()))
            << " visible=" << FormatBool(widget != 0 && widget->getVisible())
            << " inherited_visible=" << FormatBool(widget != 0 && widget->getInheritedVisible())
            << " enabled=" << FormatBool(widget != 0 && widget->getEnabled())
            << " need_mouse_focus=" << FormatBool(widget != 0 && widget->getNeedMouseFocus())
            << " child_count=" << (widget == 0 ? 0u : widget->getChildCount())
            << " user_strings=" << QuoteForLog(SafeWidgetUserStrings(widget));
    LogProbeRecord(probe, eventName, payload.str());
}

void DumpHoveredWidgetProbeImpl(const char* reason)
{
    HoverContext hover;
    const bool hoverAvailable = TryGetHoverContext(&hover);
    if (!hoverAvailable)
    {
        LogProbeRecord(
            "dump_hovered_widget",
            "summary",
            "status=no_input_manager reason=" + QuoteForLog(reason == 0 ? "manual" : reason));
        return;
    }

    std::stringstream header;
    header << "status=ok"
           << " reason=" << QuoteForLog(reason == 0 ? "manual" : reason)
           << " mouse=" << QuoteForLog(FormatPoint(hover.mouse))
           << " hovered_pointer=" << QuoteForLog(FormatPointer(hover.hoveredWidget))
           << " chain_depth=" << CountWidgetChainDepth(hover.hoveredWidget, kHoveredChainDepthLimit);
    LogProbeRecord("dump_hovered_widget", "summary", header.str());

    int depth = 0;
    for (MyGUI::Widget* current = hover.hoveredWidget;
         current != 0 && static_cast<size_t>(depth) < kHoveredChainDepthLimit;
         current = current->getParent(), ++depth)
    {
        LogWidgetRecord("dump_hovered_widget", "chain_node", depth, FindChildIndex(current), current);
    }
}

bool TryDumpHoveredWidgetProbeSeh(const char* reason)
{
    __try
    {
        DumpHoveredWidgetProbeImpl(reason);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void DumpHoveredWidgetProbe(const char* reason)
{
    EnsureActiveProbeSession(reason);
    BeginProbeLogging();
    if (!TryDumpHoveredWidgetProbeSeh(reason))
    {
        LogProbeRecord("dump_hovered_widget", "exception", "status=seh_guard");
    }
    EndProbeLogging();
}

void LogPortraitTreeNodeRecursive(
    MyGUI::Widget* widget,
    int depth,
    int siblingIndex,
    size_t* nodesLogged)
{
    if (widget == 0 || nodesLogged == 0 || *nodesLogged >= kPortraitTreeNodeLimit)
    {
        return;
    }

    LogWidgetRecord("dump_portrait_bar_tree", "tree_node", depth, siblingIndex, widget);
    ++(*nodesLogged);

    if (static_cast<size_t>(depth) >= kPortraitTreeDepthLimit)
    {
        return;
    }

    const size_t childCount = widget->getChildCount();
    for (size_t index = 0u; index < childCount; ++index)
    {
        if (*nodesLogged >= kPortraitTreeNodeLimit)
        {
            return;
        }

        LogPortraitTreeNodeRecursive(widget->getChildAt(index), depth + 1, static_cast<int>(index), nodesLogged);
    }
}

void DumpPortraitBarTreeProbeImpl(const char* reason)
{
    HoverContext hover;
    const bool hoverAvailable = TryGetHoverContext(&hover);
    MyGUI::IntSize viewSize(0, 0);
    const bool haveViewSize = TryGetViewSize(&viewSize);
    MyGUI::Widget* root = hoverAvailable ? GetTopRootWidget(hover.hoveredWidget) : 0;

    std::stringstream header;
    header << "status=" << (root == 0 ? "no_hovered_root" : "ok")
           << " reason=" << QuoteForLog(reason == 0 ? "manual" : reason)
           << " hovered_pointer=" << QuoteForLog(FormatPointer(hover.hoveredWidget))
           << " root_pointer=" << QuoteForLog(FormatPointer(root))
           << " root_type=" << QuoteForLog(SafeWidgetType(root))
           << " root_name=" << QuoteForLog(SafeWidgetName(root))
           << " root_child_count=" << (root == 0 ? 0u : root->getChildCount())
           << " view_size=" << QuoteForLog(haveViewSize ? FormatCoord(MyGUI::IntCoord(0, 0, viewSize.width, viewSize.height)) : "")
           << " depth_limit=" << kPortraitTreeDepthLimit
           << " node_limit=" << kPortraitTreeNodeLimit;
    LogProbeRecord("dump_portrait_bar_tree", "summary", header.str());

    if (root == 0)
    {
        return;
    }

    size_t nodesLogged = 0u;
    LogPortraitTreeNodeRecursive(root, 0, -1, &nodesLogged);
}

bool TryDumpPortraitBarTreeProbeSeh(const char* reason)
{
    __try
    {
        DumpPortraitBarTreeProbeImpl(reason);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void DumpPortraitBarTreeProbe(const char* reason)
{
    EnsureActiveProbeSession(reason);
    BeginProbeLogging();
    if (!TryDumpPortraitBarTreeProbeSeh(reason))
    {
        LogProbeRecord("dump_portrait_bar_tree", "exception", "status=seh_guard");
    }
    EndProbeLogging();
}

void CollectPortraitCandidatesRecursive(
    MyGUI::Widget* widget,
    MyGUI::Widget* hoveredWidget,
    std::vector<PortraitCandidateRecord>* outCandidates,
    size_t* scannedNodes)
{
    if (widget == 0 || outCandidates == 0 || scannedNodes == 0 || *scannedNodes >= kPortraitCandidateScanLimit)
    {
        return;
    }

    ++(*scannedNodes);

    PortraitCandidateRecord record = BuildPortraitCandidateRecord(widget, hoveredWidget);
    if (record.score >= kPortraitCandidateLogThreshold)
    {
        outCandidates->push_back(record);
    }

    const size_t childCount = widget->getChildCount();
    for (size_t index = 0u; index < childCount; ++index)
    {
        if (*scannedNodes >= kPortraitCandidateScanLimit)
        {
            return;
        }
        CollectPortraitCandidatesRecursive(widget->getChildAt(index), hoveredWidget, outCandidates, scannedNodes);
    }
}

bool PortraitCandidateSortPredicate(const PortraitCandidateRecord& left, const PortraitCandidateRecord& right)
{
    if (left.score != right.score)
    {
        return left.score > right.score;
    }
    if (left.absoluteCoord.top != right.absoluteCoord.top)
    {
        return left.absoluteCoord.top < right.absoluteCoord.top;
    }
    if (left.absoluteCoord.left != right.absoluteCoord.left)
    {
        return left.absoluteCoord.left < right.absoluteCoord.left;
    }
    if (left.absoluteCoord.height != right.absoluteCoord.height)
    {
        return left.absoluteCoord.height < right.absoluteCoord.height;
    }
    if (left.absoluteCoord.width != right.absoluteCoord.width)
    {
        return left.absoluteCoord.width < right.absoluteCoord.width;
    }
    return reinterpret_cast<size_t>(left.widget) < reinterpret_cast<size_t>(right.widget);
}

bool TryCollectPortraitCandidates(
    MyGUI::Widget* hoveredWidget,
    std::vector<PortraitCandidateRecord>* outCandidates,
    size_t* outVisibleRootCount,
    size_t* outScannedNodes)
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0 || outCandidates == 0 || outVisibleRootCount == 0 || outScannedNodes == 0)
    {
        return false;
    }

    *outVisibleRootCount = 0u;
    *outScannedNodes = 0u;
    outCandidates->clear();

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next() && *outScannedNodes < kPortraitCandidateScanLimit)
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        ++(*outVisibleRootCount);
        CollectPortraitCandidatesRecursive(root, hoveredWidget, outCandidates, outScannedNodes);
    }

    std::stable_sort(outCandidates->begin(), outCandidates->end(), PortraitCandidateSortPredicate);
    return true;
}

bool IsVisibleDisplayPortraitCandidate(const PortraitCandidateRecord& candidate)
{
    if (candidate.widget == 0 || !candidate.inheritedVisible)
    {
        return false;
    }

    if (candidate.typeName != "ImageBox" || !IsPortraitSizedCoord(candidate.absoluteCoord))
    {
        return false;
    }

    if (ContainsAsciiCaseInsensitive(candidate.name, "portraitimage"))
    {
        return true;
    }

    return candidate.score >= 0.95f
        && ContainsAsciiCaseInsensitive(candidate.reason, "portrait_token");
}

void CollectDisplayPortraitCandidates(
    const std::vector<PortraitCandidateRecord>& allCandidates,
    std::vector<PortraitCandidateRecord>* outDisplayPortraits)
{
    if (outDisplayPortraits == 0)
    {
        return;
    }

    outDisplayPortraits->clear();
    for (size_t index = 0u; index < allCandidates.size(); ++index)
    {
        const PortraitCandidateRecord& candidate = allCandidates[index];
        if (IsVisibleDisplayPortraitCandidate(candidate))
        {
            outDisplayPortraits->push_back(candidate);
        }
    }

    std::stable_sort(outDisplayPortraits->begin(), outDisplayPortraits->end(), PortraitCandidateSortPredicate);
}

bool TryResolveDisplayPortraitSlotIndex(
    Character* character,
    const size_t displayPortraitCount,
    int* outDisplaySlotIndex,
    const char** outMappingKey)
{
    if (character == 0 || outDisplaySlotIndex == 0 || outMappingKey == 0)
    {
        return false;
    }

    const int squadMemberIndex = static_cast<int>(character->squadMemberID);
    if (squadMemberIndex >= 0 && static_cast<size_t>(squadMemberIndex) < displayPortraitCount)
    {
        *outDisplaySlotIndex = squadMemberIndex;
        *outMappingKey = "squad_member_id";
        return true;
    }

    const int portraitIndex = static_cast<int>(character->portraitIndex);
    if (portraitIndex >= 0 && static_cast<size_t>(portraitIndex) < displayPortraitCount)
    {
        *outDisplaySlotIndex = portraitIndex;
        *outMappingKey = "portrait_index_fallback";
        return true;
    }

    return false;
}

bool TryCollectPortraitOverlayMatches(
    const PortraitOverlayState overlayState,
    std::vector<PortraitOverlayMatch>* outMatches,
    PortraitOverlayMatchStatus* outFailureStatus)
{
    if (outMatches == 0 || outFailureStatus == 0)
    {
        return false;
    }

    outMatches->clear();
    *outFailureStatus = PORTRAIT_OVERLAY_MATCH_NO_PLAYER_INTERFACE;

    SquadProbeScope scope;
    if (!TryResolveSquadProbeScope(&scope))
    {
        return true;
    }

    std::vector<PortraitOverlayMatch> stateMatches;
    if (scope.allPlayerCharacters != 0)
    {
        for (uint32_t rawIndex = 0u; rawIndex < scope.allPlayerCharacters->size(); ++rawIndex)
        {
            Character* candidate = (*scope.allPlayerCharacters)[rawIndex];
            if (!CharacterMatchesSquadProbeScope(scope, candidate))
            {
                continue;
            }

            MemberStateSnapshot snapshot;
            if (!TryResolveMemberStateSnapshot(candidate, &snapshot))
            {
                continue;
            }

            if (!SnapshotMatchesPortraitOverlayState(snapshot, overlayState))
            {
                continue;
            }

            PortraitOverlayMatch match;
            match.overlayState = overlayState;
            match.character = candidate;
            match.state = snapshot;
            stateMatches.push_back(match);
        }
    }

    if (stateMatches.empty())
    {
        *outFailureStatus = PORTRAIT_OVERLAY_MATCH_NO_STATE;
        return true;
    }

    size_t visibleRootCount = 0u;
    size_t scannedNodes = 0u;
    std::vector<PortraitCandidateRecord> allCandidates;
    if (!TryCollectPortraitCandidates(0, &allCandidates, &visibleRootCount, &scannedNodes))
    {
        *outFailureStatus = PORTRAIT_OVERLAY_MATCH_NO_GUI;
        return true;
    }

    std::vector<PortraitCandidateRecord> displayPortraits;
    CollectDisplayPortraitCandidates(allCandidates, &displayPortraits);
    std::vector<bool> usedSlots(displayPortraits.size(), false);

    for (size_t index = 0u; index < stateMatches.size(); ++index)
    {
        PortraitOverlayMatch match = stateMatches[index];
        match.visibleRootCount = visibleRootCount;
        match.scannedNodes = scannedNodes;
        match.displayPortraitCount = displayPortraits.size();

        if (!TryResolveDisplayPortraitSlotIndex(
                match.character,
                displayPortraits.size(),
                &match.displaySlotIndex,
                &match.mappingKey))
        {
            continue;
        }

        if (match.displaySlotIndex < 0
            || static_cast<size_t>(match.displaySlotIndex) >= displayPortraits.size())
        {
            continue;
        }

        if (usedSlots[static_cast<size_t>(match.displaySlotIndex)])
        {
            continue;
        }

        usedSlots[static_cast<size_t>(match.displaySlotIndex)] = true;
        match.target = displayPortraits[static_cast<size_t>(match.displaySlotIndex)];
        match.status = PORTRAIT_OVERLAY_MATCH_OK;
        outMatches->push_back(match);
    }

    if (outMatches->empty())
    {
        *outFailureStatus = PORTRAIT_OVERLAY_MATCH_NO_PORTRAIT;
        return true;
    }

    std::stable_sort(
        outMatches->begin(),
        outMatches->end(),
        [](const PortraitOverlayMatch& left, const PortraitOverlayMatch& right) -> bool
        {
            if (left.displaySlotIndex != right.displaySlotIndex)
            {
                return left.displaySlotIndex < right.displaySlotIndex;
            }
            return reinterpret_cast<size_t>(left.character) < reinterpret_cast<size_t>(right.character);
        });

    *outFailureStatus = PORTRAIT_OVERLAY_MATCH_OK;
    return true;
}

bool TryResolvePortraitOverlayMatch(
    const PortraitOverlayState overlayState,
    PortraitOverlayMatch* outMatch)
{
    if (outMatch == 0)
    {
        return false;
    }

    *outMatch = PortraitOverlayMatch();
    outMatch->overlayState = overlayState;

    std::vector<PortraitOverlayMatch> matches;
    PortraitOverlayMatchStatus failureStatus = PORTRAIT_OVERLAY_MATCH_NO_PLAYER_INTERFACE;
    if (!TryCollectPortraitOverlayMatches(overlayState, &matches, &failureStatus))
    {
        return false;
    }

    if (failureStatus != PORTRAIT_OVERLAY_MATCH_OK || matches.empty())
    {
        outMatch->status = failureStatus;
        return true;
    }

    *outMatch = matches.front();
    return true;
}

bool ShowMarkerWidgetAtPortrait(
    MyGUI::Widget* markerWidget,
    const PortraitCandidateRecord& target,
    const int markerInsetPx,
    const int markerMinSizePx,
    const int markerMaxSizePx,
    MyGUI::IntCoord* outMarkerBounds)
{
    if (markerWidget == 0)
    {
        return false;
    }

    MyGUI::IntSize rawViewSize(0, 0);
    vr_marker_ui::ViewSize viewSize;
    vr_marker_ui::ViewSize* viewSizePtr = 0;
    if (TryGetViewSize(&rawViewSize))
    {
        viewSize = vr_marker_ui::ViewSize(rawViewSize.width, rawViewSize.height);
        viewSizePtr = &viewSize;
    }

    vr_marker_ui::Rect markerBounds;
    if (!vr_marker_ui::TryPlaceMarkerWidget(
            markerWidget,
            vr_marker_ui::Rect(
                target.absoluteCoord.left,
                target.absoluteCoord.top,
                target.absoluteCoord.width,
                target.absoluteCoord.height),
            viewSizePtr,
            markerInsetPx,
            markerMinSizePx,
            markerMaxSizePx,
            &markerBounds))
    {
        return false;
    }

    if (outMarkerBounds != 0)
    {
        *outMarkerBounds = MyGUI::IntCoord(
            markerBounds.left,
            markerBounds.top,
            markerBounds.width,
            markerBounds.height);
    }

    return true;
}

bool PlaceProbeMarkerAtPortrait(
    const PortraitCandidateRecord& target,
    MyGUI::IntCoord* outMarkerBounds)
{
    if (target.widget == 0 || !EnsureHoveredMarkerWidget())
    {
        return false;
    }

    if (!ShowMarkerWidgetAtPortrait(
            g_hoveredMarkerWidget,
            target,
            kHoveredMarkerInsetPx,
            kHoveredMarkerMinSizePx,
            kHoveredMarkerMaxSizePx,
            outMarkerBounds))
    {
        return false;
    }

    g_hoveredMarkerExpireTick = GetTickCount() + kHoveredMarkerLifetimeMs;
    return true;
}

void DumpPortraitCandidatesProbeImpl(const char* reason)
{
    HoverContext hover;
    const bool hoverAvailable = TryGetHoverContext(&hover);
    std::vector<PortraitCandidateRecord> candidates;
    size_t visibleRootCount = 0u;
    size_t scannedNodes = 0u;
    if (!TryCollectPortraitCandidates(hover.hoveredWidget, &candidates, &visibleRootCount, &scannedNodes))
    {
        LogProbeRecord(
            "dump_portrait_candidates",
            "summary",
            "status=no_gui reason=" + QuoteForLog(reason == 0 ? "manual" : reason));
        return;
    }

    MyGUI::IntSize viewSize(0, 0);
    const bool haveViewSize = TryGetViewSize(&viewSize);

    std::stringstream header;
    header << "status=ok"
           << " reason=" << QuoteForLog(reason == 0 ? "manual" : reason)
           << " hovered_pointer=" << QuoteForLog(FormatPointer(hoverAvailable ? hover.hoveredWidget : 0))
           << " visible_root_count=" << visibleRootCount
           << " scanned_nodes=" << scannedNodes
           << " candidate_count=" << candidates.size()
           << " view_width=" << (haveViewSize ? viewSize.width : 0)
           << " view_height=" << (haveViewSize ? viewSize.height : 0)
           << " ui_scale_note=" << QuoteForLog("unknown")
           << " coord_space=" << QuoteForLog("absolute_gui_pixels");
    LogProbeRecord("dump_portrait_candidates", "summary", header.str());

    const size_t logCount = candidates.size() < kPortraitCandidateLogLimit
        ? candidates.size()
        : kPortraitCandidateLogLimit;
    for (size_t index = 0u; index < logCount; ++index)
    {
        const PortraitCandidateRecord& candidate = candidates[index];
        std::stringstream payload;
        payload << "index=" << index
                << " pointer=" << QuoteForLog(FormatPointer(candidate.widget))
                << " parent_pointer=" << QuoteForLog(FormatPointer(candidate.parent))
                << " type=" << QuoteForLog(candidate.typeName)
                << " name=" << QuoteForLog(candidate.name)
                << " caption=" << QuoteForLog(candidate.caption)
                << " bounds=" << QuoteForLog(FormatCoord(candidate.absoluteCoord))
                << " coord_space=" << QuoteForLog("absolute_gui_pixels")
                << " ui_scale_note=" << QuoteForLog("unknown")
                << " visible=" << FormatBool(candidate.visible)
                << " inherited_visible=" << FormatBool(candidate.inheritedVisible)
                << " child_count=" << candidate.childCount
                << " confidence_score=" << FormatFloat2(candidate.score)
                << " confidence_reason=" << QuoteForLog(candidate.reason);
        LogProbeRecord("dump_portrait_candidates", "candidate", payload.str());
    }
}

bool TryDumpPortraitCandidatesProbeSeh(const char* reason)
{
    __try
    {
        DumpPortraitCandidatesProbeImpl(reason);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void DumpPortraitCandidatesProbe(const char* reason)
{
    EnsureActiveProbeSession(reason);
    BeginProbeLogging();
    if (!TryDumpPortraitCandidatesProbeSeh(reason))
    {
        LogProbeRecord("dump_portrait_candidates", "exception", "status=seh_guard");
    }
    EndProbeLogging();
}

void DumpSelectedSquadMembersProbeImpl(const char* reason)
{
    SquadProbeScope scope;
    if (!TryResolveSquadProbeScope(&scope))
    {
        LogProbeRecord(
            "dump_selected_squad_members",
            "summary",
            "status=no_player_interface reason=" + QuoteForLog(reason == 0 ? "manual" : reason));
        return;
    }

    int selectedScopeIndex = -1;
    const size_t scopedMemberCount = CountScopedSquadMembers(scope, &selectedScopeIndex);

    std::stringstream summary;
    summary << "status=" << (scopedMemberCount == 0u ? "no_members" : "ok")
            << " reason=" << QuoteForLog(reason == 0 ? "manual" : reason)
            << " scope=" << QuoteForLog(scope.scope)
            << " player_pointer=" << QuoteForLog(FormatPointer(scope.player))
            << " selected_character_pointer=" << QuoteForLog(FormatPointer(scope.selectedCharacter))
            << " selected_character_handle=" << QuoteForLog(SafeHandleString(scope.player->selectedCharacter))
            << " selected_character_name=" << QuoteForLog(SafeCharacterName(scope.selectedCharacter))
            << " selected_scope_index=" << selectedScopeIndex
            << " target_platoon_pointer=" << QuoteForLog(FormatPointer(scope.targetPlatoon))
            << " target_platoon_name=" << QuoteForLog(SafePlatoonName(scope.targetPlatoon))
            << " target_active_platoon_pointer=" << QuoteForLog(FormatPointer(scope.targetActivePlatoon))
            << " target_platoon_index=" << (scope.targetPlatoon == 0 ? -1 : static_cast<int>(scope.targetPlatoon->index))
            << " target_squad_type=" << (scope.targetPlatoon == 0 ? -1 : static_cast<int>(scope.targetPlatoon->squadType))
            << " all_player_character_count=" << (scope.allPlayerCharacters == 0 ? 0 : scope.allPlayerCharacters->size())
            << " scoped_member_count=" << scopedMemberCount
            << " platoon_character_count_hint=" << (scope.targetPlatoon == 0 ? -1 : scope.targetPlatoon->getCharacterCount())
            << " squad_size_hint=" << (scope.targetActivePlatoon == 0 ? -1 : scope.targetActivePlatoon->getSquadSize());
    LogProbeRecord("dump_selected_squad_members", "summary", summary.str());

    if (scopedMemberCount == 0u || scope.allPlayerCharacters == 0)
    {
        return;
    }

    size_t scopedIndex = 0u;
    for (uint32_t rawIndex = 0u; rawIndex < scope.allPlayerCharacters->size(); ++rawIndex)
    {
        Character* candidate = (*scope.allPlayerCharacters)[rawIndex];
        if (!CharacterMatchesSquadProbeScope(scope, candidate))
        {
            continue;
        }

        ActivePlatoon* candidateActivePlatoon = candidate->platoon;
        Platoon* candidatePlatoon = candidateActivePlatoon == 0 ? 0 : candidateActivePlatoon->me;

        std::stringstream payload;
        payload << "index=" << scopedIndex
                << " raw_index=" << rawIndex
                << " character_pointer=" << QuoteForLog(FormatPointer(candidate))
                << " character_handle=" << QuoteForLog(SafeHandleString(candidate->handle))
                << " name=" << QuoteForLog(SafeCharacterName(candidate))
                << " active_selected=" << FormatBool(candidate == scope.selectedCharacter)
                << " currently_selected=" << FormatBool(TryIsCharacterSelected(scope.player, candidate))
                << " active_platoon_pointer=" << QuoteForLog(FormatPointer(candidateActivePlatoon))
                << " platoon_pointer=" << QuoteForLog(FormatPointer(candidatePlatoon))
                << " platoon_name=" << QuoteForLog(SafePlatoonName(candidatePlatoon))
                << " platoon_index=" << (candidatePlatoon == 0 ? -1 : static_cast<int>(candidatePlatoon->index))
                << " squad_type=" << (candidatePlatoon == 0 ? -1 : static_cast<int>(candidatePlatoon->squadType))
                << " squad_member_id=" << candidate->squadMemberID
                << " portrait_index=" << static_cast<int>(candidate->portraitIndex)
                << " current_scope_match=" << FormatBool(CharacterMatchesSquadProbeScope(scope, candidate));
        LogProbeRecord("dump_selected_squad_members", "member", payload.str());
        ++scopedIndex;
    }
}

bool TryDumpSelectedSquadMembersProbeSeh(const char* reason)
{
    __try
    {
        DumpSelectedSquadMembersProbeImpl(reason);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void DumpSelectedSquadMembersProbe(const char* reason)
{
    EnsureActiveProbeSession(reason);
    BeginProbeLogging();
    if (!TryDumpSelectedSquadMembersProbeSeh(reason))
    {
        LogProbeRecord("dump_selected_squad_members", "exception", "status=seh_guard");
    }
    EndProbeLogging();
}

void DumpMemberStatesProbeImpl(const char* reason)
{
    SquadProbeScope scope;
    if (!TryResolveSquadProbeScope(&scope))
    {
        LogProbeRecord(
            "dump_member_states",
            "summary",
            "status=no_player_interface reason=" + QuoteForLog(reason == 0 ? "manual" : reason));
        return;
    }

    int selectedScopeIndex = -1;
    const size_t scopedMemberCount = CountScopedSquadMembers(scope, &selectedScopeIndex);
    size_t deadCount = 0u;
    size_t unconsciousCount = 0u;
    size_t recoveryComaCount = 0u;
    size_t playingDeadCount = 0u;
    size_t dyingCount = 0u;
    size_t awakeCount = 0u;

    if (scope.allPlayerCharacters != 0)
    {
        for (uint32_t rawIndex = 0u; rawIndex < scope.allPlayerCharacters->size(); ++rawIndex)
        {
            Character* candidate = (*scope.allPlayerCharacters)[rawIndex];
            if (!CharacterMatchesSquadProbeScope(scope, candidate))
            {
                continue;
            }

            MemberStateSnapshot snapshot;
            if (!TryResolveMemberStateSnapshot(candidate, &snapshot))
            {
                continue;
            }

            if (snapshot.dead)
            {
                ++deadCount;
            }
            else if (snapshot.dying)
            {
                ++dyingCount;
            }
            else if (snapshot.playingDead)
            {
                ++playingDeadCount;
            }
            else if (snapshot.recoveryComa)
            {
                ++recoveryComaCount;
            }
            else if (snapshot.unconscious)
            {
                ++unconsciousCount;
            }
            else
            {
                ++awakeCount;
            }
        }
    }

    std::stringstream summary;
    summary << "status=" << (scopedMemberCount == 0u ? "no_members" : "ok")
            << " reason=" << QuoteForLog(reason == 0 ? "manual" : reason)
            << " scope=" << QuoteForLog(scope.scope)
            << " selected_scope_index=" << selectedScopeIndex
            << " target_platoon_pointer=" << QuoteForLog(FormatPointer(scope.targetPlatoon))
            << " target_platoon_name=" << QuoteForLog(SafePlatoonName(scope.targetPlatoon))
            << " scoped_member_count=" << scopedMemberCount
            << " unconscious_count=" << unconsciousCount
            << " recovery_coma_count=" << recoveryComaCount
            << " playing_dead_count=" << playingDeadCount
            << " dying_count=" << dyingCount
            << " dead_count=" << deadCount
            << " awake_count=" << awakeCount;
    LogProbeRecord("dump_member_states", "summary", summary.str());

    if (scopedMemberCount == 0u || scope.allPlayerCharacters == 0)
    {
        return;
    }

    size_t scopedIndex = 0u;
    for (uint32_t rawIndex = 0u; rawIndex < scope.allPlayerCharacters->size(); ++rawIndex)
    {
        Character* candidate = (*scope.allPlayerCharacters)[rawIndex];
        if (!CharacterMatchesSquadProbeScope(scope, candidate))
        {
            continue;
        }

        MemberStateSnapshot snapshot;
        if (!TryResolveMemberStateSnapshot(candidate, &snapshot))
        {
            std::stringstream failurePayload;
            failurePayload << "index=" << scopedIndex
                           << " raw_index=" << rawIndex
                           << " character_pointer=" << QuoteForLog(FormatPointer(candidate))
                           << " status=state_read_failed";
            LogProbeRecord(
                "dump_member_states",
                "member",
                failurePayload.str());
            ++scopedIndex;
            continue;
        }

        std::stringstream payload;
        payload << "index=" << scopedIndex
                << " raw_index=" << rawIndex
                << " character_pointer=" << QuoteForLog(FormatPointer(candidate))
                << " character_handle=" << QuoteForLog(SafeHandleString(candidate->handle))
                << " name=" << QuoteForLog(SafeCharacterName(candidate))
                << " active_selected=" << FormatBool(candidate == scope.selectedCharacter)
                << " portrait_index=" << static_cast<int>(candidate->portraitIndex)
                << " squad_member_id=" << candidate->squadMemberID
                << " prone_state=" << static_cast<int>(snapshot.proneState)
                << " state_label=" << QuoteForLog(snapshot.stateLabel)
                << " unconscious=" << FormatBool(snapshot.unconscious)
                << " playing_dead=" << FormatBool(snapshot.playingDead)
                << " dying=" << FormatBool(snapshot.dying)
                << " dead=" << FormatBool(snapshot.dead)
                << " recovery_coma=" << FormatBool(snapshot.recoveryComa)
                << " medical_unconcious=" << FormatBool(snapshot.medicalUnconcious)
                << " ko_prone=" << FormatBool(snapshot.koProne)
                << " probably_dying=" << FormatBool(snapshot.probablyDying)
                << " bloodloss_trauma=" << FormatBool(snapshot.bloodlossTrauma)
                << " sub50ko=" << FormatBool(snapshot.sub50KO)
                << " can_get_up=" << FormatBool(snapshot.canGetUpWakeUp)
                << " blood=" << FormatFloat2(snapshot.blood)
                << " point_of_no_return=" << FormatFloat2(snapshot.pointOfNoReturn)
                << " current_bleed_rate=" << FormatFloat2(snapshot.currentBleedRate)
                << " extra_blood_loss=" << FormatFloat2(snapshot.extraBloodLoss)
                << " knockout_timer=" << FormatFloat2(snapshot.knockoutTimer);
        LogProbeRecord("dump_member_states", "member", payload.str());
        ++scopedIndex;
    }
}

bool TryDumpMemberStatesProbeSeh(const char* reason)
{
    __try
    {
        DumpMemberStatesProbeImpl(reason);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void DumpMemberStatesProbe(const char* reason)
{
    EnsureActiveProbeSession(reason);
    BeginProbeLogging();
    if (!TryDumpMemberStatesProbeSeh(reason))
    {
        LogProbeRecord("dump_member_states", "exception", "status=seh_guard");
    }
    EndProbeLogging();
}

void MarkStateMatchedPortraitProbeImpl(const char* reason)
{
    PortraitOverlayMatch match;
    if (!TryResolvePortraitOverlayMatch(PORTRAIT_OVERLAY_STATE_STRICT_UNCONSCIOUS, &match))
    {
        HideHoveredMarker();
        LogProbeRecord("mark_state_matched_portrait", "exception", "status=match_resolve_failed");
        return;
    }

    if (match.status == PORTRAIT_OVERLAY_MATCH_NO_PLAYER_INTERFACE)
    {
        HideHoveredMarker();
        LogProbeRecord(
            "mark_state_matched_portrait",
            "summary",
            "status=no_player_interface reason=" + QuoteForLog(reason == 0 ? "manual" : reason));
        return;
    }

    if (match.status == PORTRAIT_OVERLAY_MATCH_NO_STATE)
    {
        HideHoveredMarker();
        LogProbeRecord(
            "mark_state_matched_portrait",
            "summary",
            "status=no_state_match reason=" + QuoteForLog(reason == 0 ? "manual" : reason)
                + " target_state=" + QuoteForLog(GetPortraitOverlayStateLabel(match.overlayState)));
        return;
    }

    if (match.status == PORTRAIT_OVERLAY_MATCH_NO_GUI)
    {
        HideHoveredMarker();
        LogProbeRecord(
            "mark_state_matched_portrait",
            "summary",
            "status=no_gui reason=" + QuoteForLog(reason == 0 ? "manual" : reason));
        return;
    }

    if (match.status == PORTRAIT_OVERLAY_MATCH_NO_PORTRAIT)
    {
        HideHoveredMarker();
        std::stringstream payload;
        payload << "status=no_portrait_match"
                << " reason=" << QuoteForLog(reason == 0 ? "manual" : reason)
                << " target_state=" << QuoteForLog(GetPortraitOverlayStateLabel(match.overlayState))
                << " character_pointer=" << QuoteForLog(FormatPointer(match.character))
                << " character_handle=" << QuoteForLog(match.character == 0 ? "" : SafeHandleString(match.character->handle))
                << " character_name=" << QuoteForLog(SafeCharacterName(match.character))
                << " squad_member_id=" << (match.character == 0 ? -1 : static_cast<int>(match.character->squadMemberID))
                << " portrait_index=" << (match.character == 0 ? -1 : static_cast<int>(match.character->portraitIndex))
                << " display_slot_index=" << match.displaySlotIndex
                << " mapping_key=" << QuoteForLog(match.mappingKey == 0 ? "unresolved" : match.mappingKey)
                << " display_portrait_count=" << match.displayPortraitCount
                << " visible_root_count=" << match.visibleRootCount
                << " scanned_nodes=" << match.scannedNodes;
        LogProbeRecord("mark_state_matched_portrait", "summary", payload.str());
        return;
    }

    MyGUI::IntCoord markerBounds;
    if (!PlaceProbeMarkerAtPortrait(match.target, &markerBounds))
    {
        HideHoveredMarker();
        LogProbeRecord(
            "mark_state_matched_portrait",
            "summary",
            "status=no_marker_widget reason=" + QuoteForLog(reason == 0 ? "manual" : reason));
        return;
    }

    std::stringstream payload;
    payload << "status=placed"
            << " reason=" << QuoteForLog(reason == 0 ? "manual" : reason)
            << " target_state=" << QuoteForLog(GetPortraitOverlayStateLabel(match.overlayState))
            << " character_pointer=" << QuoteForLog(FormatPointer(match.character))
            << " character_handle=" << QuoteForLog(SafeHandleString(match.character->handle))
            << " character_name=" << QuoteForLog(SafeCharacterName(match.character))
            << " display_slot_index=" << match.displaySlotIndex
            << " mapping_key=" << QuoteForLog(match.mappingKey == 0 ? "unresolved" : match.mappingKey)
            << " portrait_index=" << static_cast<int>(match.character->portraitIndex)
            << " squad_member_id=" << match.character->squadMemberID
            << " state_label=" << QuoteForLog(match.state.stateLabel)
            << " target_pointer=" << QuoteForLog(FormatPointer(match.target.widget))
            << " target_bounds=" << QuoteForLog(FormatCoord(match.target.absoluteCoord))
            << " marker_bounds=" << QuoteForLog(FormatCoord(markerBounds))
            << " anchor=" << QuoteForLog("bottom_left")
            << " lifetime_ms=" << kHoveredMarkerLifetimeMs
            << " display_portrait_count=" << match.displayPortraitCount
            << " visible_root_count=" << match.visibleRootCount
            << " scanned_nodes=" << match.scannedNodes
            << " confidence_score=" << FormatFloat2(1.00f)
            << " confidence_reason=" << QuoteForLog(
                   std::string("strict_unconscious,")
                   + (match.mappingKey == 0 ? "unresolved" : match.mappingKey)
                   + ",portraitimage_widget_order");
    LogProbeRecord("mark_state_matched_portrait", "summary", payload.str());
}

bool TryMarkStateMatchedPortraitProbeSeh(const char* reason)
{
    __try
    {
        MarkStateMatchedPortraitProbeImpl(reason);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void MarkStateMatchedPortraitProbe(const char* reason)
{
    EnsureActiveProbeSession(reason);
    BeginProbeLogging();
    if (!TryMarkStateMatchedPortraitProbeSeh(reason))
    {
        HideHoveredMarker();
        LogProbeRecord("mark_state_matched_portrait", "exception", "status=seh_guard");
    }
    EndProbeLogging();
}

void RunMappingProbeChain(const char* reason)
{
    const char* effectiveReason = reason == 0 ? "manual" : reason;
    StartNewProbeSession(effectiveReason);

    BeginProbeLogging();
    LogProbeRecord(
        "probe_chain",
        "summary",
        "status=started reason=" + QuoteForLog(effectiveReason)
            + " chain=" + QuoteForLog("dump_portrait_candidates,dump_selected_squad_members,dump_member_states,mark_state_matched_portrait"));
    EndProbeLogging();

    DumpPortraitCandidatesProbe(effectiveReason);
    DumpSelectedSquadMembersProbe(effectiveReason);
    DumpMemberStatesProbe(effectiveReason);
    MarkStateMatchedPortraitProbe(effectiveReason);

    BeginProbeLogging();
    LogProbeRecord(
        "probe_chain",
        "summary",
        "status=completed reason=" + QuoteForLog(effectiveReason)
            + " chain=" + QuoteForLog("dump_portrait_candidates,dump_selected_squad_members,dump_member_states,mark_state_matched_portrait"));
    EndProbeLogging();
}

bool EnsureHoveredMarkerWidget()
{
    if (g_hoveredMarkerWidget != 0)
    {
        return true;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return false;
    }

    g_hoveredMarkerWidget = gui->findWidgetT(kProbeMarkerWidgetName, false);
    if (g_hoveredMarkerWidget == 0)
    {
        g_hoveredMarkerWidget = gui->createWidget<MyGUI::Widget>(
            kProbeMarkerSkin,
            MyGUI::IntCoord(0, 0, kHoveredMarkerMinSizePx, kHoveredMarkerMinSizePx),
            MyGUI::Align::Left | MyGUI::Align::Top,
            "Top",
            kProbeMarkerWidgetName);
    }

    if (g_hoveredMarkerWidget == 0)
    {
        return false;
    }

    g_hoveredMarkerWidget->setNeedMouseFocus(false);
    g_hoveredMarkerWidget->setAlpha(0.95f);
    g_hoveredMarkerWidget->setColour(MyGUI::Colour(1.0f, 0.25f, 0.15f, 0.98f));
    g_hoveredMarkerWidget->setVisible(false);
    return true;
}

PortraitOverlayRuntime* GetPortraitOverlayRuntime(const PortraitOverlayState overlayState)
{
    switch (overlayState)
    {
    case PORTRAIT_OVERLAY_STATE_DYING:
        return &g_dyingOverlayRuntime;
    case PORTRAIT_OVERLAY_STATE_RECOVERY_COMA:
        return &g_recoveryComaOverlayRuntime;
    case PORTRAIT_OVERLAY_STATE_STRICT_UNCONSCIOUS:
    default:
        return &g_unconsciousOverlayRuntime;
    }
}

vr_marker_ui::OverlayStyle BuildPortraitOverlayStyle(const PortraitOverlayState overlayState)
{
    vr_marker_ui::OverlayStyle style;
    style.fallbackSkin = kUnconsciousOverlaySkin;
    style.alpha = 0.92f;
    style.insetPx = kPortraitOverlayInsetPx;
    style.minSizePx = kPortraitOverlayMinSizePx;
    style.maxSizePx = kPortraitOverlayMaxSizePx;

    switch (overlayState)
    {
    case PORTRAIT_OVERLAY_STATE_DYING:
        style.widgetNamePrefix = kDyingOverlayWidgetNamePrefix;
        style.iconTexture = g_dyingIconTexture;
        style.iconTextureSizePx = static_cast<int>(g_dyingIconSizePx);
        style.hasIconImageCoord = g_config.dyingIconHasImageCoord;
        style.iconImageCoord = vr_marker_ui::Rect(
            g_config.dyingIconCoordLeft,
            g_config.dyingIconCoordTop,
            g_config.dyingIconCoordWidth,
            g_config.dyingIconCoordHeight);
        style.colour = MyGUI::Colour(1.0f, 0.24f, 0.24f, 0.95f);
        break;
    case PORTRAIT_OVERLAY_STATE_RECOVERY_COMA:
        style.widgetNamePrefix = kRecoveryComaOverlayWidgetNamePrefix;
        style.iconTexture = g_recoveryComaIconTexture;
        style.iconTextureSizePx = static_cast<int>(g_recoveryComaIconSizePx);
        style.hasIconImageCoord = g_config.recoveryComaIconHasImageCoord;
        style.iconImageCoord = vr_marker_ui::Rect(
            g_config.recoveryComaIconCoordLeft,
            g_config.recoveryComaIconCoordTop,
            g_config.recoveryComaIconCoordWidth,
            g_config.recoveryComaIconCoordHeight);
        style.colour = MyGUI::Colour(0.20f, 0.82f, 0.60f, 0.95f);
        break;
    case PORTRAIT_OVERLAY_STATE_STRICT_UNCONSCIOUS:
    default:
        style.widgetNamePrefix = kUnconsciousOverlayWidgetNamePrefix;
        style.iconTexture = g_unconsciousIconTexture;
        style.iconTextureSizePx = static_cast<int>(g_unconsciousIconSizePx);
        style.hasIconImageCoord = g_config.unconsciousIconHasImageCoord;
        style.iconImageCoord = vr_marker_ui::Rect(
            g_config.unconsciousIconCoordLeft,
            g_config.unconsciousIconCoordTop,
            g_config.unconsciousIconCoordWidth,
            g_config.unconsciousIconCoordHeight);
        style.colour = MyGUI::Colour(1.0f, 0.42f, 0.10f, 0.95f);
        break;
    }

    return style;
}

void HideHoveredMarker()
{
    if (g_hoveredMarkerWidget != 0)
    {
        g_hoveredMarkerWidget->setVisible(false);
    }
    g_hoveredMarkerExpireTick = 0u;
}

void HidePortraitOverlay(PortraitOverlayRuntime* overlayRuntime)
{
    if (overlayRuntime == 0)
    {
        return;
    }

    vr_marker_ui::HideWidgets(&overlayRuntime->widgets);
}

void ResetPortraitOverlayRuntime(PortraitOverlayRuntime* overlayRuntime)
{
    if (overlayRuntime == 0)
    {
        return;
    }

    HidePortraitOverlay(overlayRuntime);
    overlayRuntime->nextRefreshTick = 0u;
}

void ResetStateOverlays()
{
    ResetPortraitOverlayRuntime(&g_unconsciousOverlayRuntime);
    ResetPortraitOverlayRuntime(&g_recoveryComaOverlayRuntime);
    ResetPortraitOverlayRuntime(&g_dyingOverlayRuntime);
}

void TickHoveredMarker()
{
    if (g_hoveredMarkerWidget == 0 || !g_hoveredMarkerWidget->getVisible() || g_hoveredMarkerExpireTick == 0u)
    {
        return;
    }

    if (GetTickCount() >= g_hoveredMarkerExpireTick)
    {
        HideHoveredMarker();
    }
}

void RefreshPortraitOverlayImpl(const PortraitOverlayState overlayState)
{
    PortraitOverlayRuntime* overlayRuntime = GetPortraitOverlayRuntime(overlayState);
    if (overlayRuntime == 0)
    {
        return;
    }

    std::vector<PortraitOverlayMatch> matches;
    PortraitOverlayMatchStatus failureStatus = PORTRAIT_OVERLAY_MATCH_NO_PLAYER_INTERFACE;
    if (!TryCollectPortraitOverlayMatches(overlayState, &matches, &failureStatus)
        || failureStatus != PORTRAIT_OVERLAY_MATCH_OK
        || matches.empty())
    {
        HidePortraitOverlay(overlayRuntime);
        return;
    }

    MyGUI::IntSize rawViewSize(0, 0);
    vr_marker_ui::ViewSize viewSize;
    vr_marker_ui::ViewSize* viewSizePtr = 0;
    if (TryGetViewSize(&rawViewSize))
    {
        viewSize = vr_marker_ui::ViewSize(rawViewSize.width, rawViewSize.height);
        viewSizePtr = &viewSize;
    }

    const vr_marker_ui::OverlayStyle overlayStyle = BuildPortraitOverlayStyle(overlayState);
    size_t visibleWidgetCount = 0u;
    for (size_t index = 0u; index < matches.size(); ++index)
    {
        if (!vr_marker_ui::ShowOverlayMarker(
                &overlayRuntime->widgets,
                visibleWidgetCount,
                kPluginName,
                overlayStyle,
                vr_marker_ui::Rect(
                    matches[index].target.absoluteCoord.left,
                    matches[index].target.absoluteCoord.top,
                    matches[index].target.absoluteCoord.width,
                    matches[index].target.absoluteCoord.height),
                viewSizePtr))
        {
            continue;
        }

        ++visibleWidgetCount;
    }

    vr_marker_ui::HideWidgetsFrom(&overlayRuntime->widgets, visibleWidgetCount);

    if (visibleWidgetCount == 0u)
    {
        HidePortraitOverlay(overlayRuntime);
    }
}

bool TryRefreshPortraitOverlaySeh(const PortraitOverlayState overlayState)
{
    __try
    {
        RefreshPortraitOverlayImpl(overlayState);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void TickPortraitOverlay(const PortraitOverlayState overlayState)
{
    PortraitOverlayRuntime* overlayRuntime = GetPortraitOverlayRuntime(overlayState);
    if (overlayRuntime == 0)
    {
        return;
    }

    const DWORD now = GetTickCount();
    if (overlayRuntime->nextRefreshTick != 0u && now < overlayRuntime->nextRefreshTick)
    {
        return;
    }

    overlayRuntime->nextRefreshTick = now + kPortraitOverlayRefreshIntervalMs;
    if (!TryRefreshPortraitOverlaySeh(overlayState))
    {
        HidePortraitOverlay(overlayRuntime);
    }
}

void TickStateOverlays()
{
    TickPortraitOverlay(PORTRAIT_OVERLAY_STATE_STRICT_UNCONSCIOUS);
    TickPortraitOverlay(PORTRAIT_OVERLAY_STATE_RECOVERY_COMA);
    TickPortraitOverlay(PORTRAIT_OVERLAY_STATE_DYING);
}

bool ResolveHoveredPortraitTarget(MyGUI::Widget* hoveredWidget, PortraitCandidateRecord* outRecord)
{
    if (outRecord == 0)
    {
        return false;
    }

    PortraitCandidateRecord best;
    for (MyGUI::Widget* current = hoveredWidget; current != 0; current = current->getParent())
    {
        PortraitCandidateRecord candidate = BuildPortraitCandidateRecord(current, hoveredWidget);
        if (IsAcceptableHoveredPortraitCandidate(candidate) && candidate.score > best.score)
        {
            best = candidate;
        }
    }

    if (best.widget == 0)
    {
        return false;
    }

    *outRecord = best;
    return true;
}

void MarkHoveredPortraitProbeImpl(const char* reason)
{
    HoverContext hover;
    const bool hoverAvailable = TryGetHoverContext(&hover);
    if (!hoverAvailable)
    {
        LogProbeRecord(
            "mark_hovered_portrait",
            "summary",
            "status=no_input_manager reason=" + QuoteForLog(reason == 0 ? "manual" : reason));
        return;
    }

    PortraitCandidateRecord target;
    if (!ResolveHoveredPortraitTarget(hover.hoveredWidget, &target))
    {
        HideHoveredMarker();

        PortraitCandidateRecord hoveredRecord = BuildPortraitCandidateRecord(hover.hoveredWidget, hover.hoveredWidget);
        std::stringstream payload;
        payload << "status=no_match"
                << " reason=" << QuoteForLog(reason == 0 ? "manual" : reason)
                << " hovered_pointer=" << QuoteForLog(FormatPointer(hover.hoveredWidget))
                << " confidence_score=" << FormatFloat2(hoveredRecord.score)
                << " confidence_reason=" << QuoteForLog(hoveredRecord.reason);
        LogProbeRecord("mark_hovered_portrait", "summary", payload.str());
        return;
    }

    if (!EnsureHoveredMarkerWidget())
    {
        LogProbeRecord(
            "mark_hovered_portrait",
            "summary",
            "status=no_marker_widget reason=" + QuoteForLog(reason == 0 ? "manual" : reason));
        return;
    }

    MyGUI::IntCoord markerBounds;
    if (!PlaceProbeMarkerAtPortrait(target, &markerBounds))
    {
        LogProbeRecord(
            "mark_hovered_portrait",
            "summary",
            "status=no_marker_widget reason=" + QuoteForLog(reason == 0 ? "manual" : reason));
        return;
    }

    std::stringstream payload;
    payload << "status=placed"
            << " reason=" << QuoteForLog(reason == 0 ? "manual" : reason)
            << " hovered_pointer=" << QuoteForLog(FormatPointer(hover.hoveredWidget))
            << " target_pointer=" << QuoteForLog(FormatPointer(target.widget))
            << " target_bounds=" << QuoteForLog(FormatCoord(target.absoluteCoord))
            << " marker_bounds=" << QuoteForLog(FormatCoord(markerBounds))
            << " anchor=" << QuoteForLog("bottom_left")
            << " lifetime_ms=" << kHoveredMarkerLifetimeMs
            << " confidence_score=" << FormatFloat2(target.score)
            << " confidence_reason=" << QuoteForLog(target.reason);
    LogProbeRecord("mark_hovered_portrait", "summary", payload.str());
}

bool TryMarkHoveredPortraitProbeSeh(const char* reason)
{
    __try
    {
        MarkHoveredPortraitProbeImpl(reason);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void MarkHoveredPortraitProbe(const char* reason)
{
    EnsureActiveProbeSession(reason);
    BeginProbeLogging();
    if (!TryMarkHoveredPortraitProbeSeh(reason))
    {
        HideHoveredMarker();
        LogProbeRecord("mark_hovered_portrait", "exception", "status=seh_guard");
    }
    EndProbeLogging();
}

bool AreProbeModifiersPressed(const InputHandler* inputHandler)
{
    return inputHandler != 0
        && inputHandler->ctrl
        && inputHandler->alt
        && !inputHandler->shift;
}

void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    if (PlayerInterface_updateUT_orig != 0)
    {
        PlayerInterface_updateUT_orig(thisptr);
    }

    if (!g_enabled)
    {
        HideHoveredMarker();
        ResetStateOverlays();
        return;
    }

    TickHoveredMarker();
    TickStateOverlays();
}

void InputHandler_keyDownEvent_hook(InputHandler* thisptr, OIS::KeyCode keyCode)
{
    if (g_enabled && AreProbeModifiersPressed(thisptr))
    {
        if (keyCode == kRunMappingProbeHotkey)
        {
            RunMappingProbeChain("manual_hotkey");
            return;
        }

        if (keyCode == kProbeNewSessionHotkey)
        {
            StartNewProbeSession("manual_hotkey");
            return;
        }

        if (keyCode == kMarkStateMatchedPortraitHotkey)
        {
            MarkStateMatchedPortraitProbe("manual_hotkey");
            return;
        }

        if (keyCode == kDumpHoveredWidgetHotkey)
        {
            DumpHoveredWidgetProbe("manual_hotkey");
            return;
        }

        if (keyCode == kDumpPortraitBarTreeHotkey)
        {
            DumpPortraitBarTreeProbe("manual_hotkey");
            return;
        }

        if (keyCode == kDumpPortraitCandidatesHotkey)
        {
            DumpPortraitCandidatesProbe("manual_hotkey");
            return;
        }

        if (keyCode == kMarkHoveredPortraitHotkey)
        {
            MarkHoveredPortraitProbe("manual_hotkey");
            return;
        }

        if (keyCode == kDumpSelectedSquadMembersHotkey)
        {
            DumpSelectedSquadMembersProbe("manual_hotkey");
            return;
        }

        if (keyCode == kDumpMemberStatesHotkey)
        {
            DumpMemberStatesProbe("manual_hotkey");
            return;
        }
    }

    if (InputHandler_keyDownEvent_orig != 0)
    {
        InputHandler_keyDownEvent_orig(thisptr, keyCode);
    }
}

#include "VitalReadModHub.inl"
}

__declspec(dllexport) void startPlugin()
{
    LogInfoLine("startPlugin()");

    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    if (!IsSupportedVersion(versionInfo))
    {
        std::stringstream error;
        error << "unsupported Kenshi version/platform"
              << " version=" << versionInfo.GetVersion()
              << " platform=" << versionInfo.GetPlatform();
        LogErrorLine(error.str());
        return;
    }

    std::stringstream versionLine;
    versionLine << "supported Kenshi version detected: " << versionInfo.GetVersion();
    LogInfoLine(versionLine.str());

    LoadLoggingConfig();

    const intptr_t updateUTTarget = KenshiLib::GetRealAddress(&PlayerInterface::updateUT);
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        updateUTTarget,
        PlayerInterface_updateUT_hook,
        &PlayerInterface_updateUT_orig))
    {
        LogErrorLine("could not hook PlayerInterface::updateUT");
        return;
    }

    const intptr_t keyDownEventTarget = KenshiLib::GetRealAddress(&InputHandler::keyDownEvent);
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        keyDownEventTarget,
        InputHandler_keyDownEvent_hook,
        &InputHandler_keyDownEvent_orig))
    {
        LogErrorLine("could not hook InputHandler::keyDownEvent");
        return;
    }

    ConfigureModHubClient();
    StartModHubClient();

    LogDebugLine("runtime debug logging is enabled");
    LogSearchDebugLine("search diagnostics are enabled");
    LogBindingDebugLine("binding diagnostics are enabled");

    if (g_enabled)
    {
        LogInfoLine(
            "probe hotkeys ready: full mapping chain Ctrl+Alt+F4, matched state marker Ctrl+Alt+F5, start session Ctrl+Alt+F6, hovered widget Ctrl+Alt+F7, portrait tree Ctrl+Alt+F8, portrait candidates Ctrl+Alt+F9, hovered marker Ctrl+Alt+F10, selected squad members Ctrl+Alt+F11, member states Ctrl+Alt+F12");
    }
    else
    {
        LogInfoLine("plugin disabled via mod-config.json; Mod Hub remains available and runtime probes are inactive");
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        char dllPath[MAX_PATH] = { 0 };
        if (GetModuleFileNameA(hModule, dllPath, MAX_PATH) > 0)
        {
            const std::string fullPath(dllPath);
            const std::string::size_type sep = fullPath.find_last_of("\\/");
            if (sep != std::string::npos)
            {
                g_configPath = fullPath.substr(0, sep) + "\\mod-config.json";
            }
        }
    }

    return TRUE;
}
