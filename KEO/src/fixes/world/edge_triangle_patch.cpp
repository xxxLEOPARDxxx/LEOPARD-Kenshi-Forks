#include "fixes/world/edge_triangles.h"
#include "fixes/code_patch_batch.h"
#include "fixes/near_page.h"
#include "fixes/world/save_gog_native.h"
#include <cstring>
#include <limits.h>

#ifndef KEO_EDGE_TRIANGLE_TEST
#include "base/core.h"
#include "base/ini_names.h"
#include "base/log_admission.h"
#include "fixes/fixes_config.h"
#include "render/module_hooks.h"
#include <ogre/OgreEdgeListBuilder.h>
#endif

namespace edge_triangle_patch {

static const uintptr_t callRva = 0x1cd03e;
static const unsigned char nativeCall[] = {0xe8, 0x8d, 0x7d, 0xee, 0xff};
static const save_gog_native::Window ogreProof[] = {
    {0x1ccf50, 0x600, 0x773ac80e7eb02995ULL}, // Complete private LOD reader and native continuation
    {0x0b4dd0, 0x0ff, 0xf69702b99dd2b4d8ULL}, // Native edge-group vector resize
    {0x0a2840, 0x059, 0x7ef2413838bc7109ULL}, // MemoryDataStream::read
    {0x337950, 0x042, 0x9560ef8a79036f53ULL}, // Serializer::flipFromLittleEndian
};

struct Binding {
    typedef void (*Resize)(void*, size_t);
    Resize resize;
    const unsigned char* streamVtable;
    const unsigned char* read;
    const unsigned char* flip;
};
static Binding binding = {};
static bool admitted = false;

static unsigned char* ReadPointer(const void* slot)
{
    unsigned char* value;
    std::memcpy(&value, slot, sizeof(value));
    return value;
}

static bool MatchArray(const unsigned char* array, const unsigned char* data, size_t bytes)
{
    const uintptr_t begin = reinterpret_cast<uintptr_t>(data);
    const uintptr_t end = reinterpret_cast<uintptr_t>(ReadPointer(array + 8));
    const uintptr_t capacity = reinterpret_cast<uintptr_t>(ReadPointer(array + 0x10));
    return begin && begin <= end && end <= capacity && end - begin == bytes;
}

static bool Decode(unsigned char* edges, unsigned char* counts,
                   const unsigned char* serializer, const void* sharedStream,
                   uint32_t groups)
{
    uint32_t count;
    std::memcpy(&count, counts, sizeof(count));
    unsigned char* stream = ReadPointer(sharedStream);
    // Mesh::prepareImpl wraps both archive and RAM-cache sources in this exact
    // class. Mesh::loadImpl passes it unchanged to the native serializer.
    const unsigned char readEntry[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83};
    const unsigned char flipEntry[] = {0x80, 0x79, 0x48, 0x00, 0x4d, 0x8b, 0xd8, 0x4c};
    if (count < 128 || !stream || ReadPointer(stream) != binding.streamVtable ||
        serializer[0x48] || ReadPointer(ReadPointer(serializer) + 0x38) != binding.flip ||
        ReadPointer(binding.streamVtable + 0x18) != binding.read ||
        std::memcmp(binding.read, readEntry, sizeof(readEntry)) ||
        std::memcmp(binding.flip, flipEntry, sizeof(flipEntry)))
        return false;

    const unsigned char* start = ReadPointer(stream + 0x40);
    const unsigned char* current = ReadPointer(stream + 0x48);
    const unsigned char* end = ReadPointer(stream + 0x50);
    const uintptr_t firstByte = reinterpret_cast<uintptr_t>(start);
    const uintptr_t cursor = reinterpret_cast<uintptr_t>(current);
    if (!start || cursor < firstByte || cursor - firstByte < 18)
        return false;

    unsigned char* triangles = ReadPointer(edges);
    unsigned char* normals = ReadPointer(edges + 0x20);
    if (!MatchArray(edges, triangles, 64 * size_t(count)) ||
        !MatchArray(edges + 0x20, normals, 16 * size_t(count)) ||
        !edge_triangles::TryDecode(current, end, triangles, normals, counts + 0x190, count, groups))
        return false;

    current += 48 * size_t(count);
    std::memcpy(stream + 0x48, &current, sizeof(current));
    // Only the immediately following native triangle loop reads this slot.
    // The edge-group count, chunk operations and group/edge reads stay native.
    const uint32_t consumed = 0;
    std::memcpy(counts, &consumed, sizeof(consumed));
    return true;
}

static void __declspec(noinline) ResizeAndRead(unsigned char* groupArray, unsigned char* counts,
                                              const unsigned char* serializer, const void* sharedStream)
{
    uint32_t groups;
    std::memcpy(&groups, counts + 4, sizeof(groups));
    binding.resize(groupArray, groups);
    if (IsMainThread() && admitted && fixes::g_fixesCfg.meshWeightsEnabled)
        Decode(groupArray - 0x60, counts, serializer, sharedStream, groups);
}

static bool Arm(unsigned char* site, code_patch_threads::Set* threads, unsigned char** published)
{
    if (std::memcmp(site, nativeCall, sizeof(nativeCall)))
        return false;
    unsigned char* island = AllocateNearPages(reinterpret_cast<uintptr_t>(site), 4096);
    if (!island)
        return false;

    // Existing CALL supplies return address and shadow space. The stub leaves
    // RSP and nonvolatile registers untouched, then tail-jumps to a C++ frame.
    // RCX is the native group array, RDX the triangle/group locals in the caller,
    // R8 the saved serializer (RDI), R9 the incoming stream reference (R12).
    const unsigned char stub[] = {0x48, 0x8d, 0x54, 0x24, 0x48, 0x4c, 0x8b, 0xc7,
                                  0x4d, 0x8b, 0xcc, 0xff, 0x25, 0, 0, 0, 0};
    std::memcpy(island, stub, sizeof(stub));
    const uintptr_t target = reinterpret_cast<uintptr_t>(&ResizeAndRead);
    std::memcpy(island + sizeof(stub), &target, sizeof(target));

    const int64_t delta = reinterpret_cast<int64_t>(island) - reinterpret_cast<int64_t>(site + 5);
    DWORD oldProtection;
    if (delta < INT_MIN || delta > INT_MAX ||
        !VirtualProtect(island, 4096, PAGE_EXECUTE_READ, &oldProtection) ||
        !FlushInstructionCache(GetCurrentProcess(), island, sizeof(stub) + sizeof(target))) {
        VirtualFree(island, 0, MEM_RELEASE);
        return false;
    }
    const int32_t relative = static_cast<int32_t>(delta);
    code_patch_batch::Edit edit = {site, 5, {0xe8, 0x8d, 0x7d, 0xee, 0xff}, {0xe8}};
    std::memcpy(edit.after + 1, &relative, sizeof(relative));
    bool mayPublish = false;
    const bool ok = code_patch_batch::Apply(&edit, 1, threads, &mayPublish);
    if (mayPublish)
        *published = island;
    else
        VirtualFree(island, 0, MEM_RELEASE);
    return ok;
}

#ifndef KEO_EDGE_TRIANGLE_TEST

static unsigned char* Admit(HMODULE ogre)
{
    __try {
        DWORD stamp = 0, size = 0;
        if (!IsMainThread() || !ogre || !ReadModuleImageId(ogre, &stamp, &size) ||
            stamp != 0x5ca5f929 || size != 0x9c9000 || !save_gog_native::Match(ogre, ogreProof))
            return NULL;

        const unsigned char* const base = reinterpret_cast<unsigned char*>(ogre);
        const unsigned char* const read = base + 0xa2840;
        const unsigned char* const table = base + 0x6810d8;
        if (reinterpret_cast<const void*>(GetProcAddress(ogre, "?read@MemoryDataStream@Ogre@@UEAA_KPEAX_K@Z")) != read ||
            reinterpret_cast<const void*>(GetProcAddress(ogre, "??_7MemoryDataStream@Ogre@@6B@")) != table ||
            ReadPointer(table + 0x18) != read)
            return NULL;
        binding.resize = reinterpret_cast<Binding::Resize>(reinterpret_cast<uintptr_t>(base) + 0xb4dd0);
        binding.streamVtable = table;
        binding.read = read;
        binding.flip = base + 0x337950;
        return reinterpret_cast<unsigned char*>(ogre) + callRva;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return NULL;
    }
}

static_assert(sizeof(Ogre::EdgeData::Triangle) == 64 && sizeof(Ogre::Vector4) == 16 &&
              sizeof(Ogre::EdgeData::EdgeGroup) == 64 && offsetof(Ogre::EdgeData, triangleFaceNormals) == 0x20 &&
              offsetof(Ogre::EdgeData, edgeGroups) == 0x60, "native edge reader storage layout");

static void Install()
{
    static bool tried = false;
    static code_patch_threads::Set threads = {};
    static unsigned char* island = NULL;
    if (tried)
        return;
    tried = true;
    unsigned char* site = Admit(GetModuleHandleA("OgreMain_x64.dll"));
    if (!site || !code_patch_threads::Prepare((GetDLLDirectory() + OPTIMIZER_LOG_NAME).c_str()) ||
        !Arm(site, &threads, &island)) {
        LogError("Model edge triangles: native reader contract or patch setup refused");
        return;
    }
    admitted = true;
    KEO_DETAIL("Model edge triangles: little-endian memory stream reader admitted");
}
#endif

} // namespace edge_triangle_patch

#ifndef KEO_EDGE_TRIANGLE_TEST
void InstallEdgeTriangles(int*, int*)
{
    edge_triangle_patch::Install();
}
#endif
