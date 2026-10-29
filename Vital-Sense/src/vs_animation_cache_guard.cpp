#include "vs_animation_cache_guard.h"
#include <windows.h>
#include <string.h>
#include <core/Functions.h>

namespace vs_animation_cache_guard
{
namespace
{
    static const size_t NUM_STRIPES = 64;
    CRITICAL_SECTION locks[NUM_STRIPES];
    bool locksInitialized = false;

    volatile LONG ready = 0;
    volatile LONG attempted = 0;
    typedef void* (*GetTime)(const void*, void*, float);
    typedef void (*BuildTime)(const void*);
    typedef float (*GetFrames)(const void*, const void*, void**, void**, unsigned short*);
    typedef void (*BuildMap)(void*, const void*);
    GetTime originalGetTime = 0;
    BuildTime originalBuildTime = 0;
    GetFrames originalGetFrames = 0;
    BuildMap originalBuildMap = 0;

    inline size_t HashPtr(const void* ptr)
    {
        uintptr_t v = reinterpret_cast<uintptr_t>(ptr);
        return (v >> 4) & (NUM_STRIPES - 1);
    }

    inline CRITICAL_SECTION* GetLockForAnimation(const void* anim)
    {
        return &locks[HashPtr(anim)];
    }

    inline CRITICAL_SECTION* GetLockForTrack(const void* track)
    {
        if (!track) return &locks[0];
        // Ogre::AnimationTrack::mParent is at offset 0x28 (verified from OgreMain_x64!getParent disassembly)
        __try
        {
            const void* parent = *reinterpret_cast<const void* const*>(static_cast<const char*>(track) + 0x28);
            if (parent) return &locks[HashPtr(parent)];
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return &locks[HashPtr(track)];
    }

    // MSVC x64 member function return convention for non-trivial struct (Ogre::TimeIndex):
    // RCX = this, RDX = TimeIndex* output buffer, XMM2 = float timePos, RAX = output buffer.
    void* GetTimeHook(const void* self, void* output, float time)
    {
        if (!Ready()) return originalGetTime(self, output, time);
        CRITICAL_SECTION* l = GetLockForAnimation(self);
        EnterCriticalSection(l);
        __try { return originalGetTime(self, output, time); }
        __finally { LeaveCriticalSection(l); }
    }

    void BuildTimeHook(const void* self)
    {
        if (!Ready()) { originalBuildTime(self); return; }
        CRITICAL_SECTION* l = GetLockForAnimation(self);
        EnterCriticalSection(l);
        __try { originalBuildTime(self); }
        __finally { LeaveCriticalSection(l); }
    }

    float GetFramesHook(const void* self, const void* time, void** first,
        void** second, unsigned short* index)
    {
        if (!Ready()) return originalGetFrames(self, time, first, second, index);
        CRITICAL_SECTION* l = GetLockForTrack(self);
        EnterCriticalSection(l);
        __try { return originalGetFrames(self, time, first, second, index); }
        __finally { LeaveCriticalSection(l); }
    }

    void BuildMapHook(void* self, const void* times)
    {
        if (!Ready()) { originalBuildMap(self, times); return; }
        CRITICAL_SECTION* l = GetLockForTrack(self);
        EnterCriticalSection(l);
        __try { originalBuildMap(self, times); }
        __finally { LeaveCriticalSection(l); }
    }

    struct Binding
    {
        const char* symbol;
        size_t rva;
        unsigned char prefix[8];
        void* detour;
        void** original;
    };
} // namespace

bool Ready()
{
    return InterlockedCompareExchange(&ready, 0, 0) != 0;
}

bool Install()
{
    if (Ready()) return true;
    if (InterlockedCompareExchange(&attempted, 1, 0) != 0)
        return false;

    HMODULE ogre = GetModuleHandleW(L"OgreMain_x64.dll");
    if (!ogre) return false;

    const unsigned char* image = reinterpret_cast<const unsigned char*>(ogre);
    const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;

    const IMAGE_NT_HEADERS64* nt =
        reinterpret_cast<const IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64
        || nt->FileHeader.TimeDateStamp != 0x5CA5F929
        || nt->OptionalHeader.SizeOfImage != 0x9C9000)
    {
        return false;
    }

    Binding bindings[] = {
        { "?_getTimeIndex@Animation@Ogre@@QEBA?AVTimeIndex@2@M@Z", 0x175C0,
          { 0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83 },
          reinterpret_cast<void*>(&GetTimeHook), reinterpret_cast<void**>(&originalGetTime) },
        { "?buildKeyFrameTimeList@Animation@Ogre@@IEBAXXZ", 0x17680,
          { 0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x6C },
          reinterpret_cast<void*>(&BuildTimeHook), reinterpret_cast<void**>(&originalBuildTime) },
        { "?getKeyFramesAtTime@AnimationTrack@Ogre@@UEBAMAEBVTimeIndex@2@PEAPEAVKeyFrame@2@1PEAG@Z", 0x2E7E0,
          { 0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74 },
          reinterpret_cast<void*>(&GetFramesHook), reinterpret_cast<void**>(&originalGetFrames) },
        { "?_buildKeyFrameIndexMap@AnimationTrack@Ogre@@UEAAXAEBV?$vector@MV?$STLAllocator@MV?$CategorisedAllocPolicy@$0A@@Ogre@@@Ogre@@@std@@@Z", 0x2EC70,
          { 0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74 },
          reinterpret_cast<void*>(&BuildMapHook), reinterpret_cast<void**>(&originalBuildMap) }
    };

    void* targets[4] = { 0 };
    for (unsigned i = 0; i < 4; ++i)
    {
        targets[i] = reinterpret_cast<void*>(GetProcAddress(ogre, bindings[i].symbol));
        if (reinterpret_cast<size_t>(targets[i]) - reinterpret_cast<size_t>(ogre) != bindings[i].rva
            || memcmp(targets[i], bindings[i].prefix, sizeof(bindings[i].prefix)) != 0)
        {
            return false;
        }
    }

    HMODULE pinned = 0;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<const wchar_t*>(&GetTimeHook), &pinned))
    {
        return false;
    }

    for (size_t i = 0; i < NUM_STRIPES; ++i)
    {
        if (!InitializeCriticalSectionEx(&locks[i], 0, 0))
            return false;
    }
    locksInitialized = true;

    // 64-stripe hashed lock table: shards animation contention across instances by (ptr >> 4) & 63.
    // Tracks map to their parent Animation object (offset 0x28).
    for (unsigned i = 0; i < 4; ++i)
    {
        if (KenshiLib::AddHook(targets[i], bindings[i].detour, bindings[i].original)
            != KenshiLib::SUCCESS)
        {
            return false;
        }
    }

    InterlockedExchange(&ready, 1);
    return true;
}

} // namespace vs_animation_cache_guard
