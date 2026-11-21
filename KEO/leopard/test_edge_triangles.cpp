#define NOMINMAX
#include <Windows.h>
#include <ogre/OgreRoot.h>
#include <ogre/OgreMesh.h>
#include <ogre/OgreMeshManager.h>
#include <ogre/OgreMeshSerializer.h>
#include <ogre/OgreDefaultHardwareBufferManager.h>
#include <ogre/OgreDataStream.h>
#include <ogre/OgreEdgeListBuilder.h>
#include <cstdio>
#include <fstream>
#include <vector>
#include <cstring>
#include "fixes/world/edge_triangles.h"

namespace fixes {
struct Config { bool meshWeightsEnabled; };
static Config g_fixesCfg = {true};
}
static DWORD owner = GetCurrentThreadId();
static bool IsMainThread() { return GetCurrentThreadId() == owner; }

namespace edge_triangles {
static unsigned optimizedCalls = 0;
bool ObserveDecode(const unsigned char* src, const unsigned char* end,
                   unsigned char* triangles, unsigned char* normals, unsigned char* scratch,
                   uint32_t count, uint32_t groups)
{
    if (!TryDecode(src, end, triangles, normals, scratch, count, groups))
        return false;
    ++optimizedCalls;
    return true;
}
}
#define KEO_EDGE_TRIANGLE_TEST
#define TryDecode ObserveDecode
#include "fixes/world/edge_triangle_patch.cpp"
#undef TryDecode

namespace native_triangle_test {
static unsigned originalCalls = 0;
static unsigned char* published = NULL;
static unsigned char* site = NULL;
static edge_triangle_patch::Binding::Resize grow = NULL;
static code_patch_threads::Set threads = {};
static bool throwFromGrow = false;

static void NativeGrow(void* array, size_t count)
{
    ++originalCalls;
    if (throwFromGrow)
        throw 17;
    grow(array, count);
}

static bool Arm(HMODULE ogre)
{
    unsigned char* base = reinterpret_cast<unsigned char*>(ogre);
    site = base + edge_triangle_patch::callRva;
    if (!save_gog_native::Match(base, edge_triangle_patch::ogreProof) ||
        !code_patch_threads::Prepare("keo-edge-triangle-fixture-emergency.txt"))
        return false;
    grow = reinterpret_cast<edge_triangle_patch::Binding::Resize>(base + 0xb4dd0);
    edge_triangle_patch::binding.resize = &NativeGrow;
    edge_triangle_patch::binding.flip = base + 0x337950;
    edge_triangle_patch::binding.read = base + 0xa2840;
    edge_triangle_patch::binding.streamVtable = base + 0x6810d8;
    edge_triangle_patch::admitted = edge_triangle_patch::Arm(site, &threads, &published);
    return edge_triangle_patch::admitted;
}

static void BindStream(void* stream, const void* nativeTable)
{
    // MSVC emits an import vtable in an external caller. Mesh::prepareImpl
    // and the stock Ogre constructor use the exported table directly.
    std::memcpy(stream, &nativeTable, sizeof(nativeTable));
}

static bool Restore()
{
    DWORD old, ignored;
    if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old))
        return false;
    std::memcpy(site, edge_triangle_patch::nativeCall, 5);
    return FlushInstructionCache(GetCurrentProcess(), site, 5) &&
           VirtualProtect(site, 5, old, &ignored) && VirtualFree(published, 0, MEM_RELEASE);
}
}

static unsigned checks = 0;
#define CHECK(test) do { ++checks; if (!(test)) { std::fprintf(stderr, "CHECK %d failed: %s\n", __LINE__, #test); return 1; } } while (0)

static uint64_t Hash(uint64_t previous, const void* source, size_t length) {
    const unsigned char* data = static_cast<const unsigned char*>(source);
    for (size_t index = 0; index < length; ++index)
        previous = (previous ^ data[index]) * 1099511628211ULL;
    return previous;
}

static uint64_t Digest(const Ogre::EdgeData& topology) {
    uint64_t out = 14695981039346656037ULL;
    out = Hash(out, &topology.triangles[0], topology.triangles.size() * sizeof(Ogre::EdgeData::Triangle));
    out = Hash(out, &topology.triangleFaceNormals[0], topology.triangleFaceNormals.size() * sizeof(Ogre::Vector4));
    for (size_t group = 0; group < topology.edgeGroups.size(); ++group) {
        const Ogre::EdgeData::EdgeGroup& g = topology.edgeGroups[group];
        for (size_t n = 0; n < g.edges.size(); ++n)
            out = Hash(out, &g.edges[n], offsetof(Ogre::EdgeData::Edge, degenerate) + 1);
    }
    return out;
}

