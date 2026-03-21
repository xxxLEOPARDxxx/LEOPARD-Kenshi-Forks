#include <Debug.h>

#include <core/Functions.h>
#include "emc/mod_hub_client.h"
#include <kenshi/Building.h>
#include <kenshi/Character.h>
#include <kenshi/GameData.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/InputHandler.h>
#include <kenshi/Inventory.h>
#include <kenshi/Kenshi.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/RootObject.h>

#include <mygui/MyGUI_Colour.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_ImageBox.h>
#include <mygui/MyGUI_RenderManager.h>
#include <mygui/MyGUI_TextBox.h>

#include <ogre/OgreAxisAlignedBox.h>
#include <ogre/OgreColourValue.h>
#include <ogre/OgreEntity.h>
#include <ogre/OgreGpuProgramParams.h>
#include <ogre/OgreMaterial.h>
#include <ogre/OgreMovableObject.h>
#include <ogre/OgrePass.h>
#include <ogre/OgreSceneNode.h>
#include <ogre/OgreSubEntity.h>
#include <ogre/OgreTechnique.h>

#include <ois/OISKeyboard.h>

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cmath>
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
const char* kPluginName = "Container-Highlight";
const char* kConfigFileName = "mod-config.json";
const size_t kMaxContainerMarkers = 48;
const int kMarkerWidthPx = 88;
const int kMarkerHeightPx = 18;
const int kMarkerYOffsetPx = 24;
const int kMarkerIconGapPx = 2;
const int kMarkerIconSizePx = 18;
const float kDefaultAnchorYOffsetWorld = 0.90f;
const float kContainerAnchorPaddingWorld = 0.30f;
const DWORD kDefaultUpdateIntervalMs = 150;
const float kDefaultMaxHighlightDistance = 900.0f;
const int kDefaultMaxObjectsPerType = 256;
const char* kDefaultMarkerText = "BOX";
const char* kDefaultMarkerIconTexture = "gui/gfx/figure.png";
const int kKeyCodeUnbound = -1;
const int kDefaultHighlightKeyCode = static_cast<int>(OIS::KC_LMENU);
const float kHubMinHighlightDistance = 100.0f;
const float kHubMaxHighlightDistance = 5000.0f;
const char* kHubNamespaceId = "emkej.qol";
const char* kHubNamespaceDisplayName = "Emkej QoL";
const char* kHubModId = "container_highlight";
const char* kHubModDisplayName = "Container Highlight";
const char* kHubSettingEnabledId = "enabled";
const char* kHubSettingShowIconsId = "show_icons";
const char* kHubSettingShowTextId = "show_text";
const char* kHubSettingEnableTintId = "enable_tint";
const char* kHubSettingDebugLoggingId = "debug_logging";
const char* kHubSettingMaxHighlightDistanceId = "max_highlight_distance_m";
const char* kHubActionLogProbeSnapshotId = "log_probe_snapshot";

typedef void PlayerInterfaceUpdateUTFn(PlayerInterface* thisptr);

struct PluginConfig
{
    bool enabled;
    DWORD updateIntervalMs;
    int highlightKeyCode;
    bool highlightKeyRequireCtrl;
    bool highlightKeyRequireShift;
    bool highlightKeyRequireAlt;
    float maxHighlightDistance;
    int maxObjectsPerType;
    bool showMarkerIcons;
    bool showMarkerText;
    bool enableTint;
    bool tintForceDepthOverride;
    bool debugLogging;
    std::string markerText;
    DWORD markerTextSizePx;
    std::string markerIconTexture;
    MyGUI::Colour markerColour;
    Ogre::ColourValue tintColour;
};

struct MarkerWidget
{
    MyGUI::ImageBox* icon;
    MyGUI::TextBox* text;
};

struct ContainerTarget
{
    hand targetHandle;
    Ogre::Vector3 worldPos;
    float anchorYOffsetWorld;
    float distanceSq;
    itemType dataType;
};

struct TintEntityBinding
{
    Ogre::Entity* entity;
    std::vector<Ogre::MaterialPtr> originalMaterials;
    std::vector<Ogre::MaterialPtr> cloneMaterials;
};

struct ContainerTintEntry
{
    hand targetHandle;
    std::vector<TintEntityBinding> bindings;
};

struct RuntimeState
{
    RuntimeState()
        : projectionUtility(0)
        , lastProbeTickMs(0)
        , markerWidgetSerial(0)
        , tintCloneSerial(0)
        , highlightRuntimeActive(false)
    {
    }

    std::string settingsPath;
    PluginConfig config;
    std::vector<ContainerTarget> targetCache;
    std::vector<MarkerWidget> markerWidgets;
    std::vector<ContainerTintEntry> tintEntries;
    UtilityT* projectionUtility;
    DWORD lastProbeTickMs;
    unsigned int markerWidgetSerial;
    unsigned int tintCloneSerial;
    bool highlightRuntimeActive;
};

struct KeyNameEntry
{
    const char* name;
    OIS::KeyCode keyCode;
};

const KeyNameEntry kKeyNameMap[] = {
    { "A", OIS::KC_A }, { "B", OIS::KC_B }, { "C", OIS::KC_C }, { "D", OIS::KC_D }, { "E", OIS::KC_E },
    { "F", OIS::KC_F }, { "G", OIS::KC_G }, { "H", OIS::KC_H }, { "I", OIS::KC_I }, { "J", OIS::KC_J },
    { "K", OIS::KC_K }, { "L", OIS::KC_L }, { "M", OIS::KC_M }, { "N", OIS::KC_N }, { "O", OIS::KC_O },
    { "P", OIS::KC_P }, { "Q", OIS::KC_Q }, { "R", OIS::KC_R }, { "S", OIS::KC_S }, { "T", OIS::KC_T },
    { "U", OIS::KC_U }, { "V", OIS::KC_V }, { "W", OIS::KC_W }, { "X", OIS::KC_X }, { "Y", OIS::KC_Y },
    { "Z", OIS::KC_Z },
    { "0", OIS::KC_0 }, { "1", OIS::KC_1 }, { "2", OIS::KC_2 }, { "3", OIS::KC_3 }, { "4", OIS::KC_4 },
    { "5", OIS::KC_5 }, { "6", OIS::KC_6 }, { "7", OIS::KC_7 }, { "8", OIS::KC_8 }, { "9", OIS::KC_9 },
    { "SPACE", OIS::KC_SPACE }, { "TAB", OIS::KC_TAB }, { "RETURN", OIS::KC_RETURN }, { "ENTER", OIS::KC_RETURN },
    { "BACK", OIS::KC_BACK }, { "INSERT", OIS::KC_INSERT }, { "DELETE", OIS::KC_DELETE },
    { "HOME", OIS::KC_HOME }, { "END", OIS::KC_END }, { "PGUP", OIS::KC_PGUP }, { "PGDOWN", OIS::KC_PGDOWN },
    { "PAGEUP", OIS::KC_PGUP }, { "PAGEDOWN", OIS::KC_PGDOWN },
    { "F1", OIS::KC_F1 }, { "F2", OIS::KC_F2 }, { "F3", OIS::KC_F3 }, { "F4", OIS::KC_F4 },
    { "F5", OIS::KC_F5 }, { "F6", OIS::KC_F6 }, { "F7", OIS::KC_F7 }, { "F8", OIS::KC_F8 },
    { "F9", OIS::KC_F9 }, { "F10", OIS::KC_F10 }, { "F11", OIS::KC_F11 }, { "F12", OIS::KC_F12 },
    { "MINUS", OIS::KC_MINUS }, { "EQUALS", OIS::KC_EQUALS }, { "LBRACKET", OIS::KC_LBRACKET },
    { "RBRACKET", OIS::KC_RBRACKET }, { "BACKSLASH", OIS::KC_BACKSLASH }, { "SEMICOLON", OIS::KC_SEMICOLON },
    { "APOSTROPHE", OIS::KC_APOSTROPHE }, { "GRAVE", OIS::KC_GRAVE }, { "COMMA", OIS::KC_COMMA },
    { "PERIOD", OIS::KC_PERIOD }, { "SLASH", OIS::KC_SLASH },
    { "LSHIFT", OIS::KC_LSHIFT }, { "RSHIFT", OIS::KC_RSHIFT }, { "SHIFT", OIS::KC_LSHIFT },
    { "LCONTROL", OIS::KC_LCONTROL }, { "RCONTROL", OIS::KC_RCONTROL }, { "CTRL", OIS::KC_LCONTROL },
    { "LALT", OIS::KC_LMENU }, { "RALT", OIS::KC_RMENU }, { "ALT", OIS::KC_LMENU },
    { "ESCAPE", OIS::KC_ESCAPE }, { "ESC", OIS::KC_ESCAPE }, { "LWIN", OIS::KC_LWIN }, { "RWIN", OIS::KC_RWIN },
    { "SYSRQ", OIS::KC_SYSRQ }, { "APPS", OIS::KC_APPS }
};

const size_t kKeyNameMapCount = sizeof(kKeyNameMap) / sizeof(kKeyNameMap[0]);

const char* kColorOverrideParam = "coloroverride";
const char* kColourOverrideParam = "colouroverride";
const char* kColorOverrideParamCamel = "colorOverride";
const char* kColourOverrideParamCamel = "colourOverride";
const char* kDepthOverrideParam = "overrideDepth";
const char* kOverrideDepthLowerParam = "overridedepth";

RuntimeState g_state;
PlayerInterfaceUpdateUTFn* g_playerInterfaceUpdateUTOrig = 0;
void (*g_gameWorldMainLoopGPUSensitiveStuffOrig)(GameWorld* thisptr, float time) = 0;
emc::ModHubClient g_modHubClient;
bool g_probeSnapshotRequested = false;
void AppendTintEntityUnique(std::vector<Ogre::Entity*>* outEntities, Ogre::Entity* entity);
bool SaveConfigState();
void RefreshContainerTargetCache();
std::string RootObjectDisplayNameForLog(RootObject* object);
Inventory* TryGetBuildingInventorySafe(Building* building);
bool LooksLikeContainerDisplayName(const std::string& value);
Inventory* ResolveContainerInventory(RootObject* object, bool* usedBuildingFallback);

