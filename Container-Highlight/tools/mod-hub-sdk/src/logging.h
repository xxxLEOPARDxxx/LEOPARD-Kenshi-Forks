#pragma once

#include <string>

void LoadLoggingConfig();

void LogInfoLine(const std::string& message);
void LogWarnLine(const std::string& message);
void LogErrorLine(const std::string& message);

bool ShouldCompileVerboseDiagnostics();
bool ShouldLogDebug();
bool ShouldLogSearchDebug();
bool ShouldLogBindingDebug();

void LogDebugLine(const std::string& message);
void LogSearchDebugLine(const std::string& message);
void LogBindingDebugLine(const std::string& message);
