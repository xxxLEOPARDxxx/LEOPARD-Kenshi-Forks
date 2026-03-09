#include <Debug.h>

#ifndef BOOST_ALL_NO_LIB
#define BOOST_ALL_NO_LIB
#endif

#ifndef BOOST_ERROR_CODE_HEADER_ONLY
#define BOOST_ERROR_CODE_HEADER_ONLY
#endif

#include <core/Functions.h>
#include <kenshi/InputHandler.h>
#include <kenshi/Kenshi.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/SaveFileSystem.h>
#include <kenshi/SaveManager.h>

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_EditBox.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_ImageBox.h>
#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Window.h>

#include <ois/OISKeyboard.h>

#include <Windows.h>

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
const char* kPluginName = "Map-markers";
const OIS::KeyCode kProbeSnapshotHotkey = OIS::KC_F7;
const OIS::KeyCode kProbeLiveHotkey = OIS::KC_F8;
const int kMarkerSize = 18;
const int kSelectedMarkerSize = 26;
const int kMinimumMapImageSize = 200;
const int kMarkerEditorPanelWidth = 256;
const int kMarkerEditorPanelHeight = 106;
const int kMarkerLabelMaxLength = 48;
const char* kMarkerPersistenceFileName = "Map-markers.json";
const char* kMarkerWidgetNamePrefix = "MapMarkers_Marker_";
const char* kMarkerEditorPanelName = "MapMarkers_EditorPanel";
const char* kMarkerEditorHeaderName = "MapMarkers_EditorHeader";
const char* kMarkerEditorTypeButtonName = "MapMarkers_EditorTypeButton";
const char* kMarkerEditorLabelTitleName = "MapMarkers_EditorLabelTitle";
const char* kMarkerEditorLabelEditName = "MapMarkers_EditorLabelEdit";
const char* kMarkerEditorHintName = "MapMarkers_EditorHint";

void (*PlayerInterface_updateUT_orig)(PlayerInterface*) = 0;
void (*InputHandler_keyDownEvent_orig)(InputHandler*, OIS::KeyCode) = 0;

enum MarkerType
{
    MarkerType_Note = 0,
    MarkerType_Stash = 1,
    MarkerType_Danger = 2
};

struct MarkerState
{
    int id;
    float normalizedX;
    float normalizedY;
    MarkerType type;
    std::string label;
};

bool g_probeLive = false;
DWORD g_lastVisibleRootsScanTick = 0;
DWORD g_lastHoverLogTick = 0;
std::string g_lastVisibleRootsSignature;
std::string g_lastHoveredSignature;
std::string g_lastSaveIdentity;
std::string g_lastMarkerRenderSignature;
std::vector<MarkerState> g_markers;
int g_selectedMarkerId = 0;
int g_nextMarkerId = 1;
bool g_lastLeftMouseDownObserved = false;
bool g_lastMiddleMouseDownObserved = false;
bool g_lastRightMouseDownObserved = false;
bool g_suppressNextMarkerLabelChangeEvent = false;

void LogProbeLine(const std::string& message);

bool IsSupportedVersion(KenshiLib::BinaryVersion versionInfo)
{
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    return platform != KenshiLib::BinaryVersion::UNKNOWN
        && (version == "1.0.65" || version == "1.0.68");
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

    const std::string needleLower = ToLowerAscii(needle);
    return ToLowerAscii(haystack).find(needleLower) != std::string::npos;
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

float ClampFloat(float value, float minimum, float maximum)
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

std::string SanitizeMarkerLabel(const std::string& value)
{
    std::string sanitized;
    sanitized.reserve(value.size());

    bool previousWasSpace = false;
    for (std::string::size_type index = 0; index < value.size(); ++index)
    {
        unsigned char ch = static_cast<unsigned char>(value[index]);
        char out = static_cast<char>(ch);

        if (ch == '\r' || ch == '\n' || ch == '\t')
        {
            out = ' ';
        }
        else if (out == '[' || out == '{')
        {
            out = '(';
        }
        else if (out == ']' || out == '}')
        {
            out = ')';
        }
        else if (ch < 32)
        {
            continue;
        }

        if (out == ' ')
        {
            if (sanitized.empty() || previousWasSpace)
            {
                continue;
            }
            previousWasSpace = true;
        }
        else
        {
            previousWasSpace = false;
        }

        sanitized.push_back(out);
        if (static_cast<int>(sanitized.size()) >= kMarkerLabelMaxLength)
        {
            break;
        }
    }

    while (!sanitized.empty() && sanitized[sanitized.size() - 1] == ' ')
    {
        sanitized.erase(sanitized.size() - 1);
    }

    return sanitized;
}

const char* MarkerTypeToJsonValue(MarkerType type)
{
    switch (type)
    {
    case MarkerType_Stash:
        return "stash";
    case MarkerType_Danger:
        return "danger";
    case MarkerType_Note:
    default:
        return "note";
    }
}

const char* MarkerTypeToDisplayName(MarkerType type)
{
    switch (type)
    {
    case MarkerType_Stash:
        return "Stash";
    case MarkerType_Danger:
        return "Danger";
    case MarkerType_Note:
    default:
        return "Note";
    }
}

const char* MarkerTypeToGlyph(MarkerType type)
{
    switch (type)
    {
    case MarkerType_Stash:
        return "S";
    case MarkerType_Danger:
        return "!";
    case MarkerType_Note:
    default:
        return "N";
    }
}

MarkerType MarkerTypeFromString(const std::string& value)
{
    const std::string lowered = ToLowerAscii(value);
    if (lowered == "stash")
    {
        return MarkerType_Stash;
    }
    if (lowered == "danger")
    {
        return MarkerType_Danger;
    }
    return MarkerType_Note;
}

MarkerType GetNextMarkerType(MarkerType type)
{
    switch (type)
    {
    case MarkerType_Note:
        return MarkerType_Stash;
    case MarkerType_Stash:
        return MarkerType_Danger;
    case MarkerType_Danger:
    default:
        return MarkerType_Note;
    }
}

MyGUI::Colour BuildMarkerColour(MarkerType type, bool selected)
{
    switch (type)
    {
    case MarkerType_Stash:
        return selected
            ? MyGUI::Colour(0.45f, 0.98f, 1.0f, 1.0f)
            : MyGUI::Colour(0.18f, 0.78f, 0.92f, 1.0f);
    case MarkerType_Danger:
        return selected
            ? MyGUI::Colour(1.0f, 0.45f, 0.40f, 1.0f)
            : MyGUI::Colour(0.88f, 0.18f, 0.18f, 1.0f);
    case MarkerType_Note:
    default:
        return selected
            ? MyGUI::Colour(1.0f, 0.92f, 0.30f, 1.0f)
            : MyGUI::Colour(1.0f, 0.55f, 0.24f, 1.0f);
    }
}

std::string BuildMarkerEditorHeader(const MarkerState& marker)
{
    std::stringstream caption;
    caption << "Marker " << marker.id;
    if (!marker.label.empty())
    {
        caption << ": " << marker.label;
    }
    return caption.str();
}

std::string JsonEscapeString(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size() + 8);

    for (std::string::size_type index = 0; index < value.size(); ++index)
    {
        const char ch = value[index];
        switch (ch)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(ch);
            break;
        }
    }

    return escaped;
}

