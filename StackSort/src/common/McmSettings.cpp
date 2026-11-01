#define KLOC_DOMAIN "stack_sort"
#include <PluginSettings.h>
#include <Localization.h>
#include <ModConfigMenu.h>
#include "StackSortSettings.h"
#include "McmSettings.h"
#include "Log.h"
namespace
{
bool g_enabled = true;
bool g_merge = true;
int g_solutions = 5;
void LoadLive()
{
    const std::string path = PluginSettings::Path();
    g_enabled = PluginSettings::Bool("StackSort", "Enabled", true, path);
    g_merge = PluginSettings::Bool("StackSort", "ConsolidateStacks", true, path);
    g_solutions = (int)PluginSettings::Number("StackSort", "Solutions", (float)g_solutions, 1, 5, path);
    SetLogLevel(PluginSettings::Bool("StackSort", "Debug", false, path) ? LOG_LEVEL_VERBOSE : LOG_LEVEL_ERROR);
    KLocLog::SetEnabled(PluginSettings::Bool("StackSort", "Debug", false, path));
}
}
void StackSortMcm::LoadStartup(StackSortSettings& cfg)
{
    const std::string path = PluginSettings::Path();
    cfg.numWorkers = (int)PluginSettings::Number("StackSort", "Workers", (float)cfg.numWorkers, 1, 8, path);
    // These are immutable worker inputs after startup.
    cfg.defaultDim = PluginSettings::Text("StackSort", "DefaultDimension", cfg.defaultDim ? "W" : "H", path) == "W" ? 1 : 0;
    g_solutions = cfg.topTargets;
    LoadLive();
    g_merge = PluginSettings::Bool("StackSort", "ConsolidateStacks", cfg.consolidateStacks, path);
}
bool StackSortMcm::Enabled() { return g_enabled; }
bool StackSortMcm::MergeStacks() { return g_merge; }
int StackSortMcm::Solutions() { return g_solutions; }
extern "C" __declspec(dllexport) void MCM_Describe(MCM_Api* api)
{
    static const std::string path = PluginSettings::Path();
    api->beginMod(api, "StackSort", "Stack Sort", path.c_str(), LoadLive);
    if (api->version >= 2) api->info(api, Tr("Pack inventory items, merge compatible split stacks and reserve free space."));
    api->toggle(api, "StackSort", "Enabled", Tr("Enable Stack Sort"), Tr("Replace Arrange in inventories with room to sort. Single-slot storage keeps its original controls."), 1, 0);
    api->toggle(api, "StackSort", "ConsolidateStacks", Tr("Merge split stacks"), Tr("On sort clicks, merge compatible stacks after contents or remaining charges change. Uses the game's item and ownership rules. Undo restores positions, without splitting stacks."), 1, 0);
    static const char* values[] = {"H", "W"};
    const char* labels[] = {Tr("Height"), Tr("Width")};
    api->choice(api, "StackSort", "DefaultDimension", Tr("Starting free-space direction"), Tr("Starting H/W mode when the plugin starts. The inventory H/W button changes it during play."), "H", values, labels, 2, MCM_RESTART);
    api->integer(api, "StackSort", "Solutions", Tr("Solutions to cycle"), Tr("Number of best arrangements shown by repeated clicks on StackSort."), 5, 1, 5, 0);
    api->integer(api, "StackSort", "Workers", Tr("Background workers"), Tr("Threads used to prepare inventory arrangements. More workers can increase CPU use."), 4, 1, 8, MCM_RESTART);
    api->toggle(api, "StackSort", "Debug", Tr("Detailed log"), Tr("Log sorting, stack merging, rotation detection and background jobs in RE_Kenshi_log.txt. Errors are always logged."), 0, 0);
}
