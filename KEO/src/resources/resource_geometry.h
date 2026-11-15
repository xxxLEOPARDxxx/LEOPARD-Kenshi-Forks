#pragma once
namespace resource_geometry {
typedef unsigned __int64 U64;
// VC10 /O2 emits aligned SSE copies for this returned aggregate. Explicit
// alignment keeps caller-owned hidden-return storage valid as well.
struct __declspec(align(16)) Stats {
    U64 heldBytes,cpuShadowBytes,observed,retained,evicted,reused,rejected;
    U64 inputDifferent,queueFull,managerMissing,identityMismatch,loadTimeout,unsupportedManual,unsupportedDeform,unsupportedOther,noGpuBuffers,admissionRefused;
    unsigned models,pending;
    bool admitted,enabled,pressure;
};
// Main owner thread only. Retains actual natively loaded Mesh resources for
// revisits. Never loads unseen models, creates entities, or calls unload.
void Init();
// Borrow only a successful native switchBodyMesh RETURNED Entity. The old
// Entity/input MeshPtr are already consumed. Off-owner/OFF exits before any
// state or entity access; no Entity reference survives this call.
void ObserveFinalEntity(void* returnedEntity);
void Tick(bool enabled,U64 sharedGpuCap,U64 worldGeneration);
void Clear(); // BEFORE world/Ogre shutdown; never from DllMain
Stats Snapshot();
}
