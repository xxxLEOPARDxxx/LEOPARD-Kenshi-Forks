#include <Debug.h>

#include <core/Functions.h>
#include <kenshi/Kenshi.h>

#include <Windows.h>

#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
const char* kPluginName = "Organize-the-Inventory";
std::string g_configPath;
bool g_debugLogging = false;
bool g_debugSearchLogging = false;
bool g_debugBindingLogging = false;

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
        line << "logging flags debugLogging=" << (g_debugLogging ? "true" : "false")
             << " debugSearchLogging=" << (g_debugSearchLogging ? "true" : "false")
             << " debugBindingLogging=" << (g_debugBindingLogging ? "true" : "false")
             << " verboseDiagnostics=" << (ShouldCompileVerboseDiagnostics() ? "true" : "false");
        LogDebugLine(line.str());
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

    std::stringstream versionLine;
    versionLine << "supported Kenshi version detected: " << versionInfo.GetVersion();
    LogInfoLine(versionLine.str());

    LoadLoggingConfig();

    LogDebugLine("runtime debug logging is enabled");
    LogInfoLine("base plugin initialized");
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
