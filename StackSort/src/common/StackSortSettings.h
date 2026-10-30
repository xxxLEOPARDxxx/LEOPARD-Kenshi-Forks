#pragma once

#include <istream>
#include <string>
#include <vector>

// Values read from StackSort.cfg. Pure STL and packer-free: -1 on the sentinel
// fields means "leave the packer's compiled default alone", so no constant from
// src\packer is ever mirrored here.
struct StackSortSettings
{
    int logLevel;   // 0 = errors only, 1 = info, 2 = verbose
    int defaultDim; // 0 = H, 1 = W
    bool consolidateStacks;
    int numWorkers;
    int mutationDebounceMs;
    int resnapshotsPerFrame;
    int topTargets;
    int arrowButtonWidth;
    int dimButtonWidth;
    int sortButtonWidth;
    int buttonGap;
    int bodyXOffset;

    // -1 keeps the packer's own compiled default.
    int firstPassRestarts;
    int firstPassIters;
    int refineRestarts;
    int refineIters;
    int refineLahcHistory;
    int refinePlateau;
    int refineItemThreshold;

    int syncRestarts;
    int syncIters;

    StackSortSettings();
};

struct SettingsReport
{
    bool fileFound;
    std::vector<std::string> warnings;
    std::vector<std::string> overrides;

    SettingsReport() : fileFound(false) {}
};

namespace StackSortSettingsStore
{
// Pure and testable: parses cfg text, leaving every unparsable or out-of-range
// key at its default and recording a warning. Overrides list only the keys
// whose final value differs from the default.
void LoadFromStream(std::istream& in, StackSortSettings& out, SettingsReport& report);

void LoadFromFile(const std::string& path, StackSortSettings& out, SettingsReport& report);

// Fills the process-wide settings. Called once from startPlugin, before any
// worker thread exists; Get() is read-only from then on, which is what makes
// worker-thread reads safe without a lock.
SettingsReport Load(const std::string& path);

const StackSortSettings& Get();
} // namespace StackSortSettingsStore
