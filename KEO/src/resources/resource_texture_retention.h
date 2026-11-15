#pragma once
namespace resource_texture_retention {
typedef unsigned __int64 U64;
struct __declspec(align(16)) Stats {
    U64 heldBytes,observed,retained,reused,evicted,rejected,queueFull,identityMismatch,unsupported,loadTimeout;
    U64 managerRequests,loadCompletions,loaderCompletions;
    unsigned textures,pending;
    bool enabled,admitted,pressure;
};
void Init();
void Tick(bool enabled,U64 sharedCap,U64 worldGeneration);
void Clear(); // Main owner, BEFORE Ogre shutdown. Never DllMain.
// Borrow caller-owned native TexturePtr result storage after successful loader
// completion. Does not copy, overwrite or destroy that output. No native load.
void ObserveLoadedResult(void* texturePtrOutput);
Stats Snapshot();
}
