#include <Debug.h>

#include <core/Functions.h>

#include <kenshi/Kenshi.h>
#include <kenshi/PlayerInterface.h>

#include <Windows.h>

#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

struct PluginConfig
{
    bool enabled;
    bool pauseOnSaveLoad;
    DWORD pauseDebounceMs;
    bool debugLogTransitions;
};

struct RuntimeState
{
    bool loadInProgress;
    bool pauseArmed;
    DWORD loadStartedMs;
    DWORD lastPauseMs;
    DWORD lastTickAliveLogMs;
    bool loggedMissingLoadSignal;
    bool loggedMissingPauseApi;
};

typedef bool (*FnIsLoadingSave)();
typedef void (*FnSetPaused)(bool);

static const char* kPluginName = "Auto-Pause-on-Load";
static const char* kConfigFileName = "mod-config.json";

static PluginConfig g_config = { true, true, 2000, false };
static RuntimeState g_state = { false, false, 0, 0, 0, false, false };

static FnIsLoadingSave g_fnIsLoadingSave = 0;
static FnSetPaused g_fnSetPaused = 0;

static std::string g_settingsPath;

static void (*PlayerInterface_updateUT_orig)(PlayerInterface*) = 0;

static std::string TrimAscii(const std::string& value)
{
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
    {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }

    return value.substr(start, end - start);
}

static bool TryFindJsonValueStart(const std::string& body, const char* key, size_t* valueStartOut)
{
    if (!key || !valueStartOut)
    {
        return false;
    }

    const std::string quotedKey = std::string("\"") + key + "\"";
    const size_t keyPos = body.find(quotedKey);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    const size_t colonPos = body.find(':', keyPos + quotedKey.size());
    if (colonPos == std::string::npos)
    {
        return false;
    }

    size_t valuePos = colonPos + 1;
    while (valuePos < body.size() && std::isspace(static_cast<unsigned char>(body[valuePos])) != 0)
    {
        ++valuePos;
    }

    if (valuePos >= body.size())
    {
        return false;
    }

    *valueStartOut = valuePos;
    return true;
}

static bool TryExtractJsonBool(const std::string& body, const char* key, bool* valueOut)
{
    if (!valueOut)
    {
        return false;
    }

    size_t valuePos = 0;
    if (!TryFindJsonValueStart(body, key, &valuePos))
    {
        return false;
    }

    if (body.compare(valuePos, 4, "true") == 0)
    {
        *valueOut = true;
        return true;
    }

    if (body.compare(valuePos, 5, "false") == 0)
    {
        *valueOut = false;
        return true;
    }

    return false;
}

static bool TryExtractJsonUnsigned(const std::string& body, const char* key, DWORD* valueOut)
{
    if (!valueOut)
    {
        return false;
    }

    size_t valuePos = 0;
    if (!TryFindJsonValueStart(body, key, &valuePos))
    {
        return false;
    }

    size_t endPos = valuePos;
    while (endPos < body.size() && std::isdigit(static_cast<unsigned char>(body[endPos])) != 0)
    {
        ++endPos;
    }

    if (endPos == valuePos)
    {
        return false;
    }

    const std::string numberText = body.substr(valuePos, endPos - valuePos);
    unsigned long parsed = 0;
    try
    {
        parsed = std::stoul(numberText);
    }
    catch (...)
    {
        return false;
    }

    if (parsed > 600000UL)
    {
        parsed = 600000UL;
    }

    *valueOut = static_cast<DWORD>(parsed);
    return true;
}

static bool ReadConfigFromFile(const std::string& configPath, PluginConfig* configOut)
{
    if (!configOut)
    {
        return false;
    }

    std::ifstream in(configPath.c_str(), std::ios::in | std::ios::binary);
    if (!in)
    {
        return true;
    }

    const std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    bool boolValue = false;
    DWORD unsignedValue = 0;

    if (TryExtractJsonBool(body, "enabled", &boolValue))
    {
        configOut->enabled = boolValue;
    }

    if (TryExtractJsonBool(body, "pause_on_save_load", &boolValue))
    {
        configOut->pauseOnSaveLoad = boolValue;
    }

    if (TryExtractJsonUnsigned(body, "pause_debounce_ms", &unsignedValue))
    {
        configOut->pauseDebounceMs = unsignedValue;
    }

    if (TryExtractJsonBool(body, "debug_log_transitions", &boolValue))
    {
        configOut->debugLogTransitions = boolValue;
    }

    return true;
}