std::string JoinWindowsPath(const std::string& directory, const char* fileName)
{
    if (directory.empty() || fileName == 0 || *fileName == '\0')
    {
        return "";
    }

    if (directory[directory.size() - 1] == '\\' || directory[directory.size() - 1] == '/')
    {
        return directory + fileName;
    }

    return directory + "\\" + fileName;
}

std::string GetActiveSaveDirectory()
{
    SaveFileSystem* saveFileSystem = SaveFileSystem::getSingleton();
    return saveFileSystem == 0 ? "" : saveFileSystem->getActiveSave();
}

std::string GetMarkerPersistencePath()
{
    return JoinWindowsPath(GetActiveSaveDirectory(), kMarkerPersistenceFileName);
}

bool ExtractJsonFloatField(const std::string& contents, const char* key, float& valueOut)
{
    if (key == 0 || *key == '\0')
    {
        return false;
    }

    const std::string quotedKey = std::string("\"") + key + "\"";
    const std::string::size_type keyPos = contents.find(quotedKey);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    const std::string::size_type colonPos = contents.find(':', keyPos + quotedKey.size());
    if (colonPos == std::string::npos)
    {
        return false;
    }

    std::string::size_type valuePos = colonPos + 1;
    while (valuePos < contents.size() && std::isspace(static_cast<unsigned char>(contents[valuePos])))
    {
        ++valuePos;
    }

    char* parseEnd = 0;
    const double parsedValue = std::strtod(contents.c_str() + valuePos, &parseEnd);
    if (parseEnd == contents.c_str() + valuePos)
    {
        return false;
    }

    valueOut = ClampFloat(static_cast<float>(parsedValue), 0.0f, 1.0f);
    return true;
}

bool ExtractJsonIntField(const std::string& contents, const char* key, int& valueOut)
{
    if (key == 0 || *key == '\0')
    {
        return false;
    }

    const std::string quotedKey = std::string("\"") + key + "\"";
    const std::string::size_type keyPos = contents.find(quotedKey);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    const std::string::size_type colonPos = contents.find(':', keyPos + quotedKey.size());
    if (colonPos == std::string::npos)
    {
        return false;
    }

    std::string::size_type valuePos = colonPos + 1;
    while (valuePos < contents.size() && std::isspace(static_cast<unsigned char>(contents[valuePos])))
    {
        ++valuePos;
    }

    char* parseEnd = 0;
    const long parsedValue = std::strtol(contents.c_str() + valuePos, &parseEnd, 10);
    if (parseEnd == contents.c_str() + valuePos)
    {
        return false;
    }

    valueOut = static_cast<int>(parsedValue);
    return true;
}

bool ExtractJsonStringField(const std::string& contents, const char* key, std::string& valueOut)
{
    if (key == 0 || *key == '\0')
    {
        return false;
    }

    const std::string quotedKey = std::string("\"") + key + "\"";
    const std::string::size_type keyPos = contents.find(quotedKey);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    const std::string::size_type colonPos = contents.find(':', keyPos + quotedKey.size());
    if (colonPos == std::string::npos)
    {
        return false;
    }

    std::string::size_type valuePos = colonPos + 1;
    while (valuePos < contents.size() && std::isspace(static_cast<unsigned char>(contents[valuePos])))
    {
        ++valuePos;
    }

    if (valuePos >= contents.size() || contents[valuePos] != '"')
    {
        return false;
    }

    std::string parsed;
    bool escaping = false;
    for (std::string::size_type index = valuePos + 1; index < contents.size(); ++index)
    {
        const char ch = contents[index];
        if (escaping)
        {
            switch (ch)
            {
            case 'n':
                parsed.push_back('\n');
                break;
            case 'r':
                parsed.push_back('\r');
                break;
            case 't':
                parsed.push_back('\t');
                break;
            case '\\':
            case '"':
                parsed.push_back(ch);
                break;
            default:
                parsed.push_back(ch);
                break;
            }
            escaping = false;
            continue;
        }

        if (ch == '\\')
        {
            escaping = true;
            continue;
        }

        if (ch == '"')
        {
            valueOut = parsed;
            return true;
        }

        parsed.push_back(ch);
    }

    return false;
}

bool TryExtractJsonArrayContents(const std::string& contents, const char* key, std::string& arrayContentsOut)
{
    if (key == 0 || *key == '\0')
    {
        return false;
    }

    const std::string quotedKey = std::string("\"") + key + "\"";
    const std::string::size_type keyPos = contents.find(quotedKey);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    const std::string::size_type colonPos = contents.find(':', keyPos + quotedKey.size());
    if (colonPos == std::string::npos)
    {
        return false;
    }

    std::string::size_type arrayStart = colonPos + 1;
    while (arrayStart < contents.size() && std::isspace(static_cast<unsigned char>(contents[arrayStart])))
    {
        ++arrayStart;
    }

    if (arrayStart >= contents.size() || contents[arrayStart] != '[')
    {
        return false;
    }

    int depth = 0;
    for (std::string::size_type index = arrayStart; index < contents.size(); ++index)
    {
        if (contents[index] == '[')
        {
            ++depth;
        }
        else if (contents[index] == ']')
        {
            --depth;
            if (depth == 0)
            {
                arrayContentsOut = contents.substr(arrayStart + 1, index - arrayStart - 1);
                return true;
            }
        }
    }

    return false;
}

