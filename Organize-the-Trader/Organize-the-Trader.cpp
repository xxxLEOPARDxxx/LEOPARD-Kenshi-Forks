#include <Debug.h>

#include <core/Functions.h>
#include <kenshi/GameData.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/InputHandler.h>
#include <kenshi/Inventory.h>
#include <kenshi/Item.h>
#include <kenshi/Kenshi.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/RootObject.h>
#include <kenshi/Character.h>
#include <kenshi/Dialogue.h>
#include <kenshi/Building.h>

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_ComboBox.h>
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
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// Minimal ownership interface stub to avoid including Platoon.h
// (it conflicts with Building.h enum declarations in this SDK drop).
class Ownerships
{
public:
    void getHomeFurnitureOfType(lektor<Building*>& out, BuildingFunction type);
    void getBuildingsWithFunction(lektor<Building*>& out, BuildingFunction bf);
};

class InventoryGUI;
class InventoryIcon;
class InventoryLayout;
class InventorySectionGUI
{
public:
    MyGUI::Widget* _widget;
    Ogre::vector<InventoryIcon*>::type _icons;
};

namespace
{
enum SearchFocusHotkeyKind
{
    SearchFocusHotkeyKind_None = 0,
    SearchFocusHotkeyKind_Slash,
    SearchFocusHotkeyKind_CtrlF,
};

const char* kPluginName = "Organize-the-Trader";
const char* kControlsContainerName = "OTT_TraderControlsContainer";
const char* kSearchEditName = "OTT_SearchEdit";
const char* kSearchPlaceholderName = "OTT_SearchPlaceholder";
const char* kSearchClearButtonName = "OTT_SearchClearButton";
const char* kSearchDragHandleName = "OTT_SearchDragHandle";
const char* kSearchCountTextName = "OTT_SearchCountText";
const char* kToggleHotkeyHint = "Ctrl+Shift+F8";
const char* kDiagnosticsHotkeyHint = "Ctrl+Shift+F9";

typedef void (*PlayerInterfaceUpdateUTFn)(PlayerInterface*);
PlayerInterfaceUpdateUTFn g_updateUTOrig = 0;
typedef void (*InventoryRefreshGuiFn)(Inventory*);
InventoryRefreshGuiFn g_inventoryRefreshGuiOrig = 0;
typedef InventoryLayout* (*CharacterCreateInventoryLayoutFn)(Character*);
CharacterCreateInventoryLayoutFn g_characterCreateInventoryLayoutOrig = 0;
typedef InventoryLayout* (*RootObjectCreateInventoryLayoutFn)(RootObject*);
RootObjectCreateInventoryLayoutFn g_rootObjectCreateInventoryLayoutOrig = 0;
typedef void (*InventoryLayoutCreateGUIFn)(
    void*,
    InventoryGUI*,
    Ogre::map<std::string, InventorySectionGUI*>::type&,
    Inventory*);
InventoryLayoutCreateGUIFn g_inventoryLayoutCreateGUIOrig = 0;
bool g_inventoryLayoutCreateGUIHookInstalled = false;
bool g_inventoryLayoutCreateGUIHookAttempted = false;
std::uintptr_t g_expectedInventoryLayoutCreateGUIAddress = 0;
bool g_inventoryLayoutCreateGUIEarlyAttempted = false;
std::size_t g_inventoryLayoutCreateGUIHookCallCount = 0;
std::size_t g_inventoryLayoutCreateInventoryLayoutLogCount = 0;
std::string g_lastInventoryLayoutReturnSignature;

unsigned long long g_updateTickCounter = 0;

bool g_prevToggleHotkeyDown = false;
bool g_prevDiagnosticsHotkeyDown = false;
bool g_prevSearchSlashHotkeyDown = false;
bool g_prevSearchCtrlFHotkeyDown = false;
bool g_controlsWereInjected = false;
bool g_controlsEnabled = true;
bool g_showSearchEntryCount = true;
bool g_showSearchQuantityCount = true;
int g_searchInputConfiguredWidth = 372;
int g_searchInputConfiguredHeight = 26;
bool g_suppressNextSearchEditChangeEvent = false;
bool g_pendingSlashFocusTextSuppression = false;
bool g_loggedNoVisibleTraderTarget = false;
bool g_loggedRejectedTraderCandidate = false;
std::string g_searchQueryRaw;
std::string g_searchQueryNormalized;
std::string g_pendingSlashFocusBaseQuery;
std::string g_activeTraderTargetId;
bool g_focusSearchEditOnNextInjection = false;
bool g_searchContainerDragging = false;
bool g_searchContainerPositionCustomized = false;
int g_searchContainerDragLastMouseX = 0;
int g_searchContainerDragLastMouseY = 0;
int g_searchContainerStoredLeft = 0;
int g_searchContainerStoredTop = 0;
bool g_loggedMissingBackpackForSearch = false;
bool g_loggedMissingSearchableItemText = false;
std::string g_lastZeroMatchQueryLogged;
bool g_loggedNumericOnlyQueryIgnored = false;
bool g_loggedInventoryBindingFailure = false;
bool g_loggedInventoryBindingDiagnostics = false;
bool g_loggedWidgetInventoryCandidatesMissing = false;
std::string g_lastInventoryKeysetSelectionSignature;
std::string g_lastInventoryKeysetLowCoverageSignature;
std::string g_lastBackpackResolutionSignature;
std::string g_lastZeroMatchGuardSignature;
std::string g_lastCoverageFallbackDecisionSignature;
std::string g_lastSearchSampleQueryLogged;
Inventory* g_cachedHoveredWidgetInventory = 0;
std::string g_cachedHoveredWidgetInventorySignature;
std::string g_lastSectionWidgetBindingSignature;
MyGUI::Widget* g_lockedKeysetTraderParent = 0;
std::string g_lockedKeysetStage;
std::string g_lockedKeysetSourceId;
std::string g_lockedKeysetSourcePreview;
std::size_t g_lockedKeysetExpectedCount = 0;
std::string g_lastKeysetLockSignature;
std::size_t g_lastSearchVisibleEntryCount = 0;
std::size_t g_lastSearchTotalEntryCount = 0;
std::size_t g_lastSearchVisibleQuantity = 0;

struct SectionWidgetInventoryLink
{
    MyGUI::Widget* sectionWidget;
    Inventory* inventory;
    std::string sectionName;
    std::string widgetName;
    std::size_t itemCount;
    unsigned long long lastSeenTick;
};

std::vector<SectionWidgetInventoryLink> g_sectionWidgetInventoryLinks;
std::string g_lastInventoryGuiBindingSignature;

struct InventoryGuiInventoryLink
{
    InventoryGUI* inventoryGui;
    Inventory* inventory;
    std::string ownerName;
    std::size_t itemCount;
    unsigned long long lastSeenTick;
};

std::vector<InventoryGuiInventoryLink> g_inventoryGuiInventoryLinks;
enum InventoryGuiBackPointerKind
{
    InventoryGuiBackPointerKind_DirectInventory,
    InventoryGuiBackPointerKind_OwnerObject,
    InventoryGuiBackPointerKind_CallbackObject,
};

struct InventoryGuiBackPointerOffset
{
    std::size_t offset;
    InventoryGuiBackPointerKind kind;
    std::size_t hits;
};

std::vector<InventoryGuiBackPointerOffset> g_inventoryGuiBackPointerOffsets;
std::string g_lastInventoryGuiBackPointerLearningSignature;
std::string g_lastInventoryGuiBackPointerResolutionSignature;
std::string g_lastInventoryGuiBackPointerResolutionFailureSignature;

struct TraderPanelInventoryBinding
{
    MyGUI::Widget* traderParent;
    MyGUI::Widget* entriesRoot;
    Inventory* inventory;
    std::string stage;
    std::string source;
    std::size_t expectedEntryCount;
    std::size_t nonEmptyKeyCount;
    unsigned long long lastSeenTick;
};

std::vector<TraderPanelInventoryBinding> g_traderPanelInventoryBindings;
std::string g_lastPanelBindingSignature;
std::string g_lastPanelBindingRefusedSignature;
std::string g_lastPanelBindingProbeSignature;

struct RefreshedInventoryLink
{
    Inventory* inventory;
    std::size_t itemCount;
    bool visible;
    bool ownerTrader;
    bool ownerSelected;
    std::string ownerName;
    unsigned long long lastSeenTick;
};

std::vector<RefreshedInventoryLink> g_recentRefreshedInventories;
std::string g_lastRefreshInventorySummarySignature;

MyGUI::Widget* FindBestWindowAnchor(MyGUI::Widget* fromWidget);
MyGUI::Widget* ResolveInjectionParent(MyGUI::Widget* anchor);
MyGUI::Widget* ResolveInventoryEntriesRoot(MyGUI::Widget* backpackContent);
MyGUI::Widget* ResolveBestBackpackContentWidget(MyGUI::Widget* traderParent, bool logDiagnostics);
MyGUI::Widget* FindNamedDescendantRecursive(MyGUI::Widget* root, const char* widgetName, bool requireVisible);
MyGUI::EditBox* FindSearchEditBox();
MyGUI::TextBox* FindSearchPlaceholderTextBox();
MyGUI::Button* FindSearchClearButton();
MyGUI::TextBox* FindSearchCountTextBox();
bool TryResolveHoveredTarget(MyGUI::Widget** outAnchor, MyGUI::Widget** outParent, bool logFailures);
bool IsInventoryPointerValidSafe(Inventory* inventory);
void PruneSectionWidgetInventoryLinks();
void PruneInventoryGuiInventoryLinks();
bool TryExtractSearchKeysFromInventory(Inventory* inventory, std::vector<std::string>* outKeys);
std::string BuildKeyPreviewForLog(const std::vector<std::string>& keys, std::size_t maxKeys);
void ApplySearchFilterFromControls(bool forceShowAll, bool logSummary);
void UpdateSearchCountText(
    std::size_t visibleEntryCount,
    std::size_t totalEntryCount,
    std::size_t visibleQuantity);
void UpdateSearchUiState();
bool TryResolveCaptionMatchedTraderCharacter(
    MyGUI::Widget* traderParent,
    Character** outCharacter,
    int* outCaptionScore);
bool TryResolvePreferredDialogueTraderTarget(
    Character** outTarget,
    Character** outSpeaker,
    std::string* outReason);
std::string CharacterNameForLog(Character* character);
std::size_t InventoryItemCountForLog(Inventory* inventory);

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
    DebugLog(line.str().c_str());
}

void LogErrorLine(const std::string& message)
{
    std::stringstream line;
    line << kPluginName << " ERROR: " << message;
    ErrorLog(line.str().c_str());
}

std::string GetCurrentPluginDirectoryPath()
{
    HMODULE module = 0;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&GetCurrentPluginDirectoryPath),
            &module)
        || module == 0)
    {
        return "";
    }

    char buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameA(module, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return "";
    }

    std::string path(buffer, static_cast<std::size_t>(length));
    const std::string::size_type slash = path.find_last_of("\\/");
    if (slash == std::string::npos)
    {
        return "";
    }

    return path.substr(0, slash);
}

bool TryReadTextFile(const std::string& path, std::string* outContent)
{
    if (outContent == 0 || path.empty())
    {
        return false;
    }

    std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
    if (!input.is_open())
    {
        return false;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    *outContent = buffer.str();
    return true;
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

bool TryParseJsonIntByKey(const std::string& content, const char* key, int* outValue)
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

    const std::string::size_type numberStart = valuePos;
    if (valuePos < content.size() && (content[valuePos] == '-' || content[valuePos] == '+'))
    {
        ++valuePos;
    }

    const std::string::size_type digitsStart = valuePos;
    while (valuePos < content.size()
        && std::isdigit(static_cast<unsigned char>(content[valuePos])) != 0)
    {
        ++valuePos;
    }

    if (digitsStart == valuePos)
    {
        return false;
    }

    int parsedValue = 0;
    std::stringstream parser(content.substr(numberStart, valuePos - numberStart));
    parser >> parsedValue;
    if (parser.fail())
    {
        return false;
    }

    *outValue = parsedValue;
    return true;
}

int ClampIntValue(int value, int minValue, int maxValue)
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

int ClampSearchInputConfiguredWidth(int value)
{
    return ClampIntValue(value, 120, 720);
}

int ClampSearchInputConfiguredHeight(int value)
{
    return ClampIntValue(value, 22, 48);
}

void LoadModConfig()
{
    g_controlsEnabled = true;
    g_showSearchEntryCount = true;
    g_showSearchQuantityCount = true;
    g_searchInputConfiguredWidth = 372;
    g_searchInputConfiguredHeight = 26;

    const std::string pluginDirectory = GetCurrentPluginDirectoryPath();
    if (pluginDirectory.empty())
    {
        LogWarnLine("mod config load skipped: could not resolve plugin directory");
        return;
    }

    const std::string configPath = pluginDirectory + "\\mod-config.json";
    std::string configText;
    if (!TryReadTextFile(configPath, &configText))
    {
        std::stringstream line;
        line << "mod config load skipped: could not read " << configPath
             << " (using defaults)";
        LogWarnLine(line.str());
        return;
    }

    bool parsedValue = false;
    if (TryParseJsonBoolByKey(configText, "enabled", &parsedValue))
    {
        g_controlsEnabled = parsedValue;
    }
    if (TryParseJsonBoolByKey(configText, "showSearchEntryCount", &parsedValue))
    {
        g_showSearchEntryCount = parsedValue;
    }
    if (TryParseJsonBoolByKey(configText, "showSearchQuantityCount", &parsedValue))
    {
        g_showSearchQuantityCount = parsedValue;
    }

    int parsedIntValue = 0;
    if (TryParseJsonIntByKey(configText, "searchInputWidth", &parsedIntValue))
    {
        g_searchInputConfiguredWidth = ClampSearchInputConfiguredWidth(parsedIntValue);
    }
    if (TryParseJsonIntByKey(configText, "searchInputHeight", &parsedIntValue))
    {
        g_searchInputConfiguredHeight = ClampSearchInputConfiguredHeight(parsedIntValue);
    }

    std::stringstream line;
    line << "mod config loaded"
         << " enabled=" << (g_controlsEnabled ? "true" : "false")
         << " showSearchEntryCount=" << (g_showSearchEntryCount ? "true" : "false")
         << " showSearchQuantityCount=" << (g_showSearchQuantityCount ? "true" : "false")
         << " searchInputWidth=" << g_searchInputConfiguredWidth
         << " searchInputHeight=" << g_searchInputConfiguredHeight;
    LogInfoLine(line.str());
}

std::string SafeWidgetName(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "<null>";
    }

    const std::string& name = widget->getName();
    if (name.empty())
    {
        return "<unnamed>";
    }
    return name;
}

std::string BuildParentChainForLog(MyGUI::Widget* widget)
{
    std::stringstream out;
    std::size_t depth = 0;
    while (widget != 0 && depth < 12)
    {
        if (depth > 0)
        {
            out << " <- ";
        }
        out << SafeWidgetName(widget);
        widget = widget->getParent();
        ++depth;
    }

    if (widget != 0)
    {
        out << " <- ...";
    }

    return out.str();
}

bool NameMatchesToken(const std::string& name, const char* token)
{
    if (token == 0 || *token == '\0' || name.empty())
    {
        return false;
    }

    const std::string tokenValue(token);
    if (name == tokenValue)
    {
        return true;
    }

    if (name.size() <= tokenValue.size() + 1)
    {
        return false;
    }

    const std::size_t offset = name.size() - tokenValue.size();
    if (name[offset - 1] != '_')
    {
        return false;
    }

    return name.compare(offset, tokenValue.size(), tokenValue) == 0;
}

std::string UpperAsciiCopy(const std::string& value)
{
    std::string upper = value;
    for (std::size_t index = 0; index < upper.size(); ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(upper[index]);
        upper[index] = static_cast<char>(std::toupper(ch));
    }
    return upper;
}

bool ContainsAsciiCaseInsensitive(const std::string& haystack, const char* needle)
{
    if (needle == 0 || *needle == '\0')
    {
        return false;
    }

    const std::string haystackUpper = UpperAsciiCopy(haystack);
    const std::string needleUpper = UpperAsciiCopy(std::string(needle));
    return haystackUpper.find(needleUpper) != std::string::npos;
}

int ExtractTaggedIntValue(const std::string& text, const char* tag)
{
    if (tag == 0 || *tag == '\0')
    {
        return -1;
    }

    const std::string token(tag);
    const std::size_t begin = text.find(token);
    if (begin == std::string::npos)
    {
        return -1;
    }

    std::size_t cursor = begin + token.size();
    int value = 0;
    bool hasDigit = false;
    while (cursor < text.size())
    {
        const unsigned char ch = static_cast<unsigned char>(text[cursor]);
        if (std::isdigit(ch) == 0)
        {
            break;
        }
        value = value * 10 + static_cast<int>(ch - '0');
        hasDigit = true;
        ++cursor;
    }

    return hasDigit ? value : -1;
}

bool TryExtractTaggedFraction(
    const std::string& text,
    const char* tag,
    int* outNumerator,
    int* outDenominator)
{
    if (outNumerator == 0 || outDenominator == 0 || tag == 0 || *tag == '\0')
    {
        return false;
    }

    *outNumerator = -1;
    *outDenominator = -1;

    const std::string token(tag);
    const std::size_t begin = text.find(token);
    if (begin == std::string::npos)
    {
        return false;
    }

    std::size_t cursor = begin + token.size();
    int numerator = 0;
    bool hasNumeratorDigit = false;
    while (cursor < text.size())
    {
        const unsigned char ch = static_cast<unsigned char>(text[cursor]);
        if (std::isdigit(ch) == 0)
        {
            break;
        }
        numerator = numerator * 10 + static_cast<int>(ch - '0');
        hasNumeratorDigit = true;
        ++cursor;
    }
    if (!hasNumeratorDigit || cursor >= text.size() || text[cursor] != '/')
    {
        return false;
    }

    ++cursor;
    int denominator = 0;
    bool hasDenominatorDigit = false;
    while (cursor < text.size())
    {
        const unsigned char ch = static_cast<unsigned char>(text[cursor]);
        if (std::isdigit(ch) == 0)
        {
            break;
        }
        denominator = denominator * 10 + static_cast<int>(ch - '0');
        hasDenominatorDigit = true;
        ++cursor;
    }

    if (!hasDenominatorDigit)
    {
        return false;
    }

    *outNumerator = numerator;
    *outDenominator = denominator;
    return true;
}

std::string NormalizeSearchText(const std::string& text)
{
    std::string normalized;
    normalized.reserve(text.size());

    bool previousWasSpace = true;
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(text[index]);
        if (std::isalnum(ch) == 0)
        {
            if (!normalized.empty() && !previousWasSpace)
            {
                normalized.push_back(' ');
                previousWasSpace = true;
            }
            continue;
        }

        normalized.push_back(static_cast<char>(std::tolower(ch)));
        previousWasSpace = false;
    }

    if (!normalized.empty() && normalized[normalized.size() - 1] == ' ')
    {
        normalized.resize(normalized.size() - 1);
    }

    return normalized;
}

bool IsLikelyRuntimeWidgetToken(const std::string& token);

std::string CanonicalizeSearchToken(const std::string& token)
{
    if (token.empty())
    {
        return token;
    }

    if (!IsLikelyRuntimeWidgetToken(token))
    {
        return token;
    }

    const std::size_t underscore = token.find('_');
    if (underscore == std::string::npos || underscore + 1 >= token.size())
    {
        return std::string();
    }

    return token.substr(underscore + 1);
}

bool ContainsAsciiLetter(const std::string& value)
{
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(value[index]);
        if (std::isalpha(ch) != 0)
        {
            return true;
        }
    }

    return false;
}

bool ContainsAsciiDigit(const std::string& value)
{
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(value[index]);
        if (std::isdigit(ch) != 0)
        {
            return true;
        }
    }

    return false;
}

bool IsGenericCaptionToken(const std::string& token)
{
    return token == "trader"
        || token == "shop"
        || token == "merchant"
        || token == "store"
        || token == "the"
        || token == "of"
        || token == "and";
}

int ComputeCaptionNameMatchBias(const std::string& captionNormalized, const std::string& nameNormalized)
{
    if (captionNormalized.empty() || nameNormalized.empty())
    {
        return 0;
    }

    int score = 0;
    if (captionNormalized == nameNormalized)
    {
        score += 2200;
    }
    if (captionNormalized.find(nameNormalized) != std::string::npos)
    {
        score += 1400;
    }
    if (nameNormalized.find(captionNormalized) != std::string::npos)
    {
        score += 900;
    }

    std::size_t start = 0;
    while (start < captionNormalized.size())
    {
        while (start < captionNormalized.size() && captionNormalized[start] == ' ')
        {
            ++start;
        }
        if (start >= captionNormalized.size())
        {
            break;
        }

        std::size_t end = start;
        while (end < captionNormalized.size() && captionNormalized[end] != ' ')
        {
            ++end;
        }

        const std::string token = captionNormalized.substr(start, end - start);
        if (token.size() >= 3
            && !IsGenericCaptionToken(token)
            && nameNormalized.find(token) != std::string::npos)
        {
            score += 220;
        }

        start = end + 1;
    }

    return score;
}

bool IsLikelyRuntimeWidgetToken(const std::string& token)
{
    const std::size_t underscore = token.find('_');
    if (underscore == std::string::npos || underscore < 3)
    {
        return false;
    }

    bool sawDigitOrComma = false;
    for (std::size_t index = 0; index < underscore; ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(token[index]);
        const bool hexAlpha = (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
        if (std::isdigit(ch) != 0 || ch == ',' || ch == 'x' || ch == 'X' || hexAlpha)
        {
            if (std::isdigit(ch) != 0 || ch == ',')
            {
                sawDigitOrComma = true;
            }
            continue;
        }

        return false;
    }

    return sawDigitOrComma;
}

bool ShouldIndexSearchToken(const std::string& token)
{
    if (token.empty())
    {
        return false;
    }

    const std::string normalized = NormalizeSearchText(token);
    if (normalized.empty())
    {
        return false;
    }

    if (normalized == "root"
        || normalized == "background"
        || normalized == "itemimage"
        || normalized == "quantitytext"
        || normalized == "chargebar"
        || normalized == "baselayoutprefix"
        || normalized.find("quantitytext") != std::string::npos
        || normalized.find("itemimage") != std::string::npos
        || normalized.find("background") != std::string::npos
        || normalized.find("chargebar") != std::string::npos)
    {
        return false;
    }

    if (!ContainsAsciiLetter(normalized) && ContainsAsciiDigit(normalized))
    {
        return false;
    }

    return true;
}

bool IsNumericOnlyQuery(const std::string& normalizedQuery)
{
    if (normalizedQuery.empty())
    {
        return false;
    }

    bool hasDigit = false;
    for (std::size_t index = 0; index < normalizedQuery.size(); ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(normalizedQuery[index]);
        if (std::isdigit(ch) != 0)
        {
            hasDigit = true;
            continue;
        }
        if (std::isspace(ch) != 0 || ch == '.' || ch == ',' || ch == '-' || ch == '+')
        {
            continue;
        }
        return false;
    }

    return hasDigit;
}

void AppendSearchToken(std::string* text, const std::string& token)
{
    if (text == 0 || token.empty())
    {
        return;
    }

    const std::string canonicalToken = CanonicalizeSearchToken(token);
    if (canonicalToken.empty() || !ShouldIndexSearchToken(canonicalToken))
    {
        return;
    }

    if (!text->empty())
    {
        text->push_back(' ');
    }
    text->append(canonicalToken);
}

MyGUI::Widget* FindNamedDescendantByTokenRecursive(MyGUI::Widget* root, const char* token, bool requireVisible)
{
    if (root == 0 || token == 0)
    {
        return 0;
    }

    if ((!requireVisible || root->getInheritedVisible()) && NameMatchesToken(root->getName(), token))
    {
        return root;
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        MyGUI::Widget* child = root->getChildAt(childIndex);
        MyGUI::Widget* found = FindNamedDescendantByTokenRecursive(child, token, requireVisible);
        if (found != 0)
        {
            return found;
        }
    }

    return 0;
}

void CollectNamedDescendantsByTokenRecursive(
    MyGUI::Widget* root,
    const char* token,
    bool requireVisible,
    std::size_t maxResults,
    std::vector<MyGUI::Widget*>* outWidgets)
{
    if (root == 0 || token == 0 || outWidgets == 0 || outWidgets->size() >= maxResults)
    {
        return;
    }

    if ((!requireVisible || root->getInheritedVisible()) && NameMatchesToken(root->getName(), token))
    {
        outWidgets->push_back(root);
        if (outWidgets->size() >= maxResults)
        {
            return;
        }
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        CollectNamedDescendantsByTokenRecursive(
            root->getChildAt(childIndex),
            token,
            requireVisible,
            maxResults,
            outWidgets);
        if (outWidgets->size() >= maxResults)
        {
            return;
        }
    }
}

void CollectNamedDescendantsByToken(
    MyGUI::Widget* root,
    const char* token,
    bool requireVisible,
    std::size_t maxResults,
    std::vector<MyGUI::Widget*>* outWidgets)
{
    if (outWidgets == 0)
    {
        return;
    }

    outWidgets->clear();
    if (root == 0 || token == 0 || maxResults == 0)
    {
        return;
    }

    CollectNamedDescendantsByTokenRecursive(
        root,
        token,
        requireVisible,
        maxResults,
        outWidgets);
}

MyGUI::Widget* FindAncestorByToken(MyGUI::Widget* widget, const char* token)
{
    if (widget == 0 || token == 0)
    {
        return 0;
    }

    MyGUI::Widget* current = widget;
    while (current != 0)
    {
        if (NameMatchesToken(current->getName(), token))
        {
            return current;
        }
        current = current->getParent();
    }

    return 0;
}

std::string TruncateForLog(const std::string& value, std::size_t maxLength)
{
    if (value.size() <= maxLength)
    {
        return value;
    }

    return value.substr(0, maxLength) + "...";
}

std::string WidgetTypeForLog(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "null";
    }
    if (widget->castType<MyGUI::Window>(false) != 0)
    {
        return "Window";
    }
    if (widget->castType<MyGUI::Button>(false) != 0)
    {
        return "Button";
    }
    if (widget->castType<MyGUI::EditBox>(false) != 0)
    {
        return "EditBox";
    }
    if (widget->castType<MyGUI::ComboBox>(false) != 0)
    {
        return "ComboBox";
    }
    if (widget->castType<MyGUI::TextBox>(false) != 0)
    {
        return "TextBox";
    }

    return "Widget";
}

std::string WidgetCaptionForLog(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return "";
    }

    MyGUI::Button* button = widget->castType<MyGUI::Button>(false);
    if (button != 0)
    {
        return button->getCaption().asUTF8();
    }

    MyGUI::TextBox* textBox = widget->castType<MyGUI::TextBox>(false);
    if (textBox != 0)
    {
        return textBox->getCaption().asUTF8();
    }

    return "";
}

void LogWidgetDiagnosticNode(const char* tag, MyGUI::Widget* widget, std::size_t depth)
{
    if (widget == 0)
    {
        std::stringstream line;
        line << tag << " depth=" << depth << " widget=<null>";
        LogInfoLine(line.str());
        return;
    }

    const MyGUI::IntCoord coord = widget->getCoord();
    std::stringstream line;
    line << tag
         << " depth=" << depth
         << " type=" << WidgetTypeForLog(widget)
         << " name=" << SafeWidgetName(widget)
         << " visible=" << (widget->getInheritedVisible() ? "true" : "false")
         << " children=" << widget->getChildCount()
         << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";

    const std::string caption = TruncateForLog(WidgetCaptionForLog(widget), 48);
    if (!caption.empty())
    {
        line << " caption=\"" << caption << "\"";
    }

    LogInfoLine(line.str());
}

void DumpAncestorDiagnostics(const char* label, MyGUI::Widget* widget)
{
    std::stringstream header;
    header << "diagnostic " << label << " ancestor-chain";
    LogInfoLine(header.str());

    std::size_t depth = 0;
    while (widget != 0 && depth < 12)
    {
        LogWidgetDiagnosticNode(label, widget, depth);
        widget = widget->getParent();
        ++depth;
    }

    if (widget != 0)
    {
        std::stringstream line;
        line << label << " depth=" << depth << " ...";
        LogInfoLine(line.str());
    }
}

void DumpNamedDescendantDiagnosticsRecursive(
    MyGUI::Widget* widget,
    std::size_t depth,
    std::size_t maxDepth,
    std::size_t* loggedCount,
    std::size_t maxEntries)
{
    if (widget == 0 || loggedCount == 0 || *loggedCount >= maxEntries || depth > maxDepth)
    {
        return;
    }

    const bool shouldLog = !widget->getName().empty() || !WidgetCaptionForLog(widget).empty();
    if (shouldLog)
    {
        LogWidgetDiagnosticNode("subtree", widget, depth);
        ++(*loggedCount);
        if (*loggedCount >= maxEntries)
        {
            return;
        }
    }

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        DumpNamedDescendantDiagnosticsRecursive(
            widget->getChildAt(childIndex),
            depth + 1,
            maxDepth,
            loggedCount,
            maxEntries);
        if (*loggedCount >= maxEntries)
        {
            return;
        }
    }
}

void DumpWidgetSubtreeDiagnostics(const char* label, MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return;
    }

    std::stringstream header;
    header << "diagnostic " << label << " subtree-begin";
    LogInfoLine(header.str());
    LogWidgetDiagnosticNode(label, widget, 0);

    std::size_t loggedCount = 0;
    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        DumpNamedDescendantDiagnosticsRecursive(
            widget->getChildAt(childIndex),
            1,
            3,
            &loggedCount,
            36);
        if (loggedCount >= 36)
        {
            break;
        }
    }

    if (loggedCount >= 36)
    {
        std::stringstream line;
        line << "diagnostic " << label << " subtree-truncated=true";
        LogInfoLine(line.str());
    }

    std::stringstream footer;
    footer << "diagnostic " << label << " subtree-end";
    LogInfoLine(footer.str());
}

void DumpHoveredAttachDiagnostics(MyGUI::Widget* hovered, MyGUI::Widget* anchor, MyGUI::Widget* parent)
{
    LogInfoLine("diagnostic hovered-attach begin");
    DumpAncestorDiagnostics("hovered", hovered);

    if (anchor != 0 && anchor != hovered)
    {
        DumpAncestorDiagnostics("anchor", anchor);
    }

    if (parent != 0 && parent != anchor)
    {
        DumpAncestorDiagnostics("parent", parent);
    }

    DumpWidgetSubtreeDiagnostics("anchor", anchor);
    if (parent != 0 && parent != anchor)
    {
        DumpWidgetSubtreeDiagnostics("parent", parent);
    }

    LogInfoLine("diagnostic hovered-attach end");
}

MyGUI::Widget* FindWidgetByName(const char* widgetName)
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return 0;
    }
    return gui->findWidgetT(widgetName, false);
}

MyGUI::Widget* FindControlsContainer()
{
    return FindWidgetByName(kControlsContainerName);
}

MyGUI::EditBox* FindSearchEditBox()
{
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0)
    {
        return 0;
    }

    MyGUI::Widget* found = FindNamedDescendantRecursive(controlsContainer, kSearchEditName, false);
    return found == 0 ? 0 : found->castType<MyGUI::EditBox>(false);
}

MyGUI::TextBox* FindSearchPlaceholderTextBox()
{
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0)
    {
        return 0;
    }

    MyGUI::Widget* found =
        FindNamedDescendantRecursive(controlsContainer, kSearchPlaceholderName, false);
    return found == 0 ? 0 : found->castType<MyGUI::TextBox>(false);
}

MyGUI::Button* FindSearchClearButton()
{
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0)
    {
        return 0;
    }

    MyGUI::Widget* found =
        FindNamedDescendantRecursive(controlsContainer, kSearchClearButtonName, false);
    return found == 0 ? 0 : found->castType<MyGUI::Button>(false);
}

MyGUI::TextBox* FindSearchCountTextBox()
{
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0)
    {
        return 0;
    }

    MyGUI::Widget* found = FindNamedDescendantRecursive(controlsContainer, kSearchCountTextName, false);
    return found == 0 ? 0 : found->castType<MyGUI::TextBox>(false);
}

MyGUI::Widget* FindNamedDescendantRecursive(MyGUI::Widget* root, const char* widgetName, bool requireVisible)
{
    if (root == 0 || widgetName == 0)
    {
        return 0;
    }

    if ((!requireVisible || root->getInheritedVisible()) && root->getName() == widgetName)
    {
        return root;
    }

    const std::size_t childCount = root->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        MyGUI::Widget* child = root->getChildAt(childIndex);
        MyGUI::Widget* found = FindNamedDescendantRecursive(child, widgetName, requireVisible);
        if (found != 0)
        {
            return found;
        }
    }

    return 0;
}

MyGUI::Widget* FindFirstVisibleWidgetByName(const char* widgetName)
{
    if (widgetName == 0)
    {
        return 0;
    }

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

        MyGUI::Widget* found = FindNamedDescendantRecursive(root, widgetName, true);
        if (found != 0)
        {
            return found;
        }
    }

    return 0;
}

MyGUI::Widget* FindFirstVisibleWidgetByToken(const char* token)
{
    if (token == 0)
    {
        return 0;
    }

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

        MyGUI::Widget* found = FindNamedDescendantByTokenRecursive(root, token, true);
        if (found != 0)
        {
            return found;
        }
    }

    return 0;
}

void DumpVisibleRootWidgetsForDiagnostics()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        LogWarnLine("GUI singleton unavailable while dumping root widgets");
        return;
    }

    std::size_t index = 0;
    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0)
        {
            continue;
        }

        if (!root->getInheritedVisible())
        {
            continue;
        }

        const MyGUI::IntCoord coord = root->getCoord();
        std::stringstream line;
        line << "root[" << index << "] name=" << SafeWidgetName(root)
             << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";
        LogInfoLine(line.str());
        ++index;
    }
}

bool IsDescendantOf(MyGUI::Widget* widget, MyGUI::Widget* ancestor)
{
    if (widget == 0 || ancestor == 0)
    {
        return false;
    }

    MyGUI::Widget* current = widget;
    while (current != 0)
    {
        if (current == ancestor)
        {
            return true;
        }
        current = current->getParent();
    }

    return false;
}

MyGUI::Widget* FindWidgetInParentByToken(MyGUI::Widget* parent, const char* token)
{
    if (parent == 0 || token == 0)
    {
        return 0;
    }
    return FindNamedDescendantByTokenRecursive(parent, token, false);
}

MyGUI::Window* FindOwningWindow(MyGUI::Widget* widget)
{
    while (widget != 0)
    {
        MyGUI::Window* window = widget->castType<MyGUI::Window>(false);
        if (window != 0)
        {
            return window;
        }
        widget = widget->getParent();
    }

    return 0;
}

bool HasTraderInventoryMarkers(MyGUI::Widget* parent)
{
    if (parent == 0)
    {
        return false;
    }

    const bool hasArrangeButton = FindWidgetInParentByToken(parent, "ArrangeButton") != 0;
    const bool hasScrollView = FindWidgetInParentByToken(parent, "scrollview_backpack_content") != 0;
    const bool hasBackpack = FindWidgetInParentByToken(parent, "backpack_content") != 0;

    return hasArrangeButton && hasScrollView && hasBackpack;
}

bool HasTraderMoneyMarkers(MyGUI::Widget* parent)
{
    if (parent == 0)
    {
        return false;
    }

    const char* moneyTokens[] =
    {
        "MoneyAmountTextBox",
        "MoneyAmountText",
        "TotalMoneyBuyer",
        "lbTotalMoney",
        "MoneyLabelText",
        "lbBuyersMoney"
    };

    for (std::size_t index = 0; index < sizeof(moneyTokens) / sizeof(moneyTokens[0]); ++index)
    {
        if (FindWidgetInParentByToken(parent, moneyTokens[index]) != 0)
        {
            return true;
        }
    }

    return false;
}

int ComputeTraderWindowCandidateScore(MyGUI::Widget* parent, std::string* outReason)
{
    if (outReason != 0)
    {
        outReason->clear();
    }

    if (parent == 0 || !HasTraderInventoryMarkers(parent))
    {
        return 0;
    }

    MyGUI::Window* window = FindOwningWindow(parent);
    const bool hasMoneyMarkers = HasTraderMoneyMarkers(parent);
    const std::string caption = window == 0 ? "" : window->getCaption().asUTF8();
    const bool captionHasTrader = ContainsAsciiCaseInsensitive(caption, "TRADER");
    const std::string normalizedCaption = NormalizeSearchText(caption);

    int score = 100;
    bool hasTraderSignal = false;
    std::stringstream reason;
    reason << "inventory_markers";

    if (hasMoneyMarkers)
    {
        score += 1600;
        hasTraderSignal = true;
        reason << " money_markers";
    }

    if (captionHasTrader)
    {
        score += 1400;
        hasTraderSignal = true;
        reason << " caption_token=trader";
    }

    Character* captionTrader = 0;
    int captionScore = 0;
    if (!normalizedCaption.empty()
        && TryResolveCaptionMatchedTraderCharacter(parent, &captionTrader, &captionScore)
        && captionTrader != 0
        && captionScore > 0)
    {
        score += 900 + (captionScore > 2600 ? 2600 : captionScore);
        hasTraderSignal = true;
        reason << " caption_match=\"" << TruncateForLog(CharacterNameForLog(captionTrader), 40)
               << "\"(" << captionScore << ")";
    }

    Character* dialogueTarget = 0;
    Character* dialogueSpeaker = 0;
    std::string dialogueReason;
    if (!normalizedCaption.empty()
        && TryResolvePreferredDialogueTraderTarget(&dialogueTarget, &dialogueSpeaker, &dialogueReason)
        && dialogueTarget != 0)
    {
        const int dialogueCaptionScore = ComputeCaptionNameMatchBias(
            normalizedCaption,
            NormalizeSearchText(CharacterNameForLog(dialogueTarget)));
        if (dialogueCaptionScore > 0)
        {
            score += 1800 + (dialogueCaptionScore > 2800 ? 2800 : dialogueCaptionScore);
            hasTraderSignal = true;
            reason << " dialogue_match=\"" << TruncateForLog(CharacterNameForLog(dialogueTarget), 40)
                   << "\"(" << dialogueCaptionScore << ")";
            if (captionTrader != 0 && captionTrader == dialogueTarget)
            {
                score += 700;
                reason << " caption_dialogue_same=true";
            }
        }
    }

    if (!hasTraderSignal)
    {
        return 0;
    }

    if (outReason != 0)
    {
        *outReason = reason.str();
    }

    return score;
}

bool IsLikelyTraderWindow(MyGUI::Widget* parent)
{
    return ComputeTraderWindowCandidateScore(parent, 0) > 0;
}

bool HasTraderStructure(MyGUI::Widget* parent)
{
    return HasTraderInventoryMarkers(parent) || HasTraderMoneyMarkers(parent);
}

void DumpTraderTargetProbe()
{
    const char* probeTokens[] =
    {
        "scrollview_backpack_content",
        "backpack_content",
        "ArrangeButton",
        "datapanel",
        "TradePanel",
        "CharacterSelectionItemBox",
        "MoneyAmountTextBox",
        "TotalMoneyBuyer"
    };

    for (std::size_t index = 0; index < sizeof(probeTokens) / sizeof(probeTokens[0]); ++index)
    {
        const char* token = probeTokens[index];
        const bool exactAny = FindWidgetByName(token) != 0;
        const bool exactVisible = FindFirstVisibleWidgetByName(token) != 0;
        const bool tokenVisible = FindFirstVisibleWidgetByToken(token) != 0;

        std::stringstream line;
        line << "probe token=" << token
             << " exact_any=" << (exactAny ? "true" : "false")
             << " exact_visible=" << (exactVisible ? "true" : "false")
             << " token_visible=" << (tokenVisible ? "true" : "false");
        LogInfoLine(line.str());
    }
}

void DumpVisibleWindowCandidateDiagnostics()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        LogWarnLine("GUI singleton unavailable while dumping window candidates");
        return;
    }

    std::size_t index = 0;
    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        MyGUI::Window* window = root->castType<MyGUI::Window>(false);
        if (window == 0)
        {
            continue;
        }

        MyGUI::Widget* parent = ResolveInjectionParent(root);
        if (parent == 0)
        {
            continue;
        }

        const bool hasMarkers = HasTraderStructure(parent);
        const bool hasMoneyMarkers = HasTraderMoneyMarkers(parent);
        const bool captionHasTrader = ContainsAsciiCaseInsensitive(window->getCaption().asUTF8(), "TRADER");
        if (!hasMarkers && !captionHasTrader)
        {
            continue;
        }

        std::string candidateReason;
        const int candidateScore = ComputeTraderWindowCandidateScore(parent, &candidateReason);
        const bool likelyTrader = candidateScore > 0;
        const MyGUI::IntCoord coord = root->getCoord();
        std::stringstream line;
        line << "window-candidate[" << index << "]"
             << " name=" << SafeWidgetName(root)
             << " caption=\"" << TruncateForLog(window->getCaption().asUTF8(), 60) << "\""
             << " has_markers=" << (hasMarkers ? "true" : "false")
             << " has_money_markers=" << (hasMoneyMarkers ? "true" : "false")
             << " caption_has_trader=" << (captionHasTrader ? "true" : "false")
             << " likely_trader=" << (likelyTrader ? "true" : "false")
             << " candidate_score=" << candidateScore
             << " candidate_reason=\"" << TruncateForLog(candidateReason, 120) << "\""
             << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";
        LogInfoLine(line.str());
        ++index;
    }

    if (index == 0)
    {
        LogInfoLine("window-candidate scan found no likely trader windows");
    }
}

bool TryResolveVisibleTraderTarget(MyGUI::Widget** outAnchor, MyGUI::Widget** outParent)
{
    if (outAnchor != 0)
    {
        *outAnchor = 0;
    }
    if (outParent != 0)
    {
        *outParent = 0;
    }

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        return false;
    }

    MyGUI::Widget* bestAnchor = 0;
    MyGUI::Widget* bestParent = 0;
    std::string bestReason;
    int bestScore = -1;
    int bestArea = -1;

    MyGUI::EnumeratorWidgetPtr roots = gui->getEnumerator();
    while (roots.next())
    {
        MyGUI::Widget* root = roots.current();
        if (root == 0 || !root->getInheritedVisible())
        {
            continue;
        }

        MyGUI::Window* window = root->castType<MyGUI::Window>(false);
        if (window == 0)
        {
            continue;
        }

        MyGUI::Widget* parent = ResolveInjectionParent(root);
        if (parent == 0)
        {
            continue;
        }

        std::string candidateReason;
        const int candidateScore = ComputeTraderWindowCandidateScore(parent, &candidateReason);
        if (candidateScore <= 0)
        {
            continue;
        }

        const MyGUI::IntCoord coord = root->getCoord();
        const int area = coord.width * coord.height;
        if (bestAnchor == 0
            || candidateScore > bestScore
            || (candidateScore == bestScore && area > bestArea))
        {
            bestAnchor = root;
            bestParent = parent;
            bestReason = candidateReason;
            bestScore = candidateScore;
            bestArea = area;
        }
    }

    if (bestAnchor == 0 || bestParent == 0)
    {
        return false;
    }

    if (outAnchor != 0)
    {
        *outAnchor = bestAnchor;
    }
    if (outParent != 0)
    {
        *outParent = bestParent;
    }

    g_loggedRejectedTraderCandidate = false;
    MyGUI::Window* window = bestAnchor->castType<MyGUI::Window>(false);
    std::stringstream line;
    line << "resolved trader target via window-scan"
         << " anchor=" << SafeWidgetName(bestAnchor)
         << " parent=" << SafeWidgetName(bestParent)
         << " caption=\"" << (window == 0 ? "" : TruncateForLog(window->getCaption().asUTF8(), 60)) << "\""
         << " candidate_score=" << bestScore
         << " candidate_reason=\"" << TruncateForLog(bestReason, 160) << "\"";
    LogInfoLine(line.str());
    return true;
}

bool TryResolveHoveredTarget(MyGUI::Widget** outAnchor, MyGUI::Widget** outParent, bool logFailures)
{
    if (outAnchor != 0)
    {
        *outAnchor = 0;
    }
    if (outParent != 0)
    {
        *outParent = 0;
    }

    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        if (logFailures)
        {
            LogWarnLine("hover attach failed: MyGUI InputManager unavailable");
        }
        return false;
    }

    MyGUI::Widget* hovered = inputManager->getMouseFocusWidget();
    if (hovered == 0)
    {
        if (logFailures)
        {
            LogWarnLine("hover attach failed: no mouse-focused widget");
        }
        return false;
    }

    MyGUI::Widget* anchor = FindBestWindowAnchor(hovered);
    MyGUI::Widget* parent = ResolveInjectionParent(anchor);
    if (anchor == 0 || parent == 0)
    {
        if (logFailures)
        {
            std::stringstream line;
            line << "hover attach failed: anchor/parent unresolved hovered_chain=" << BuildParentChainForLog(hovered);
            LogWarnLine(line.str());
        }
        return false;
    }

    std::string candidateReason;
    const int candidateScore = ComputeTraderWindowCandidateScore(parent, &candidateReason);
    if (candidateScore <= 0)
    {
        if (logFailures)
        {
            std::stringstream line;
            line << "hover attach rejected"
                 << " anchor=" << SafeWidgetName(anchor)
                 << " parent=" << SafeWidgetName(parent)
                 << " candidate_score=" << candidateScore
                 << " candidate_reason=\"" << TruncateForLog(candidateReason, 120) << "\""
                 << " parent_coord=(" << parent->getCoord().left << "," << parent->getCoord().top << ","
                 << parent->getCoord().width << "," << parent->getCoord().height << ")"
                 << " hovered_chain=" << BuildParentChainForLog(hovered);
            LogWarnLine(line.str());
        }
        return false;
    }

    if (outAnchor != 0)
    {
        *outAnchor = anchor;
    }
    if (outParent != 0)
    {
        *outParent = parent;
    }

    std::stringstream line;
    line << "resolved trader target via hover attach"
         << " anchor=" << SafeWidgetName(anchor)
         << " parent=" << SafeWidgetName(parent)
         << " candidate_score=" << candidateScore
         << " candidate_reason=\"" << TruncateForLog(candidateReason, 160) << "\""
         << " parent_coord=(" << parent->getCoord().left << "," << parent->getCoord().top << ","
         << parent->getCoord().width << "," << parent->getCoord().height << ")"
         << " hovered_chain=" << BuildParentChainForLog(hovered);
    LogInfoLine(line.str());
    return true;
}

MyGUI::Widget* FindBestWindowAnchor(MyGUI::Widget* fromWidget)
{
    MyGUI::Widget* current = fromWidget;
    MyGUI::Widget* rootMost = 0;
    MyGUI::Widget* windowAncestor = 0;

    while (current != 0)
    {
        rootMost = current;
        if (current->castType<MyGUI::Window>(false) != 0)
        {
            windowAncestor = current;
        }
        current = current->getParent();
    }

    if (windowAncestor != 0)
    {
        return windowAncestor;
    }
    return rootMost;
}

MyGUI::Widget* ResolveInjectionParent(MyGUI::Widget* anchor)
{
    if (anchor == 0)
    {
        return 0;
    }

    MyGUI::Window* window = anchor->castType<MyGUI::Window>(false);
    if (window != 0)
    {
        MyGUI::Widget* client = window->getClientWidget();
        if (client != 0)
        {
            return client;
        }
    }

    return anchor;
}

void AppendWidgetSearchTokens(MyGUI::Widget* widget, std::string* searchText)
{
    if (widget == 0 || searchText == 0)
    {
        return;
    }

    AppendSearchToken(searchText, widget->getName());
    AppendSearchToken(searchText, WidgetCaptionForLog(widget));

    const MyGUI::MapString& userStrings = widget->getUserStrings();
    for (MyGUI::MapString::const_iterator it = userStrings.begin(); it != userStrings.end(); ++it)
    {
        AppendSearchToken(searchText, it->first);
        AppendSearchToken(searchText, it->second);
    }
}

template <typename T>
T* ReadWidgetUserDataPointer(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    T** typed = widget->getUserData<T*>(false);
    if (typed == 0)
    {
        return 0;
    }

    return *typed;
}

template <typename T>
T* ReadWidgetInternalDataPointer(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    T** typed = widget->_getInternalData<T*>(false);
    if (typed == 0)
    {
        return 0;
    }

    return *typed;
}

template <typename T>
T* ReadWidgetUserDataObject(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    return widget->getUserData<T>(false);
}

template <typename T>
T* ReadWidgetInternalDataObject(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    return widget->_getInternalData<T>(false);
}

template <typename T>
T* ReadWidgetAnyDataPointer(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    T* pointerInternal = ReadWidgetInternalDataPointer<T>(widget);
    if (pointerInternal != 0)
    {
        return pointerInternal;
    }

    T* pointerUser = ReadWidgetUserDataPointer<T>(widget);
    if (pointerUser != 0)
    {
        return pointerUser;
    }
    return 0;
}

Item* ResolveWidgetItemPointer(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    Item* item = ReadWidgetAnyDataPointer<Item>(widget);
    if (item != 0)
    {
        return item;
    }

    InventoryItemBase* itemBase = ReadWidgetAnyDataPointer<InventoryItemBase>(widget);
    if (itemBase == 0)
    {
        return 0;
    }

    return dynamic_cast<Item*>(itemBase);
}

RootObjectBase* ResolveWidgetObjectBasePointer(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    RootObjectBase* objectBase = ReadWidgetAnyDataPointer<RootObjectBase>(widget);
    if (objectBase != 0)
    {
        return objectBase;
    }

    RootObject* object = ReadWidgetAnyDataPointer<RootObject>(widget);
    if (object != 0)
    {
        return object;
    }

    InventoryItemBase* itemBase = ReadWidgetAnyDataPointer<InventoryItemBase>(widget);
    if (itemBase != 0)
    {
        return itemBase;
    }

    return 0;
}

Inventory* ResolveWidgetInventoryPointer(MyGUI::Widget* widget)
{
    if (widget == 0)
    {
        return 0;
    }

    Inventory* inventory = ReadWidgetAnyDataPointer<Inventory>(widget);
    if (inventory != 0)
    {
        return inventory;
    }

    Item* item = ResolveWidgetItemPointer(widget);
    if (item != 0)
    {
        inventory = item->getInventory();
        if (inventory != 0)
        {
            return inventory;
        }
    }

    InventorySection* section = ReadWidgetAnyDataPointer<InventorySection>(widget);
    if (section != 0)
    {
        Inventory* sectionInventory = section->getInventory();
        if (sectionInventory != 0)
        {
            return sectionInventory;
        }
    }

    RootObjectBase* objectBase = ResolveWidgetObjectBasePointer(widget);
    RootObject* object = dynamic_cast<RootObject*>(objectBase);
    if (object != 0)
    {
        return object->getInventory();
    }

    hand* handValue = widget->_getInternalData<hand>(false);
    if (handValue == 0)
    {
        handValue = widget->getUserData<hand>(false);
    }
    if (handValue != 0 && handValue->isValid())
    {
        Item* handItem = handValue->getItem();
        if (handItem != 0)
        {
            Inventory* handInventory = handItem->getInventory();
            if (handInventory != 0)
            {
                return handInventory;
            }
        }

        RootObjectBase* handObjectBase = handValue->getRootObjectBase();
        RootObject* handObject = dynamic_cast<RootObject*>(handObjectBase);
        if (handObject != 0)
        {
            Inventory* handInventory = handObject->getInventory();
            if (handInventory != 0)
            {
                return handInventory;
            }
        }
    }

    hand** handPointer = widget->_getInternalData<hand*>(false);
    if (handPointer == 0)
    {
        handPointer = widget->getUserData<hand*>(false);
    }
    if (handPointer != 0 && *handPointer != 0 && (*handPointer)->isValid())
    {
        Item* handItem = (*handPointer)->getItem();
        if (handItem != 0)
        {
            Inventory* handInventory = handItem->getInventory();
            if (handInventory != 0)
            {
                return handInventory;
            }
        }

        RootObjectBase* handObjectBase = (*handPointer)->getRootObjectBase();
        RootObject* handObject = dynamic_cast<RootObject*>(handObjectBase);
        if (handObject != 0)
        {
            Inventory* handInventory = handObject->getInventory();
            if (handInventory != 0)
            {
                return handInventory;
            }
        }
    }

    return 0;
}

void AppendGameDataSearchTokens(GameData* data, std::string* searchText)
{
    if (data == 0 || searchText == 0)
    {
        return;
    }

    AppendSearchToken(searchText, data->name);
    AppendSearchToken(searchText, data->stringID);
}

void AppendRootObjectSearchTokens(RootObjectBase* objectBase, std::string* searchText)
{
    if (objectBase == 0 || searchText == 0)
    {
        return;
    }

    AppendSearchToken(searchText, objectBase->displayName);
    AppendSearchToken(searchText, objectBase->getName());
    AppendGameDataSearchTokens(objectBase->data, searchText);
}

void AppendWidgetObjectDataTokens(MyGUI::Widget* widget, std::string* searchText)
{
    if (widget == 0 || searchText == 0)
    {
        return;
    }

    Item* item = ResolveWidgetItemPointer(widget);
    if (item != 0)
    {
        AppendRootObjectSearchTokens(item, searchText);
        return;
    }

    RootObjectBase* objectBase = ResolveWidgetObjectBasePointer(widget);
    if (objectBase != 0)
    {
        AppendRootObjectSearchTokens(objectBase, searchText);
    }

    Inventory* inventory = ResolveWidgetInventoryPointer(widget);
    if (inventory != 0)
    {
        RootObject* owner = inventory->getOwner();
        if (owner == 0)
        {
            owner = inventory->getCallbackObject();
        }
        if (owner != 0)
        {
            AppendRootObjectSearchTokens(owner, searchText);
        }
    }

    GameData* data = ReadWidgetInternalDataPointer<GameData>(widget);
    if (data == 0)
    {
        data = ReadWidgetUserDataPointer<GameData>(widget);
    }
    if (data != 0)
    {
        AppendGameDataSearchTokens(data, searchText);
    }

    hand* handValue = widget->_getInternalData<hand>(false);
    if (handValue == 0)
    {
        handValue = widget->getUserData<hand>(false);
    }
    if (handValue != 0 && handValue->isValid())
    {
        Item* handItem = handValue->getItem();
        if (handItem != 0)
        {
            AppendRootObjectSearchTokens(handItem, searchText);
        }
        else
        {
            RootObjectBase* handObject = handValue->getRootObjectBase();
            if (handObject != 0)
            {
                AppendRootObjectSearchTokens(handObject, searchText);
            }
        }
    }

    hand** handPointer = widget->_getInternalData<hand*>(false);
    if (handPointer == 0)
    {
        handPointer = widget->getUserData<hand*>(false);
    }
    if (handPointer != 0 && *handPointer != 0 && (*handPointer)->isValid())
    {
        Item* handItem = (*handPointer)->getItem();
        if (handItem != 0)
        {
            AppendRootObjectSearchTokens(handItem, searchText);
        }
        else
        {
            RootObjectBase* handObject = (*handPointer)->getRootObjectBase();
            if (handObject != 0)
            {
                AppendRootObjectSearchTokens(handObject, searchText);
            }
        }
    }

    MyGUI::ImageBox* imageBox = widget->castType<MyGUI::ImageBox>(false);
    if (imageBox != 0)
    {
        const std::size_t imageIndex = imageBox->getImageIndex();
        MyGUI::ResourceImageSetPtr imageSet = imageBox->getItemResource();
        if (imageSet != 0)
        {
            MyGUI::EnumeratorGroupImage groups = imageSet->getEnumerator();
            std::size_t groupCount = 0;
            while (groups.next() && groupCount < 8)
            {
                const MyGUI::GroupImage& group = groups.current();
                AppendSearchToken(searchText, group.name);
                AppendSearchToken(searchText, group.texture);

                if (imageIndex < group.indexes.size())
                {
                    AppendSearchToken(searchText, group.indexes[imageIndex].name);
                }

                ++groupCount;
            }
        }
    }
}

std::string ResolveItemNameHintFromObjectBase(RootObjectBase* objectBase)
{
    if (objectBase == 0)
    {
        return "";
    }

    if (!objectBase->displayName.empty())
    {
        return objectBase->displayName;
    }

    const std::string objectName = objectBase->getName();
    if (!objectName.empty())
    {
        return objectName;
    }

    if (objectBase->data != 0)
    {
        if (!objectBase->data->name.empty())
        {
            return objectBase->data->name;
        }

        if (!objectBase->data->stringID.empty())
        {
            return objectBase->data->stringID;
        }
    }

    return "";
}

std::string ResolveItemNameHintRecursive(MyGUI::Widget* widget, std::size_t depth, std::size_t maxDepth)
{
    if (widget == 0 || depth > maxDepth)
    {
        return "";
    }

    Item* item = ResolveWidgetItemPointer(widget);
    if (item != 0)
    {
        const std::string itemName = ResolveItemNameHintFromObjectBase(item);
        if (!itemName.empty())
        {
            return itemName;
        }
    }

    RootObjectBase* objectBase = ResolveWidgetObjectBasePointer(widget);
    if (objectBase != 0)
    {
        const std::string objectName = ResolveItemNameHintFromObjectBase(objectBase);
        if (!objectName.empty())
        {
            return objectName;
        }
    }

    Inventory* inventory = ResolveWidgetInventoryPointer(widget);
    if (inventory != 0)
    {
        RootObject* owner = inventory->getOwner();
        if (owner == 0)
        {
            owner = inventory->getCallbackObject();
        }
        if (owner != 0)
        {
            const std::string ownerName = ResolveItemNameHintFromObjectBase(owner);
            if (!ownerName.empty())
            {
                return ownerName;
            }
        }
    }

    GameData* data = ReadWidgetInternalDataPointer<GameData>(widget);
    if (data == 0)
    {
        data = ReadWidgetUserDataPointer<GameData>(widget);
    }
    if (data != 0)
    {
        if (!data->name.empty())
        {
            return data->name;
        }
        if (!data->stringID.empty())
        {
            return data->stringID;
        }
    }

    hand* handValue = widget->_getInternalData<hand>(false);
    if (handValue == 0)
    {
        handValue = widget->getUserData<hand>(false);
    }
    if (handValue != 0 && handValue->isValid())
    {
        Item* handItem = handValue->getItem();
        if (handItem != 0)
        {
            const std::string itemName = ResolveItemNameHintFromObjectBase(handItem);
            if (!itemName.empty())
            {
                return itemName;
            }
        }

        RootObjectBase* handObject = handValue->getRootObjectBase();
        if (handObject != 0)
        {
            const std::string objectName = ResolveItemNameHintFromObjectBase(handObject);
            if (!objectName.empty())
            {
                return objectName;
            }
        }
    }

    hand** handPointer = widget->_getInternalData<hand*>(false);
    if (handPointer == 0)
    {
        handPointer = widget->getUserData<hand*>(false);
    }
    if (handPointer != 0 && *handPointer != 0 && (*handPointer)->isValid())
    {
        Item* handItem = (*handPointer)->getItem();
        if (handItem != 0)
        {
            const std::string itemName = ResolveItemNameHintFromObjectBase(handItem);
            if (!itemName.empty())
            {
                return itemName;
            }
        }

        RootObjectBase* handObject = (*handPointer)->getRootObjectBase();
        if (handObject != 0)
        {
            const std::string objectName = ResolveItemNameHintFromObjectBase(handObject);
            if (!objectName.empty())
            {
                return objectName;
            }
        }
    }

    MyGUI::ImageBox* imageBox = widget->castType<MyGUI::ImageBox>(false);
    if (imageBox != 0)
    {
        const std::size_t imageIndex = imageBox->getImageIndex();
        MyGUI::ResourceImageSetPtr imageSet = imageBox->getItemResource();
        if (imageSet != 0)
        {
            MyGUI::EnumeratorGroupImage groups = imageSet->getEnumerator();
            while (groups.next())
            {
                const MyGUI::GroupImage& group = groups.current();
                if (imageIndex < group.indexes.size() && !group.indexes[imageIndex].name.empty())
                {
                    return group.indexes[imageIndex].name;
                }

                if (!group.name.empty())
                {
                    return group.name;
                }
            }
        }
    }

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        const std::string childHint = ResolveItemNameHintRecursive(
            widget->getChildAt(childIndex),
            depth + 1,
            maxDepth);
        if (!childHint.empty())
        {
            return childHint;
        }
    }

    return "";
}

std::string ResolveCanonicalItemName(Item* item)
{
    if (item == 0)
    {
        return "";
    }

    if (!item->displayName.empty())
    {
        return item->displayName;
    }

    const std::string objectName = item->getName();
    if (!objectName.empty())
    {
        return objectName;
    }

    if (item->data != 0)
    {
        if (!item->data->name.empty())
        {
            return item->data->name;
        }
        if (!item->data->stringID.empty())
        {
            return item->data->stringID;
        }
    }

    Ogre::vector<StringPair>::type tooltipLines;
    item->getTooltipData1(tooltipLines);

    if (tooltipLines.empty())
    {
        item->getTooltipData2(tooltipLines);
    }

    for (std::size_t index = 0; index < tooltipLines.size(); ++index)
    {
        const StringPair& line = tooltipLines[index];
        if (!line.s1.empty() && ContainsAsciiLetter(line.s1))
        {
            return line.s1;
        }
        if (!line.s2.empty() && ContainsAsciiLetter(line.s2))
        {
            return line.s2;
        }
    }

    return "";
}

bool ParsePositiveIntFromText(const std::string& text, int* outValue)
{
    if (outValue == 0)
    {
        return false;
    }

    long long value = 0;
    bool hasDigit = false;
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(text[index]);
        if (std::isdigit(ch) == 0)
        {
            continue;
        }

        hasDigit = true;
        value = value * 10 + static_cast<long long>(ch - '0');
        if (value > 2147483647LL)
        {
            return false;
        }
    }

    if (!hasDigit || value <= 0)
    {
        return false;
    }

    *outValue = static_cast<int>(value);
    return true;
}

bool TryResolveItemQuantityFromWidgetRecursive(
    MyGUI::Widget* widget,
    std::size_t depth,
    std::size_t maxDepth,
    int* outQuantity)
{
    if (widget == 0 || outQuantity == 0 || depth > maxDepth)
    {
        return false;
    }

    const std::string widgetName = widget->getName();
    const std::string caption = WidgetCaptionForLog(widget);
    int parsedQuantity = 0;
    const bool looksLikeQuantityWidget =
        NameMatchesToken(widgetName, "QuantityText")
        || widgetName.find("QuantityText") != std::string::npos;
    if (looksLikeQuantityWidget && ParsePositiveIntFromText(caption, &parsedQuantity))
    {
        *outQuantity = parsedQuantity;
        return true;
    }

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        if (TryResolveItemQuantityFromWidgetRecursive(
                widget->getChildAt(childIndex),
                depth + 1,
                maxDepth,
                outQuantity))
        {
            return true;
        }
    }

    return false;
}

bool TryResolveItemQuantityFromWidget(MyGUI::Widget* itemWidget, int* outQuantity)
{
    if (outQuantity == 0)
    {
        return false;
    }

    *outQuantity = 0;
    return TryResolveItemQuantityFromWidgetRecursive(itemWidget, 0, 5, outQuantity);
}

void AppendNormalizedSearchChunk(const std::string& normalizedChunk, std::string* out)
{
    if (out == 0 || normalizedChunk.empty())
    {
        return;
    }

    if (!ContainsAsciiLetter(normalizedChunk))
    {
        return;
    }

    if (!out->empty())
    {
        out->push_back(' ');
    }
    out->append(normalizedChunk);
}

std::string BuildKeyPreviewForLog(const std::vector<std::string>& keys, std::size_t limit)
{
    if (keys.empty() || limit == 0)
    {
        return "";
    }

    std::stringstream out;
    const std::size_t count = keys.size() < limit ? keys.size() : limit;
    for (std::size_t index = 0; index < count; ++index)
    {
        if (index > 0)
        {
            out << " | ";
        }
        out << TruncateForLog(keys[index], 28);
    }
    if (keys.size() > count)
    {
        out << " | ...";
    }
    return out.str();
}

std::size_t CountNonEmptyKeys(const std::vector<std::string>& keys)
{
    std::size_t count = 0;
    for (std::size_t index = 0; index < keys.size(); ++index)
    {
        if (!keys[index].empty())
        {
            ++count;
        }
    }
    return count;
}

bool TryExtractSearchKeysFromInventorySection(InventorySection* section, std::vector<std::string>* outKeys)
{
    if (section == 0 || outKeys == 0)
    {
        return false;
    }

    const Ogre::vector<InventorySection::SectionItem>::type& sectionItems = section->getItems();
    if (sectionItems.empty())
    {
        return false;
    }

    std::vector<InventorySection::SectionItem> sortedItems(sectionItems.begin(), sectionItems.end());
    struct SectionItemTopLeftLess
    {
        bool operator()(const InventorySection::SectionItem& leftItem, const InventorySection::SectionItem& rightItem) const
        {
            if (leftItem.y != rightItem.y)
            {
                return leftItem.y < rightItem.y;
            }
            return leftItem.x < rightItem.x;
        }
    };
    std::sort(sortedItems.begin(), sortedItems.end(), SectionItemTopLeftLess());

    outKeys->clear();
    outKeys->reserve(sortedItems.size());
    for (std::size_t index = 0; index < sortedItems.size(); ++index)
    {
        Item* item = sortedItems[index].item;
        const std::string key = NormalizeSearchText(ResolveCanonicalItemName(item));
        outKeys->push_back(key);
    }

    return !outKeys->empty();
}

bool TryExtractSearchKeysFromInventory(Inventory* inventory, std::vector<std::string>* outKeys)
{
    if (inventory == 0 || outKeys == 0)
    {
        return false;
    }

    outKeys->clear();

    lektor<InventorySection*>& allSections = inventory->getAllSections();
    if (allSections.valid() && allSections.size() > 0)
    {
        std::vector<std::string> mergedSectionKeys;
        for (uint32_t sectionIndex = 0; sectionIndex < allSections.size(); ++sectionIndex)
        {
            InventorySection* section = allSections[sectionIndex];
            std::vector<std::string> sectionKeys;
            if (!TryExtractSearchKeysFromInventorySection(section, &sectionKeys))
            {
                continue;
            }

            for (std::size_t keyIndex = 0; keyIndex < sectionKeys.size(); ++keyIndex)
            {
                if (!sectionKeys[keyIndex].empty())
                {
                    mergedSectionKeys.push_back(sectionKeys[keyIndex]);
                }
            }
        }

        if (!mergedSectionKeys.empty())
        {
            outKeys->swap(mergedSectionKeys);
            return true;
        }
    }

    const lektor<Item*>& allItems = inventory->getAllItems();
    if (!allItems.valid() || allItems.size() == 0)
    {
        return false;
    }

    outKeys->reserve(allItems.size());
    for (uint32_t index = 0; index < allItems.size(); ++index)
    {
        const std::string key = NormalizeSearchText(ResolveCanonicalItemName(allItems[index]));
        outKeys->push_back(key);
    }

    return !outKeys->empty();
}

void AddCandidateRootObjectUnique(std::vector<RootObject*>* candidates, RootObject* candidate)
{
    if (candidates == 0 || candidate == 0)
    {
        return;
    }

    for (std::size_t index = 0; index < candidates->size(); ++index)
    {
        if ((*candidates)[index] == candidate)
        {
            return;
        }
    }

    candidates->push_back(candidate);
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

bool TryResolveCharacterInventoryVisible(Character* character, bool* visibleOut)
{
    if (visibleOut == 0)
    {
        return false;
    }

    *visibleOut = false;
    if (character == 0)
    {
        return true;
    }

    __try
    {
        if (character->inventory != 0 && character->inventory->isVisible())
        {
            *visibleOut = true;
            return true;
        }

        *visibleOut = character->isInventoryVisible();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

struct InventoryCandidateInfo
{
    Inventory* inventory;
    std::string source;
    bool traderPreferred;
    bool visible;
    int priorityBias;
};

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
    case SHOP_TRADER_CLASS:
        return "SHOP_TRADER_CLASS";
    default:
        return "OTHER";
    }
}

struct QuantityNameKey
{
    int quantity;
    std::string key;
};

bool IsShopCounterCandidateSource(const std::string& sourceLower)
{
    if (sourceLower.empty())
    {
        return false;
    }

    return sourceLower.find("shop counter") != std::string::npos
        || sourceLower.find("owner=shop counter") != std::string::npos
        || sourceLower.find("trader_furniture_shop") != std::string::npos;
}

bool IsTraderAnchoredCandidateSource(const std::string& sourceLower)
{
    if (sourceLower.empty())
    {
        return false;
    }

    if (sourceLower.find("self inventory true") != std::string::npos
        || sourceLower.find("trader home caption") != std::string::npos
        || sourceLower.find("caption trader") != std::string::npos
        || sourceLower.find("dialog target") != std::string::npos
        || sourceLower.find("widget") != std::string::npos)
    {
        return true;
    }

    return sourceLower.find("active char") != std::string::npos
        && sourceLower.find("trader true") != std::string::npos;
}

bool IsRiskyCoverageFallbackSource(const std::string& sourceLower)
{
    if (sourceLower.empty())
    {
        return false;
    }

    if (sourceLower.find("active char") != std::string::npos
        && sourceLower.find("trader false") != std::string::npos)
    {
        return true;
    }

    return sourceLower.find("nearby shop") != std::string::npos
        || sourceLower.find("nearby caption") != std::string::npos
        || sourceLower.find("nearby dialog") != std::string::npos
        || sourceLower.find("nearby world") != std::string::npos
        || sourceLower.find("root candidate") != std::string::npos;
}

std::size_t AbsoluteDiffSize(std::size_t left, std::size_t right)
{
    return left > right ? left - right : right - left;
}

std::string StripInventorySourceDiagnostics(const std::string& source)
{
    static const char* kSuffixTokens[] =
    {
        " aligned_matches=",
        " query_matches=",
        " coverage_fallback=",
        " non_empty=",
        " nearby_shop_candidates=",
        " nearby_shop_scanned=",
        " nearby_shop_with_inventory=",
        " nearby_focused=",
        " nearby_candidates=",
        " nearby_scanned=",
        " nearby_with_inventory="
    };

    std::size_t cut = source.size();
    for (std::size_t i = 0; i < sizeof(kSuffixTokens) / sizeof(kSuffixTokens[0]); ++i)
    {
        const std::size_t tokenPos = source.find(kSuffixTokens[i]);
        if (tokenPos != std::string::npos && tokenPos < cut)
        {
            cut = tokenPos;
        }
    }

    while (cut > 0 && source[cut - 1] == ' ')
    {
        --cut;
    }
    return source.substr(0, cut);
}

std::string BuildKeysetSourceId(const std::string& source)
{
    return NormalizeSearchText(StripInventorySourceDiagnostics(source));
}

void ClearLockedKeysetSource()
{
    g_lockedKeysetTraderParent = 0;
    g_lockedKeysetStage.clear();
    g_lockedKeysetSourceId.clear();
    g_lockedKeysetSourcePreview.clear();
    g_lockedKeysetExpectedCount = 0;
    g_lastKeysetLockSignature.clear();
}

bool IsPanelBindingConfidentForExpected(
    const TraderPanelInventoryBinding& binding,
    std::size_t expectedEntryCount)
{
    if (binding.inventory == 0 || !IsInventoryPointerValidSafe(binding.inventory))
    {
        return false;
    }

    const std::size_t baselineExpected =
        expectedEntryCount > 0 ? expectedEntryCount : binding.expectedEntryCount;
    if (baselineExpected == 0 || binding.nonEmptyKeyCount == 0)
    {
        return false;
    }

    if (AbsoluteDiffSize(binding.expectedEntryCount, baselineExpected) > 6)
    {
        return false;
    }

    if (baselineExpected >= 8 && binding.nonEmptyKeyCount * 2 < baselineExpected)
    {
        return false;
    }

    return true;
}

void ClearTraderPanelInventoryBindings()
{
    g_traderPanelInventoryBindings.clear();
    g_lastPanelBindingSignature.clear();
    g_lastPanelBindingRefusedSignature.clear();
    g_lastPanelBindingProbeSignature.clear();
}

void PruneTraderPanelInventoryBindings()
{
    if (g_traderPanelInventoryBindings.empty())
    {
        return;
    }

    std::vector<TraderPanelInventoryBinding> kept;
    kept.reserve(g_traderPanelInventoryBindings.size());
    for (std::size_t index = 0; index < g_traderPanelInventoryBindings.size(); ++index)
    {
        const TraderPanelInventoryBinding& binding = g_traderPanelInventoryBindings[index];
        if (binding.traderParent == 0
            || binding.entriesRoot == 0
            || binding.inventory == 0
            || !IsInventoryPointerValidSafe(binding.inventory))
        {
            continue;
        }

        if (g_updateTickCounter > binding.lastSeenTick
            && g_updateTickCounter - binding.lastSeenTick > 60000ULL)
        {
            continue;
        }

        kept.push_back(binding);
    }

    g_traderPanelInventoryBindings.swap(kept);
}

bool TryGetTraderPanelInventoryBinding(
    MyGUI::Widget* traderParent,
    MyGUI::Widget* entriesRoot,
    std::size_t expectedEntryCount,
    TraderPanelInventoryBinding* outBinding)
{
    if (traderParent == 0 || entriesRoot == 0)
    {
        return false;
    }

    PruneTraderPanelInventoryBindings();
    for (std::size_t index = 0; index < g_traderPanelInventoryBindings.size(); ++index)
    {
        TraderPanelInventoryBinding& binding = g_traderPanelInventoryBindings[index];
        if (binding.traderParent != traderParent || binding.entriesRoot != entriesRoot)
        {
            continue;
        }

        if (!IsPanelBindingConfidentForExpected(binding, expectedEntryCount))
        {
            return false;
        }

        binding.lastSeenTick = g_updateTickCounter;
        if (outBinding != 0)
        {
            *outBinding = binding;
        }
        return true;
    }

    return false;
}

void LogPanelBindingProbeOnce(
    MyGUI::Widget* traderParent,
    MyGUI::Widget* entriesRoot,
    std::size_t expectedEntryCount,
    const std::string& status,
    const TraderPanelInventoryBinding* binding)
{
    if (traderParent == 0 || entriesRoot == 0 || expectedEntryCount == 0)
    {
        return;
    }

    std::stringstream signature;
    signature << traderParent
              << "|" << entriesRoot
              << "|" << expectedEntryCount
              << "|" << status;
    if (signature.str() == g_lastPanelBindingProbeSignature)
    {
        return;
    }
    g_lastPanelBindingProbeSignature = signature.str();

    PruneSectionWidgetInventoryLinks();
    PruneInventoryGuiInventoryLinks();

    MyGUI::Widget* matchedSectionWidget = 0;
    Inventory* matchedSectionInventory = 0;
    std::string matchedSectionName;
    for (std::size_t index = 0; index < g_sectionWidgetInventoryLinks.size(); ++index)
    {
        const SectionWidgetInventoryLink& link = g_sectionWidgetInventoryLinks[index];
        if (link.sectionWidget == 0
            || link.inventory == 0
            || !IsInventoryPointerValidSafe(link.inventory))
        {
            continue;
        }

        if (link.sectionWidget == entriesRoot || IsDescendantOf(entriesRoot, link.sectionWidget))
        {
            matchedSectionWidget = link.sectionWidget;
            matchedSectionInventory = link.inventory;
            matchedSectionName = link.sectionName;
            break;
        }
    }

    std::stringstream line;
    line << "panel binding probe"
         << " panel_root=" << SafeWidgetName(traderParent)
         << "(" << traderParent << ")"
         << " entries_root=" << SafeWidgetName(entriesRoot)
         << "(" << entriesRoot << ")"
         << " expected_entries=" << expectedEntryCount
         << " status=" << status
         << " section_widget=" << SafeWidgetName(matchedSectionWidget)
         << "(" << matchedSectionWidget << ")"
         << " section_name=\"" << matchedSectionName << "\""
         << " section_inventory=" << matchedSectionInventory
         << " section_items=" << InventoryItemCountForLog(matchedSectionInventory)
         << " tracked_section_links=" << g_sectionWidgetInventoryLinks.size()
         << " tracked_gui_links=" << g_inventoryGuiInventoryLinks.size();

    if (binding != 0 && binding->inventory != 0 && IsInventoryPointerValidSafe(binding->inventory))
    {
        line << " resolved_inventory=" << binding->inventory
             << " resolved_items=" << InventoryItemCountForLog(binding->inventory)
             << " resolved_stage=" << binding->stage
             << " resolved_source=\"" << TruncateForLog(binding->source, 180) << "\"";
        LogInfoLine(line.str());

        std::vector<std::string> keys;
        if (TryExtractSearchKeysFromInventory(binding->inventory, &keys) && !keys.empty())
        {
            std::stringstream preview;
            preview << "panel binding probe key preview "
                    << BuildKeyPreviewForLog(keys, 14);
            LogInfoLine(preview.str());
        }
    }
    else
    {
        line << " resolved_inventory=0x0";
        LogWarnLine(line.str());
    }
}

void RegisterTraderPanelInventoryBinding(
    MyGUI::Widget* traderParent,
    MyGUI::Widget* entriesRoot,
    Inventory* inventory,
    const char* stage,
    const std::string& source,
    std::size_t expectedEntryCount,
    std::size_t nonEmptyKeyCount)
{
    if (traderParent == 0
        || entriesRoot == 0
        || inventory == 0
        || !IsInventoryPointerValidSafe(inventory))
    {
        return;
    }

    TraderPanelInventoryBinding updated;
    updated.traderParent = traderParent;
    updated.entriesRoot = entriesRoot;
    updated.inventory = inventory;
    updated.stage = stage == 0 ? "unknown" : stage;
    updated.source = source;
    updated.expectedEntryCount = expectedEntryCount;
    updated.nonEmptyKeyCount = nonEmptyKeyCount;
    updated.lastSeenTick = g_updateTickCounter;

    bool replaced = false;
    for (std::size_t index = 0; index < g_traderPanelInventoryBindings.size(); ++index)
    {
        TraderPanelInventoryBinding& binding = g_traderPanelInventoryBindings[index];
        if (binding.traderParent != traderParent || binding.entriesRoot != entriesRoot)
        {
            continue;
        }

        binding = updated;
        replaced = true;
        break;
    }
    if (!replaced)
    {
        g_traderPanelInventoryBindings.push_back(updated);
    }

    std::stringstream signature;
    signature << traderParent
              << "|" << entriesRoot
              << "|" << inventory
              << "|stage=" << updated.stage
              << "|expected=" << expectedEntryCount
              << "|non_empty=" << nonEmptyKeyCount;
    if (signature.str() != g_lastPanelBindingSignature)
    {
        std::stringstream line;
        line << "panel inventory binding updated"
             << " stage=" << updated.stage
             << " expected=" << expectedEntryCount
             << " non_empty=" << nonEmptyKeyCount
             << " parent=" << SafeWidgetName(traderParent)
             << " entries_root=" << SafeWidgetName(entriesRoot)
             << " source=\"" << TruncateForLog(source, 180) << "\"";
        LogInfoLine(line.str());
        g_lastPanelBindingSignature = signature.str();
    }
}

void AddInventoryCandidateUnique(
    std::vector<InventoryCandidateInfo>* candidates,
    Inventory* inventory,
    const std::string& source,
    bool traderPreferred,
    bool visible,
    int priorityBias = 0)
{
    if (candidates == 0 || inventory == 0)
    {
        return;
    }

    for (std::size_t index = 0; index < candidates->size(); ++index)
    {
        InventoryCandidateInfo& existing = (*candidates)[index];
        if (existing.inventory == inventory)
        {
            if (traderPreferred)
            {
                existing.traderPreferred = true;
            }
            if (visible)
            {
                existing.visible = true;
            }
            if (priorityBias > existing.priorityBias)
            {
                existing.priorityBias = priorityBias;
                existing.source = source;
            }
            return;
        }
    }

    InventoryCandidateInfo info;
    info.inventory = inventory;
    info.source = source;
    info.traderPreferred = traderPreferred;
    info.visible = visible;
    info.priorityBias = priorityBias;
    candidates->push_back(info);
}

void AddBuildingInventoryCandidate(
    Building* building,
    const char* sourceTag,
    const std::string& traderName,
    bool traderPreferred,
    int priorityBias,
    std::vector<InventoryCandidateInfo>* outCandidates)
{
    if (building == 0 || outCandidates == 0)
    {
        return;
    }

    Inventory* inventory = building->getInventory();
    if (inventory == 0)
    {
        return;
    }

    std::stringstream source;
    source << (sourceTag == 0 ? "trader_owned" : sourceTag)
           << ":" << traderName
           << " owner=" << RootObjectDisplayNameForLog(building)
           << " visible=" << (inventory->isVisible() ? "true" : "false")
           << " items=" << InventoryItemCountForLog(inventory);
    AddInventoryCandidateUnique(
        outCandidates,
        inventory,
        source.str(),
        traderPreferred,
        inventory->isVisible(),
        priorityBias);
}

bool CollectTraderOwnershipInventoryCandidates(
    Character* trader,
    int captionScore,
    const char* sourcePrefix,
    std::vector<InventoryCandidateInfo>* outCandidates)
{
    if (trader == 0 || outCandidates == 0)
    {
        return false;
    }

    Ownerships* ownerships = trader->getOwnerships();
    if (ownerships == 0)
    {
        return false;
    }

    const std::string traderName = CharacterNameForLog(trader);
    const int scoreBias = captionScore > 0 ? (captionScore / 12) : 0;
    bool addedAny = false;

    bool traderInventoryVisible = false;
    TryResolveCharacterInventoryVisible(trader, &traderInventoryVisible);
    if (trader->inventory != 0)
    {
        std::stringstream selfSource;
        selfSource << (sourcePrefix == 0 ? "trader_owned" : sourcePrefix)
                   << ":" << traderName
                   << " self_inventory=true"
                   << " visible=" << (traderInventoryVisible ? "true" : "false")
                   << " items=" << InventoryItemCountForLog(trader->inventory);
        AddInventoryCandidateUnique(
            outCandidates,
            trader->inventory,
            selfSource.str(),
            true,
            traderInventoryVisible,
            4200 + scoreBias);
        addedAny = true;
    }

    lektor<Building*> shopFurniture;
    ownerships->getHomeFurnitureOfType(shopFurniture, BF_SHOP);
    if (shopFurniture.size() == 0)
    {
        ownerships->getBuildingsWithFunction(shopFurniture, BF_SHOP);
    }
    for (std::size_t index = 0; index < shopFurniture.size(); ++index)
    {
        AddBuildingInventoryCandidate(
            shopFurniture[static_cast<uint32_t>(index)],
            "trader_furniture_shop",
            traderName,
            true,
            3200 + scoreBias,
            outCandidates);
        addedAny = true;
    }

    lektor<Building*> generalStorageFurniture;
    ownerships->getHomeFurnitureOfType(generalStorageFurniture, BF_GENERAL_STORAGE);
    for (std::size_t index = 0; index < generalStorageFurniture.size(); ++index)
    {
        AddBuildingInventoryCandidate(
            generalStorageFurniture[static_cast<uint32_t>(index)],
            "trader_furniture_storage",
            traderName,
            true,
            1800 + scoreBias,
            outCandidates);
        addedAny = true;
    }

    lektor<Building*> resourceStorageFurniture;
    ownerships->getHomeFurnitureOfType(resourceStorageFurniture, BF_RESOURCE_STORAGE);
    for (std::size_t index = 0; index < resourceStorageFurniture.size(); ++index)
    {
        AddBuildingInventoryCandidate(
            resourceStorageFurniture[static_cast<uint32_t>(index)],
            "trader_furniture_resource",
            traderName,
            true,
            1500 + scoreBias,
            outCandidates);
        addedAny = true;
    }

    return addedAny;
}

void CollectNearbyInventoryCandidates(
    const Ogre::Vector3& center,
    std::vector<InventoryCandidateInfo>* outCandidates,
    std::size_t* scannedObjectsOut,
    std::size_t* inventoryObjectsOut,
    float scanRadius = 900.0f,
    int maxObjectsPerType = 512,
    int priorityBiasBase = 0,
    const char* sourcePrefix = "nearby")
{
    if (outCandidates == 0 || ou == 0)
    {
        return;
    }

    if (scannedObjectsOut != 0)
    {
        *scannedObjectsOut = 0;
    }
    if (inventoryObjectsOut != 0)
    {
        *inventoryObjectsOut = 0;
    }

    const itemType scanTypes[] = { BUILDING, CONTAINER, ITEM, SHOP_TRADER_CLASS };

    for (std::size_t typeIndex = 0; typeIndex < sizeof(scanTypes) / sizeof(scanTypes[0]); ++typeIndex)
    {
        const itemType scanType = scanTypes[typeIndex];
        lektor<RootObject*> nearbyObjects;
        ou->getObjectsWithinSphere(nearbyObjects, center, scanRadius, scanType, maxObjectsPerType, 0);
        if (!nearbyObjects.valid() || nearbyObjects.size() == 0)
        {
            continue;
        }

        for (lektor<RootObject*>::const_iterator iter = nearbyObjects.begin(); iter != nearbyObjects.end(); ++iter)
        {
            RootObject* object = *iter;
            if (object == 0)
            {
                continue;
            }

            if (scannedObjectsOut != 0)
            {
                ++(*scannedObjectsOut);
            }

            Inventory* inventory = object->getInventory();
            if (inventory == 0)
            {
                continue;
            }

            if (inventoryObjectsOut != 0)
            {
                ++(*inventoryObjectsOut);
            }

            const float distanceSq = object->getPosition().squaredDistance(center);
            const float radiusSq = scanRadius * scanRadius;
            int proximityBias = priorityBiasBase;
            if (radiusSq > 1.0f)
            {
                const float normalizedDistance = distanceSq / radiusSq;
                if (normalizedDistance <= 1.0f)
                {
                    proximityBias += static_cast<int>((1.0f - normalizedDistance) * 600.0f);
                }
                else
                {
                    proximityBias -= static_cast<int>((normalizedDistance - 1.0f) * 160.0f);
                }
            }

            std::stringstream src;
            src << (sourcePrefix == 0 ? "nearby" : sourcePrefix)
                << ":" << ItemTypeNameForLog(scanType)
                << ":" << RootObjectDisplayNameForLog(object)
                << " visible=" << (inventory->isVisible() ? "true" : "false")
                << " items=" << InventoryItemCountForLog(inventory)
                << " dist2=" << static_cast<int>(distanceSq)
                << " radius=" << static_cast<int>(scanRadius);
            AddInventoryCandidateUnique(
                outCandidates,
                inventory,
                src.str(),
                false,
                inventory->isVisible(),
                proximityBias);
        }
    }
}

bool TryExtractQuantityNameKeysFromInventorySection(
    InventorySection* section,
    std::vector<QuantityNameKey>* outKeys)
{
    if (section == 0 || outKeys == 0)
    {
        return false;
    }

    const Ogre::vector<InventorySection::SectionItem>::type& sectionItems = section->getItems();
    if (sectionItems.empty())
    {
        return false;
    }

    std::vector<InventorySection::SectionItem> sortedItems(sectionItems.begin(), sectionItems.end());
    struct SectionItemTopLeftLess
    {
        bool operator()(const InventorySection::SectionItem& leftItem, const InventorySection::SectionItem& rightItem) const
        {
            if (leftItem.y != rightItem.y)
            {
                return leftItem.y < rightItem.y;
            }
            return leftItem.x < rightItem.x;
        }
    };
    std::sort(sortedItems.begin(), sortedItems.end(), SectionItemTopLeftLess());

    outKeys->clear();
    outKeys->reserve(sortedItems.size());
    for (std::size_t index = 0; index < sortedItems.size(); ++index)
    {
        Item* item = sortedItems[index].item;
        if (item == 0)
        {
            continue;
        }

        const std::string key = NormalizeSearchText(ResolveCanonicalItemName(item));
        if (key.empty())
        {
            continue;
        }

        QuantityNameKey hint;
        hint.quantity = item->quantity;
        hint.key = key;
        outKeys->push_back(hint);
    }

    return !outKeys->empty();
}

bool TryExtractQuantityNameKeysFromInventory(
    Inventory* inventory,
    std::vector<QuantityNameKey>* outKeys)
{
    if (inventory == 0 || outKeys == 0)
    {
        return false;
    }

    outKeys->clear();

    lektor<InventorySection*>& allSections = inventory->getAllSections();
    if (allSections.valid() && allSections.size() > 0)
    {
        std::vector<QuantityNameKey> mergedSectionKeys;
        for (uint32_t sectionIndex = 0; sectionIndex < allSections.size(); ++sectionIndex)
        {
            InventorySection* section = allSections[sectionIndex];
            std::vector<QuantityNameKey> sectionKeys;
            if (!TryExtractQuantityNameKeysFromInventorySection(section, &sectionKeys))
            {
                continue;
            }

            for (std::size_t keyIndex = 0; keyIndex < sectionKeys.size(); ++keyIndex)
            {
                if (!sectionKeys[keyIndex].key.empty())
                {
                    mergedSectionKeys.push_back(sectionKeys[keyIndex]);
                }
            }
        }

        if (!mergedSectionKeys.empty())
        {
            outKeys->swap(mergedSectionKeys);
            return true;
        }
    }

    const lektor<Item*>& allItems = inventory->getAllItems();
    if (!allItems.valid() || allItems.size() == 0)
    {
        return false;
    }

    outKeys->reserve(allItems.size());
    for (uint32_t index = 0; index < allItems.size(); ++index)
    {
        Item* item = allItems[index];
        if (item == 0)
        {
            continue;
        }

        const std::string key = NormalizeSearchText(ResolveCanonicalItemName(item));
        if (key.empty())
        {
            continue;
        }

        QuantityNameKey hint;
        hint.quantity = item->quantity;
        hint.key = key;
        outKeys->push_back(hint);
    }

    return !outKeys->empty();
}

std::string ResolveUniqueQuantityNameHint(const std::vector<QuantityNameKey>& keys, int quantity)
{
    if (quantity <= 0 || keys.empty())
    {
        return "";
    }

    std::string match;
    for (std::size_t index = 0; index < keys.size(); ++index)
    {
        const QuantityNameKey& hint = keys[index];
        if (hint.quantity != quantity || hint.key.empty())
        {
            continue;
        }

        if (match.empty())
        {
            match = hint.key;
            continue;
        }

        if (match != hint.key)
        {
            return "";
        }
    }

    return match;
}

std::string ResolveTopQuantityNameHints(
    const std::vector<QuantityNameKey>& keys,
    int quantity,
    std::size_t maxHints)
{
    if (quantity <= 0 || keys.empty() || maxHints == 0)
    {
        return "";
    }

    struct NameScore
    {
        std::string key;
        int count;
    };

    std::vector<NameScore> scores;
    for (std::size_t index = 0; index < keys.size(); ++index)
    {
        const QuantityNameKey& quantityName = keys[index];
        if (quantityName.quantity != quantity || quantityName.key.empty())
        {
            continue;
        }

        bool merged = false;
        for (std::size_t existing = 0; existing < scores.size(); ++existing)
        {
            if (scores[existing].key == quantityName.key)
            {
                ++scores[existing].count;
                merged = true;
                break;
            }
        }

        if (!merged)
        {
            NameScore score;
            score.key = quantityName.key;
            score.count = 1;
            scores.push_back(score);
        }
    }

    if (scores.empty())
    {
        return "";
    }

    struct NameScoreGreater
    {
        bool operator()(const NameScore& left, const NameScore& right) const
        {
            if (left.count != right.count)
            {
                return left.count > right.count;
            }
            return left.key < right.key;
        }
    };
    std::sort(scores.begin(), scores.end(), NameScoreGreater());

    std::string merged;
    const std::size_t limit = scores.size() < maxHints ? scores.size() : maxHints;
    for (std::size_t index = 0; index < limit; ++index)
    {
        if (!merged.empty())
        {
            merged.push_back(' ');
        }
        merged.append(scores[index].key);
    }

    return merged;
}

bool BuildAlignedInventoryNameHintsByQuantity(
    const std::vector<int>& uiQuantities,
    const std::vector<QuantityNameKey>& inventoryQuantityNameKeys,
    std::vector<std::string>* outAlignedNames)
{
    if (outAlignedNames == 0)
    {
        return false;
    }

    outAlignedNames->assign(uiQuantities.size(), "");
    if (uiQuantities.empty() || inventoryQuantityNameKeys.empty())
    {
        return false;
    }

    const std::size_t n = uiQuantities.size();
    const std::size_t m = inventoryQuantityNameKeys.size();
    const int gapScore = -2;
    const int mismatchScore = -3;
    const int matchScore = 8;

    std::vector<int> dp((n + 1) * (m + 1), 0);
    std::vector<unsigned char> dir((n + 1) * (m + 1), 0);

    for (std::size_t i = 1; i <= n; ++i)
    {
        dp[i * (m + 1)] = static_cast<int>(i) * gapScore;
        dir[i * (m + 1)] = 1;
    }
    for (std::size_t j = 1; j <= m; ++j)
    {
        dp[j] = static_cast<int>(j) * gapScore;
        dir[j] = 2;
    }

    for (std::size_t i = 1; i <= n; ++i)
    {
        for (std::size_t j = 1; j <= m; ++j)
        {
            const int uiQuantity = uiQuantities[i - 1];
            const int inventoryQuantity = inventoryQuantityNameKeys[j - 1].quantity;
            const bool quantityMatch = uiQuantity > 0 && uiQuantity == inventoryQuantity;

            const int diag = dp[(i - 1) * (m + 1) + (j - 1)] + (quantityMatch ? matchScore : mismatchScore);
            const int up = dp[(i - 1) * (m + 1) + j] + gapScore;
            const int left = dp[i * (m + 1) + (j - 1)] + gapScore;

            unsigned char bestDir = 0;
            int bestScore = diag;
            if (up > bestScore)
            {
                bestScore = up;
                bestDir = 1;
            }
            if (left > bestScore)
            {
                bestScore = left;
                bestDir = 2;
            }

            // Prefer taking exact quantity matches on ties.
            if (quantityMatch && diag == bestScore)
            {
                bestDir = 0;
            }

            dp[i * (m + 1) + j] = bestScore;
            dir[i * (m + 1) + j] = bestDir;
        }
    }

    std::size_t i = n;
    std::size_t j = m;
    std::size_t matchedCount = 0;
    while (i > 0 || j > 0)
    {
        unsigned char step = 0;
        if (i > 0 && j > 0)
        {
            step = dir[i * (m + 1) + j];
        }
        else if (i > 0)
        {
            step = 1;
        }
        else
        {
            step = 2;
        }

        if (step == 0 && i > 0 && j > 0)
        {
            const int uiQuantity = uiQuantities[i - 1];
            const QuantityNameKey& quantityName = inventoryQuantityNameKeys[j - 1];
            if (uiQuantity > 0
                && uiQuantity == quantityName.quantity
                && !quantityName.key.empty())
            {
                (*outAlignedNames)[i - 1] = quantityName.key;
                ++matchedCount;
            }
            --i;
            --j;
            continue;
        }

        if (step == 1 && i > 0)
        {
            --i;
            continue;
        }

        if (j > 0)
        {
            --j;
        }
    }

    return matchedCount > 0;
}

bool TryResolveInventoryNameKeysFromCandidates(
    const std::vector<InventoryCandidateInfo>& candidates,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys,
    Inventory** outSelectedInventory = 0)
{
    if (outKeys == 0 || candidates.empty())
    {
        return false;
    }
    if (outSelectedInventory != 0)
    {
        *outSelectedInventory = 0;
    }

    Inventory* bestInventory = 0;
    std::vector<std::string> bestKeys;
    std::vector<QuantityNameKey> bestQuantityKeys;
    std::string bestSource;
    int bestScore = -1000000;
    int bestAlignedMatches = 0;
    int bestAlignedTotal = 0;
    int bestQueryMatches = 0;
    int bestNonEmptyCount = 0;
    Inventory* bestCoverageInventory = 0;
    std::vector<std::string> bestCoverageKeys;
    std::vector<QuantityNameKey> bestCoverageQuantityKeys;
    std::string bestCoverageSource;
    int bestCoverageScore = -1000000;
    int bestCoverageAlignedMatches = 0;
    int bestCoverageAlignedTotal = 0;
    int bestCoverageQueryMatches = 0;
    int bestCoverageNonEmptyCount = 0;
    bool usedCoverageFallback = false;

    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        const InventoryCandidateInfo& candidate = candidates[index];
        if (candidate.inventory == 0)
        {
            continue;
        }

        std::vector<std::string> keys;
        if (!TryExtractSearchKeysFromInventory(candidate.inventory, &keys))
        {
            continue;
        }
        std::vector<QuantityNameKey> quantityKeys;
        TryExtractQuantityNameKeysFromInventory(candidate.inventory, &quantityKeys);
        int candidateAlignedMatches = 0;
        int candidateAlignedTotal = 0;
        int candidateQueryMatches = 0;

        const int keyCount = static_cast<int>(keys.size());
        const int nonEmptyKeyCount = static_cast<int>(CountNonEmptyKeys(keys));
        const int emptyKeyCount = keyCount - nonEmptyKeyCount;
        const int expected = static_cast<int>(expectedEntryCount);
        const int diff =
            nonEmptyKeyCount > expected
                ? nonEmptyKeyCount - expected
                : expected - nonEmptyKeyCount;
        const bool lowCoverage = expected > 0 && nonEmptyKeyCount * 2 < expected;
        const bool noUsableNames = nonEmptyKeyCount == 0;

        int score = 0;
        score += nonEmptyKeyCount * 28;
        score -= emptyKeyCount * 140;
        score -= diff * 28;
        if (diff == 0)
        {
            score += 2200;
        }
        else if (diff <= 1)
        {
            score += 900;
        }
        else if (lowCoverage)
        {
            score -= 2500;
        }
        if (candidate.traderPreferred)
        {
            score += 420;
        }
        if (candidate.visible)
        {
            score += 220;
        }
        if (noUsableNames)
        {
            score -= 5400;
        }

        score += candidate.priorityBias;

        const std::string sourceLower = NormalizeSearchText(candidate.source);
        if (sourceLower.find("nearby") != std::string::npos)
        {
            score += 120;
        }
        if (sourceLower.find("root candidate") != std::string::npos)
        {
            score -= 140;
        }

        if (!g_searchQueryNormalized.empty())
        {
            for (std::size_t keyIndex = 0; keyIndex < keys.size(); ++keyIndex)
            {
                if (keys[keyIndex].find(g_searchQueryNormalized) != std::string::npos)
                {
                    ++candidateQueryMatches;
                }
            }

            if (candidateQueryMatches > 0)
            {
                const int queryMatchBoost = lowCoverage ? 140 : 520;
                score += candidateQueryMatches * queryMatchBoost;
                if (candidateQueryMatches >= 2 && !lowCoverage)
                {
                    score += 260;
                }
                if (sourceLower.find("nearby") != std::string::npos)
                {
                    score += lowCoverage ? 40 : 220;
                }
            }
            else if (g_searchQueryNormalized.size() >= 2)
            {
                score -= 980;
            }
        }

        if (uiQuantities != 0 && !uiQuantities->empty() && !quantityKeys.empty())
        {
            int sequenceAlignedMatches = 0;
            {
                std::vector<std::string> alignedNames;
                if (BuildAlignedInventoryNameHintsByQuantity(*uiQuantities, quantityKeys, &alignedNames))
                {
                    for (std::size_t alignedIndex = 0; alignedIndex < alignedNames.size(); ++alignedIndex)
                    {
                        if (!alignedNames[alignedIndex].empty())
                        {
                            ++sequenceAlignedMatches;
                        }
                    }
                }
            }
            candidateAlignedMatches = sequenceAlignedMatches;
            candidateAlignedTotal = static_cast<int>(uiQuantities->size());
            score += sequenceAlignedMatches * 180;
            if (uiQuantities->size() >= 10 && sequenceAlignedMatches * 3 < static_cast<int>(uiQuantities->size()))
            {
                score -= 2800;
            }

            const std::size_t quantityCompareCount =
                uiQuantities->size() < quantityKeys.size()
                    ? uiQuantities->size()
                    : quantityKeys.size();

            int exactQuantityPositionMatches = 0;
            for (std::size_t q = 0; q < quantityCompareCount; ++q)
            {
                const int uiQuantity = (*uiQuantities)[q];
                if (uiQuantity > 0 && quantityKeys[q].quantity == uiQuantity)
                {
                    ++exactQuantityPositionMatches;
                }
            }
            score += exactQuantityPositionMatches * 220;

            std::vector<int> uiSorted;
            uiSorted.reserve(uiQuantities->size());
            for (std::size_t q = 0; q < uiQuantities->size(); ++q)
            {
                const int uiQuantity = (*uiQuantities)[q];
                if (uiQuantity > 0)
                {
                    uiSorted.push_back(uiQuantity);
                }
            }
            std::vector<int> candidateSorted;
            candidateSorted.reserve(quantityKeys.size());
            for (std::size_t q = 0; q < quantityKeys.size(); ++q)
            {
                const int candidateQuantity = quantityKeys[q].quantity;
                if (candidateQuantity > 0)
                {
                    candidateSorted.push_back(candidateQuantity);
                }
            }

            std::sort(uiSorted.begin(), uiSorted.end());
            std::sort(candidateSorted.begin(), candidateSorted.end());

            std::size_t uiIndex = 0;
            std::size_t candidateIndex = 0;
            int multisetQuantityMatches = 0;
            while (uiIndex < uiSorted.size() && candidateIndex < candidateSorted.size())
            {
                if (uiSorted[uiIndex] == candidateSorted[candidateIndex])
                {
                    ++multisetQuantityMatches;
                    ++uiIndex;
                    ++candidateIndex;
                }
                else if (uiSorted[uiIndex] < candidateSorted[candidateIndex])
                {
                    ++uiIndex;
                }
                else
                {
                    ++candidateIndex;
                }
            }
            score += multisetQuantityMatches * 40;
        }

        if (score > bestScore)
        {
            bestScore = score;
            bestInventory = candidate.inventory;
            bestSource = candidate.source;
            bestKeys.swap(keys);
            bestQuantityKeys.swap(quantityKeys);
            bestAlignedMatches = candidateAlignedMatches;
            bestAlignedTotal = candidateAlignedTotal;
            bestQueryMatches = candidateQueryMatches;
            bestNonEmptyCount = nonEmptyKeyCount;
        }

        const bool coverageCandidate = expected < 8 || !lowCoverage;
        if (coverageCandidate && score > bestCoverageScore)
        {
            bestCoverageScore = score;
            bestCoverageInventory = candidate.inventory;
            bestCoverageSource = candidate.source;
            bestCoverageKeys = keys;
            bestCoverageQuantityKeys = quantityKeys;
            bestCoverageAlignedMatches = candidateAlignedMatches;
            bestCoverageAlignedTotal = candidateAlignedTotal;
            bestCoverageQueryMatches = candidateQueryMatches;
            bestCoverageNonEmptyCount = nonEmptyKeyCount;
        }
    }

    if (bestInventory == 0 || bestKeys.empty())
    {
        return false;
    }

    if (expectedEntryCount >= 8
        && bestNonEmptyCount * 2 < static_cast<int>(expectedEntryCount)
        && bestCoverageInventory != 0
        && !bestCoverageKeys.empty())
    {
        bestInventory = bestCoverageInventory;
        bestSource = bestCoverageSource;
        bestKeys.swap(bestCoverageKeys);
        bestQuantityKeys.swap(bestCoverageQuantityKeys);
        bestScore = bestCoverageScore;
        bestAlignedMatches = bestCoverageAlignedMatches;
        bestAlignedTotal = bestCoverageAlignedTotal;
        bestQueryMatches = bestCoverageQueryMatches;
        bestNonEmptyCount = bestCoverageNonEmptyCount;
        usedCoverageFallback = true;
    }

    if (bestKeys.size() > expectedEntryCount)
    {
        bestKeys.resize(expectedEntryCount);
    }
    if (bestQuantityKeys.size() > expectedEntryCount)
    {
        bestQuantityKeys.resize(expectedEntryCount);
    }

    outKeys->swap(bestKeys);
    if (outSource != 0)
    {
        std::stringstream source;
        source << bestSource;
        if (bestAlignedTotal > 0)
        {
            source << " aligned_matches=" << bestAlignedMatches
                   << "/" << bestAlignedTotal;
        }
        if (!g_searchQueryNormalized.empty())
        {
            source << " query_matches=" << bestQueryMatches;
        }
        if (usedCoverageFallback)
        {
            source << " coverage_fallback=true";
        }
        source << " non_empty=" << bestNonEmptyCount;
        *outSource = source.str();
    }
    if (outQuantityKeys != 0)
    {
        outQuantityKeys->swap(bestQuantityKeys);
    }
    if (outSelectedInventory != 0)
    {
        *outSelectedInventory = bestInventory;
    }
    return true;
}

bool TryResolveTraderInventoryNameKeysFromWindowCaption(
    MyGUI::Widget* traderParent,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys)
{
    if (traderParent == 0 || outKeys == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    MyGUI::Window* owningWindow = FindOwningWindow(traderParent);
    if (owningWindow == 0)
    {
        return false;
    }

    const std::string windowCaption = owningWindow->getCaption().asUTF8();
    const std::string normalizedCaption = NormalizeSearchText(windowCaption);
    if (normalizedCaption.empty())
    {
        return false;
    }

    std::vector<InventoryCandidateInfo> inventoryCandidates;
    std::size_t captionMatchCount = 0;

    const ogre_unordered_set<Character*>::type& activeCharacters = ou->getCharacterUpdateList();
    for (ogre_unordered_set<Character*>::type::const_iterator it = activeCharacters.begin();
         it != activeCharacters.end();
         ++it)
    {
        Character* candidate = *it;
        if (candidate == 0 || candidate->inventory == 0 || !candidate->isATrader())
        {
            continue;
        }

        const std::string characterName = CharacterNameForLog(candidate);
        const std::string normalizedCharacterName = NormalizeSearchText(characterName);
        const int captionBias = ComputeCaptionNameMatchBias(normalizedCaption, normalizedCharacterName);
        if (captionBias <= 0)
        {
            continue;
        }

        bool inventoryVisible = false;
        TryResolveCharacterInventoryVisible(candidate, &inventoryVisible);

        std::stringstream src;
        src << "caption_trader:" << characterName
            << " visible=" << (inventoryVisible ? "true" : "false")
            << " caption_score=" << captionBias;
        AddInventoryCandidateUnique(
            &inventoryCandidates,
            candidate->inventory,
            src.str(),
            true,
            inventoryVisible,
            captionBias);
        ++captionMatchCount;
    }

    if (inventoryCandidates.empty())
    {
        return false;
    }

    const bool resolved = TryResolveInventoryNameKeysFromCandidates(
        inventoryCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys);
    if (!resolved || outSource == 0)
    {
        return resolved;
    }

    std::stringstream source;
    source << *outSource
           << " caption_matches=" << captionMatchCount
           << " caption=\"" << TruncateForLog(windowCaption, 48) << "\"";
    *outSource = source.str();
    return true;
}

bool TryResolveTraderInventoryNameKeysFromDialogue(
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys)
{
    if (outKeys == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    std::vector<Character*> playerCharacters;
    Character* selected = 0;

    selected = ou->player->selectedCharacter.getCharacter();
    if (selected != 0)
    {
        playerCharacters.push_back(selected);
    }

    const lektor<Character*>& allPlayerCharacters = ou->player->getAllPlayerCharacters();
    for (lektor<Character*>::const_iterator iter = allPlayerCharacters.begin(); iter != allPlayerCharacters.end(); ++iter)
    {
        Character* candidate = *iter;
        if (candidate == 0 || candidate == selected)
        {
            continue;
        }
        playerCharacters.push_back(candidate);
    }

    if (playerCharacters.empty())
    {
        return false;
    }

    std::vector<InventoryCandidateInfo> inventoryCandidates;

    for (std::size_t charIndex = 0; charIndex < playerCharacters.size(); ++charIndex)
    {
        Character* playerChar = playerCharacters[charIndex];
        if (playerChar == 0)
        {
            continue;
        }

        Dialogue* dialogue = playerChar->dialogue;
        if (dialogue == 0)
        {
            continue;
        }

        Character* target = dialogue->getConversationTarget().getCharacter();
        if (target == 0)
        {
            continue;
        }

        bool playerInventoryVisible = false;
        bool targetInventoryVisible = false;
        TryResolveCharacterInventoryVisible(playerChar, &playerInventoryVisible);
        TryResolveCharacterInventoryVisible(target, &targetInventoryVisible);

        const bool dialogActive = !dialogue->conversationHasEndedPrettyMuch();
        const bool engaged = playerChar->_isEngagedWithAPlayer || target->_isEngagedWithAPlayer;
        const bool targetIsTrader = target->isATrader();
        if (!dialogActive && !targetIsTrader)
        {
            continue;
        }
        if (!engaged && !targetIsTrader && !playerInventoryVisible && !targetInventoryVisible)
        {
            continue;
        }

        if (target->inventory != 0)
        {
            std::stringstream src;
            src << "dialog_target:" << RootObjectDisplayNameForLog(target)
                << " trader=" << (target->isATrader() ? "true" : "false")
                << " visible=" << (targetInventoryVisible ? "true" : "false");
            AddInventoryCandidateUnique(
                &inventoryCandidates,
                target->inventory,
                src.str(),
                target->isATrader(),
                targetInventoryVisible);
        }

        if (playerChar->inventory != 0)
        {
            std::stringstream src;
            src << "dialog_player:" << RootObjectDisplayNameForLog(playerChar)
                << " trader=" << (playerChar->isATrader() ? "true" : "false")
                << " visible=" << (playerInventoryVisible ? "true" : "false");
            AddInventoryCandidateUnique(
                &inventoryCandidates,
                playerChar->inventory,
                src.str(),
                false,
                playerInventoryVisible);
        }
    }

    return TryResolveInventoryNameKeysFromCandidates(
        inventoryCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys);
}

bool TryResolveTraderInventoryNameKeysFromActiveCharacters(
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys)
{
    if (outKeys == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    Character* selectedCharacter = ou->player->selectedCharacter.getCharacter();
    std::vector<InventoryCandidateInfo> inventoryCandidates;

    const ogre_unordered_set<Character*>::type& activeCharacters = ou->getCharacterUpdateList();
    for (ogre_unordered_set<Character*>::type::const_iterator it = activeCharacters.begin();
         it != activeCharacters.end();
         ++it)
    {
        Character* character = *it;
        if (character == 0 || character->inventory == 0)
        {
            continue;
        }

        bool inventoryVisible = false;
        TryResolveCharacterInventoryVisible(character, &inventoryVisible);

        const bool isSelected = character == selectedCharacter;
        const bool isTrader = character->isATrader();

        std::stringstream src;
        src << "active_char:" << CharacterNameForLog(character)
            << " trader=" << (isTrader ? "true" : "false")
            << " visible=" << (inventoryVisible ? "true" : "false")
            << " selected=" << (isSelected ? "true" : "false");
        AddInventoryCandidateUnique(
            &inventoryCandidates,
            character->inventory,
            src.str(),
            isTrader || isSelected,
            inventoryVisible || isSelected);
    }

    return TryResolveInventoryNameKeysFromCandidates(
        inventoryCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys);
}

void CollectWidgetChainInventoryCandidates(
    MyGUI::Widget* rootWidget,
    const char* sourcePrefix,
    int basePriorityBias,
    std::vector<InventoryCandidateInfo>* outCandidates)
{
    if (rootWidget == 0 || outCandidates == 0)
    {
        return;
    }

    MyGUI::Widget* current = rootWidget;
    for (int depth = 0; current != 0 && depth < 12; ++depth)
    {
        Inventory* inventory = ResolveWidgetInventoryPointer(current);
        if (inventory != 0)
        {
            RootObject* owner = inventory->getOwner();
            if (owner == 0)
            {
                owner = inventory->getCallbackObject();
            }

            std::stringstream source;
            source << (sourcePrefix == 0 ? "widget_inventory" : sourcePrefix)
                   << " root=" << SafeWidgetName(rootWidget)
                   << " via=" << SafeWidgetName(current)
                   << " depth=" << depth
                   << " owner=" << RootObjectDisplayNameForLog(owner)
                   << " visible=" << (inventory->isVisible() ? "true" : "false")
                   << " items=" << InventoryItemCountForLog(inventory);
            AddInventoryCandidateUnique(
                outCandidates,
                inventory,
                source.str(),
                true,
                inventory->isVisible(),
                basePriorityBias - depth * 90);
        }

        current = current->getParent();
    }
}

void CollectWidgetTreeInventoryCandidatesRecursive(
    MyGUI::Widget* widget,
    MyGUI::Widget* rootWidget,
    const char* sourcePrefix,
    int basePriorityBias,
    std::size_t depth,
    std::size_t maxDepth,
    std::size_t maxNodes,
    std::size_t* nodesVisited,
    std::vector<InventoryCandidateInfo>* outCandidates)
{
    if (widget == 0
        || rootWidget == 0
        || outCandidates == 0
        || nodesVisited == 0
        || *nodesVisited >= maxNodes)
    {
        return;
    }

    ++(*nodesVisited);

    Inventory* inventory = ResolveWidgetInventoryPointer(widget);
    if (inventory != 0)
    {
        RootObject* owner = inventory->getOwner();
        if (owner == 0)
        {
            owner = inventory->getCallbackObject();
        }

        std::stringstream source;
        source << (sourcePrefix == 0 ? "widget_tree" : sourcePrefix)
               << " root=" << SafeWidgetName(rootWidget)
               << " via=" << SafeWidgetName(widget)
               << " depth=" << depth
               << " owner=" << RootObjectDisplayNameForLog(owner)
               << " visible=" << (inventory->isVisible() ? "true" : "false")
               << " items=" << InventoryItemCountForLog(inventory);
        AddInventoryCandidateUnique(
            outCandidates,
            inventory,
            source.str(),
            true,
            inventory->isVisible(),
            basePriorityBias - static_cast<int>(depth) * 40);
    }

    if (depth >= maxDepth)
    {
        return;
    }

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        if (*nodesVisited >= maxNodes)
        {
            break;
        }

        CollectWidgetTreeInventoryCandidatesRecursive(
            widget->getChildAt(childIndex),
            rootWidget,
            sourcePrefix,
            basePriorityBias,
            depth + 1,
            maxDepth,
            maxNodes,
            nodesVisited,
            outCandidates);
    }
}

void CollectWidgetTreeInventoryCandidates(
    MyGUI::Widget* rootWidget,
    const char* sourcePrefix,
    int basePriorityBias,
    std::size_t maxDepth,
    std::size_t maxNodes,
    std::vector<InventoryCandidateInfo>* outCandidates)
{
    if (rootWidget == 0 || outCandidates == 0)
    {
        return;
    }

    std::size_t nodesVisited = 0;
    CollectWidgetTreeInventoryCandidatesRecursive(
        rootWidget,
        rootWidget,
        sourcePrefix,
        basePriorityBias,
        0,
        maxDepth,
        maxNodes,
        &nodesVisited,
        outCandidates);
}

InventoryGUI* TryGetInventoryGuiSafe(Inventory* inventory)
{
    if (inventory == 0)
    {
        return 0;
    }

    __try
    {
        return inventory->getInventoryGUI();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

bool IsInventoryPointerValidSafe(Inventory* inventory)
{
    if (inventory == 0)
    {
        return false;
    }

    __try
    {
        inventory->isVisible();
        inventory->getOwner();
        inventory->getAllItems();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool IsRootObjectPointerValidSafe(RootObject* object)
{
    if (object == 0)
    {
        return false;
    }

    __try
    {
        object->getInventory();
        object->data;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

Inventory* TryGetRootObjectInventorySafe(RootObject* object)
{
    if (object == 0)
    {
        return 0;
    }

    __try
    {
        return object->getInventory();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

bool TryGetInventoryOwnerPointersSafe(
    Inventory* inventory,
    RootObject** outOwner,
    RootObject** outCallbackObject)
{
    if (outOwner != 0)
    {
        *outOwner = 0;
    }
    if (outCallbackObject != 0)
    {
        *outCallbackObject = 0;
    }

    if (inventory == 0)
    {
        return false;
    }

    __try
    {
        if (outOwner != 0)
        {
            *outOwner = inventory->getOwner();
        }
        if (outCallbackObject != 0)
        {
            *outCallbackObject = inventory->getCallbackObject();
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (outOwner != 0)
        {
            *outOwner = 0;
        }
        if (outCallbackObject != 0)
        {
            *outCallbackObject = 0;
        }
        return false;
    }
}

void RegisterSectionWidgetInventoryLink(
    MyGUI::Widget* sectionWidget,
    Inventory* inventory,
    const std::string& sectionName)
{
    if (sectionWidget == 0 || inventory == 0 || !IsInventoryPointerValidSafe(inventory))
    {
        return;
    }

    const std::string widgetName = SafeWidgetName(sectionWidget);
    const std::size_t itemCount = InventoryItemCountForLog(inventory);
    for (std::size_t index = 0; index < g_sectionWidgetInventoryLinks.size(); ++index)
    {
        SectionWidgetInventoryLink& link = g_sectionWidgetInventoryLinks[index];
        if (link.sectionWidget != sectionWidget)
        {
            continue;
        }

        link.inventory = inventory;
        link.sectionName = sectionName;
        link.widgetName = widgetName;
        link.itemCount = itemCount;
        link.lastSeenTick = g_updateTickCounter;
        return;
    }

    SectionWidgetInventoryLink link;
    link.sectionWidget = sectionWidget;
    link.inventory = inventory;
    link.sectionName = sectionName;
    link.widgetName = widgetName;
    link.itemCount = itemCount;
    link.lastSeenTick = g_updateTickCounter;
    g_sectionWidgetInventoryLinks.push_back(link);
}

void PruneSectionWidgetInventoryLinks()
{
    if (g_sectionWidgetInventoryLinks.empty())
    {
        return;
    }

    std::vector<SectionWidgetInventoryLink> kept;
    kept.reserve(g_sectionWidgetInventoryLinks.size());
    for (std::size_t index = 0; index < g_sectionWidgetInventoryLinks.size(); ++index)
    {
        const SectionWidgetInventoryLink& link = g_sectionWidgetInventoryLinks[index];
        if (link.sectionWidget == 0 || link.inventory == 0 || !IsInventoryPointerValidSafe(link.inventory))
        {
            continue;
        }

        // Keep a long retention window so links survive while the trader window is open.
        if (g_updateTickCounter > link.lastSeenTick
            && g_updateTickCounter - link.lastSeenTick > 60000ULL)
        {
            continue;
        }

        kept.push_back(link);
    }

    g_sectionWidgetInventoryLinks.swap(kept);
}

void RegisterInventoryGuiInventoryLink(InventoryGUI* inventoryGui, Inventory* inventory)
{
    if (inventoryGui == 0 || inventory == 0 || !IsInventoryPointerValidSafe(inventory))
    {
        return;
    }

    RootObject* owner = inventory->getOwner();
    if (owner == 0)
    {
        owner = inventory->getCallbackObject();
    }

    const std::string ownerName = RootObjectDisplayNameForLog(owner);
    const std::size_t itemCount = InventoryItemCountForLog(inventory);
    for (std::size_t index = 0; index < g_inventoryGuiInventoryLinks.size(); ++index)
    {
        InventoryGuiInventoryLink& link = g_inventoryGuiInventoryLinks[index];
        if (link.inventoryGui != inventoryGui)
        {
            continue;
        }

        link.inventory = inventory;
        link.ownerName = ownerName;
        link.itemCount = itemCount;
        link.lastSeenTick = g_updateTickCounter;

        std::stringstream signature;
        signature << inventoryGui << "|" << inventory
                  << "|" << ownerName << "|" << itemCount;
        if (signature.str() != g_lastInventoryGuiBindingSignature)
        {
            std::stringstream line;
            line << "inventory layout gui binding"
                 << " inv_gui=" << inventoryGui
                 << " owner=" << ownerName
                 << " inv_items=" << itemCount;
            LogInfoLine(line.str());
            g_lastInventoryGuiBindingSignature = signature.str();
        }
        return;
    }

    InventoryGuiInventoryLink link;
    link.inventoryGui = inventoryGui;
    link.inventory = inventory;
    link.ownerName = ownerName;
    link.itemCount = itemCount;
    link.lastSeenTick = g_updateTickCounter;
    g_inventoryGuiInventoryLinks.push_back(link);

    std::stringstream signature;
    signature << inventoryGui << "|" << inventory
              << "|" << ownerName << "|" << itemCount;
    if (signature.str() != g_lastInventoryGuiBindingSignature)
    {
        std::stringstream line;
        line << "inventory layout gui binding"
             << " inv_gui=" << inventoryGui
             << " owner=" << ownerName
             << " inv_items=" << itemCount;
        LogInfoLine(line.str());
        g_lastInventoryGuiBindingSignature = signature.str();
    }
}

void PruneInventoryGuiInventoryLinks()
{
    if (g_inventoryGuiInventoryLinks.empty())
    {
        return;
    }

    std::vector<InventoryGuiInventoryLink> kept;
    kept.reserve(g_inventoryGuiInventoryLinks.size());
    for (std::size_t index = 0; index < g_inventoryGuiInventoryLinks.size(); ++index)
    {
        const InventoryGuiInventoryLink& link = g_inventoryGuiInventoryLinks[index];
        if (link.inventoryGui == 0 || link.inventory == 0 || !IsInventoryPointerValidSafe(link.inventory))
        {
            continue;
        }

        if (g_updateTickCounter > link.lastSeenTick
            && g_updateTickCounter - link.lastSeenTick > 60000ULL)
        {
            continue;
        }

        kept.push_back(link);
    }

    g_inventoryGuiInventoryLinks.swap(kept);
}

void ClearInventoryGuiInventoryLinks()
{
    g_inventoryGuiInventoryLinks.clear();
    g_lastInventoryGuiBindingSignature.clear();
}

bool TryReadPointerValueSafe(const void* base, std::size_t offset, const void** outValue)
{
    if (base == 0 || outValue == 0)
    {
        return false;
    }

    *outValue = 0;
    __try
    {
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(base);
        *outValue = *reinterpret_cast<const void* const*>(bytes + offset);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        *outValue = 0;
        return false;
    }
}

const char* InventoryGuiBackPointerKindLabel(InventoryGuiBackPointerKind kind)
{
    switch (kind)
    {
    case InventoryGuiBackPointerKind_DirectInventory:
        return "inventory";
    case InventoryGuiBackPointerKind_OwnerObject:
        return "owner";
    case InventoryGuiBackPointerKind_CallbackObject:
        return "callback";
    default:
        return "unknown";
    }
}

int InventoryGuiBackPointerKindPriority(InventoryGuiBackPointerKind kind)
{
    switch (kind)
    {
    case InventoryGuiBackPointerKind_DirectInventory:
        return 0;
    case InventoryGuiBackPointerKind_OwnerObject:
        return 1;
    case InventoryGuiBackPointerKind_CallbackObject:
        return 2;
    default:
        return 3;
    }
}

void RecordInventoryGuiBackPointerOffsetHit(
    std::vector<InventoryGuiBackPointerOffset>* hits,
    std::size_t offset,
    InventoryGuiBackPointerKind kind)
{
    if (hits == 0)
    {
        return;
    }

    for (std::size_t index = 0; index < hits->size(); ++index)
    {
        InventoryGuiBackPointerOffset& hit = (*hits)[index];
        if (hit.offset != offset || hit.kind != kind)
        {
            continue;
        }

        ++hit.hits;
        return;
    }

    InventoryGuiBackPointerOffset hit;
    hit.offset = offset;
    hit.kind = kind;
    hit.hits = 1;
    hits->push_back(hit);
}

void LearnInventoryGuiBackPointerOffsets()
{
    if (g_inventoryGuiInventoryLinks.empty())
    {
        return;
    }

    const std::size_t kMaxScanOffset = 0x400;
    std::vector<InventoryGuiBackPointerOffset> offsetHits;
    std::size_t validatedLinks = 0;
    for (std::size_t linkIndex = 0; linkIndex < g_inventoryGuiInventoryLinks.size(); ++linkIndex)
    {
        const InventoryGuiInventoryLink& link = g_inventoryGuiInventoryLinks[linkIndex];
        if (link.inventoryGui == 0
            || link.inventory == 0
            || !IsInventoryPointerValidSafe(link.inventory))
        {
            continue;
        }

        InventoryGUI* resolvedGui = TryGetInventoryGuiSafe(link.inventory);
        if (resolvedGui == 0 || resolvedGui != link.inventoryGui)
        {
            continue;
        }

        ++validatedLinks;

        RootObject* owner = 0;
        RootObject* callbackObject = 0;
        TryGetInventoryOwnerPointersSafe(link.inventory, &owner, &callbackObject);

        for (std::size_t offset = sizeof(void*); offset <= kMaxScanOffset; offset += sizeof(void*))
        {
            const void* value = 0;
            if (!TryReadPointerValueSafe(link.inventoryGui, offset, &value) || value == 0)
            {
                continue;
            }

            if (value == link.inventory)
            {
                RecordInventoryGuiBackPointerOffsetHit(
                    &offsetHits,
                    offset,
                    InventoryGuiBackPointerKind_DirectInventory);
            }

            if (owner != 0 && value == owner)
            {
                Inventory* ownerInventory = TryGetRootObjectInventorySafe(owner);
                if (ownerInventory == link.inventory)
                {
                    RecordInventoryGuiBackPointerOffsetHit(
                        &offsetHits,
                        offset,
                        InventoryGuiBackPointerKind_OwnerObject);
                }
            }

            if (callbackObject != 0 && callbackObject != owner && value == callbackObject)
            {
                Inventory* callbackInventory = TryGetRootObjectInventorySafe(callbackObject);
                if (callbackInventory == link.inventory)
                {
                    RecordInventoryGuiBackPointerOffsetHit(
                        &offsetHits,
                        offset,
                        InventoryGuiBackPointerKind_CallbackObject);
                }
            }
        }
    }

    if (offsetHits.empty())
    {
        g_inventoryGuiBackPointerOffsets.clear();
        std::stringstream signature;
        signature << "none|tracked=" << g_inventoryGuiInventoryLinks.size()
                  << "|validated=" << validatedLinks
                  << "|scan=0x" << std::hex << std::uppercase << kMaxScanOffset;
        if (signature.str() != g_lastInventoryGuiBackPointerLearningSignature)
        {
            std::stringstream line;
            line << "inventory gui back-pointer offsets not learned"
                 << " tracked_links=" << g_inventoryGuiInventoryLinks.size()
                 << " validated_links=" << validatedLinks
                 << " scan_limit=0x" << std::hex << std::uppercase << kMaxScanOffset << std::dec;
            LogWarnLine(line.str());
            g_lastInventoryGuiBackPointerLearningSignature = signature.str();
        }
        return;
    }

    std::sort(
        offsetHits.begin(),
        offsetHits.end(),
        [](const InventoryGuiBackPointerOffset& left, const InventoryGuiBackPointerOffset& right) -> bool
        {
            if (left.hits != right.hits)
            {
                return left.hits > right.hits;
            }
            const int leftPriority = InventoryGuiBackPointerKindPriority(left.kind);
            const int rightPriority = InventoryGuiBackPointerKindPriority(right.kind);
            if (leftPriority != rightPriority)
            {
                return leftPriority < rightPriority;
            }
            return left.offset < right.offset;
        });

    g_inventoryGuiBackPointerOffsets.swap(offsetHits);

    std::stringstream signature;
    for (std::size_t index = 0; index < g_inventoryGuiBackPointerOffsets.size(); ++index)
    {
        const InventoryGuiBackPointerOffset& learned = g_inventoryGuiBackPointerOffsets[index];
        signature << InventoryGuiBackPointerKindLabel(learned.kind)
                  << ":" << learned.offset
                  << ":" << learned.hits << "|";
    }

    if (signature.str() != g_lastInventoryGuiBackPointerLearningSignature)
    {
        std::stringstream line;
        line << "inventory gui back-pointer offsets learned";
        const std::size_t previewCount =
            g_inventoryGuiBackPointerOffsets.size() < 8 ? g_inventoryGuiBackPointerOffsets.size() : 8;
        for (std::size_t index = 0; index < previewCount; ++index)
        {
            const InventoryGuiBackPointerOffset& learned = g_inventoryGuiBackPointerOffsets[index];
            line << " offset" << index << "=0x"
                 << std::hex << std::uppercase << learned.offset
                 << std::dec
                 << "(" << InventoryGuiBackPointerKindLabel(learned.kind)
                 << ",hits=" << learned.hits << ")";
        }
        line << " tracked_links=" << g_inventoryGuiInventoryLinks.size()
             << " validated_links=" << validatedLinks
             << " scan_limit=0x" << std::hex << std::uppercase << kMaxScanOffset << std::dec;
        LogInfoLine(line.str());
        g_lastInventoryGuiBackPointerLearningSignature = signature.str();
    }
}

bool TryResolveInventoryFromInventoryGuiBackPointerOffsets(
    InventoryGUI* inventoryGui,
    Inventory** outInventory,
    std::size_t* outOffset,
    InventoryGuiBackPointerKind* outKind,
    RootObject** outResolvedOwner)
{
    if (outInventory == 0 || inventoryGui == 0)
    {
        return false;
    }

    *outInventory = 0;
    if (outOffset != 0)
    {
        *outOffset = 0;
    }
    if (outKind != 0)
    {
        *outKind = InventoryGuiBackPointerKind_DirectInventory;
    }
    if (outResolvedOwner != 0)
    {
        *outResolvedOwner = 0;
    }

    LearnInventoryGuiBackPointerOffsets();
    if (g_inventoryGuiBackPointerOffsets.empty())
    {
        return false;
    }

    for (std::size_t index = 0; index < g_inventoryGuiBackPointerOffsets.size(); ++index)
    {
        const InventoryGuiBackPointerOffset& learned = g_inventoryGuiBackPointerOffsets[index];
        const std::size_t offset = learned.offset;
        const void* value = 0;
        if (!TryReadPointerValueSafe(inventoryGui, offset, &value) || value == 0)
        {
            continue;
        }

        Inventory* candidateInventory = 0;
        RootObject* resolvedOwner = 0;
        if (learned.kind == InventoryGuiBackPointerKind_DirectInventory)
        {
            candidateInventory = reinterpret_cast<Inventory*>(const_cast<void*>(value));
            if (!IsInventoryPointerValidSafe(candidateInventory))
            {
                continue;
            }
        }
        else
        {
            resolvedOwner = reinterpret_cast<RootObject*>(const_cast<void*>(value));
            if (!IsRootObjectPointerValidSafe(resolvedOwner))
            {
                continue;
            }

            candidateInventory = TryGetRootObjectInventorySafe(resolvedOwner);
            if (!IsInventoryPointerValidSafe(candidateInventory))
            {
                continue;
            }

            RootObject* inventoryOwner = 0;
            RootObject* inventoryCallbackObject = 0;
            TryGetInventoryOwnerPointersSafe(
                candidateInventory,
                &inventoryOwner,
                &inventoryCallbackObject);

            if (resolvedOwner != inventoryOwner && resolvedOwner != inventoryCallbackObject)
            {
                continue;
            }
        }

        *outInventory = candidateInventory;
        if (outOffset != 0)
        {
            *outOffset = offset;
        }
        if (outKind != 0)
        {
            *outKind = learned.kind;
        }
        if (outResolvedOwner != 0)
        {
            *outResolvedOwner = resolvedOwner;
        }
        return true;
    }

    std::stringstream signature;
    signature << inventoryGui << "|miss|" << g_inventoryGuiBackPointerOffsets.size();
    if (signature.str() != g_lastInventoryGuiBackPointerResolutionFailureSignature)
    {
        std::stringstream line;
        line << "inventory gui back-pointer unresolved"
             << " inv_gui=" << inventoryGui
             << " learned_offsets=" << g_inventoryGuiBackPointerOffsets.size();
        LogWarnLine(line.str());
        g_lastInventoryGuiBackPointerResolutionFailureSignature = signature.str();
    }

    return false;
}

void RegisterRecentlyRefreshedInventory(Inventory* inventory)
{
    if (inventory == 0 || !IsInventoryPointerValidSafe(inventory))
    {
        return;
    }

    RootObject* owner = inventory->getOwner();
    if (owner == 0)
    {
        owner = inventory->getCallbackObject();
    }

    Character* ownerCharacter = dynamic_cast<Character*>(owner);
    Character* selectedCharacter =
        (ou == 0 || ou->player == 0) ? 0 : ou->player->selectedCharacter.getCharacter();
    const bool ownerSelected = ownerCharacter != 0 && ownerCharacter == selectedCharacter;
    const bool ownerTrader = ownerCharacter != 0 && ownerCharacter->isATrader();
    const std::string ownerName = RootObjectDisplayNameForLog(owner);
    const std::size_t itemCount = InventoryItemCountForLog(inventory);
    const bool visible = inventory->isVisible();

    for (std::size_t index = 0; index < g_recentRefreshedInventories.size(); ++index)
    {
        RefreshedInventoryLink& link = g_recentRefreshedInventories[index];
        if (link.inventory != inventory)
        {
            continue;
        }

        link.itemCount = itemCount;
        link.visible = visible;
        link.ownerTrader = ownerTrader;
        link.ownerSelected = ownerSelected;
        link.ownerName = ownerName;
        link.lastSeenTick = g_updateTickCounter;
        return;
    }

    RefreshedInventoryLink link;
    link.inventory = inventory;
    link.itemCount = itemCount;
    link.visible = visible;
    link.ownerTrader = ownerTrader;
    link.ownerSelected = ownerSelected;
    link.ownerName = ownerName;
    link.lastSeenTick = g_updateTickCounter;
    g_recentRefreshedInventories.push_back(link);
}

void PruneRecentlyRefreshedInventories()
{
    if (g_recentRefreshedInventories.empty())
    {
        return;
    }

    std::vector<RefreshedInventoryLink> kept;
    kept.reserve(g_recentRefreshedInventories.size());
    for (std::size_t index = 0; index < g_recentRefreshedInventories.size(); ++index)
    {
        const RefreshedInventoryLink& link = g_recentRefreshedInventories[index];
        if (link.inventory == 0 || !IsInventoryPointerValidSafe(link.inventory))
        {
            continue;
        }

        if (g_updateTickCounter > link.lastSeenTick
            && g_updateTickCounter - link.lastSeenTick > 3000ULL)
        {
            continue;
        }

        kept.push_back(link);
    }

    g_recentRefreshedInventories.swap(kept);
}

void AddInventoryGuiPointerUnique(std::vector<InventoryGUI*>* pointers, InventoryGUI* pointer)
{
    if (pointers == 0 || pointer == 0)
    {
        return;
    }

    for (std::size_t index = 0; index < pointers->size(); ++index)
    {
        if ((*pointers)[index] == pointer)
        {
            return;
        }
    }

    pointers->push_back(pointer);
}

bool HasInventoryGuiPointer(
    const std::vector<InventoryGUI*>& pointers,
    InventoryGUI* pointer)
{
    if (pointer == 0)
    {
        return false;
    }

    for (std::size_t index = 0; index < pointers.size(); ++index)
    {
        if (pointers[index] == pointer)
        {
            return true;
        }
    }

    return false;
}

void AddPointerAliasUnique(std::vector<std::uintptr_t>* aliases, std::uintptr_t value)
{
    if (aliases == 0 || value == 0)
    {
        return;
    }

    for (std::size_t index = 0; index < aliases->size(); ++index)
    {
        if ((*aliases)[index] == value)
        {
            return;
        }
    }

    aliases->push_back(value);
}

void CollectPointerAliasesFromRawPointer(const void* rawPointer, std::vector<std::uintptr_t>* aliases)
{
    if (rawPointer == 0 || aliases == 0)
    {
        return;
    }

    const std::uintptr_t rawValue = reinterpret_cast<std::uintptr_t>(rawPointer);
    AddPointerAliasUnique(aliases, rawValue);

    __try
    {
        const void* firstDeref = *reinterpret_cast<const void* const*>(rawPointer);
        AddPointerAliasUnique(aliases, reinterpret_cast<std::uintptr_t>(firstDeref));
        if (firstDeref != 0)
        {
            const void* secondDeref = *reinterpret_cast<const void* const*>(firstDeref);
            AddPointerAliasUnique(aliases, reinterpret_cast<std::uintptr_t>(secondDeref));
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

bool HasPointerAlias(
    const std::vector<std::uintptr_t>& aliases,
    const void* pointerValue)
{
    const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(pointerValue);
    if (value == 0)
    {
        return false;
    }

    for (std::size_t index = 0; index < aliases.size(); ++index)
    {
        if (aliases[index] == value)
        {
            return true;
        }
    }

    return false;
}

void CollectWidgetInventoryGuiPointersRecursive(
    MyGUI::Widget* widget,
    std::size_t depth,
    std::size_t maxDepth,
    std::size_t maxNodes,
    std::size_t* nodesVisited,
    std::vector<InventoryGUI*>* outPointers)
{
    if (widget == 0 || outPointers == 0 || nodesVisited == 0 || *nodesVisited >= maxNodes)
    {
        return;
    }

    ++(*nodesVisited);

    InventoryGUI* inventoryGui = ReadWidgetAnyDataPointer<InventoryGUI>(widget);
    AddInventoryGuiPointerUnique(outPointers, inventoryGui);

    if (depth >= maxDepth)
    {
        return;
    }

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        if (*nodesVisited >= maxNodes)
        {
            break;
        }

        CollectWidgetInventoryGuiPointersRecursive(
            widget->getChildAt(childIndex),
            depth + 1,
            maxDepth,
            maxNodes,
            nodesVisited,
            outPointers);
    }
}

void CollectWidgetInventoryGuiPointers(
    MyGUI::Widget* rootWidget,
    std::size_t maxDepth,
    std::size_t maxNodes,
    std::vector<InventoryGUI*>* outPointers)
{
    if (rootWidget == 0 || outPointers == 0)
    {
        return;
    }

    std::size_t nodesVisited = 0;
    CollectWidgetInventoryGuiPointersRecursive(
        rootWidget,
        0,
        maxDepth,
        maxNodes,
        &nodesVisited,
        outPointers);

    MyGUI::Widget* current = rootWidget->getParent();
    for (std::size_t depth = 0; current != 0 && depth < 12; ++depth)
    {
        InventoryGUI* inventoryGui = ReadWidgetAnyDataPointer<InventoryGUI>(current);
        AddInventoryGuiPointerUnique(outPointers, inventoryGui);
        current = current->getParent();
    }
}

bool TryResolveTraderInventoryNameKeysFromWidgetBindings(
    MyGUI::Widget* traderParent,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys,
    Inventory** outSelectedInventory = 0)
{
    if (traderParent == 0 || outKeys == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    std::vector<InventoryCandidateInfo> inventoryCandidates;

    MyGUI::Widget* backpackContent = ResolveBestBackpackContentWidget(traderParent, false);
    if (backpackContent == 0)
    {
        backpackContent = FindWidgetInParentByToken(traderParent, "backpack_content");
    }
    if (backpackContent != 0)
    {
        CollectWidgetChainInventoryCandidates(
            backpackContent,
            "widget_backpack",
            7600,
            &inventoryCandidates);
    }

    MyGUI::Widget* scrollBackpackContent = FindAncestorByToken(backpackContent, "scrollview_backpack_content");
    if (scrollBackpackContent == 0)
    {
        scrollBackpackContent = FindWidgetInParentByToken(traderParent, "scrollview_backpack_content");
    }
    if (scrollBackpackContent != 0)
    {
        CollectWidgetChainInventoryCandidates(
            scrollBackpackContent,
            "widget_scroll",
            7400,
            &inventoryCandidates);
    }

    MyGUI::Widget* entriesRoot =
        backpackContent == 0 ? 0 : ResolveInventoryEntriesRoot(backpackContent);
    if (entriesRoot != 0)
    {
        CollectWidgetChainInventoryCandidates(
            entriesRoot,
            "widget_entries",
            7800,
            &inventoryCandidates);

        CollectWidgetTreeInventoryCandidates(
            entriesRoot,
            "widget_tree_entries",
            8600,
            7,
            1200,
            &inventoryCandidates);
    }

    CollectWidgetChainInventoryCandidates(
        traderParent,
        "widget_parent",
        7000,
        &inventoryCandidates);
    CollectWidgetTreeInventoryCandidates(
        traderParent,
        "widget_tree_parent",
        8200,
        6,
        1200,
        &inventoryCandidates);

    MyGUI::Window* owningWindow = FindOwningWindow(traderParent);

    std::vector<InventoryGUI*> widgetInventoryGuis;
    CollectWidgetInventoryGuiPointers(backpackContent, 4, 320, &widgetInventoryGuis);
    CollectWidgetInventoryGuiPointers(scrollBackpackContent, 4, 320, &widgetInventoryGuis);
    CollectWidgetInventoryGuiPointers(entriesRoot, 3, 256, &widgetInventoryGuis);
    CollectWidgetInventoryGuiPointers(traderParent, 2, 192, &widgetInventoryGuis);
    if (owningWindow != 0)
    {
        CollectWidgetInventoryGuiPointers(owningWindow, 6, 2400, &widgetInventoryGuis);
        CollectWidgetTreeInventoryCandidates(
            owningWindow,
            "widget_tree_window",
            8000,
            6,
            1600,
            &inventoryCandidates);
    }

    std::vector<std::uintptr_t> widgetPointerAliases;
    for (std::size_t pointerIndex = 0; pointerIndex < widgetInventoryGuis.size(); ++pointerIndex)
    {
        CollectPointerAliasesFromRawPointer(widgetInventoryGuis[pointerIndex], &widgetPointerAliases);
    }

    std::vector<InventoryCandidateInfo> ownershipCandidates;

    const std::string normalizedCaption =
        owningWindow == 0 ? "" : NormalizeSearchText(owningWindow->getCaption().asUTF8());

    const ogre_unordered_set<Character*>::type& activeCharacters = ou->getCharacterUpdateList();
    for (ogre_unordered_set<Character*>::type::const_iterator it = activeCharacters.begin();
         it != activeCharacters.end();
         ++it)
    {
        Character* character = *it;
        if (character == 0 || !character->isATrader() || character->inventory == 0)
        {
            continue;
        }

        int captionBias = 0;
        if (!normalizedCaption.empty())
        {
            captionBias = ComputeCaptionNameMatchBias(
                normalizedCaption,
                NormalizeSearchText(CharacterNameForLog(character)));
            if (captionBias <= 0)
            {
                continue;
            }
        }

        CollectTraderOwnershipInventoryCandidates(
            character,
            captionBias,
            "widget_trader",
            &ownershipCandidates);
    }

    std::size_t guiMatchedCandidateCount = 0;
    for (std::size_t candidateIndex = 0; candidateIndex < ownershipCandidates.size(); ++candidateIndex)
    {
        const InventoryCandidateInfo& ownershipCandidate = ownershipCandidates[candidateIndex];
        InventoryGUI* candidateGui = TryGetInventoryGuiSafe(ownershipCandidate.inventory);
        const bool guiMatch =
            HasInventoryGuiPointer(widgetInventoryGuis, candidateGui)
            || HasPointerAlias(widgetPointerAliases, candidateGui)
            || HasPointerAlias(widgetPointerAliases, ownershipCandidate.inventory);
        if (!guiMatch)
        {
            continue;
        }

        ++guiMatchedCandidateCount;

        std::stringstream source;
        source << "widget_gui_match:" << ownershipCandidate.source
               << " gui_matched=true";
        AddInventoryCandidateUnique(
            &inventoryCandidates,
            ownershipCandidate.inventory,
            source.str(),
            true,
            ownershipCandidate.visible,
            ownershipCandidate.priorityBias + 6800);
    }

    if (inventoryCandidates.empty())
    {
        if (!g_loggedWidgetInventoryCandidatesMissing)
        {
            std::stringstream line;
            line << "widget inventory candidate scan found none"
                 << " parent=" << SafeWidgetName(traderParent)
                 << " has_backpack="
                 << (backpackContent == 0 ? "false" : "true")
                 << " has_scroll="
                 << (scrollBackpackContent == 0 ? "false" : "true")
                 << " has_entries="
                 << (entriesRoot == 0 ? "false" : "true")
                 << " widget_gui_ptrs=" << widgetInventoryGuis.size()
                 << " widget_aliases=" << widgetPointerAliases.size()
                 << " ownership_candidates=" << ownershipCandidates.size()
                 << " gui_matches=" << guiMatchedCandidateCount;
            LogInfoLine(line.str());
            g_loggedWidgetInventoryCandidatesMissing = true;
        }
        return false;
    }
    g_loggedWidgetInventoryCandidatesMissing = false;

    const bool resolved = TryResolveInventoryNameKeysFromCandidates(
        inventoryCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys,
        outSelectedInventory);
    if (!resolved || outSource == 0)
    {
        return resolved;
    }

    std::stringstream line;
    line << *outSource
         << " widget_candidates=" << inventoryCandidates.size()
         << " widget_gui_ptrs=" << widgetInventoryGuis.size()
         << " widget_aliases=" << widgetPointerAliases.size()
         << " ownership_candidates=" << ownershipCandidates.size()
         << " gui_matches=" << guiMatchedCandidateCount;
    *outSource = line.str();
    return true;
}

Inventory* TryGetInventoryFromItemSafe(Item* item)
{
    if (item == 0)
    {
        return 0;
    }

    __try
    {
        return item->getInventory();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

Item* TryGetSelectedObjectItemSafe(PlayerInterface* player)
{
    if (player == 0)
    {
        return 0;
    }

    __try
    {
        if (player->selectedObject.isValid())
        {
            return player->selectedObject.getItem();
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }

    return 0;
}

Item* TryGetMouseTargetItemSafe(PlayerInterface* player)
{
    if (player == 0 || player->mouseRightTarget == 0)
    {
        return 0;
    }

    __try
    {
        return player->mouseRightTarget->getHandle().getItem();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

void AddSelectedItemInventoryCandidate(
    Item* item,
    const char* origin,
    std::vector<InventoryCandidateInfo>* inventoryCandidates)
{
    if (item == 0 || inventoryCandidates == 0)
    {
        return;
    }

    Inventory* inventory = TryGetInventoryFromItemSafe(item);
    if (inventory == 0)
    {
        return;
    }

    RootObject* owner = inventory->getOwner();
    if (owner == 0)
    {
        owner = inventory->getCallbackObject();
    }

    std::stringstream source;
    source << "selected_item:" << (origin == 0 ? "unknown" : origin)
           << ":" << TruncateForLog(ResolveCanonicalItemName(item), 48)
           << " owner=" << RootObjectDisplayNameForLog(owner)
           << " visible=" << (inventory->isVisible() ? "true" : "false")
           << " owner_items=" << InventoryItemCountForLog(owner == 0 ? 0 : owner->getInventory());
    AddInventoryCandidateUnique(
        inventoryCandidates,
        inventory,
        source.str(),
        true,
        inventory->isVisible(),
        5200);
}

bool TryResolveTraderInventoryNameKeysFromSelectedItemHandles(
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys)
{
    if (outKeys == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    std::vector<InventoryCandidateInfo> inventoryCandidates;

    Item* selectedItem = TryGetSelectedObjectItemSafe(ou->player);
    AddSelectedItemInventoryCandidate(selectedItem, "selected_object", &inventoryCandidates);

    Item* mouseTargetItem = TryGetMouseTargetItemSafe(ou->player);
    AddSelectedItemInventoryCandidate(mouseTargetItem, "mouse_target", &inventoryCandidates);

    if (inventoryCandidates.empty())
    {
        return false;
    }

    return TryResolveInventoryNameKeysFromCandidates(
        inventoryCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys);
}

void UpdateHoveredInventoryCache(
    Inventory* inventory,
    MyGUI::Widget* hoveredWidget,
    const char* sourceTag)
{
    if (inventory == 0)
    {
        return;
    }

    RootObject* owner = inventory->getOwner();
    if (owner == 0)
    {
        owner = inventory->getCallbackObject();
    }

    std::stringstream signature;
    signature << inventory
              << "|owner=" << RootObjectDisplayNameForLog(owner)
              << "|items=" << InventoryItemCountForLog(inventory);
    if (sourceTag != 0)
    {
        signature << "|source=" << sourceTag;
    }

    g_cachedHoveredWidgetInventory = inventory;
    if (signature.str() != g_cachedHoveredWidgetInventorySignature)
    {
        std::stringstream line;
        line << "hovered inventory cached"
             << " source=" << (sourceTag == 0 ? "<unknown>" : sourceTag)
             << " inventory_items=" << InventoryItemCountForLog(inventory)
             << " owner=" << RootObjectDisplayNameForLog(owner)
             << " hovered_widget=" << SafeWidgetName(hoveredWidget);
        LogInfoLine(line.str());
        g_cachedHoveredWidgetInventorySignature = signature.str();
    }
}

bool TryResolveTraderInventoryNameKeysFromHoveredWidget(
    MyGUI::Widget* traderParent,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys)
{
    if (traderParent == 0 || outKeys == 0)
    {
        return false;
    }

    std::vector<InventoryCandidateInfo> inventoryCandidates;

    MyGUI::Widget* backpackContent = ResolveBestBackpackContentWidget(traderParent, false);
    if (backpackContent == 0)
    {
        backpackContent = FindWidgetInParentByToken(traderParent, "backpack_content");
    }
    MyGUI::Widget* entriesRoot =
        backpackContent == 0 ? 0 : ResolveInventoryEntriesRoot(backpackContent);

    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    MyGUI::Widget* hovered = inputManager == 0 ? 0 : inputManager->getMouseFocusWidget();
    const bool hoveredInsideEntries =
        hovered != 0 && entriesRoot != 0 && IsDescendantOf(hovered, entriesRoot);
    if (hoveredInsideEntries)
    {
        CollectWidgetChainInventoryCandidates(
            hovered,
            "hovered_entry_chain",
            9800,
            &inventoryCandidates);
        CollectWidgetTreeInventoryCandidates(
            hovered,
            "hovered_entry_tree",
            10200,
            4,
            240,
            &inventoryCandidates);

        Inventory* bestHoveredInventory = 0;
        std::size_t bestHoveredInventoryItems = 0;
        for (std::size_t index = 0; index < inventoryCandidates.size(); ++index)
        {
            Inventory* inventory = inventoryCandidates[index].inventory;
            if (inventory == 0)
            {
                continue;
            }

            const std::size_t itemCount = InventoryItemCountForLog(inventory);
            if (bestHoveredInventory == 0
                || itemCount > bestHoveredInventoryItems
                || (itemCount == bestHoveredInventoryItems
                    && inventoryCandidates[index].priorityBias > 0))
            {
                bestHoveredInventory = inventory;
                bestHoveredInventoryItems = itemCount;
            }
        }

        if (bestHoveredInventory != 0)
        {
            UpdateHoveredInventoryCache(bestHoveredInventory, hovered, "hovered_entry");
        }
    }

    if (g_cachedHoveredWidgetInventory != 0 && !IsInventoryPointerValidSafe(g_cachedHoveredWidgetInventory))
    {
        g_cachedHoveredWidgetInventory = 0;
        g_cachedHoveredWidgetInventorySignature.clear();
    }

    if (g_cachedHoveredWidgetInventory != 0)
    {
        RootObject* owner = g_cachedHoveredWidgetInventory->getOwner();
        if (owner == 0)
        {
            owner = g_cachedHoveredWidgetInventory->getCallbackObject();
        }

        std::stringstream source;
        source << "hovered_cached"
               << " owner=" << RootObjectDisplayNameForLog(owner)
               << " visible=" << (g_cachedHoveredWidgetInventory->isVisible() ? "true" : "false")
               << " items=" << InventoryItemCountForLog(g_cachedHoveredWidgetInventory)
               << " hovered_inside_entries=" << (hoveredInsideEntries ? "true" : "false");
        AddInventoryCandidateUnique(
            &inventoryCandidates,
            g_cachedHoveredWidgetInventory,
            source.str(),
            true,
            g_cachedHoveredWidgetInventory->isVisible(),
            9400);
    }

    if (inventoryCandidates.empty())
    {
        return false;
    }

    const bool resolved = TryResolveInventoryNameKeysFromCandidates(
        inventoryCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys);
    if (!resolved)
    {
        return false;
    }

    if (outSource != 0)
    {
        std::stringstream source;
        source << *outSource
               << " hovered_candidates=" << inventoryCandidates.size()
               << " hovered_inside_entries=" << (hoveredInsideEntries ? "true" : "false");
        *outSource = source.str();
    }

    return true;
}

bool TryResolveTraderInventoryNameKeysFromSectionWidgetMap(
    MyGUI::Widget* traderParent,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys,
    Inventory** outSelectedInventory = 0)
{
    if (traderParent == 0 || outKeys == 0)
    {
        return false;
    }

    PruneSectionWidgetInventoryLinks();
    if (g_sectionWidgetInventoryLinks.empty())
    {
        return false;
    }

    MyGUI::Widget* backpackContent = ResolveBestBackpackContentWidget(traderParent, false);
    if (backpackContent == 0)
    {
        backpackContent = FindWidgetInParentByToken(traderParent, "backpack_content");
    }

    MyGUI::Widget* entriesRoot =
        backpackContent == 0 ? 0 : ResolveInventoryEntriesRoot(backpackContent);
    if (entriesRoot == 0)
    {
        return false;
    }

    std::vector<MyGUI::Widget*> ancestry;
    ancestry.reserve(24);
    for (MyGUI::Widget* current = entriesRoot; current != 0; current = current->getParent())
    {
        bool duplicate = false;
        for (std::size_t index = 0; index < ancestry.size(); ++index)
        {
            if (ancestry[index] == current)
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
        {
            ancestry.push_back(current);
        }
    }

    std::vector<InventoryCandidateInfo> inventoryCandidates;
    std::size_t matchedLinks = 0;
    std::size_t directEntriesMatches = 0;
    std::size_t directBackpackMatches = 0;

    for (std::size_t linkIndex = 0; linkIndex < g_sectionWidgetInventoryLinks.size(); ++linkIndex)
    {
        const SectionWidgetInventoryLink& link = g_sectionWidgetInventoryLinks[linkIndex];
        if (link.sectionWidget == 0
            || link.inventory == 0
            || !IsInventoryPointerValidSafe(link.inventory))
        {
            continue;
        }

        int matchedDepth = -1;
        for (std::size_t depth = 0; depth < ancestry.size(); ++depth)
        {
            if (ancestry[depth] == link.sectionWidget)
            {
                matchedDepth = static_cast<int>(depth);
                break;
            }
        }

        if (matchedDepth < 0)
        {
            continue;
        }

        ++matchedLinks;
        if (link.sectionWidget == entriesRoot)
        {
            ++directEntriesMatches;
        }
        if (link.sectionWidget == backpackContent)
        {
            ++directBackpackMatches;
        }

        int depthBonus = 1600 - matchedDepth * 90;
        if (depthBonus < 240)
        {
            depthBonus = 240;
        }

        const unsigned long long ageTicks =
            g_updateTickCounter >= link.lastSeenTick
                ? g_updateTickCounter - link.lastSeenTick
                : 0ULL;

        std::stringstream source;
        source << "section_widget_map"
               << " section=" << link.sectionName
               << " widget=" << link.widgetName
               << " depth=" << matchedDepth
               << " age_ticks=" << ageTicks
               << " items=" << link.itemCount;
        AddInventoryCandidateUnique(
            &inventoryCandidates,
            link.inventory,
            source.str(),
            true,
            link.inventory->isVisible(),
            10800 + depthBonus);
    }

    if (inventoryCandidates.empty())
    {
        return false;
    }

    const bool resolved = TryResolveInventoryNameKeysFromCandidates(
        inventoryCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys,
        outSelectedInventory);
    if (!resolved)
    {
        return false;
    }

    if (outSource != 0)
    {
        std::stringstream source;
        source << *outSource
               << " section_widget_matches=" << matchedLinks
               << " direct_entries_matches=" << directEntriesMatches
               << " direct_backpack_matches=" << directBackpackMatches
               << " tracked_links=" << g_sectionWidgetInventoryLinks.size();
        *outSource = source.str();
    }

    return true;
}

bool TryResolveTraderInventoryNameKeysFromInventoryGuiMap(
    MyGUI::Widget* traderParent,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys,
    Inventory** outSelectedInventory = 0)
{
    if (traderParent == 0 || outKeys == 0)
    {
        return false;
    }

    PruneInventoryGuiInventoryLinks();
    if (g_inventoryGuiInventoryLinks.empty())
    {
        return false;
    }

    MyGUI::Widget* backpackContent = ResolveBestBackpackContentWidget(traderParent, false);
    if (backpackContent == 0)
    {
        backpackContent = FindWidgetInParentByToken(traderParent, "backpack_content");
    }

    MyGUI::Widget* scrollBackpackContent = FindAncestorByToken(backpackContent, "scrollview_backpack_content");
    if (scrollBackpackContent == 0)
    {
        scrollBackpackContent = FindWidgetInParentByToken(traderParent, "scrollview_backpack_content");
    }

    MyGUI::Widget* entriesRoot =
        backpackContent == 0 ? 0 : ResolveInventoryEntriesRoot(backpackContent);
    MyGUI::Window* owningWindow = FindOwningWindow(traderParent);

    std::vector<InventoryGUI*> widgetInventoryGuis;
    CollectWidgetInventoryGuiPointers(backpackContent, 4, 320, &widgetInventoryGuis);
    CollectWidgetInventoryGuiPointers(scrollBackpackContent, 4, 320, &widgetInventoryGuis);
    CollectWidgetInventoryGuiPointers(entriesRoot, 3, 256, &widgetInventoryGuis);
    CollectWidgetInventoryGuiPointers(traderParent, 2, 192, &widgetInventoryGuis);
    if (owningWindow != 0)
    {
        CollectWidgetInventoryGuiPointers(owningWindow, 6, 2400, &widgetInventoryGuis);
    }
    if (widgetInventoryGuis.empty())
    {
        return false;
    }

    std::vector<std::uintptr_t> widgetPointerAliases;
    for (std::size_t pointerIndex = 0; pointerIndex < widgetInventoryGuis.size(); ++pointerIndex)
    {
        CollectPointerAliasesFromRawPointer(widgetInventoryGuis[pointerIndex], &widgetPointerAliases);
    }

    std::vector<InventoryCandidateInfo> inventoryCandidates;
    std::size_t matchedLinks = 0;
    std::size_t inferredMatches = 0;
    for (std::size_t linkIndex = 0; linkIndex < g_inventoryGuiInventoryLinks.size(); ++linkIndex)
    {
        const InventoryGuiInventoryLink& link = g_inventoryGuiInventoryLinks[linkIndex];
        if (link.inventoryGui == 0
            || link.inventory == 0
            || !IsInventoryPointerValidSafe(link.inventory))
        {
            continue;
        }

        const bool guiMatch =
            HasInventoryGuiPointer(widgetInventoryGuis, link.inventoryGui)
            || HasPointerAlias(widgetPointerAliases, link.inventoryGui);
        if (!guiMatch)
        {
            continue;
        }

        ++matchedLinks;
        const unsigned long long ageTicks =
            g_updateTickCounter >= link.lastSeenTick
                ? g_updateTickCounter - link.lastSeenTick
                : 0ULL;
        std::stringstream source;
        source << "inventory_gui_map"
               << " owner=" << link.ownerName
               << " age_ticks=" << ageTicks
               << " items=" << link.itemCount
               << " visible=" << (link.inventory->isVisible() ? "true" : "false");
        AddInventoryCandidateUnique(
            &inventoryCandidates,
            link.inventory,
            source.str(),
            true,
            link.inventory->isVisible(),
            12600);
    }

    for (std::size_t guiIndex = 0; guiIndex < widgetInventoryGuis.size(); ++guiIndex)
    {
        InventoryGUI* widgetGui = widgetInventoryGuis[guiIndex];
        Inventory* inferredInventory = 0;
        std::size_t resolvedOffset = 0;
        InventoryGuiBackPointerKind resolvedKind = InventoryGuiBackPointerKind_DirectInventory;
        RootObject* resolvedOwner = 0;
        if (!TryResolveInventoryFromInventoryGuiBackPointerOffsets(
                widgetGui,
                &inferredInventory,
                &resolvedOffset,
                &resolvedKind,
                &resolvedOwner))
        {
            continue;
        }

        ++inferredMatches;
        RootObject* owner = resolvedOwner;
        if (owner == 0)
        {
            owner = inferredInventory->getOwner();
            if (owner == 0)
            {
                owner = inferredInventory->getCallbackObject();
            }
        }

        Character* ownerCharacter = dynamic_cast<Character*>(owner);
        const bool ownerTrader = ownerCharacter != 0 && ownerCharacter->isATrader();
        const std::size_t itemCount = InventoryItemCountForLog(inferredInventory);
        InventoryGUI* directResolvedGui = TryGetInventoryGuiSafe(inferredInventory);
        const bool guiExactMatch = directResolvedGui != 0 && directResolvedGui == widgetGui;
        std::stringstream source;
        source << "inventory_gui_offset"
               << " kind=" << InventoryGuiBackPointerKindLabel(resolvedKind)
               << " owner=" << RootObjectDisplayNameForLog(owner)
               << " offset=0x" << std::hex << std::uppercase << resolvedOffset << std::dec
               << " items=" << itemCount
               << " gui_exact=" << (guiExactMatch ? "true" : "false")
               << " visible=" << (inferredInventory->isVisible() ? "true" : "false");
        AddInventoryCandidateUnique(
            &inventoryCandidates,
            inferredInventory,
            source.str(),
            ownerTrader,
            inferredInventory->isVisible(),
            12350);

        std::stringstream resolutionSignature;
        resolutionSignature << widgetGui
                            << "|" << inferredInventory
                            << "|" << static_cast<int>(resolvedKind)
                            << "|" << resolvedOffset
                            << "|" << itemCount;
        if (resolutionSignature.str() != g_lastInventoryGuiBackPointerResolutionSignature)
        {
            std::stringstream line;
            line << "inventory gui back-pointer resolved"
                 << " inv_gui=" << widgetGui
                 << " inventory=" << inferredInventory
                 << " kind=" << InventoryGuiBackPointerKindLabel(resolvedKind)
                 << " owner=" << RootObjectDisplayNameForLog(owner)
                 << " offset=0x" << std::hex << std::uppercase << resolvedOffset << std::dec
                 << " gui_exact=" << (guiExactMatch ? "true" : "false")
                 << " items=" << itemCount;
            LogInfoLine(line.str());
            g_lastInventoryGuiBackPointerResolutionSignature = resolutionSignature.str();
        }
    }

    if (inventoryCandidates.empty())
    {
        return false;
    }

    const bool resolved = TryResolveInventoryNameKeysFromCandidates(
        inventoryCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys,
        outSelectedInventory);
    if (!resolved)
    {
        return false;
    }

    if (outSource != 0)
    {
        std::stringstream source;
        source << *outSource
               << " gui_map_matches=" << matchedLinks
               << " gui_offset_matches=" << inferredMatches
               << " tracked_gui_links=" << g_inventoryGuiInventoryLinks.size()
               << " learned_gui_offsets=" << g_inventoryGuiBackPointerOffsets.size()
               << " widget_gui_ptrs=" << widgetInventoryGuis.size()
               << " widget_aliases=" << widgetPointerAliases.size();
        *outSource = source.str();
    }

    return true;
}

bool TryResolveTraderInventoryNameKeysFromRecentRefreshedInventories(
    MyGUI::Widget* traderParent,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys)
{
    if (traderParent == 0 || outKeys == 0)
    {
        return false;
    }

    PruneRecentlyRefreshedInventories();
    if (g_recentRefreshedInventories.empty())
    {
        return false;
    }

    Character* captionTrader = 0;
    int captionScore = 0;
    TryResolveCaptionMatchedTraderCharacter(traderParent, &captionTrader, &captionScore);
    Inventory* captionTraderInventory = captionTrader == 0 ? 0 : captionTrader->inventory;

    std::vector<InventoryCandidateInfo> inventoryCandidates;
    for (std::size_t index = 0; index < g_recentRefreshedInventories.size(); ++index)
    {
        const RefreshedInventoryLink& link = g_recentRefreshedInventories[index];
        if (link.inventory == 0)
        {
            continue;
        }

        int priorityBias = 9800;
        if (link.ownerTrader)
        {
            priorityBias += 1200;
        }
        if (link.ownerSelected)
        {
            priorityBias -= 1800;
        }
        if (link.visible)
        {
            priorityBias += 180;
        }

        if (expectedEntryCount > 0)
        {
            const int expected = static_cast<int>(expectedEntryCount);
            const int count = static_cast<int>(link.itemCount);
            const int diff = count > expected ? count - expected : expected - count;
            priorityBias += 1200 - diff * 120;
            if (diff <= 1)
            {
                priorityBias += 400;
            }
            else if (diff <= 3)
            {
                priorityBias += 180;
            }
        }

        if (captionTraderInventory != 0 && link.inventory == captionTraderInventory)
        {
            priorityBias += 2600 + (captionScore / 2);
        }

        const unsigned long long ageTicks =
            g_updateTickCounter >= link.lastSeenTick
                ? g_updateTickCounter - link.lastSeenTick
                : 0ULL;
        if (ageTicks > 0)
        {
            const int agePenalty = static_cast<int>(ageTicks > 1200ULL ? 1200ULL : ageTicks);
            priorityBias -= agePenalty;
        }

        std::stringstream source;
        source << "recent_refresh"
               << " owner=" << link.ownerName
               << " owner_trader=" << (link.ownerTrader ? "true" : "false")
               << " owner_selected=" << (link.ownerSelected ? "true" : "false")
               << " visible=" << (link.visible ? "true" : "false")
               << " items=" << link.itemCount
               << " age_ticks=" << ageTicks;
        AddInventoryCandidateUnique(
            &inventoryCandidates,
            link.inventory,
            source.str(),
            link.ownerTrader,
            link.visible,
            priorityBias);
    }

    if (inventoryCandidates.empty())
    {
        return false;
    }

    const bool resolved = TryResolveInventoryNameKeysFromCandidates(
        inventoryCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys);
    if (!resolved)
    {
        return false;
    }

    if (outSource != 0)
    {
        std::stringstream source;
        source << *outSource
               << " refresh_candidates=" << inventoryCandidates.size()
               << " tracked_recent=" << g_recentRefreshedInventories.size();
        *outSource = source.str();
    }

    return true;
}

bool TryResolvePreferredDialogueTraderTarget(
    Character** outTarget,
    Character** outSpeaker,
    std::string* outReason)
{
    if (outTarget == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    *outTarget = 0;
    if (outSpeaker != 0)
    {
        *outSpeaker = 0;
    }
    if (outReason != 0)
    {
        outReason->clear();
    }

    Character* selectedCharacter = ou->player->selectedCharacter.getCharacter();
    std::vector<Character*> playerCharacters;
    if (selectedCharacter != 0)
    {
        playerCharacters.push_back(selectedCharacter);
    }

    const lektor<Character*>& allPlayerCharacters = ou->player->getAllPlayerCharacters();
    for (lektor<Character*>::const_iterator iter = allPlayerCharacters.begin(); iter != allPlayerCharacters.end(); ++iter)
    {
        Character* candidate = *iter;
        if (candidate == 0 || candidate == selectedCharacter)
        {
            continue;
        }
        playerCharacters.push_back(candidate);
    }

    Character* bestTarget = 0;
    Character* bestSpeaker = 0;
    int bestScore = -1000000;
    std::string bestReason;

    for (std::size_t charIndex = 0; charIndex < playerCharacters.size(); ++charIndex)
    {
        Character* playerChar = playerCharacters[charIndex];
        if (playerChar == 0 || playerChar->dialogue == 0)
        {
            continue;
        }

        Character* target = playerChar->dialogue->getConversationTarget().getCharacter();
        if (target == 0 || target->inventory == 0)
        {
            continue;
        }

        bool playerInventoryVisible = false;
        bool targetInventoryVisible = false;
        TryResolveCharacterInventoryVisible(playerChar, &playerInventoryVisible);
        TryResolveCharacterInventoryVisible(target, &targetInventoryVisible);

        const bool dialogActive = !playerChar->dialogue->conversationHasEndedPrettyMuch();
        const bool engaged = playerChar->_isEngagedWithAPlayer || target->_isEngagedWithAPlayer;
        const bool targetIsTrader = target->isATrader();
        if (!dialogActive && !targetIsTrader)
        {
            continue;
        }

        int score = 0;
        if (targetIsTrader)
        {
            score += 1800;
        }
        if (dialogActive)
        {
            score += 600;
        }
        if (engaged)
        {
            score += 240;
        }
        if (targetInventoryVisible)
        {
            score += 160;
        }
        if (playerInventoryVisible)
        {
            score += 90;
        }
        if (playerChar == selectedCharacter)
        {
            score += 180;
        }
        score += static_cast<int>(InventoryItemCountForLog(target->inventory));

        if (score > bestScore)
        {
            bestScore = score;
            bestTarget = target;
            bestSpeaker = playerChar;
            std::stringstream reason;
            reason << "dialog_target:" << CharacterNameForLog(target)
                   << " speaker=" << CharacterNameForLog(playerChar)
                   << " trader=" << (targetIsTrader ? "true" : "false")
                   << " dialog_active=" << (dialogActive ? "true" : "false")
                   << " engaged=" << (engaged ? "true" : "false")
                   << " visible=" << (targetInventoryVisible ? "true" : "false");
            bestReason = reason.str();
        }
    }

    if (bestTarget == 0)
    {
        return false;
    }

    *outTarget = bestTarget;
    if (outSpeaker != 0)
    {
        *outSpeaker = bestSpeaker;
    }
    if (outReason != 0)
    {
        *outReason = bestReason;
    }
    return true;
}

bool TryResolveCaptionMatchedTraderCharacter(
    MyGUI::Widget* traderParent,
    Character** outCharacter,
    int* outCaptionScore)
{
    if (outCharacter == 0 || traderParent == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    *outCharacter = 0;
    if (outCaptionScore != 0)
    {
        *outCaptionScore = 0;
    }

    MyGUI::Window* owningWindow = FindOwningWindow(traderParent);
    if (owningWindow == 0)
    {
        return false;
    }

    const std::string normalizedCaption = NormalizeSearchText(owningWindow->getCaption().asUTF8());
    if (normalizedCaption.empty())
    {
        return false;
    }

    Character* bestCharacter = 0;
    int bestScore = 0;
    const ogre_unordered_set<Character*>::type& activeCharacters = ou->getCharacterUpdateList();
    for (ogre_unordered_set<Character*>::type::const_iterator it = activeCharacters.begin();
         it != activeCharacters.end();
         ++it)
    {
        Character* candidate = *it;
        if (candidate == 0 || candidate->inventory == 0 || !candidate->isATrader())
        {
            continue;
        }

        const int captionScore = ComputeCaptionNameMatchBias(
            normalizedCaption,
            NormalizeSearchText(CharacterNameForLog(candidate)));
        if (captionScore <= 0)
        {
            continue;
        }

        if (captionScore > bestScore)
        {
            bestScore = captionScore;
            bestCharacter = candidate;
        }
    }

    if (bestCharacter == 0)
    {
        return false;
    }

    *outCharacter = bestCharacter;
    if (outCaptionScore != 0)
    {
        *outCaptionScore = bestScore;
    }
    return true;
}

bool TryResolveTraderInventoryNameKeysFromTraderOwnership(
    MyGUI::Widget* traderParent,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys)
{
    if (traderParent == 0 || outKeys == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    std::vector<InventoryCandidateInfo> inventoryCandidates;

    Character* captionTrader = 0;
    int captionScore = 0;
    if (TryResolveCaptionMatchedTraderCharacter(traderParent, &captionTrader, &captionScore)
        && captionTrader != 0)
    {
        CollectTraderOwnershipInventoryCandidates(
            captionTrader,
            captionScore,
            "trader_home_caption",
            &inventoryCandidates);
    }

    Character* dialogueTarget = 0;
    Character* dialogueSpeaker = 0;
    std::string dialogueReason;
    if (TryResolvePreferredDialogueTraderTarget(&dialogueTarget, &dialogueSpeaker, &dialogueReason)
        && dialogueTarget != 0)
    {
        CollectTraderOwnershipInventoryCandidates(
            dialogueTarget,
            0,
            "trader_home_dialogue",
            &inventoryCandidates);
    }

    if (inventoryCandidates.empty())
    {
        return false;
    }

    std::size_t selfInventoryCandidateCount = 0;
    for (std::size_t candidateIndex = 0; candidateIndex < inventoryCandidates.size(); ++candidateIndex)
    {
        const std::string sourceLower = NormalizeSearchText(inventoryCandidates[candidateIndex].source);
        if (sourceLower.find("self inventory true") != std::string::npos)
        {
            ++selfInventoryCandidateCount;
        }
    }

    // Aggregate all ownership-linked inventories and align by visible UI quantity sequence.
    // This avoids picking a single furniture inventory when the trader UI spans multiple containers.
    std::vector<std::string> aggregateKeys;
    std::vector<QuantityNameKey> aggregateQuantityKeys;
    for (std::size_t index = 0; index < inventoryCandidates.size(); ++index)
    {
        Inventory* inventory = inventoryCandidates[index].inventory;
        if (inventory == 0)
        {
            continue;
        }

        std::vector<std::string> inventoryKeys;
        if (TryExtractSearchKeysFromInventory(inventory, &inventoryKeys))
        {
            for (std::size_t keyIndex = 0; keyIndex < inventoryKeys.size(); ++keyIndex)
            {
                if (!inventoryKeys[keyIndex].empty())
                {
                    aggregateKeys.push_back(inventoryKeys[keyIndex]);
                }
            }
        }

        std::vector<QuantityNameKey> inventoryQuantityKeys;
        if (TryExtractQuantityNameKeysFromInventory(inventory, &inventoryQuantityKeys))
        {
            for (std::size_t q = 0; q < inventoryQuantityKeys.size(); ++q)
            {
                if (!inventoryQuantityKeys[q].key.empty())
                {
                    aggregateQuantityKeys.push_back(inventoryQuantityKeys[q]);
                }
            }
        }
    }

    std::vector<std::string> aggregateFallbackKeys;
    std::size_t aggregateAlignedMatchCount = 0;
    std::size_t aggregateNonEmptyCount = 0;
    bool aggregateAlignmentComputed = false;

    if (uiQuantities != 0 && !uiQuantities->empty() && !aggregateQuantityKeys.empty())
    {
        std::vector<std::string> aggregateAlignedNames;
        if (BuildAlignedInventoryNameHintsByQuantity(
                *uiQuantities,
                aggregateQuantityKeys,
                &aggregateAlignedNames))
        {
            aggregateAlignmentComputed = true;
            std::size_t alignedMatches = 0;
            for (std::size_t index = 0; index < aggregateAlignedNames.size(); ++index)
            {
                if (!aggregateAlignedNames[index].empty())
                {
                    ++alignedMatches;
                }
            }
            aggregateAlignedMatchCount = alignedMatches;

            // Fill unresolved slots with quantity-derived candidates to improve coverage.
            for (std::size_t index = 0; index < aggregateAlignedNames.size(); ++index)
            {
                if (!aggregateAlignedNames[index].empty() || index >= uiQuantities->size())
                {
                    continue;
                }

                const int quantity = (*uiQuantities)[index];
                if (quantity <= 0)
                {
                    continue;
                }

                std::string fillHint = ResolveUniqueQuantityNameHint(aggregateQuantityKeys, quantity);
                if (fillHint.empty())
                {
                    fillHint = ResolveTopQuantityNameHints(aggregateQuantityKeys, quantity, 4);
                }
                if (!fillHint.empty())
                {
                    aggregateAlignedNames[index] = fillHint;
                }
            }

            aggregateFallbackKeys = aggregateAlignedNames;
            aggregateNonEmptyCount = CountNonEmptyKeys(aggregateFallbackKeys);

            const bool strongAggregateAlignment =
                uiQuantities->size() > 0
                && (alignedMatches * 3 >= uiQuantities->size() * 2);
            if (strongAggregateAlignment)
            {
                outKeys->assign(aggregateFallbackKeys.begin(), aggregateFallbackKeys.end());
                if (outSource != 0)
                {
                    std::stringstream source;
                    source << "trader_owned_aggregate aligned_matches=" << alignedMatches
                           << "/" << uiQuantities->size()
                           << " non_empty=" << aggregateNonEmptyCount
                           << " ownership_keys=" << aggregateKeys.size()
                           << " ownership_quantity_keys=" << aggregateQuantityKeys.size()
                           << " self_inventory_candidates=" << selfInventoryCandidateCount
                           << " trader_owned_candidates=" << inventoryCandidates.size();
                    *outSource = source.str();
                }
                if (outQuantityKeys != 0)
                {
                    outQuantityKeys->assign(
                        aggregateQuantityKeys.begin(),
                        aggregateQuantityKeys.end());
                }
                return true;
            }
        }
    }

    const bool resolved = TryResolveInventoryNameKeysFromCandidates(
        inventoryCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys);
    if (!resolved)
    {
        if (!aggregateFallbackKeys.empty())
        {
            outKeys->assign(aggregateFallbackKeys.begin(), aggregateFallbackKeys.end());
            if (outSource != 0)
            {
                std::stringstream source;
                source << "trader_owned_aggregate_fallback aligned_matches=" << aggregateAlignedMatchCount
                       << "/" << (uiQuantities == 0 ? 0 : uiQuantities->size())
                       << " non_empty=" << aggregateNonEmptyCount
                       << " ownership_keys=" << aggregateKeys.size()
                       << " ownership_quantity_keys=" << aggregateQuantityKeys.size()
                       << " self_inventory_candidates=" << selfInventoryCandidateCount
                       << " trader_owned_candidates=" << inventoryCandidates.size();
                *outSource = source.str();
            }
            if (outQuantityKeys != 0)
            {
                outQuantityKeys->assign(
                    aggregateQuantityKeys.begin(),
                    aggregateQuantityKeys.end());
            }
            return true;
        }

        return false;
    }

    const std::size_t resolvedNonEmptyCount = CountNonEmptyKeys(*outKeys);
    if (!aggregateFallbackKeys.empty() && aggregateAlignmentComputed)
    {
        // Only fall back to aggregate when direct candidate resolution produced no usable names.
        // Aggregate blending introduces heavy ambiguity across many owned inventories.
        if (resolvedNonEmptyCount == 0 && aggregateNonEmptyCount > 0)
        {
            outKeys->assign(aggregateFallbackKeys.begin(), aggregateFallbackKeys.end());
            if (outSource != 0)
            {
                std::stringstream source;
                source << "trader_owned_aggregate_coverage_fallback aligned_matches="
                       << aggregateAlignedMatchCount
                       << "/" << (uiQuantities == 0 ? 0 : uiQuantities->size())
                       << " non_empty=" << aggregateNonEmptyCount
                       << " ownership_keys=" << aggregateKeys.size()
                       << " ownership_quantity_keys=" << aggregateQuantityKeys.size()
                       << " self_inventory_candidates=" << selfInventoryCandidateCount
                       << " trader_owned_candidates=" << inventoryCandidates.size();
                *outSource = source.str();
            }
            if (outQuantityKeys != 0)
            {
                outQuantityKeys->assign(
                    aggregateQuantityKeys.begin(),
                    aggregateQuantityKeys.end());
            }
            return true;
        }
    }

    if (outSource != 0)
    {
        std::stringstream source;
        source << *outSource
               << " non_empty=" << resolvedNonEmptyCount
               << " self_inventory_candidates=" << selfInventoryCandidateCount
               << " trader_owned_candidates=" << inventoryCandidates.size();
        *outSource = source.str();
    }
    return true;
}

bool TryResolveTraderInventoryNameKeysFromNearbyObjects(
    MyGUI::Widget* traderParent,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys)
{
    if (outKeys == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    std::vector<InventoryCandidateInfo> inventoryCandidates;
    std::size_t scannedObjects = 0;
    std::size_t inventoryObjects = 0;
    bool hasFocusedTraderContext = false;

    Character* dialogueTarget = 0;
    Character* dialogueSpeaker = 0;
    std::string dialogueReason;
    if (TryResolvePreferredDialogueTraderTarget(&dialogueTarget, &dialogueSpeaker, &dialogueReason)
        && dialogueTarget != 0)
    {
        hasFocusedTraderContext = true;
        std::size_t focusedScanned = 0;
        std::size_t focusedInventoryObjects = 0;
        CollectNearbyInventoryCandidates(
            dialogueTarget->getPosition(),
            &inventoryCandidates,
            &focusedScanned,
            &focusedInventoryObjects,
            220.0f,
            160,
            2200,
            "nearby_dialog");
        scannedObjects += focusedScanned;
        inventoryObjects += focusedInventoryObjects;

        bool targetInventoryVisible = false;
        TryResolveCharacterInventoryVisible(dialogueTarget, &targetInventoryVisible);
        if (dialogueTarget->inventory != 0)
        {
            std::stringstream source;
            source << dialogueReason << " explicit_target_inv=true";
            AddInventoryCandidateUnique(
                &inventoryCandidates,
                dialogueTarget->inventory,
                source.str(),
                dialogueTarget->isATrader(),
                targetInventoryVisible,
                1900);
        }
    }

    Character* captionTrader = 0;
    int captionScore = 0;
    if (TryResolveCaptionMatchedTraderCharacter(traderParent, &captionTrader, &captionScore)
        && captionTrader != 0)
    {
        hasFocusedTraderContext = true;
        std::size_t captionScanned = 0;
        std::size_t captionInventoryObjects = 0;
        CollectNearbyInventoryCandidates(
            captionTrader->getPosition(),
            &inventoryCandidates,
            &captionScanned,
            &captionInventoryObjects,
            260.0f,
            192,
            1300 + (captionScore / 4),
            "nearby_caption");
        scannedObjects += captionScanned;
        inventoryObjects += captionInventoryObjects;
    }

    if (!hasFocusedTraderContext)
    {
        Character* selectedCharacter = ou->player->selectedCharacter.getCharacter();
        const Ogre::Vector3 fallbackCenter =
            selectedCharacter != 0 ? selectedCharacter->getPosition() : ou->getCameraCenter();
        std::size_t fallbackScanned = 0;
        std::size_t fallbackInventoryObjects = 0;
        CollectNearbyInventoryCandidates(
            fallbackCenter,
            &inventoryCandidates,
            &fallbackScanned,
            &fallbackInventoryObjects,
            900.0f,
            512,
            0,
            "nearby_world");
        scannedObjects += fallbackScanned;
        inventoryObjects += fallbackInventoryObjects;
    }

    const bool resolved = TryResolveInventoryNameKeysFromCandidates(
        inventoryCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys);

    if (!resolved)
    {
        return false;
    }

    if (outSource != 0)
    {
        std::stringstream line;
        line << *outSource
             << " nearby_focused=" << (hasFocusedTraderContext ? "true" : "false")
             << " nearby_candidates=" << inventoryCandidates.size()
             << " nearby_scanned=" << scannedObjects
             << " nearby_with_inventory=" << inventoryObjects;
        *outSource = line.str();
    }

    return true;
}

bool TryResolveTraderInventoryNameKeysFromNearbyShopCounters(
    MyGUI::Widget* traderParent,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys)
{
    if (traderParent == 0 || outKeys == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    std::vector<InventoryCandidateInfo> nearbyCandidates;
    std::size_t scannedObjects = 0;
    std::size_t inventoryObjects = 0;
    bool hasFocusedTraderContext = false;

    Character* dialogueTarget = 0;
    Character* dialogueSpeaker = 0;
    std::string dialogueReason;
    if (TryResolvePreferredDialogueTraderTarget(&dialogueTarget, &dialogueSpeaker, &dialogueReason)
        && dialogueTarget != 0)
    {
        hasFocusedTraderContext = true;
        std::size_t focusedScanned = 0;
        std::size_t focusedInventoryObjects = 0;
        CollectNearbyInventoryCandidates(
            dialogueTarget->getPosition(),
            &nearbyCandidates,
            &focusedScanned,
            &focusedInventoryObjects,
            360.0f,
            256,
            1400,
            "nearby_shop_dialog");
        scannedObjects += focusedScanned;
        inventoryObjects += focusedInventoryObjects;
    }

    Character* captionTrader = 0;
    int captionScore = 0;
    if (TryResolveCaptionMatchedTraderCharacter(traderParent, &captionTrader, &captionScore)
        && captionTrader != 0)
    {
        hasFocusedTraderContext = true;
        std::size_t captionScanned = 0;
        std::size_t captionInventoryObjects = 0;
        CollectNearbyInventoryCandidates(
            captionTrader->getPosition(),
            &nearbyCandidates,
            &captionScanned,
            &captionInventoryObjects,
            360.0f,
            256,
            1200 + (captionScore / 8),
            "nearby_shop_caption");
        scannedObjects += captionScanned;
        inventoryObjects += captionInventoryObjects;
    }

    if (!hasFocusedTraderContext)
    {
        return false;
    }

    std::vector<InventoryCandidateInfo> shopCounterCandidates;
    const int expected = static_cast<int>(expectedEntryCount);
    for (std::size_t index = 0; index < nearbyCandidates.size(); ++index)
    {
        const InventoryCandidateInfo& nearbyCandidate = nearbyCandidates[index];
        if (nearbyCandidate.inventory == 0)
        {
            continue;
        }

        const std::string sourceLower = NormalizeSearchText(nearbyCandidate.source);
        if (!IsShopCounterCandidateSource(sourceLower))
        {
            continue;
        }

        int priorityBias = nearbyCandidate.priorityBias + 1200;
        const int itemCount = static_cast<int>(InventoryItemCountForLog(nearbyCandidate.inventory));
        if (expected > 0)
        {
            const int diff = itemCount > expected ? itemCount - expected : expected - itemCount;
            priorityBias += 900 - diff * 140;
            if (diff <= 2)
            {
                priorityBias += 240;
            }
            else if (diff <= 5)
            {
                priorityBias += 100;
            }
        }

        std::stringstream focusedSource;
        focusedSource << nearbyCandidate.source
                      << " shop_counter_focus=true";
        AddInventoryCandidateUnique(
            &shopCounterCandidates,
            nearbyCandidate.inventory,
            focusedSource.str(),
            true,
            nearbyCandidate.visible,
            priorityBias);
    }

    if (shopCounterCandidates.empty())
    {
        return false;
    }

    const bool resolved = TryResolveInventoryNameKeysFromCandidates(
        shopCounterCandidates,
        expectedEntryCount,
        uiQuantities,
        outKeys,
        outSource,
        outQuantityKeys);
    if (!resolved)
    {
        return false;
    }

    if (outSource != 0)
    {
        std::stringstream line;
        line << *outSource
             << " nearby_shop_candidates=" << shopCounterCandidates.size()
             << " nearby_shop_scanned=" << scannedObjects
             << " nearby_shop_with_inventory=" << inventoryObjects;
        *outSource = line.str();
    }

    return true;
}

std::size_t InventoryItemCountForLog(Inventory* inventory)
{
    if (inventory == 0)
    {
        return 0;
    }

    const lektor<Item*>& items = inventory->getAllItems();
    if (!items.valid())
    {
        return 0;
    }

    return items.size();
}

std::string CharacterNameForLog(Character* character)
{
    if (character == 0)
    {
        return "<null>";
    }

    if (!character->displayName.empty())
    {
        return character->displayName;
    }

    const std::string objectName = character->getName();
    if (!objectName.empty())
    {
        return objectName;
    }

    if (character->data != 0 && !character->data->name.empty())
    {
        return character->data->name;
    }

    return "<unnamed>";
}

bool TryResolveTraderQuantityNameKeysFromCaption(
    MyGUI::Widget* traderParent,
    std::vector<QuantityNameKey>* outKeys,
    std::string* outSource)
{
    if (traderParent == 0 || outKeys == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    outKeys->clear();
    if (outSource != 0)
    {
        outSource->clear();
    }

    MyGUI::Window* owningWindow = FindOwningWindow(traderParent);
    const std::string windowCaption = owningWindow == 0 ? "" : owningWindow->getCaption().asUTF8();

    const ogre_unordered_set<Character*>::type& activeCharacters = ou->getCharacterUpdateList();
    Character* bestCharacter = 0;
    bool bestVisible = false;
    int bestScore = -1000000;

    for (ogre_unordered_set<Character*>::type::const_iterator it = activeCharacters.begin();
         it != activeCharacters.end();
         ++it)
    {
        Character* candidate = *it;
        if (candidate == 0 || candidate->inventory == 0 || !candidate->isATrader())
        {
            continue;
        }

        bool inventoryVisible = false;
        TryResolveCharacterInventoryVisible(candidate, &inventoryVisible);

        const std::string characterName = CharacterNameForLog(candidate);
        const bool captionMatchesCharacter =
            !windowCaption.empty()
            && !characterName.empty()
            && ContainsAsciiCaseInsensitive(windowCaption, characterName.c_str());

        const int itemCount = static_cast<int>(InventoryItemCountForLog(candidate->inventory));
        int score = 0;
        score += 300;
        if (inventoryVisible)
        {
            score += 120;
        }
        if (captionMatchesCharacter)
        {
            score += 500;
        }
        score += itemCount;

        if (score > bestScore)
        {
            bestScore = score;
            bestCharacter = candidate;
            bestVisible = inventoryVisible;
        }
    }

    if (bestCharacter == 0 || bestCharacter->inventory == 0)
    {
        return false;
    }

    if (!TryExtractQuantityNameKeysFromInventory(bestCharacter->inventory, outKeys))
    {
        return false;
    }

    if (outSource != 0)
    {
        std::stringstream source;
        source << "caption_trader:" << CharacterNameForLog(bestCharacter)
               << " visible=" << (bestVisible ? "true" : "false")
               << " item_count=" << InventoryItemCountForLog(bestCharacter->inventory);
        *outSource = source.str();
    }

    return true;
}

void LogInventoryBindingDiagnostics(std::size_t expectedEntryCount)
{
    if (ou == 0 || ou->player == 0)
    {
        LogWarnLine("inventory binding diagnostics: GameWorld/player unavailable");
        return;
    }

    std::stringstream header;
    header << "inventory binding diagnostics begin expected_entries=" << expectedEntryCount
           << " gui_display_object=" << RootObjectDisplayNameForLog(ou->guiDisplayObject.getRootObject());
    LogWarnLine(header.str());

    Character* selectedCharacter = ou->player->selectedCharacter.getCharacter();
    RootObject* mouseTargetRoot = ou->player->mouseRightTarget;
    Character* mouseTargetCharacter = mouseTargetRoot == 0 ? 0 : mouseTargetRoot->getHandle().getCharacter();

    std::stringstream selectedLine;
    selectedLine << "binding selected_character=" << CharacterNameForLog(selectedCharacter)
                 << " selected_inventory_items=" << InventoryItemCountForLog(
                        selectedCharacter == 0 ? 0 : selectedCharacter->inventory)
                 << " mouse_target_root=" << RootObjectDisplayNameForLog(mouseTargetRoot)
                 << " mouse_target_character=" << CharacterNameForLog(mouseTargetCharacter)
                 << " mouse_target_inventory_items=" << InventoryItemCountForLog(
                        mouseTargetCharacter == 0 ? 0 : mouseTargetCharacter->inventory);
    LogWarnLine(selectedLine.str());

    const lektor<Character*>& allPlayerCharacters = ou->player->getAllPlayerCharacters();
    std::size_t index = 0;
    for (lektor<Character*>::const_iterator iter = allPlayerCharacters.begin(); iter != allPlayerCharacters.end(); ++iter)
    {
        Character* playerChar = *iter;
        if (playerChar == 0)
        {
            continue;
        }

        Dialogue* dialogue = playerChar->dialogue;
        Character* target = dialogue == 0 ? 0 : dialogue->getConversationTarget().getCharacter();

        bool playerInventoryVisible = false;
        bool targetInventoryVisible = false;
        TryResolveCharacterInventoryVisible(playerChar, &playerInventoryVisible);
        TryResolveCharacterInventoryVisible(target, &targetInventoryVisible);

        const bool dialogActive = dialogue != 0 && !dialogue->conversationHasEndedPrettyMuch();
        const bool engaged = (target != 0) && (playerChar->_isEngagedWithAPlayer || target->_isEngagedWithAPlayer);

        std::stringstream line;
        line << "binding player_char[" << index << "]=" << CharacterNameForLog(playerChar)
             << " inv_visible=" << (playerInventoryVisible ? "true" : "false")
             << " inv_items=" << InventoryItemCountForLog(playerChar->inventory)
             << " has_dialogue=" << (dialogue != 0 ? "true" : "false")
             << " dialog_active=" << (dialogActive ? "true" : "false")
             << " engaged=" << (engaged ? "true" : "false")
             << " target=" << CharacterNameForLog(target)
             << " target_trader=" << (target != 0 && target->isATrader() ? "true" : "false")
             << " target_inv_visible=" << (targetInventoryVisible ? "true" : "false")
             << " target_inv_items=" << InventoryItemCountForLog(target == 0 ? 0 : target->inventory);
        LogWarnLine(line.str());

        ++index;
        if (index >= 12)
        {
            break;
        }
    }

    const ogre_unordered_set<Character*>::type& activeCharacters = ou->getCharacterUpdateList();
    std::size_t activeCount = 0;
    std::size_t activeTraders = 0;
    std::size_t activeVisibleInventories = 0;
    std::size_t activeExactCountInventories = 0;
    std::size_t loggedCandidates = 0;

    for (ogre_unordered_set<Character*>::type::const_iterator it = activeCharacters.begin();
         it != activeCharacters.end();
         ++it)
    {
        Character* candidate = *it;
        if (candidate == 0)
        {
            continue;
        }

        ++activeCount;

        bool inventoryVisible = false;
        TryResolveCharacterInventoryVisible(candidate, &inventoryVisible);
        const bool isTrader = candidate->isATrader();
        const std::size_t inventoryItemCount = InventoryItemCountForLog(candidate->inventory);
        const bool exactCountMatch = inventoryItemCount == expectedEntryCount && inventoryItemCount > 0;

        if (isTrader)
        {
            ++activeTraders;
        }
        if (inventoryVisible)
        {
            ++activeVisibleInventories;
        }
        if (exactCountMatch)
        {
            ++activeExactCountInventories;
        }

        if (loggedCandidates < 12 && (isTrader || inventoryVisible || exactCountMatch))
        {
            std::stringstream line;
            line << "binding active_char[" << loggedCandidates << "]=" << CharacterNameForLog(candidate)
                 << " trader=" << (isTrader ? "true" : "false")
                 << " inv_visible=" << (inventoryVisible ? "true" : "false")
                 << " inv_items=" << inventoryItemCount
                 << " exact_items_match=" << (exactCountMatch ? "true" : "false");
            LogWarnLine(line.str());
            ++loggedCandidates;
        }
    }

    std::stringstream summary;
    summary << "binding active_chars_summary total=" << activeCount
            << " traders=" << activeTraders
            << " inv_visible=" << activeVisibleInventories
            << " inv_items_match_expected=" << activeExactCountInventories;
    LogWarnLine(summary.str());

    Character* selectedForCenter = ou->player->selectedCharacter.getCharacter();
    Ogre::Vector3 nearbyCenter = selectedForCenter != 0 ? selectedForCenter->getPosition() : ou->getCameraCenter();
    std::vector<InventoryCandidateInfo> nearbyCandidates;
    std::size_t nearbyScannedObjects = 0;
    std::size_t nearbyInventoryObjects = 0;
    CollectNearbyInventoryCandidates(
        nearbyCenter,
        &nearbyCandidates,
        &nearbyScannedObjects,
        &nearbyInventoryObjects);

    std::stringstream nearbySummary;
    nearbySummary << "binding nearby_inventory_summary scanned_objects=" << nearbyScannedObjects
                  << " with_inventory=" << nearbyInventoryObjects
                  << " candidates=" << nearbyCandidates.size();
    LogWarnLine(nearbySummary.str());

    std::size_t nearbyLogged = 0;
    for (std::size_t nearbyIndex = 0;
         nearbyIndex < nearbyCandidates.size() && nearbyLogged < 12;
         ++nearbyIndex)
    {
        const InventoryCandidateInfo& candidate = nearbyCandidates[nearbyIndex];
        std::vector<std::string> keys;
        const bool hasKeys = TryExtractSearchKeysFromInventory(candidate.inventory, &keys);

        std::stringstream line;
        line << "binding nearby_candidate[" << nearbyLogged << "]"
             << " key_count=" << (hasKeys ? keys.size() : 0)
             << " visible=" << (candidate.visible ? "true" : "false")
             << " trader_preferred=" << (candidate.traderPreferred ? "true" : "false")
             << " source=\"" << TruncateForLog(candidate.source, 220) << "\"";
        if (hasKeys && !keys.empty())
        {
            line << " key0=\"" << TruncateForLog(keys[0], 48) << "\"";
        }
        LogWarnLine(line.str());
        ++nearbyLogged;
    }

    LogWarnLine("inventory binding diagnostics end");
}

bool TryResolveAndCacheTraderPanelInventoryBinding(
    MyGUI::Widget* traderParent,
    MyGUI::Widget* entriesRoot,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    TraderPanelInventoryBinding* outBinding,
    std::string* outStatus)
{
    if (outBinding != 0)
    {
        outBinding->traderParent = 0;
        outBinding->entriesRoot = 0;
        outBinding->inventory = 0;
        outBinding->stage.clear();
        outBinding->source.clear();
        outBinding->expectedEntryCount = 0;
        outBinding->nonEmptyKeyCount = 0;
        outBinding->lastSeenTick = 0;
    }
    if (outStatus != 0)
    {
        outStatus->clear();
    }

    if (traderParent == 0 || entriesRoot == 0 || expectedEntryCount == 0)
    {
        if (outStatus != 0)
        {
            *outStatus = "panel_or_entries_missing";
        }
        return false;
    }

    TraderPanelInventoryBinding cached;
    if (TryGetTraderPanelInventoryBinding(traderParent, entriesRoot, expectedEntryCount, &cached))
    {
        if (outBinding != 0)
        {
            *outBinding = cached;
        }
        if (outStatus != 0)
        {
            *outStatus = "cached";
        }
        return true;
    }

    std::vector<std::string> keys;
    std::vector<QuantityNameKey> quantityKeys;
    std::string source;
    Inventory* selectedInventory = 0;

    if (TryResolveTraderInventoryNameKeysFromInventoryGuiMap(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &keys,
            &source,
            &quantityKeys,
            &selectedInventory))
    {
        const std::size_t nonEmptyKeyCount = CountNonEmptyKeys(keys);
        const bool coverageStrong =
            expectedEntryCount < 8 || nonEmptyKeyCount * 2 >= expectedEntryCount;
        if (selectedInventory != 0 && coverageStrong)
        {
            RegisterTraderPanelInventoryBinding(
                traderParent,
                entriesRoot,
                selectedInventory,
                "inventory_gui_map",
                source,
                expectedEntryCount,
                nonEmptyKeyCount);

            if (TryGetTraderPanelInventoryBinding(
                    traderParent,
                    entriesRoot,
                    expectedEntryCount,
                    outBinding))
            {
                if (outStatus != 0)
                {
                    *outStatus = "resolved_inventory_gui_map";
                }
                return true;
            }
        }
    }

    keys.clear();
    quantityKeys.clear();
    source.clear();
    selectedInventory = 0;
    if (TryResolveTraderInventoryNameKeysFromSectionWidgetMap(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &keys,
            &source,
            &quantityKeys,
            &selectedInventory))
    {
        const std::size_t nonEmptyKeyCount = CountNonEmptyKeys(keys);
        const int directEntriesMatches =
            ExtractTaggedIntValue(source, "direct_entries_matches=");
        const int directBackpackMatches =
            ExtractTaggedIntValue(source, "direct_backpack_matches=");
        const bool directSectionMatch =
            directEntriesMatches > 0 || directBackpackMatches > 0;
        const bool coverageStrong =
            expectedEntryCount < 8 || nonEmptyKeyCount * 2 >= expectedEntryCount;

        if (selectedInventory != 0 && directSectionMatch && coverageStrong)
        {
            RegisterTraderPanelInventoryBinding(
                traderParent,
                entriesRoot,
                selectedInventory,
                "section_widget",
                source,
                expectedEntryCount,
                nonEmptyKeyCount);

            if (TryGetTraderPanelInventoryBinding(
                    traderParent,
                    entriesRoot,
                    expectedEntryCount,
                    outBinding))
            {
                if (outStatus != 0)
                {
                    *outStatus = "resolved_section_widget";
                }
                return true;
            }
        }
    }

    keys.clear();
    quantityKeys.clear();
    source.clear();
    selectedInventory = 0;
    if (TryResolveTraderInventoryNameKeysFromWidgetBindings(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &keys,
            &source,
            &quantityKeys,
            &selectedInventory))
    {
        const std::size_t nonEmptyKeyCount = CountNonEmptyKeys(keys);
        const bool coverageStrong =
            expectedEntryCount < 8 || nonEmptyKeyCount * 2 >= expectedEntryCount;
        const std::string sourceLower = NormalizeSearchText(source);
        const bool hasGuiMatchTag =
            sourceLower.find("widget gui match") != std::string::npos;

        if (selectedInventory != 0 && hasGuiMatchTag && coverageStrong)
        {
            RegisterTraderPanelInventoryBinding(
                traderParent,
                entriesRoot,
                selectedInventory,
                "widget",
                source,
                expectedEntryCount,
                nonEmptyKeyCount);

            if (TryGetTraderPanelInventoryBinding(
                    traderParent,
                    entriesRoot,
                    expectedEntryCount,
                    outBinding))
            {
                if (outStatus != 0)
                {
                    *outStatus = "resolved_widget";
                }
                return true;
            }
        }
    }

    if (outStatus != 0)
    {
        *outStatus = "high_confidence_binding_not_found";
    }
    return false;
}

bool TryResolveTraderInventoryNameKeys(
    MyGUI::Widget* traderParent,
    std::size_t expectedEntryCount,
    const std::vector<int>* uiQuantities,
    std::vector<std::string>* outKeys,
    std::string* outSource,
    std::vector<QuantityNameKey>* outQuantityKeys,
    bool preferCoverageFallbackWhenWidgetOpaque)
{
    if (traderParent == 0 || outKeys == 0 || ou == 0 || ou->player == 0)
    {
        return false;
    }

    outKeys->clear();
    if (outSource != 0)
    {
        outSource->clear();
    }
    if (outQuantityKeys != 0)
    {
        outQuantityKeys->clear();
    }

    if (g_lockedKeysetTraderParent != 0 && g_lockedKeysetTraderParent != traderParent)
    {
        ClearLockedKeysetSource();
    }

    std::vector<std::string> bestKeys;
    std::vector<QuantityNameKey> bestQuantityKeys;
    std::string bestSource;
    std::string bestStage;
    int bestScore = -1000000;
    std::vector<std::string> bestCoverageKeys;
    std::vector<QuantityNameKey> bestCoverageQuantityKeys;
    std::string bestCoverageSource;
    std::string bestCoverageStage;
    int bestCoverageScore = -1000000;
    int bestQueryMatchCount = -1;
    int bestCoverageQueryMatchCount = -1;
    bool usedCoverageFallback = false;
    std::vector<std::string> candidateDiagnostics;

    const bool hasLockedSourceForQuery =
        !g_searchQueryNormalized.empty()
        && g_lockedKeysetTraderParent == traderParent
        && !g_lockedKeysetSourceId.empty()
        && g_lockedKeysetExpectedCount > 0
        && expectedEntryCount > 0
        && AbsoluteDiffSize(g_lockedKeysetExpectedCount, expectedEntryCount) <= 6;

    struct ResolvedKeySetCandidate
    {
        const char* stage;
        std::vector<std::string> keys;
        std::vector<QuantityNameKey> quantityKeys;
        std::string source;
    };

    ResolvedKeySetCandidate stageCandidate;

    auto considerCandidate = [&](const ResolvedKeySetCandidate& candidate)
    {
        if (candidate.keys.empty())
        {
            return;
        }

        const int keyCount = static_cast<int>(candidate.keys.size());
        int nonEmptyKeyCount = 0;
        for (std::size_t keyIndex = 0; keyIndex < candidate.keys.size(); ++keyIndex)
        {
            if (!candidate.keys[keyIndex].empty())
            {
                ++nonEmptyKeyCount;
            }
        }
        const int expected = static_cast<int>(expectedEntryCount);
        const int diff = expected > 0
            ? (nonEmptyKeyCount > expected ? nonEmptyKeyCount - expected : expected - nonEmptyKeyCount)
            : 0;
        const bool lowCoverage = expected >= 8 && nonEmptyKeyCount * 2 < expected;
        const int emptyKeyCount = keyCount - nonEmptyKeyCount;
        const int sourceQueryMatches = ExtractTaggedIntValue(candidate.source, "query_matches=");
        int keyQueryMatches = 0;
        if (!g_searchQueryNormalized.empty())
        {
            for (std::size_t keyIndex = 0; keyIndex < candidate.keys.size(); ++keyIndex)
            {
                if (!candidate.keys[keyIndex].empty()
                    && candidate.keys[keyIndex].find(g_searchQueryNormalized) != std::string::npos)
                {
                    ++keyQueryMatches;
                }
            }
        }
        const int effectiveQueryMatches =
            sourceQueryMatches >= 0 ? sourceQueryMatches : keyQueryMatches;
        int sourceAlignedMatches = -1;
        int sourceAlignedTotal = -1;
        const bool hasSourceAlignedMatches =
            TryExtractTaggedFraction(
                candidate.source,
                "aligned_matches=",
                &sourceAlignedMatches,
                &sourceAlignedTotal);

        int score = 0;
        score += nonEmptyKeyCount * 18;
        score -= emptyKeyCount * 110;
        score -= diff * 64;

        const std::string stageName = candidate.stage == 0 ? "" : candidate.stage;
        if (stageName == "widget")
        {
            score += 1800;
        }
        else if (stageName == "section_widget")
        {
            score += 5200;
        }
        else if (stageName == "recent_refresh")
        {
            score += 4200;
        }
        else if (stageName == "hovered_widget")
        {
            score += 3200;
        }
        else if (stageName == "ownership")
        {
            score += 900;
        }
        else if (stageName == "nearby_shop_counter")
        {
            score += 700;
        }
        else if (stageName == "nearby")
        {
            score -= 700;
        }

        if (expected > 0 && nonEmptyKeyCount == expected)
        {
            score += 1500;
        }
        else if (diff <= 1)
        {
            score += 520;
        }
        else if (lowCoverage)
        {
            score -= 3200;
        }

        if (!g_searchQueryNormalized.empty() && g_searchQueryNormalized.size() >= 3)
        {
            if (effectiveQueryMatches > 0)
            {
                score += 2600;
                score += effectiveQueryMatches * 760;
                if (effectiveQueryMatches >= 2)
                {
                    score += 620;
                }
            }
            else if (effectiveQueryMatches == 0)
            {
                score -= 3200;
                if (stageName == "nearby_shop_counter")
                {
                    score -= 4200;
                }
            }

            if (hasSourceAlignedMatches
                && sourceAlignedTotal >= 10
                && sourceAlignedMatches * 3 < sourceAlignedTotal)
            {
                score -= 1400;
                if (stageName == "nearby_shop_counter")
                {
                    score -= 1800;
                }
            }
        }

        if (hasSourceAlignedMatches && sourceAlignedTotal >= 10)
        {
            if (sourceAlignedMatches * 3 < sourceAlignedTotal)
            {
                score -= 2600;
                if (stageName == "nearby_shop_counter")
                {
                    score -= 6200;
                }
                else if (stageName == "nearby")
                {
                    score -= 2600;
                }
            }
            else if (sourceAlignedMatches * 2 >= sourceAlignedTotal)
            {
                score += 900;
            }
        }

        const std::string sourceLower = NormalizeSearchText(candidate.source);
        const std::string sourceId = BuildKeysetSourceId(candidate.source);
        const bool sourceIsActiveNonTrader =
            sourceLower.find("active char") != std::string::npos
            && sourceLower.find("trader false") != std::string::npos;

        if (preferCoverageFallbackWhenWidgetOpaque && sourceIsActiveNonTrader)
        {
            return;
        }
        if (!sourceLower.empty())
        {
            if (sourceLower.find("self inventory true") != std::string::npos)
            {
                if (expected >= 8 && nonEmptyKeyCount * 3 < expected * 2)
                {
                    score -= 2800;
                }
                else
                {
                    score += 700;
                }
            }
            if (IsShopCounterCandidateSource(sourceLower))
            {
                score += 520;
                if (expected >= 8)
                {
                    if (diff <= 2)
                    {
                        score += 360;
                    }
                    else if (diff <= 5)
                    {
                        score += 220;
                    }
                }
            }
            if (sourceLower.find("trader furniture shop") != std::string::npos)
            {
                score += 900;
            }
            if (sourceLower.find("trader furniture storage") != std::string::npos)
            {
                score -= 360;
            }
            if (sourceLower.find("trader furniture resource") != std::string::npos)
            {
                score -= 520;
            }
            if (sourceLower.find("nearby world") != std::string::npos)
            {
                score -= 1800;
            }
            if (sourceLower.find("nearby caption") != std::string::npos)
            {
                score += 280;
            }
            if (sourceLower.find("nearby dialog") != std::string::npos)
            {
                score += 360;
            }
            if (sourceLower.find("caption trader") != std::string::npos)
            {
                score += 160;
            }
            if (sourceLower.find("dialog target") != std::string::npos
                && sourceLower.find("trader true") != std::string::npos)
            {
                score += 260;
            }
            if (sourceLower.find("active char") != std::string::npos
                && sourceLower.find("trader true") != std::string::npos)
            {
                score += 140;
            }
            if (sourceIsActiveNonTrader)
            {
                score -= 5200;
            }
            if (sourceLower.find("selected item") != std::string::npos)
            {
                score += 520;
            }
            if (sourceLower.find("widget") != std::string::npos)
            {
                score += 880;
            }
            if (sourceLower.find("section widget map") != std::string::npos)
            {
                score += 1800;
            }
            if (sourceLower.find("recent refresh") != std::string::npos)
            {
                score += 1400;
            }
            if (sourceLower.find("visible true") != std::string::npos)
            {
                score += 120;
            }
            if (sourceLower.find("nearby") != std::string::npos)
            {
                score += 100;
            }
            if (sourceLower.find("shop counter focus true") != std::string::npos)
            {
                score += 300;
            }
            if (sourceLower.find("root candidate") != std::string::npos)
            {
                score -= 180;
            }
            if (sourceLower.find("selected true") != std::string::npos
                && sourceLower.find("trader true") == std::string::npos)
            {
                score -= 120;
            }
        }

        int lockBoost = 0;
        if (hasLockedSourceForQuery)
        {
            const bool stageMatchesLock = !g_lockedKeysetStage.empty() && stageName == g_lockedKeysetStage;
            const bool sourceMatchesLock = !sourceId.empty() && sourceId == g_lockedKeysetSourceId;

            if (sourceMatchesLock && stageMatchesLock)
            {
                lockBoost = 6200;
            }
            else if (sourceMatchesLock)
            {
                lockBoost = 4200;
            }
            else if (stageMatchesLock)
            {
                lockBoost = 1200;
            }

            if (lockBoost > 0)
            {
                score += lockBoost;
            }
            else if (IsRiskyCoverageFallbackSource(sourceLower))
            {
                score -= 1600;
            }
        }

        {
            std::stringstream diag;
            diag << "inventory keyset candidate stage=" << candidate.stage
                 << " key_count=" << candidate.keys.size()
                 << " non_empty=" << nonEmptyKeyCount
                 << " expected=" << expectedEntryCount
                 << " score=" << score
                 << " lock_boost=" << lockBoost
                 << " source=\"" << TruncateForLog(candidate.source, 220) << "\"";
            candidateDiagnostics.push_back(diag.str());
        }

        if (score > bestScore)
        {
            bestScore = score;
            bestKeys = candidate.keys;
            bestQuantityKeys = candidate.quantityKeys;
            bestSource = candidate.source;
            bestStage = stageName;
            bestQueryMatchCount = effectiveQueryMatches;
        }

        if (!lowCoverage && score > bestCoverageScore)
        {
            bestCoverageScore = score;
            bestCoverageKeys = candidate.keys;
            bestCoverageQuantityKeys = candidate.quantityKeys;
            bestCoverageSource = candidate.source;
            bestCoverageStage = stageName;
            bestCoverageQueryMatchCount = effectiveQueryMatches;
        }
    };

    stageCandidate.stage = "recent_refresh";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromRecentRefreshedInventories(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "section_widget";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromSectionWidgetMap(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "widget";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromWidgetBindings(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "hovered_widget";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromHoveredWidget(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "caption";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromWindowCaption(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "dialogue";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromDialogue(
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "active";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromActiveCharacters(
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "nearby_shop_counter";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromNearbyShopCounters(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "ownership";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromTraderOwnership(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "selected_item";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromSelectedItemHandles(
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    stageCandidate.stage = "nearby";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveTraderInventoryNameKeysFromNearbyObjects(
            traderParent,
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        if (g_searchQueryNormalized.empty())
        {
            considerCandidate(stageCandidate);
        }
    }

    std::vector<RootObject*> candidates;
    AddCandidateRootObjectUnique(&candidates, ou->guiDisplayObject.getRootObject());
    AddCandidateRootObjectUnique(&candidates, ou->player->selectedObject.getRootObject());
    AddCandidateRootObjectUnique(&candidates, ou->player->selectedCharacter.getRootObject());
    AddCandidateRootObjectUnique(&candidates, ou->player->mouseRightTarget);

    std::vector<InventoryCandidateInfo> inventoryCandidates;
    for (std::size_t candidateIndex = 0; candidateIndex < candidates.size(); ++candidateIndex)
    {
        RootObject* owner = candidates[candidateIndex];
        if (owner == 0)
        {
            continue;
        }

        Inventory* inventory = owner->getInventory();
        if (inventory == 0)
        {
            continue;
        }

        std::stringstream src;
        src << "root_candidate:" << RootObjectDisplayNameForLog(owner)
            << " visible=" << (inventory->isVisible() ? "true" : "false");
        AddInventoryCandidateUnique(
            &inventoryCandidates,
            inventory,
            src.str(),
            false,
            inventory->isVisible());
    }

    stageCandidate.stage = "root";
    stageCandidate.keys.clear();
    stageCandidate.quantityKeys.clear();
    stageCandidate.source.clear();
    if (TryResolveInventoryNameKeysFromCandidates(
            inventoryCandidates,
            expectedEntryCount,
            uiQuantities,
            &stageCandidate.keys,
            &stageCandidate.source,
            &stageCandidate.quantityKeys))
    {
        considerCandidate(stageCandidate);
    }

    if (bestKeys.empty())
    {
        return false;
    }

    if (expectedEntryCount >= 8
        && bestKeys.size() * 2 < expectedEntryCount
        && !bestCoverageKeys.empty())
    {
        bool allowCoverageFallback = true;
        std::string fallbackSkipReason;
        const int currentMatches = bestQueryMatchCount < 0 ? 0 : bestQueryMatchCount;
        const int coverageMatches = bestCoverageQueryMatchCount < 0 ? 0 : bestCoverageQueryMatchCount;
        if (!g_searchQueryNormalized.empty())
        {
            if (coverageMatches < currentMatches)
            {
                allowCoverageFallback = false;
                fallbackSkipReason = "query_match_regression";
            }
        }

        const std::string bestSourceLower = NormalizeSearchText(bestSource);
        const std::string bestCoverageSourceLower = NormalizeSearchText(bestCoverageSource);
        const std::string bestSourceId = BuildKeysetSourceId(bestSource);
        const std::string bestCoverageSourceId = BuildKeysetSourceId(bestCoverageSource);
        const bool currentSourceTraderAnchored = IsTraderAnchoredCandidateSource(bestSourceLower);
        const bool coverageSourceRisky = IsRiskyCoverageFallbackSource(bestCoverageSourceLower);
        const bool coverageSourceNonTraderActive =
            bestCoverageSourceLower.find("active char") != std::string::npos
            && bestCoverageSourceLower.find("trader false") != std::string::npos;
        const bool currentSourceMatchesLock =
            hasLockedSourceForQuery
            && !bestSourceId.empty()
            && bestSourceId == g_lockedKeysetSourceId;
        const bool coverageSourceMatchesLock =
            hasLockedSourceForQuery
            && !bestCoverageSourceId.empty()
            && bestCoverageSourceId == g_lockedKeysetSourceId;
        int currentAlignedMatches = -1;
        int currentAlignedTotal = -1;
        const bool hasCurrentAlignedMatches =
            TryExtractTaggedFraction(
                bestSource,
                "aligned_matches=",
                &currentAlignedMatches,
                &currentAlignedTotal);

        const bool strongQueryEvidence =
            !g_searchQueryNormalized.empty()
            && g_searchQueryNormalized.size() >= 4
            && coverageMatches >= 3
            && coverageMatches >= currentMatches + 2;

        if (allowCoverageFallback && coverageSourceNonTraderActive && !strongQueryEvidence)
        {
            allowCoverageFallback = false;
            fallbackSkipReason = "coverage_non_trader_active";
        }

        if (allowCoverageFallback
            && currentSourceMatchesLock
            && !coverageSourceMatchesLock
            && !strongQueryEvidence)
        {
            allowCoverageFallback = false;
            fallbackSkipReason = "locked_source_preserved";
        }

        if (allowCoverageFallback && currentSourceTraderAnchored && coverageSourceRisky && !strongQueryEvidence)
        {
            allowCoverageFallback = false;
            fallbackSkipReason = "coverage_source_risky_for_trader_anchor";
        }

        int coverageAlignedMatches = -1;
        int coverageAlignedTotal = -1;
        const bool hasCoverageAlignedMatches =
            TryExtractTaggedFraction(
                bestCoverageSource,
                "aligned_matches=",
                &coverageAlignedMatches,
                &coverageAlignedTotal);
        if (allowCoverageFallback
            && hasCoverageAlignedMatches
            && coverageAlignedTotal >= 10
            && coverageAlignedMatches * 2 < coverageAlignedTotal
            && !(strongQueryEvidence && coverageMatches >= 4))
        {
            allowCoverageFallback = false;
            fallbackSkipReason = "coverage_alignment_too_low";
        }

        if (!allowCoverageFallback
            && preferCoverageFallbackWhenWidgetOpaque
            && coverageSourceRisky
            && !coverageSourceNonTraderActive
            && expectedEntryCount >= 8)
        {
            const bool coverageHasStrongCount =
                bestCoverageKeys.size() * 3 >= expectedEntryCount * 2;
            const bool coverageQueryNotWorse = coverageMatches >= currentMatches;
            const bool coverageAlignmentNotWorse =
                !hasCoverageAlignedMatches
                || !hasCurrentAlignedMatches
                || currentAlignedTotal <= 0
                || coverageAlignedTotal <= 0
                || (coverageAlignedMatches * currentAlignedTotal
                    >= currentAlignedMatches * coverageAlignedTotal);
            if (coverageHasStrongCount && coverageQueryNotWorse && coverageAlignmentNotWorse)
            {
                allowCoverageFallback = true;
                fallbackSkipReason.clear();
            }
        }

        std::stringstream decisionSignature;
        decisionSignature
            << (allowCoverageFallback ? "allow" : "skip")
            << "|" << bestKeys.size()
            << "|" << bestCoverageKeys.size()
            << "|" << currentMatches
            << "|" << coverageMatches
            << "|" << bestSource
            << "|" << bestCoverageSource
            << "|" << g_searchQueryNormalized;
        const bool logDecision =
            decisionSignature.str() != g_lastCoverageFallbackDecisionSignature;
        g_lastCoverageFallbackDecisionSignature = decisionSignature.str();

        if (allowCoverageFallback)
        {
            if (logDecision)
            {
                std::stringstream line;
                line << "inventory keyset fallback selected key_count=" << bestCoverageKeys.size()
                     << " expected=" << expectedEntryCount
                     << " current_query_matches=" << currentMatches
                     << " coverage_query_matches=" << coverageMatches
                     << " opaque_prefer_mode=" << (preferCoverageFallbackWhenWidgetOpaque ? "true" : "false")
                     << " replacing_source=\"" << TruncateForLog(bestSource, 220) << "\""
                     << " with_source=\"" << TruncateForLog(bestCoverageSource, 220) << "\"";
                LogWarnLine(line.str());
            }

            bestKeys.swap(bestCoverageKeys);
            bestQuantityKeys.swap(bestCoverageQuantityKeys);
            bestSource = bestCoverageSource;
            bestStage = bestCoverageStage;
            bestScore = bestCoverageScore;
            bestQueryMatchCount = bestCoverageQueryMatchCount;
            usedCoverageFallback = true;
        }
        else
        {
            if (logDecision)
            {
                std::stringstream line;
                line << "inventory keyset fallback skipped reason="
                     << (fallbackSkipReason.empty() ? "unknown" : fallbackSkipReason)
                     << " current_query_matches=" << currentMatches
                     << " coverage_query_matches=" << coverageMatches
                     << " opaque_prefer_mode=" << (preferCoverageFallbackWhenWidgetOpaque ? "true" : "false")
                     << " current_source=\"" << TruncateForLog(bestSource, 160) << "\""
                     << " coverage_source=\"" << TruncateForLog(bestCoverageSource, 160) << "\"";
                LogWarnLine(line.str());
            }
        }
    }
    else
    {
        g_lastCoverageFallbackDecisionSignature.clear();
    }

    if (expectedEntryCount >= 8 && bestKeys.size() * 2 < expectedEntryCount)
    {
        std::stringstream signature;
        signature << bestKeys.size() << "|" << expectedEntryCount << "|"
                  << bestSource << "|" << g_searchQueryNormalized;
        if (signature.str() != g_lastInventoryKeysetLowCoverageSignature)
        {
            std::stringstream line;
            line << "inventory keyset low-coverage key_count=" << bestKeys.size()
                 << " expected=" << expectedEntryCount
                 << " source=\"" << TruncateForLog(bestSource, 220) << "\""
                 << " continuing_with_partial_keys=true";
            LogWarnLine(line.str());
            g_lastInventoryKeysetLowCoverageSignature = signature.str();
        }
    }
    else
    {
        g_lastInventoryKeysetLowCoverageSignature.clear();
    }

    outKeys->swap(bestKeys);
    if (outSource != 0)
    {
        *outSource = bestSource;
    }
    if (outQuantityKeys != 0)
    {
        outQuantityKeys->swap(bestQuantityKeys);
    }

    const std::size_t selectedNonEmptyKeyCount = CountNonEmptyKeys(*outKeys);
    const bool selectedLowCoverage =
        expectedEntryCount >= 8 && selectedNonEmptyKeyCount * 2 < expectedEntryCount;
    const std::string selectedSourceId = BuildKeysetSourceId(bestSource);

    if (g_searchQueryNormalized.empty())
    {
        if (!selectedSourceId.empty() && expectedEntryCount > 0 && !selectedLowCoverage)
        {
            const bool lockChanged =
                g_lockedKeysetTraderParent != traderParent
                || g_lockedKeysetStage != bestStage
                || g_lockedKeysetSourceId != selectedSourceId
                || g_lockedKeysetExpectedCount != expectedEntryCount;

            g_lockedKeysetTraderParent = traderParent;
            g_lockedKeysetStage = bestStage;
            g_lockedKeysetSourceId = selectedSourceId;
            g_lockedKeysetSourcePreview = StripInventorySourceDiagnostics(bestSource);
            g_lockedKeysetExpectedCount = expectedEntryCount;

            if (lockChanged)
            {
                std::stringstream line;
                line << "inventory keyset lock updated"
                     << " stage=" << (bestStage.empty() ? "<unknown>" : bestStage)
                     << " expected=" << expectedEntryCount
                     << " source=\"" << TruncateForLog(g_lockedKeysetSourcePreview, 220) << "\"";
                LogInfoLine(line.str());
            }
        }
    }
    else if (hasLockedSourceForQuery
             && !selectedSourceId.empty()
             && selectedSourceId != g_lockedKeysetSourceId)
    {
        std::stringstream lockSignature;
        lockSignature << "deviate|" << g_searchQueryNormalized
                      << "|" << selectedSourceId
                      << "|" << g_lockedKeysetSourceId
                      << "|" << expectedEntryCount;
        if (lockSignature.str() != g_lastKeysetLockSignature)
        {
            std::stringstream line;
            line << "inventory keyset lock deviation"
                 << " query=\"" << TruncateForLog(g_searchQueryNormalized, 64) << "\""
                 << " locked_stage=" << (g_lockedKeysetStage.empty() ? "<unknown>" : g_lockedKeysetStage)
                 << " selected_stage=" << (bestStage.empty() ? "<unknown>" : bestStage)
                 << " selected_source=\"" << TruncateForLog(StripInventorySourceDiagnostics(bestSource), 200) << "\""
                 << " locked_source=\"" << TruncateForLog(g_lockedKeysetSourcePreview, 200) << "\"";
            LogWarnLine(line.str());
            g_lastKeysetLockSignature = lockSignature.str();
        }
    }

    const std::size_t selectedKeyCount = outKeys->size();
    std::stringstream signature;
    signature << selectedKeyCount << "|" << expectedEntryCount << "|" << bestSource
              << "|" << (usedCoverageFallback ? "coverage_fallback" : "direct");
    if (signature.str() != g_lastInventoryKeysetSelectionSignature)
    {
        for (std::size_t index = 0; index < candidateDiagnostics.size(); ++index)
        {
            LogInfoLine(candidateDiagnostics[index]);
        }

        std::stringstream line;
        line << "inventory keyset selected key_count=" << selectedKeyCount
             << " expected=" << expectedEntryCount
             << " best_score=" << bestScore
             << " source=\"" << TruncateForLog(bestSource, 220) << "\"";
        LogInfoLine(line.str());

        std::stringstream previewLine;
        previewLine << "inventory keyset preview " << BuildKeyPreviewForLog(*outKeys, 14);
        LogInfoLine(previewLine.str());
        g_lastInventoryKeysetSelectionSignature = signature.str();
    }
    return true;
}

void BuildItemSearchTextRecursive(MyGUI::Widget* widget, std::size_t depth, std::size_t maxDepth, std::string* searchText)
{
    if (widget == 0 || searchText == 0 || depth > maxDepth)
    {
        return;
    }

    AppendWidgetSearchTokens(widget, searchText);
    AppendWidgetObjectDataTokens(widget, searchText);

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        BuildItemSearchTextRecursive(widget->getChildAt(childIndex), depth + 1, maxDepth, searchText);
    }
}

std::string BuildItemSearchText(MyGUI::Widget* itemWidget)
{
    std::string searchText;
    BuildItemSearchTextRecursive(itemWidget, 0, 5, &searchText);
    return searchText;
}

void AppendRawTokenForProbe(const std::string& token, std::string* probeText, std::size_t* tokenCount, std::size_t maxTokens)
{
    if (probeText == 0 || tokenCount == 0 || token.empty() || *tokenCount >= maxTokens)
    {
        return;
    }

    if (!probeText->empty())
    {
        probeText->append(" | ");
    }
    probeText->append(token);
    ++(*tokenCount);
}

void AppendObjectNameProbeToken(
    const char* tag,
    RootObjectBase* objectBase,
    std::string* probeText,
    std::size_t* tokenCount,
    std::size_t maxTokens)
{
    if (tag == 0 || objectBase == 0)
    {
        return;
    }

    std::stringstream objectToken;
    objectToken << tag << "=" << TruncateForLog(ResolveItemNameHintFromObjectBase(objectBase), 48);
    AppendRawTokenForProbe(objectToken.str(), probeText, tokenCount, maxTokens);
}

void AppendGameDataProbeToken(
    const char* tag,
    GameData* data,
    std::string* probeText,
    std::size_t* tokenCount,
    std::size_t maxTokens)
{
    if (tag == 0 || data == 0)
    {
        return;
    }

    if (!data->name.empty())
    {
        std::stringstream token;
        token << tag << "_name=" << TruncateForLog(data->name, 48);
        AppendRawTokenForProbe(token.str(), probeText, tokenCount, maxTokens);
    }

    if (!data->stringID.empty())
    {
        std::stringstream token;
        token << tag << "_id=" << TruncateForLog(data->stringID, 48);
        AppendRawTokenForProbe(token.str(), probeText, tokenCount, maxTokens);
    }
}

void AppendWidgetObjectProbeTokens(
    MyGUI::Widget* widget,
    std::string* probeText,
    std::size_t* tokenCount,
    std::size_t maxTokens)
{
    if (widget == 0 || probeText == 0 || tokenCount == 0 || *tokenCount >= maxTokens)
    {
        return;
    }

    Item* itemInternal = ReadWidgetInternalDataPointer<Item>(widget);
    Item* itemUser = ReadWidgetUserDataPointer<Item>(widget);
    InventoryItemBase* itemBaseInternal = ReadWidgetInternalDataPointer<InventoryItemBase>(widget);
    InventoryItemBase* itemBaseUser = ReadWidgetUserDataPointer<InventoryItemBase>(widget);
    RootObjectBase* objectInternal = ReadWidgetInternalDataPointer<RootObjectBase>(widget);
    RootObjectBase* objectUser = ReadWidgetUserDataPointer<RootObjectBase>(widget);
    RootObject* rootInternal = ReadWidgetInternalDataPointer<RootObject>(widget);
    RootObject* rootUser = ReadWidgetUserDataPointer<RootObject>(widget);
    Inventory* inventoryInternal = ReadWidgetInternalDataPointer<Inventory>(widget);
    Inventory* inventoryUser = ReadWidgetUserDataPointer<Inventory>(widget);
    InventorySection* sectionInternal = ReadWidgetInternalDataPointer<InventorySection>(widget);
    InventorySection* sectionUser = ReadWidgetUserDataPointer<InventorySection>(widget);
    GameData* dataInternal = ReadWidgetInternalDataPointer<GameData>(widget);
    GameData* dataUser = ReadWidgetUserDataPointer<GameData>(widget);

    AppendObjectNameProbeToken("idata_item", itemInternal, probeText, tokenCount, maxTokens);
    AppendObjectNameProbeToken("udata_item", itemUser, probeText, tokenCount, maxTokens);
    AppendObjectNameProbeToken("idata_itembase", itemBaseInternal, probeText, tokenCount, maxTokens);
    AppendObjectNameProbeToken("udata_itembase", itemBaseUser, probeText, tokenCount, maxTokens);
    AppendObjectNameProbeToken("idata_obj", objectInternal, probeText, tokenCount, maxTokens);
    AppendObjectNameProbeToken("udata_obj", objectUser, probeText, tokenCount, maxTokens);
    AppendObjectNameProbeToken("idata_root", rootInternal, probeText, tokenCount, maxTokens);
    AppendObjectNameProbeToken("udata_root", rootUser, probeText, tokenCount, maxTokens);

    if (inventoryInternal != 0)
    {
        RootObject* owner = inventoryInternal->getOwner();
        if (owner == 0)
        {
            owner = inventoryInternal->getCallbackObject();
        }
        AppendObjectNameProbeToken("idata_inv_owner", owner, probeText, tokenCount, maxTokens);
    }
    if (inventoryUser != 0)
    {
        RootObject* owner = inventoryUser->getOwner();
        if (owner == 0)
        {
            owner = inventoryUser->getCallbackObject();
        }
        AppendObjectNameProbeToken("udata_inv_owner", owner, probeText, tokenCount, maxTokens);
    }
    if (sectionInternal != 0)
    {
        Inventory* sectionInventory = sectionInternal->getInventory();
        RootObject* owner = sectionInventory == 0 ? 0 : sectionInventory->getOwner();
        if (owner == 0 && sectionInventory != 0)
        {
            owner = sectionInventory->getCallbackObject();
        }
        AppendObjectNameProbeToken("idata_section_inv_owner", owner, probeText, tokenCount, maxTokens);
        if (!sectionInternal->name.empty())
        {
            AppendRawTokenForProbe(
                std::string("idata_section_name=") + TruncateForLog(sectionInternal->name, 48),
                probeText,
                tokenCount,
                maxTokens);
        }
    }
    if (sectionUser != 0)
    {
        Inventory* sectionInventory = sectionUser->getInventory();
        RootObject* owner = sectionInventory == 0 ? 0 : sectionInventory->getOwner();
        if (owner == 0 && sectionInventory != 0)
        {
            owner = sectionInventory->getCallbackObject();
        }
        AppendObjectNameProbeToken("udata_section_inv_owner", owner, probeText, tokenCount, maxTokens);
        if (!sectionUser->name.empty())
        {
            AppendRawTokenForProbe(
                std::string("udata_section_name=") + TruncateForLog(sectionUser->name, 48),
                probeText,
                tokenCount,
                maxTokens);
        }
    }
    AppendGameDataProbeToken("idata_data", dataInternal, probeText, tokenCount, maxTokens);
    AppendGameDataProbeToken("udata_data", dataUser, probeText, tokenCount, maxTokens);

    hand* handValue = widget->_getInternalData<hand>(false);
    if (handValue == 0)
    {
        handValue = widget->getUserData<hand>(false);
    }
    if (handValue != 0 && handValue->isValid())
    {
        AppendObjectNameProbeToken("hand_item", handValue->getItem(), probeText, tokenCount, maxTokens);
        AppendObjectNameProbeToken("hand_obj", handValue->getRootObjectBase(), probeText, tokenCount, maxTokens);
    }

    hand** handPointer = widget->_getInternalData<hand*>(false);
    if (handPointer == 0)
    {
        handPointer = widget->getUserData<hand*>(false);
    }
    if (handPointer != 0 && *handPointer != 0 && (*handPointer)->isValid())
    {
        AppendObjectNameProbeToken("handptr_item", (*handPointer)->getItem(), probeText, tokenCount, maxTokens);
        AppendObjectNameProbeToken("handptr_obj", (*handPointer)->getRootObjectBase(), probeText, tokenCount, maxTokens);
    }

    MyGUI::ImageBox* imageBox = widget->castType<MyGUI::ImageBox>(false);
    if (imageBox != 0)
    {
        const std::size_t imageIndex = imageBox->getImageIndex();
        std::stringstream imageIndexToken;
        imageIndexToken << "image_index=" << imageIndex;
        AppendRawTokenForProbe(imageIndexToken.str(), probeText, tokenCount, maxTokens);

        MyGUI::ResourceImageSetPtr imageSet = imageBox->getItemResource();
        if (imageSet != 0)
        {
            MyGUI::EnumeratorGroupImage groups = imageSet->getEnumerator();
            std::size_t groupCount = 0;
            while (groups.next() && groupCount < 8 && *tokenCount < maxTokens)
            {
                const MyGUI::GroupImage& group = groups.current();
                if (!group.name.empty())
                {
                    AppendRawTokenForProbe(
                        std::string("img_group=") + TruncateForLog(group.name, 48),
                        probeText,
                        tokenCount,
                        maxTokens);
                }
                if (!group.texture.empty())
                {
                    AppendRawTokenForProbe(
                        std::string("img_tex=") + TruncateForLog(group.texture, 48),
                        probeText,
                        tokenCount,
                        maxTokens);
                }
                if (imageIndex < group.indexes.size() && !group.indexes[imageIndex].name.empty())
                {
                    AppendRawTokenForProbe(
                        std::string("img_name=") + TruncateForLog(group.indexes[imageIndex].name, 48),
                        probeText,
                        tokenCount,
                        maxTokens);
                }
                ++groupCount;
            }
        }
    }
}

void BuildItemRawProbeRecursive(
    MyGUI::Widget* widget,
    std::size_t depth,
    std::size_t maxDepth,
    std::string* probeText,
    std::size_t* tokenCount,
    std::size_t maxTokens)
{
    if (widget == 0 || probeText == 0 || tokenCount == 0 || depth > maxDepth || *tokenCount >= maxTokens)
    {
        return;
    }

    AppendRawTokenForProbe(widget->getName(), probeText, tokenCount, maxTokens);
    AppendRawTokenForProbe(WidgetCaptionForLog(widget), probeText, tokenCount, maxTokens);

    const MyGUI::MapString& userStrings = widget->getUserStrings();
    for (MyGUI::MapString::const_iterator it = userStrings.begin(); it != userStrings.end(); ++it)
    {
        AppendRawTokenForProbe(it->first, probeText, tokenCount, maxTokens);
        AppendRawTokenForProbe(it->second, probeText, tokenCount, maxTokens);
        if (*tokenCount >= maxTokens)
        {
            break;
        }
    }

    AppendWidgetObjectProbeTokens(widget, probeText, tokenCount, maxTokens);

    const std::size_t childCount = widget->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        BuildItemRawProbeRecursive(
            widget->getChildAt(childIndex),
            depth + 1,
            maxDepth,
            probeText,
            tokenCount,
            maxTokens);
        if (*tokenCount >= maxTokens)
        {
            break;
        }
    }
}

std::string BuildItemRawProbe(MyGUI::Widget* itemWidget)
{
    std::string probeText;
    std::size_t tokenCount = 0;
    BuildItemRawProbeRecursive(itemWidget, 0, 4, &probeText, &tokenCount, 56);
    return probeText;
}

bool ItemMatchesSearch(MyGUI::Widget* itemWidget, const std::string& normalizedQuery, bool* hadSearchableText)
{
    if (hadSearchableText != 0)
    {
        *hadSearchableText = false;
    }

    if (itemWidget == 0)
    {
        return false;
    }
    if (normalizedQuery.empty())
    {
        return true;
    }

    const std::string searchableText = NormalizeSearchText(BuildItemSearchText(itemWidget));
    if (hadSearchableText != 0)
    {
        *hadSearchableText = !searchableText.empty();
    }
    if (searchableText.empty())
    {
        return true;
    }
    return searchableText.find(normalizedQuery) != std::string::npos;
}

MyGUI::Widget* ResolveInventoryEntriesRoot(MyGUI::Widget* backpackContent)
{
    if (backpackContent == 0)
    {
        return 0;
    }

    MyGUI::Widget* current = backpackContent;
    for (std::size_t unwrapDepth = 0; unwrapDepth < 8; ++unwrapDepth)
    {
        if (current->getChildCount() != 1)
        {
            break;
        }

        MyGUI::Widget* onlyChild = current->getChildAt(0);
        if (onlyChild == 0)
        {
            break;
        }

        current = onlyChild;
    }

    return current;
}

std::size_t CountOccupiedEntriesInEntriesRoot(MyGUI::Widget* entriesRoot)
{
    if (entriesRoot == 0)
    {
        return 0;
    }

    std::size_t occupiedCount = 0;
    const std::size_t childCount = entriesRoot->getChildCount();
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        int quantity = 0;
        if (TryResolveItemQuantityFromWidget(entriesRoot->getChildAt(childIndex), &quantity)
            && quantity > 0)
        {
            ++occupiedCount;
        }
    }

    return occupiedCount;
}

MyGUI::Widget* ResolveBestBackpackContentWidget(MyGUI::Widget* traderParent, bool logDiagnostics)
{
    if (traderParent == 0)
    {
        return 0;
    }

    std::vector<MyGUI::Widget*> candidates;
    CollectNamedDescendantsByToken(
        traderParent,
        "backpack_content",
        false,
        24,
        &candidates);
    if (candidates.empty())
    {
        return 0;
    }

    struct CandidateScore
    {
        MyGUI::Widget* backpack;
        MyGUI::Widget* entriesRoot;
        std::size_t childCount;
        std::size_t occupiedCount;
        MyGUI::IntCoord absoluteCoord;
        int score;
    };

    std::vector<CandidateScore> scoredCandidates;
    scoredCandidates.reserve(candidates.size());

    int bestScore = -1000000;
    int bestLeft = -1000000;
    MyGUI::Widget* bestBackpack = 0;
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        MyGUI::Widget* backpack = candidates[index];
        if (backpack == 0)
        {
            continue;
        }

        MyGUI::Widget* entriesRoot = ResolveInventoryEntriesRoot(backpack);
        const std::size_t childCount = entriesRoot == 0 ? 0 : entriesRoot->getChildCount();
        const std::size_t occupiedCount = CountOccupiedEntriesInEntriesRoot(entriesRoot);
        const MyGUI::IntCoord absoluteCoord =
            entriesRoot != 0 ? entriesRoot->getAbsoluteCoord() : backpack->getAbsoluteCoord();

        int score = 0;
        score += backpack->getInheritedVisible() ? 1800 : 0;
        score += entriesRoot != 0 ? 800 : 0;
        score += static_cast<int>(occupiedCount) * 240;
        score += static_cast<int>(childCount) * 14;
        if (childCount > 0 && occupiedCount * 2 >= childCount)
        {
            score += 600;
        }

        CandidateScore candidateScore;
        candidateScore.backpack = backpack;
        candidateScore.entriesRoot = entriesRoot;
        candidateScore.childCount = childCount;
        candidateScore.occupiedCount = occupiedCount;
        candidateScore.absoluteCoord = absoluteCoord;
        candidateScore.score = score;
        scoredCandidates.push_back(candidateScore);

        if (bestBackpack == 0
            || score > bestScore
            || (score == bestScore && absoluteCoord.left > bestLeft))
        {
            bestScore = score;
            bestLeft = absoluteCoord.left;
            bestBackpack = backpack;
        }
    }

    std::size_t selectedOccupied = 0;
    std::size_t selectedChildCount = 0;
    MyGUI::Widget* selectedEntriesRoot = 0;
    MyGUI::IntCoord selectedCoord;
    for (std::size_t index = 0; index < scoredCandidates.size(); ++index)
    {
        if (scoredCandidates[index].backpack != bestBackpack)
        {
            continue;
        }

        selectedOccupied = scoredCandidates[index].occupiedCount;
        selectedChildCount = scoredCandidates[index].childCount;
        selectedEntriesRoot = scoredCandidates[index].entriesRoot;
        selectedCoord = scoredCandidates[index].absoluteCoord;
        break;
    }

    std::stringstream signature;
    signature << SafeWidgetName(traderParent) << "|candidates=" << scoredCandidates.size()
              << "|selected=" << SafeWidgetName(bestBackpack)
              << "|entries=" << SafeWidgetName(selectedEntriesRoot)
              << "|occupied=" << selectedOccupied
              << "|children=" << selectedChildCount
              << "|x=" << selectedCoord.left;
    const bool shouldLog =
        (logDiagnostics || scoredCandidates.size() > 1)
        && signature.str() != g_lastBackpackResolutionSignature;
    if (shouldLog)
    {
        std::stringstream summary;
        summary << "backpack resolver selected="
                << SafeWidgetName(bestBackpack)
                << " entries_root=" << SafeWidgetName(selectedEntriesRoot)
                << " occupied=" << selectedOccupied
                << " child_count=" << selectedChildCount
                << " candidates=" << scoredCandidates.size()
                << " parent=" << SafeWidgetName(traderParent);
        LogInfoLine(summary.str());

        for (std::size_t index = 0; index < scoredCandidates.size(); ++index)
        {
            const CandidateScore& candidate = scoredCandidates[index];
            std::stringstream line;
            line << "backpack resolver candidate[" << index << "]"
                 << " name=" << SafeWidgetName(candidate.backpack)
                 << " entries_root=" << SafeWidgetName(candidate.entriesRoot)
                 << " occupied=" << candidate.occupiedCount
                 << " child_count=" << candidate.childCount
                 << " visible=" << (candidate.backpack->getInheritedVisible() ? "true" : "false")
                 << " score=" << candidate.score
                 << " abs_coord=(" << candidate.absoluteCoord.left
                 << "," << candidate.absoluteCoord.top
                 << "," << candidate.absoluteCoord.width
                 << "," << candidate.absoluteCoord.height << ")";
            LogInfoLine(line.str());
        }

        g_lastBackpackResolutionSignature = signature.str();
    }

    return bestBackpack;
}

bool SearchTextMatchesQuery(const std::string& searchableTextNormalized, const std::string& normalizedQuery)
{
    if (normalizedQuery.empty())
    {
        return true;
    }
    if (searchableTextNormalized.empty())
    {
        return true;
    }

    return searchableTextNormalized.find(normalizedQuery) != std::string::npos;
}

void LogSearchSampleForQuery(MyGUI::Widget* entriesRoot, const std::string& normalizedQuery, std::size_t maxItems)
{
    if (entriesRoot == 0 || maxItems == 0)
    {
        return;
    }

    std::stringstream header;
    header << "search debug sample begin"
           << " query=\"" << normalizedQuery << "\""
           << " entries_root=" << SafeWidgetName(entriesRoot)
           << " child_count=" << entriesRoot->getChildCount()
           << " max_items=" << maxItems;
    LogInfoLine(header.str());

    const std::size_t childCount = entriesRoot->getChildCount();
    const std::size_t limit = childCount < maxItems ? childCount : maxItems;
    for (std::size_t childIndex = 0; childIndex < limit; ++childIndex)
    {
        MyGUI::Widget* child = entriesRoot->getChildAt(childIndex);
        if (child == 0)
        {
            continue;
        }

        const std::string searchRaw = BuildItemSearchText(child);
        const std::string searchNormalized = NormalizeSearchText(searchRaw);
        const bool matches = SearchTextMatchesQuery(searchNormalized, normalizedQuery);
        const std::string itemNameHint = ResolveItemNameHintRecursive(child, 0, 5);
        const std::string rawProbe = BuildItemRawProbe(child);

        std::stringstream line;
        line << "search sample idx=" << childIndex
             << " name=" << SafeWidgetName(child)
             << " caption=\"" << TruncateForLog(WidgetCaptionForLog(child), 48) << "\""
             << " item_hint=\"" << TruncateForLog(itemNameHint, 64) << "\""
             << " children=" << child->getChildCount()
             << " raw_len=" << searchRaw.size()
             << " normalized_len=" << searchNormalized.size()
             << " match=" << (matches ? "true" : "false")
             << " text=\"" << TruncateForLog(searchNormalized, 180) << "\""
             << " raw_probe=\"" << TruncateForLog(rawProbe, 220) << "\"";
        LogInfoLine(line.str());
    }

    LogInfoLine("search debug sample end");
}

MyGUI::Widget* ResolveTraderParentFromControlsContainer()
{
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0)
    {
        return 0;
    }

    MyGUI::Widget* anchor = FindBestWindowAnchor(controlsContainer);
    MyGUI::Widget* parent = ResolveInjectionParent(anchor);
    if (parent != 0 && IsLikelyTraderWindow(parent))
    {
        return parent;
    }

    MyGUI::Widget* current = controlsContainer->getParent();
    while (current != 0)
    {
        if (IsLikelyTraderWindow(current))
        {
            return current;
        }

        current = current->getParent();
    }

    return parent;
}

bool ApplySearchFilterToTraderParent(MyGUI::Widget* traderParent, bool forceShowAll, bool logSummary)
{
    if (traderParent == 0)
    {
        return false;
    }

    MyGUI::Widget* backpackContent = ResolveBestBackpackContentWidget(traderParent, logSummary);
    if (backpackContent == 0)
    {
        backpackContent = FindWidgetInParentByToken(traderParent, "backpack_content");
    }
    if (backpackContent == 0)
    {
        if (!g_loggedMissingBackpackForSearch)
        {
            std::stringstream line;
            line << "search filter skipped: backpack_content not found parent=" << SafeWidgetName(traderParent);
            LogWarnLine(line.str());
            g_loggedMissingBackpackForSearch = true;
        }
        UpdateSearchCountText(0, 0, 0);
        return false;
    }
    g_loggedMissingBackpackForSearch = false;

    MyGUI::Widget* entriesRoot = ResolveInventoryEntriesRoot(backpackContent);
    if (entriesRoot == 0)
    {
        UpdateSearchCountText(0, 0, 0);
        return false;
    }

    const std::string query = forceShowAll ? std::string() : g_searchQueryNormalized;
    std::size_t totalCount = 0;
    std::size_t visibleCount = 0;
    std::size_t missingSearchableTextCount = 0;
    std::size_t fallbackKeptVisibleCount = 0;
    std::size_t itemNameHintCount = 0;

    struct OrderedEntry
    {
        MyGUI::Widget* widget;
        MyGUI::IntCoord coord;
        int quantity;
    };

    std::vector<OrderedEntry> orderedEntries;
    const std::size_t childCount = entriesRoot->getChildCount();
    orderedEntries.reserve(childCount);
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        MyGUI::Widget* child = entriesRoot->getChildAt(childIndex);
        if (child == 0)
        {
            continue;
        }

        OrderedEntry entry;
        entry.widget = child;
        entry.coord = child->getCoord();
        entry.quantity = 0;
        TryResolveItemQuantityFromWidget(child, &entry.quantity);
        orderedEntries.push_back(entry);
    }

    struct OrderedEntryCoordLess
    {
        bool operator()(const OrderedEntry& left, const OrderedEntry& right) const
        {
            if (left.coord.top != right.coord.top)
            {
                return left.coord.top < right.coord.top;
            }
            if (left.coord.left != right.coord.left)
            {
                return left.coord.left < right.coord.left;
            }
            return left.widget < right.widget;
        }
    };
    std::sort(orderedEntries.begin(), orderedEntries.end(), OrderedEntryCoordLess());

    const std::size_t orderedEntryCount = orderedEntries.size();
    std::vector<std::size_t> occupiedEntryIndices;
    occupiedEntryIndices.reserve(orderedEntryCount);
    std::vector<int> uiQuantities;
    uiQuantities.reserve(orderedEntryCount);
    for (std::size_t index = 0; index < orderedEntryCount; ++index)
    {
        if (orderedEntries[index].quantity <= 0)
        {
            continue;
        }

        occupiedEntryIndices.push_back(index);
        uiQuantities.push_back(orderedEntries[index].quantity);
    }
    const std::size_t expectedEntryCount = occupiedEntryIndices.size();
    std::size_t totalOccupiedQuantity = 0;
    for (std::size_t quantityIndex = 0; quantityIndex < uiQuantities.size(); ++quantityIndex)
    {
        if (uiQuantities[quantityIndex] > 0)
        {
            totalOccupiedQuantity += static_cast<std::size_t>(uiQuantities[quantityIndex]);
        }
    }
    std::size_t widgetSearchableEntryCount = 0;
    if (!query.empty())
    {
        for (std::size_t index = 0; index < orderedEntryCount; ++index)
        {
            const std::string widgetSearchText =
                NormalizeSearchText(BuildItemSearchText(orderedEntries[index].widget));
            if (!widgetSearchText.empty())
            {
                ++widgetSearchableEntryCount;
            }
        }
    }
    const bool preferCoverageFallbackWhenWidgetOpaque =
        !query.empty() && expectedEntryCount >= 8 && widgetSearchableEntryCount == 0;

    TraderPanelInventoryBinding panelBinding;
    std::string panelBindingStatus;
    const bool hasPanelBinding = TryResolveAndCacheTraderPanelInventoryBinding(
        traderParent,
        entriesRoot,
        expectedEntryCount,
        &uiQuantities,
        &panelBinding,
        &panelBindingStatus);
    if (expectedEntryCount > 0)
    {
        LogPanelBindingProbeOnce(
            traderParent,
            entriesRoot,
            expectedEntryCount,
            hasPanelBinding ? std::string("resolved_") + panelBindingStatus : panelBindingStatus,
            hasPanelBinding ? &panelBinding : 0);
    }

    if (!query.empty() && !hasPanelBinding)
    {
        totalCount = orderedEntryCount;
        visibleCount = orderedEntryCount;
        for (std::size_t index = 0; index < orderedEntryCount; ++index)
        {
            MyGUI::Widget* child = orderedEntries[index].widget;
            if (child != 0)
            {
                child->setVisible(true);
            }
        }

        std::stringstream signature;
        signature << query
                  << "|" << panelBindingStatus
                  << "|" << expectedEntryCount
                  << "|" << SafeWidgetName(traderParent);
        if (signature.str() != g_lastPanelBindingRefusedSignature)
        {
            std::stringstream line;
            line << "search refused: missing high-confidence panel inventory binding"
                 << " query=\"" << query << "\""
                 << " reason=" << panelBindingStatus
                 << " expected_entries=" << expectedEntryCount
                 << " parent=" << SafeWidgetName(traderParent);
            LogWarnLine(line.str());
            g_lastPanelBindingRefusedSignature = signature.str();
        }

        if (!g_loggedInventoryBindingDiagnostics)
        {
            LogInventoryBindingDiagnostics(expectedEntryCount);
            g_loggedInventoryBindingDiagnostics = true;
        }
        if (g_lastSearchSampleQueryLogged != query)
        {
            LogSearchSampleForQuery(entriesRoot, query, 12);
            g_lastSearchSampleQueryLogged = query;
        }
        g_loggedInventoryBindingFailure = true;

        if (logSummary)
        {
            std::stringstream line;
            line << "search filter applied query=\"" << (forceShowAll ? "" : g_searchQueryRaw)
                 << "\" normalized=\"" << (forceShowAll ? "" : query)
                 << "\" visible=" << visibleCount
                 << " total=" << totalCount
                 << " panel_binding=false"
                 << " panel_binding_reason=" << panelBindingStatus
                 << " occupied_entries=" << expectedEntryCount
                 << " entries_root=" << SafeWidgetName(entriesRoot)
                 << " backpack_content=" << SafeWidgetName(backpackContent)
                 << " searchable_entries=0"
                 << " filtering_refused=true";
            LogInfoLine(line.str());
        }
        UpdateSearchCountText(expectedEntryCount, expectedEntryCount, totalOccupiedQuantity);
        return true;
    }

    std::vector<std::string> inventoryNameKeys;
    std::vector<QuantityNameKey> inventoryQuantityNameKeys;
    std::string inventorySource;
    bool hasInventoryNameKeys = false;
    if (hasPanelBinding && panelBinding.inventory != 0)
    {
        hasInventoryNameKeys =
            TryExtractSearchKeysFromInventory(panelBinding.inventory, &inventoryNameKeys);
        TryExtractQuantityNameKeysFromInventory(panelBinding.inventory, &inventoryQuantityNameKeys);
        if (expectedEntryCount > 0 && inventoryNameKeys.size() > expectedEntryCount)
        {
            inventoryNameKeys.resize(expectedEntryCount);
        }
        if (expectedEntryCount > 0 && inventoryQuantityNameKeys.size() > expectedEntryCount)
        {
            inventoryQuantityNameKeys.resize(expectedEntryCount);
        }

        std::stringstream source;
        source << "panel_binding:"
               << panelBinding.stage
               << " " << panelBinding.source;
        inventorySource = source.str();
    }

    std::vector<std::string> alignedInventoryNameHints;
    const bool hasAlignedInventoryNameHints = hasInventoryNameKeys
        && BuildAlignedInventoryNameHintsByQuantity(
            uiQuantities,
            inventoryQuantityNameKeys,
            &alignedInventoryNameHints);
    const std::size_t panelBindingNonEmptyKeyCount = hasInventoryNameKeys
        ? CountNonEmptyKeys(inventoryNameKeys)
        : 0;
    const bool panelBindingLowCoverage =
        hasInventoryNameKeys
        && expectedEntryCount >= 8
        && panelBindingNonEmptyKeyCount * 2 < expectedEntryCount;

    if (!query.empty() && (!hasInventoryNameKeys || panelBindingLowCoverage))
    {
        totalCount = orderedEntryCount;
        visibleCount = orderedEntryCount;
        for (std::size_t index = 0; index < orderedEntryCount; ++index)
        {
            MyGUI::Widget* child = orderedEntries[index].widget;
            if (child != 0)
            {
                child->setVisible(true);
            }
        }

        const std::string refusalReason =
            !hasInventoryNameKeys ? "panel_binding_keys_missing" : "panel_binding_low_coverage";
        std::stringstream signature;
        signature << query
                  << "|" << refusalReason
                  << "|" << panelBindingNonEmptyKeyCount
                  << "|" << expectedEntryCount
                  << "|" << SafeWidgetName(traderParent);
        if (signature.str() != g_lastPanelBindingRefusedSignature)
        {
            std::stringstream line;
            line << "search refused: panel binding confidence too low"
                 << " query=\"" << query << "\""
                 << " reason=" << refusalReason
                 << " non_empty_keys=" << panelBindingNonEmptyKeyCount
                 << " expected_entries=" << expectedEntryCount
                 << " source=\"" << TruncateForLog(inventorySource, 160) << "\"";
            LogWarnLine(line.str());
            g_lastPanelBindingRefusedSignature = signature.str();
        }

        if (!g_loggedInventoryBindingDiagnostics)
        {
            LogInventoryBindingDiagnostics(expectedEntryCount);
            g_loggedInventoryBindingDiagnostics = true;
        }
        g_loggedInventoryBindingFailure = true;

        if (logSummary)
        {
            std::stringstream line;
            line << "search filter applied query=\"" << (forceShowAll ? "" : g_searchQueryRaw)
                 << "\" normalized=\"" << (forceShowAll ? "" : query)
                 << "\" visible=" << visibleCount
                 << " total=" << totalCount
                 << " panel_binding=true"
                 << " panel_binding_reason=" << refusalReason
                 << " occupied_entries=" << expectedEntryCount
                 << " inventory_non_empty_keys=" << panelBindingNonEmptyKeyCount
                 << " entries_root=" << SafeWidgetName(entriesRoot)
                 << " backpack_content=" << SafeWidgetName(backpackContent)
                 << " filtering_refused=true";
            LogInfoLine(line.str());
        }
        UpdateSearchCountText(expectedEntryCount, expectedEntryCount, totalOccupiedQuantity);
        return true;
    }

    if (!hasInventoryNameKeys && !query.empty() && !g_loggedInventoryBindingFailure)
    {
        LogWarnLine("could not resolve trader inventory-backed name keys; search is using widget-only metadata");
        g_loggedInventoryBindingFailure = true;
    }
    if (hasInventoryNameKeys)
    {
        g_loggedInventoryBindingFailure = false;
    }
    if (!hasInventoryNameKeys && !query.empty() && !g_loggedInventoryBindingDiagnostics)
    {
        LogInventoryBindingDiagnostics(expectedEntryCount);
        g_loggedInventoryBindingDiagnostics = true;
    }
    if (hasPanelBinding && hasInventoryNameKeys)
    {
        g_loggedInventoryBindingFailure = false;
    }
    if (query.empty())
    {
        g_loggedInventoryBindingDiagnostics = false;
    }

    struct EntryFilterState
    {
        MyGUI::Widget* widget;
        std::string searchableText;
        bool occupied;
        int quantity;
        bool lowCoverageQuantityMatched;
    };
    std::vector<EntryFilterState> entries;
    entries.reserve(orderedEntryCount);
    std::size_t visibleOccupiedCount = 0;
    std::size_t visibleQuantity = 0;
    std::size_t searchableEntryCount = 0;
    std::size_t sequenceAlignedNameHintCount = 0;
    std::size_t quantityAlignedNameHintCount = 0;
    std::size_t quantityCandidateNameHintCount = 0;
    std::size_t inventoryKeyQueryMatches = 0;
    std::size_t zeroMatchGuardRestoredCount = 0;
    const std::size_t inventoryNonEmptyKeyCount = hasInventoryNameKeys
        ? CountNonEmptyKeys(inventoryNameKeys)
        : 0;
    const bool inventoryKeyCoverageLow = hasInventoryNameKeys
        && expectedEntryCount >= 8
        && inventoryNonEmptyKeyCount * 2 < expectedEntryCount;
    std::size_t alignedInventoryNameHintCoverage = 0;
    if (hasAlignedInventoryNameHints)
    {
        alignedInventoryNameHintCoverage = CountNonEmptyKeys(alignedInventoryNameHints);
    }
    const bool strongAlignedHintCoverage = hasAlignedInventoryNameHints
        && (expectedEntryCount < 8 || alignedInventoryNameHintCoverage * 2 >= expectedEntryCount);
    const std::string inventorySourceLower = NormalizeSearchText(inventorySource);
    const bool inventorySourceLooksDirect =
        inventorySourceLower.find("widget") != std::string::npos
        || inventorySourceLower.find("selected item") != std::string::npos
        || inventorySourceLower.find("hovered") != std::string::npos;
    const bool inventorySourceTraderAnchored = IsTraderAnchoredCandidateSource(inventorySourceLower);
    const bool inventorySourceRisky = IsRiskyCoverageFallbackSource(inventorySourceLower);
    const bool allowSequenceAlignedHints = hasAlignedInventoryNameHints && !inventoryKeyCoverageLow;
    const bool opaqueIndexedHintsHaveConfidence =
        inventorySourceLooksDirect
        || (inventorySourceTraderAnchored
            && expectedEntryCount > 0
            && alignedInventoryNameHintCoverage * 2 >= expectedEntryCount)
        || (expectedEntryCount > 0
            && alignedInventoryNameHintCoverage * 3 >= expectedEntryCount * 2);
    const bool allowOpaqueIndexedHints =
        preferCoverageFallbackWhenWidgetOpaque
        && hasInventoryNameKeys
        && expectedEntryCount > 0
        && inventoryNonEmptyKeyCount * 2 >= expectedEntryCount
        && !inventorySourceRisky
        && opaqueIndexedHintsHaveConfidence;
    const bool allowIndexedNameHints =
        hasInventoryNameKeys
        && (!inventoryKeyCoverageLow || allowOpaqueIndexedHints)
        && (inventorySourceLooksDirect || strongAlignedHintCoverage || allowOpaqueIndexedHints);
    const bool allowQuantityCandidateHints = false;
    std::vector<int> lowCoverageMatchedQuantities;
    if (inventoryKeyCoverageLow && !query.empty() && !inventoryQuantityNameKeys.empty())
    {
        const std::size_t pairCount =
            inventoryNameKeys.size() < inventoryQuantityNameKeys.size()
                ? inventoryNameKeys.size()
                : inventoryQuantityNameKeys.size();
        for (std::size_t pairIndex = 0; pairIndex < pairCount; ++pairIndex)
        {
            if (inventoryNameKeys[pairIndex].find(query) == std::string::npos)
            {
                continue;
            }

            const int matchedQuantity = inventoryQuantityNameKeys[pairIndex].quantity;
            if (matchedQuantity <= 0)
            {
                continue;
            }

            bool alreadyAdded = false;
            for (std::size_t existingIndex = 0; existingIndex < lowCoverageMatchedQuantities.size(); ++existingIndex)
            {
                if (lowCoverageMatchedQuantities[existingIndex] == matchedQuantity)
                {
                    alreadyAdded = true;
                    break;
                }
            }
            if (!alreadyAdded)
            {
                lowCoverageMatchedQuantities.push_back(matchedQuantity);
            }
        }
    }
    const bool lowCoverageQuantityAssistActive =
        inventoryKeyCoverageLow && !query.empty() && !lowCoverageMatchedQuantities.empty();
    std::size_t lowCoverageQuantityMatchedEntryCount = 0;
    if (hasInventoryNameKeys && !query.empty())
    {
        for (std::size_t keyIndex = 0; keyIndex < inventoryNameKeys.size(); ++keyIndex)
        {
            if (inventoryNameKeys[keyIndex].find(query) != std::string::npos)
            {
                ++inventoryKeyQueryMatches;
            }
        }
    }

    for (std::size_t childIndex = 0; childIndex < orderedEntryCount; ++childIndex)
    {
        const OrderedEntry& ordered = orderedEntries[childIndex];
        MyGUI::Widget* child = ordered.widget;
        ++totalCount;
        const bool occupied = ordered.quantity > 0;
        std::string searchableText;

        std::string itemNameHint;
        std::size_t occupiedOrdinal = static_cast<std::size_t>(-1);
        if (occupied)
        {
            for (std::size_t occupiedIndex = 0; occupiedIndex < occupiedEntryIndices.size(); ++occupiedIndex)
            {
                if (occupiedEntryIndices[occupiedIndex] == childIndex)
                {
                    occupiedOrdinal = occupiedIndex;
                    break;
                }
            }
        }
        const std::string indexedNameHint =
            (allowIndexedNameHints
             && occupiedOrdinal != static_cast<std::size_t>(-1)
             && occupiedOrdinal < inventoryNameKeys.size())
            ? inventoryNameKeys[occupiedOrdinal]
            : "";
        const std::string sequenceAlignedNameHint =
            (allowSequenceAlignedHints
             && occupiedOrdinal != static_cast<std::size_t>(-1)
             && occupiedOrdinal < alignedInventoryNameHints.size())
            ? alignedInventoryNameHints[occupiedOrdinal]
            : "";

        std::string quantityAlignedNameHint;
        if (!inventoryQuantityNameKeys.empty())
        {
            if (ordered.quantity > 0)
            {
                quantityAlignedNameHint = ResolveUniqueQuantityNameHint(inventoryQuantityNameKeys, ordered.quantity);
            }
        }

        std::string quantityCandidateNameHint;
        if (allowQuantityCandidateHints
            && quantityAlignedNameHint.empty()
            && !inventoryQuantityNameKeys.empty()
            && ordered.quantity > 0)
        {
            quantityCandidateNameHint = ResolveTopQuantityNameHints(
                inventoryQuantityNameKeys,
                ordered.quantity,
                4);
        }

        if (!sequenceAlignedNameHint.empty())
        {
            itemNameHint = sequenceAlignedNameHint;
            ++sequenceAlignedNameHintCount;
        }
        else if (!quantityAlignedNameHint.empty())
        {
            itemNameHint = quantityAlignedNameHint;
            ++quantityAlignedNameHintCount;
        }
        else if (!quantityCandidateNameHint.empty())
        {
            itemNameHint = quantityCandidateNameHint;
            ++quantityCandidateNameHintCount;
        }
        else if (!indexedNameHint.empty())
        {
            itemNameHint = indexedNameHint;
        }
        else
        {
            itemNameHint = NormalizeSearchText(ResolveItemNameHintRecursive(child, 0, 5));
        }
        if (!itemNameHint.empty() && !ContainsAsciiLetter(itemNameHint))
        {
            itemNameHint.clear();
        }

        if (!itemNameHint.empty())
        {
            ++itemNameHintCount;
        }
        AppendNormalizedSearchChunk(itemNameHint, &searchableText);

        const std::string widgetSearchText = NormalizeSearchText(BuildItemSearchText(child));
        AppendNormalizedSearchChunk(widgetSearchText, &searchableText);
        if (!searchableText.empty())
        {
            ++searchableEntryCount;
        }

        EntryFilterState state;
        state.widget = child;
        state.searchableText = searchableText;
        state.occupied = occupied;
        state.quantity = ordered.quantity;
        state.lowCoverageQuantityMatched = false;
        if (lowCoverageQuantityAssistActive && ordered.quantity > 0)
        {
            for (std::size_t quantityIndex = 0; quantityIndex < lowCoverageMatchedQuantities.size(); ++quantityIndex)
            {
                if (lowCoverageMatchedQuantities[quantityIndex] == ordered.quantity)
                {
                    state.lowCoverageQuantityMatched = true;
                    ++lowCoverageQuantityMatchedEntryCount;
                    break;
                }
            }
        }
        entries.push_back(state);
    }

    const bool hasAnySearchableText = searchableEntryCount > 0;
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        const EntryFilterState& entry = entries[index];
        bool shouldBeVisible = true;

        if (!query.empty())
        {
            if (lowCoverageQuantityAssistActive)
            {
                if (!entry.searchableText.empty())
                {
                    shouldBeVisible =
                        entry.searchableText.find(query) != std::string::npos
                        || entry.lowCoverageQuantityMatched;
                }
                else
                {
                    shouldBeVisible = entry.lowCoverageQuantityMatched;
                    if (!entry.occupied)
                    {
                        shouldBeVisible = false;
                    }
                    else
                    {
                        ++missingSearchableTextCount;
                    }
                }
            }
            else if (entry.searchableText.empty())
            {
                ++missingSearchableTextCount;
                if (!entry.occupied)
                {
                    shouldBeVisible = false;
                }
                else
                {
                    shouldBeVisible = !hasAnySearchableText;
                    if (shouldBeVisible)
                    {
                        ++fallbackKeptVisibleCount;
                    }
                }
            }
            else
            {
                shouldBeVisible = entry.searchableText.find(query) != std::string::npos;
            }
        }

        entry.widget->setVisible(shouldBeVisible);
        if (shouldBeVisible)
        {
            ++visibleCount;
            if (entry.occupied)
            {
                ++visibleOccupiedCount;
                if (entry.quantity > 0)
                {
                    visibleQuantity += static_cast<std::size_t>(entry.quantity);
                }
            }
        }
    }

    const bool lowAlignmentConfidence =
        !query.empty()
        && expectedEntryCount >= 10
        && !inventorySourceLooksDirect
        && (sequenceAlignedNameHintCount + quantityAlignedNameHintCount) * 3 < expectedEntryCount;
    if (!query.empty()
        && visibleCount == 0
        && expectedEntryCount > 0
        && !allowOpaqueIndexedHints
        && (inventoryKeyCoverageLow || lowAlignmentConfidence))
    {
        for (std::size_t index = 0; index < entries.size(); ++index)
        {
            const EntryFilterState& entry = entries[index];
            if (!entry.occupied)
            {
                continue;
            }

            if (!entry.widget->getVisible())
            {
                entry.widget->setVisible(true);
                ++zeroMatchGuardRestoredCount;
            }
        }
        visibleCount = zeroMatchGuardRestoredCount;

        if (zeroMatchGuardRestoredCount > 0)
        {
            std::stringstream signature;
            signature << query
                      << "|" << inventorySource
                      << "|" << (inventoryKeyCoverageLow ? "1" : "0")
                      << "|" << (lowAlignmentConfidence ? "1" : "0")
                      << "|" << zeroMatchGuardRestoredCount;
            if (signature.str() != g_lastZeroMatchGuardSignature)
            {
                std::stringstream line;
                line << "search zero-match guard restored occupied entries="
                     << zeroMatchGuardRestoredCount
                     << " query=\"" << query << "\""
                     << " inventory_low_coverage=" << (inventoryKeyCoverageLow ? "true" : "false")
                     << " low_alignment_confidence=" << (lowAlignmentConfidence ? "true" : "false")
                     << " source=\"" << TruncateForLog(inventorySource, 120) << "\"";
                LogWarnLine(line.str());
                g_lastZeroMatchGuardSignature = signature.str();
            }
        }
    }

    if (!query.empty() && fallbackKeptVisibleCount > 0 && !g_loggedMissingSearchableItemText)
    {
        std::stringstream line;
        line << "search fallback: kept " << fallbackKeptVisibleCount
             << " entries visible because searchable text was missing";
        LogWarnLine(line.str());
        g_loggedMissingSearchableItemText = true;
    }
    if (query.empty())
    {
        g_loggedMissingSearchableItemText = false;
        g_lastZeroMatchQueryLogged.clear();
        g_lastZeroMatchGuardSignature.clear();
        g_lastSearchSampleQueryLogged.clear();
        g_loggedInventoryBindingFailure = false;
        g_loggedInventoryBindingDiagnostics = false;
        g_lastPanelBindingRefusedSignature.clear();
    }

    if (logSummary)
    {
        std::stringstream line;
        line << "search filter applied query=\"" << (forceShowAll ? "" : g_searchQueryRaw)
             << "\" normalized=\"" << (forceShowAll ? "" : query)
             << "\" visible=" << visibleCount
             << " total=" << totalCount
             << " item_hints=" << itemNameHintCount
             << " sequence_aligned_hints=" << sequenceAlignedNameHintCount
             << " quantity_aligned_hints=" << quantityAlignedNameHintCount
             << " quantity_candidate_hints=" << quantityCandidateNameHintCount
             << " inventory_keys=" << (hasInventoryNameKeys ? inventoryNameKeys.size() : 0)
             << " inventory_non_empty_keys=" << inventoryNonEmptyKeyCount
             << " inventory_low_coverage=" << (inventoryKeyCoverageLow ? "true" : "false")
             << " occupied_entries=" << expectedEntryCount
             << " aligned_hint_coverage=" << alignedInventoryNameHintCoverage
             << "/" << expectedEntryCount
             << " allow_indexed_hints=" << (allowIndexedNameHints ? "true" : "false")
             << " allow_opaque_indexed_hints=" << (allowOpaqueIndexedHints ? "true" : "false")
             << " opaque_prefer_mode=" << (preferCoverageFallbackWhenWidgetOpaque ? "true" : "false")
             << " widget_searchable_pre_resolve=" << widgetSearchableEntryCount
             << " low_coverage_quantity_assist=" << (lowCoverageQuantityAssistActive ? "true" : "false")
             << " low_coverage_quantity_matches=" << lowCoverageQuantityMatchedEntryCount
             << " inventory_key_query_matches=" << inventoryKeyQueryMatches
             << " zero_match_guard_restored=" << zeroMatchGuardRestoredCount
             << " entries_root=" << SafeWidgetName(entriesRoot)
             << " backpack_content=" << SafeWidgetName(backpackContent)
             << " missing_searchable=" << missingSearchableTextCount
             << " searchable_entries=" << searchableEntryCount;
        if (hasInventoryNameKeys)
        {
            line << " inventory_source=\"" << TruncateForLog(inventorySource, 96) << "\"";
        }
        LogInfoLine(line.str());
    }

    if (!query.empty() && g_lastSearchSampleQueryLogged != query)
    {
        LogSearchSampleForQuery(entriesRoot, query, 12);
        g_lastSearchSampleQueryLogged = query;
    }

    if (!query.empty() && !hasAnySearchableText && g_lastZeroMatchQueryLogged != query)
    {
        if (hasInventoryNameKeys)
        {
            std::stringstream previewLine;
            previewLine << "search zero-match key preview query=\"" << query << "\" "
                        << BuildKeyPreviewForLog(inventoryNameKeys, 16);
            LogWarnLine(previewLine.str());
        }
        LogSearchSampleForQuery(entriesRoot, query, 12);
        g_lastZeroMatchQueryLogged = query;
    }
    if (!query.empty() && visibleCount == 0 && g_lastZeroMatchQueryLogged != query)
    {
        if (hasInventoryNameKeys)
        {
            std::stringstream previewLine;
            previewLine << "search zero-match key preview query=\"" << query << "\" "
                        << BuildKeyPreviewForLog(inventoryNameKeys, 16);
            LogWarnLine(previewLine.str());
        }
        LogSearchSampleForQuery(entriesRoot, query, 12);
        g_lastZeroMatchQueryLogged = query;
    }

    UpdateSearchCountText(visibleOccupiedCount, expectedEntryCount, visibleQuantity);
    return true;
}

void ApplySearchFilterFromControls(bool forceShowAll, bool logSummary)
{
    MyGUI::Widget* traderParent = ResolveTraderParentFromControlsContainer();
    if (traderParent == 0)
    {
        return;
    }

    ApplySearchFilterToTraderParent(traderParent, forceShowAll, logSummary);
}

std::string BuildTraderTargetIdentity(MyGUI::Widget* anchor, MyGUI::Widget* parent)
{
    MyGUI::Widget* identityWidget = parent != 0 ? parent : anchor;

    Character* captionTrader = 0;
    int captionScore = 0;
    if (identityWidget != 0
        && TryResolveCaptionMatchedTraderCharacter(identityWidget, &captionTrader, &captionScore)
        && captionTrader != 0
        && captionScore > 0)
    {
        return std::string("caption_trader:") + NormalizeSearchText(CharacterNameForLog(captionTrader));
    }

    MyGUI::Window* owningWindow = FindOwningWindow(identityWidget);
    const std::string normalizedCaption =
        owningWindow == 0 ? std::string() : NormalizeSearchText(owningWindow->getCaption().asUTF8());
    if (!normalizedCaption.empty())
    {
        Character* dialogueTarget = 0;
        Character* dialogueSpeaker = 0;
        std::string dialogueReason;
        if (TryResolvePreferredDialogueTraderTarget(&dialogueTarget, &dialogueSpeaker, &dialogueReason)
            && dialogueTarget != 0)
        {
            const int dialogueCaptionScore = ComputeCaptionNameMatchBias(
                normalizedCaption,
                NormalizeSearchText(CharacterNameForLog(dialogueTarget)));
            if (dialogueCaptionScore > 0)
            {
                return std::string("dialogue_trader:")
                    + NormalizeSearchText(CharacterNameForLog(dialogueTarget));
            }
        }

        return std::string("caption:") + normalizedCaption;
    }

    return std::string("widget:") + NormalizeSearchText(SafeWidgetName(identityWidget));
}

void ResetSearchQueryForTraderSwitch(const char* reason)
{
    const bool hadQuery = !g_searchQueryRaw.empty() || !g_searchQueryNormalized.empty();

    g_searchQueryRaw.clear();
    g_searchQueryNormalized.clear();
    g_pendingSlashFocusBaseQuery.clear();
    g_pendingSlashFocusTextSuppression = false;
    g_suppressNextSearchEditChangeEvent = false;
    g_loggedNumericOnlyQueryIgnored = false;
    g_lastSearchSampleQueryLogged.clear();
    g_lastZeroMatchQueryLogged.clear();

    if (hadQuery)
    {
        std::stringstream line;
        line << "search query reset"
             << " reason=" << (reason == 0 ? "<unknown>" : reason);
        LogInfoLine(line.str());
    }
}

void FocusSearchEdit(MyGUI::EditBox* searchEdit, const char* reason)
{
    if (searchEdit == 0)
    {
        return;
    }

    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    if (input == 0)
    {
        LogWarnLine("search focus skipped: MyGUI InputManager unavailable");
        return;
    }

    input->setKeyFocusWidget(searchEdit);

    std::stringstream line;
    line << "search edit focused"
         << " reason=" << (reason == 0 ? "<unknown>" : reason);
    LogInfoLine(line.str());
}

void FocusSearchEditIfRequested(MyGUI::EditBox* searchEdit, const char* reason)
{
    if (!g_focusSearchEditOnNextInjection || searchEdit == 0)
    {
        return;
    }

    g_focusSearchEditOnNextInjection = false;
    FocusSearchEdit(searchEdit, reason);
}

bool IsSearchEditFocused(MyGUI::EditBox* searchEdit)
{
    if (searchEdit == 0)
    {
        return false;
    }

    MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
    if (input == 0)
    {
        return false;
    }

    return input->getKeyFocusWidget() == searchEdit;
}

bool ShouldShowAnySearchCountMetric()
{
    return g_showSearchEntryCount || g_showSearchQuantityCount;
}

bool SearchHasNoVisibleResults()
{
    return !g_searchQueryNormalized.empty()
        && g_lastSearchTotalEntryCount > 0
        && g_lastSearchVisibleEntryCount == 0;
}

int ResolvePreferredSearchCountTextWidth()
{
    if (!ShouldShowAnySearchCountMetric())
    {
        return 0;
    }

    if (g_showSearchEntryCount && g_showSearchQuantityCount)
    {
        return 132;
    }

    return 72;
}

int ResolveSearchCountTextWidth(int availableWidth)
{
    if (!ShouldShowAnySearchCountMetric())
    {
        return 0;
    }

    int desiredWidth = ResolvePreferredSearchCountTextWidth();

    const int minSearchInputWidth = 120;
    const int minCountWidth = 44;
    const int maxAllowedWidth = availableWidth - minSearchInputWidth;
    if (maxAllowedWidth < minCountWidth)
    {
        return 0;
    }
    if (desiredWidth > maxAllowedWidth)
    {
        desiredWidth = maxAllowedWidth;
    }

    return desiredWidth;
}

std::string BuildSearchCountCaption(
    std::size_t visibleEntryCount,
    std::size_t totalEntryCount,
    std::size_t visibleQuantity)
{
    std::stringstream line;
    bool wroteMetric = false;

    if (g_showSearchEntryCount)
    {
        line << visibleEntryCount << " / " << totalEntryCount;
        wroteMetric = true;
    }

    if (g_showSearchQuantityCount)
    {
        if (wroteMetric)
        {
            line << " | ";
        }
        line << "qty " << visibleQuantity;
    }

    return line.str();
}

void UpdateSearchUiState()
{
    MyGUI::EditBox* searchEdit = FindSearchEditBox();
    if (searchEdit == 0)
    {
        return;
    }

    MyGUI::Button* clearButton = FindSearchClearButton();
    MyGUI::TextBox* placeholder = FindSearchPlaceholderTextBox();
    MyGUI::TextBox* countText = FindSearchCountTextBox();

    const bool hasQuery = !g_searchQueryRaw.empty();
    const bool focused = IsSearchEditFocused(searchEdit);
    const bool noVisibleResults = SearchHasNoVisibleResults();

    if (clearButton != 0)
    {
        const MyGUI::IntCoord clearCoord = clearButton->getCoord();
        const int clearButtonWidth = clearCoord.width;
        const int clearGap = 4;
        const int fullRight = clearCoord.left + clearCoord.width;
        const int maxEditWidth = fullRight - searchEdit->getLeft();
        int desiredEditWidth = maxEditWidth;
        if (hasQuery)
        {
            desiredEditWidth -= clearButtonWidth + clearGap;
        }
        if (desiredEditWidth < 80)
        {
            desiredEditWidth = 80;
        }

        const MyGUI::IntCoord editCoord = searchEdit->getCoord();
        if (editCoord.width != desiredEditWidth)
        {
            searchEdit->setCoord(editCoord.left, editCoord.top, desiredEditWidth, editCoord.height);
        }

        clearButton->setVisible(hasQuery);
        clearButton->setEnabled(hasQuery);
        clearButton->setAlpha(focused ? 1.0f : 0.86f);
        clearButton->setColour(noVisibleResults
            ? MyGUI::Colour(1.0f, 0.78f, 0.78f, 1.0f)
            : MyGUI::Colour::White);
    }

    if (placeholder != 0)
    {
        const bool showPlaceholder = !hasQuery && !focused;
        if (clearButton != 0)
        {
            const MyGUI::IntCoord editCoord = searchEdit->getCoord();
            const int placeholderLeft = editCoord.left + 10;
            const int placeholderTop = editCoord.top + 1;
            int placeholderWidth = editCoord.width - 16;
            if (placeholderWidth < 40)
            {
                placeholderWidth = 40;
            }

            const MyGUI::IntCoord placeholderCoord = placeholder->getCoord();
            if (placeholderCoord.left != placeholderLeft
                || placeholderCoord.top != placeholderTop
                || placeholderCoord.width != placeholderWidth
                || placeholderCoord.height != editCoord.height)
            {
                placeholder->setCoord(
                    placeholderLeft,
                    placeholderTop,
                    placeholderWidth,
                    editCoord.height);
            }
        }
        placeholder->setVisible(showPlaceholder);
        placeholder->setTextColour(
            focused
                ? MyGUI::Colour(0.82f, 0.82f, 0.82f, 1.0f)
                : MyGUI::Colour(0.63f, 0.63f, 0.63f, 1.0f));
    }

    searchEdit->setAlpha(focused ? 1.0f : 0.92f);
    searchEdit->setColour(
        noVisibleResults
            ? MyGUI::Colour(1.0f, 0.9f, 0.9f, 1.0f)
            : (focused
                ? MyGUI::Colour::White
                : MyGUI::Colour(0.93f, 0.93f, 0.93f, 1.0f)));

    if (countText != 0)
    {
        countText->setTextColour(
            noVisibleResults
                ? MyGUI::Colour(1.0f, 0.42f, 0.42f, 1.0f)
                : MyGUI::Colour(0.83f, 0.83f, 0.83f, 1.0f));
        countText->setAlpha(focused ? 1.0f : 0.92f);
    }
}

void UpdateSearchCountText(
    std::size_t visibleEntryCount,
    std::size_t totalEntryCount,
    std::size_t visibleQuantity)
{
    g_lastSearchVisibleEntryCount = visibleEntryCount;
    g_lastSearchTotalEntryCount = totalEntryCount;
    g_lastSearchVisibleQuantity = visibleQuantity;

    MyGUI::TextBox* countText = FindSearchCountTextBox();
    if (countText == 0)
    {
        UpdateSearchUiState();
        return;
    }

    countText->setVisible(ShouldShowAnySearchCountMetric());
    countText->setCaption(BuildSearchCountCaption(visibleEntryCount, totalEntryCount, visibleQuantity));
    UpdateSearchUiState();
}

bool TryGetCurrentMousePosition(int* xOut, int* yOut)
{
    if (xOut == 0 || yOut == 0)
    {
        return false;
    }

    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        return false;
    }

    const MyGUI::IntPoint mouse = inputManager->getMousePosition();
    *xOut = mouse.left;
    *yOut = mouse.top;
    return true;
}

MyGUI::IntCoord ClampSearchContainerCoord(MyGUI::Widget* parent, const MyGUI::IntCoord& inputCoord)
{
    if (parent == 0)
    {
        return inputCoord;
    }

    int left = inputCoord.left;
    int top = inputCoord.top;
    const int width = inputCoord.width;
    const int height = inputCoord.height;

    if (left < 8)
    {
        left = 8;
    }
    if (top < 8)
    {
        top = 8;
    }

    int maxLeft = parent->getWidth() - width - 8;
    int maxTop = parent->getHeight() - height - 8;
    if (maxLeft < 8)
    {
        maxLeft = 8;
    }
    if (maxTop < 8)
    {
        maxTop = 8;
    }

    if (left > maxLeft)
    {
        left = maxLeft;
    }
    if (top > maxTop)
    {
        top = maxTop;
    }

    return MyGUI::IntCoord(left, top, width, height);
}

void RememberSearchContainerPosition(MyGUI::Widget* container)
{
    if (container == 0)
    {
        return;
    }

    const MyGUI::IntCoord coord = container->getCoord();
    g_searchContainerStoredLeft = coord.left;
    g_searchContainerStoredTop = coord.top;
    g_searchContainerPositionCustomized = true;
}

void MoveSearchContainerByDelta(int deltaX, int deltaY)
{
    if (deltaX == 0 && deltaY == 0)
    {
        return;
    }

    MyGUI::Widget* container = FindControlsContainer();
    if (container == 0)
    {
        return;
    }

    MyGUI::Widget* parent = container->getParent();
    const MyGUI::IntCoord current = container->getCoord();
    const MyGUI::IntCoord moved = ClampSearchContainerCoord(
        parent,
        MyGUI::IntCoord(current.left + deltaX, current.top + deltaY, current.width, current.height));
    if (moved.left == current.left && moved.top == current.top)
    {
        return;
    }

    container->setCoord(moved);
    RememberSearchContainerPosition(container);
}

void FinalizeSearchContainerDrag(const char* source)
{
    if (!g_searchContainerDragging)
    {
        return;
    }

    g_searchContainerDragging = false;
    MyGUI::Widget* container = FindControlsContainer();
    if (container == 0)
    {
        return;
    }

    RememberSearchContainerPosition(container);

    std::stringstream line;
    const MyGUI::IntCoord coord = container->getCoord();
    line << "search container drag finalized"
         << " source=" << (source == 0 ? "<unknown>" : source)
         << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";
    LogInfoLine(line.str());
}

void OnSearchDragHandleMousePressed(MyGUI::Widget*, int left, int top, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left)
    {
        return;
    }

    g_searchContainerDragging = true;
    if (!TryGetCurrentMousePosition(&g_searchContainerDragLastMouseX, &g_searchContainerDragLastMouseY))
    {
        g_searchContainerDragLastMouseX = left;
        g_searchContainerDragLastMouseY = top;
    }
}

void OnSearchDragHandleMouseDrag(MyGUI::Widget*, int left, int top, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left || !g_searchContainerDragging)
    {
        return;
    }

    int mouseX = left;
    int mouseY = top;
    TryGetCurrentMousePosition(&mouseX, &mouseY);

    const int deltaX = mouseX - g_searchContainerDragLastMouseX;
    const int deltaY = mouseY - g_searchContainerDragLastMouseY;
    if (deltaX == 0 && deltaY == 0)
    {
        return;
    }

    MoveSearchContainerByDelta(deltaX, deltaY);
    g_searchContainerDragLastMouseX = mouseX;
    g_searchContainerDragLastMouseY = mouseY;
}

void OnSearchDragHandleMouseMove(MyGUI::Widget*, int left, int top)
{
    if (!g_searchContainerDragging)
    {
        return;
    }

    OnSearchDragHandleMouseDrag(0, left, top, MyGUI::MouseButton::Left);
}

void OnSearchDragHandleMouseReleased(MyGUI::Widget*, int, int, MyGUI::MouseButton id)
{
    if (id != MyGUI::MouseButton::Left)
    {
        return;
    }

    FinalizeSearchContainerDrag("drag_release");
}

void TickSearchContainerDrag()
{
    if (!g_searchContainerDragging)
    {
        return;
    }

    int mouseX = 0;
    int mouseY = 0;
    if (TryGetCurrentMousePosition(&mouseX, &mouseY))
    {
        const int deltaX = mouseX - g_searchContainerDragLastMouseX;
        const int deltaY = mouseY - g_searchContainerDragLastMouseY;
        MoveSearchContainerByDelta(deltaX, deltaY);
        g_searchContainerDragLastMouseX = mouseX;
        g_searchContainerDragLastMouseY = mouseY;
    }

    if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)
    {
        FinalizeSearchContainerDrag("drag_release_poll");
    }
}

void DestroyControlsIfPresent()
{
    MyGUI::Widget* controlsContainer = FindControlsContainer();
    if (controlsContainer == 0)
    {
        g_searchContainerDragging = false;
        g_controlsWereInjected = false;
        g_pendingSlashFocusBaseQuery.clear();
        g_pendingSlashFocusTextSuppression = false;
        g_suppressNextSearchEditChangeEvent = false;
        g_cachedHoveredWidgetInventory = 0;
        g_cachedHoveredWidgetInventorySignature.clear();
        ClearLockedKeysetSource();
        return;
    }

    RememberSearchContainerPosition(controlsContainer);
    g_searchContainerDragging = false;

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui != 0)
    {
        gui->destroyWidget(controlsContainer);
        LogInfoLine("controls container destroyed");
    }
    g_controlsWereInjected = false;
    g_pendingSlashFocusBaseQuery.clear();
    g_pendingSlashFocusTextSuppression = false;
    g_suppressNextSearchEditChangeEvent = false;
    g_cachedHoveredWidgetInventory = 0;
    g_cachedHoveredWidgetInventorySignature.clear();
    ClearLockedKeysetSource();
    ClearInventoryGuiInventoryLinks();
    ClearTraderPanelInventoryBindings();
}

void OnControlsAnchorCoordChanged(MyGUI::Widget* sender)
{
    if (sender == 0)
    {
        return;
    }

    const MyGUI::IntCoord coord = sender->getCoord();
    std::stringstream line;
    line << "anchor moved/resized name=" << SafeWidgetName(sender)
         << " coord=(" << coord.left << "," << coord.top << "," << coord.width << "," << coord.height << ")";
    LogInfoLine(line.str());
}

int RelativeBottomInParent(MyGUI::Widget* parent, MyGUI::Widget* child)
{
    if (parent == 0 || child == 0)
    {
        return 0;
    }

    const MyGUI::IntCoord parentAbsolute = parent->getAbsoluteCoord();
    const MyGUI::IntCoord childAbsolute = child->getAbsoluteCoord();
    return (childAbsolute.top - parentAbsolute.top) + childAbsolute.height;
}

int ResolveControlsTop(MyGUI::Widget* parent)
{
    const int defaultTop = 16;
    if (parent == 0)
    {
        return defaultTop;
    }

    const char* moneyWidgetPriority[] =
    {
        "MoneyAmountTextBox",
        "MoneyAmountText",
        "TotalMoneyBuyer",
        "lbTotalMoney",
        "MoneyLabelText",
        "lbBuyersMoney",
        "datapanel"
    };

    for (std::size_t index = 0; index < sizeof(moneyWidgetPriority) / sizeof(moneyWidgetPriority[0]); ++index)
    {
        MyGUI::Widget* moneyWidget = FindWidgetInParentByToken(parent, moneyWidgetPriority[index]);
        if (moneyWidget == 0)
        {
            continue;
        }

        int top = RelativeBottomInParent(parent, moneyWidget) + 8;
        if (top < defaultTop)
        {
            top = defaultTop;
        }

        std::stringstream line;
        line << "controls top resolved from widget=" << moneyWidgetPriority[index]
             << " top=" << top;
        LogInfoLine(line.str());
        return top;
    }

    LogInfoLine("controls top fallback to default (no money widget found)");
    return defaultTop;
}

bool TryStripSingleSlashShortcutInsertion(
    const std::string& currentText,
    const std::string& baseText,
    std::string* outRestoredText)
{
    if (outRestoredText == 0 || currentText.size() != baseText.size() + 1)
    {
        return false;
    }

    for (std::size_t index = 0; index < currentText.size(); ++index)
    {
        if (currentText[index] != '/')
        {
            continue;
        }

        const std::string restored =
            currentText.substr(0, index) + currentText.substr(index + 1);
        if (restored == baseText)
        {
            *outRestoredText = restored;
            return true;
        }
    }

    return false;
}

void SetSearchQueryAndRefresh(
    MyGUI::EditBox* searchEdit,
    const std::string& rawText,
    const char* reason,
    bool focusAfterSet)
{
    if (searchEdit == 0)
    {
        return;
    }

    g_searchQueryRaw = rawText;
    g_searchQueryNormalized = NormalizeSearchText(g_searchQueryRaw);
    g_loggedNumericOnlyQueryIgnored = false;
    g_lastSearchSampleQueryLogged.clear();
    g_lastZeroMatchQueryLogged.clear();
    g_pendingSlashFocusBaseQuery.clear();
    g_pendingSlashFocusTextSuppression = false;

    const std::string currentOnlyText = searchEdit->getOnlyText().asUTF8();
    if (currentOnlyText != rawText)
    {
        g_suppressNextSearchEditChangeEvent = true;
        searchEdit->setOnlyText(rawText);
    }

    if (focusAfterSet)
    {
        FocusSearchEdit(searchEdit, reason);
    }

    std::stringstream line;
    line << "search ui action"
         << " reason=" << (reason == 0 ? "<unknown>" : reason)
         << " raw=\"" << TruncateForLog(g_searchQueryRaw, 64) << "\""
         << " normalized=\"" << TruncateForLog(g_searchQueryNormalized, 64) << "\"";
    LogInfoLine(line.str());

    ApplySearchFilterFromControls(false, true);
    UpdateSearchUiState();
}

void OnSearchClearButtonClicked(MyGUI::Widget*)
{
    MyGUI::EditBox* searchEdit = FindSearchEditBox();
    if (searchEdit == 0)
    {
        return;
    }

    SetSearchQueryAndRefresh(searchEdit, "", "clear_button", true);
}

void OnSearchPlaceholderClicked(MyGUI::Widget*)
{
    MyGUI::EditBox* searchEdit = FindSearchEditBox();
    if (searchEdit == 0)
    {
        return;
    }

    FocusSearchEdit(searchEdit, "placeholder_click");
}

void OnSearchEditKeyFocusChanged(MyGUI::Widget*, MyGUI::Widget*)
{
    UpdateSearchUiState();
}

void OnSearchTextChanged(MyGUI::EditBox* sender)
{
    if (sender == 0)
    {
        return;
    }

    const std::string captionText = sender->getCaption().asUTF8();
    const std::string onlyText = sender->getOnlyText().asUTF8();
    if (g_suppressNextSearchEditChangeEvent && onlyText == g_searchQueryRaw)
    {
        g_suppressNextSearchEditChangeEvent = false;
        return;
    }
    g_suppressNextSearchEditChangeEvent = false;

    if (g_pendingSlashFocusTextSuppression)
    {
        std::string restoredText;
        if (TryStripSingleSlashShortcutInsertion(
                onlyText,
                g_pendingSlashFocusBaseQuery,
                &restoredText))
        {
            g_pendingSlashFocusTextSuppression = false;
            g_suppressNextSearchEditChangeEvent = true;
            sender->setOnlyText(restoredText);
            return;
        }

        g_pendingSlashFocusTextSuppression = false;
    }

    g_searchQueryRaw = onlyText;
    g_searchQueryNormalized = NormalizeSearchText(g_searchQueryRaw);
    g_loggedNumericOnlyQueryIgnored = false;

    std::stringstream line;
    line << "search input changed"
         << " raw=\"" << TruncateForLog(g_searchQueryRaw, 64) << "\""
         << " normalized=\"" << TruncateForLog(g_searchQueryNormalized, 64) << "\""
         << " raw_len=" << g_searchQueryRaw.size()
         << " caption_len=" << captionText.size()
         << " only_len=" << onlyText.size();
    LogInfoLine(line.str());

    ApplySearchFilterFromControls(false, true);
    UpdateSearchUiState();
}

bool BuildControlsScaffold(MyGUI::Widget* parent, int topOverride)
{
    if (parent == 0)
    {
        return false;
    }

    const int outerPadding = 8;
    const int rowHeight = g_searchInputConfiguredHeight;
    const int handleWidth = 28;
    const int handleGap = 6;
    const int preferredCountWidth = ResolvePreferredSearchCountTextWidth();
    const int preferredCountGap = preferredCountWidth > 0 ? 6 : 0;
    const int searchInputLeft = outerPadding + handleWidth + handleGap;
    const int desiredContainerWidth =
        searchInputLeft
        + g_searchInputConfiguredWidth
        + outerPadding
        + preferredCountWidth
        + preferredCountGap;
    const int maxContainerWidth =
        parent->getWidth() > 8 ? parent->getWidth() - 8 : parent->getWidth();
    int containerWidth = desiredContainerWidth;
    if (maxContainerWidth > 0 && containerWidth > maxContainerWidth)
    {
        containerWidth = maxContainerWidth;
    }
    if (containerWidth < 240)
    {
        containerWidth = 240;
    }
    if (maxContainerWidth > 0 && containerWidth > maxContainerWidth)
    {
        containerWidth = maxContainerWidth;
    }

    const int containerHeight = rowHeight + (outerPadding * 2);

    const int rightMargin = 16;
    int left = parent->getWidth() - containerWidth - rightMargin;
    if (left < 8)
    {
        left = 8;
    }

    int top = topOverride >= 0 ? topOverride : ResolveControlsTop(parent);
    const int maxTop = parent->getHeight() - containerHeight - 8;
    if (top > maxTop)
    {
        top = maxTop;
    }
    if (top < 8)
    {
        top = 8;
    }

    MyGUI::IntCoord containerCoord(left, top, containerWidth, containerHeight);
    if (g_searchContainerPositionCustomized)
    {
        containerCoord.left = g_searchContainerStoredLeft;
        containerCoord.top = g_searchContainerStoredTop;
    }
    containerCoord = ClampSearchContainerCoord(parent, containerCoord);

    {
        const MyGUI::IntCoord parentCoord = parent->getCoord();
        std::stringstream line;
        line << "building controls scaffold"
             << " parent=" << SafeWidgetName(parent)
             << " parent_coord=(" << parentCoord.left << "," << parentCoord.top << ","
             << parentCoord.width << "," << parentCoord.height << ")"
             << " container_coord=(" << containerCoord.left << "," << containerCoord.top << ","
             << containerCoord.width << "," << containerCoord.height << ")"
             << " customized_position=" << (g_searchContainerPositionCustomized ? "true" : "false")
             << " search_only=true";
        LogInfoLine(line.str());
    }

    MyGUI::Widget* container = parent->createWidget<MyGUI::Widget>(
        "Kenshi_GenericTextBoxFlatSkin",
        containerCoord,
        MyGUI::Align::Right | MyGUI::Align::Top,
        kControlsContainerName);
    if (container == 0)
    {
        LogErrorLine("failed to create controls container");
        return false;
    }
    const int searchInputAvailableWidth = containerWidth - searchInputLeft - outerPadding;
    int countWidth = ResolveSearchCountTextWidth(searchInputAvailableWidth);
    int countGap = countWidth > 0 ? 6 : 0;
    int searchAreaWidth = searchInputAvailableWidth - countWidth - countGap;
    if (searchAreaWidth < 120)
    {
        searchAreaWidth = 120;
    }
    int clearButtonWidth = rowHeight;
    if (clearButtonWidth > 26)
    {
        clearButtonWidth = 26;
    }
    if (clearButtonWidth < 18)
    {
        clearButtonWidth = 18;
    }

    MyGUI::Button* dragHandle = container->createWidget<MyGUI::Button>(
        "Kenshi_Button1",
        MyGUI::IntCoord(outerPadding, outerPadding, handleWidth, rowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSearchDragHandleName);
    if (dragHandle == 0)
    {
        LogErrorLine("failed to create search drag handle");
        DestroyControlsIfPresent();
        return false;
    }
    dragHandle->setCaption("::");
    dragHandle->setNeedMouseFocus(true);
    dragHandle->eventMouseButtonPressed += MyGUI::newDelegate(&OnSearchDragHandleMousePressed);
    dragHandle->eventMouseMove += MyGUI::newDelegate(&OnSearchDragHandleMouseMove);
    dragHandle->eventMouseDrag += MyGUI::newDelegate(&OnSearchDragHandleMouseDrag);
    dragHandle->eventMouseButtonReleased += MyGUI::newDelegate(&OnSearchDragHandleMouseReleased);

    if (countWidth > 0)
    {
        MyGUI::TextBox* countText = container->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(containerWidth - outerPadding - countWidth, outerPadding, countWidth, rowHeight),
            MyGUI::Align::Left | MyGUI::Align::Top,
            kSearchCountTextName);
        if (countText == 0)
        {
            LogErrorLine("failed to create search count text");
            DestroyControlsIfPresent();
            return false;
        }
        countText->setTextAlign(MyGUI::Align::Right | MyGUI::Align::VCenter);
        UpdateSearchCountText(0, 0, 0);
    }

    MyGUI::EditBox* searchEdit = container->createWidget<MyGUI::EditBox>(
        "Kenshi_EditBox",
        MyGUI::IntCoord(searchInputLeft, outerPadding, searchAreaWidth, rowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSearchEditName);
    if (searchEdit == 0)
    {
        LogErrorLine("failed to create search edit box");
        DestroyControlsIfPresent();
        return false;
    }
    searchEdit->setOnlyText(g_searchQueryRaw);
    searchEdit->eventEditTextChange += MyGUI::newDelegate(&OnSearchTextChanged);
    searchEdit->eventKeySetFocus += MyGUI::newDelegate(&OnSearchEditKeyFocusChanged);
    searchEdit->eventKeyLostFocus += MyGUI::newDelegate(&OnSearchEditKeyFocusChanged);

    MyGUI::TextBox* placeholder = container->createWidget<MyGUI::TextBox>(
        "Kenshi_TextboxStandardText",
        MyGUI::IntCoord(searchInputLeft + 10, outerPadding + 1, searchAreaWidth - 16, rowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSearchPlaceholderName);
    if (placeholder == 0)
    {
        LogErrorLine("failed to create search placeholder");
        DestroyControlsIfPresent();
        return false;
    }
    placeholder->setCaption("Search items...");
    placeholder->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
    placeholder->setNeedMouseFocus(true);
    placeholder->eventMouseButtonClick += MyGUI::newDelegate(&OnSearchPlaceholderClicked);

    MyGUI::Button* clearButton = container->createWidget<MyGUI::Button>(
        "Kenshi_Button1",
        MyGUI::IntCoord(
            searchInputLeft + searchAreaWidth - clearButtonWidth,
            outerPadding,
            clearButtonWidth,
            rowHeight),
        MyGUI::Align::Left | MyGUI::Align::Top,
        kSearchClearButtonName);
    if (clearButton == 0)
    {
        LogErrorLine("failed to create search clear button");
        DestroyControlsIfPresent();
        return false;
    }
    clearButton->setCaption("X");
    clearButton->eventMouseButtonClick += MyGUI::newDelegate(&OnSearchClearButtonClicked);

    FocusSearchEditIfRequested(searchEdit, "controls_built");
    UpdateSearchUiState();

    return true;
}

bool TryInjectControlsToTarget(MyGUI::Widget* anchor, MyGUI::Widget* parent, const char* sourceTag)
{
    if (anchor == 0 || parent == 0)
    {
        LogErrorLine("could not resolve anchor/parent widget for controls injection");
        return false;
    }

    std::string candidateReason;
    const int candidateScore = ComputeTraderWindowCandidateScore(parent, &candidateReason);
    const bool acceptedTarget = (sourceTag != 0 && std::string(sourceTag) == "hover-direct")
        || candidateScore > 0;

    if (!acceptedTarget)
    {
        std::stringstream line;
        line << "rejecting injection target reason=not_likely_trader_window"
             << " source=" << (sourceTag == 0 ? "<unknown>" : sourceTag)
             << " anchor=" << SafeWidgetName(anchor)
             << " parent=" << SafeWidgetName(parent)
             << " candidate_score=" << candidateScore
             << " candidate_reason=\"" << TruncateForLog(candidateReason, 120) << "\""
             << " has_trader_structure=" << (HasTraderStructure(parent) ? "true" : "false");
        LogWarnLine(line.str());
        return false;
    }

    const std::string nextTraderTargetId = BuildTraderTargetIdentity(anchor, parent);
    if (!nextTraderTargetId.empty() && !g_activeTraderTargetId.empty() && nextTraderTargetId != g_activeTraderTargetId)
    {
        ResetSearchQueryForTraderSwitch("target_changed");
    }
    g_activeTraderTargetId = nextTraderTargetId;
    g_focusSearchEditOnNextInjection = true;

    MyGUI::Widget* controlsParent = parent;
    int topOverride = -1;

    MyGUI::Window* owningWindow = FindOwningWindow(parent);
    if (owningWindow != 0 && parent != owningWindow)
    {
        controlsParent = owningWindow;
        topOverride = 12;
    }

    DestroyControlsIfPresent();
    if (!BuildControlsScaffold(controlsParent, topOverride))
    {
        LogErrorLine("failed to build phase 2 controls scaffold");
        return false;
    }

    g_controlsWereInjected = true;
    ApplySearchFilterToTraderParent(parent, false, true);

    std::stringstream line;
    line << "controls scaffold injected"
         << " source=" << (sourceTag == 0 ? "<unknown>" : sourceTag)
         << " anchor=" << SafeWidgetName(anchor)
         << " parent=" << SafeWidgetName(parent);
    LogInfoLine(line.str());
    return true;
}

bool TryInjectControlsToHoveredWindowDirect()
{
    MyGUI::InputManager* inputManager = MyGUI::InputManager::getInstancePtr();
    if (inputManager == 0)
    {
        LogWarnLine("manual attach failed: MyGUI InputManager unavailable");
        return false;
    }

    MyGUI::Widget* hovered = inputManager->getMouseFocusWidget();
    if (hovered == 0)
    {
        LogWarnLine("manual attach failed: no mouse-focused widget; hover target window and press hotkey again");
        return false;
    }

    MyGUI::Widget* anchor = FindBestWindowAnchor(hovered);
    MyGUI::Widget* parent = ResolveInjectionParent(anchor);
    if (anchor == 0 || parent == 0)
    {
        std::stringstream line;
        line << "manual attach failed: anchor/parent unresolved hovered_chain=" << BuildParentChainForLog(hovered);
        LogWarnLine(line.str());
        return false;
    }

    std::stringstream line;
    line << "manual attach using hovered window"
         << " anchor=" << SafeWidgetName(anchor)
         << " parent=" << SafeWidgetName(parent)
         << " parent_coord=(" << parent->getCoord().left << "," << parent->getCoord().top << ","
         << parent->getCoord().width << "," << parent->getCoord().height << ")"
         << " hovered_chain=" << BuildParentChainForLog(hovered);
    LogInfoLine(line.str());

    DumpHoveredAttachDiagnostics(hovered, anchor, parent);

    return TryInjectControlsToTarget(anchor, parent, "hover-direct");
}

void EnsureControlsInjectedIfEnabled()
{
    if (!g_controlsEnabled)
    {
        return;
    }

    if (FindControlsContainer() != 0)
    {
        return;
    }

    MyGUI::Widget* anchor = 0;
    MyGUI::Widget* parent = 0;
    if (!TryResolveVisibleTraderTarget(&anchor, &parent))
    {
        if (TryResolveHoveredTarget(&anchor, &parent, false))
        {
            g_loggedNoVisibleTraderTarget = false;
            g_loggedRejectedTraderCandidate = false;

            if (!TryInjectControlsToTarget(anchor, parent, "hover-auto"))
            {
                LogWarnLine("hover auto controls scaffold injection failed");
            }
            return;
        }

        if (!g_loggedNoVisibleTraderTarget)
        {
            LogInfoLine("controls enabled but no visible trader target found yet");
            DumpTraderTargetProbe();
            DumpVisibleWindowCandidateDiagnostics();
            g_loggedNoVisibleTraderTarget = true;
        }
        return;
    }
    g_loggedNoVisibleTraderTarget = false;
    g_loggedRejectedTraderCandidate = false;

    if (!TryInjectControlsToTarget(anchor, parent, "auto"))
    {
        LogWarnLine("auto controls scaffold injection failed");
    }
}

bool IsToggleHotkeyPressedEdge()
{
    if (key == 0 || key->keyboard == 0)
    {
        g_prevToggleHotkeyDown = false;
        return false;
    }

    const bool ctrlDown = key->keyboard->isKeyDown(OIS::KC_LCONTROL)
        || key->keyboard->isKeyDown(OIS::KC_RCONTROL);
    const bool shiftDown = key->keyboard->isKeyDown(OIS::KC_LSHIFT)
        || key->keyboard->isKeyDown(OIS::KC_RSHIFT);
    const bool f8Down = key->keyboard->isKeyDown(OIS::KC_F8);

    const bool chordDown = ctrlDown && shiftDown && f8Down;
    const bool pressedEdge = chordDown && !g_prevToggleHotkeyDown;
    g_prevToggleHotkeyDown = chordDown;
    return pressedEdge;
}

bool IsDiagnosticsHotkeyPressedEdge()
{
    if (key == 0 || key->keyboard == 0)
    {
        g_prevDiagnosticsHotkeyDown = false;
        return false;
    }

    const bool ctrlDown = key->keyboard->isKeyDown(OIS::KC_LCONTROL)
        || key->keyboard->isKeyDown(OIS::KC_RCONTROL);
    const bool shiftDown = key->keyboard->isKeyDown(OIS::KC_LSHIFT)
        || key->keyboard->isKeyDown(OIS::KC_RSHIFT);
    const bool f9Down = key->keyboard->isKeyDown(OIS::KC_F9);

    const bool chordDown = ctrlDown && shiftDown && f9Down;
    const bool pressedEdge = chordDown && !g_prevDiagnosticsHotkeyDown;
    g_prevDiagnosticsHotkeyDown = chordDown;
    return pressedEdge;
}

bool IsVirtualKeyDown(int virtualKey)
{
    return virtualKey > 0 && (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

bool IsSlashCharacterChordDown(bool shiftDown, bool ctrlDown, bool altDown)
{
    const HKL keyboardLayout = GetKeyboardLayout(0);
    const SHORT slashMapping = VkKeyScanExA('/', keyboardLayout);
    if (slashMapping != -1)
    {
        const int virtualKey = LOBYTE(static_cast<WORD>(slashMapping));
        const BYTE modifierMask = HIBYTE(static_cast<WORD>(slashMapping));
        const bool shiftRequired = (modifierMask & 1U) != 0;
        const bool ctrlRequired = (modifierMask & 2U) != 0;
        const bool altRequired = (modifierMask & 4U) != 0;

        if (shiftDown == shiftRequired
            && ctrlDown == ctrlRequired
            && altDown == altRequired
            && IsVirtualKeyDown(virtualKey))
        {
            return true;
        }
    }

    const bool oemSlashDown = IsVirtualKeyDown(VK_OEM_2);
    const bool numpadSlashDown = IsVirtualKeyDown(VK_DIVIDE);
    return !ctrlDown && !altDown && ((!shiftDown && oemSlashDown) || numpadSlashDown);
}

SearchFocusHotkeyKind DetectSearchFocusHotkeyPressedEdge(MyGUI::EditBox* searchEdit)
{
    if (searchEdit == 0 || key == 0 || key->keyboard == 0)
    {
        g_prevSearchSlashHotkeyDown = false;
        g_prevSearchCtrlFHotkeyDown = false;
        return SearchFocusHotkeyKind_None;
    }

    const bool searchFocused = IsSearchEditFocused(searchEdit);
    const bool ctrlDown = key->keyboard->isKeyDown(OIS::KC_LCONTROL)
        || key->keyboard->isKeyDown(OIS::KC_RCONTROL);
    const bool altDown = key->keyboard->isKeyDown(OIS::KC_LMENU)
        || key->keyboard->isKeyDown(OIS::KC_RMENU);
    const bool shiftDown = key->keyboard->isKeyDown(OIS::KC_LSHIFT)
        || key->keyboard->isKeyDown(OIS::KC_RSHIFT);
    const bool fDown = key->keyboard->isKeyDown(OIS::KC_F);

    const bool slashChordDown =
        !searchFocused && IsSlashCharacterChordDown(shiftDown, ctrlDown, altDown);
    const bool ctrlFChordDown = !searchFocused && ctrlDown && fDown;

    const bool slashPressedEdge = slashChordDown && !g_prevSearchSlashHotkeyDown;
    const bool ctrlFPressedEdge = ctrlFChordDown && !g_prevSearchCtrlFHotkeyDown;

    g_prevSearchSlashHotkeyDown = slashChordDown;
    g_prevSearchCtrlFHotkeyDown = ctrlFChordDown;

    if (slashPressedEdge)
    {
        return SearchFocusHotkeyKind_Slash;
    }
    if (ctrlFPressedEdge)
    {
        return SearchFocusHotkeyKind_CtrlF;
    }

    return SearchFocusHotkeyKind_None;
}

void LogRecentRefreshedInventorySummary(std::size_t expectedEntryCount)
{
    PruneRecentlyRefreshedInventories();
    if (g_recentRefreshedInventories.empty())
    {
        LogWarnLine("recent refresh inventory summary empty");
        return;
    }

    std::vector<RefreshedInventoryLink> sorted = g_recentRefreshedInventories;
    struct RecentInventorySorter
    {
        bool operator()(const RefreshedInventoryLink& left, const RefreshedInventoryLink& right) const
        {
            if (left.lastSeenTick != right.lastSeenTick)
            {
                return left.lastSeenTick > right.lastSeenTick;
            }
            if (left.ownerTrader != right.ownerTrader)
            {
                return left.ownerTrader;
            }
            if (left.visible != right.visible)
            {
                return left.visible;
            }
            return left.inventory < right.inventory;
        }
    };
    std::sort(sorted.begin(), sorted.end(), RecentInventorySorter());

    std::stringstream summary;
    summary << "recent refresh inventory summary"
            << " expected_entries=" << expectedEntryCount
            << " tracked=" << sorted.size();
    LogWarnLine(summary.str());

    std::size_t limit = sorted.size() < 14 ? sorted.size() : 14;
    for (std::size_t index = 0; index < limit; ++index)
    {
        const RefreshedInventoryLink& link = sorted[index];
        const unsigned long long ageTicks =
            g_updateTickCounter >= link.lastSeenTick
                ? g_updateTickCounter - link.lastSeenTick
                : 0ULL;
        std::vector<std::string> keys;
        const bool hasKeys = TryExtractSearchKeysFromInventory(link.inventory, &keys);
        std::stringstream line;
        line << "recent_refresh[" << index << "]"
             << " ptr=" << link.inventory
             << " owner=" << link.ownerName
             << " owner_trader=" << (link.ownerTrader ? "true" : "false")
             << " owner_selected=" << (link.ownerSelected ? "true" : "false")
             << " visible=" << (link.visible ? "true" : "false")
             << " items=" << link.itemCount
             << " age_ticks=" << ageTicks
             << " key_count=" << (hasKeys ? keys.size() : 0);
        if (hasKeys && !keys.empty())
        {
            line << " key0=\"" << TruncateForLog(keys[0], 48) << "\"";
        }
        LogWarnLine(line.str());
    }
}

void DumpOnDemandTraderDiagnosticsSnapshot()
{
    MyGUI::Widget* traderParent = ResolveTraderParentFromControlsContainer();
    if (traderParent == 0)
    {
        MyGUI::Widget* anchor = 0;
        MyGUI::Widget* parent = 0;
        if (TryResolveVisibleTraderTarget(&anchor, &parent) && parent != 0)
        {
            traderParent = parent;
        }
    }

    if (traderParent == 0)
    {
        LogWarnLine("manual diagnostics: could not resolve trader parent");
        LogInventoryBindingDiagnostics(0);
        return;
    }

    MyGUI::Widget* backpackContent = ResolveBestBackpackContentWidget(traderParent, true);
    if (backpackContent == 0)
    {
        backpackContent = FindWidgetInParentByToken(traderParent, "backpack_content");
    }
    MyGUI::Widget* entriesRoot =
        backpackContent == 0 ? 0 : ResolveInventoryEntriesRoot(backpackContent);

    if (entriesRoot == 0)
    {
        std::stringstream line;
        line << "manual diagnostics: entries root missing"
             << " parent=" << SafeWidgetName(traderParent)
             << " backpack=" << SafeWidgetName(backpackContent);
        LogWarnLine(line.str());
        LogInventoryBindingDiagnostics(0);
        return;
    }

    struct OrderedEntry
    {
        MyGUI::Widget* widget;
        MyGUI::IntCoord coord;
        int quantity;
    };

    std::vector<OrderedEntry> orderedEntries;
    const std::size_t childCount = entriesRoot->getChildCount();
    orderedEntries.reserve(childCount);
    for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex)
    {
        MyGUI::Widget* child = entriesRoot->getChildAt(childIndex);
        if (child == 0)
        {
            continue;
        }

        OrderedEntry entry;
        entry.widget = child;
        entry.coord = child->getCoord();
        entry.quantity = 0;
        TryResolveItemQuantityFromWidget(child, &entry.quantity);
        orderedEntries.push_back(entry);
    }

    struct OrderedEntryCoordLess
    {
        bool operator()(const OrderedEntry& left, const OrderedEntry& right) const
        {
            if (left.coord.top != right.coord.top)
            {
                return left.coord.top < right.coord.top;
            }
            if (left.coord.left != right.coord.left)
            {
                return left.coord.left < right.coord.left;
            }
            return left.widget < right.widget;
        }
    };
    std::sort(orderedEntries.begin(), orderedEntries.end(), OrderedEntryCoordLess());

    std::vector<int> uiQuantities;
    uiQuantities.reserve(orderedEntries.size());
    std::size_t occupiedCount = 0;
    std::stringstream quantityPreview;
    for (std::size_t index = 0; index < orderedEntries.size(); ++index)
    {
        const int quantity = orderedEntries[index].quantity;
        uiQuantities.push_back(quantity);
        if (quantity > 0)
        {
            ++occupiedCount;
        }
        if (index < 20)
        {
            if (index > 0)
            {
                quantityPreview << ",";
            }
            quantityPreview << quantity;
        }
    }
    if (orderedEntries.size() > 20)
    {
        quantityPreview << ",...";
    }

    {
        std::stringstream line;
        line << "manual diagnostics snapshot"
             << " parent=" << SafeWidgetName(traderParent)
             << " entries_root=" << SafeWidgetName(entriesRoot)
             << " total_entries=" << orderedEntries.size()
             << " occupied_entries=" << occupiedCount
             << " quantities=[" << quantityPreview.str() << "]";
        LogWarnLine(line.str());
    }
    LogRecentRefreshedInventorySummary(orderedEntries.size());

    g_lastInventoryKeysetSelectionSignature.clear();
    g_lastInventoryKeysetLowCoverageSignature.clear();
    g_lastCoverageFallbackDecisionSignature.clear();

    std::vector<std::string> inventoryNameKeys;
    std::vector<QuantityNameKey> inventoryQuantityNameKeys;
    std::string inventorySource;
    const bool resolved = TryResolveTraderInventoryNameKeys(
        traderParent,
        orderedEntries.size(),
        &uiQuantities,
        &inventoryNameKeys,
        &inventorySource,
        &inventoryQuantityNameKeys,
        false);

    std::stringstream result;
    result << "manual diagnostics keyset"
           << " resolved=" << (resolved ? "true" : "false")
           << " key_count=" << inventoryNameKeys.size()
           << " non_empty=" << CountNonEmptyKeys(inventoryNameKeys)
           << " quantity_key_count=" << inventoryQuantityNameKeys.size()
           << " source=\"" << TruncateForLog(inventorySource, 220) << "\"";
    LogWarnLine(result.str());

    if (resolved && !inventoryNameKeys.empty())
    {
        std::stringstream preview;
        preview << "manual diagnostics key preview " << BuildKeyPreviewForLog(inventoryNameKeys, 20);
        LogWarnLine(preview.str());
    }

    LogSearchSampleForQuery(entriesRoot, g_searchQueryNormalized, 12);

    LogInventoryBindingDiagnostics(orderedEntries.size());
}

void TickPhase2ControlsScaffold()
{
    TickSearchContainerDrag();

    if (IsDiagnosticsHotkeyPressedEdge())
    {
        DumpOnDemandTraderDiagnosticsSnapshot();
    }

    if (IsToggleHotkeyPressedEdge())
    {
        if (g_controlsEnabled)
        {
            ApplySearchFilterFromControls(true, false);
            g_controlsEnabled = false;
            DestroyControlsIfPresent();
            g_loggedNoVisibleTraderTarget = false;
            g_loggedRejectedTraderCandidate = false;
            LogInfoLine("controls toggled OFF");
            return;
        }

        g_controlsEnabled = true;
        LogInfoLine("controls toggled ON");

        g_loggedNoVisibleTraderTarget = false;
        g_loggedRejectedTraderCandidate = false;

        EnsureControlsInjectedIfEnabled();
        if (FindControlsContainer() == 0)
        {
            if (!TryInjectControlsToHoveredWindowDirect())
            {
                LogWarnLine("manual attach did not inject controls");
            }
        }
    }

    if (!g_controlsEnabled)
    {
        return;
    }

    EnsureControlsInjectedIfEnabled();
    bool handledSearchShortcut = false;
    MyGUI::EditBox* searchEdit = FindSearchEditBox();
    if (searchEdit != 0)
    {
        const SearchFocusHotkeyKind hotkeyKind =
            DetectSearchFocusHotkeyPressedEdge(searchEdit);
        if (hotkeyKind != SearchFocusHotkeyKind_None)
        {
            if (hotkeyKind == SearchFocusHotkeyKind_Slash)
            {
                g_pendingSlashFocusBaseQuery = g_searchQueryRaw;
                g_pendingSlashFocusTextSuppression = true;
            }
            else
            {
                g_pendingSlashFocusBaseQuery.clear();
                g_pendingSlashFocusTextSuppression = false;
            }
            FocusSearchEdit(searchEdit, "focus_hotkey");
            handledSearchShortcut = true;
        }
    }
    else
    {
        g_prevSearchSlashHotkeyDown = false;
        g_prevSearchCtrlFHotkeyDown = false;
        g_pendingSlashFocusBaseQuery.clear();
        g_pendingSlashFocusTextSuppression = false;
    }

    if (FindControlsContainer() != 0 && !handledSearchShortcut)
    {
        ApplySearchFilterFromControls(false, false);
    }

    if (g_controlsWereInjected && FindControlsContainer() == 0)
    {
        g_controlsWereInjected = false;
        g_cachedHoveredWidgetInventory = 0;
        g_cachedHoveredWidgetInventorySignature.clear();
        ClearLockedKeysetSource();
        ClearInventoryGuiInventoryLinks();
        ClearTraderPanelInventoryBindings();
        LogInfoLine("controls container no longer present (window likely closed/destroyed); hover target window and press Ctrl+Shift+F8 to attach again");
    }
}

void InventoryLayoutCreateGUI_hook(
    void* self,
    InventoryGUI* invGui,
    Ogre::map<std::string, InventorySectionGUI*>::type& sectionGuis,
    Inventory* inv)
{
    if (g_inventoryLayoutCreateGUIOrig != 0)
    {
        g_inventoryLayoutCreateGUIOrig(self, invGui, sectionGuis, inv);
    }

    if (inv == 0)
    {
        return;
    }

    ++g_inventoryLayoutCreateGUIHookCallCount;
    if (g_inventoryLayoutCreateGUIHookCallCount <= 8)
    {
        RootObject* owner = inv->getOwner();
        if (owner == 0)
        {
            owner = inv->getCallbackObject();
        }

        std::vector<std::string> keys;
        std::string keyPreview;
        if (TryExtractSearchKeysFromInventory(inv, &keys) && !keys.empty())
        {
            keyPreview = BuildKeyPreviewForLog(keys, 8);
        }

        MyGUI::Widget* firstSectionWidget = 0;
        std::string firstSectionName;
        for (Ogre::map<std::string, InventorySectionGUI*>::type::const_iterator it = sectionGuis.begin();
             it != sectionGuis.end();
             ++it)
        {
            InventorySectionGUI* sectionGui = it->second;
            if (sectionGui == 0 || sectionGui->_widget == 0)
            {
                continue;
            }

            firstSectionWidget = sectionGui->_widget;
            firstSectionName = it->first;
            break;
        }

        std::stringstream line;
        line << "createGUI hook call"
             << " index=" << g_inventoryLayoutCreateGUIHookCallCount
             << " self=" << self
             << " inv_gui=" << invGui
             << " owner=" << RootObjectDisplayNameForLog(owner)
             << " inv_items=" << InventoryItemCountForLog(inv)
             << " sections_total=" << sectionGuis.size()
             << " first_section=" << firstSectionName
             << " first_section_widget=" << firstSectionWidget;
        if (!keyPreview.empty())
        {
            line << " key_preview=\"" << TruncateForLog(keyPreview, 180) << "\"";
        }
        LogInfoLine(line.str());
    }

    RegisterInventoryGuiInventoryLink(invGui, inv);

    std::size_t boundSections = 0;
    std::stringstream sectionPreview;
    for (Ogre::map<std::string, InventorySectionGUI*>::type::const_iterator it = sectionGuis.begin();
         it != sectionGuis.end();
         ++it)
    {
        InventorySectionGUI* sectionGui = it->second;
        MyGUI::Widget* sectionWidget = sectionGui == 0 ? 0 : sectionGui->_widget;
        if (sectionWidget == 0)
        {
            continue;
        }

        RegisterSectionWidgetInventoryLink(sectionWidget, inv, it->first);
        if (boundSections < 6)
        {
            if (boundSections > 0)
            {
                sectionPreview << " | ";
            }
            sectionPreview << it->first << ":" << SafeWidgetName(sectionWidget);
        }

        ++boundSections;
    }

    if (boundSections == 0)
    {
        return;
    }

    std::stringstream signature;
    signature << inv
              << "|sections=" << boundSections
              << "|" << sectionPreview.str();
    if (signature.str() != g_lastSectionWidgetBindingSignature)
    {
        RootObject* owner = inv->getOwner();
        if (owner == 0)
        {
            owner = inv->getCallbackObject();
        }

        std::stringstream line;
        line << "inventory layout section binding"
             << " owner=" << RootObjectDisplayNameForLog(owner)
             << " inv_items=" << InventoryItemCountForLog(inv)
             << " sections=" << boundSections
             << " preview=\"" << TruncateForLog(sectionPreview.str(), 220) << "\"";
        LogInfoLine(line.str());
        g_lastSectionWidgetBindingSignature = signature.str();
    }
}

void Inventory_refreshGui_hook(Inventory* self)
{
    if (g_inventoryRefreshGuiOrig != 0)
    {
        g_inventoryRefreshGuiOrig(self);
    }

    RegisterRecentlyRefreshedInventory(self);
}

std::uintptr_t ResolveInventoryLayoutCreateGUIHookAddress(KenshiLib::BinaryVersion versionInfo)
{
    const std::string version = versionInfo.GetVersion();
    if (version != "1.0.65" && version != "1.0.68")
    {
        return 0;
    }

    const std::uintptr_t baseAddress = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(0));
    if (baseAddress == 0)
    {
        return 0;
    }

    const bool hasNonZeroPlatform = versionInfo.GetPlatform() != 0;
    if (hasNonZeroPlatform)
    {
        if (version == "1.0.65")
        {
            // 1.0.65 Steam: slot 37 (0x14F530) resolves into the backpack attach branch,
            // but the inventory-item population path lives at slot 1 / RVA 0x14EEA0.
            return baseAddress + 0x0014EEA0;
        }
        return baseAddress + 0x0014F570;
    }

    if (version == "1.0.65")
    {
        return baseAddress + 0x0014F450;
    }
    return baseAddress + 0x0014F470;
}

bool IsAddressInMainModuleTextSection(std::uintptr_t address)
{
    const std::uintptr_t baseAddress = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(0));
    if (baseAddress == 0 || address == 0)
    {
        return false;
    }

    const IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(baseAddress);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return false;
    }

    const IMAGE_NT_HEADERS64* ntHeaders =
        reinterpret_cast<const IMAGE_NT_HEADERS64*>(baseAddress + static_cast<std::uintptr_t>(dosHeader->e_lfanew));
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
    {
        return false;
    }

    const IMAGE_SECTION_HEADER* sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
    for (unsigned int index = 0; index < ntHeaders->FileHeader.NumberOfSections; ++index)
    {
        const IMAGE_SECTION_HEADER& section = sectionHeader[index];
        if (std::memcmp(section.Name, ".text", 5) != 0)
        {
            continue;
        }

        const std::uintptr_t sectionBegin = baseAddress + static_cast<std::uintptr_t>(section.VirtualAddress);
        const std::uintptr_t sectionSpan =
            section.Misc.VirtualSize > 0
                ? static_cast<std::uintptr_t>(section.Misc.VirtualSize)
                : static_cast<std::uintptr_t>(section.SizeOfRawData);
        const std::uintptr_t sectionEnd = sectionBegin + sectionSpan;
        return address >= sectionBegin && address < sectionEnd;
    }

    return false;
}

std::string FormatAbsoluteAddressForLog(std::uintptr_t address)
{
    if (address == 0)
    {
        return "0x0";
    }

    std::stringstream out;
    out << "0x" << std::hex << std::uppercase << address;

    const std::uintptr_t baseAddress = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(0));
    if (baseAddress != 0 && address >= baseAddress)
    {
        out << " (rva=0x" << std::hex << std::uppercase << (address - baseAddress) << ")";
    }
    return out.str();
}

void TryInstallInventoryLayoutCreateGUIHookEarly()
{
    if (g_inventoryLayoutCreateGUIHookInstalled || g_inventoryLayoutCreateGUIOrig != 0)
    {
        return;
    }

    if (g_inventoryLayoutCreateGUIEarlyAttempted)
    {
        return;
    }
    g_inventoryLayoutCreateGUIEarlyAttempted = true;

    if (g_expectedInventoryLayoutCreateGUIAddress == 0
        || !IsAddressInMainModuleTextSection(g_expectedInventoryLayoutCreateGUIAddress))
    {
        std::stringstream line;
        line << "early createGUI hook skipped: expected target invalid "
             << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress);
        LogWarnLine(line.str());
        return;
    }

    g_inventoryLayoutCreateGUIHookAttempted = true;
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            g_expectedInventoryLayoutCreateGUIAddress,
            InventoryLayoutCreateGUI_hook,
            &g_inventoryLayoutCreateGUIOrig))
    {
        std::stringstream line;
        line << "early createGUI hook install failed at "
             << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress)
             << " (will rely on deferred install)";
        LogWarnLine(line.str());
        return;
    }

    g_inventoryLayoutCreateGUIHookInstalled = true;
    std::stringstream line;
    line << "early createGUI hook installed at "
         << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress);
    LogInfoLine(line.str());
}

std::uintptr_t PointerDistance(std::uintptr_t left, std::uintptr_t right)
{
    return left >= right ? (left - right) : (right - left);
}

bool TryResolveRelativeJumpTarget(std::uintptr_t address, std::uintptr_t* outTarget)
{
    if (outTarget == 0 || address == 0 || !IsAddressInMainModuleTextSection(address))
    {
        return false;
    }

    const unsigned char* code = reinterpret_cast<const unsigned char*>(address);
    if (code == 0 || code[0] != 0xE9)
    {
        return false;
    }

    const std::int32_t rel = *reinterpret_cast<const std::int32_t*>(code + 1);
    const std::uintptr_t target =
        address + static_cast<std::uintptr_t>(5)
        + static_cast<std::uintptr_t>(static_cast<std::intptr_t>(rel));
    if (!IsAddressInMainModuleTextSection(target))
    {
        return false;
    }

    *outTarget = target;
    return true;
}

std::uintptr_t ResolveExecutableThunkTarget(std::uintptr_t address, int maxHops)
{
    std::uintptr_t resolved = address;
    for (int hop = 0; hop < maxHops; ++hop)
    {
        std::uintptr_t nextTarget = 0;
        if (!TryResolveRelativeJumpTarget(resolved, &nextTarget) || nextTarget == resolved)
        {
            break;
        }
        resolved = nextTarget;
    }
    return resolved;
}

void LogInventoryLayoutReturnDiagnostics(
    const char* sourceTag,
    RootObject* owner,
    InventoryLayout* layout)
{
    if (layout == 0 || sourceTag == 0)
    {
        return;
    }

    void*** vtableHolder = reinterpret_cast<void***>(layout);
    if (vtableHolder == 0 || *vtableHolder == 0)
    {
        return;
    }

    struct LayoutSlotCandidate
    {
        int slot;
        std::uintptr_t entryAddress;
        std::uintptr_t resolvedAddress;
        std::uintptr_t deltaToExpected;
        bool exactExpectedMatch;
    };

    std::vector<LayoutSlotCandidate> candidates;
    candidates.reserve(48);

    for (int slot = 0; slot < 48; ++slot)
    {
        const std::uintptr_t entryAddress = reinterpret_cast<std::uintptr_t>((*vtableHolder)[slot]);
        if (entryAddress == 0 || !IsAddressInMainModuleTextSection(entryAddress))
        {
            continue;
        }

        LayoutSlotCandidate candidate;
        candidate.slot = slot;
        candidate.entryAddress = entryAddress;
        candidate.resolvedAddress = ResolveExecutableThunkTarget(entryAddress, 2);
        candidate.deltaToExpected = 0;
        candidate.exactExpectedMatch = false;

        if (g_expectedInventoryLayoutCreateGUIAddress != 0)
        {
            const std::uintptr_t entryDelta =
                PointerDistance(candidate.entryAddress, g_expectedInventoryLayoutCreateGUIAddress);
            const std::uintptr_t resolvedDelta =
                PointerDistance(candidate.resolvedAddress, g_expectedInventoryLayoutCreateGUIAddress);
            candidate.deltaToExpected = entryDelta < resolvedDelta ? entryDelta : resolvedDelta;
            candidate.exactExpectedMatch =
                candidate.entryAddress == g_expectedInventoryLayoutCreateGUIAddress
                || candidate.resolvedAddress == g_expectedInventoryLayoutCreateGUIAddress;
        }

        candidates.push_back(candidate);
    }

    if (candidates.empty())
    {
        return;
    }

    std::size_t nearestIndex = 0;
    bool exactMatchFound = false;
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        if (candidates[index].exactExpectedMatch)
        {
            nearestIndex = index;
            exactMatchFound = true;
            break;
        }

        if (index == 0 || candidates[index].deltaToExpected < candidates[nearestIndex].deltaToExpected)
        {
            nearestIndex = index;
        }
    }

    std::stringstream preview;
    const std::size_t maxPreview = candidates.size() < 8 ? candidates.size() : 8;
    for (std::size_t index = 0; index < maxPreview; ++index)
    {
        if (index > 0)
        {
            preview << " | ";
        }
        preview << "slot" << candidates[index].slot
                << "=" << FormatAbsoluteAddressForLog(candidates[index].entryAddress)
                << "->" << FormatAbsoluteAddressForLog(candidates[index].resolvedAddress);
    }

    const LayoutSlotCandidate& nearest = candidates[nearestIndex];
    std::stringstream signature;
    signature << sourceTag
              << "|" << owner
              << "|" << layout
              << "|" << reinterpret_cast<void*>(*vtableHolder)
              << "|" << exactMatchFound
              << "|" << nearest.slot
              << "|" << nearest.entryAddress
              << "|" << g_inventoryLayoutCreateGUIHookInstalled
              << "|" << g_inventoryLayoutCreateGUIHookCallCount;
    if (signature.str() == g_lastInventoryLayoutReturnSignature
        && g_inventoryLayoutCreateInventoryLayoutLogCount >= 16)
    {
        return;
    }

    ++g_inventoryLayoutCreateInventoryLayoutLogCount;
    g_lastInventoryLayoutReturnSignature = signature.str();

    std::stringstream line;
    line << "createInventoryLayout result"
         << " source=" << sourceTag
         << " owner=" << RootObjectDisplayNameForLog(owner)
         << " layout=" << layout
         << " vtable=" << reinterpret_cast<void*>(*vtableHolder)
         << " expected=" << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress)
         << " exact_expected_slot=" << (exactMatchFound ? "true" : "false")
         << " nearest_slot=" << nearest.slot
         << " nearest_entry=" << FormatAbsoluteAddressForLog(nearest.entryAddress)
         << " nearest_resolved=" << FormatAbsoluteAddressForLog(nearest.resolvedAddress);
    if (!exactMatchFound && g_expectedInventoryLayoutCreateGUIAddress != 0)
    {
        line << " nearest_delta=0x" << std::hex << std::uppercase << nearest.deltaToExpected << std::dec;
    }
    line << " hook_installed=" << (g_inventoryLayoutCreateGUIHookInstalled ? "true" : "false")
         << " hook_calls=" << g_inventoryLayoutCreateGUIHookCallCount
         << " preview=\"" << TruncateForLog(preview.str(), 320) << "\"";

    if (!exactMatchFound && g_expectedInventoryLayoutCreateGUIAddress != 0)
    {
        LogWarnLine(line.str());
        return;
    }

    LogInfoLine(line.str());
}

void TryInstallInventoryLayoutCreateGUIHookFromLayout(InventoryLayout* layout)
{
    if (layout == 0 || g_inventoryLayoutCreateGUIHookInstalled)
    {
        return;
    }

    void*** vtableHolder = reinterpret_cast<void***>(layout);
    if (vtableHolder == 0 || *vtableHolder == 0)
    {
        return;
    }

    struct CreateGUIHookCandidate
    {
        int slot;
        std::uintptr_t entryAddress;
        std::uintptr_t resolvedAddress;
        std::uintptr_t deltaToExpected;
        bool exactExpectedMatch;
    };

    const std::uintptr_t kExpectedAddressTolerance = 0x3000;
    std::vector<CreateGUIHookCandidate> candidates;
    candidates.reserve(48);

    for (int slot = 0; slot < 48; ++slot)
    {
        const std::uintptr_t candidateAddress = reinterpret_cast<std::uintptr_t>((*vtableHolder)[slot]);
        if (candidateAddress == 0 || !IsAddressInMainModuleTextSection(candidateAddress))
        {
            continue;
        }

        CreateGUIHookCandidate candidate;
        candidate.slot = slot;
        candidate.entryAddress = candidateAddress;
        candidate.resolvedAddress = ResolveExecutableThunkTarget(candidateAddress, 2);
        candidate.deltaToExpected = 0;
        candidate.exactExpectedMatch = false;

        if (g_expectedInventoryLayoutCreateGUIAddress != 0)
        {
            const std::uintptr_t entryDelta =
                PointerDistance(candidate.entryAddress, g_expectedInventoryLayoutCreateGUIAddress);
            const std::uintptr_t resolvedDelta =
                PointerDistance(candidate.resolvedAddress, g_expectedInventoryLayoutCreateGUIAddress);
            candidate.deltaToExpected = entryDelta < resolvedDelta ? entryDelta : resolvedDelta;
            candidate.exactExpectedMatch =
                candidate.entryAddress == g_expectedInventoryLayoutCreateGUIAddress
                || candidate.resolvedAddress == g_expectedInventoryLayoutCreateGUIAddress;
        }

        candidates.push_back(candidate);
    }

    if (candidates.empty())
    {
        if (!g_inventoryLayoutCreateGUIHookAttempted)
        {
            LogWarnLine("deferred inventory layout createGUI hook skipped: no valid vtable slot candidate");
            g_inventoryLayoutCreateGUIHookAttempted = true;
        }
        return;
    }

    std::size_t selectedIndex = 0;
    bool selected = false;
    bool selectedNearExpected = false;
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        if (!candidates[index].exactExpectedMatch)
        {
            continue;
        }
        selectedIndex = index;
        selected = true;
        break;
    }

    if (!selected && g_expectedInventoryLayoutCreateGUIAddress != 0)
    {
        std::size_t bestIndex = 0;
        std::uintptr_t bestDelta = static_cast<std::uintptr_t>(-1);
        for (std::size_t index = 0; index < candidates.size(); ++index)
        {
            if (candidates[index].deltaToExpected < bestDelta)
            {
                bestIndex = index;
                bestDelta = candidates[index].deltaToExpected;
            }
        }

        if (bestDelta <= kExpectedAddressTolerance)
        {
            selectedIndex = bestIndex;
            selected = true;
            selectedNearExpected = true;
        }
    }

    if (!selected && g_expectedInventoryLayoutCreateGUIAddress == 0)
    {
        selectedIndex = 0;
        selected = true;
    }

    if (!selected)
    {
        std::stringstream candidatesLine;
        candidatesLine << "deferred inventory layout createGUI hook skipped: no expected/nearby candidate"
                       << " expected=" << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress);
        const std::size_t maxPreview = candidates.size() < 8 ? candidates.size() : 8;
        for (std::size_t index = 0; index < maxPreview; ++index)
        {
            candidatesLine << " slot" << candidates[index].slot
                           << "=" << FormatAbsoluteAddressForLog(candidates[index].entryAddress)
                           << "->" << FormatAbsoluteAddressForLog(candidates[index].resolvedAddress);
        }
        LogWarnLine(candidatesLine.str());
        g_inventoryLayoutCreateGUIHookAttempted = true;
        return;
    }

    const CreateGUIHookCandidate& selectedCandidate = candidates[selectedIndex];
    g_inventoryLayoutCreateGUIHookAttempted = true;
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            selectedCandidate.entryAddress,
            InventoryLayoutCreateGUI_hook,
            &g_inventoryLayoutCreateGUIOrig))
    {
        std::stringstream line;
        line << "could not install deferred inventory layout createGUI hook"
             << " target=" << FormatAbsoluteAddressForLog(selectedCandidate.entryAddress)
             << " resolved=" << FormatAbsoluteAddressForLog(selectedCandidate.resolvedAddress)
             << " expected=" << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress)
             << " slot=" << selectedCandidate.slot;
        LogWarnLine(line.str());
        return;
    }

    g_inventoryLayoutCreateGUIHookInstalled = true;
    if (selectedNearExpected)
    {
        std::stringstream line;
        line << "deferred inventory layout createGUI hook installed from nearby candidate"
             << " target=" << FormatAbsoluteAddressForLog(selectedCandidate.entryAddress)
             << " resolved=" << FormatAbsoluteAddressForLog(selectedCandidate.resolvedAddress)
             << " expected=" << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress)
             << " delta=0x" << std::hex << std::uppercase << selectedCandidate.deltaToExpected
             << std::dec
             << " slot=" << selectedCandidate.slot;
        LogWarnLine(line.str());
    }
    else
    {
        std::stringstream line;
        line << "deferred inventory layout createGUI hook installed"
             << " target=" << FormatAbsoluteAddressForLog(selectedCandidate.entryAddress)
             << " resolved=" << FormatAbsoluteAddressForLog(selectedCandidate.resolvedAddress)
             << " expected=" << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress)
             << " slot=" << selectedCandidate.slot;
        LogInfoLine(line.str());
    }
}

InventoryLayout* Character_createInventoryLayout_hook(Character* self)
{
    if (g_characterCreateInventoryLayoutOrig == 0)
    {
        return 0;
    }

    TryInstallInventoryLayoutCreateGUIHookEarly();
    InventoryLayout* layout = g_characterCreateInventoryLayoutOrig(self);
    LogInventoryLayoutReturnDiagnostics("character", self, layout);
    TryInstallInventoryLayoutCreateGUIHookFromLayout(layout);
    return layout;
}

InventoryLayout* RootObject_createInventoryLayout_hook(RootObject* self)
{
    if (g_rootObjectCreateInventoryLayoutOrig == 0)
    {
        return 0;
    }

    TryInstallInventoryLayoutCreateGUIHookEarly();
    InventoryLayout* layout = g_rootObjectCreateInventoryLayoutOrig(self);
    LogInventoryLayoutReturnDiagnostics("root_object", self, layout);
    TryInstallInventoryLayoutCreateGUIHookFromLayout(layout);
    return layout;
}

void PlayerInterface_updateUT_hook(PlayerInterface* self)
{
    ++g_updateTickCounter;
    if ((g_updateTickCounter % 240ULL) == 0ULL)
    {
        PruneRecentlyRefreshedInventories();
        PruneSectionWidgetInventoryLinks();
        PruneInventoryGuiInventoryLinks();
        PruneTraderPanelInventoryBindings();
    }
    if (g_updateUTOrig != 0)
    {
        g_updateUTOrig(self);
    }

    TickPhase2ControlsScaffold();
}
}

__declspec(dllexport) void startPlugin()
{
    LogInfoLine("startPlugin()");

    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    if (!IsSupportedVersion(versionInfo))
    {
        LogErrorLine("unsupported Kenshi version/platform");
        return;
    }

    {
        std::stringstream line;
        line << "binary version detected"
             << " platform=" << versionInfo.GetPlatform()
             << " version=" << versionInfo.GetVersion();
        LogInfoLine(line.str());
    }

    LoadModConfig();

    g_expectedInventoryLayoutCreateGUIAddress =
        ResolveInventoryLayoutCreateGUIHookAddress(versionInfo);
    if (g_expectedInventoryLayoutCreateGUIAddress != 0)
    {
        std::stringstream line;
        line << "inventory layout createGUI expected target="
             << FormatAbsoluteAddressForLog(g_expectedInventoryLayoutCreateGUIAddress)
             << " (deferred install)";
        LogInfoLine(line.str());
    }
    else
    {
        LogWarnLine("inventory layout createGUI expected target unresolved; deferred hook disabled");
    }

    LogWarnLine("Inventory::refreshGui hook disabled (unsafe in current runtime)");

    const std::uintptr_t characterCreateLayoutAddress =
        KenshiLib::GetRealAddress(&Character::_NV_createInventoryLayout);
    if (characterCreateLayoutAddress == 0
        || KenshiLib::SUCCESS != KenshiLib::AddHook(
            characterCreateLayoutAddress,
            Character_createInventoryLayout_hook,
            &g_characterCreateInventoryLayoutOrig))
    {
        LogWarnLine("could not hook Character::_NV_createInventoryLayout; deferred createGUI binding may be unavailable");
    }
    else
    {
        std::stringstream line;
        line << "hooked Character::_NV_createInventoryLayout at "
             << FormatAbsoluteAddressForLog(characterCreateLayoutAddress);
        LogInfoLine(line.str());
    }

    const std::uintptr_t rootObjectCreateLayoutAddress =
        KenshiLib::GetRealAddress(&RootObject::_NV_createInventoryLayout);
    if (rootObjectCreateLayoutAddress == 0
        || rootObjectCreateLayoutAddress == characterCreateLayoutAddress
        || KenshiLib::SUCCESS != KenshiLib::AddHook(
            rootObjectCreateLayoutAddress,
            RootObject_createInventoryLayout_hook,
            &g_rootObjectCreateInventoryLayoutOrig))
    {
        LogWarnLine("could not hook RootObject::_NV_createInventoryLayout; deferred createGUI binding for building/shop layouts may be unavailable");
    }
    else
    {
        std::stringstream line;
        line << "hooked RootObject::_NV_createInventoryLayout at "
             << FormatAbsoluteAddressForLog(rootObjectCreateLayoutAddress);
        LogInfoLine(line.str());
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        &g_updateUTOrig))
    {
        LogErrorLine("could not hook PlayerInterface::updateUT");
        return;
    }

    std::stringstream info;
    info << "phase 2 controls scaffold active."
         << " Auto-attach is enabled for detected trader windows."
         << " Press " << kToggleHotkeyHint << " to hide/show."
         << " Press " << kDiagnosticsHotkeyHint << " for a diagnostics snapshot.";
    LogInfoLine(info.str());
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