PluginConfig MakeDefaultConfig()
{
    PluginConfig config;
    config.enabled = true;
    config.updateIntervalMs = kDefaultUpdateIntervalMs;
    config.highlightKeyCode = kDefaultHighlightKeyCode;
    config.highlightKeyRequireCtrl = false;
    config.highlightKeyRequireShift = false;
    config.highlightKeyRequireAlt = false;
    config.maxHighlightDistance = kDefaultMaxHighlightDistance;
    config.maxObjectsPerType = kDefaultMaxObjectsPerType;
    config.showMarkerIcons = true;
    config.showMarkerText = true;
    config.enableTint = true;
    config.tintForceDepthOverride = true;
    config.debugLogging = false;
    config.markerText = kDefaultMarkerText;
    config.markerTextSizePx = 18;
    config.markerIconTexture = kDefaultMarkerIconTexture;
    config.markerColour = MyGUI::Colour(0.9647f, 0.7686f, 0.3255f, 1.0f);
    config.tintColour = Ogre::ColourValue(0.9647f, 0.7686f, 0.3255f, 0.85f);
    return config;
}

bool IsSupportedVersion(KenshiLib::BinaryVersion versionInfo)
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

bool ShouldLogDebug()
{
    return g_state.config.debugLogging;
}

void LogDebugLine(const std::string& message)
{
    if (!ShouldLogDebug())
    {
        return;
    }

    LogInfoLine(message);
}

const char* ItemTypeNameForLog(itemType type)
{
    switch (type)
    {
    case BUILDING:
        return "BUILDING";
    case CONTAINER:
        return "CONTAINER";
    case ITEM:
        return "ITEM";
    default:
        return "OTHER";
    }
}

bool IsStorageBuildingFunction(BuildingFunction functionType)
{
    return functionType == BF_GENERAL_STORAGE
        || functionType == BF_RESOURCE_STORAGE;
}

const char* BuildingClassTypeNameForLog(BuildingClassType classType)
{
    switch (classType)
    {
    case BCTYPE_STORAGE:
        return "STORAGE";
    case BCTYPE_PRODUCTION:
        return "PRODUCTION";
    case BCTYPE_CRAFTING:
        return "CRAFTING";
    case BCTYPE_FLUFF:
        return "FLUFF";
    case BCTYPE_SHELL_WITH_INTERIOR:
        return "SHELL_WITH_INTERIOR";
    default:
        return "OTHER";
    }
}

const char* BuildingFunctionNameForLog(BuildingFunction functionType)
{
    switch (functionType)
    {
    case BF_GENERAL_STORAGE:
        return "GENERAL_STORAGE";
    case BF_RESOURCE_STORAGE:
        return "RESOURCE_STORAGE";
    case BF_SHOP:
        return "SHOP";
    case BF_CRAFTING:
        return "CRAFTING";
    case BF_FLUFF:
        return "FLUFF";
    default:
        return "OTHER";
    }
}