static bool SaveConfigToFile(const std::string& configPath, const PluginConfig& config)
{
    std::ofstream out(configPath.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out)
    {
        return false;
    }

    out << "{\n";
    out << "  \"enabled\": " << (config.enabled ? "true" : "false") << ",\n";
    out << "  \"pause_on_save_load\": " << (config.pauseOnSaveLoad ? "true" : "false") << ",\n";
    out << "  \"pause_debounce_ms\": " << config.pauseDebounceMs << ",\n";
    out << "  \"debug_log_transitions\": " << (config.debugLogTransitions ? "true" : "false") << "\n";
    out << "}\n";

    return true;
}

static void LoadConfigState()
{
    g_config.enabled = true;
    g_config.pauseOnSaveLoad = true;
    g_config.pauseDebounceMs = 2000;
    g_config.debugLogTransitions = false;

    if (g_settingsPath.empty())
    {
        return;
    }

    if (!ReadConfigFromFile(g_settingsPath, &g_config))
    {
        ErrorLog("Auto-Pause-on-Load ERROR: failed to read mod-config.json; using defaults");
        return;
    }

    std::stringstream info;
    info << "Auto-Pause-on-Load INFO: loaded config enabled=" << (g_config.enabled ? "true" : "false")
         << " pause_on_save_load=" << (g_config.pauseOnSaveLoad ? "true" : "false")
         << " pause_debounce_ms=" << g_config.pauseDebounceMs
         << " debug_log_transitions=" << (g_config.debugLogTransitions ? "true" : "false");
    DebugLog(info.str().c_str());
}

static bool SaveConfigState()
{
    if (g_settingsPath.empty())
    {
        ErrorLog("Auto-Pause-on-Load ERROR: settings path is empty; cannot save mod-config.json");
        return false;
    }

    if (!SaveConfigToFile(g_settingsPath, g_config))
    {
        ErrorLog("Auto-Pause-on-Load ERROR: failed to save mod-config.json");
        return false;
    }

    return true;
}

static bool InitPauseOnLoadFunctions(unsigned int platform, const std::string& version, uintptr_t baseAddr)
{
    (void)platform;
    (void)version;
    (void)baseAddr;

    // Scaffold only: wire these once save-load signal and pause setter offsets are verified.
    // Example shape expected:
    // g_fnIsLoadingSave = reinterpret_cast<FnIsLoadingSave>(baseAddr + 0xDEADBEEF);
    // g_fnSetPaused = reinterpret_cast<FnSetPaused>(baseAddr + 0xDEADBEEF);
    g_fnIsLoadingSave = 0;
    g_fnSetPaused = 0;

    return true;
}

static bool QuerySaveLoadSignal(bool* availableOut, bool* isLoadingOut)
{
    if (!availableOut || !isLoadingOut)
    {
        return false;
    }

    *availableOut = false;
    *isLoadingOut = false;

    if (!g_fnIsLoadingSave)
    {
        return true;
    }

    *isLoadingOut = g_fnIsLoadingSave();
    *availableOut = true;
    return true;
}

static bool ForcePauseTrue()
{
    if (!g_fnSetPaused)
    {
        if (!g_state.loggedMissingPauseApi)
        {
            ErrorLog("Auto-Pause-on-Load WARN: pause setter not wired yet; auto-pause scaffold is idle");
            g_state.loggedMissingPauseApi = true;
        }
        return false;
    }

    g_fnSetPaused(true);
    g_state.loggedMissingPauseApi = false;
    return true;
}

static bool DebounceWindowElapsed(DWORD nowMs, DWORD lastEventMs, DWORD minGapMs)
{
    const DWORD elapsed = nowMs - lastEventMs;
    return elapsed >= minGapMs;
}

static void ResetLifecycleState()
{
    g_state.loadInProgress = false;
    g_state.pauseArmed = false;
    g_state.loadStartedMs = 0;
}

static void OnLoadStarted(DWORD nowMs)
{
    g_state.loadInProgress = true;
    g_state.pauseArmed = true;
    g_state.loadStartedMs = nowMs;

    if (g_config.debugLogTransitions)
    {
        DebugLog("Auto-Pause-on-Load DEBUG: load started; pause armed");
    }
}

