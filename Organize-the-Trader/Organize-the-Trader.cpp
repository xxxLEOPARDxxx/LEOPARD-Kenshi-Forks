#include <Debug.h>

#include <core/Functions.h>
#include <kenshi/Kenshi.h>

#include <Windows.h>

#include <sstream>
#include <string>

namespace
{
const char* kPluginName = "Organize-the-Trader";

bool IsSupportedVersion(const KenshiLib::BinaryVersion& versionInfo)
{
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    return platform != KenshiLib::BinaryVersion::UNKNOWN
        && (version == "1.0.65" || version == "1.0.68");
}
}

__declspec(dllexport) void startPlugin()
{
    DebugLog("Organize-the-Trader: startPlugin()");

    const KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    if (!IsSupportedVersion(versionInfo))
    {
        ErrorLog("Organize-the-Trader: unsupported Kenshi version/platform");
        return;
    }

    std::stringstream info;
    info << kPluginName << " INFO: base plugin initialized";
    DebugLog(info.str().c_str());
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
