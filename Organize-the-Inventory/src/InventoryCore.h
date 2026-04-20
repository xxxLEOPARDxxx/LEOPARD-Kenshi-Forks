#pragma once

#include <string>

static const int kSearchInputConfiguredWidthMin = 120;
static const int kSearchInputConfiguredWidthMax = 720;
static const int kSearchInputConfiguredHeightMin = 22;
static const int kSearchInputConfiguredHeightMax = 48;
static const int kSearchBarConfiguredWidthMin = 236;
static const int kSearchBarConfiguredWidthMax = 960;
static const int kDefaultSearchInputConfiguredWidth = 372;
static const int kDefaultSearchInputConfiguredHeight = 26;
static const int kDefaultSearchBarConfiguredWidth = 504;

struct InventoryConfigSnapshot
{
    InventoryConfigSnapshot()
        : enabled(true)
        , creatureSearchEnabled(true)
        , showSearchEntryCount(true)
        , showSearchQuantityCount(true)
        , showSearchClearButton(true)
        , autoFocusSearchInput(false)
        , followActiveInventory(true)
        , debugLogging(false)
        , debugSearchLogging(false)
        , debugBindingLogging(false)
        , enableDebugProbes(false)
        , searchBarWidth(kDefaultSearchBarConfiguredWidth)
        , searchInputWidth(kDefaultSearchInputConfiguredWidth)
        , searchInputHeight(kDefaultSearchInputConfiguredHeight)
        , searchInputPositionCustomized(false)
        , searchInputLeft(0)
        , searchInputTop(0)
        , creatureSearchBarWidth(kDefaultSearchBarConfiguredWidth)
        , creatureSearchInputWidth(kDefaultSearchInputConfiguredWidth)
        , creatureSearchInputHeight(kDefaultSearchInputConfiguredHeight)
        , creatureSearchInputPositionCustomized(false)
        , creatureSearchInputLeft(0)
        , creatureSearchInputTop(0)
    {
    }

    bool enabled;
    bool creatureSearchEnabled;
    bool showSearchEntryCount;
    bool showSearchQuantityCount;
    bool showSearchClearButton;
    bool autoFocusSearchInput;
    bool followActiveInventory;
    bool debugLogging;
    bool debugSearchLogging;
    bool debugBindingLogging;
    bool enableDebugProbes;
    int searchBarWidth;
    int searchInputWidth;
    int searchInputHeight;
    bool searchInputPositionCustomized;
    int searchInputLeft;
    int searchInputTop;
    int creatureSearchBarWidth;
    int creatureSearchInputWidth;
    int creatureSearchInputHeight;
    bool creatureSearchInputPositionCustomized;
    int creatureSearchInputLeft;
    int creatureSearchInputTop;
};

struct InventoryRuntimeState
{
    InventoryRuntimeState()
        : g_controlsEnabled(true)
        , g_creatureSearchEnabled(true)
        , g_showSearchEntryCount(true)
        , g_showSearchQuantityCount(true)
        , g_showSearchClearButton(true)
        , g_autoFocusSearchInput(false)
        , g_followActiveInventory(true)
        , g_debugLogging(false)
        , g_debugSearchLogging(false)
        , g_debugBindingLogging(false)
        , g_enableDebugProbes(false)
        , g_searchBarConfiguredWidth(kDefaultSearchBarConfiguredWidth)
        , g_searchInputConfiguredWidth(kDefaultSearchInputConfiguredWidth)
        , g_searchInputConfiguredHeight(kDefaultSearchInputConfiguredHeight)
        , g_searchInputPositionCustomized(false)
        , g_searchInputStoredLeft(0)
        , g_searchInputStoredTop(0)
        , g_creatureSearchBarConfiguredWidth(kDefaultSearchBarConfiguredWidth)
        , g_creatureSearchInputConfiguredWidth(kDefaultSearchInputConfiguredWidth)
        , g_creatureSearchInputConfiguredHeight(kDefaultSearchInputConfiguredHeight)
        , g_creatureSearchInputPositionCustomized(false)
        , g_creatureSearchInputStoredLeft(0)
        , g_creatureSearchInputStoredTop(0)
    {
    }

    bool g_controlsEnabled;
    bool g_creatureSearchEnabled;
    bool g_showSearchEntryCount;
    bool g_showSearchQuantityCount;
    bool g_showSearchClearButton;
    bool g_autoFocusSearchInput;
    bool g_followActiveInventory;
    bool g_debugLogging;
    bool g_debugSearchLogging;
    bool g_debugBindingLogging;
    bool g_enableDebugProbes;
    int g_searchBarConfiguredWidth;
    int g_searchInputConfiguredWidth;
    int g_searchInputConfiguredHeight;
    bool g_searchInputPositionCustomized;
    int g_searchInputStoredLeft;
    int g_searchInputStoredTop;
    int g_creatureSearchBarConfiguredWidth;
    int g_creatureSearchInputConfiguredWidth;
    int g_creatureSearchInputConfiguredHeight;
    bool g_creatureSearchInputPositionCustomized;
    int g_creatureSearchInputStoredLeft;
    int g_creatureSearchInputStoredTop;
    std::string g_searchQueryRaw;
};

InventoryRuntimeState& InventoryState();

void LogInfoLine(const std::string& message);
void LogWarnLine(const std::string& message);
void LogErrorLine(const std::string& message);
bool ShouldCompileVerboseDiagnostics();
bool ShouldLogDebug();
bool ShouldLogSearchDebug();
bool ShouldLogBindingDebug();
bool ShouldEnableDebugProbes();
void LogDebugLine(const std::string& message);
void LogSearchDebugLine(const std::string& message);
void LogBindingDebugLine(const std::string& message);
