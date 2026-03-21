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
#include <ogre/OgreInstanceBatch.h>
#include <ogre/OgreInstancedEntity.h>
#include <ogre/OgreGpuProgramParams.h>
#include <ogre/OgreMaterial.h>
#include <ogre/OgreMaterialManager.h>
#include <ogre/OgreManualObject.h>
#include <ogre/OgreMovableObject.h>
#include <ogre/OgrePass.h>
#include <ogre/OgreRenderQueue.h>
#include <ogre/OgreResourceGroupManager.h>
#include <ogre/OgreSceneManager.h>
#include <ogre/OgreSceneNode.h>
#include <ogre/OgreSubEntity.h>
#include <ogre/OgreTechnique.h>
#include <ogre/Math/Simple/C/OgreAabb.h>

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
const int kMarkerWidthPx = 64;
const int kMarkerHeightPx = 18;
const int kMarkerYOffsetPx = 24;
const int kMarkerIconGapPx = 1;
const int kMarkerIconSizePx = 18;
const float kDefaultAnchorYOffsetWorld = 0.90f;
const float kContainerAnchorPaddingWorld = 0.30f;
const DWORD kDefaultUpdateIntervalMs = 150;
const float kDefaultMaxHighlightDistance = 900.0f;
const int kDefaultMaxObjectsPerType = 256;
const char* kDefaultMarkerText = "BOX";
const char* kDefaultMarkerIconTexture = "figure.png";
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

struct TintBatchBinding
{
    Ogre::InstanceBatch* batch;
    Ogre::MaterialPtr originalMaterial;
    Ogre::MaterialPtr cloneMaterial;
};

struct TintOverlayBinding
{
    TintOverlayBinding()
        : sceneManager(0)
        , sceneNode(0)
        , manualObject(0)
    {
    }

    Ogre::SceneManager* sceneManager;
    Ogre::SceneNode* sceneNode;
    Ogre::ManualObject* manualObject;
};

struct ContainerTintEntry
{
    hand targetHandle;
    std::vector<TintEntityBinding> entityBindings;
    std::vector<TintBatchBinding> batchBindings;
    TintOverlayBinding overlayBinding;
};