bool TryParseMarkersArray(const std::string& contents, std::vector<MarkerState>& markersOut)
{
    std::string arrayContents;
    if (!TryExtractJsonArrayContents(contents, "markers", arrayContents))
    {
        return false;
    }

    markersOut.clear();
    std::string::size_type searchPos = 0;
    while (true)
    {
        const std::string::size_type objectStart = arrayContents.find('{', searchPos);
        if (objectStart == std::string::npos)
        {
            break;
        }

        int depth = 0;
        std::string::size_type objectEnd = std::string::npos;
        for (std::string::size_type index = objectStart; index < arrayContents.size(); ++index)
        {
            if (arrayContents[index] == '{')
            {
                ++depth;
            }
            else if (arrayContents[index] == '}')
            {
                --depth;
                if (depth == 0)
                {
                    objectEnd = index;
                    break;
                }
            }
        }

        if (objectEnd == std::string::npos)
        {
            return false;
        }

        const std::string objectText = arrayContents.substr(objectStart, objectEnd - objectStart + 1);
        MarkerState marker;
        marker.id = 0;
        marker.normalizedX = 0.0f;
        marker.normalizedY = 0.0f;
        marker.type = MarkerType_Note;
        marker.label.clear();
        if (!ExtractJsonIntField(objectText, "id", marker.id)
            || !ExtractJsonFloatField(objectText, "x", marker.normalizedX)
            || !ExtractJsonFloatField(objectText, "y", marker.normalizedY))
        {
            return false;
        }

        std::string typeValue;
        if (ExtractJsonStringField(objectText, "type", typeValue))
        {
            marker.type = MarkerTypeFromString(typeValue);
        }

        std::string labelValue;
        if (ExtractJsonStringField(objectText, "label", labelValue))
        {
            marker.label = SanitizeMarkerLabel(labelValue);
        }

        markersOut.push_back(marker);
        searchPos = objectEnd + 1;
    }

    return true;
}

void ResetMarkersForActiveSave()
{
    g_markers.clear();
    g_selectedMarkerId = 0;
    g_nextMarkerId = 1;
}

int FindMarkerIndexById(int markerId)
{
    if (markerId <= 0)
    {
        return -1;
    }

    for (std::size_t index = 0; index < g_markers.size(); ++index)
    {
        if (g_markers[index].id == markerId)
        {
            return static_cast<int>(index);
        }
    }

    return -1;
}

MarkerState* FindMarkerById(int markerId)
{
    const int index = FindMarkerIndexById(markerId);
    return index < 0 ? 0 : &g_markers[static_cast<std::size_t>(index)];
}

void RefreshNextMarkerId()
{
    int nextMarkerId = 1;
    for (std::size_t index = 0; index < g_markers.size(); ++index)
    {
        if (g_markers[index].id >= nextMarkerId)
        {
            nextMarkerId = g_markers[index].id + 1;
        }
    }
    g_nextMarkerId = nextMarkerId;
}

void SaveMarkersForActiveSave(bool logSuccess = true)
{
    const std::string persistencePath = GetMarkerPersistencePath();
    if (persistencePath.empty())
    {
        LogProbeLine("markers persist skipped: active save path unavailable");
        return;
    }

    std::ofstream output(persistencePath.c_str(), std::ios::out | std::ios::trunc);
    if (!output)
    {
        std::stringstream line;
        line << "markers persist failed path=\"" << persistencePath << "\"";
        LogProbeLine(line.str());
        return;
    }

    output << "{\n"
           << "  \"version\": 3,\n"
           << "  \"markers\": [\n";
    for (std::size_t index = 0; index < g_markers.size(); ++index)
    {
        const MarkerState& marker = g_markers[index];
        output << "    {\n"
               << "      \"id\": " << marker.id << ",\n"
               << "      \"x\": " << marker.normalizedX << ",\n"
               << "      \"y\": " << marker.normalizedY << ",\n"
               << "      \"type\": \"" << MarkerTypeToJsonValue(marker.type) << "\",\n"
               << "      \"label\": \"" << JsonEscapeString(marker.label) << "\"\n"
               << "    }";
        if (index + 1 != g_markers.size())
        {
            output << ",";
        }
        output << "\n";
    }
    output << "  ]\n"
           << "}\n";

    if (!output.good())
    {
        std::stringstream line;
        line << "markers persist failed_write path=\"" << persistencePath << "\"";
        LogProbeLine(line.str());
        return;
    }

    if (logSuccess)
    {
        std::stringstream line;
        line << "markers persisted path=\"" << persistencePath
             << "\" count=" << g_markers.size();
        LogProbeLine(line.str());
    }
}

void LoadMarkersForActiveSave()
{
    ResetMarkersForActiveSave();

    const std::string persistencePath = GetMarkerPersistencePath();
    if (persistencePath.empty())
    {
        return;
    }

    std::ifstream input(persistencePath.c_str(), std::ios::in);
    if (!input)
    {
        std::stringstream line;
        line << "markers persistence missing path=\"" << persistencePath << "\" count=0";
        LogProbeLine(line.str());
        return;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    const std::string contents = buffer.str();

    std::vector<MarkerState> loadedMarkers;
    if (TryParseMarkersArray(contents, loadedMarkers))
    {
        for (std::size_t index = 0; index < loadedMarkers.size(); ++index)
        {
            if (loadedMarkers[index].id <= 0)
            {
                loadedMarkers[index].id = static_cast<int>(index) + 1;
            }

            loadedMarkers[index].normalizedX = ClampFloat(loadedMarkers[index].normalizedX, 0.0f, 1.0f);
            loadedMarkers[index].normalizedY = ClampFloat(loadedMarkers[index].normalizedY, 0.0f, 1.0f);
        }

        g_markers = loadedMarkers;
        RefreshNextMarkerId();
        g_lastMarkerRenderSignature.clear();

        std::stringstream line;
        line << "markers loaded path=\"" << persistencePath
             << "\" count=" << g_markers.size()
             << " format=array";
        LogProbeLine(line.str());
        return;
    }

    float loadedX = 0.0f;
    float loadedY = 0.0f;
    if (ExtractJsonFloatField(contents, "x", loadedX)
        && ExtractJsonFloatField(contents, "y", loadedY))
    {
        MarkerState marker;
        marker.id = 1;
        marker.normalizedX = loadedX;
        marker.normalizedY = loadedY;
        marker.type = MarkerType_Note;
        marker.label.clear();
        g_markers.push_back(marker);
        RefreshNextMarkerId();
        g_lastMarkerRenderSignature.clear();

        std::stringstream line;
        line << "markers loaded path=\"" << persistencePath
             << "\" count=1 format=legacy_single";
        LogProbeLine(line.str());
        return;
    }

    std::stringstream line;
    line << "markers persistence invalid path=\"" << persistencePath << "\" count=0";
    LogProbeLine(line.str());
}

void LogProbeLine(const std::string& message)
{
    std::stringstream line;
    line << kPluginName << " PROBE: " << message;
    DebugLog(line.str().c_str());
}

std::string SafeWidgetName(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "<null>";
    }

    const std::string& name = widget->getName();
    return name.empty() ? "<unnamed>" : name;
}

std::string SafeWidgetType(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "<null>";
    }
    return widget->getTypeName();
}

std::string SafeWindowCaption(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "";
    }

    MyGUI::Window* window = widget->castType<MyGUI::Window>(false);
    return window == 0 ? "" : window->getCaption().asUTF8();
}

