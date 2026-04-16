#include "InventoryCore.h"

#include <Debug.h>

#include <sstream>

namespace
{
const char* kPluginName = "Organize-the-Inventory";

InventoryRuntimeState g_inventoryRuntimeState;
}

InventoryRuntimeState& InventoryState()
{
    return g_inventoryRuntimeState;
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
    return InventoryState().g_debugLogging;
}

bool ShouldLogSearchDebug()
{
    return InventoryState().g_debugLogging && InventoryState().g_debugSearchLogging;
}

bool ShouldLogBindingDebug()
{
    return InventoryState().g_debugLogging && InventoryState().g_debugBindingLogging;
}

bool ShouldEnableDebugProbes()
{
    return InventoryState().g_enableDebugProbes;
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
