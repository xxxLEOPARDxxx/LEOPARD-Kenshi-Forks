#pragma once

#include <string>

static const int kSearchInputConfiguredWidthMin = 120;
static const int kSearchInputConfiguredWidthMax = 720;
static const int kSearchInputConfiguredHeightMin = 22;
static const int kSearchInputConfiguredHeightMax = 48;
static const int kDefaultSearchInputConfiguredWidth = 372;
static const int kDefaultSearchInputConfiguredHeight = 26;

struct InventoryConfigSnapshot
{
    InventoryConfigSnapshot()
        : enabled(true)
        , showSearchEntryCount(true)
        , showSearchQuantityCount(true)
        , showSearchClearButton(true)
        , autoFocusSearchInput(false)
        , followActiveInventory(true)
        , debugLogging(false)
        , debugSearchLogging(false)
        , debugBindingLogging(false)
        , enableDebugProbes(false)
        , searchInputWidth(kDefaultSearchInputConfiguredWidth)
        , searchInputHeight(kDefaultSearchInputConfiguredHeight)
        , searchInputPositionCustomized(false)
        , searchInputLeft(0)
        , searchInputTop(0)
    {
    }

    bool enabled;
    bool showSearchEntryCount;
    bool showSearchQuantityCount;
    bool showSearchClearButton;
    bool autoFocusSearchInput;
    bool followActiveInventory;
    bool debugLogging;
    bool debugSearchLogging;
    bool debugBindingLogging;
    bool enableDebugProbes;
    int searchInputWidth;
    int searchInputHeight;
    bool searchInputPositionCustomized;
    int searchInputLeft;
    int searchInputTop;
};

struct InventoryRuntimeState
{
    InventoryRuntimeState()
        : g_controlsEnabled(true)
        , g_showSearchEntryCount(true)
        , g_showSearchQuantityCount(true)
        , g_showSearchClearButton(true)
        , g_autoFocusSearchInput(false)
        , g_followActiveInventory(true)
        , g_debugLogging(false)
        , g_debugSearchLogging(false)
        , g_debugBindingLogging(false)
        , g_enableDebugProbes(false)
        , g_searchInputConfiguredWidth(kDefaultSearchInputConfiguredWidth)
        , g_searchInputConfiguredHeight(kDefaultSearchInputConfiguredHeight)
        , g_searchInputPositionCustomized(false)
        , g_searchInputStoredLeft(0)
        , g_searchInputStoredTop(0)
    {
    }

    bool g_controlsEnabled;
    bool g_showSearchEntryCount;
    bool g_showSearchQuantityCount;
    bool g_showSearchClearButton;
    bool g_autoFocusSearchInput;
    bool g_followActiveInventory;
    bool g_debugLogging;
    bool g_debugSearchLogging;
    bool g_debugBindingLogging;
    bool g_enableDebugProbes;
    int g_searchInputConfiguredWidth;
    int g_searchInputConfiguredHeight;
    bool g_searchInputPositionCustomized;
    int g_searchInputStoredLeft;
    int g_searchInputStoredTop;
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
