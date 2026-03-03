#include "vs_log.h"

#include <Debug.h>

#include <sstream>

namespace vs_log
{

void LogWithPrefix(const char* pluginName, void (*sink)(const char*), const char* level, const std::string& message)
{
    if (!sink)
    {
        return;
    }

    std::stringstream line;
    line << pluginName << " " << level << ": " << message;
    sink(line.str().c_str());
}

void LogInfo(const char* pluginName, const std::string& message)
{
    LogWithPrefix(pluginName, &DebugLog, "INFO", message);
}

void LogWarn(const char* pluginName, const std::string& message)
{
    LogWithPrefix(pluginName, &ErrorLog, "WARN", message);
}

void LogError(const char* pluginName, const std::string& message)
{
    LogWithPrefix(pluginName, &ErrorLog, "ERROR", message);
}

} // namespace vs_log