static int CheckDecodeBounds() {
    const uint32_t count = 256;
    const uint32_t groupCount = 0;
    std::vector<unsigned char> input(18 + 48 * count, 0);
    std::vector<unsigned char> triangles(64 * count, 0xee), normals(16 * count, 0xee);
    unsigned char scratch[12];
    std::memset(scratch, 0xee, sizeof(scratch));
    input[1] = 0xb1;
    uint32_t length = static_cast<uint32_t>(input.size());
    std::memcpy(&input[2], &length, sizeof(length));
    std::memcpy(&input[10], &count, sizeof(count));
    for (uint32_t i = 0; i < count; ++i) {
        for (unsigned field = 0; field < 12; ++field) {
            const uint32_t value = 0x80000000u + i * 17 + field * 10101;
            std::memcpy(&input[18 + 48 * i + 4 * field], &value, sizeof(value));
        }
    }
    unsigned char* const data = &input[18];
    const unsigned char* const end = &input[0] + input.size();
    CHECK(!edge_triangles::TryDecode(data, end - 1, &triangles[0], &normals[0], scratch, count, groupCount));
    CHECK(triangles[0] == 0xee && normals[0] == 0xee && scratch[0] == 0xee);
    CHECK(!edge_triangles::TryDecode(data, end, &triangles[0], &triangles[0], scratch, count, groupCount));
    CHECK(triangles[0] == 0xee && scratch[0] == 0xee);
    CHECK(!edge_triangles::TryDecode(NULL, NULL, NULL, NULL, NULL, 127, 0));
    CHECK(edge_triangles::TryDecode(data, end, &triangles[0], &normals[0], scratch, count, groupCount));
    for (uint32_t i = 0; i < count; ++i) {
        for (unsigned field = 0; field < 8; ++field) {
            uint32_t expected;
            uint64_t actual;
            std::memcpy(&expected, &data[48 * i + 4 * field], sizeof(expected));
            std::memcpy(&actual, &triangles[64 * i + 8 * field], sizeof(actual));
            CHECK(expected == actual);
        }
        CHECK(std::memcmp(&data[48 * i + 32], &normals[16 * i], 16) == 0);
    }
    CHECK(std::memcmp(scratch, end - 28, 12) == 0);
    return 0;
}

static int Verify() {
    const HMODULE ogre = GetModuleHandleA("OgreMain_x64.dll");
    const void* const streamVtable = GetProcAddress(ogre, "??_7MemoryDataStream@Ogre@@6B@");
    CHECK(streamVtable != NULL);
    const void* const nativeRead = GetProcAddress(ogre, "?read@MemoryDataStream@Ogre@@UEAA_KPEAX_K@Z");
    CHECK(nativeRead != NULL);
    CHECK(native_triangle_test::Arm(ogre));
    const char* const names[] = {"human/human_male.mesh", "human/human_female.mesh",
        "stick_person/Stick_person.mesh", "stick_person/Hive_Queen.mesh", "robot/robot.mesh"};
    const size_t lodCount[] = {2, 1, 2, 1, 2};
    const size_t triangles[][2] = {{5168, 1284}, {5538, 0}, {3138, 768}, {8550, 0}, {9909, 1826}};
    const size_t edges[][2] = {{7770, 1932}, {8319, 0}, {4725, 1161}, {12936, 0}, {15234, 2820}};
    const std::string prefix = "D:/SteamLibrary/steamapps/common/Kenshi/data/character/meshes/";
    for (unsigned asset = 0; asset < 5; ++asset) {
        uint64_t originalTopology[2] = {};
        size_t originalCursor = 0;
        const unsigned passes = asset == 0 ? 4 : 2;
        for (unsigned pass = 0; pass < passes; ++pass) {
            const std::string path = prefix + names[asset];
            std::ifstream file(path.c_str(), std::ios::binary | std::ios::ate);
            CHECK(file.good() && file.tellg() > 0);
            std::vector<unsigned char> bytes(static_cast<size_t>(file.tellg()));
            file.seekg(0);
            file.read(reinterpret_cast<char*>(&bytes[0]), bytes.size());
            CHECK(file.good());
            Ogre::DataStreamPtr stream(OGRE_NEW Ogre::MemoryDataStream(&bytes[0], bytes.size(), false, true));
            if (pass != 2)
                native_triangle_test::BindStream(stream.get(), streamVtable);
            fixes::g_fixesCfg.meshWeightsEnabled = pass != 0;
            const char* suffix[] = {".stock", ".fast", ".other-stream", ".other-thread"};
            Ogre::MeshPtr mesh = Ogre::MeshManager::getSingleton().createManual(std::string(names[asset]) + suffix[pass], "General", NULL);
            Ogre::MeshSerializer serializer;
            const unsigned before = edge_triangles::optimizedCalls;
            if (asset == 0 && pass == 1) {
                Ogre::MeshPtr failed = Ogre::MeshManager::getSingleton().createManual("throwing-edge-group", "General", NULL);
                Ogre::MeshSerializer failingSerializer;
                const unsigned beforeFailure = native_triangle_test::originalCalls;
                native_triangle_test::throwFromGrow = true;
                bool caught = false;
                try { failingSerializer.importMesh(stream, failed.get()); }
                catch (int error) { caught = error == 17; }
                native_triangle_test::throwFromGrow = false;
                CHECK(caught && native_triangle_test::originalCalls == beforeFailure + 1);
                CHECK(edge_triangles::optimizedCalls == before);
                CHECK(stream->tell() == 1435641);
                stream->seek(0);
            }
            if (pass == 3)
                owner = 0;
            serializer.importMesh(stream, mesh.get());
            owner = GetCurrentThreadId();
            if (pass) {
                CHECK(edge_triangles::optimizedCalls - before == (pass == 1 ? lodCount[asset] : 0));
                CHECK(stream->tell() == originalCursor);
            } else originalCursor = stream->tell();
            const Ogre::Mesh& nativeMesh = *mesh;
            CHECK(nativeMesh.getNumLodLevels() == lodCount[asset]);
            for (unsigned lod = 0; lod < lodCount[asset]; ++lod) {
                const Ogre::EdgeData* const topology = nativeMesh.getEdgeList(static_cast<unsigned short>(lod));
                CHECK(topology != NULL && topology->edgeGroups.size() == 1);
                CHECK(topology->triangles.size() == triangles[asset][lod]);
                CHECK(topology->triangleFaceNormals.size() == triangles[asset][lod]);
                CHECK(topology->edgeGroups[0].edges.size() == edges[asset][lod]);
                if (pass) CHECK(Digest(*topology) == originalTopology[lod]);
                else originalTopology[lod] = Digest(*topology);
                std::printf("%s lod=%u triangles=%u edges=%u\n", names[asset], lod,
                    static_cast<unsigned>(triangles[asset][lod]), static_cast<unsigned>(edges[asset][lod]));
            }
        }
    }
    CHECK(native_triangle_test::Restore());
    CHECK(native_triangle_test::originalCalls == 21 && edge_triangles::optimizedCalls == 8);
    unsigned char bytes[] = {1, 2, 3, 4, 5, 6, 7};
    Ogre::MemoryDataStream stream(bytes, sizeof(bytes), false, true);
    unsigned char left[] = {0, 0, 0, 0}, right[] = {0xee, 0xee, 0xee, 0xee};
    CHECK(stream.read(left, 4) == 4 && stream.tell() == 4);
    CHECK(stream.read(right, 4) == 3 && stream.tell() == sizeof(bytes));
    CHECK(right[0] == 5 && right[1] == 6 && right[2] == 7 && right[3] == 0xee);
    CHECK(stream.read(right, 4) == 0 && right[3] == 0xee);
    return 0;
}

