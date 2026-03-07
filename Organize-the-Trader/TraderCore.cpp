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
    return ClampIntValue(value, 120, 720);
}

int ClampSearchInputConfiguredHeight(int value)
{
    return ClampIntValue(value, 22, 48);
}

void LoadModConfig()
{
    TraderState().core.g_controlsEnabled = true;
    TraderState().core.g_showSearchEntryCount = true;
    TraderState().core.g_showSearchQuantityCount = true;
    TraderState().core.g_debugLogging = false;
    TraderState().core.g_debugSearchLogging = false;
    TraderState().core.g_debugBindingLogging = false;
    TraderState().core.g_searchInputConfiguredWidth = 372;
    TraderState().core.g_searchInputConfiguredHeight = 26;

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
        TraderState().core.g_controlsEnabled = parsedValue;
    }
    if (TryParseJsonBoolByKey(configText, "showSearchEntryCount", &parsedValue))
    {
        TraderState().core.g_showSearchEntryCount = parsedValue;
    }
    if (TryParseJsonBoolByKey(configText, "showSearchQuantityCount", &parsedValue))
    {
        TraderState().core.g_showSearchQuantityCount = parsedValue;
    }
    if (TryParseJsonBoolByKey(configText, "debugLogging", &parsedValue))
    {
        TraderState().core.g_debugLogging = parsedValue;
    }
    if (TryParseJsonBoolByKey(configText, "debugSearchLogging", &parsedValue))
    {
        TraderState().core.g_debugSearchLogging = parsedValue;
    }
    if (TryParseJsonBoolByKey(configText, "debugBindingLogging", &parsedValue))
    {
        TraderState().core.g_debugBindingLogging = parsedValue;
    }

    int parsedIntValue = 0;
    if (TryParseJsonIntByKey(configText, "searchInputWidth", &parsedIntValue))
    {
        TraderState().core.g_searchInputConfiguredWidth =
            ClampSearchInputConfiguredWidth(parsedIntValue);
    }
    if (TryParseJsonIntByKey(configText, "searchInputHeight", &parsedIntValue))
    {
        TraderState().core.g_searchInputConfiguredHeight =
            ClampSearchInputConfiguredHeight(parsedIntValue);
    }

    std::stringstream line;
    line << "mod config loaded"
         << " enabled=" << (TraderState().core.g_controlsEnabled ? "true" : "false")
         << " showSearchEntryCount="
         << (TraderState().core.g_showSearchEntryCount ? "true" : "false")
         << " showSearchQuantityCount="
         << (TraderState().core.g_showSearchQuantityCount ? "true" : "false")
         << " debugLogging=" << (TraderState().core.g_debugLogging ? "true" : "false")
         << " debugSearchLogging="
         << (TraderState().core.g_debugSearchLogging ? "true" : "false")
         << " debugBindingLogging="
         << (TraderState().core.g_debugBindingLogging ? "true" : "false")
         << " searchInputWidth=" << TraderState().core.g_searchInputConfiguredWidth
         << " searchInputHeight=" << TraderState().core.g_searchInputConfiguredHeight
         << " verboseDiagnosticsCompiled="
         << (ShouldCompileVerboseDiagnostics() ? "true" : "false");
    LogInfoLine(line.str());
}
