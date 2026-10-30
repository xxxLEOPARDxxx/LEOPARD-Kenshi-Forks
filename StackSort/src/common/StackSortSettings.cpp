#include <fstream>

#include "StackSortSettings.h"
#include "McmSettings.h"
#include "IniLexer.h"
#include "ModPath.h"
#include "StrUtil.h"

static const char* const CFG_NAME = "StackSort.cfg";

StackSortSettings::StackSortSettings()
    : logLevel(0), defaultDim(0), consolidateStacks(true), numWorkers(4), mutationDebounceMs(150),
      resnapshotsPerFrame(3), topTargets(5), arrowButtonWidth(30), dimButtonWidth(40), sortButtonWidth(180),
      buttonGap(2), bodyXOffset(-35), firstPassRestarts(-1), firstPassIters(-1), refineRestarts(-1), refineIters(-1),
      refineLahcHistory(-1), refinePlateau(-1), refineItemThreshold(-1), syncRestarts(4), syncIters(1000)
{
}

struct IntKeySpec
{
    const char* key;
    int* field;
    int defaultValue;
    int minValue;
    int maxValue;
    bool allowMinusOne; // -1 means "leave the packer's compiled default alone"
};

static std::string Where(int line)
{
    return std::string(CFG_NAME) + ":" + IntToStr(line) + ": ";
}

void StackSortSettingsStore::LoadFromStream(std::istream& in, StackSortSettings& out, SettingsReport& report)
{
    out = StackSortSettings();
    const StackSortSettings def;
    report.warnings.clear();
    report.overrides.clear();

    std::vector<IniEntry> entries;
    std::vector<std::string> lexWarnings;
    IniLex(in, entries, lexWarnings);
    for (size_t i = 0; i < lexWarnings.size(); ++i)
        report.warnings.push_back(std::string(CFG_NAME) + ":" + lexWarnings[i]);

    const IntKeySpec specs[] = {
        {"log_level", &out.logLevel, def.logLevel, 0, 2, false},
        {"num_workers", &out.numWorkers, def.numWorkers, 1, 8, false},
        {"mutation_debounce_ms", &out.mutationDebounceMs, def.mutationDebounceMs, 0, 5000, false},
        {"resnapshots_per_frame", &out.resnapshotsPerFrame, def.resnapshotsPerFrame, 1, 16, false},
        {"top_targets", &out.topTargets, def.topTargets, 1, 10, false},
        {"arrow_button_width", &out.arrowButtonWidth, def.arrowButtonWidth, 10, 200, false},
        {"dim_button_width", &out.dimButtonWidth, def.dimButtonWidth, 10, 200, false},
        {"sort_button_width", &out.sortButtonWidth, def.sortButtonWidth, 40, 600, false},
        {"button_gap", &out.buttonGap, def.buttonGap, 0, 50, false},
        {"body_x_offset", &out.bodyXOffset, def.bodyXOffset, -500, 500, false},
        {"first_pass_restarts", &out.firstPassRestarts, def.firstPassRestarts, 1, 256, true},
        {"first_pass_iters", &out.firstPassIters, def.firstPassIters, 100, 1000000, true},
        {"refine_restarts", &out.refineRestarts, def.refineRestarts, 1, 256, true},
        {"refine_iters", &out.refineIters, def.refineIters, 100, 1000000, true},
        {"refine_lahc_history", &out.refineLahcHistory, def.refineLahcHistory, 1, 100000, true},
        {"refine_plateau", &out.refinePlateau, def.refinePlateau, 1, 1000000, true},
        {"refine_item_threshold", &out.refineItemThreshold, def.refineItemThreshold, 1, 1000, true},
        // The sync fallback runs on the main thread with no abort flag, so the
        // ranges cap restarts x iters at 16000, 4x the default 4 x 1000.
        {"sync_restarts", &out.syncRestarts, def.syncRestarts, 1, 8, false},
        {"sync_iters", &out.syncIters, def.syncIters, 100, 2000, false},
    };
    const int numSpecs = (int)(sizeof(specs) / sizeof(specs[0]));

    for (size_t e = 0; e < entries.size(); ++e)
    {
        const IniEntry& entry = entries[e];

        if (entry.key == "consolidate_stacks")
        {
            bool v = def.consolidateStacks;
            if (IniParseBool(entry.value, v)) out.consolidateStacks = v;
            else
                report.warnings.push_back(Where(entry.line) + entry.key + "=" + entry.value +
                                          " is not a boolean, using " + (out.consolidateStacks ? "true" : "false"));
            continue;
        }

        if (entry.key == "default_dim")
        {
            std::string v = ToLowerAscii(entry.value);
            if (v == "h" || v == "height") out.defaultDim = 0;
            else if (v == "w" || v == "width") out.defaultDim = 1;
            else
                report.warnings.push_back(Where(entry.line) + entry.key + "=" + entry.value +
                                          " is not H/W/height/width, using " + (out.defaultDim == 1 ? "W" : "H"));
            continue;
        }

        int specIdx = -1;
        for (int s = 0; s < numSpecs; ++s)
        {
            if (entry.key == specs[s].key)
            {
                specIdx = s;
                break;
            }
        }
        if (specIdx < 0)
        {
            report.warnings.push_back(Where(entry.line) + "unknown key '" + entry.key + "'");
            continue;
        }

        const IntKeySpec& spec = specs[specIdx];

        int value     = 0;
        bool accepted = IniParseInt(entry.value, value);
        if (accepted && !(spec.allowMinusOne && value == -1))
            accepted = (value >= spec.minValue && value <= spec.maxValue);

        if (!accepted)
        {
            report.warnings.push_back(Where(entry.line) + entry.key + "=" + entry.value + " out of range [" +
                                      IntToStr(spec.minValue) + ".." + IntToStr(spec.maxValue) + "], using " +
                                      IntToStr(*spec.field));
            continue;
        }

        // Duplicate keys: last one wins.
        *spec.field = value;
    }

    // An override is a final value that differs from the default, so a key
    // written at its default value, or one that was rejected, never appears.
    for (int s = 0; s < numSpecs; ++s)
    {
        if (*specs[s].field != specs[s].defaultValue)
            report.overrides.push_back(std::string(specs[s].key) + "=" + IntToStr(*specs[s].field));
    }
    if (out.consolidateStacks != def.consolidateStacks)
        report.overrides.push_back(std::string("consolidate_stacks=") + (out.consolidateStacks ? "true" : "false"));
    if (out.defaultDim != def.defaultDim)
        report.overrides.push_back(std::string("default_dim=") + (out.defaultDim == 1 ? "W" : "H"));
}

void StackSortSettingsStore::LoadFromFile(const std::string& path, StackSortSettings& out, SettingsReport& report)
{
    std::ifstream f(ModPath::ToWide(path).c_str(), std::ios::binary);
    if (!f.is_open())
    {
        out              = StackSortSettings();
        report.fileFound = false;
        return;
    }
    LoadFromStream(f, out, report);
    report.fileFound = true;
}

static StackSortSettings g_settings;

SettingsReport StackSortSettingsStore::Load(const std::string& path)
{
    SettingsReport report;
    LoadFromFile(path, g_settings, report);
    StackSortMcm::LoadStartup(g_settings);
    return report;
}

const StackSortSettings& StackSortSettingsStore::Get()
{
    return g_settings;
}
