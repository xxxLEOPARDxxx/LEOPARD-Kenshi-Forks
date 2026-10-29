#define KLOC_DOMAIN "character_inspector"
#include <PluginSettings.h>
#include <PluginDiagnostics.h>
#include <Localization.h>
#include <ModConfigMenu.h>
#include "InspectorSettings.h"

namespace InspectorSettings
{
    bool enabled = true, showButton = true;
    void Load()
    {
        const std::string p = PluginSettings::Path();
        enabled = PluginSettings::Bool("CharacterInspector", "Enabled", true, p);
        showButton = PluginSettings::Bool("CharacterInspector", "InventoryButton", true, p);
        PluginDiagnostics::Configure("CharacterInspector", p);
        if (PluginDiagnostics::Enabled())
            DebugLog(std::string("Character Inspector: settings enabled=") + (enabled ? "1" : "0") +
                     " inventoryButton=" + (showButton ? "1" : "0"));
    }
}
extern "C" __declspec(dllexport) void MCM_Describe(MCM_Api* api)
{
    static const std::string p = PluginSettings::Path();
    api->beginMod(api, "CharacterInspector", "Character Inspector", p.c_str(), InspectorSettings::Load);
    if (api->version >= 2) api->info(api, Tr("Inspect body parts, armour layers and protection using the inventory STATS button."));
    api->toggle(api, "CharacterInspector", "Enabled", Tr("Enabled"), Tr("Enable the inspector window and its controls."), 1, 0);
    api->toggle(api, "CharacterInspector", "InventoryButton", Tr("Inventory button"), Tr("Show the STATS button in character inventories."), 1, 0);
    api->toggle(api, "CharacterInspector", "Debug", Tr("Detailed log"), Tr("Details in RE_Kenshi_log.txt: inventory button, inspected character and protection window changes."), 0, 0);
}
