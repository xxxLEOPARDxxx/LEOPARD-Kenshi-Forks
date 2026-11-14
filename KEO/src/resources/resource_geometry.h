#pragma once
namespace resource_geometry {
typedef unsigned __int64 U64;
// VC10 /O2 emits aligned SSE copies for this returned aggregate. Explicit
// alignment keeps caller-owned hidden-return storage valid as well.
struct __declspec(align(16)) Stats {
    U64 heldBytes,cpuShadowBytes,observed,retained,evicted,reused,rejected;
    unsigned models,pending;
    bool admitted,enabled,pressure;
};
// Main owner thread only. Retains actual natively loaded Mesh resources for
// revisits. Never loads unseen models, creates entities, or calls unload.
void Init();
void Tick(bool enabled,U64 sharedGpuCap,U64 worldGeneration);
void Clear(); // BEFORE world/Ogre shutdown; never from DllMain
Stats Snapshot();
}
