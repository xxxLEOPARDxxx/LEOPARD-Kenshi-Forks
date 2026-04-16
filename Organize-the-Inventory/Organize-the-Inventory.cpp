#include "src/InventoryConfig.h"
#include "src/InventoryCore.h"
#include "src/InventoryHooks.h"
#include "src/InventoryModHub.h"

#include <core/Functions.h>
#include <kenshi/Kenshi.h>

#include <Windows.h>

#include <sstream>

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

    LoadInventoryConfig();

    if (!InstallInventoryHooks())
    {
        LogErrorLine("could not hook PlayerInterface::updateUT");
        return;
    }

    InventoryModHub_OnStartup();

    LogDebugLine("runtime debug logging is enabled");
    LogInfoLine("inventory search scaffold tick is active");
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