std::string BuildWidgetDescriptor(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "<null>";
    }

    const MyGUI::IntCoord coord = widget->getCoord();
    std::stringstream line;
    line << SafeWidgetType(widget)
         << ":" << SafeWidgetName(widget)
         << "@(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";

    const std::string caption = SafeWindowCaption(widget);
    if (!caption.empty())
    {
        line << " caption=\"" << caption << "\"";
    }

    const bool nameHasMap = ContainsAsciiCaseInsensitive(SafeWidgetName(widget), "map");
    const bool typeHasMap = ContainsAsciiCaseInsensitive(SafeWidgetType(widget), "map");
    const bool captionHasMap = ContainsAsciiCaseInsensitive(caption, "map");
    if (nameHasMap || typeHasMap || captionHasMap)
    {
        line << " map_token=true";
    }

    return line.str();
}

std::string BuildWidgetChainForLog(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "<none>";
    }

    std::stringstream line;
    std::size_t depth = 0;
    for (MyGUI::Widget* current = widget; current != 0 && depth < 8; current = current->getParent(), ++depth)
    {
        if (depth != 0)
        {
            line << " <- ";
        }
        line << BuildWidgetDescriptor(current);
    }
    return line.str();
}

bool WidgetNameContains(MyGUI::Widget* widget, const char* token)
{
    return widget != 0 && ContainsAsciiCaseInsensitive(SafeWidgetName(widget), token);
}

bool WidgetChainHasMapIdentity(MyGUI::Widget* widget)
{
    for (MyGUI::Widget* current = widget; current != 0; current = current->getParent())
    {
        if (WidgetNameContains(current, "mapscrollview")
            || WidgetNameContains(current, "maptab")
            || WidgetNameContains(current, "tabsmain")
            || ContainsAsciiCaseInsensitive(SafeWindowCaption(current), "map"))
        {
            return true;
        }
    }

    return false;
}

MyGUI::Widget* FindDescendantByName(MyGUI::Widget* root, const char* name)
{
    if (root == 0 || name == 0 || *name == '\0')
    {
        return 0;
    }

    if (SafeWidgetName(root) == name)
    {
        return root;
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t index = 0; index < childCount; ++index)
    {
        if (MyGUI::Widget* found = FindDescendantByName(root->getChildAt(index), name))
        {
            return found;
        }
    }

    return 0;
}

MyGUI::ImageBox* FindMapImageRecursive(MyGUI::Widget* root)
{
    if (root == 0)
    {
        return 0;
    }

    MyGUI::ImageBox* imageBox = root->castType<MyGUI::ImageBox>(false);
    if (imageBox != 0
        && WidgetNameContains(imageBox, "mapimage")
        && WidgetChainHasMapIdentity(imageBox))
    {
        return imageBox;
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t index = 0; index < childCount; ++index)
    {
        if (MyGUI::ImageBox* found = FindMapImageRecursive(root->getChildAt(index)))
        {
            return found;
        }
    }

    return 0;
}

MyGUI::ImageBox* FindMapImageInParentChain(MyGUI::Widget* widget)
{
    for (MyGUI::Widget* current = widget; current != 0; current = current->getParent())
    {
        MyGUI::ImageBox* imageBox = current->castType<MyGUI::ImageBox>(false);
        if (imageBox != 0
            && WidgetNameContains(imageBox, "mapimage")
            && WidgetChainHasMapIdentity(imageBox))
        {
            return imageBox;
        }
    }

    return 0;
}

MyGUI::ImageBox* FindActiveMapImage()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return 0;
    }

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        if (MyGUI::ImageBox* found = FindMapImageRecursive(root))
        {
            return found;
        }
    }

    return 0;
}

std::string BuildMarkerWidgetName(int markerId)
{
    std::stringstream name;
    name << kMarkerWidgetNamePrefix << markerId;
    return name.str();
}

bool TryParseMarkerWidgetId(const std::string& widgetName, int& markerIdOut)
{
    const std::string prefix = kMarkerWidgetNamePrefix;
    if (widgetName.size() <= prefix.size()
        || widgetName.compare(0, prefix.size(), prefix) != 0)
    {
        return false;
    }

    char* parseEnd = 0;
    const long parsedId = std::strtol(widgetName.c_str() + prefix.size(), &parseEnd, 10);
    if (parseEnd == widgetName.c_str() + prefix.size()
        || *parseEnd != '\0'
        || parsedId <= 0)
    {
        return false;
    }

    markerIdOut = static_cast<int>(parsedId);
    return true;
}

int FindMarkerWidgetIdInChain(MyGUI::Widget* widget)
{
    for (MyGUI::Widget* current = widget; current != 0; current = current->getParent())
    {
        int markerId = 0;
        if (TryParseMarkerWidgetId(SafeWidgetName(current), markerId))
        {
            return markerId;
        }
    }

    return 0;
}

MyGUI::Widget* FindDirectChildByName(MyGUI::Widget* parent, const std::string& name)
{
    if (parent == 0 || name.empty())
    {
        return 0;
    }

    const std::size_t childCount = parent->getChildCount();
    for (std::size_t index = 0; index < childCount; ++index)
    {
        MyGUI::Widget* child = parent->getChildAt(index);
        if (child != 0 && SafeWidgetName(child) == name)
        {
            return child;
        }
    }

    return 0;
}

MyGUI::Window* FindOwningWindow(MyGUI::Widget* widget)
{
    for (MyGUI::Widget* current = widget; current != 0; current = current->getParent())
    {
        MyGUI::Window* window = current->castType<MyGUI::Window>(false);
        if (window != 0)
        {
            return window;
        }
    }

    return 0;
}

MyGUI::Widget* FindMarkerEditorParent(MyGUI::ImageBox* mapImage)
{
    if (mapImage == 0)
    {
        return 0;
    }

    if (MyGUI::Window* window = FindOwningWindow(mapImage))
    {
        return window;
    }

    return mapImage->getParent() == 0 ? mapImage : mapImage->getParent();
}

bool IsWidgetKeyFocused(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return false;
    }

    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    return input != 0 && input->getKeyFocusWidget() == widget;
}

void OnMarkerTypeButtonClicked(MyGUI::Widget*)
{
    MarkerState* selectedMarker = FindMarkerById(g_selectedMarkerId);
    if (selectedMarker == 0)
    {
        return;
    }

    selectedMarker->type = GetNextMarkerType(selectedMarker->type);
    g_lastMarkerRenderSignature.clear();
    SaveMarkersForActiveSave();

    std::stringstream line;
    line << "marker type_changed marker_id=" << selectedMarker->id
         << " type=\"" << MarkerTypeToJsonValue(selectedMarker->type) << "\"";
    LogProbeLine(line.str());
}