bool HasStorageFlagsSafe(Building* building)
{
    if (building == 0 || !building->isValid())
    {
        return false;
    }

    __try
    {
        if (building->getBuildingClass() == BCTYPE_STORAGE
            || IsStorageBuildingFunction(building->getSpecialFunction()))
        {
            return true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool IsHighlightableContainerBuilding(Building* building)
{
    if (HasStorageFlagsSafe(building))
    {
        return true;
    }

    if (TryGetBuildingInventorySafe(building) == 0)
    {
        return false;
    }

    return LooksLikeContainerDisplayName(RootObjectDisplayNameForLog(static_cast<RootObject*>(building)));
}

Inventory* TryGetBuildingInventorySafe(Building* building)
{
    if (building == 0 || !building->isValid())
    {
        return 0;
    }

    __try
    {
        return building->getInventory();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

std::string ToLowerAsciiCopy(const std::string& value)
{
    std::string out = value;
    for (std::string::size_type index = 0; index < out.size(); ++index)
    {
        out[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[index])));
    }
    return out;
}

bool LooksLikeContainerDisplayName(const std::string& value)
{
    if (value.empty())
    {
        return false;
    }

    const std::string lower = ToLowerAsciiCopy(value);
    return lower.find("storage") != std::string::npos
        || lower.find("barrel") != std::string::npos
        || lower.find("crate") != std::string::npos
        || lower.find("chest") != std::string::npos
        || lower.find("container") != std::string::npos
        || lower.find("bin") != std::string::npos;
}

Inventory* ResolveContainerInventory(RootObject* object, bool* usedBuildingFallback)
{
    if (usedBuildingFallback != 0)
    {
        *usedBuildingFallback = false;
    }

    if (object == 0 || !object->isValid())
    {
        return 0;
    }

    __try
    {
        Inventory* inventory = object->getInventory();
        if (inventory != 0)
        {
            return inventory;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }

    Building* building = 0;
    __try
    {
        building = object->getHandle().getBuilding();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        building = 0;
    }

    Inventory* buildingInventory = TryGetBuildingInventorySafe(building);
    if (buildingInventory != 0 && usedBuildingFallback != 0)
    {
        *usedBuildingFallback = true;
    }

    return buildingInventory;
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

std::string ToUpperAscii(const std::string& value)
{
    std::string upper = value;
    for (size_t i = 0; i < upper.size(); ++i)
    {
        upper[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(upper[i])));
    }
    return upper;
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

bool TryFindJsonValueStart(const std::string& content, const char* key, size_t* valuePosOut)
{
    if (!key || !valuePosOut)
    {
        return false;
    }

    const std::string needle = std::string("\"") + key + "\"";
    const size_t keyPos = content.find(needle);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    size_t valuePos = content.find(':', keyPos + needle.size());
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

    if (valuePos >= content.size())
    {
        return false;
    }

    *valuePosOut = valuePos;
    return true;
}

bool TryParseJsonBoolByKey(const std::string& content, const char* key, bool* outValue)
{
    if (outValue == 0)
    {
        return false;
    }

    size_t valuePos = 0;
    if (!TryFindJsonValueStart(content, key, &valuePos))
    {
        return false;
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

bool TryParseJsonUnsignedByKey(const std::string& content, const char* key, DWORD* outValue)
{
    if (outValue == 0)
    {
        return false;
    }

    size_t valuePos = 0;
    if (!TryFindJsonValueStart(content, key, &valuePos))
    {
        return false;
    }

    size_t valueEnd = valuePos;
    while (valueEnd < content.size()
        && std::isdigit(static_cast<unsigned char>(content[valueEnd])) != 0)
    {
        ++valueEnd;
    }

    if (valueEnd == valuePos)
    {
        return false;
    }

    std::stringstream parser(content.substr(valuePos, valueEnd - valuePos));
    unsigned int parsed = 0;
    parser >> parsed;
    if (parser.fail())
    {
        return false;
    }

    *outValue = static_cast<DWORD>(parsed);
    return true;
}

bool TryParseJsonIntByKey(const std::string& content, const char* key, int* outValue)
{
    if (outValue == 0)
    {
        return false;
    }

    size_t valuePos = 0;
    if (!TryFindJsonValueStart(content, key, &valuePos))
    {
        return false;
    }

    size_t valueEnd = valuePos;
    if (content[valueEnd] == '-' || content[valueEnd] == '+')
    {
        ++valueEnd;
    }
    while (valueEnd < content.size()
        && std::isdigit(static_cast<unsigned char>(content[valueEnd])) != 0)
    {
        ++valueEnd;
    }

    if (valueEnd == valuePos)
    {
        return false;
    }

    std::stringstream parser(content.substr(valuePos, valueEnd - valuePos));
    int parsed = 0;
    parser >> parsed;
    if (parser.fail())
    {
        return false;
    }

    *outValue = parsed;
    return true;
}

bool TryParseJsonFloatByKey(const std::string& content, const char* key, float* outValue)
{
    if (outValue == 0)
    {
        return false;
    }

    size_t valuePos = 0;
    if (!TryFindJsonValueStart(content, key, &valuePos))
    {
        return false;
    }

    size_t valueEnd = valuePos;
    if (content[valueEnd] == '-' || content[valueEnd] == '+')
    {
        ++valueEnd;
    }
    while (valueEnd < content.size())
    {
        const char current = content[valueEnd];
        if (std::isdigit(static_cast<unsigned char>(current)) != 0
            || current == '.'
            || current == 'e'
            || current == 'E'
            || current == '-'
            || current == '+')
        {
            ++valueEnd;
            continue;
        }
        break;
    }

    if (valueEnd == valuePos)
    {
        return false;
    }

    std::stringstream parser(content.substr(valuePos, valueEnd - valuePos));
    float parsed = 0.0f;
    parser >> parsed;
    if (parser.fail())
    {
        return false;
    }

    *outValue = parsed;
    return true;
}

bool TryParseJsonStringByKey(const std::string& content, const char* key, std::string* outValue)
{
    if (outValue == 0)
    {
        return false;
    }

    size_t valuePos = 0;
    if (!TryFindJsonValueStart(content, key, &valuePos) || content[valuePos] != '"')
    {
        return false;
    }

    ++valuePos;
    std::string parsed;
    while (valuePos < content.size())
    {
        const char current = content[valuePos];
        if (current == '\\')
        {
            ++valuePos;
            if (valuePos >= content.size())
            {
                return false;
            }
            parsed.push_back(content[valuePos]);
            ++valuePos;
            continue;
        }

        if (current == '"')
        {
            *outValue = parsed;
            return true;
        }

        parsed.push_back(current);
        ++valuePos;
    }

    return false;
}

int ParseHexNibble(char value)
{
    if (value >= '0' && value <= '9')
    {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f')
    {
        return 10 + (value - 'a');
    }
    if (value >= 'A' && value <= 'F')
    {
        return 10 + (value - 'A');
    }
    return -1;
}

bool TryParseHexColour(const std::string& rawValue, float* redOut, float* greenOut, float* blueOut, float* alphaOut)
{
    if (!redOut || !greenOut || !blueOut || !alphaOut)
    {
        return false;
    }

    std::string value = TrimAscii(rawValue);
    if (!value.empty() && value[0] == '#')
    {
        value.erase(0, 1);
    }

    if (value.size() != 6 && value.size() != 8)
    {
        return false;
    }

    const int r1 = ParseHexNibble(value[0]);
    const int r2 = ParseHexNibble(value[1]);
    const int g1 = ParseHexNibble(value[2]);
    const int g2 = ParseHexNibble(value[3]);
    const int b1 = ParseHexNibble(value[4]);
    const int b2 = ParseHexNibble(value[5]);
    if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0)
    {
        return false;
    }

    int a1 = 15;
    int a2 = 15;
    if (value.size() == 8)
    {
        a1 = ParseHexNibble(value[6]);
        a2 = ParseHexNibble(value[7]);
        if (a1 < 0 || a2 < 0)
        {
            return false;
        }
    }

    const float inv255 = 1.0f / 255.0f;
    *redOut = static_cast<float>((r1 * 16) + r2) * inv255;
    *greenOut = static_cast<float>((g1 * 16) + g2) * inv255;
    *blueOut = static_cast<float>((b1 * 16) + b2) * inv255;
    *alphaOut = static_cast<float>((a1 * 16) + a2) * inv255;
    return true;
}

bool TryParseConfigColour(
    const std::string& configText,
    const char* key,
    MyGUI::Colour* markerColourOut,
    Ogre::ColourValue* tintColourOut)
{
    std::string rawValue;
    if (!TryParseJsonStringByKey(configText, key, &rawValue))
    {
        return false;
    }

    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
    float alpha = 0.0f;
    if (!TryParseHexColour(rawValue, &red, &green, &blue, &alpha))
    {
        return false;
    }

    if (markerColourOut)
    {
        *markerColourOut = MyGUI::Colour(red, green, blue, alpha);
    }
    if (tintColourOut)
    {
        *tintColourOut = Ogre::ColourValue(red, green, blue, alpha);
    }
    return true;
}

bool IsCtrlKeyCode(int keyCode)
{
    return keyCode == static_cast<int>(OIS::KC_LCONTROL)
        || keyCode == static_cast<int>(OIS::KC_RCONTROL);
}

bool IsShiftKeyCode(int keyCode)
{
    return keyCode == static_cast<int>(OIS::KC_LSHIFT)
        || keyCode == static_cast<int>(OIS::KC_RSHIFT);
}

bool IsAltKeyCode(int keyCode)
{
    return keyCode == static_cast<int>(OIS::KC_LMENU)
        || keyCode == static_cast<int>(OIS::KC_RMENU);
}

bool IsModifierDown(OIS::Keyboard* keyboard, int keyCode)
{
    if (!keyboard)
    {
        return false;
    }

    if (IsCtrlKeyCode(keyCode))
    {
        return keyboard->isKeyDown(OIS::KC_LCONTROL) || keyboard->isKeyDown(OIS::KC_RCONTROL);
    }
    if (IsShiftKeyCode(keyCode))
    {
        return keyboard->isKeyDown(OIS::KC_LSHIFT) || keyboard->isKeyDown(OIS::KC_RSHIFT);
    }
    if (IsAltKeyCode(keyCode))
    {
        return keyboard->isKeyDown(OIS::KC_LMENU) || keyboard->isKeyDown(OIS::KC_RMENU);
    }

    return false;
}

bool IsKeyDown(OIS::Keyboard* keyboard, int keyCode)
{
    if (!keyboard)
    {
        return false;
    }
    if (keyCode == kKeyCodeUnbound)
    {
        return true;
    }
    if (IsCtrlKeyCode(keyCode) || IsShiftKeyCode(keyCode) || IsAltKeyCode(keyCode))
    {
        return IsModifierDown(keyboard, keyCode);
    }
    return keyboard->isKeyDown(static_cast<OIS::KeyCode>(keyCode));
}

bool TryParseKeyCode(const std::string& rawValue, int* keyCodeOut)
{
    if (keyCodeOut == 0)
    {
        return false;
    }

    const std::string normalized = ToUpperAscii(TrimAscii(rawValue));
    if (normalized.empty())
    {
        return false;
    }

    if (normalized == "UNBOUND" || normalized == "NONE")
    {
        *keyCodeOut = kKeyCodeUnbound;
        return true;
    }

    for (size_t i = 0; i < kKeyNameMapCount; ++i)
    {
        if (normalized == kKeyNameMap[i].name)
        {
            *keyCodeOut = static_cast<int>(kKeyNameMap[i].keyCode);
            return true;
        }
    }

    return false;
}

bool ValidatePrimaryKeyCode(int keyCode, std::string* reasonOut)
{
    if (keyCode == kKeyCodeUnbound)
    {
        if (reasonOut)
        {
            reasonOut->clear();
        }
        return true;
    }

    bool supported = false;
    for (size_t i = 0; i < kKeyNameMapCount; ++i)
    {
        if (keyCode == static_cast<int>(kKeyNameMap[i].keyCode))
        {
            supported = true;
            break;
        }
    }
    if (!supported)
    {
        if (reasonOut)
        {
            *reasonOut = "unsupported key";
        }
        return false;
    }

    if (keyCode == static_cast<int>(OIS::KC_ESCAPE)
        || keyCode == static_cast<int>(OIS::KC_LWIN)
        || keyCode == static_cast<int>(OIS::KC_RWIN)
        || keyCode == static_cast<int>(OIS::KC_SYSRQ)
        || keyCode == static_cast<int>(OIS::KC_APPS))
    {
        if (reasonOut)
        {
            *reasonOut = "reserved key";
        }
        return false;
    }

    if (reasonOut)
    {
        reasonOut->clear();
    }
    return true;
}

std::string KeyCodeToConfigString(int keyCode)
{
    if (keyCode == kKeyCodeUnbound)
    {
        return "UNBOUND";
    }
    if (IsCtrlKeyCode(keyCode))
    {
        return "CTRL";
    }
    if (IsShiftKeyCode(keyCode))
    {
        return "SHIFT";
    }
    if (IsAltKeyCode(keyCode))
    {
        return "ALT";
    }

    for (size_t i = 0; i < kKeyNameMapCount; ++i)
    {
        if (keyCode == static_cast<int>(kKeyNameMap[i].keyCode))
        {
            return kKeyNameMap[i].name;
        }
    }

    return "UNKNOWN";
}

std::string FormatKeybind(const PluginConfig& config)
{
    if (config.highlightKeyCode == kKeyCodeUnbound)
    {
        return "UNBOUND";
    }

    std::stringstream ss;
    bool wrotePrefix = false;
    if (config.highlightKeyRequireCtrl && !IsCtrlKeyCode(config.highlightKeyCode))
    {
        ss << "CTRL";
        wrotePrefix = true;
    }
    if (config.highlightKeyRequireShift && !IsShiftKeyCode(config.highlightKeyCode))
    {
        if (wrotePrefix)
        {
            ss << "+";
        }
        ss << "SHIFT";
        wrotePrefix = true;
    }
    if (config.highlightKeyRequireAlt && !IsAltKeyCode(config.highlightKeyCode))
    {
        if (wrotePrefix)
        {
            ss << "+";
        }
        ss << "ALT";
        wrotePrefix = true;
    }
    if (wrotePrefix)
    {
        ss << "+";
    }
    ss << KeyCodeToConfigString(config.highlightKeyCode);
    return ss.str();
}

bool IsHighlightGateOpen(const PluginConfig& config)
{
    if (config.highlightKeyCode == kKeyCodeUnbound)
    {
        return true;
    }
    if (key == 0 || key->keyboard == 0)
    {
        return false;
    }

    OIS::Keyboard* keyboard = key->keyboard;
    if (!IsKeyDown(keyboard, config.highlightKeyCode))
    {
        return false;
    }
    if (config.highlightKeyRequireCtrl && !IsModifierDown(keyboard, static_cast<int>(OIS::KC_LCONTROL)))
    {
        return false;
    }
    if (config.highlightKeyRequireShift && !IsModifierDown(keyboard, static_cast<int>(OIS::KC_LSHIFT)))
    {
        return false;
    }
    if (config.highlightKeyRequireAlt && !IsModifierDown(keyboard, static_cast<int>(OIS::KC_LMENU)))
    {
        return false;
    }
    return true;
}

void LoadConfigState()
{
    g_state.config = MakeDefaultConfig();

    if (g_state.settingsPath.empty())
    {
        LogWarnLine("config load skipped: could not resolve plugin directory");
        return;
    }

    std::string configText;
    if (!TryReadTextFile(g_state.settingsPath, &configText))
    {
        std::stringstream line;
        line << "config load skipped: could not read " << g_state.settingsPath;
        LogWarnLine(line.str());
        return;
    }

    bool boolValue = false;
    if (TryParseJsonBoolByKey(configText, "enabled", &boolValue))
    {
        g_state.config.enabled = boolValue;
    }
    if (TryParseJsonBoolByKey(configText, "highlight_key_require_ctrl", &boolValue))
    {
        g_state.config.highlightKeyRequireCtrl = boolValue;
    }
    if (TryParseJsonBoolByKey(configText, "highlight_key_require_shift", &boolValue))
    {
        g_state.config.highlightKeyRequireShift = boolValue;
    }
    if (TryParseJsonBoolByKey(configText, "highlight_key_require_alt", &boolValue))
    {
        g_state.config.highlightKeyRequireAlt = boolValue;
    }
    if (TryParseJsonBoolByKey(configText, "show_icons", &boolValue))
    {
        g_state.config.showMarkerIcons = boolValue;
    }
    if (TryParseJsonBoolByKey(configText, "show_text", &boolValue))
    {
        g_state.config.showMarkerText = boolValue;
    }
    if (TryParseJsonBoolByKey(configText, "enable_tint", &boolValue))
    {
        g_state.config.enableTint = boolValue;
    }
    if (TryParseJsonBoolByKey(configText, "tint_force_depth_override", &boolValue))
    {
        g_state.config.tintForceDepthOverride = boolValue;
    }
    if (TryParseJsonBoolByKey(configText, "debug_logging", &boolValue))
    {
        g_state.config.debugLogging = boolValue;
    }

    DWORD unsignedValue = 0;
    if (TryParseJsonUnsignedByKey(configText, "update_interval_ms", &unsignedValue) && unsignedValue > 0)
    {
        g_state.config.updateIntervalMs = unsignedValue;
    }
    if (TryParseJsonUnsignedByKey(configText, "marker_text_size_px", &unsignedValue) && unsignedValue >= 8)
    {
        g_state.config.markerTextSizePx = unsignedValue;
    }

    int intValue = 0;
    if (TryParseJsonIntByKey(configText, "max_objects_per_type", &intValue) && intValue > 0)
    {
        g_state.config.maxObjectsPerType = intValue;
    }

    float floatValue = 0.0f;
    if (TryParseJsonFloatByKey(configText, "max_highlight_distance_m", &floatValue) && floatValue > 0.0f)
    {
        g_state.config.maxHighlightDistance = floatValue;
    }

    std::string stringValue;
    if (TryParseJsonStringByKey(configText, "highlight_key", &stringValue))
    {
        int parsedKeyCode = 0;
        if (TryParseKeyCode(stringValue, &parsedKeyCode))
        {
            std::string reason;
            if (ValidatePrimaryKeyCode(parsedKeyCode, &reason))
            {
                g_state.config.highlightKeyCode = parsedKeyCode;
            }
            else
            {
                std::stringstream warn;
                warn << "ignoring highlight_key value '" << stringValue << "': " << reason;
                LogWarnLine(warn.str());
            }
        }
        else
        {
            std::stringstream warn;
            warn << "ignoring unrecognized highlight_key value '" << stringValue << "'";
            LogWarnLine(warn.str());
        }
    }
    if (TryParseJsonStringByKey(configText, "marker_text", &stringValue) && !TrimAscii(stringValue).empty())
    {
        g_state.config.markerText = TrimAscii(stringValue);
    }
    if (TryParseJsonStringByKey(configText, "marker_icon_texture", &stringValue))
    {
        g_state.config.markerIconTexture = TrimAscii(stringValue);
    }

    MyGUI::Colour markerColour;
    Ogre::ColourValue tintColour;
    if (TryParseConfigColour(configText, "marker_color_hex", &markerColour, 0))
    {
        g_state.config.markerColour = markerColour;
    }
    if (TryParseConfigColour(configText, "tint_color_hex", 0, &tintColour))
    {
        g_state.config.tintColour = tintColour;
    }

    std::stringstream info;
    info << "config loaded"
         << " enabled=" << (g_state.config.enabled ? "true" : "false")
         << " update_interval_ms=" << g_state.config.updateIntervalMs
         << " highlight_key=" << FormatKeybind(g_state.config)
         << " max_highlight_distance_m=" << g_state.config.maxHighlightDistance
         << " show_icons=" << (g_state.config.showMarkerIcons ? "true" : "false")
         << " show_text=" << (g_state.config.showMarkerText ? "true" : "false")
         << " enable_tint=" << (g_state.config.enableTint ? "true" : "false");
    LogInfoLine(info.str());
}

std::string RootObjectDisplayNameForLog(RootObject* object)
{
    if (object == 0)
    {
        return "<null>";
    }

    if (!object->displayName.empty())
    {
        return object->displayName;
    }

    const std::string objectName = object->getName();
    if (!objectName.empty())
    {
        return objectName;
    }

    if (object->data != 0 && !object->data->name.empty())
    {
        return object->data->name;
    }

    return "<unnamed>";
}

int InventoryItemCountForLog(Inventory* inventory)
{
    if (inventory == 0)
    {
        return 0;
    }

    __try
    {
        return inventory->getNumItems();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

bool TryGetInventoryVisibleForLog(Inventory* inventory, bool* visibleOut)
{
    if (inventory == 0 || visibleOut == 0)
    {
        return false;
    }

    __try
    {
        *visibleOut = inventory->isVisible();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

char HexNibbleToChar(int value)
{
    const int clamped = (value < 0) ? 0 : ((value > 15) ? 15 : value);
    return static_cast<char>(clamped < 10 ? ('0' + clamped) : ('A' + (clamped - 10)));
}

void AppendHexByte(std::string* out, int value)
{
    if (out == 0)
    {
        return;
    }

    int clamped = value;
    if (clamped < 0)
    {
        clamped = 0;
    }
    else if (clamped > 255)
    {
        clamped = 255;
    }

    out->push_back(HexNibbleToChar((clamped >> 4) & 0xF));
    out->push_back(HexNibbleToChar(clamped & 0xF));
}

int ClampColourChannelByte(float value)
{
    int scaled = static_cast<int>(value * 255.0f + 0.5f);
    if (scaled < 0)
    {
        return 0;
    }
    if (scaled > 255)
    {
        return 255;
    }
    return scaled;
}

std::string BuildHexColourString(float red, float green, float blue, float alpha, bool includeAlpha)
{
    std::string value("#");
    AppendHexByte(&value, ClampColourChannelByte(red));
    AppendHexByte(&value, ClampColourChannelByte(green));
    AppendHexByte(&value, ClampColourChannelByte(blue));
    if (includeAlpha)
    {
        AppendHexByte(&value, ClampColourChannelByte(alpha));
    }
    return value;
}

std::string EscapeJsonString(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size() + 8);

    for (size_t i = 0; i < value.size(); ++i)
    {
        const char current = value[i];
        switch (current)
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
            escaped.push_back(current);
            break;
        }
    }

    return escaped;
}

bool SaveConfigState()
{
    if (g_state.settingsPath.empty())
    {
        LogErrorLine("settings path is empty; cannot save mod-config.json");
        return false;
    }

    std::ofstream out(g_state.settingsPath.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out)
    {
        std::stringstream line;
        line << "failed to open " << g_state.settingsPath << " for writing";
        LogErrorLine(line.str());
        return false;
    }

    out << "{\n"
        << "  \"enabled\": " << (g_state.config.enabled ? "true" : "false") << ",\n"
        << "  \"update_interval_ms\": " << g_state.config.updateIntervalMs << ",\n"
        << "  \"highlight_key\": \"" << EscapeJsonString(KeyCodeToConfigString(g_state.config.highlightKeyCode)) << "\",\n"
        << "  \"highlight_key_require_ctrl\": " << (g_state.config.highlightKeyRequireCtrl ? "true" : "false") << ",\n"
        << "  \"highlight_key_require_shift\": " << (g_state.config.highlightKeyRequireShift ? "true" : "false") << ",\n"
        << "  \"highlight_key_require_alt\": " << (g_state.config.highlightKeyRequireAlt ? "true" : "false") << ",\n"
        << "  \"max_highlight_distance_m\": " << g_state.config.maxHighlightDistance << ",\n"
        << "  \"max_objects_per_type\": " << g_state.config.maxObjectsPerType << ",\n"
        << "  \"show_icons\": " << (g_state.config.showMarkerIcons ? "true" : "false") << ",\n"
        << "  \"show_text\": " << (g_state.config.showMarkerText ? "true" : "false") << ",\n"
        << "  \"enable_tint\": " << (g_state.config.enableTint ? "true" : "false") << ",\n"
        << "  \"tint_force_depth_override\": " << (g_state.config.tintForceDepthOverride ? "true" : "false") << ",\n"
        << "  \"marker_text\": \"" << EscapeJsonString(g_state.config.markerText) << "\",\n"
        << "  \"marker_text_size_px\": " << g_state.config.markerTextSizePx << ",\n"
        << "  \"marker_icon_texture\": \"" << EscapeJsonString(g_state.config.markerIconTexture) << "\",\n"
        << "  \"marker_color_hex\": \""
        << BuildHexColourString(
            g_state.config.markerColour.red,
            g_state.config.markerColour.green,
            g_state.config.markerColour.blue,
            g_state.config.markerColour.alpha,
            false)
        << "\",\n"
        << "  \"tint_color_hex\": \""
        << BuildHexColourString(
            g_state.config.tintColour.r,
            g_state.config.tintColour.g,
            g_state.config.tintColour.b,
            g_state.config.tintColour.a,
            true)
        << "\",\n"
        << "  \"debug_logging\": " << (g_state.config.debugLogging ? "true" : "false") << "\n"
        << "}\n";

    if (!out.good())
    {
        std::stringstream line;
        line << "failed to save " << g_state.settingsPath;
        LogErrorLine(line.str());
        return false;
    }

    return true;
}

void WriteRuntimeApiError(char* err_buf, uint32_t err_buf_size, const char* text)
{
    if (err_buf == 0 || err_buf_size == 0u)
    {
        return;
    }

    if (text == 0)
    {
        err_buf[0] = '\0';
        return;
    }

    const size_t copyLen = static_cast<size_t>(err_buf_size - 1u);
    std::strncpy(err_buf, text, copyLen);
    err_buf[copyLen] = '\0';
}

bool IsHubUserDataValid(void* user_data)
{
    return user_data == &g_modHubClient;
}

EMC_Result HubGetBoolSetting(void* user_data, bool value, int32_t* out_value)
{
    if (!IsHubUserDataValid(user_data) || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = value ? 1 : 0;
    return EMC_OK;
}

EMC_Result HubSetBoolSetting(void* user_data, int32_t value, bool* target, char* err_buf, uint32_t err_buf_size)
{
    if (!IsHubUserDataValid(user_data) || target == 0)
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value != 0 && value != 1)
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "invalid_bool");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const bool previousValue = *target;
    *target = (value != 0);
    if (!SaveConfigState())
    {
        *target = previousValue;
        WriteRuntimeApiError(err_buf, err_buf_size, "persist_failed");
        return EMC_ERR_INTERNAL;
    }

    return EMC_OK;
}

EMC_Result HubGetFloatSetting(void* user_data, float value, float* out_value)
{
    if (!IsHubUserDataValid(user_data) || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_value = value;
    return EMC_OK;
}

EMC_Result HubSetFloatSetting(
    void* user_data,
    float value,
    float minValue,
    float maxValue,
    float* target,
    char* err_buf,
    uint32_t err_buf_size)
{
    if (!IsHubUserDataValid(user_data) || target == 0)
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (!(value >= minValue && value <= maxValue))
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "invalid_range");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    const float previousValue = *target;
    *target = value;
    if (!SaveConfigState())
    {
        *target = previousValue;
        WriteRuntimeApiError(err_buf, err_buf_size, "persist_failed");
        return EMC_ERR_INTERNAL;
    }

    return EMC_OK;
}

EMC_Result __cdecl HubGetEnabledSetting(void* user_data, int32_t* out_value)
{
    return HubGetBoolSetting(user_data, g_state.config.enabled, out_value);
}

EMC_Result __cdecl HubSetEnabledSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return HubSetBoolSetting(user_data, value, &g_state.config.enabled, err_buf, err_buf_size);
}

EMC_Result __cdecl HubGetShowIconsSetting(void* user_data, int32_t* out_value)
{
    return HubGetBoolSetting(user_data, g_state.config.showMarkerIcons, out_value);
}

EMC_Result __cdecl HubSetShowIconsSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return HubSetBoolSetting(user_data, value, &g_state.config.showMarkerIcons, err_buf, err_buf_size);
}

EMC_Result __cdecl HubGetShowTextSetting(void* user_data, int32_t* out_value)
{
    return HubGetBoolSetting(user_data, g_state.config.showMarkerText, out_value);
}

EMC_Result __cdecl HubSetShowTextSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return HubSetBoolSetting(user_data, value, &g_state.config.showMarkerText, err_buf, err_buf_size);
}

EMC_Result __cdecl HubGetEnableTintSetting(void* user_data, int32_t* out_value)
{
    return HubGetBoolSetting(user_data, g_state.config.enableTint, out_value);
}

EMC_Result __cdecl HubSetEnableTintSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return HubSetBoolSetting(user_data, value, &g_state.config.enableTint, err_buf, err_buf_size);
}

EMC_Result __cdecl HubGetDebugLoggingSetting(void* user_data, int32_t* out_value)
{
    return HubGetBoolSetting(user_data, g_state.config.debugLogging, out_value);
}

EMC_Result __cdecl HubSetDebugLoggingSetting(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return HubSetBoolSetting(user_data, value, &g_state.config.debugLogging, err_buf, err_buf_size);
}

EMC_Result __cdecl HubGetMaxHighlightDistanceSetting(void* user_data, float* out_value)
{
    return HubGetFloatSetting(user_data, g_state.config.maxHighlightDistance, out_value);
}

EMC_Result __cdecl HubSetMaxHighlightDistanceSetting(void* user_data, float value, char* err_buf, uint32_t err_buf_size)
{
    return HubSetFloatSetting(
        user_data,
        value,
        kHubMinHighlightDistance,
        kHubMaxHighlightDistance,
        &g_state.config.maxHighlightDistance,
        err_buf,
        err_buf_size);
}

EMC_Result __cdecl HubLogProbeSnapshotAction(void* user_data, char* err_buf, uint32_t err_buf_size)
{
    if (!IsHubUserDataValid(user_data))
    {
        WriteRuntimeApiError(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    g_probeSnapshotRequested = true;
    LogInfoLine("probe snapshot requested via mod hub");
    if (ou != 0)
    {
        RefreshContainerTargetCache();
    }

    return EMC_OK;
}

const emc::ModHubClientTableRegistrationV1* GetModHubTableRegistration()
{
    static const EMC_ModDescriptorV1 kModDescriptor = {
        kHubNamespaceId,
        kHubNamespaceDisplayName,
        kHubModId,
        kHubModDisplayName,
        &g_modHubClient };

    static const EMC_BoolSettingDefV1 kEnabledSettingDef = {
        kHubSettingEnabledId,
        "Enabled",
        "Enable Container Highlight runtime behavior",
        &g_modHubClient,
        &HubGetEnabledSetting,
        &HubSetEnabledSetting };

    static const EMC_BoolSettingDefV1 kShowIconsSettingDef = {
        kHubSettingShowIconsId,
        "Show icons",
        "Show screen-space icon markers above matched containers",
        &g_modHubClient,
        &HubGetShowIconsSetting,
        &HubSetShowIconsSetting };

    static const EMC_BoolSettingDefV1 kShowTextSettingDef = {
        kHubSettingShowTextId,
        "Show text",
        "Show screen-space text markers above matched containers",
        &g_modHubClient,
        &HubGetShowTextSetting,
        &HubSetShowTextSetting };

    static const EMC_BoolSettingDefV1 kEnableTintSettingDef = {
        kHubSettingEnableTintId,
        "Enable tint",
        "Apply model tint to matched containers when the highlight gate is active",
        &g_modHubClient,
        &HubGetEnableTintSetting,
        &HubSetEnableTintSetting };

    static const EMC_BoolSettingDefV1 kDebugLoggingSettingDef = {
        kHubSettingDebugLoggingId,
        "Debug logging",
        "Write probe summaries and diagnostics to RE_Kenshi_log.txt",
        &g_modHubClient,
        &HubGetDebugLoggingSetting,
        &HubSetDebugLoggingSetting };

    static const EMC_FloatSettingDefV1 kMaxHighlightDistanceSettingDef = {
        kHubSettingMaxHighlightDistanceId,
        "Max highlight distance",
        "Maximum scan radius in meters for nearby container detection",
        &g_modHubClient,
        kHubMinHighlightDistance,
        kHubMaxHighlightDistance,
        50.0f,
        0u,
        &HubGetMaxHighlightDistanceSetting,
        &HubSetMaxHighlightDistanceSetting };

    static const EMC_ActionRowDefV1 kProbeSnapshotActionDef = {
        kHubActionLogProbeSnapshotId,
        "Log nearby probe snapshot",
        "Dump nearby scanned objects and filter decisions into RE_Kenshi_log.txt for barrel/container debugging",
        &g_modHubClient,
        EMC_ACTION_FORCE_REFRESH,
        &HubLogProbeSnapshotAction };

    static const emc::ModHubClientSettingRowV1 kRows[] = {
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kEnabledSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_FLOAT, &kMaxHighlightDistanceSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kShowIconsSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kShowTextSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kEnableTintSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kDebugLoggingSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION, &kProbeSnapshotActionDef }
    };

    static const emc::ModHubClientTableRegistrationV1 kRegistration = {
        &kModDescriptor,
        kRows,
        static_cast<uint32_t>(sizeof(kRows) / sizeof(kRows[0])) };

    return &kRegistration;
}

void ConfigureModHubClient()
{
    emc::ModHubClient::Config config;
    config.table_registration = GetModHubTableRegistration();
    g_modHubClient.SetConfig(config);
}

void StartModHubClient()
{
    const emc::ModHubClient::AttemptResult result = g_modHubClient.OnStartup();
    if (result == emc::ModHubClient::ATTACH_SUCCESS)
    {
        LogInfoLine("event=mod_hub_attached use_hub_ui=1");
        return;
    }

    if (result == emc::ModHubClient::ATTACH_FAILED)
    {
        if (g_modHubClient.LastAttemptFailureResult() == EMC_ERR_NOT_FOUND)
        {
            return;
        }

        LogWarnLine("event=mod_hub_fallback reason=get_api_failed use_hub_ui=0");
        return;
    }

    if (result == emc::ModHubClient::REGISTRATION_FAILED)
    {
        LogWarnLine("event=mod_hub_fallback reason=register_mod_or_setting_failed use_hub_ui=0");
        return;
    }

    LogWarnLine("event=mod_hub_fallback reason=invalid_client_configuration use_hub_ui=0");
}

bool HandlesEqualByKey(const hand& a, const hand& b)
{
    return a.type == b.type
        && a.index == b.index
        && a.serial == b.serial;
}

bool HandVectorContains(const std::vector<hand>& values, const hand& targetHandle)
{
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (HandlesEqualByKey(values[i], targetHandle))
        {
            return true;
        }
    }
    return false;
}

bool TargetCacheContainsHandle(const std::vector<ContainerTarget>& values, const hand& targetHandle)
{
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (HandlesEqualByKey(values[i].targetHandle, targetHandle))
        {
            return true;
        }
    }
    return false;
}

bool EntityVectorContains(const std::vector<Ogre::Entity*>& values, Ogre::Entity* entity)
{
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (values[i] == entity)
        {
            return true;
        }
    }
    return false;
}

void CollectBuildingEntitiesRecursive(Ogre::SceneNode* node, size_t depth, std::vector<Ogre::Entity*>* outEntities)
{
    if (node == 0 || outEntities == 0 || depth > 10)
    {
        return;
    }

    __try
    {
        const size_t attachedCount = node->numAttachedObjects();
        for (size_t attachedIndex = 0; attachedIndex < attachedCount; ++attachedIndex)
        {
            Ogre::MovableObject* movable = node->getAttachedObject(attachedIndex);
            if (movable == 0)
            {
                continue;
            }

            if (movable->getMovableType() == Ogre::EntityFactory::FACTORY_TYPE_NAME)
            {
                AppendTintEntityUnique(outEntities, static_cast<Ogre::Entity*>(movable));
            }
        }

        const size_t childCount = node->numChildren();
        for (size_t childIndex = 0; childIndex < childCount; ++childIndex)
        {
            Ogre::SceneNode* childNode = static_cast<Ogre::SceneNode*>(node->getChild(childIndex));
            CollectBuildingEntitiesRecursive(childNode, depth + 1, outEntities);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

Ogre::SceneNode* TryGetBuildingRootNode(Building* building)
{
    if (building == 0)
    {
        return 0;
    }

    __try
    {
        return building->getRootNode();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

bool TryGetEntityHalfSizeY(Ogre::Entity* entity, float* halfSizeYOut)
{
    if (entity == 0 || halfSizeYOut == 0)
    {
        return false;
    }

    __try
    {
        *halfSizeYOut = entity->getLocalAabb().mHalfSize.y;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool TryGetProbeCenter(Ogre::Vector3* centerOut)
{
    if (!centerOut || ou == 0)
    {
        return false;
    }

    if (ou->player != 0)
    {
        Character* selectedCharacter = ou->player->selectedCharacter.getCharacter();
        if (selectedCharacter != 0 && selectedCharacter->isValid())
        {
            *centerOut = selectedCharacter->getPosition();
            return true;
        }
    }

    *centerOut = ou->getCameraCenter();
    return true;
}

bool TryGetTargetWorldPosition(const hand& targetHandle, Ogre::Vector3* worldPosOut)
{
    if (!worldPosOut || targetHandle.isNull())
    {
        return false;
    }

    RootObjectBase* base = targetHandle.getRootObjectBase();
    if (base == 0 || !base->isValid())
    {
        return false;
    }

    __try
    {
        *worldPosOut = base->getPosition();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return true;
}

float ResolveTargetAnchorYOffsetWorld(const hand& targetHandle)
{
    Building* building = targetHandle.getBuilding();
    if (building != 0 && IsHighlightableContainerBuilding(building))
    {
        Ogre::SceneNode* rootNode = TryGetBuildingRootNode(building);
        std::vector<Ogre::Entity*> entities;
        CollectBuildingEntitiesRecursive(rootNode, 0, &entities);
        float maxHalfSizeY = 0.0f;
        for (size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex)
        {
            Ogre::Entity* entity = entities[entityIndex];
            float halfSizeY = 0.0f;
            if (!TryGetEntityHalfSizeY(entity, &halfSizeY))
            {
                continue;
            }

            if (halfSizeY > maxHalfSizeY)
            {
                maxHalfSizeY = halfSizeY;
            }
        }

        if (maxHalfSizeY > 0.02f)
        {
            return maxHalfSizeY + kContainerAnchorPaddingWorld;
        }
    }

    return kDefaultAnchorYOffsetWorld;
}

bool IsContainerLikeObject(RootObject* object, Inventory* inventory)
{
    if (object == 0 || inventory == 0)
    {
        return false;
    }

    const itemType dataType = object->getDataType();
    if (dataType == CONTAINER)
    {
        return true;
    }

    if (dataType != BUILDING)
    {
        return false;
    }

    Building* building = object->getHandle().getBuilding();
    if (IsHighlightableContainerBuilding(building))
    {
        return true;
    }

    if (TryGetBuildingInventorySafe(building) == 0)
    {
        return false;
    }

    if (LooksLikeContainerDisplayName(RootObjectDisplayNameForLog(object)))
    {
        return true;
    }

    return LooksLikeContainerDisplayName(RootObjectDisplayNameForLog(static_cast<RootObject*>(building)));
}

void LogProbeCandidateDecision(
    itemType scanType,
    RootObject* object,
    Inventory* inventory,
    const char* outcome,
    bool usedBuildingInventoryFallback)
{
    std::stringstream line;
    line << "probe_snapshot"
         << " outcome=" << (outcome == 0 ? "unknown" : outcome)
         << " scan_type=" << ItemTypeNameForLog(scanType)
         << " inv_source=" << (usedBuildingInventoryFallback ? "building" : "object");

    if (object == 0)
    {
        line << " object=<null>";
        LogInfoLine(line.str());
        return;
    }

    const itemType dataType = object->getDataType();
    const hand targetHandle = object->getHandle();
    line << " data_type=" << ItemTypeNameForLog(dataType)
         << " name=\"" << RootObjectDisplayNameForLog(object) << "\""
         << " handle=" << targetHandle.type << ":" << targetHandle.index << ":" << targetHandle.serial;

    if (inventory != 0)
    {
        bool inventoryVisible = false;
        line << " inv_items=" << InventoryItemCountForLog(inventory);
        if (TryGetInventoryVisibleForLog(inventory, &inventoryVisible))
        {
            line << " inv_visible=" << (inventoryVisible ? "true" : "false");
        }
    }
    else
    {
        line << " inv_items=<none>";
    }

    Building* building = targetHandle.getBuilding();
    if (building != 0)
    {
        line << " building_class=" << BuildingClassTypeNameForLog(building->getBuildingClass())
             << " special_function=" << BuildingFunctionNameForLog(building->getSpecialFunction())
             << " storage_match=" << (IsHighlightableContainerBuilding(building) ? "true" : "false");
    }

    LogInfoLine(line.str());
}

bool ContainerTargetDistanceLess(const ContainerTarget& left, const ContainerTarget& right)
{
    return left.distanceSq < right.distanceSq;
}

void RefreshContainerTargetCache()
{
    g_state.targetCache.clear();
    if (ou == 0)
    {
        return;
    }

    Ogre::Vector3 probeCenter;
    if (!TryGetProbeCenter(&probeCenter))
    {
        return;
    }

    const itemType scanTypes[] = { BUILDING, CONTAINER };
    size_t scannedObjects = 0;
    size_t matchedTargets = 0;
    size_t matchedObjectsByType[2] = { 0, 0 };
    const bool emitProbeSnapshot = g_probeSnapshotRequested;
    g_probeSnapshotRequested = false;

    if (emitProbeSnapshot)
    {
        std::stringstream line;
        line << "probe_snapshot begin"
             << " center=(" << probeCenter.x << "," << probeCenter.y << "," << probeCenter.z << ")"
             << " max_distance=" << g_state.config.maxHighlightDistance
             << " gate_open=" << (IsHighlightGateOpen(g_state.config) ? "true" : "false");
        LogInfoLine(line.str());
    }

    for (size_t typeIndex = 0; typeIndex < (sizeof(scanTypes) / sizeof(scanTypes[0])); ++typeIndex)
    {
        lektor<RootObject*> nearbyObjects;
        ou->getObjectsWithinSphere(
            nearbyObjects,
            probeCenter,
            g_state.config.maxHighlightDistance,
            scanTypes[typeIndex],
            g_state.config.maxObjectsPerType,
            0);

        if (!nearbyObjects.valid() || nearbyObjects.size() == 0)
        {
            if (emitProbeSnapshot)
            {
                std::stringstream line;
                line << "probe_snapshot no_objects"
                     << " scan_type=" << ItemTypeNameForLog(scanTypes[typeIndex]);
                LogInfoLine(line.str());
            }
            continue;
        }

        for (lektor<RootObject*>::const_iterator it = nearbyObjects.begin(); it != nearbyObjects.end(); ++it)
        {
            RootObject* object = *it;
            if (object == 0 || !object->isValid())
            {
                continue;
            }

            ++scannedObjects;

            bool usedBuildingInventoryFallback = false;
            Inventory* inventory = ResolveContainerInventory(object, &usedBuildingInventoryFallback);

            if (inventory == 0 || !IsContainerLikeObject(object, inventory))
            {
                if (emitProbeSnapshot)
                {
                    LogProbeCandidateDecision(
                        scanTypes[typeIndex],
                        object,
                        inventory,
                        inventory == 0 ? "reject_no_inventory" : "reject_filtered",
                        usedBuildingInventoryFallback);
                }
                continue;
            }

            const hand targetHandle = object->getHandle();
            if (targetHandle.isNull() || TargetCacheContainsHandle(g_state.targetCache, targetHandle))
            {
                if (emitProbeSnapshot)
                {
                    LogProbeCandidateDecision(
                        scanTypes[typeIndex],
                        object,
                        inventory,
                        targetHandle.isNull() ? "reject_null_handle" : "reject_duplicate",
                        usedBuildingInventoryFallback);
                }
                continue;
            }

            Ogre::Vector3 worldPos;
            if (!TryGetTargetWorldPosition(targetHandle, &worldPos))
            {
                if (emitProbeSnapshot)
                {
                    LogProbeCandidateDecision(
                        scanTypes[typeIndex],
                        object,
                        inventory,
                        "reject_no_world_pos",
                        usedBuildingInventoryFallback);
                }
                continue;
            }

            ContainerTarget target;
            target.targetHandle = targetHandle;
            target.worldPos = worldPos;
            target.anchorYOffsetWorld = ResolveTargetAnchorYOffsetWorld(targetHandle);
            target.distanceSq = worldPos.squaredDistance(probeCenter);
            target.dataType = object->getDataType();
            g_state.targetCache.push_back(target);
            ++matchedTargets;
            ++matchedObjectsByType[typeIndex];

            if (emitProbeSnapshot)
            {
                LogProbeCandidateDecision(
                    scanTypes[typeIndex],
                    object,
                    inventory,
                    "match",
                    usedBuildingInventoryFallback);
            }
        }
    }

    std::sort(g_state.targetCache.begin(), g_state.targetCache.end(), ContainerTargetDistanceLess);
    if (g_state.targetCache.size() > kMaxContainerMarkers)
    {
        g_state.targetCache.resize(kMaxContainerMarkers);
    }

    if (ShouldLogDebug())
    {
        std::stringstream line;
        line << "probe center=(" << probeCenter.x << "," << probeCenter.y << "," << probeCenter.z << ")"
             << " scanned=" << scannedObjects
             << " matched=" << matchedTargets
             << " matched_building=" << matchedObjectsByType[0]
             << " matched_container=" << matchedObjectsByType[1]
             << " cached=" << g_state.targetCache.size();
        LogDebugLine(line.str());
    }

    if (emitProbeSnapshot)
    {
        std::stringstream line;
        line << "probe_snapshot end"
             << " scanned=" << scannedObjects
             << " matched=" << matchedTargets
             << " cached=" << g_state.targetCache.size();
        LogInfoLine(line.str());
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

void SetMarkerCaption(MyGUI::TextBox* textBox, const char* caption)
{
    if (!textBox || !caption)
    {
        return;
    }

    try
    {
        textBox->setCaption(caption);
    }
    catch (...)
    {
    }
}

void SetMarkerTextColour(MyGUI::TextBox* textBox, const MyGUI::Colour& colour)
{
    if (!textBox)
    {
        return;
    }

    try
    {
        textBox->setTextColour(colour);
    }
    catch (...)
    {
    }
}

void SetMarkerTextFontHeight(MyGUI::TextBox* textBox, int fontHeight)
{
    if (!textBox || fontHeight <= 0)
    {
        return;
    }

    try
    {
        textBox->setFontHeight(fontHeight);
    }
    catch (...)
    {
    }
}

void SetMarkerIconColour(MyGUI::ImageBox* imageBox, const MyGUI::Colour& colour)
{
    if (!imageBox)
    {
        return;
    }

    try
    {
        imageBox->setColour(colour);
    }
    catch (...)
    {
    }
}

bool TryApplyMarkerIconTexture(MyGUI::ImageBox* imageBox, const std::string& textureName)
{
    if (!imageBox || textureName.empty())
    {
        return false;
    }

    std::vector<std::string> candidates;
    candidates.push_back(textureName);
    if (textureName.find('/') == std::string::npos && textureName.find('\\') == std::string::npos)
    {
        candidates.push_back(std::string("gui/gfx/") + textureName);
        candidates.push_back(std::string("mods/") + kPluginName + "/gui/gfx/" + textureName);
    }
    else if (textureName.find("mods/") != 0 && textureName.find("mods\\") != 0)
    {
        candidates.push_back(std::string("mods/") + kPluginName + "/" + textureName);
    }

    for (size_t i = 0; i < candidates.size(); ++i)
    {
        try
        {
            imageBox->setImageTexture(candidates[i]);
            const MyGUI::IntSize imageSize = imageBox->getImageSize();
            if (imageSize.width <= 0 || imageSize.height <= 0)
            {
                continue;
            }

            imageBox->setImageCoord(MyGUI::IntCoord(0, 0, imageSize.width, imageSize.height));
            imageBox->setImageTile(imageSize);
            return true;
        }
        catch (...)
        {
        }
    }

    return false;
}

void SetMarkerPosition(MarkerWidget& marker, int left, int top, bool showIcon, bool showText)
{
    int iconLeft = left;
    if (!showText && showIcon)
    {
        iconLeft = left + ((kMarkerWidthPx - kMarkerIconSizePx) / 2);
    }

    if (marker.icon)
    {
        try
        {
            marker.icon->setCoord(iconLeft, top, kMarkerIconSizePx, kMarkerIconSizePx);
        }
        catch (...)
        {
        }
    }

    if (marker.text)
    {
        try
        {
            const int textLeft = showIcon ? (iconLeft + kMarkerIconSizePx + kMarkerIconGapPx) : left;
            int textWidth = kMarkerWidthPx - (textLeft - left);
            if (textWidth < 0)
            {
                textWidth = 0;
            }
            marker.text->setCoord(textLeft, top, textWidth, kMarkerHeightPx);
            marker.text->setTextAlign(showIcon ? MyGUI::Align::Left : MyGUI::Align::Center);
        }
        catch (...)
        {
        }
    }
}

void SetMarkerVisible(MarkerWidget& marker, bool showIcon, bool showText)
{
    SetWidgetVisible(marker.icon, showIcon);
    SetWidgetVisible(marker.text, showText);
}

void HideAllMarkerWidgetsInternal()
{
    for (size_t i = 0; i < g_state.markerWidgets.size(); ++i)
    {
        SetMarkerVisible(g_state.markerWidgets[i], false, false);
    }
}

bool CreateMarkerWidgetAt(size_t index)
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return false;
    }

    try
    {
        std::stringstream name;
        name << "CH_Marker_" << index << "_" << g_state.markerWidgetSerial++;

        MyGUI::ImageBox* icon = gui->createWidget<MyGUI::ImageBox>(
            "ImageBox",
            MyGUI::IntCoord(0, 0, kMarkerIconSizePx, kMarkerIconSizePx),
            MyGUI::Align::Default,
            "Top",
            name.str() + "_icon");

        MyGUI::TextBox* text = gui->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(kMarkerIconSizePx + kMarkerIconGapPx, 0, kMarkerWidthPx - (kMarkerIconSizePx + kMarkerIconGapPx), kMarkerHeightPx),
            MyGUI::Align::Default,
            "Top",
            name.str());
        if (text == 0)
        {
            text = gui->createWidget<MyGUI::TextBox>(
                "TextBox",
                MyGUI::IntCoord(kMarkerIconSizePx + kMarkerIconGapPx, 0, kMarkerWidthPx - (kMarkerIconSizePx + kMarkerIconGapPx), kMarkerHeightPx),
                MyGUI::Align::Default,
                "Top",
                name.str() + "_fallback");
        }

        if (icon)
        {
            icon->setNeedMouseFocus(false);
            icon->setVisible(false);
        }
        if (text)
        {
            text->setNeedMouseFocus(false);
            text->setTextShadow(true);
            text->setVisible(false);
        }

        MarkerWidget marker;
        marker.icon = icon;
        marker.text = text;

        if (index >= g_state.markerWidgets.size())
        {
            g_state.markerWidgets.push_back(marker);
        }
        else
        {
            g_state.markerWidgets[index] = marker;
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool EnsureMarkerPool(size_t requiredCount)
{
    if (requiredCount > kMaxContainerMarkers)
    {
        requiredCount = kMaxContainerMarkers;
    }

    while (g_state.markerWidgets.size() < requiredCount)
    {
        if (!CreateMarkerWidgetAt(g_state.markerWidgets.size()))
        {
            return false;
        }
    }

    for (size_t i = 0; i < requiredCount; ++i)
    {
        if (!g_state.markerWidgets[i].icon
            && !g_state.markerWidgets[i].text
            && !CreateMarkerWidgetAt(i))
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

bool ConvertProjectionToPixels(float rawX, float rawY, int viewWidth, int viewHeight, float* pixelXOut, float* pixelYOut)
{
    if (!pixelXOut || !pixelYOut || viewWidth <= 0 || viewHeight <= 0)
    {
        return false;
    }

    if (rawX >= 0.0f && rawX <= 1.0f && rawY >= 0.0f && rawY <= 1.0f)
    {
        *pixelXOut = rawX * static_cast<float>(viewWidth);
        *pixelYOut = rawY * static_cast<float>(viewHeight);
        return true;
    }

    if (rawX >= -1.0f && rawX <= 1.0f && rawY >= -1.0f && rawY <= 1.0f)
    {
        *pixelXOut = (rawX * 0.5f + 0.5f) * static_cast<float>(viewWidth);
        *pixelYOut = (rawY * 0.5f + 0.5f) * static_cast<float>(viewHeight);
        return true;
    }

    *pixelXOut = rawX;
    *pixelYOut = rawY;
    return true;
}

bool EnsureProjectionUtility()
{
    if (g_state.projectionUtility)
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

    g_state.projectionUtility = reinterpret_cast<UtilityT*>(baseAddress + utilityOffset);
    return g_state.projectionUtility != 0;
}

bool TryProjectWorldToScreenPx(const Ogre::Vector3& worldPos, float* xOut, float* yOut)
{
    if (!xOut || !yOut || !EnsureProjectionUtility())
    {
        return false;
    }

    float x = 0.0f;
    float y = 0.0f;
    bool projected = false;
    __try
    {
        projected = g_state.projectionUtility->worldToScreenPX(worldPos, x, y);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_state.projectionUtility = 0;
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

void TickMarkerRender()
{
    if (!g_state.config.enabled
        || !g_state.highlightRuntimeActive
        || (!g_state.config.showMarkerIcons && !g_state.config.showMarkerText)
        || g_state.targetCache.empty())
    {
        HideAllMarkerWidgetsInternal();
        return;
    }

    if (!EnsureMarkerPool(g_state.targetCache.size()))
    {
        HideAllMarkerWidgetsInternal();
        return;
    }

    int viewWidth = 0;
    int viewHeight = 0;
    const bool hasViewSize = TryGetViewSize(&viewWidth, &viewHeight);

    size_t visibleMarkerCount = 0;
    for (size_t i = 0; i < g_state.targetCache.size(); ++i)
    {
        Ogre::Vector3 worldPos = g_state.targetCache[i].worldPos;
        if (!TryGetTargetWorldPosition(g_state.targetCache[i].targetHandle, &worldPos))
        {
            worldPos = g_state.targetCache[i].worldPos;
        }

        Ogre::Vector3 markerAnchor = worldPos;
        markerAnchor.y += g_state.targetCache[i].anchorYOffsetWorld;

        float projectedX = 0.0f;
        float projectedY = 0.0f;
        if (!TryProjectWorldToScreenPx(markerAnchor, &projectedX, &projectedY))
        {
            continue;
        }

        float pixelX = projectedX;
        float pixelY = projectedY;
        if (hasViewSize && !ConvertProjectionToPixels(projectedX, projectedY, viewWidth, viewHeight, &pixelX, &pixelY))
        {
            continue;
        }

        if (visibleMarkerCount >= g_state.markerWidgets.size())
        {
            break;
        }

        int markerLeft = static_cast<int>(pixelX) - (kMarkerWidthPx / 2);
        int markerTop = static_cast<int>(pixelY) - kMarkerYOffsetPx;
        if (hasViewSize)
        {
            const int maxLeft = (viewWidth > kMarkerWidthPx) ? (viewWidth - kMarkerWidthPx) : 0;
            const int maxTop = (viewHeight > kMarkerHeightPx) ? (viewHeight - kMarkerHeightPx) : 0;
            markerLeft = ClampInt(markerLeft, 0, maxLeft);
            markerTop = ClampInt(markerTop, 0, maxTop);
        }

        MarkerWidget& marker = g_state.markerWidgets[visibleMarkerCount];
        bool showIcon = g_state.config.showMarkerIcons
            && marker.icon != 0
            && TryApplyMarkerIconTexture(marker.icon, g_state.config.markerIconTexture);
        if (showIcon)
        {
            SetMarkerIconColour(marker.icon, g_state.config.markerColour);
        }

        const bool showText = g_state.config.showMarkerText && marker.text != 0;
        if (showText)
        {
            SetMarkerCaption(marker.text, g_state.config.markerText.c_str());
            SetMarkerTextFontHeight(marker.text, static_cast<int>(g_state.config.markerTextSizePx));
            SetMarkerTextColour(marker.text, g_state.config.markerColour);
        }

        SetMarkerPosition(marker, markerLeft, markerTop, showIcon, showText);
        SetMarkerVisible(marker, showIcon, showText);
        ++visibleMarkerCount;
    }

    for (size_t i = visibleMarkerCount; i < g_state.markerWidgets.size(); ++i)
    {
        SetMarkerVisible(g_state.markerWidgets[i], false, false);
    }
}

bool MaterialPtrsReferSameObject(const Ogre::MaterialPtr& left, const Ogre::MaterialPtr& right)
{
    if (left.isNull() || right.isNull())
    {
        return false;
    }

    return left.getPointer() == right.getPointer();
}

std::string BuildTintCloneName(const hand& targetHandle, size_t bindingIndex, size_t subEntityIndex)
{
    std::stringstream name;
    name << "CH_Tint_"
         << targetHandle.type << "_"
         << targetHandle.index << "_"
         << targetHandle.serial << "_"
         << bindingIndex << "_"
         << subEntityIndex << "_"
         << g_state.tintCloneSerial++;
    return name.str();
}

bool ApplyTintConstantsToPass(Ogre::Pass* pass, const Ogre::ColourValue& colour, bool depthOverride, bool* appliedColourOut)
{
    if (!pass)
    {
        return false;
    }

    bool appliedAny = false;
    bool appliedColour = false;

    if (pass->hasFragmentProgram())
    {
        try
        {
            Ogre::GpuProgramParametersSharedPtr fragmentParams = pass->getFragmentProgramParameters();
            if (!fragmentParams.isNull())
            {
                const char* colourParamNames[] = {
                    kColorOverrideParam,
                    kColourOverrideParam,
                    kColorOverrideParamCamel,
                    kColourOverrideParamCamel
                };
                for (size_t i = 0; i < (sizeof(colourParamNames) / sizeof(colourParamNames[0])); ++i)
                {
                    try
                    {
                        fragmentParams->setNamedConstant(colourParamNames[i], colour);
                        appliedAny = true;
                        appliedColour = true;
                    }
                    catch (...)
                    {
                    }
                }
            }
        }
        catch (...)
        {
        }
    }

    if (pass->hasVertexProgram())
    {
        try
        {
            Ogre::GpuProgramParametersSharedPtr vertexParams = pass->getVertexProgramParameters();
            if (!vertexParams.isNull())
            {
                const char* depthParamNames[] = {
                    kDepthOverrideParam,
                    kOverrideDepthLowerParam
                };
                for (size_t i = 0; i < (sizeof(depthParamNames) / sizeof(depthParamNames[0])); ++i)
                {
                    try
                    {
                        vertexParams->setNamedConstant(depthParamNames[i], depthOverride ? 1 : 0);
                        appliedAny = true;
                    }
                    catch (...)
                    {
                    }
                }
            }
        }
        catch (...)
        {
        }
    }

    if (appliedColourOut)
    {
        *appliedColourOut = appliedColour;
    }
    return appliedAny;
}

bool ApplyFallbackTintToPass(Ogre::Pass* pass, const Ogre::ColourValue& colour, bool depthOverride)
{
    if (!pass)
    {
        return false;
    }

    try
    {
        pass->setAmbient(Ogre::ColourValue(colour.r * 0.35f, colour.g * 0.35f, colour.b * 0.35f, 1.0f));
        pass->setDiffuse(Ogre::ColourValue(1.0f, 1.0f, 1.0f, 1.0f));
        pass->setSelfIllumination(Ogre::ColourValue(colour.r * 0.65f, colour.g * 0.65f, colour.b * 0.65f, 1.0f));
        if (depthOverride)
        {
            pass->setDepthCheckEnabled(false);
            pass->setDepthWriteEnabled(false);
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool ApplyTintToMaterialClone(const Ogre::MaterialPtr& material, const Ogre::ColourValue& colour, bool depthOverride)
{
    if (material.isNull())
    {
        return false;
    }

    unsigned short techniqueCount = 0;
    try
    {
        techniqueCount = material->getNumTechniques();
    }
    catch (...)
    {
        return false;
    }

    bool appliedAny = false;
    for (unsigned short techniqueIndex = 0; techniqueIndex < techniqueCount; ++techniqueIndex)
    {
        Ogre::Technique* technique = 0;
        try
        {
            technique = material->getTechnique(techniqueIndex);
        }
        catch (...)
        {
            technique = 0;
        }
        if (!technique)
        {
            continue;
        }

        unsigned short passCount = 0;
        try
        {
            passCount = technique->getNumPasses();
        }
        catch (...)
        {
            passCount = 0;
        }

        for (unsigned short passIndex = 0; passIndex < passCount; ++passIndex)
        {
            Ogre::Pass* pass = 0;
            try
            {
                pass = technique->getPass(passIndex);
            }
            catch (...)
            {
                pass = 0;
            }
            if (!pass)
            {
                continue;
            }

            bool appliedColourConstant = false;
            ApplyTintConstantsToPass(pass, colour, depthOverride, &appliedColourConstant);
            if (appliedColourConstant)
            {
                appliedAny = true;
                continue;
            }

            if (ApplyFallbackTintToPass(pass, colour, depthOverride))
            {
                appliedAny = true;
            }
        }
    }

    return appliedAny;
}

void ClearTintBinding(TintEntityBinding* binding)
{
    if (binding == 0)
    {
        return;
    }

    if (binding->entity != 0)
    {
        for (size_t i = 0; i < binding->originalMaterials.size(); ++i)
        {
            if (binding->originalMaterials[i].isNull())
            {
                continue;
            }

            try
            {
                Ogre::SubEntity* subEntity = binding->entity->getSubEntity(i);
                if (subEntity != 0)
                {
                    subEntity->setMaterial(binding->originalMaterials[i]);
                }
            }
            catch (...)
            {
            }
        }
    }

    binding->entity = 0;
    binding->originalMaterials.clear();
    binding->cloneMaterials.clear();
}

void ClearTintEntry(ContainerTintEntry* entry)
{
    if (entry == 0)
    {
        return;
    }

    for (size_t i = 0; i < entry->bindings.size(); ++i)
    {
        ClearTintBinding(&entry->bindings[i]);
    }
    entry->bindings.clear();
}

int FindTintEntryByHandle(const hand& targetHandle)
{
    for (size_t i = 0; i < g_state.tintEntries.size(); ++i)
    {
        if (HandlesEqualByKey(g_state.tintEntries[i].targetHandle, targetHandle))
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void AppendTintEntityUnique(std::vector<Ogre::Entity*>* outEntities, Ogre::Entity* entity)
{
    if (outEntities == 0 || entity == 0 || EntityVectorContains(*outEntities, entity))
    {
        return;
    }

    outEntities->push_back(entity);
}

void CollectTintEntitiesForTarget(const hand& targetHandle, std::vector<Ogre::Entity*>* outEntities)
{
    if (outEntities == 0)
    {
        return;
    }

    outEntities->clear();

    Building* building = targetHandle.getBuilding();
    if (building != 0 && IsHighlightableContainerBuilding(building))
    {
        Ogre::SceneNode* rootNode = TryGetBuildingRootNode(building);
        CollectBuildingEntitiesRecursive(rootNode, 0, outEntities);
    }

    (void)targetHandle;
}

bool ApplyTintToEntityBinding(const hand& targetHandle, size_t bindingIndex, Ogre::Entity* entity, TintEntityBinding* binding)
{
    if (entity == 0 || binding == 0)
    {
        return false;
    }

    if (binding->entity != entity)
    {
        ClearTintBinding(binding);
        binding->entity = entity;
    }

    size_t subEntityCount = 0;
    try
    {
        subEntityCount = entity->getNumSubEntities();
    }
    catch (...)
    {
        return false;
    }

    if (subEntityCount == 0)
    {
        return false;
    }

    if (binding->originalMaterials.size() != subEntityCount || binding->cloneMaterials.size() != subEntityCount)
    {
        binding->originalMaterials.clear();
        binding->cloneMaterials.clear();
        binding->originalMaterials.resize(subEntityCount);
        binding->cloneMaterials.resize(subEntityCount);
    }

    bool appliedAny = false;
    for (size_t subEntityIndex = 0; subEntityIndex < subEntityCount; ++subEntityIndex)
    {
        Ogre::SubEntity* subEntity = 0;
        try
        {
            subEntity = entity->getSubEntity(subEntityIndex);
        }
        catch (...)
        {
            subEntity = 0;
        }
        if (subEntity == 0)
        {
            continue;
        }

        Ogre::MaterialPtr currentMaterial;
        try
        {
            currentMaterial = subEntity->getMaterial();
        }
        catch (...)
        {
            currentMaterial.setNull();
        }
        if (currentMaterial.isNull())
        {
            continue;
        }

        Ogre::MaterialPtr& cloneMaterial = binding->cloneMaterials[subEntityIndex];
        const bool currentIsClone =
            !cloneMaterial.isNull() && MaterialPtrsReferSameObject(currentMaterial, cloneMaterial);
        if (!currentIsClone)
        {
            binding->originalMaterials[subEntityIndex] = currentMaterial;
            cloneMaterial.setNull();
        }
        if (binding->originalMaterials[subEntityIndex].isNull())
        {
            binding->originalMaterials[subEntityIndex] = currentMaterial;
        }

        if (cloneMaterial.isNull())
        {
            try
            {
                cloneMaterial = binding->originalMaterials[subEntityIndex]->clone(
                    BuildTintCloneName(targetHandle, bindingIndex, subEntityIndex));
            }
            catch (...)
            {
                cloneMaterial.setNull();
            }
        }
        if (cloneMaterial.isNull())
        {
            continue;
        }

        if (!ApplyTintToMaterialClone(cloneMaterial, g_state.config.tintColour, g_state.config.tintForceDepthOverride))
        {
            continue;
        }

        try
        {
            subEntity->setMaterial(cloneMaterial);
            appliedAny = true;
        }
        catch (...)
        {
        }
    }

    return appliedAny;
}

void ClearAllTint()
{
    for (size_t i = 0; i < g_state.tintEntries.size(); ++i)
    {
        ClearTintEntry(&g_state.tintEntries[i]);
    }
    g_state.tintEntries.clear();
}

void SyncTint()
{
    if (!g_state.config.enableTint || !g_state.highlightRuntimeActive || g_state.targetCache.empty())
    {
        ClearAllTint();
        return;
    }

    std::vector<hand> activeHandles;
    for (size_t targetIndex = 0; targetIndex < g_state.targetCache.size(); ++targetIndex)
    {
        std::vector<Ogre::Entity*> entities;
        CollectTintEntitiesForTarget(g_state.targetCache[targetIndex].targetHandle, &entities);
        if (entities.empty())
        {
            continue;
        }

        int tintEntryIndex = FindTintEntryByHandle(g_state.targetCache[targetIndex].targetHandle);
        if (tintEntryIndex < 0)
        {
            ContainerTintEntry entry;
            entry.targetHandle = g_state.targetCache[targetIndex].targetHandle;
            g_state.tintEntries.push_back(entry);
            tintEntryIndex = static_cast<int>(g_state.tintEntries.size() - 1);
        }

        ContainerTintEntry& tintEntry = g_state.tintEntries[static_cast<size_t>(tintEntryIndex)];
        while (tintEntry.bindings.size() > entities.size())
        {
            ClearTintBinding(&tintEntry.bindings.back());
            tintEntry.bindings.pop_back();
        }
        while (tintEntry.bindings.size() < entities.size())
        {
            TintEntityBinding binding;
            binding.entity = 0;
            tintEntry.bindings.push_back(binding);
        }

        bool appliedAny = false;
        for (size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex)
        {
            if (ApplyTintToEntityBinding(
                tintEntry.targetHandle,
                entityIndex,
                entities[entityIndex],
                &tintEntry.bindings[entityIndex]))
            {
                appliedAny = true;
            }
        }

        if (appliedAny)
        {
            activeHandles.push_back(tintEntry.targetHandle);
        }
    }

    for (size_t i = 0; i < g_state.tintEntries.size();)
    {
        if (HandVectorContains(activeHandles, g_state.tintEntries[i].targetHandle))
        {
            ++i;
            continue;
        }

        ClearTintEntry(&g_state.tintEntries[i]);
        g_state.tintEntries.erase(g_state.tintEntries.begin() + i);
    }
}

void ResetHighlightRuntime()
{
    g_state.highlightRuntimeActive = false;
    g_state.targetCache.clear();
    g_state.lastProbeTickMs = 0;
    HideAllMarkerWidgetsInternal();
    ClearAllTint();
}

void TickContainerHighlightRuntime()
{
    if (!g_state.config.enabled || ou == 0 || ou->player == 0)
    {
        ResetHighlightRuntime();
        return;
    }

    if (!IsHighlightGateOpen(g_state.config))
    {
        ResetHighlightRuntime();
        return;
    }

    g_state.highlightRuntimeActive = true;

    const DWORD nowMs = GetTickCount();
    if (g_state.lastProbeTickMs == 0
        || (nowMs - g_state.lastProbeTickMs) >= g_state.config.updateIntervalMs)
    {
        RefreshContainerTargetCache();
        g_state.lastProbeTickMs = nowMs;
    }

    TickMarkerRender();
}

void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    if (g_playerInterfaceUpdateUTOrig)
    {
        g_playerInterfaceUpdateUTOrig(thisptr);
    }

    TickContainerHighlightRuntime();
}

void GameWorld_mainLoopGPUSensitiveStuff_hook(GameWorld* thisptr, float time)
{
    (void)thisptr;
    (void)time;

    if (g_state.highlightRuntimeActive)
    {
        SyncTint();
    }
    else
    {
        ClearAllTint();
    }

    if (g_gameWorldMainLoopGPUSensitiveStuffOrig)
    {
        g_gameWorldMainLoopGPUSensitiveStuffOrig(thisptr, time);
    }
}
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

    g_state.targetCache.reserve(kMaxContainerMarkers);
    g_state.markerWidgets.reserve(kMaxContainerMarkers);
    g_state.tintEntries.reserve(kMaxContainerMarkers);

    LoadConfigState();
    ConfigureModHubClient();

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        &g_playerInterfaceUpdateUTOrig))
    {
        LogErrorLine("could not hook PlayerInterface::updateUT");
        return;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&GameWorld::_NV_mainLoop_GPUSensitiveStuff),
        GameWorld_mainLoopGPUSensitiveStuff_hook,
        &g_gameWorldMainLoopGPUSensitiveStuffOrig))
    {
        LogErrorLine("could not hook GameWorld::mainLoop_GPUSensitiveStuff");
        return;
    }

    LogInfoLine("update and gpu hooks installed");
    StartModHubClient();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        char dllPath[MAX_PATH] = { 0 };
        if (GetModuleFileNameA(hModule, dllPath, MAX_PATH) > 0)
        {
            const std::string fullPath(dllPath);
            const std::string::size_type separator = fullPath.find_last_of("\\/");
            if (separator != std::string::npos)
            {
                g_state.settingsPath = fullPath.substr(0, separator) + "\\" + kConfigFileName;
            }
        }
    }

    return TRUE;
}
