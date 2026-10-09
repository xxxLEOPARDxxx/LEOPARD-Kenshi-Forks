#include <Debug.h>

#include "src/logging.h"
#include "src/hub_menu_bridge.h"

#include <kenshi/Kenshi.h>

#include <Windows.h>

#include <string>

namespace
{
bool IsSupportedVersion(unsigned int platform, const std::string& version)
{
    return platform != KenshiLib::BinaryVersion::UNKNOWN
        && (version == "1.0.65" || version == "1.0.68");
}

bool ResolveSupportedRuntimeNoSeh(unsigned int* out_platform, std::string* out_version)
{
    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();
    if (!IsSupportedVersion(platform, version))
    {
        return false;
    }

    if (out_platform != 0)
    {
        *out_platform = platform;
    }
    if (out_version != 0)
    {
        *out_version = version;
    }
    return true;
}

bool ResolveSupportedRuntime(unsigned int* out_platform, std::string* out_version)
{
#ifdef _DEBUG
    // Debug deployments run under RE_Kenshi.exe, and the KenshiLib version helper can
    // fault across CRT boundaries in that configuration. Use the local Steam test runtime.
    if (out_platform != 0)
    {
        *out_platform = KenshiLib::BinaryVersion::STEAM;
    }
    if (out_version != 0)
    {
        *out_version = "1.0.65";
    }
    return true;
#else
    __try
    {
        return ResolveSupportedRuntimeNoSeh(out_platform, out_version);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ErrorLog("Emkejs-Mod-Core ERROR: GetKenshiVersion() faulted during startup");
        return false;
    }
#endif
}
}

__declspec(dllexport) void startPlugin()
{
    LogInfoLine("startPlugin()");
    LoadLoggingConfig();

    unsigned int platform = KenshiLib::BinaryVersion::UNKNOWN;
    std::string version;
    if (!ResolveSupportedRuntime(&platform, &version))
    {
        LogErrorLine("unsupported Kenshi version/platform");
        return;
    }
    if (!HubMenuBridge_InstallHooks(platform, version))
    {
        HubMenuBridge_SetHubEnabled(false);
        LogErrorLine("failed to install Mod Hub hooks; hub path disabled");
        return;
    }

    LogInfoLine("startup complete");
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