void OnMarkerLabelFocusChanged(MyGUI::Widget* sender, MyGUI::Widget*)
{
    MyGUI::EditBox* labelEdit = sender == 0 ? 0 : sender->castType<MyGUI::EditBox>(false);
    if (labelEdit == 0 || IsWidgetKeyFocused(labelEdit))
    {
        return;
    }

    MarkerState* selectedMarker = FindMarkerById(g_selectedMarkerId);
    if (selectedMarker == 0)
    {
        return;
    }

    std::stringstream line;
    line << "marker label_committed marker_id=" << selectedMarker->id
         << " length=" << selectedMarker->label.size();
    LogProbeLine(line.str());
}

void OnMarkerLabelChanged(MyGUI::EditBox* sender)
{
    if (sender == 0)
    {
        return;
    }

    if (g_suppressNextMarkerLabelChangeEvent)
    {
        g_suppressNextMarkerLabelChangeEvent = false;
        return;
    }

    MarkerState* selectedMarker = FindMarkerById(g_selectedMarkerId);
    if (selectedMarker == 0)
    {
        return;
    }

    const std::string currentText = sender->getOnlyText().asUTF8();
    const std::string sanitized = SanitizeMarkerLabel(currentText);
    if (sanitized != currentText)
    {
        g_suppressNextMarkerLabelChangeEvent = true;
        sender->setOnlyText(sanitized);
    }

    if (selectedMarker->label == sanitized)
    {
        return;
    }

    selectedMarker->label = sanitized;
    SaveMarkersForActiveSave(false);
}

bool BuildMarkerEditorUi(MyGUI::Widget* panelParent)
{
    if (panelParent == 0)
    {
        return false;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return false;
    }

    MyGUI::Widget* panel = panelParent->createWidget<MyGUI::Widget>(
        "Kenshi_GenericTextBoxFlatSkin",
        MyGUI::IntCoord(0, 0, kMarkerEditorPanelWidth, kMarkerEditorPanelHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kMarkerEditorPanelName);
    if (panel == 0)
    {
        return false;
    }

    panel->setAlpha(0.98f);

    MyGUI::TextBox* header = panel->createWidget<MyGUI::TextBox>(
        "Kenshi_TextboxStandardText",
        MyGUI::IntCoord(8, 6, kMarkerEditorPanelWidth - 16, 20),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kMarkerEditorHeaderName);
    if (header == 0)
    {
        gui->destroyWidget(panel);
        return false;
    }
    header->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);

    MyGUI::Button* typeButton = panel->createWidget<MyGUI::Button>(
        "Kenshi_Button1",
        MyGUI::IntCoord(8, 30, 112, 24),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kMarkerEditorTypeButtonName);
    if (typeButton == 0)
    {
        gui->destroyWidget(panel);
        return false;
    }
    typeButton->setNeedMouseFocus(true);
    typeButton->eventMouseButtonClick += MyGUI::newDelegate(&OnMarkerTypeButtonClicked);

    MyGUI::TextBox* labelTitle = panel->createWidget<MyGUI::TextBox>(
        "Kenshi_TextboxStandardText",
        MyGUI::IntCoord(8, 62, 44, 20),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kMarkerEditorLabelTitleName);
    if (labelTitle == 0)
    {
        gui->destroyWidget(panel);
        return false;
    }
    labelTitle->setCaption("Label");
    labelTitle->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);

    MyGUI::EditBox* labelEdit = panel->createWidget<MyGUI::EditBox>(
        "Kenshi_EditBox",
        MyGUI::IntCoord(56, 60, kMarkerEditorPanelWidth - 64, 24),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kMarkerEditorLabelEditName);
    if (labelEdit == 0)
    {
        gui->destroyWidget(panel);
        return false;
    }
    labelEdit->eventEditTextChange += MyGUI::newDelegate(&OnMarkerLabelChanged);
    labelEdit->eventKeySetFocus += MyGUI::newDelegate(&OnMarkerLabelFocusChanged);
    labelEdit->eventKeyLostFocus += MyGUI::newDelegate(&OnMarkerLabelFocusChanged);

    MyGUI::TextBox* hint = panel->createWidget<MyGUI::TextBox>(
        "Kenshi_TextboxStandardText",
        MyGUI::IntCoord(8, 86, kMarkerEditorPanelWidth - 16, 16),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kMarkerEditorHintName);
    if (hint == 0)
    {
        gui->destroyWidget(panel);
        return false;
    }
    hint->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);

    return true;
}

void EnsureMarkerEditorUi()
{
    MyGUI::ImageBox* mapImage = FindActiveMapImage();
    if (mapImage == 0 || !mapImage->getInheritedVisible())
    {
        return;
    }

    MyGUI::Widget* panelParent = FindMarkerEditorParent(mapImage);
    if (panelParent == 0 || !panelParent->getInheritedVisible())
    {
        return;
    }

    MyGUI::Widget* panel = FindDirectChildByName(panelParent, kMarkerEditorPanelName);
    MarkerState* selectedMarker = FindMarkerById(g_selectedMarkerId);
    if (selectedMarker == 0)
    {
        if (panel != 0)
        {
            panel->setVisible(false);
        }
        return;
    }

    if (panel == 0)
    {
        if (!BuildMarkerEditorUi(panelParent))
        {
            return;
        }
        panel = FindDirectChildByName(panelParent, kMarkerEditorPanelName);
        if (panel == 0)
        {
            return;
        }
    }

    const MyGUI::IntCoord parentCoord = panelParent->getCoord();
    const int maxLeft = parentCoord.width > kMarkerEditorPanelWidth ? parentCoord.width - kMarkerEditorPanelWidth : 0;
    const int maxTop = parentCoord.height > kMarkerEditorPanelHeight ? parentCoord.height - kMarkerEditorPanelHeight : 0;
    const int panelLeft = ClampInt(parentCoord.width - kMarkerEditorPanelWidth - 18, 0, maxLeft);
    const int panelTop = ClampInt(48, 0, maxTop);

    panel->setVisible(true);
    panel->setCoord(panelLeft, panelTop, kMarkerEditorPanelWidth, kMarkerEditorPanelHeight);

    MyGUI::Widget* headerWidget = FindDirectChildByName(panel, kMarkerEditorHeaderName);
    MyGUI::Widget* typeButtonWidget = FindDirectChildByName(panel, kMarkerEditorTypeButtonName);
    MyGUI::Widget* labelEditWidget = FindDirectChildByName(panel, kMarkerEditorLabelEditName);
    MyGUI::Widget* hintWidget = FindDirectChildByName(panel, kMarkerEditorHintName);

    MyGUI::TextBox* header = headerWidget == 0 ? 0 : headerWidget->castType<MyGUI::TextBox>(false);
    MyGUI::Button* typeButton = typeButtonWidget == 0 ? 0 : typeButtonWidget->castType<MyGUI::Button>(false);
    MyGUI::EditBox* labelEdit = labelEditWidget == 0 ? 0 : labelEditWidget->castType<MyGUI::EditBox>(false);
    MyGUI::TextBox* hint = hintWidget == 0 ? 0 : hintWidget->castType<MyGUI::TextBox>(false);

    if (header != 0)
    {
        header->setCaption(BuildMarkerEditorHeader(*selectedMarker));
    }

    if (typeButton != 0)
    {
        std::stringstream caption;
        caption << "Type: " << MarkerTypeToDisplayName(selectedMarker->type);
        typeButton->setCaption(caption.str());
        typeButton->setColour(BuildMarkerColour(selectedMarker->type, true));
    }

    if (labelEdit != 0 && !IsWidgetKeyFocused(labelEdit))
    {
        const std::string onlyText = labelEdit->getOnlyText().asUTF8();
        if (onlyText != selectedMarker->label)
        {
            g_suppressNextMarkerLabelChangeEvent = true;
            labelEdit->setOnlyText(selectedMarker->label);
        }
    }

    if (hint != 0)
    {
        hint->setCaption("Middle click add | Left click move | Right click deselect");
    }
}

