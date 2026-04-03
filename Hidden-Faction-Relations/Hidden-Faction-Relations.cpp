#include "src/HiddenFactionRelations.h"
#include "src/HiddenFactionRelationsConfig.h"
#include "src/HiddenFactionRelationsModHub.h"
#include "src/HiddenFactionRelationsPanel.h"

#include <Debug.h>

#include <core/Functions.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Kenshi.h>

#include <Windows.h>

#include <sstream>
#include <string>

namespace
{
const char* kPluginName = "Hidden-Faction-Relations";
bool g_hiddenFactionDumpCompleted = false;
bool g_hiddenFactionDumpHookInstalled = false;
void (*g_gameWorldMainLoopOrig)(GameWorld* thisptr, float time) = 0;

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
    return HiddenFactionRelationsConfig_IsDebugLoggingEnabled();
}

bool ShouldLogSearchDebug()
{
    return HiddenFactionRelationsConfig_IsDebugSearchLoggingEnabled();
}

bool ShouldLogBindingDebug()
{
    return HiddenFactionRelationsConfig_IsDebugBindingLoggingEnabled();
}

void LogDebugLine(const std::string& message)
{
    if (ShouldLogDebug())
    {
        LogInfoLine(message);
    }
}

void LogHiddenFactionRelationsDump(const HiddenFactionRelationsSnapshot& snapshot)
{
    std::stringstream startLine;
    startLine << "hidden faction relations dump begin totalFactions=" << snapshot.totalFactions
              << " playerFactionId=" << snapshot.playerFactionId
              << " playerFactionName=" << snapshot.playerFactionName;
    LogInfoLine(startLine.str());

    for (size_t i = 0; i < snapshot.factions.size(); ++i)
    {
        const HiddenFactionRelationEntry& entry = snapshot.factions[i];
        std::stringstream line;
        line << "faction_dump index=" << entry.index;

        if (entry.isNullEntry)
        {
            line << " entry=null";
            LogWarnLine(line.str());
            continue;
        }

        line << " id=" << entry.factionId
             << " name=" << entry.factionName
             << " hidden=" << HiddenFactionRelationsConfig_BoolToString(entry.isHidden)
             << " isPlayer=" << HiddenFactionRelationsConfig_BoolToString(entry.isPlayerFaction);

        if (entry.hasPlayerRelation)
        {
            line << " playerRelation=" << entry.playerRelation;
        }
        else
        {
            line << " playerRelation=n/a relationState=missing";
        }

        LogInfoLine(line.str());
    }

    std::stringstream summaryLine;
    summaryLine << "hidden faction relations dump end"
                << " totalFactions=" << snapshot.totalFactions
                << " hiddenFactions=" << snapshot.hiddenFactions
                << " visibleFactions=" << snapshot.visibleFactions
                << " nullEntries=" << snapshot.nullEntries
                << " missingRelationEntries=" << snapshot.missingRelationEntries;
    LogInfoLine(summaryLine.str());
}

void TryRunHiddenFactionDump(GameWorld* gameWorld)
{
    HiddenFactionRelationsSnapshot snapshot;
    if (!HiddenFactionRelations_TryCollectSnapshot(gameWorld, &snapshot))
    {
        return;
    }

    LogHiddenFactionRelationsDump(snapshot);
    g_hiddenFactionDumpCompleted = true;
}

void GameWorldMainLoopHook(GameWorld* thisptr, float time)
{
    if (!g_hiddenFactionDumpCompleted && HiddenFactionRelationsConfig_ShouldDumpHiddenFactionRelations())
    {
        TryRunHiddenFactionDump(thisptr);
    }

    g_gameWorldMainLoopOrig(thisptr, time);
}

void InstallHiddenFactionDumpHook()
{
    if (!HiddenFactionRelationsConfig_ShouldDumpHiddenFactionRelations() || g_hiddenFactionDumpHookInstalled)
    {
        return;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            KenshiLib::GetRealAddress(&GameWorld::_NV_mainLoop_GPUSensitiveStuff),
            &GameWorldMainLoopHook,
            &g_gameWorldMainLoopOrig))
    {
        LogErrorLine("failed to install hidden faction relations dump hook");
        return;
    }

    g_hiddenFactionDumpHookInstalled = true;
    LogInfoLine("hidden faction relations dump hook installed");
}

void LoadLoggingConfig()
{
    std::string loadError;
    if (!HiddenFactionRelationsConfig_Load(&loadError))
    {
        std::stringstream line;
        line << "mod config load skipped: " << loadError
             << " (using quiet logging defaults)";
        LogWarnLine(line.str());
        return;
    }

    LogInfoLine("mod config loaded");

    if (ShouldLogDebug())
    {
        const HiddenFactionRelationsConfigSnapshot config = HiddenFactionRelationsConfig_Capture();
        std::stringstream line;
        line << "logging flags debugLogging=" << HiddenFactionRelationsConfig_BoolToString(config.debugLogging)
             << " debugSearchLogging=" << HiddenFactionRelationsConfig_BoolToString(config.debugSearchLogging)
             << " debugBindingLogging=" << HiddenFactionRelationsConfig_BoolToString(config.debugBindingLogging)
             << " dumpHiddenFactionRelations=" << HiddenFactionRelationsConfig_BoolToString(config.dumpHiddenFactionRelations)
             << " autoFocusSearchOnOpen=" << HiddenFactionRelationsConfig_BoolToString(config.autoFocusSearchOnOpen)
             << " verboseDiagnostics=" << HiddenFactionRelationsConfig_BoolToString(ShouldCompileVerboseDiagnostics());
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
    InstallHiddenFactionDumpHook();
    HiddenFactionRelationsModHub_OnStartup();

    const uintptr_t baseAddress = reinterpret_cast<uintptr_t>(GetModuleHandleA(0));
    if (!HiddenFactionRelationsPanel_Initialize(versionInfo.GetPlatform(), versionInfo.GetVersion(), baseAddress))
    {
        LogWarnLine("hidden faction panel initialization failed");
    }

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
                HiddenFactionRelationsConfig_SetConfigPath(fullPath.substr(0, sep) + "\\mod-config.json");
            }
        }
    }

    return TRUE;
}
