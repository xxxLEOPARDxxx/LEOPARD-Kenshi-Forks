#include <Debug.h>

#include "src/hub_menu_bridge.h"

#include <kenshi/Kenshi.h>

#include <Windows.h>

#include <string>

namespace
{
bool IsSupportedVersion(KenshiLib::BinaryVersion& versionInfo)
{
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    return platform != KenshiLib::BinaryVersion::UNKNOWN
        && (version == "1.0.65" || version == "1.0.68");
}
}

__declspec(dllexport) void startPlugin()
{
    DebugLog("Emkejs-Mod-Core: startPlugin()");

    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    if (!IsSupportedVersion(versionInfo))
    {
        ErrorLog("Emkejs-Mod-Core: unsupported Kenshi version/platform");
        return;
    }

    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();
    if (!HubMenuBridge_InstallHooks(platform, version))
    {
        HubMenuBridge_SetHubEnabled(false);
        ErrorLog("Emkejs-Mod-Core: failed to install Mod Hub hooks; hub path disabled");
        return;
    }

    DebugLog("Emkejs-Mod-Core INFO: startup complete");
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
