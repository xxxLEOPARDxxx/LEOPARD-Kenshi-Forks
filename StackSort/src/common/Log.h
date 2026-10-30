#pragma once

#include <Debug.h> // KenshiLib: DebugLog, ErrorLog
#include <Windows.h>

#include <string>

#include "StrUtil.h"

enum LogLevel
{
    LOG_LEVEL_ERROR   = 0,
    LOG_LEVEL_INFO    = 1,
    LOG_LEVEL_VERBOSE = 2
};

// Header-only mutable state: a function-local static inside an inline function
// has exactly one instance across the DLL, so no .cpp is needed.
//
// MCM can toggle details live while workers run. Interlocked reads/writes keep
// the runtime level shared without changing immutable solver settings.
inline volatile LONG& LogLevelRef()
{
    static volatile LONG level = LOG_LEVEL_ERROR;
    return level;
}

inline void SetLogLevel(int level)
{
    InterlockedExchange(&LogLevelRef(), level);
}

inline int GetLogLevel()
{
    return (int)InterlockedCompareExchange(&LogLevelRef(), 0, 0);
}

// Main thread only: KenshiLib's DebugLog/ErrorLog append to a global stringstream
// and ofstream with no lock, and RE_Kenshi reads the stringstream every frame.
// Worker threads log through SortWorker.h's WorkerLog* macros instead.

// Error: always emitted, whatever the level.
inline void LogError(const std::string& m)
{
    ErrorLog(m);
}
inline void LogError(const char* m)
{
    ErrorLog(m);
}

inline void LogInfo(const std::string& m)
{
    if (GetLogLevel() >= LOG_LEVEL_INFO) DebugLog(m);
}
inline void LogInfo(const char* m)
{
    if (GetLogLevel() >= LOG_LEVEL_INFO) DebugLog(m);
}

// Verbose: runtime-gated. Macro so call-site string construction
// (e.g. "foo" + IntToStr(bar)) is skipped below LOG_LEVEL_VERBOSE.
#define LogVerbose(msg)                                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        if (GetLogLevel() >= LOG_LEVEL_VERBOSE) DebugLog(msg);                                                         \
    } while (0)

// Debug: compiled out in prod. Macro so call-site string construction
// is elided entirely.
#ifdef STACKSORT_VERBOSE
#define LogDebug(msg) DebugLog(msg)
#else
#define LogDebug(msg) ((void)0)
#endif
