#include "logging.h"

#include <Debug.h>

#include <Windows.h>

#include <cctype>
#include <fstream>
#include <sstream>

namespace
{
const char* kPluginName = "Emkejs-Mod-Core";
const char* kModConfigFileName = "mod-config.json";
bool g_debug_logging = false;
bool g_debug_search_logging = false;
bool g_debug_binding_logging = false;

bool TryResolveModConfigPath(std::string* out_path)
{
    if (out_path == 0)
    {
        return false;
    }

    HMODULE module = 0;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&TryResolveModConfigPath),
            &module))
    {
        return false;
    }

    char module_path[MAX_PATH] = {};
    const DWORD module_path_len = GetModuleFileNameA(module, module_path, MAX_PATH);
    if (module_path_len == 0 || module_path_len >= MAX_PATH)
    {
        return false;
    }

    std::string path(module_path, module_path_len);
    const size_t separator = path.find_last_of("\\/");
    if (separator == std::string::npos)
    {
        path = kModConfigFileName;
    }
    else
    {
        path.erase(separator + 1u);
        path += kModConfigFileName;
    }

    *out_path = path;
    return true;
}

bool TryReadTextFile(const std::string& path, std::string* out_content)
{
    if (out_content == 0)
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

    *out_content = buffer.str();
    return true;
}

bool TryParseJsonBoolByKey(const std::string& content, const char* key, bool* out_value)
{
    if (key == 0 || out_value == 0)
    {
        return false;
    }

    const std::string needle = std::string("\"") + key + "\"";
    const std::string::size_type key_pos = content.find(needle);
    if (key_pos == std::string::npos)
    {
        return false;
    }

    std::string::size_type value_pos = content.find(':', key_pos + needle.size());
    if (value_pos == std::string::npos)
    {
        return false;
    }

    ++value_pos;
    while (value_pos < content.size()
        && std::isspace(static_cast<unsigned char>(content[value_pos])) != 0)
    {
        ++value_pos;
    }

    if (content.compare(value_pos, 4, "true") == 0)
    {
        *out_value = true;
        return true;
    }

    if (content.compare(value_pos, 5, "false") == 0)
    {
        *out_value = false;
        return true;
    }

    return false;
}
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
    return g_debug_logging;
}

bool ShouldLogSearchDebug()
{
    return g_debug_logging && g_debug_search_logging;
}

bool ShouldLogBindingDebug()
{
    return g_debug_logging && g_debug_binding_logging;
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

void LoadLoggingConfig()
{
    g_debug_logging = false;
    g_debug_search_logging = false;
    g_debug_binding_logging = false;

    std::string config_path;
    if (!TryResolveModConfigPath(&config_path))
    {
        LogWarnLine("mod config load skipped: could not resolve plugin directory (using quiet logging defaults)");
        return;
    }

    std::string config_text;
    if (!TryReadTextFile(config_path, &config_text))
    {
        std::stringstream line;
        line << "mod config load skipped: could not read " << config_path
             << " (using quiet logging defaults)";
        LogWarnLine(line.str());
        return;
    }

    bool parsed_value = false;
    if (TryParseJsonBoolByKey(config_text, "debugLogging", &parsed_value))
    {
        g_debug_logging = parsed_value;
    }
    if (TryParseJsonBoolByKey(config_text, "debugSearchLogging", &parsed_value))
    {
        g_debug_search_logging = parsed_value;
    }
    if (TryParseJsonBoolByKey(config_text, "debugBindingLogging", &parsed_value))
    {
        g_debug_binding_logging = parsed_value;
    }

    LogInfoLine("mod config loaded");
}