void DestroyStaleMarkerWidgets(MyGUI::ImageBox* mapImage)
{
    if (mapImage == 0)
    {
        return;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return;
    }

    for (std::size_t index = mapImage->getChildCount(); index > 0; --index)
    {
        MyGUI::Widget* child = mapImage->getChildAt(index - 1);
        if (child == 0)
        {
            continue;
        }

        int markerId = 0;
        if (!TryParseMarkerWidgetId(SafeWidgetName(child), markerId))
        {
            continue;
        }

        if (FindMarkerById(markerId) == 0)
        {
            gui->destroyWidget(child);
        }
    }
}

void EnsureMarkerWidgetsAttached()
{
    MyGUI::ImageBox* mapImage = FindActiveMapImage();
    if (mapImage == 0 || !mapImage->getInheritedVisible())
    {
        g_lastMarkerRenderSignature.clear();
        return;
    }

    const MyGUI::IntCoord imageCoord = mapImage->getCoord();
    if (imageCoord.width < kMinimumMapImageSize || imageCoord.height < kMinimumMapImageSize)
    {
        return;
    }

    DestroyStaleMarkerWidgets(mapImage);

    std::stringstream signature;
    signature << SafeWidgetName(mapImage)
              << "|" << imageCoord.width << "x" << imageCoord.height
              << "|selected=" << g_selectedMarkerId
              << "|count=" << g_markers.size();

    std::stringstream line;
    line << "markers rendered"
         << " parent=" << BuildWidgetDescriptor(mapImage)
         << " count=" << g_markers.size()
         << " selected=" << g_selectedMarkerId;

    for (std::size_t index = 0; index < g_markers.size(); ++index)
    {
        const MarkerState& marker = g_markers[index];
        const bool isSelected = marker.id == g_selectedMarkerId;
        const int markerSize = isSelected ? kSelectedMarkerSize : kMarkerSize;
        const int maxLeft = imageCoord.width > markerSize ? imageCoord.width - markerSize : 0;
        const int maxTop = imageCoord.height > markerSize ? imageCoord.height - markerSize : 0;
        const int markerLeft = ClampInt(
            static_cast<int>(imageCoord.width * marker.normalizedX + 0.5f) - (markerSize / 2),
            0,
            maxLeft);
        const int markerTop = ClampInt(
            static_cast<int>(imageCoord.height * marker.normalizedY + 0.5f) - (markerSize / 2),
            0,
            maxTop);

        const std::string widgetName = BuildMarkerWidgetName(marker.id);
        MyGUI::Widget* widgetBase = FindDirectChildByName(mapImage, widgetName);
        MyGUI::Button* widget = widgetBase == 0 ? 0 : widgetBase->castType<MyGUI::Button>(false);
        if (widget == 0)
        {
            widget = mapImage->createWidget<MyGUI::Button>(
                "Kenshi_Button1",
                MyGUI::IntCoord(0, 0, markerSize, markerSize),
                MyGUI::Align::Default,
                widgetName);
        }

        widget->setNeedMouseFocus(true);
        widget->setAlpha(isSelected ? 1.0f : 0.92f);
        widget->setColour(BuildMarkerColour(marker.type, isSelected));
        widget->setCaption(MarkerTypeToGlyph(marker.type));
        widget->setCoord(markerLeft, markerTop, markerSize, markerSize);

        signature << "|" << marker.id << ":" << markerLeft << "," << markerTop << "," << markerSize
                  << ":" << MarkerTypeToJsonValue(marker.type);
        line << " marker[" << marker.id << "]=(" << markerLeft << "," << markerTop << "," << markerSize << ")"
             << ":" << MarkerTypeToJsonValue(marker.type);
    }

    const std::string signatureString = signature.str();
    if (signatureString != g_lastMarkerRenderSignature)
    {
        g_lastMarkerRenderSignature = signatureString;
        LogProbeLine(line.str());
    }
}

std::string BuildVisibleRootsSignature()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return "<no_gui>";
    }

    std::stringstream signature;
    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        signature << BuildWidgetDescriptor(root)
                  << " child_count=" << root->getChildCount()
                  << "\n";
    }

    return signature.str();
}

void LogVisibleRootsSnapshot(const char* reason)
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        LogProbeLine("visible_roots unavailable: MyGUI::Gui singleton missing");
        return;
    }

    std::size_t rootCount = 0;
    std::stringstream header;
    header << "visible_roots reason=" << (reason == 0 ? "<unknown>" : reason);
    LogProbeLine(header.str());

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        std::stringstream line;
        line << "visible_root[" << rootCount << "] "
             << BuildWidgetDescriptor(root)
             << " child_count=" << root->getChildCount();
        LogProbeLine(line.str());
        ++rootCount;
    }

    if (rootCount == 0)
    {
        LogProbeLine("visible_root scan found no visible MyGUI roots");
    }
}

