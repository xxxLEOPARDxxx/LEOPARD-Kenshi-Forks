#pragma once
// Main/UI thread only. Snapshot owns no native pointers; producer copies counters.
namespace resource_warm_hud {
struct Snapshot {
    unsigned __int64 ramBytes,ramCap,ramReadBytes;
    unsigned ramFiles,ramPending;
    bool ramActive;
    unsigned __int64 gpuBytes,gpuCap;
    unsigned gpuReady,gpuPending;
    unsigned __int64 gpuCreated,gpuAdopted,gpuSkipUploads;
    bool workPaused,ramOn,gpuOn;
};
}
void ResourceWarmHudTick(bool enabled,const resource_warm_hud::Snapshot& snapshot);
// Before a world reset or plugin shutdown, while GUI is still available.
void ResourceWarmHudCleanup();
// After cached GUI disappears: forget references without touching old widgets.
void ResourceWarmHudGuiGone();