struct RuntimeState
{
    RuntimeState()
        : projectionUtility(0)
        , lastProbeTickMs(0)
        , lastTintDebugLogTickMs(0)
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
    DWORD lastTintDebugLogTickMs;
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
const char* kHighlightObjectVertexProgram = "CH_Object_VP";
const char* kHighlightObjectColouredVertexProgram = "CH_Object_Coloured_VP";
const char* kHighlightObjectFragmentProgram = "CH_Object_FP";
const char* kHighlightObjectAlphaFragmentProgram = "CH_Object_Alpha_FP";
const char* kHighlightObjectColouredFragmentProgram = "CH_Object_Coloured_FP";
const char* kHighlightObjectDoubleSidedFragmentProgram = "CH_Object_DoubleSided_FP";
const char* kHighlightObjectDoubleSidedColouredFragmentProgram = "CH_Object_DoubleSided_Coloured_FP";
const bool kEnableInstancedBatchTint = false;
const char* kHighlightObjectDualFragmentProgram = "CH_Object_Dual_FP";
const char* kHighlightObjectEmissiveFragmentProgram = "CH_Object_Emissive_FP";
const char* kTintOverlayFillMaterialName = "ContainerHighlight/TintOverlayFill";
const char* kTintOverlayOutlineMaterialName = "ContainerHighlight/TintOverlayOutline";
const char* kTintOverlayDepthOutlineMaterialName = "ContainerHighlight/TintOverlayDepthOutline";
const float kTintOverlayPaddingWorld = 0.35f;
const float kTintOverlayMinExtentWorld = 0.60f;
const unsigned char kTintOverlayRenderQueueGroup = Ogre::RENDER_QUEUE_OVERLAY;
const float kTintOverlayDiagnosticScaleMultiplier = 1.75f;
const float kTintOverlayDiagnosticYOffsetMultiplier = 0.90f;

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
PhysicsCollection* TryGetBuildingPhysical(Building* building);
Ogre::SceneNode* TryGetBuildingRootNode(Building* building);

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

bool ShouldEmitTintInvestigation()
{
    if (!ShouldLogDebug())
    {
        return false;
    }

    const DWORD nowMs = GetTickCount();
    if (g_state.lastTintDebugLogTickMs != 0 && (nowMs - g_state.lastTintDebugLogTickMs) < 1000)
    {
        return false;
    }

    g_state.lastTintDebugLogTickMs = nowMs;
    return true;
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

void AppendMovableAsTintEntityUnique(std::vector<Ogre::Entity*>* outEntities, Ogre::MovableObject* movable)
{
    if (outEntities == 0 || movable == 0)
    {
        return;
    }

    try
    {
        if (movable->getMovableType() == Ogre::EntityFactory::FACTORY_TYPE_NAME)
        {
            AppendTintEntityUnique(outEntities, static_cast<Ogre::Entity*>(movable));
        }
    }
    catch (...)
    {
    }
}

bool InstanceBatchVectorContains(const std::vector<Ogre::InstanceBatch*>& batches, Ogre::InstanceBatch* batch)
{
    if (batch == 0)
    {
        return false;
    }

    for (size_t i = 0; i < batches.size(); ++i)
    {
        if (batches[i] == batch)
        {
            return true;
        }
    }

    return false;
}

void AppendTintBatchUnique(std::vector<Ogre::InstanceBatch*>* outBatches, Ogre::InstanceBatch* batch)
{
    if (outBatches == 0 || batch == 0 || InstanceBatchVectorContains(*outBatches, batch))
    {
        return;
    }

    outBatches->push_back(batch);
}

void AppendMovableAsTintBatchUnique(std::vector<Ogre::InstanceBatch*>* outBatches, Ogre::MovableObject* movable)
{
    if (outBatches == 0 || movable == 0)
    {
        return;
    }

    try
    {
        const std::string movableType = movable->getMovableType().c_str();
        if (movableType == "InstancedEntity")
        {
            Ogre::InstancedEntity* instancedEntity = static_cast<Ogre::InstancedEntity*>(movable);
            AppendTintBatchUnique(outBatches, instancedEntity->_getOwner());
        }
        else if (movableType == "InstanceBatch")
        {
            AppendTintBatchUnique(outBatches, static_cast<Ogre::InstanceBatch*>(movable));
        }
    }
    catch (...)
    {
    }
}

std::string SafeGetMovableTypeName(Ogre::MovableObject* movable)
{
    if (movable == 0)
    {
        return "null";
    }

    try
    {
        return movable->getMovableType().c_str();
    }
    catch (...)
    {
        return "unknown";
    }
}

std::string SafeGetMovableName(Ogre::MovableObject* movable)
{
    if (movable == 0)
    {
        return "<null>";
    }

    try
    {
        const std::string name = movable->getName().c_str();
        return name.empty() ? "<unnamed>" : name;
    }
    catch (...)
    {
        return "<name-error>";
    }
}

const char* SafeGetBuildingClassTypeName(Building* building)
{
    if (building == 0)
    {
        return "null";
    }

    __try
    {
        return BuildingClassTypeNameForLog(building->getBuildingClass());
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return "error";
    }
}

const char* SafeGetBuildingFunctionTypeName(Building* building)
{
    if (building == 0)
    {
        return "null";
    }

    __try
    {
        return BuildingFunctionNameForLog(building->getSpecialFunction());
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return "error";
    }
}

int SafeGetBuildingEntitiesLoaded(Building* building)
{
    if (building == 0)
    {
        return -1;
    }

    __try
    {
        return building->entitiesLoaded ? 1 : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

int SafeGetBuildingVisible(Building* building)
{
    if (building == 0)
    {
        return -1;
    }

    __try
    {
        return building->getVisible() ? 1 : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

int SafeGetBuildingCreated(Building* building)
{
    if (building == 0)
    {
        return -1;
    }

    __try
    {
        return building->isCreated() ? 1 : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

int SafeGetPhysicalLoaded(PhysicsCollection* physical)
{
    if (physical == 0)
    {
        return -1;
    }

    __try
    {
        return physical->isLoaded() ? 1 : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

std::string BuildMovableInvestigationSample(const char* prefix, size_t depth, Ogre::MovableObject* movable)
{
    std::stringstream sample;
    sample << prefix;
    if (depth != static_cast<size_t>(-1))
    {
        sample << ":" << depth;
    }
    sample << ":" << SafeGetMovableTypeName(movable)
           << ":" << SafeGetMovableName(movable);
    return sample.str();
}

void AppendInvestigationSample(std::vector<std::string>* samples, const std::string& value)
{
    if (samples == 0 || value.empty() || samples->size() >= 6)
    {
        return;
    }

    samples->push_back(value);
}

void CollectSceneNodeMovableTypeSamples(
    Ogre::SceneNode* node,
    size_t depth,
    size_t* nodeCountOut,
    size_t* attachedCountOut,
    size_t* entityLikeCountOut,
    std::vector<std::string>* samples)
{
    if (node == 0 || depth > 6)
    {
        return;
    }

    if (nodeCountOut != 0)
    {
        ++(*nodeCountOut);
    }

    const size_t attachedCount = node->numAttachedObjects();
    if (attachedCountOut != 0)
    {
        *attachedCountOut += attachedCount;
    }

    for (size_t attachedIndex = 0; attachedIndex < attachedCount; ++attachedIndex)
    {
        Ogre::MovableObject* movable = node->getAttachedObject(attachedIndex);
        if (movable == 0)
        {
            continue;
        }

        const std::string movableType = SafeGetMovableTypeName(movable);
        if (entityLikeCountOut != 0 && movableType == Ogre::EntityFactory::FACTORY_TYPE_NAME)
        {
            ++(*entityLikeCountOut);
        }

        AppendInvestigationSample(samples, BuildMovableInvestigationSample("node", depth, movable));
    }

    const size_t childCount = node->numChildren();
    for (size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        Ogre::SceneNode* childNode = static_cast<Ogre::SceneNode*>(node->getChild(childIndex));
        CollectSceneNodeMovableTypeSamples(
            childNode,
            depth + 1,
            nodeCountOut,
            attachedCountOut,
            entityLikeCountOut,
            samples);
    }
}

void CollectPhysicalMovableTypeSamples(
    PhysicsCollection* physical,
    size_t* staticEntCountOut,
    size_t* partCountOut,
    size_t* instancedStaticEntCountOut,
    size_t* entityLikeStaticEntCountOut,
    std::vector<std::string>* staticEntSamples)
{
    if (physical == 0)
    {
        return;
    }

    if (staticEntCountOut != 0)
    {
        *staticEntCountOut = physical->staticEnts.size();
    }
    if (partCountOut != 0)
    {
        *partCountOut = physical->parts.size();
    }

    for (lektor<PhysicsCollection::StaticEnt*>::const_iterator it = physical->staticEnts.begin();
         it != physical->staticEnts.end();
         ++it)
    {
        PhysicsCollection::StaticEnt* staticEnt = *it;
        if (staticEnt == 0 || staticEnt->ent == 0)
        {
            continue;
        }

        if (instancedStaticEntCountOut != 0 && staticEnt->instanced)
        {
            ++(*instancedStaticEntCountOut);
        }

        const std::string movableType = SafeGetMovableTypeName(staticEnt->ent);
        if (entityLikeStaticEntCountOut != 0 && movableType == Ogre::EntityFactory::FACTORY_TYPE_NAME)
        {
            ++(*entityLikeStaticEntCountOut);
        }

        std::stringstream sample;
        sample << BuildMovableInvestigationSample("static", static_cast<size_t>(-1), staticEnt->ent)
               << ":inst=" << (staticEnt->instanced ? 1 : 0)
               << ":shell=" << (staticEnt->isShell ? 1 : 0)
               << ":emissive=" << (staticEnt->isEmissive ? 1 : 0);
        if (staticEnt->partData != 0 && !staticEnt->partData->name.empty())
        {
            sample << ":part=" << staticEnt->partData->name;
        }
        if (staticEnt->mat != 0 && !staticEnt->mat->name.empty())
        {
            sample << ":mat=" << staticEnt->mat->name;
        }
        AppendInvestigationSample(staticEntSamples, sample.str());
    }
}

void LogTintEntityCollectionInvestigation(const hand& targetHandle)
{
    Building* building = targetHandle.getBuilding();
    Ogre::SceneNode* rootNode = TryGetBuildingRootNode(building);
    PhysicsCollection* physical = TryGetBuildingPhysical(building);
    size_t rootNodeCount = 0;
    size_t rootAttachedCount = 0;
    size_t rootEntityLikeCount = 0;
    size_t staticEntCount = 0;
    size_t partCount = 0;
    size_t instancedStaticEntCount = 0;
    size_t entityLikeStaticEntCount = 0;
    std::vector<std::string> rootSamples;
    std::vector<std::string> staticSamples;
    CollectSceneNodeMovableTypeSamples(
        rootNode,
        0,
        &rootNodeCount,
        &rootAttachedCount,
        &rootEntityLikeCount,
        &rootSamples);
    CollectPhysicalMovableTypeSamples(
        physical,
        &staticEntCount,
        &partCount,
        &instancedStaticEntCount,
        &entityLikeStaticEntCount,
        &staticSamples);

    std::stringstream summary;
    summary << "[investigate][tint_collect] target="
            << targetHandle.type << ":" << targetHandle.index << ":" << targetHandle.serial
            << " name=\"" << RootObjectDisplayNameForLog(static_cast<RootObject*>(building)) << "\""
            << " building_present=" << (building != 0 ? 1 : 0)
            << " building_class=" << SafeGetBuildingClassTypeName(building)
            << " building_function=" << SafeGetBuildingFunctionTypeName(building)
            << " created=" << SafeGetBuildingCreated(building)
            << " visible=" << SafeGetBuildingVisible(building)
            << " entities_loaded=" << SafeGetBuildingEntitiesLoaded(building)
            << " root_node=" << (rootNode != 0 ? 1 : 0)
            << " root_nodes=" << rootNodeCount
            << " root_attached=" << rootAttachedCount
            << " root_entity_like=" << rootEntityLikeCount
            << " physical=" << (physical != 0 ? 1 : 0)
            << " physical_loaded=" << SafeGetPhysicalLoaded(physical)
            << " static_ents=" << staticEntCount
            << " static_entity_like=" << entityLikeStaticEntCount
            << " static_instanced=" << instancedStaticEntCount
            << " parts=" << partCount;
    LogDebugLine(summary.str());

    if (!rootSamples.empty())
    {
        std::stringstream line;
        line << "[investigate][tint_collect] root_samples=";
        for (size_t i = 0; i < rootSamples.size(); ++i)
        {
            if (i != 0)
            {
                line << ",";
            }
            line << rootSamples[i];
        }
        LogDebugLine(line.str());
    }

    if (!staticSamples.empty())
    {
        std::stringstream line;
        line << "[investigate][tint_collect] static_samples=";
        for (size_t i = 0; i < staticSamples.size(); ++i)
        {
            if (i != 0)
            {
                line << ",";
            }
            line << staticSamples[i];
        }
        LogDebugLine(line.str());
    }
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

            AppendMovableAsTintEntityUnique(outEntities, movable);
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

void CollectBuildingInstanceBatchesRecursive(
    Ogre::SceneNode* node,
    size_t depth,
    std::vector<Ogre::InstanceBatch*>* outBatches)
{
    if (node == 0 || outBatches == 0 || depth > 10)
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

            AppendMovableAsTintBatchUnique(outBatches, movable);
        }

        const size_t childCount = node->numChildren();
        for (size_t childIndex = 0; childIndex < childCount; ++childIndex)
        {
            Ogre::SceneNode* childNode = static_cast<Ogre::SceneNode*>(node->getChild(childIndex));
            CollectBuildingInstanceBatchesRecursive(childNode, depth + 1, outBatches);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

PhysicsCollection* TryGetBuildingPhysical(Building* building)
{
    if (building == 0 || !building->isValid())
    {
        return 0;
    }

    __try
    {
        return building->physical;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

void CollectBuildingPhysicalEntities(Building* building, std::vector<Ogre::Entity*>* outEntities)
{
    if (building == 0 || outEntities == 0)
    {
        return;
    }

    PhysicsCollection* physical = TryGetBuildingPhysical(building);
    if (physical == 0)
    {
        return;
    }

    __try
    {
        for (lektor<PhysicsCollection::StaticEnt*>::const_iterator it = physical->staticEnts.begin();
             it != physical->staticEnts.end();
             ++it)
        {
            PhysicsCollection::StaticEnt* staticEnt = *it;
            if (staticEnt == 0)
            {
                continue;
            }

            AppendMovableAsTintEntityUnique(outEntities, staticEnt->ent);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

void CollectBuildingPhysicalInstanceBatches(Building* building, std::vector<Ogre::InstanceBatch*>* outBatches)
{
    if (building == 0 || outBatches == 0)
    {
        return;
    }

    PhysicsCollection* physical = TryGetBuildingPhysical(building);
    if (physical == 0)
    {
        return;
    }

    __try
    {
        for (lektor<PhysicsCollection::StaticEnt*>::const_iterator it = physical->staticEnts.begin();
             it != physical->staticEnts.end();
             ++it)
        {
            PhysicsCollection::StaticEnt* staticEnt = *it;
            if (staticEnt == 0)
            {
                continue;
            }

            AppendMovableAsTintBatchUnique(outBatches, staticEnt->ent);
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
        CollectBuildingPhysicalEntities(building, &entities);
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
    const auto addCandidate = [&candidates](const std::string& candidate)
    {
        if (!candidate.empty() && !StringListContains(candidates, candidate))
        {
            candidates.push_back(candidate);
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

    addCandidateWithSeparatorVariants(textureName);

    const bool hasPathSeparator = (textureName.find('/') != std::string::npos || textureName.find('\\') != std::string::npos);
    const bool isAbsolutePath = (textureName.size() >= 2 && textureName[1] == ':')
        || (!textureName.empty() && (textureName[0] == '/' || textureName[0] == '\\'));
    const bool isModQualified = (textureName.rfind("mods/", 0) == 0 || textureName.rfind("mods\\", 0) == 0);

    if (!hasPathSeparator)
    {
        addCandidateWithSeparatorVariants(std::string("gui/gfx/") + textureName);
        addCandidateWithSeparatorVariants(std::string("mods/") + kPluginName + "/gui/gfx/" + textureName);
        addCandidateWithSeparatorVariants(std::string("mods/") + kPluginName + "/" + textureName);
    }
    else if (!isAbsolutePath && !isModQualified)
    {
        addCandidateWithSeparatorVariants(std::string("mods/") + kPluginName + "/" + textureName);

        const size_t fileNameStart = textureName.find_last_of("/\\");
        if (fileNameStart != std::string::npos && (fileNameStart + 1) < textureName.size())
        {
            const std::string fileName = textureName.substr(fileNameStart + 1);
            addCandidateWithSeparatorVariants(fileName);
            addCandidateWithSeparatorVariants(std::string("gui/gfx/") + fileName);
            addCandidateWithSeparatorVariants(std::string("mods/") + kPluginName + "/gui/gfx/" + fileName);
            addCandidateWithSeparatorVariants(std::string("mods/") + kPluginName + "/" + fileName);
        }
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

    if (textureName != kDefaultMarkerIconTexture)
    {
        return TryApplyMarkerIconTexture(imageBox, kDefaultMarkerIconTexture);
    }

    return false;
}

void SetMarkerPosition(MarkerWidget& marker, int left, int top, bool showIcon, bool showText)
{
    int layoutLeft = left;
    int iconLeft = left;

    if (showIcon)
    {
        iconLeft = layoutLeft;
        layoutLeft += kMarkerHeightPx + kMarkerIconGapPx;
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
            int textWidth = kMarkerWidthPx - (layoutLeft - left);
            if (textWidth < 0)
            {
                textWidth = 0;
            }
            if (textWidth > kMarkerWidthPx)
            {
                textWidth = kMarkerWidthPx;
            }

            if (showText)
            {
                marker.text->setCoord(layoutLeft, top, textWidth, kMarkerHeightPx);
            }
            else
            {
                marker.text->setCoord(layoutLeft, top, 0, kMarkerHeightPx);
            }
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
            MyGUI::IntCoord(kMarkerHeightPx + kMarkerIconGapPx, 0, kMarkerWidthPx - (kMarkerHeightPx + kMarkerIconGapPx), kMarkerHeightPx),
            MyGUI::Align::Default,
            "Top",
            name.str());
        if (text == 0)
        {
            text = gui->createWidget<MyGUI::TextBox>(
                "TextBox",
                MyGUI::IntCoord(kMarkerHeightPx + kMarkerIconGapPx, 0, kMarkerWidthPx - (kMarkerHeightPx + kMarkerIconGapPx), kMarkerHeightPx),
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

std::string SafeGetPassVertexProgramName(Ogre::Pass* pass)
{
    if (!pass)
    {
        return std::string();
    }

    try
    {
        return pass->getVertexProgramName().c_str();
    }
    catch (...)
    {
        return std::string();
    }
}

std::string SafeGetPassFragmentProgramName(Ogre::Pass* pass)
{
    if (!pass)
    {
        return std::string();
    }

    try
    {
        return pass->getFragmentProgramName().c_str();
    }
    catch (...)
    {
        return std::string();
    }
}

bool PassUsesObjectLikePrograms(const std::string& vertexProgramNameLower, const std::string& fragmentProgramNameLower)
{
    if (vertexProgramNameLower.find("shadow") != std::string::npos
        || fragmentProgramNameLower.find("shadow") != std::string::npos)
    {
        return false;
    }

    return vertexProgramNameLower.empty()
        || fragmentProgramNameLower.empty()
        || vertexProgramNameLower.find("object") != std::string::npos
        || fragmentProgramNameLower.find("object") != std::string::npos
        || vertexProgramNameLower.find("building") != std::string::npos
        || fragmentProgramNameLower.find("building") != std::string::npos
        || vertexProgramNameLower.find("ch_object") != std::string::npos
        || fragmentProgramNameLower.find("ch_object") != std::string::npos;
}

const char* SelectHighlightVertexProgramName(
    const std::string& vertexProgramNameLower,
    const std::string& fragmentProgramNameLower)
{
    if (vertexProgramNameLower.find("coloured") != std::string::npos
        || fragmentProgramNameLower.find("coloured") != std::string::npos
        || fragmentProgramNameLower.find("dual") != std::string::npos)
    {
        return kHighlightObjectColouredVertexProgram;
    }

    return kHighlightObjectVertexProgram;
}

const char* SelectHighlightFragmentProgramName(
    const std::string& vertexProgramNameLower,
    const std::string& fragmentProgramNameLower)
{
    const bool usesColouring =
        vertexProgramNameLower.find("coloured") != std::string::npos
        || fragmentProgramNameLower.find("coloured") != std::string::npos;

    if (fragmentProgramNameLower.find("double") != std::string::npos)
    {
        return usesColouring
            ? kHighlightObjectDoubleSidedColouredFragmentProgram
            : kHighlightObjectDoubleSidedFragmentProgram;
    }

    if (fragmentProgramNameLower.find("dual") != std::string::npos)
    {
        return kHighlightObjectDualFragmentProgram;
    }

    if (fragmentProgramNameLower.find("emissive") != std::string::npos)
    {
        return kHighlightObjectEmissiveFragmentProgram;
    }

    if (fragmentProgramNameLower.find("alpha") != std::string::npos)
    {
        return kHighlightObjectAlphaFragmentProgram;
    }

    if (usesColouring)
    {
        return kHighlightObjectColouredFragmentProgram;
    }

    return kHighlightObjectFragmentProgram;
}

bool ApplyContainerHighlightProgramsToPass(Ogre::Pass* pass)
{
    if (!pass)
    {
        return false;
    }

    unsigned short textureUnitCount = 0;
    try
    {
        textureUnitCount = pass->getNumTextureUnitStates();
    }
    catch (...)
    {
        textureUnitCount = 0;
    }
    if (textureUnitCount == 0)
    {
        return false;
    }

    const std::string originalVertexProgramName = SafeGetPassVertexProgramName(pass);
    const std::string originalFragmentProgramName = SafeGetPassFragmentProgramName(pass);
    const std::string vertexProgramNameLower = ToLowerAsciiCopy(originalVertexProgramName);
    const std::string fragmentProgramNameLower = ToLowerAsciiCopy(originalFragmentProgramName);
    if (!PassUsesObjectLikePrograms(vertexProgramNameLower, fragmentProgramNameLower))
    {
        return false;
    }

    const char* highlightVertexProgram = SelectHighlightVertexProgramName(
        vertexProgramNameLower,
        fragmentProgramNameLower);
    const char* highlightFragmentProgram = SelectHighlightFragmentProgramName(
        vertexProgramNameLower,
        fragmentProgramNameLower);

    try
    {
        pass->setVertexProgram(highlightVertexProgram, true);
        pass->setFragmentProgram(highlightFragmentProgram, true);
        return true;
    }
    catch (...)
    {
        return false;
    }
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
        pass->setDiffuse(Ogre::ColourValue(colour.r, colour.g, colour.b, 1.0f));
        pass->setSelfIllumination(Ogre::ColourValue(colour.r * 0.75f, colour.g * 0.75f, colour.b * 0.75f, 1.0f));
        pass->setSpecular(Ogre::ColourValue(colour.r * 0.15f, colour.g * 0.15f, colour.b * 0.15f, 1.0f));
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

            ApplyContainerHighlightProgramsToPass(pass);

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

bool IsFiniteVector3(const Ogre::Vector3& value)
{
    return _finite(value.x) != 0 && _finite(value.y) != 0 && _finite(value.z) != 0;
}

Ogre::SceneManager* TryGetSceneManagerFromNode(Ogre::SceneNode* node)
{
    if (node == 0)
    {
        return 0;
    }

    __try
    {
        return node->getCreator();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

Ogre::SceneManager* TryResolveMovableSceneManager(Ogre::MovableObject* movable)
{
    if (movable == 0)
    {
        return 0;
    }

    __try
    {
        Ogre::SceneManager* manager = movable->_getManager();
        if (manager != 0)
        {
            return manager;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }

    __try
    {
        return TryGetSceneManagerFromNode(movable->getParentSceneNode());
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

bool TryGetMovableWorldBounds(
    Ogre::MovableObject* movable,
    Ogre::Vector3* minOut,
    Ogre::Vector3* maxOut,
    Ogre::SceneManager** sceneManagerOut)
{
    if (movable == 0 || minOut == 0 || maxOut == 0)
    {
        return false;
    }

    __try
    {
        const Ogre::Aabb aabb = movable->getWorldAabbUpdated();
        const Ogre::Vector3 minValue = aabb.mCenter - aabb.mHalfSize;
        const Ogre::Vector3 maxValue = aabb.mCenter + aabb.mHalfSize;
        if (!IsFiniteVector3(minValue) || !IsFiniteVector3(maxValue))
        {
            return false;
        }

        *minOut = minValue;
        *maxOut = maxValue;
        if (sceneManagerOut != 0 && *sceneManagerOut == 0)
        {
            *sceneManagerOut = TryResolveMovableSceneManager(movable);
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void MergeBounds(
    bool* hasBounds,
    Ogre::Vector3* minOut,
    Ogre::Vector3* maxOut,
    const Ogre::Vector3& candidateMin,
    const Ogre::Vector3& candidateMax)
{
    if (hasBounds == 0 || minOut == 0 || maxOut == 0)
    {
        return;
    }

    if (!*hasBounds)
    {
        *minOut = candidateMin;
        *maxOut = candidateMax;
        *hasBounds = true;
        return;
    }

    minOut->x = std::min(minOut->x, candidateMin.x);
    minOut->y = std::min(minOut->y, candidateMin.y);
    minOut->z = std::min(minOut->z, candidateMin.z);
    maxOut->x = std::max(maxOut->x, candidateMax.x);
    maxOut->y = std::max(maxOut->y, candidateMax.y);
    maxOut->z = std::max(maxOut->z, candidateMax.z);
}

void CollectSceneNodeTintOverlayBoundsRecursive(
    Ogre::SceneNode* node,
    size_t depth,
    bool* hasBounds,
    Ogre::Vector3* minOut,
    Ogre::Vector3* maxOut,
    Ogre::SceneManager** sceneManagerOut)
{
    if (node == 0 || depth > 10 || hasBounds == 0 || minOut == 0 || maxOut == 0)
    {
        return;
    }

    __try
    {
        if (sceneManagerOut != 0 && *sceneManagerOut == 0)
        {
            *sceneManagerOut = TryGetSceneManagerFromNode(node);
        }

        const size_t attachedCount = node->numAttachedObjects();
        for (size_t attachedIndex = 0; attachedIndex < attachedCount; ++attachedIndex)
        {
            Ogre::MovableObject* movable = node->getAttachedObject(attachedIndex);
            Ogre::Vector3 movableMin;
            Ogre::Vector3 movableMax;
            if (TryGetMovableWorldBounds(movable, &movableMin, &movableMax, sceneManagerOut))
            {
                MergeBounds(hasBounds, minOut, maxOut, movableMin, movableMax);
            }
        }

        const size_t childCount = node->numChildren();
        for (size_t childIndex = 0; childIndex < childCount; ++childIndex)
        {
            Ogre::SceneNode* childNode = static_cast<Ogre::SceneNode*>(node->getChild(childIndex));
            CollectSceneNodeTintOverlayBoundsRecursive(
                childNode,
                depth + 1,
                hasBounds,
                minOut,
                maxOut,
                sceneManagerOut);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

void CollectPhysicalTintOverlayBounds(
    Building* building,
    bool* hasBounds,
    Ogre::Vector3* minOut,
    Ogre::Vector3* maxOut,
    Ogre::SceneManager** sceneManagerOut)
{
    if (building == 0 || hasBounds == 0 || minOut == 0 || maxOut == 0)
    {
        return;
    }

    PhysicsCollection* physical = TryGetBuildingPhysical(building);
    if (physical == 0)
    {
        return;
    }

    __try
    {
        for (lektor<PhysicsCollection::StaticEnt*>::const_iterator it = physical->staticEnts.begin();
             it != physical->staticEnts.end();
             ++it)
        {
            PhysicsCollection::StaticEnt* staticEnt = *it;
            if (staticEnt == 0 || staticEnt->ent == 0)
            {
                continue;
            }

            Ogre::Vector3 movableMin;
            Ogre::Vector3 movableMax;
            if (TryGetMovableWorldBounds(staticEnt->ent, &movableMin, &movableMax, sceneManagerOut))
            {
                MergeBounds(hasBounds, minOut, maxOut, movableMin, movableMax);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

bool TryGetTintOverlayBoundsForTarget(
    const hand& targetHandle,
    Ogre::SceneManager** sceneManagerOut,
    Ogre::Vector3* centerOut,
    Ogre::Vector3* sizeOut)
{
    if (centerOut == 0 || sizeOut == 0)
    {
        return false;
    }

    Building* building = targetHandle.getBuilding();
    if (building == 0 || !IsHighlightableContainerBuilding(building))
    {
        return false;
    }

    bool hasBounds = false;
    Ogre::Vector3 minValue = Ogre::Vector3::ZERO;
    Ogre::Vector3 maxValue = Ogre::Vector3::ZERO;
    Ogre::SceneNode* rootNode = TryGetBuildingRootNode(building);
    if (sceneManagerOut != 0 && *sceneManagerOut == 0)
    {
        *sceneManagerOut = TryGetSceneManagerFromNode(rootNode);
    }
    CollectSceneNodeTintOverlayBoundsRecursive(rootNode, 0, &hasBounds, &minValue, &maxValue, sceneManagerOut);
    CollectPhysicalTintOverlayBounds(building, &hasBounds, &minValue, &maxValue, sceneManagerOut);
    if (!hasBounds)
    {
        return false;
    }

    Ogre::Vector3 size = maxValue - minValue;
    size.x = std::max(size.x + (kTintOverlayPaddingWorld * 2.0f), kTintOverlayMinExtentWorld);
    size.y = std::max(size.y + (kTintOverlayPaddingWorld * 2.0f), kTintOverlayMinExtentWorld);
    size.z = std::max(size.z + (kTintOverlayPaddingWorld * 2.0f), kTintOverlayMinExtentWorld);
    *centerOut = (minValue + maxValue) * 0.5f;
    *sizeOut = size;
    return true;
}

Ogre::MaterialPtr EnsureTintOverlayMaterial(
    const char* materialName,
    const Ogre::ColourValue& colour,
    bool additive,
    bool depthCheckEnabled,
    bool depthWriteEnabled,
    Ogre::PolygonMode polygonMode,
    float depthBias)
{
    if (materialName == 0)
    {
        return Ogre::MaterialPtr();
    }

    Ogre::MaterialPtr material = Ogre::MaterialManager::getSingleton().getByName(materialName);
    if (material.isNull())
    {
        try
        {
            material = Ogre::MaterialManager::getSingleton().create(
                materialName,
                Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        }
        catch (...)
        {
            material.setNull();
        }
    }
    if (material.isNull())
    {
        return material;
    }

    try
    {
        material->removeAllTechniques();
        Ogre::Technique* technique = material->createTechnique();
        Ogre::Pass* pass = technique->createPass();
        pass->setLightingEnabled(false);
        pass->setAmbient(Ogre::ColourValue(colour.r, colour.g, colour.b, 1.0f));
        pass->setDiffuse(colour);
        pass->setSelfIllumination(Ogre::ColourValue(colour.r, colour.g, colour.b, 1.0f));
        pass->setSceneBlending(additive ? Ogre::SBT_ADD : Ogre::SBT_TRANSPARENT_ALPHA);
        pass->setDepthWriteEnabled(depthWriteEnabled);
        pass->setDepthCheckEnabled(depthCheckEnabled);
        pass->setCullingMode(Ogre::CULL_NONE);
        pass->setPolygonMode(polygonMode);
        pass->setDepthBias(depthBias, depthBias);
        material->setReceiveShadows(false);
        material->load();
    }
    catch (...)
    {
    }

    return material;
}

bool EnsureTintOverlayMaterials()
{
    const Ogre::ColourValue fillColour(1.0f, 0.0f, 1.0f, 1.0f);
    const Ogre::MaterialPtr fillMaterial = EnsureTintOverlayMaterial(
        kTintOverlayFillMaterialName,
        fillColour,
        false,
        false,
        false,
        Ogre::PM_SOLID,
        0.0f);
    if (fillMaterial.isNull())
    {
        return false;
    }

    const Ogre::ColourValue outlineColour(0.0f, 1.0f, 1.0f, 1.0f);
    const Ogre::MaterialPtr outlineMaterial = EnsureTintOverlayMaterial(
        kTintOverlayOutlineMaterialName,
        outlineColour,
        true,
        false,
        false,
        Ogre::PM_SOLID,
        0.0f);
    if (outlineMaterial.isNull())
    {
        return false;
    }

    const Ogre::ColourValue depthOutlineColour(0.2f, 1.0f, 0.1f, 1.0f);
    const Ogre::MaterialPtr depthOutlineMaterial = EnsureTintOverlayMaterial(
        kTintOverlayDepthOutlineMaterialName,
        depthOutlineColour,
        true,
        true,
        false,
        Ogre::PM_SOLID,
        2.0f);
    return !depthOutlineMaterial.isNull();
}

void BuildTintOverlayGeometry(Ogre::ManualObject* manualObject)
{
    if (manualObject == 0)
    {
        return;
    }

    manualObject->clear();
    manualObject->setDynamic(true);
    manualObject->estimateVertexCount(24);
    manualObject->estimateIndexCount(84);
    manualObject->begin(kTintOverlayFillMaterialName, Ogre::RenderOperation::OT_TRIANGLE_LIST);

    const Ogre::Vector3 vertices[8] = {
        Ogre::Vector3(-0.5f, -0.5f, -0.5f),
        Ogre::Vector3( 0.5f, -0.5f, -0.5f),
        Ogre::Vector3( 0.5f,  0.5f, -0.5f),
        Ogre::Vector3(-0.5f,  0.5f, -0.5f),
        Ogre::Vector3(-0.5f, -0.5f,  0.5f),
        Ogre::Vector3( 0.5f, -0.5f,  0.5f),
        Ogre::Vector3( 0.5f,  0.5f,  0.5f),
        Ogre::Vector3(-0.5f,  0.5f,  0.5f)
    };
    for (size_t i = 0; i < 8; ++i)
    {
        manualObject->position(vertices[i]);
    }

    const unsigned int indices[] = {
        0, 1, 2, 0, 2, 3,
        4, 6, 5, 4, 7, 6,
        0, 4, 5, 0, 5, 1,
        3, 2, 6, 3, 6, 7,
        1, 5, 6, 1, 6, 2,
        0, 3, 7, 0, 7, 4
    };
    for (size_t i = 0; i < (sizeof(indices) / sizeof(indices[0])); ++i)
    {
        manualObject->index(indices[i]);
    }

    manualObject->end();
    manualObject->begin(kTintOverlayOutlineMaterialName, Ogre::RenderOperation::OT_LINE_LIST);
    for (size_t i = 0; i < 8; ++i)
    {
        manualObject->position(vertices[i]);
    }

    const unsigned int lineIndices[] = {
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7
    };
    for (size_t i = 0; i < (sizeof(lineIndices) / sizeof(lineIndices[0])); ++i)
    {
        manualObject->index(lineIndices[i]);
    }

    manualObject->end();
    manualObject->begin(kTintOverlayDepthOutlineMaterialName, Ogre::RenderOperation::OT_LINE_LIST);
    for (size_t i = 0; i < 8; ++i)
    {
        manualObject->position(vertices[i]);
    }

    for (size_t i = 0; i < (sizeof(lineIndices) / sizeof(lineIndices[0])); ++i)
    {
        manualObject->index(lineIndices[i]);
    }

    manualObject->end();
    manualObject->setRenderQueueGroup(kTintOverlayRenderQueueGroup);
}

void ClearTintOverlayBinding(TintOverlayBinding* binding)
{
    if (binding == 0)
    {
        return;
    }

    if (binding->sceneNode != 0)
    {
        try
        {
            binding->sceneNode->detachAllObjects();
        }
        catch (...)
        {
        }
    }

    if (binding->sceneManager != 0 && binding->manualObject != 0)
    {
        try
        {
            binding->sceneManager->destroyManualObject(binding->manualObject);
        }
        catch (...)
        {
        }
    }

    if (binding->sceneManager != 0 && binding->sceneNode != 0)
    {
        try
        {
            binding->sceneManager->destroySceneNode(binding->sceneNode);
        }
        catch (...)
        {
        }
    }

    binding->sceneManager = 0;
    binding->sceneNode = 0;
    binding->manualObject = 0;
}

bool ApplyTintOverlayBinding(
    Ogre::SceneManager* sceneManager,
    const Ogre::Vector3& center,
    const Ogre::Vector3& size,
    TintOverlayBinding* binding,
    const char** outcomeOut)
{
    if (outcomeOut != 0)
    {
        *outcomeOut = "unknown";
    }

    if (sceneManager == 0 || binding == 0)
    {
        if (outcomeOut != 0)
        {
            *outcomeOut = "scene_manager_null";
        }
        return false;
    }

    if (!EnsureTintOverlayMaterials())
    {
        if (outcomeOut != 0)
        {
            *outcomeOut = "material_missing";
        }
        return false;
    }

    if (binding->sceneManager != sceneManager || binding->sceneNode == 0 || binding->manualObject == 0)
    {
        ClearTintOverlayBinding(binding);

        try
        {
            binding->sceneManager = sceneManager;
            binding->manualObject = sceneManager->createManualObject(Ogre::SCENE_DYNAMIC);
            binding->manualObject->setCastShadows(false);
            binding->manualObject->setRenderingDistance(0.0f);
            binding->manualObject->setQueryFlags(0);
            BuildTintOverlayGeometry(binding->manualObject);
            binding->sceneNode = sceneManager->getRootSceneNode(Ogre::SCENE_DYNAMIC)->createChildSceneNode(Ogre::SCENE_DYNAMIC);
            binding->sceneNode->attachObject(binding->manualObject);
        }
        catch (...)
        {
            ClearTintOverlayBinding(binding);
            if (outcomeOut != 0)
            {
                *outcomeOut = "create_failed";
            }
            return false;
        }
    }

    try
    {
        Ogre::Vector3 diagnosticSize = size * kTintOverlayDiagnosticScaleMultiplier;
        diagnosticSize.x = std::max(diagnosticSize.x, 4.0f);
        diagnosticSize.y = std::max(diagnosticSize.y, 4.0f);
        diagnosticSize.z = std::max(diagnosticSize.z, 4.0f);
        Ogre::Vector3 diagnosticCenter = center;
        diagnosticCenter.y += (size.y * kTintOverlayDiagnosticYOffsetMultiplier) + 2.0f;
        binding->sceneNode->setPosition(diagnosticCenter);
        binding->sceneNode->setScale(diagnosticSize);
        binding->manualObject->setVisible(true);
        if (outcomeOut != 0)
        {
            *outcomeOut = "applied";
        }
        return true;
    }
    catch (...)
    {
        ClearTintOverlayBinding(binding);
        if (outcomeOut != 0)
        {
            *outcomeOut = "apply_failed";
        }
        return false;
    }
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

Ogre::MaterialPtr GetInstanceBatchMaterial(Ogre::InstanceBatch* batch)
{
    if (batch == 0)
    {
        return Ogre::MaterialPtr();
    }

    try
    {
        return batch->getMaterial();
    }
    catch (...)
    {
        return Ogre::MaterialPtr();
    }
}

bool SetInstanceBatchMaterial(Ogre::InstanceBatch* batch, const Ogre::MaterialPtr& material)
{
    if (batch == 0 || material.isNull())
    {
        return false;
    }

    try
    {
        Ogre::MaterialPtr& materialSlot = const_cast<Ogre::MaterialPtr&>(batch->getMaterial());
        materialSlot = material;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void ClearTintBatchBinding(TintBatchBinding* binding)
{
    if (binding == 0)
    {
        return;
    }

    if (binding->batch != 0 && !binding->originalMaterial.isNull())
    {
        SetInstanceBatchMaterial(binding->batch, binding->originalMaterial);
    }

    binding->batch = 0;
    binding->originalMaterial.setNull();
    binding->cloneMaterial.setNull();
}

void ClearTintEntry(ContainerTintEntry* entry)
{
    if (entry == 0)
    {
        return;
    }

    for (size_t i = 0; i < entry->entityBindings.size(); ++i)
    {
        ClearTintBinding(&entry->entityBindings[i]);
    }
    entry->entityBindings.clear();

    for (size_t i = 0; i < entry->batchBindings.size(); ++i)
    {
        ClearTintBatchBinding(&entry->batchBindings[i]);
    }
    entry->batchBindings.clear();

    ClearTintOverlayBinding(&entry->overlayBinding);
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

void CollectTintTargetsForTarget(
    const hand& targetHandle,
    std::vector<Ogre::Entity*>* outEntities,
    std::vector<Ogre::InstanceBatch*>* outBatches)
{
    if (outEntities == 0 || outBatches == 0)
    {
        return;
    }

    outEntities->clear();
    outBatches->clear();

    Building* building = targetHandle.getBuilding();
    if (building != 0 && IsHighlightableContainerBuilding(building))
    {
        Ogre::SceneNode* rootNode = TryGetBuildingRootNode(building);
        CollectBuildingEntitiesRecursive(rootNode, 0, outEntities);
        CollectBuildingPhysicalEntities(building, outEntities);
        if (kEnableInstancedBatchTint)
        {
            CollectBuildingInstanceBatchesRecursive(rootNode, 0, outBatches);
            CollectBuildingPhysicalInstanceBatches(building, outBatches);
        }
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

bool ApplyTintToBatchBinding(const hand& targetHandle, size_t bindingIndex, Ogre::InstanceBatch* batch, TintBatchBinding* binding)
{
    if (batch == 0 || binding == 0)
    {
        return false;
    }

    if (binding->batch != batch)
    {
        ClearTintBatchBinding(binding);
        binding->batch = batch;
    }

    Ogre::MaterialPtr currentMaterial = GetInstanceBatchMaterial(batch);
    if (currentMaterial.isNull())
    {
        return false;
    }

    const bool currentIsClone =
        !binding->cloneMaterial.isNull() && MaterialPtrsReferSameObject(currentMaterial, binding->cloneMaterial);
    if (!currentIsClone)
    {
        binding->originalMaterial = currentMaterial;
        binding->cloneMaterial.setNull();
    }
    if (binding->originalMaterial.isNull())
    {
        binding->originalMaterial = currentMaterial;
    }

    if (binding->cloneMaterial.isNull())
    {
        try
        {
            binding->cloneMaterial = binding->originalMaterial->clone(
                BuildTintCloneName(targetHandle, bindingIndex, 0));
        }
        catch (...)
        {
            binding->cloneMaterial.setNull();
        }
    }
    if (binding->cloneMaterial.isNull())
    {
        return false;
    }

    if (!ApplyTintToMaterialClone(binding->cloneMaterial, g_state.config.tintColour, g_state.config.tintForceDepthOverride))
    {
        return false;
    }

    return SetInstanceBatchMaterial(batch, binding->cloneMaterial);
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

    const bool emitTintInvestigation = ShouldEmitTintInvestigation();
    std::vector<hand> activeHandles;
    for (size_t targetIndex = 0; targetIndex < g_state.targetCache.size(); ++targetIndex)
    {
        const hand targetHandle = g_state.targetCache[targetIndex].targetHandle;
        std::vector<Ogre::Entity*> entities;
        std::vector<Ogre::InstanceBatch*> batches;
        CollectTintTargetsForTarget(targetHandle, &entities, &batches);
        Ogre::SceneManager* overlaySceneManager = 0;
        Ogre::Vector3 overlayCenter = Ogre::Vector3::ZERO;
        Ogre::Vector3 overlaySize = Ogre::Vector3::ZERO;
        const bool hasOverlayBounds = TryGetTintOverlayBoundsForTarget(
            targetHandle,
            &overlaySceneManager,
            &overlayCenter,
            &overlaySize);
        if (entities.empty() && batches.empty() && !hasOverlayBounds)
        {
            continue;
        }

        int tintEntryIndex = FindTintEntryByHandle(targetHandle);
        if (tintEntryIndex < 0)
        {
            ContainerTintEntry entry;
            entry.targetHandle = targetHandle;
            g_state.tintEntries.push_back(entry);
            tintEntryIndex = static_cast<int>(g_state.tintEntries.size() - 1);
        }

        ContainerTintEntry& tintEntry = g_state.tintEntries[static_cast<size_t>(tintEntryIndex)];
        while (tintEntry.entityBindings.size() > entities.size())
        {
            ClearTintBinding(&tintEntry.entityBindings.back());
            tintEntry.entityBindings.pop_back();
        }
        while (tintEntry.entityBindings.size() < entities.size())
        {
            TintEntityBinding binding;
            binding.entity = 0;
            tintEntry.entityBindings.push_back(binding);
        }

        while (tintEntry.batchBindings.size() > batches.size())
        {
            ClearTintBatchBinding(&tintEntry.batchBindings.back());
            tintEntry.batchBindings.pop_back();
        }
        while (tintEntry.batchBindings.size() < batches.size())
        {
            TintBatchBinding binding;
            binding.batch = 0;
            tintEntry.batchBindings.push_back(binding);
        }

        bool appliedAny = false;
        for (size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex)
        {
            if (ApplyTintToEntityBinding(
                tintEntry.targetHandle,
                entityIndex,
                entities[entityIndex],
                &tintEntry.entityBindings[entityIndex]))
            {
                appliedAny = true;
            }
        }

        for (size_t batchIndex = 0; batchIndex < batches.size(); ++batchIndex)
        {
            if (ApplyTintToBatchBinding(
                tintEntry.targetHandle,
                entities.size() + batchIndex,
                batches[batchIndex],
                &tintEntry.batchBindings[batchIndex]))
            {
                appliedAny = true;
            }
        }

        bool overlayApplied = false;
        const char* overlayOutcome = "not_attempted";
        if (!appliedAny && hasOverlayBounds)
        {
            overlayApplied = ApplyTintOverlayBinding(
                overlaySceneManager,
                overlayCenter,
                overlaySize,
                &tintEntry.overlayBinding,
                &overlayOutcome);
        }
        else
        {
            ClearTintOverlayBinding(&tintEntry.overlayBinding);
            overlayOutcome = appliedAny ? "skipped_direct_tint" : "no_bounds";
        }

        if (appliedAny || overlayApplied)
        {
            activeHandles.push_back(tintEntry.targetHandle);
        }

        if (emitTintInvestigation)
        {
            std::stringstream line;
            line << "[investigate][tint_overlay] target="
                 << targetHandle.type << ":" << targetHandle.index << ":" << targetHandle.serial
                 << " name=\"" << RootObjectDisplayNameForLog(static_cast<RootObject*>(targetHandle.getBuilding())) << "\""
                 << " entities=" << entities.size()
                 << " batches=" << batches.size()
                 << " direct=" << (appliedAny ? 1 : 0)
                 << " bounds=" << (hasOverlayBounds ? 1 : 0)
                 << " scene_manager=" << (overlaySceneManager != 0 ? 1 : 0)
                 << " overlay=" << (overlayApplied ? 1 : 0)
                 << " overlay_result=" << overlayOutcome
                 << " node=" << (tintEntry.overlayBinding.sceneNode != 0 ? 1 : 0)
                 << " manual=" << (tintEntry.overlayBinding.manualObject != 0 ? 1 : 0);
            if (hasOverlayBounds)
            {
                line << " center=("
                     << overlayCenter.x << "," << overlayCenter.y << "," << overlayCenter.z << ")"
                     << " size=("
                     << overlaySize.x << "," << overlaySize.y << "," << overlaySize.z << ")";
            }
            LogDebugLine(line.str());
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
    g_state.lastTintDebugLogTickMs = 0;
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

    if (g_gameWorldMainLoopGPUSensitiveStuffOrig)
    {
        g_gameWorldMainLoopGPUSensitiveStuffOrig(thisptr, time);
    }

    if (g_state.highlightRuntimeActive)
    {
        SyncTint();
    }
    else
    {
        ClearAllTint();
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
