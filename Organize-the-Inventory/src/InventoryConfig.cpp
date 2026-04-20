#include "InventoryConfig.h"

#include <Windows.h>

#include <cctype>
#include <fstream>
#include <sstream>

namespace
{
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

bool TryResolveModConfigPath(std::string* outPath)
{
    if (outPath == 0)
    {
        return false;
    }

    const std::string pluginDirectory = GetCurrentPluginDirectoryPath();
    if (pluginDirectory.empty())
    {
        return false;
    }

    *outPath = pluginDirectory + "\\mod-config.json";
    return true;
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

bool TryWriteTextFileAtomically(const std::string& path, const std::string& content)
{
    if (path.empty())
    {
        return false;
    }

    const std::string tempPath = path + ".tmp";
    {
        std::ofstream output(tempPath.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            return false;
        }

        output.write(content.c_str(), static_cast<std::streamsize>(content.size()));
        output.flush();
        if (!output.good())
        {
            output.close();
            DeleteFileA(tempPath.c_str());
            return false;
        }
    }

    if (MoveFileExA(
            tempPath.c_str(),
            path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH) == 0)
    {
        DeleteFileA(tempPath.c_str());
        return false;
    }

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
    return ClampIntValue(value, kSearchInputConfiguredWidthMin, kSearchInputConfiguredWidthMax);
}

int ClampSearchInputConfiguredHeight(int value)
{
    return ClampIntValue(value, kSearchInputConfiguredHeightMin, kSearchInputConfiguredHeightMax);
}

int ComputeDefaultSearchBarConfiguredWidth(int searchInputWidth)
{
    return searchInputWidth + (kDefaultSearchBarConfiguredWidth - kDefaultSearchInputConfiguredWidth);
}

int ComputeMinimumSearchBarConfiguredWidth(const InventoryConfigSnapshot& config)
{
    const int kSearchBarChromeWidth = 52;
    const int kSearchCountWidthMin = 56;
    const bool reserveCount = config.showSearchEntryCount || config.showSearchQuantityCount;
    return config.searchInputWidth + kSearchBarChromeWidth + (reserveCount ? kSearchCountWidthMin : 0);
}

int ClampSearchBarConfiguredWidth(int value)
{
    return ClampIntValue(value, kSearchBarConfiguredWidthMin, kSearchBarConfiguredWidthMax);
}

std::string BuildInventoryConfigText(const InventoryConfigSnapshot& config)
{
    std::stringstream content;
    content << "{\n"
            << "  \"enabled\": " << (config.enabled ? "true" : "false") << ",\n"
            << "  \"showSearchEntryCount\": "
            << (config.showSearchEntryCount ? "true" : "false") << ",\n"
            << "  \"showSearchQuantityCount\": "
            << (config.showSearchQuantityCount ? "true" : "false") << ",\n"
            << "  \"showSearchClearButton\": "
            << (config.showSearchClearButton ? "true" : "false") << ",\n"
            << "  \"autoFocusSearchInput\": "
            << (config.autoFocusSearchInput ? "true" : "false") << ",\n"
            << "  \"followActiveInventory\": "
            << (config.followActiveInventory ? "true" : "false") << ",\n"
            << "  \"debugLogging\": " << (config.debugLogging ? "true" : "false") << ",\n"
            << "  \"debugSearchLogging\": "
            << (config.debugSearchLogging ? "true" : "false") << ",\n"
            << "  \"debugBindingLogging\": "
            << (config.debugBindingLogging ? "true" : "false") << ",\n"
            << "  \"enableDebugProbes\": "
            << (config.enableDebugProbes ? "true" : "false") << ",\n"
            << "  \"searchBarWidth\": " << config.searchBarWidth << ",\n"
            << "  \"searchInputWidth\": " << config.searchInputWidth << ",\n"
            << "  \"searchInputHeight\": " << config.searchInputHeight << ",\n"
            << "  \"searchInputPositionCustomized\": "
            << (config.searchInputPositionCustomized ? "true" : "false") << ",\n"
            << "  \"searchInputLeft\": " << config.searchInputLeft << ",\n"
            << "  \"searchInputTop\": " << config.searchInputTop << "\n"
            << "}\n";
    return content.str();
}

void LogInventoryConfigSnapshot(const char* prefix, const InventoryConfigSnapshot& config)
{
    std::stringstream line;
    line << prefix
         << " enabled=" << (config.enabled ? "true" : "false")
         << " showSearchEntryCount=" << (config.showSearchEntryCount ? "true" : "false")
         << " showSearchQuantityCount="
         << (config.showSearchQuantityCount ? "true" : "false")
         << " showSearchClearButton="
         << (config.showSearchClearButton ? "true" : "false")
         << " autoFocusSearchInput="
         << (config.autoFocusSearchInput ? "true" : "false")
         << " followActiveInventory="
         << (config.followActiveInventory ? "true" : "false")
         << " debugLogging=" << (config.debugLogging ? "true" : "false")
         << " debugSearchLogging=" << (config.debugSearchLogging ? "true" : "false")
         << " debugBindingLogging=" << (config.debugBindingLogging ? "true" : "false")
         << " enableDebugProbes=" << (config.enableDebugProbes ? "true" : "false")
         << " searchBarWidth=" << config.searchBarWidth
         << " searchInputWidth=" << config.searchInputWidth
         << " searchInputHeight=" << config.searchInputHeight
         << " searchInputPositionCustomized="
         << (config.searchInputPositionCustomized ? "true" : "false")
         << " searchInputLeft=" << config.searchInputLeft
         << " searchInputTop=" << config.searchInputTop
         << " verboseDiagnosticsCompiled="
         << (ShouldCompileVerboseDiagnostics() ? "true" : "false");
    LogInfoLine(line.str());
}
}

void NormalizeInventoryConfigSnapshot(InventoryConfigSnapshot* config)
{
    if (config == 0)
    {
        return;
    }

    config->searchInputWidth = ClampSearchInputConfiguredWidth(config->searchInputWidth);
    config->searchInputHeight = ClampSearchInputConfiguredHeight(config->searchInputHeight);
    config->searchBarWidth = ClampSearchBarConfiguredWidth(config->searchBarWidth);
    const int minimumSearchBarWidth = ComputeMinimumSearchBarConfiguredWidth(*config);
    if (config->searchBarWidth < minimumSearchBarWidth)
    {
        config->searchBarWidth = minimumSearchBarWidth;
    }
    if (!config->searchInputPositionCustomized)
    {
        config->searchInputLeft = 0;
        config->searchInputTop = 0;
    }
}

InventoryConfigSnapshot CaptureInventoryConfigSnapshot()
{
    InventoryConfigSnapshot config;
    config.enabled = InventoryState().g_controlsEnabled;
    config.showSearchEntryCount = InventoryState().g_showSearchEntryCount;
    config.showSearchQuantityCount = InventoryState().g_showSearchQuantityCount;
    config.showSearchClearButton = InventoryState().g_showSearchClearButton;
    config.autoFocusSearchInput = InventoryState().g_autoFocusSearchInput;
    config.followActiveInventory = InventoryState().g_followActiveInventory;
    config.debugLogging = InventoryState().g_debugLogging;
    config.debugSearchLogging = InventoryState().g_debugSearchLogging;
    config.debugBindingLogging = InventoryState().g_debugBindingLogging;
    config.enableDebugProbes = InventoryState().g_enableDebugProbes;
    config.searchBarWidth = InventoryState().g_searchBarConfiguredWidth;
    config.searchInputWidth = InventoryState().g_searchInputConfiguredWidth;
    config.searchInputHeight = InventoryState().g_searchInputConfiguredHeight;
    config.searchInputPositionCustomized = InventoryState().g_searchInputPositionCustomized;
    config.searchInputLeft = InventoryState().g_searchInputStoredLeft;
    config.searchInputTop = InventoryState().g_searchInputStoredTop;
    NormalizeInventoryConfigSnapshot(&config);
    return config;
}

void ApplyInventoryConfigSnapshot(const InventoryConfigSnapshot& config)
{
    InventoryConfigSnapshot normalized = config;
    NormalizeInventoryConfigSnapshot(&normalized);

    InventoryState().g_controlsEnabled = normalized.enabled;
    InventoryState().g_showSearchEntryCount = normalized.showSearchEntryCount;
    InventoryState().g_showSearchQuantityCount = normalized.showSearchQuantityCount;
    InventoryState().g_showSearchClearButton = normalized.showSearchClearButton;
    InventoryState().g_autoFocusSearchInput = normalized.autoFocusSearchInput;
    InventoryState().g_followActiveInventory = normalized.followActiveInventory;
    InventoryState().g_debugLogging = normalized.debugLogging;
    InventoryState().g_debugSearchLogging = normalized.debugSearchLogging;
    InventoryState().g_debugBindingLogging = normalized.debugBindingLogging;
    InventoryState().g_enableDebugProbes = normalized.enableDebugProbes;
    InventoryState().g_searchBarConfiguredWidth = normalized.searchBarWidth;
    InventoryState().g_searchInputConfiguredWidth = normalized.searchInputWidth;
    InventoryState().g_searchInputConfiguredHeight = normalized.searchInputHeight;
    InventoryState().g_searchInputPositionCustomized = normalized.searchInputPositionCustomized;
    InventoryState().g_searchInputStoredLeft = normalized.searchInputLeft;
    InventoryState().g_searchInputStoredTop = normalized.searchInputTop;
}

bool SaveInventoryConfigSnapshot(const InventoryConfigSnapshot& config)
{
    InventoryConfigSnapshot normalized = config;
    NormalizeInventoryConfigSnapshot(&normalized);

    std::string configPath;
    if (!TryResolveModConfigPath(&configPath))
    {
        LogWarnLine("mod config save skipped: could not resolve plugin directory");
        return false;
    }

    if (!TryWriteTextFileAtomically(configPath, BuildInventoryConfigText(normalized)))
    {
        std::stringstream line;
        line << "mod config save failed path=" << configPath;
        LogWarnLine(line.str());
        return false;
    }

    return true;
}

void LoadInventoryConfig()
{
    ApplyInventoryConfigSnapshot(InventoryConfigSnapshot());

    std::string configPath;
    if (!TryResolveModConfigPath(&configPath))
    {
        LogWarnLine("mod config load skipped: could not resolve plugin directory (using defaults)");
        return;
    }

    std::string configText;
    if (!TryReadTextFile(configPath, &configText))
    {
        std::stringstream line;
        line << "mod config load skipped: could not read " << configPath
             << " (using defaults)";
        LogWarnLine(line.str());
        return;
    }

    InventoryConfigSnapshot config = CaptureInventoryConfigSnapshot();

    bool parsedValue = false;
    if (TryParseJsonBoolByKey(configText, "enabled", &parsedValue))
    {
        config.enabled = parsedValue;
    }
    if (TryParseJsonBoolByKey(configText, "showSearchEntryCount", &parsedValue))
    {
        config.showSearchEntryCount = parsedValue;
    }
    if (TryParseJsonBoolByKey(configText, "showSearchQuantityCount", &parsedValue))
    {
        config.showSearchQuantityCount = parsedValue;
    }
    if (TryParseJsonBoolByKey(configText, "showSearchClearButton", &parsedValue))
    {
        config.showSearchClearButton = parsedValue;
    }
    if (TryParseJsonBoolByKey(configText, "autoFocusSearchInput", &parsedValue))
    {
        config.autoFocusSearchInput = parsedValue;
    }
    if (TryParseJsonBoolByKey(configText, "followActiveInventory", &parsedValue))
    {
        config.followActiveInventory = parsedValue;
    }
    if (TryParseJsonBoolByKey(configText, "debugLogging", &parsedValue))
    {
        config.debugLogging = parsedValue;
    }
    if (TryParseJsonBoolByKey(configText, "debugSearchLogging", &parsedValue))
    {
        config.debugSearchLogging = parsedValue;
    }
    if (TryParseJsonBoolByKey(configText, "debugBindingLogging", &parsedValue))
    {
        config.debugBindingLogging = parsedValue;
    }
    if (TryParseJsonBoolByKey(configText, "enableDebugProbes", &parsedValue))
    {
        config.enableDebugProbes = parsedValue;
    }

    int parsedIntValue = 0;
    bool parsedSearchBarWidth = false;
    if (TryParseJsonIntByKey(configText, "searchBarWidth", &parsedIntValue))
    {
        config.searchBarWidth = parsedIntValue;
        parsedSearchBarWidth = true;
    }
    if (TryParseJsonIntByKey(configText, "searchInputWidth", &parsedIntValue))
    {
        config.searchInputWidth = parsedIntValue;
    }
    if (TryParseJsonIntByKey(configText, "searchInputHeight", &parsedIntValue))
    {
        config.searchInputHeight = parsedIntValue;
    }
    if (TryParseJsonBoolByKey(configText, "searchInputPositionCustomized", &parsedValue))
    {
        config.searchInputPositionCustomized = parsedValue;
    }
    if (TryParseJsonIntByKey(configText, "searchInputLeft", &parsedIntValue))
    {
        config.searchInputLeft = parsedIntValue;
    }
    if (TryParseJsonIntByKey(configText, "searchInputTop", &parsedIntValue))
    {
        config.searchInputTop = parsedIntValue;
    }

    if (!parsedSearchBarWidth)
    {
        config.searchBarWidth = ComputeDefaultSearchBarConfiguredWidth(config.searchInputWidth);
    }

    ApplyInventoryConfigSnapshot(config);
    LogInventoryConfigSnapshot("mod config loaded", CaptureInventoryConfigSnapshot());
}
