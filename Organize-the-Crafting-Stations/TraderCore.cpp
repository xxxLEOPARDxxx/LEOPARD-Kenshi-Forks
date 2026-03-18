#include "TraderCore.h"

#include <Debug.h>

#include <Windows.h>

#include <cctype>
#include <fstream>
#include <sstream>

namespace
{
const char* kPluginName = "Organize-the-Trader";

TraderRuntimeState g_traderRuntimeState;

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

std::string BuildTraderConfigText(const TraderConfigSnapshot& config)
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
            << "  \"debugLogging\": " << (config.debugLogging ? "true" : "false") << ",\n"
            << "  \"debugSearchLogging\": "
            << (config.debugSearchLogging ? "true" : "false") << ",\n"
            << "  \"debugBindingLogging\": "
            << (config.debugBindingLogging ? "true" : "false") << ",\n"
            << "  \"searchInputWidth\": " << config.searchInputWidth << ",\n"
            << "  \"searchInputHeight\": " << config.searchInputHeight << ",\n"
            << "  \"searchInputPositionCustomized\": "
            << (config.searchInputPositionCustomized ? "true" : "false") << ",\n"
            << "  \"searchInputLeft\": " << config.searchInputLeft << ",\n"
            << "  \"searchInputTop\": " << config.searchInputTop << "\n"
            << "}\n";
    return content.str();
}

void LogTraderConfigSnapshot(const char* prefix, const TraderConfigSnapshot& config)
{
    std::stringstream line;
    line << prefix
         << " enabled=" << (config.enabled ? "true" : "false")
         << " showSearchEntryCount=" << (config.showSearchEntryCount ? "true" : "false")
         << " showSearchQuantityCount="
         << (config.showSearchQuantityCount ? "true" : "false")
         << " showSearchClearButton="
         << (config.showSearchClearButton ? "true" : "false")
         << " debugLogging=" << (config.debugLogging ? "true" : "false")
         << " debugSearchLogging=" << (config.debugSearchLogging ? "true" : "false")
         << " debugBindingLogging=" << (config.debugBindingLogging ? "true" : "false")
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

TraderRuntimeState& TraderState()
{
    return g_traderRuntimeState;
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

bool ShouldCompileVerboseDiagnostics()
{
#if defined(OTT_ENABLE_VERBOSE_DIAGNOSTICS)
    return true;
#else
    return false;
#endif
}

bool ShouldLogDebug()
{
    return TraderState().core.g_debugLogging;
}

bool ShouldLogSearchDebug()
{
    return TraderState().core.g_debugLogging && TraderState().core.g_debugSearchLogging;
}

bool ShouldLogBindingDebug()
{
    return TraderState().core.g_debugLogging && TraderState().core.g_debugBindingLogging;
}

bool ShouldLogVerboseSearchDiagnostics()
{
    return ShouldCompileVerboseDiagnostics() && ShouldLogSearchDebug();
}

bool ShouldLogVerboseBindingDiagnostics()
{
    return ShouldCompileVerboseDiagnostics() && ShouldLogBindingDebug();
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

int ClampSearchInputConfiguredWidth(int value)
{
    return ClampIntValue(value, kSearchInputConfiguredWidthMin, kSearchInputConfiguredWidthMax);
}

int ClampSearchInputConfiguredHeight(int value)
{
    return ClampIntValue(value, kSearchInputConfiguredHeightMin, kSearchInputConfiguredHeightMax);
}

void NormalizeTraderConfigSnapshot(TraderConfigSnapshot* config)
{
    if (config == 0)
    {
        return;
    }

    config->searchInputWidth = ClampSearchInputConfiguredWidth(config->searchInputWidth);
    config->searchInputHeight = ClampSearchInputConfiguredHeight(config->searchInputHeight);
    if (!config->searchInputPositionCustomized)
    {
        config->searchInputLeft = 0;
        config->searchInputTop = 0;
    }
}

TraderConfigSnapshot CaptureTraderConfigSnapshot()
{
    TraderConfigSnapshot config;
    config.enabled = TraderState().core.g_controlsEnabled;
    config.showSearchEntryCount = TraderState().core.g_showSearchEntryCount;
    config.showSearchQuantityCount = TraderState().core.g_showSearchQuantityCount;
    config.showSearchClearButton = TraderState().core.g_showSearchClearButton;
    config.debugLogging = TraderState().core.g_debugLogging;
    config.debugSearchLogging = TraderState().core.g_debugSearchLogging;
    config.debugBindingLogging = TraderState().core.g_debugBindingLogging;
    config.searchInputWidth = TraderState().core.g_searchInputConfiguredWidth;
    config.searchInputHeight = TraderState().core.g_searchInputConfiguredHeight;
    config.searchInputPositionCustomized = TraderState().searchUi.g_searchContainerPositionCustomized;
    config.searchInputLeft = TraderState().searchUi.g_searchContainerStoredLeft;
    config.searchInputTop = TraderState().searchUi.g_searchContainerStoredTop;
    NormalizeTraderConfigSnapshot(&config);
    return config;
}

void ApplyTraderConfigSnapshot(const TraderConfigSnapshot& config)
{
    TraderConfigSnapshot normalized = config;
    NormalizeTraderConfigSnapshot(&normalized);

    TraderState().core.g_controlsEnabled = normalized.enabled;
    TraderState().core.g_showSearchEntryCount = normalized.showSearchEntryCount;
    TraderState().core.g_showSearchQuantityCount = normalized.showSearchQuantityCount;
    TraderState().core.g_showSearchClearButton = normalized.showSearchClearButton;
    TraderState().core.g_debugLogging = normalized.debugLogging;
    TraderState().core.g_debugSearchLogging = normalized.debugSearchLogging;
    TraderState().core.g_debugBindingLogging = normalized.debugBindingLogging;
    TraderState().core.g_searchInputConfiguredWidth = normalized.searchInputWidth;
    TraderState().core.g_searchInputConfiguredHeight = normalized.searchInputHeight;
    TraderState().searchUi.g_searchContainerPositionCustomized =
        normalized.searchInputPositionCustomized;
    TraderState().searchUi.g_searchContainerStoredLeft = normalized.searchInputLeft;
    TraderState().searchUi.g_searchContainerStoredTop = normalized.searchInputTop;
}

bool SaveTraderConfigSnapshot(const TraderConfigSnapshot& config)
{
    TraderConfigSnapshot normalized = config;
    NormalizeTraderConfigSnapshot(&normalized);

    std::string configPath;
    if (!TryResolveModConfigPath(&configPath))
    {
        LogErrorLine("mod config save failed: could not resolve plugin directory");
        return false;
    }

    if (!TryWriteTextFileAtomically(configPath, BuildTraderConfigText(normalized)))
    {
        std::stringstream line;
        line << "mod config save failed: could not write " << configPath;
        LogErrorLine(line.str());
        return false;
    }

    if (ShouldLogDebug())
    {
        LogTraderConfigSnapshot("mod config saved", normalized);
    }

    return true;
}

void LoadModConfig()
{
    ApplyTraderConfigSnapshot(TraderConfigSnapshot());

    std::string configPath;
    if (!TryResolveModConfigPath(&configPath))
    {
        LogWarnLine("mod config load skipped: could not resolve plugin directory");
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

    TraderConfigSnapshot config = CaptureTraderConfigSnapshot();

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

    int parsedIntValue = 0;
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

    ApplyTraderConfigSnapshot(config);
    LogTraderConfigSnapshot("mod config loaded", CaptureTraderConfigSnapshot());
}