static void OnLoadFinished(DWORD nowMs)
{
    if (g_config.debugLogTransitions)
    {
        DebugLog("Auto-Pause-on-Load DEBUG: load finished");
    }

    if (!g_state.pauseArmed)
    {
        ResetLifecycleState();
        return;
    }

    if (!DebounceWindowElapsed(nowMs, g_state.lastPauseMs, g_config.pauseDebounceMs))
    {
        if (g_config.debugLogTransitions)
        {
            DebugLog("Auto-Pause-on-Load DEBUG: pause skipped (debounce)");
        }
        ResetLifecycleState();
        return;
    }

    if (ForcePauseTrue())
    {
        g_state.lastPauseMs = nowMs;
        DebugLog("Auto-Pause-on-Load INFO: paused_after_load=true");
    }

    ResetLifecycleState();
}

static void TickPauseOnLoad()
{
    if (!g_config.enabled || !g_config.pauseOnSaveLoad)
    {
        return;
    }

    const DWORD nowMs = GetTickCount();

    if (g_config.debugLogTransitions)
    {
        if (g_state.lastTickAliveLogMs == 0 || DebounceWindowElapsed(nowMs, g_state.lastTickAliveLogMs, 5000))
        {
            DebugLog("Auto-Pause-on-Load DEBUG: tick alive");
            g_state.lastTickAliveLogMs = nowMs;
        }
    }

    bool signalAvailable = false;
    bool isLoadingSave = false;
    if (!QuerySaveLoadSignal(&signalAvailable, &isLoadingSave))
    {
        ErrorLog("Auto-Pause-on-Load WARN: load signal query failed");
        return;
    }

    if (!signalAvailable)
    {
        if (!g_state.loggedMissingLoadSignal)
        {
            ErrorLog("Auto-Pause-on-Load WARN: load detection not wired yet; auto-pause scaffold is idle");
            g_state.loggedMissingLoadSignal = true;
        }
        return;
    }

    g_state.loggedMissingLoadSignal = false;

    if (!g_state.loadInProgress && isLoadingSave)
    {
        OnLoadStarted(nowMs);
        return;
    }

    if (g_state.loadInProgress && !isLoadingSave)
    {
        OnLoadFinished(nowMs);
    }
}

static void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    PlayerInterface_updateUT_orig(thisptr);
    TickPauseOnLoad();
}

__declspec(dllexport) void startPlugin()
{
    DebugLog("Auto-Pause-on-Load: startPlugin()");

    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    if (platform == KenshiLib::BinaryVersion::UNKNOWN || (version != "1.0.65" && version != "1.0.68"))
    {
        ErrorLog("Auto-Pause-on-Load: unsupported Kenshi version/platform");
        return;
    }

    const uintptr_t baseAddr = reinterpret_cast<uintptr_t>(GetModuleHandleA(0));
    if (!InitPauseOnLoadFunctions(platform, version, baseAddr))
    {
        ErrorLog("Auto-Pause-on-Load: failed to initialize runtime functions");
        return;
    }

    LoadConfigState();
    SaveConfigState();

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&PlayerInterface::updateUT),
        PlayerInterface_updateUT_hook,
        &PlayerInterface_updateUT_orig))
    {
        ErrorLog("Auto-Pause-on-Load: Could not hook PlayerInterface::updateUT");
        return;
    }

    std::stringstream info;
    info << "Auto-Pause-on-Load INFO: initialized (enabled=" << (g_config.enabled ? "true" : "false")
         << ", pause_on_save_load=" << (g_config.pauseOnSaveLoad ? "true" : "false")
         << ", pause_debounce_ms=" << g_config.pauseDebounceMs << ")";
    DebugLog(info.str().c_str());
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        char dllPath[_MAX_PATH] = { 0 };
        if (GetModuleFileNameA(hModule, dllPath, _MAX_PATH) > 0)
        {
            std::string fullPath = TrimAscii(std::string(dllPath));
            size_t sep = fullPath.find_last_of("\\/");
            if (sep != std::string::npos)
            {
                const std::string myDirectory = fullPath.substr(0, sep);
                g_settingsPath = myDirectory + "\\" + kConfigFileName;
            }
        }
    }
    return TRUE;
}