static int Benchmark() {
    unsigned checks = 0;
    HMODULE ogre = GetModuleHandleA("OgreMain_x64.dll");
    CHECK(native_triangle_test::Arm(ogre));
    const char* path = "D:/SteamLibrary/steamapps/common/Kenshi/data/character/meshes/robot/robot.mesh";
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    CHECK(file.good() && file.tellg() > 0);
    std::vector<unsigned char> bytes(static_cast<size_t>(file.tellg()));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&bytes[0]), bytes.size());
    CHECK(file.good());
    LARGE_INTEGER frequency;
    CHECK(QueryPerformanceFrequency(&frequency));
    const unsigned pairCount = 30;
    for (unsigned pair = 0; pair < pairCount; ++pair) {
        double times[2];
        for (unsigned order = 0; order < 2; ++order) {
            const unsigned mode = order ^ (pair & 1);
            char name[64];
            sprintf_s(name, "native-edge-bench-%u-%u", pair, mode);
            Ogre::DataStreamPtr stream(OGRE_NEW Ogre::MemoryDataStream(&bytes[0], bytes.size(), false, true));
            native_triangle_test::BindStream(stream.get(), edge_triangle_patch::binding.streamVtable);
            fixes::g_fixesCfg.meshWeightsEnabled = mode != 0;
            Ogre::MeshPtr mesh = Ogre::MeshManager::getSingleton().createManual(name, "General", NULL);
            Ogre::MeshSerializer serializer;
            LARGE_INTEGER begin, end;
            QueryPerformanceCounter(&begin);
            serializer.importMesh(stream, mesh.get());
            QueryPerformanceCounter(&end);
            times[mode] = (end.QuadPart - begin.QuadPart) * 1e6 / frequency.QuadPart;
            Ogre::MeshManager::getSingleton().remove(name);
        }
        std::printf("PAIR %u native=%.2f fast=%.2f\n", pair, times[0], times[1]);
    }
    CHECK(native_triangle_test::Restore());
    CHECK(edge_triangles::optimizedCalls == pairCount * 2);
    return 0;
}

int main(int argc, char**) {
    setvbuf(stdout, NULL, _IONBF, 0);
    Ogre::Root root("", "", "");
    Ogre::DefaultHardwareBufferManager hardware;
    const int result = argc == 1 ? (CheckDecodeBounds() || Verify()) : Benchmark();
    Ogre::MeshManager::getSingleton().removeAll();
    if (!result && argc == 1)
        std::printf("PASS %u native import and partial-read checks\n", checks);
    else if (!result)
        std::puts("PASS 30 paired native imports");
    return result;
}
