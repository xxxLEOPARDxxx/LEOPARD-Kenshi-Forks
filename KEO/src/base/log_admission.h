#pragma once
#include <string>
// Producer admission: the argument is never evaluated while Debug is off.
// LogMsg and error/fatal/retirement sinks remain unconditional.
bool DetailedLogEnabled();
void LogMsg(const std::string& line);
void LogMsgDeferrable(const char* line);
void LogError(const std::string& line);
void LogStatus(const std::string& line); // mixed config/benchmark callback severity
void LogErrorDeferrable(const char* line);
#define KEO_DETAIL(line) do { if (DetailedLogEnabled()) LogMsg(line); } while (0)
#define KEO_DETAIL_DEFERRED(line) do { if (DetailedLogEnabled()) LogMsgDeferrable(line); } while (0)
