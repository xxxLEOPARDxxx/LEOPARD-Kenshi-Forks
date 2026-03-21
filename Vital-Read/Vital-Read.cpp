#include <Debug.h>

#include <core/Functions.h>
#include <emc/mod_hub_client.h>
#include <emc/mod_hub_consumer_helpers.h>
#include <kenshi/InputHandler.h>
#include <kenshi/Kenshi.h>
#include <kenshi/PlayerInterface.h>

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
const char* kProbeMarkerSkin = "Kenshi_GenericTextBoxFlatSkin";

const OIS::KeyCode kProbeNewSessionHotkey = OIS::KC_F6;
const OIS::KeyCode kDumpHoveredWidgetHotkey = OIS::KC_F7;
const OIS::KeyCode kDumpPortraitBarTreeHotkey = OIS::KC_F8;
const OIS::KeyCode kDumpPortraitCandidatesHotkey = OIS::KC_F9;
const OIS::KeyCode kMarkHoveredPortraitHotkey = OIS::KC_F10;

const DWORD kHoveredMarkerLifetimeMs = 1500;
const int kHoveredMarkerInsetPx = 2;
const int kHoveredMarkerMinSizePx = 10;
const int kHoveredMarkerMaxSizePx = 18;
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
bool g_enabled = true;
bool g_debugLogging = false;
bool g_debugSearchLogging = false;
bool g_debugBindingLogging = false;
unsigned int g_probeLogScopeDepth = 0u;
unsigned int g_nextProbeSessionId = 1u;
unsigned int g_activeProbeSessionId = 0u;
unsigned int g_activeProbeSequence = 0u;
DWORD g_hoveredMarkerExpireTick = 0u;
MyGUI::Widget* g_hoveredMarkerWidget = 0;

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

bool TryReadTextFile(const std::string& path, std::string* outContent)
{
    if (outContent == 0)
    {
        return false;
    }

    std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
    if (!input)
    {
        return false;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    if (!input.good() && !input.eof())
    {
        return false;
    }

    *outContent = buffer.str();
    return true;
}

bool TryWriteTextFile(const std::string& path, const std::string& content)
{
    std::ofstream output(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output)
    {
        return false;
    }

    output << content;
    output.flush();
    return output.good();
}

bool TryParseJsonBoolByKey(const std::string& content, const char* key, bool* outValue)
{
    if (key == 0 || outValue == 0)
    {
        return false;
    }

    const std::string needle = std::string("\"") + key + "\"";
    const std::string::size_type keyPos = content.find(needle);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    std::string::size_type valuePos = content.find(':', keyPos + needle.size());
    if (valuePos == std::string::npos)
    {
        return false;
    }

    ++valuePos;
    while (valuePos < content.size()
        && std::isspace(static_cast<unsigned char>(content[valuePos])) != 0)
    {
        ++valuePos;
    }

    if (content.compare(valuePos, 4, "true") == 0)
    {
        *outValue = true;
        return true;
    }

    if (content.compare(valuePos, 5, "false") == 0)
    {
        *outValue = false;
        return true;
    }

    return false;
}

void LoadLoggingConfig()
{
    g_enabled = true;
    g_debugLogging = false;
    g_debugSearchLogging = false;
    g_debugBindingLogging = false;

    std::string configPath;
    if (!TryResolveModConfigPath(&configPath))
    {
        LogWarnLine("mod config load skipped: could not resolve plugin directory (using quiet logging defaults)");
        return;
    }

    std::string configText;
    if (!TryReadTextFile(configPath, &configText))
    {
        std::stringstream line;
        line << "mod config load skipped: could not read " << configPath
             << " (using quiet logging defaults)";
        LogWarnLine(line.str());
        return;
    }

    bool parsedValue = false;
    if (TryParseJsonBoolByKey(configText, "enabled", &parsedValue))
    {
        g_enabled = parsedValue;
    }
    if (TryParseJsonBoolByKey(configText, "debugLogging", &parsedValue))
    {
        g_debugLogging = parsedValue;
    }
    if (TryParseJsonBoolByKey(configText, "debugSearchLogging", &parsedValue))
    {
        g_debugSearchLogging = parsedValue;
    }
    if (TryParseJsonBoolByKey(configText, "debugBindingLogging", &parsedValue))
    {
        g_debugBindingLogging = parsedValue;
    }

    LogInfoLine("mod config loaded");

    if (ShouldLogDebug())
    {
        std::stringstream line;
        line << "logging flags enabled=" << (g_enabled ? "true" : "false")
             << " debugLogging=" << (g_debugLogging ? "true" : "false")
             << " debugSearchLogging=" << (g_debugSearchLogging ? "true" : "false")
             << " debugBindingLogging=" << (g_debugBindingLogging ? "true" : "false")
             << " verboseDiagnostics=" << (ShouldCompileVerboseDiagnostics() ? "true" : "false");
        LogDebugLine(line.str());
    }
}

std::string BuildConfigText()
{
    std::stringstream out;
    out << "{\n";
    out << "  \"enabled\": " << (g_enabled ? "true" : "false") << ",\n";
    out << "  \"debugLogging\": " << (g_debugLogging ? "true" : "false") << ",\n";
    out << "  \"debugSearchLogging\": " << (g_debugSearchLogging ? "true" : "false") << ",\n";
    out << "  \"debugBindingLogging\": " << (g_debugBindingLogging ? "true" : "false") << "\n";
    out << "}\n";
    return out.str();
}

bool SaveConfigState()
{
    std::string configPath;
    if (!TryResolveModConfigPath(&configPath))
    {
        LogErrorLine("settings path is empty; cannot save mod-config.json");
        return false;
    }

    if (!TryWriteTextFile(configPath, BuildConfigText()))
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

void DumpPortraitCandidatesProbeImpl(const char* reason)
{
    HoverContext hover;
    const bool hoverAvailable = TryGetHoverContext(&hover);
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        LogProbeRecord(
            "dump_portrait_candidates",
            "summary",
            "status=no_gui reason=" + QuoteForLog(reason == 0 ? "manual" : reason));
        return;
    }

    MyGUI::IntSize viewSize(0, 0);
    const bool haveViewSize = TryGetViewSize(&viewSize);

    size_t visibleRootCount = 0u;
    size_t scannedNodes = 0u;
    std::vector<PortraitCandidateRecord> candidates;

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next() && scannedNodes < kPortraitCandidateScanLimit)
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        ++visibleRootCount;
        CollectPortraitCandidatesRecursive(root, hover.hoveredWidget, &candidates, &scannedNodes);
    }

    std::stable_sort(candidates.begin(), candidates.end(), PortraitCandidateSortPredicate);

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