void LogSaveIdentityIfChanged(bool force)
{
    SaveManager* saveManager = SaveManager::getSingleton();
    SaveFileSystem* saveFileSystem = SaveFileSystem::getSingleton();

    const std::string currentGame = saveManager == 0 ? "" : saveManager->getCurrentGame();
    const std::string activeSave = saveFileSystem == 0 ? "" : saveFileSystem->getActiveSave();

    std::stringstream identity;
    identity << currentGame << "\n" << activeSave;
    const std::string identityString = identity.str();
    const bool identityChanged = identityString != g_lastSaveIdentity;
    if (!force && !identityChanged)
    {
        return;
    }

    if (identityChanged)
    {
        g_lastSaveIdentity = identityString;
        LoadMarkersForActiveSave();
    }

    if (!force && currentGame.empty() && activeSave.empty())
    {
        return;
    }

    std::stringstream line;
    line << "save_identity current_game=\"" << currentGame
         << "\" active_save=\"" << activeSave << "\"";
    LogProbeLine(line.str());
}

void LogHoveredWidgetState(bool force)
{
    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        if (force)
        {
            LogProbeLine("hover unavailable: MyGUI::InputManager singleton missing");
        }
        return;
    }

    const MyGUI::IntPoint mouse = inputManager->getMousePosition();
    MyGUI::Widget* hovered = inputManager->getMouseFocusWidget();

    std::stringstream signature;
    signature << mouse.left << "," << mouse.top << "\n" << BuildWidgetChainForLog(hovered);
    const std::string signatureString = signature.str();
    if (!force && signatureString == g_lastHoveredSignature)
    {
        return;
    }

    g_lastHoveredSignature = signatureString;

    std::stringstream line;
    line << "hover mouse=(" << mouse.left << "," << mouse.top << ")"
         << " focus_chain=" << BuildWidgetChainForLog(hovered);
    LogProbeLine(line.str());
}

void TriggerManualSnapshot(const char* reason)
{
    std::stringstream line;
    line << "snapshot reason=" << (reason == 0 ? "<unknown>" : reason);
    LogProbeLine(line.str());
    LogSaveIdentityIfChanged(true);
    LogHoveredWidgetState(true);
    LogVisibleRootsSnapshot(reason);
}

bool AreProbeModifiersPressed(const InputHandler* inputHandler)
{
    return inputHandler != 0
        && inputHandler->ctrl
        && inputHandler->alt
        && !inputHandler->shift;
}

struct MapPointerContext
{
    MyGUI::Widget* hoveredWidget;
    MyGUI::ImageBox* mapImage;
    MyGUI::IntCoord absoluteCoord;
    MyGUI::IntPoint mouse;
    int localLeft;
    int localTop;
    int hoveredMarkerId;
};

bool DidMouseButtonJustGoDown(int virtualKeyCode, bool& lastObserved)
{
    const bool mouseDown = (GetAsyncKeyState(virtualKeyCode) & 0x8000) != 0;
    const bool justWentDown = mouseDown && !lastObserved;
    lastObserved = mouseDown;
    return justWentDown;
}

bool TryBuildMapPointerContext(MapPointerContext& contextOut)
{
    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        return false;
    }

    MyGUI::Widget* hoveredWidget = inputManager->getMouseFocusWidget();
    MyGUI::ImageBox* mapImage = FindMapImageInParentChain(hoveredWidget);
    if (mapImage == 0 || !mapImage->getInheritedVisible())
    {
        return false;
    }

    const MyGUI::IntCoord absoluteCoord = mapImage->getAbsoluteCoord();
    if (absoluteCoord.width <= 0 || absoluteCoord.height <= 0)
    {
        return false;
    }

    const MyGUI::IntPoint mouse = inputManager->getMousePosition();
    if (mouse.left < absoluteCoord.left
        || mouse.top < absoluteCoord.top
        || mouse.left >= absoluteCoord.left + absoluteCoord.width
        || mouse.top >= absoluteCoord.top + absoluteCoord.height)
    {
        return false;
    }

    contextOut.hoveredWidget = hoveredWidget;
    contextOut.mapImage = mapImage;
    contextOut.absoluteCoord = absoluteCoord;
    contextOut.mouse = mouse;
    contextOut.localLeft = mouse.left - absoluteCoord.left;
    contextOut.localTop = mouse.top - absoluteCoord.top;
    contextOut.hoveredMarkerId = FindMarkerWidgetIdInChain(hoveredWidget);
    return true;
}

float BuildNormalizedMapCoordinate(int localCoordinate, int extent)
{
    if (extent <= 0)
    {
        return 0.0f;
    }

    return ClampFloat(static_cast<float>(localCoordinate) / static_cast<float>(extent), 0.0f, 1.0f);
}

void ClearSelectedMarker(const char* reason)
{
    if (g_selectedMarkerId == 0)
    {
        return;
    }

    std::stringstream line;
    line << "marker deselected reason=" << (reason == 0 ? "<unknown>" : reason)
         << " marker_id=" << g_selectedMarkerId;

    g_selectedMarkerId = 0;
    g_lastMarkerRenderSignature.clear();
    LogProbeLine(line.str());
}

void SelectMarker(int markerId, const char* reason)
{
    if (markerId <= 0 || FindMarkerById(markerId) == 0 || g_selectedMarkerId == markerId)
    {
        return;
    }

    g_selectedMarkerId = markerId;
    g_lastMarkerRenderSignature.clear();

    std::stringstream line;
    line << "marker selected reason=" << (reason == 0 ? "<unknown>" : reason)
         << " marker_id=" << markerId;
    LogProbeLine(line.str());
}

void TryAddMarkerFromMiddleClick()
{
    if (!DidMouseButtonJustGoDown(VK_MBUTTON, g_lastMiddleMouseDownObserved))
    {
        return;
    }

    MapPointerContext context;
    if (!TryBuildMapPointerContext(context) || context.hoveredMarkerId != 0)
    {
        return;
    }

    MarkerState marker;
    marker.id = g_nextMarkerId++;
    marker.normalizedX = BuildNormalizedMapCoordinate(context.localLeft, context.absoluteCoord.width);
    marker.normalizedY = BuildNormalizedMapCoordinate(context.localTop, context.absoluteCoord.height);
    marker.type = MarkerType_Note;
    marker.label.clear();
    g_markers.push_back(marker);
    g_selectedMarkerId = marker.id;
    g_lastMarkerRenderSignature.clear();
    SaveMarkersForActiveSave();

    std::stringstream line;
    line << "marker added trigger=MIDDLE_CLICK"
         << " marker_id=" << marker.id
         << " mouse=(" << context.mouse.left << "," << context.mouse.top << ")"
         << " local=(" << context.localLeft << "," << context.localTop << ")"
         << " normalized=(" << marker.normalizedX << "," << marker.normalizedY << ")"
         << " parent=" << BuildWidgetDescriptor(context.mapImage);
    LogProbeLine(line.str());
}

