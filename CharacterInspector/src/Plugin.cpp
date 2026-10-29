#include <Debug.h>
#include <PluginDiagnostics.h>

#include "InventoryStatsButton.h"
#include "InspectorSettings.h"

__declspec(dllexport) void startPlugin()
{
    InspectorSettings::Load();
    PluginDiagnostics::Write("Character Inspector: startPlugin");
    if (!InventoryStatsButton::InstallHooks())
        ErrorLog("Character Inspector: failed to install hooks");
}