void HideHoveredMarker()
{
    if (g_hoveredMarkerWidget != 0)
    {
        g_hoveredMarkerWidget->setVisible(false);
    }
    g_hoveredMarkerExpireTick = 0u;
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

    const int smallerSide = target.absoluteCoord.width < target.absoluteCoord.height
        ? target.absoluteCoord.width
        : target.absoluteCoord.height;
    const int markerSize = ClampInt(
        smallerSide / 4,
        kHoveredMarkerMinSizePx,
        kHoveredMarkerMaxSizePx);

    MyGUI::IntSize viewSize(0, 0);
    const bool haveViewSize = TryGetViewSize(&viewSize);

    int markerLeft = target.absoluteCoord.left + kHoveredMarkerInsetPx;
    int markerTop = target.absoluteCoord.top + target.absoluteCoord.height - markerSize - kHoveredMarkerInsetPx;
    if (haveViewSize)
    {
        const int maxLeft = viewSize.width - markerSize > 0 ? viewSize.width - markerSize : 0;
        const int maxTop = viewSize.height - markerSize > 0 ? viewSize.height - markerSize : 0;
        markerLeft = ClampInt(markerLeft, 0, maxLeft);
        markerTop = ClampInt(markerTop, 0, maxTop);
    }

    g_hoveredMarkerWidget->setCoord(markerLeft, markerTop, markerSize, markerSize);
    g_hoveredMarkerWidget->setVisible(true);
    g_hoveredMarkerExpireTick = GetTickCount() + kHoveredMarkerLifetimeMs;

    std::stringstream payload;
    payload << "status=placed"
            << " reason=" << QuoteForLog(reason == 0 ? "manual" : reason)
            << " hovered_pointer=" << QuoteForLog(FormatPointer(hover.hoveredWidget))
            << " target_pointer=" << QuoteForLog(FormatPointer(target.widget))
            << " target_bounds=" << QuoteForLog(FormatCoord(target.absoluteCoord))
            << " marker_bounds=" << QuoteForLog(FormatCoord(MyGUI::IntCoord(markerLeft, markerTop, markerSize, markerSize)))
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
        return;
    }

    TickHoveredMarker();
}

void InputHandler_keyDownEvent_hook(InputHandler* thisptr, OIS::KeyCode keyCode)
{
    if (g_enabled && AreProbeModifiersPressed(thisptr))
    {
        if (keyCode == kProbeNewSessionHotkey)
        {
            StartNewProbeSession("manual_hotkey");
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
            "phase 1 probes ready: start session Ctrl+Alt+F6, hovered widget Ctrl+Alt+F7, portrait tree Ctrl+Alt+F8, portrait candidates Ctrl+Alt+F9, hovered marker Ctrl+Alt+F10");
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
