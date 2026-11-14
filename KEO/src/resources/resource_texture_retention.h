#pragma once
namespace resource_texture_retention {
typedef unsigned __int64 U64;
struct __declspec(align(16)) Stats {
    U64 heldBytes,observed,retained,reused,evicted,rejected,queueFull,identityMismatch,unsupported,loadTimeout;
    U64 managerRequests,loadCompletions;
    unsigned textures,pending;
    bool enabled,admitted,pressure;
};
void Init();
void Tick(bool enabled,U64 sharedCap,U64 worldGeneration);
void Clear(); // Main owner, BEFORE Ogre shutdown. Never DllMain.
Stats Snapshot();
}
