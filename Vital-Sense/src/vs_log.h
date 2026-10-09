#pragma once

#include <string>

namespace vs_log
{

void LogWithPrefix(const char* pluginName, void (*sink)(const char*), const char* level, const std::string& message);
void LogInfo(const char* pluginName, const std::string& message);
void LogWarn(const char* pluginName, const std::string& message);
void LogError(const char* pluginName, const std::string& message);

} // namespace vs_log