void TryHandleLeftClickSelectionOrMove()
{
    if (!DidMouseButtonJustGoDown(VK_LBUTTON, g_lastLeftMouseDownObserved))
    {
        return;
    }

    MapPointerContext context;
    if (!TryBuildMapPointerContext(context))
    {
        return;
    }

    if (context.hoveredMarkerId != 0)
    {
        SelectMarker(context.hoveredMarkerId, "left_click_marker");
        return;
    }

    MarkerState* selectedMarker = FindMarkerById(g_selectedMarkerId);
    if (selectedMarker == 0)
    {
        return;
    }

    selectedMarker->normalizedX = BuildNormalizedMapCoordinate(context.localLeft, context.absoluteCoord.width);
    selectedMarker->normalizedY = BuildNormalizedMapCoordinate(context.localTop, context.absoluteCoord.height);
    g_lastMarkerRenderSignature.clear();
    SaveMarkersForActiveSave();

    std::stringstream line;
    line << "marker moved trigger=LEFT_CLICK"
         << " marker_id=" << selectedMarker->id
         << " mouse=(" << context.mouse.left << "," << context.mouse.top << ")"
         << " local=(" << context.localLeft << "," << context.localTop << ")"
         << " normalized=(" << selectedMarker->normalizedX << "," << selectedMarker->normalizedY << ")"
         << " parent=" << BuildWidgetDescriptor(context.mapImage);
    LogProbeLine(line.str());
}

void TryDeselectMarkerFromRightClick()
{
    if (g_selectedMarkerId == 0
        || !DidMouseButtonJustGoDown(VK_RBUTTON, g_lastRightMouseDownObserved))
    {
        return;
    }

    MapPointerContext context;
    if (!TryBuildMapPointerContext(context))
    {
        return;
    }

    ClearSelectedMarker("right_click_map");
}

MyGUI::EditBox* FindActiveMarkerLabelEdit()
{
    MyGUI::ImageBox* mapImage = FindActiveMapImage();
    if (mapImage == 0)
    {
        return 0;
    }

    MyGUI::Widget* panelParent = FindMarkerEditorParent(mapImage);
    if (panelParent == 0)
    {
        return 0;
    }

    MyGUI::Widget* panel = FindDirectChildByName(panelParent, kMarkerEditorPanelName);
    if (panel == 0)
    {
        return 0;
    }

    MyGUI::Widget* labelEditWidget = FindDirectChildByName(panel, kMarkerEditorLabelEditName);
    return labelEditWidget == 0 ? 0 : labelEditWidget->castType<MyGUI::EditBox>(false);
}

bool TryHandleMarkerKeyDown(OIS::KeyCode keyCode)
{
    if (FindActiveMapImage() == 0 || g_selectedMarkerId == 0)
    {
        return false;
    }

    if (IsWidgetKeyFocused(FindActiveMarkerLabelEdit()))
    {
        return false;
    }

    if (keyCode != OIS::KC_DELETE)
    {
        return false;
    }

    const int markerIndex = FindMarkerIndexById(g_selectedMarkerId);
    if (markerIndex < 0)
    {
        ClearSelectedMarker("delete_missing_selection");
        return true;
    }

    const int deletedMarkerId = g_selectedMarkerId;
    g_markers.erase(g_markers.begin() + markerIndex);
    g_selectedMarkerId = 0;
    RefreshNextMarkerId();
    g_lastMarkerRenderSignature.clear();
    SaveMarkersForActiveSave();

    std::stringstream line;
    line << "marker deleted trigger=DELETE"
         << " marker_id=" << deletedMarkerId
         << " remaining=" << g_markers.size();
    LogProbeLine(line.str());
    return true;
}

void TickUiDiagnostics()
{
    LogSaveIdentityIfChanged(false);
    TryAddMarkerFromMiddleClick();
    TryHandleLeftClickSelectionOrMove();
    TryDeselectMarkerFromRightClick();
    EnsureMarkerWidgetsAttached();
    EnsureMarkerEditorUi();

    const DWORD now = GetTickCount();
    if (now - g_lastVisibleRootsScanTick >= 500)
    {
        g_lastVisibleRootsScanTick = now;
        const std::string signature = BuildVisibleRootsSignature();
        if (signature != g_lastVisibleRootsSignature)
        {
            g_lastVisibleRootsSignature = signature;
            LogVisibleRootsSnapshot("visible_root_change");
        }
    }

    if (g_probeLive && now - g_lastHoverLogTick >= 200)
    {
        g_lastHoverLogTick = now;
        LogHoveredWidgetState(false);
    }
}

void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    if (PlayerInterface_updateUT_orig)
    {
        PlayerInterface_updateUT_orig(thisptr);
    }

    TickUiDiagnostics();
}

void InputHandler_keyDownEvent_hook(InputHandler* thisptr, OIS::KeyCode keyCode)
{
    const bool modifiersPressed = AreProbeModifiersPressed(thisptr);

    if (modifiersPressed && keyCode == kProbeSnapshotHotkey)
    {
        TriggerManualSnapshot("manual_hotkey");
        return;
    }

    if (modifiersPressed && keyCode == kProbeLiveHotkey)
    {
        g_probeLive = !g_probeLive;

        std::stringstream line;
        line << "live_hover_probe=" << (g_probeLive ? "enabled" : "disabled");
        LogProbeLine(line.str());
        if (g_probeLive)
        {
            TriggerManualSnapshot("live_hover_enabled");
        }
        return;
    }

    if (TryHandleMarkerKeyDown(keyCode))
    {
        return;
    }

    if (InputHandler_keyDownEvent_orig)
    {
        InputHandler_keyDownEvent_orig(thisptr, keyCode);
    }
}
}

__declspec(dllexport) void startPlugin()
{
    DebugLog("Map-markers: startPlugin()");

    const KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    if (!IsSupportedVersion(versionInfo))
    {
        ErrorLog("Map-markers: unsupported Kenshi version/platform");
        return;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        &PlayerInterface_updateUT_orig))
    {
        ErrorLog("Map-markers: could not hook PlayerInterface::updateUT");
        return;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&InputHandler::keyDownEvent),
        InputHandler_keyDownEvent_hook,
        &InputHandler_keyDownEvent_orig))
    {
        ErrorLog("Map-markers: could not hook InputHandler::keyDownEvent");
        return;
    }

    std::stringstream info;
    info << kPluginName
         << " INFO: diagnostics hooks installed"
         << " snapshot_hotkey=CTRL+ALT+F7"
         << " live_hover_hotkey=CTRL+ALT+F8"
         << " add_action=MIDDLE_CLICK"
         << " select_move_action=LEFT_CLICK"
         << " delete_action=DELETE"
         << " deselect_action=RIGHT_CLICK"
         << " marker_data=type_label"
         << " persistence=active_save_json"
         << " markers=managed_widgets_with_editor";
    DebugLog(info.str().c_str());
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
