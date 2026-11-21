#include "fixes/world/save_transform_read.h"
#include "fixes/code_patch_batch.h"
#include "fixes/near_page.h"
#include "fixes/world/save_gog_native.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits.h>

#ifndef KEO_SAVE_TRANSFORM_TEST
#include "base/core.h"
#include "base/ini_names.h"
#include "base/log_admission.h"
#include "fixes/fixes_config.h"
#include "render/module_hooks.h"
#endif

namespace save_transform_read {

typedef void* (*ReadFn)(std::istream*, char*, std::streamsize);
static const int positionBytes = 12;
static const int rotationBytes = 16;
static const int transformBytes = positionBytes + rotationBytes;
static const int rotationOffset = 0x18; // Distance between the native stack locals.
static const int countOffset = 8;

struct Binding {
    ReadFn const volatile* readSlot;
    ReadFn nativeRead;
    const void* filebufVtable;
    const bool* enabled;
    DWORD ownerThread;
    const void* const volatile* bulkReadSlot;
    const void* nativeBulkRead;
    const void* bulkReadThunk;
    const void* lockThunk;
    const void* unlockThunk;
    uint64_t readEntry, bulkReadEntry;
};

static Binding binding = {};
static volatile bool admitted = false;
static unsigned char* island = NULL;
static code_patch_threads::Set threads = {};
static const unsigned char* rotationCall = NULL;
static int32_t rotationCallDisplacement = 0;
#ifdef KEO_SAVE_TRANSFORM_TEST
static bool testEnabled = true;
#endif

template<class T>
static T Member(const void* object, int offset)
{
    T value;
    std::memcpy(&value, static_cast<const char*>(object) + offset, sizeof(value));
    return value;
}

static bool HasBufferedTransform(std::istream& stream)
{
    const std::streambuf* buffer = stream.rdbuf();
    if (buffer != reinterpret_cast<const std::streambuf*>(reinterpret_cast<const char*>(&stream) + 0x10) ||
        Member<const void*>(buffer, 0) != binding.filebufVtable ||
        Member<const void*>(binding.filebufVtable, 0x40) != binding.bulkReadThunk ||
        Member<const void*>(binding.filebufVtable, 8) != binding.lockThunk ||
        Member<const void*>(binding.filebufVtable, 0x10) != binding.unlockThunk ||
        !stream.good() || stream.tie() ||
        stream.exceptions() || Member<const void*>(buffer, 0x80))
        return false;

    // Native VC10 filebuf can bind its get area to the CRT FILE. Refuse any
    // other state: in particular, small get areas and EOF go through both calls.
    const FILE* file = Member<const FILE*>(buffer, 0x98);
    return file && file->_cnt >= transformBytes && file->_ptr &&
           Member<const void*>(buffer, 0x40) == &file->_ptr &&
           Member<const void*>(buffer, 0x58) == &file->_cnt;
}

static __declspec(noinline) void* ReadPosition(std::istream* stream, char* position, std::streamsize size, char* rotation)
{
    const ReadFn read = *binding.readSlot;
    if (!admitted || GetCurrentThreadId() != binding.ownerThread || !*binding.enabled ||
        size != positionBytes || read != binding.nativeRead ||
        Member<uint16_t>(rotationCall, 0) != 0x15ff ||
        Member<int32_t>(rotationCall, 2) != rotationCallDisplacement ||
        *binding.bulkReadSlot != binding.nativeBulkRead ||
        Member<uint64_t>(reinterpret_cast<const void*>(read), 0) != binding.readEntry ||
        Member<uint64_t>(binding.nativeBulkRead, 0) != binding.bulkReadEntry ||
        !HasBufferedTransform(*stream))
    {
        read(stream, position, size);
        return stream;
    }

    char bytes[transformBytes];
    read(stream, bytes, transformBytes);
    const std::streamsize counted = stream->gcount();
    std::streamsize lastCount = rotationBytes;
    if (counted == transformBytes)
    {
        std::memcpy(position, bytes, positionBytes);
        std::memcpy(rotation, bytes + positionBytes, rotationBytes);
    }
    else
    {
        // The private stream has the complete block buffered. Still preserve
        // partial writes and the second read's gcount if a read returns short.
        const size_t available = counted > 0 && counted < transformBytes ? static_cast<size_t>(counted) : 0;
        lastCount = available > positionBytes ? available - positionBytes : 0;
        std::memcpy(position, bytes, available < positionBytes ? available : positionBytes);
        if (lastCount)
            std::memcpy(rotation, bytes + positionBytes, static_cast<size_t>(lastCount));
    }
    std::memcpy(reinterpret_cast<char*>(stream) + countOffset, &lastCount, sizeof(lastCount));
    // All intervening argument-setup instructions preserve RAX. The next
    // stub compares it with the rotation address to recognize a completed pair.
    return rotation;
}

static bool SetCallTarget(code_patch_batch::Edit& edit, unsigned char* site, const void* const* target)
{
    const unsigned char* next = site + 6;
    if (site[0] != 0xff || site[1] != 0x15 ||
        reinterpret_cast<uintptr_t>(next + Member<int32_t>(site, 2)) != reinterpret_cast<uintptr_t>(binding.readSlot))
        return false;
    const int64_t distance = reinterpret_cast<int64_t>(target) - reinterpret_cast<int64_t>(next);
    if (distance < INT_MIN || distance > INT_MAX)
        return false;
    edit.address = site;
    edit.size = 6;
    std::memcpy(edit.before, site, 6);
    edit.after[0] = 0xff;
    edit.after[1] = 0x15;
    const int32_t relative = static_cast<int32_t>(distance);
    std::memcpy(edit.after + 2, &relative, sizeof(relative));
    return true;
}

static bool Arm(unsigned char* positionSite, unsigned char* rotationSite)
{
    if (island)
        return false;
    unsigned char* page = AllocateNearPages(reinterpret_cast<uintptr_t>(positionSite), 4096);
    if (!page)
        return false;

    // First CALL keeps its address and shadow space. Supply the second
    // destination in R9 and tail jump to the compiled, unwindable bridge.
    const unsigned char positionStub[] = {0x4c, 0x8d, 0x4a, rotationOffset, 0xff, 0x25, 0, 0, 0, 0};
    std::memcpy(page, positionStub, sizeof(positionStub));
    const uintptr_t bridge = reinterpret_cast<uintptr_t>(&ReadPosition);
    std::memcpy(page + sizeof(positionStub), &bridge, sizeof(bridge));

    // Compare the marker before touching RAX. On fallback, R11 carries the
    // native IAT slot address and we tail jump to its current target.
    const unsigned char rotationStub[] = {
        0x48, 0x39, 0xd0,                   // cmp rax, rdx
        0x75, 0x04,                         // jne forward
        0x48, 0x8b, 0xc1, 0xc3,             // mov rax, rcx; ret
        0x49, 0xbb, 0, 0, 0, 0, 0, 0, 0, 0, // forward: mov r11, slot
        0x41, 0xff, 0x23                    // jmp qword ptr [r11]
    };
    std::memcpy(page + 32, rotationStub, sizeof(rotationStub));
    const uintptr_t slot = reinterpret_cast<uintptr_t>(binding.readSlot);
    std::memcpy(page + 43, &slot, sizeof(slot));
    const unsigned char* targets[] = {page, page + 32};
    std::memcpy(page + 64, targets, sizeof(targets));

    code_patch_batch::Edit edits[2] = {};
    const void* const* targetSlots = reinterpret_cast<const void* const*>(page + 64);
    DWORD oldProtection = 0;
    if (!SetCallTarget(edits[0], positionSite, targetSlots) ||
        !SetCallTarget(edits[1], rotationSite, targetSlots + 1) ||
        !VirtualProtect(page, 4096, PAGE_EXECUTE_READ, &oldProtection) ||
        !FlushInstructionCache(GetCurrentProcess(), page, 80))
    {
        VirtualFree(page, 0, MEM_RELEASE);
        return false;
    }
    bool mayPublish = false;
    const bool installed = code_patch_batch::Apply(edits, 2, &threads, &mayPublish);
    if (mayPublish)
        island = page;
    else
        VirtualFree(page, 0, MEM_RELEASE);
    // A partial publication never emits a marker. A native first read that
    // was already in flight also returns the stream, so the second still runs.
    if (installed)
    {
        rotationCall = rotationSite;
        rotationCallDisplacement = Member<int32_t>(edits[1].after, 2);
        admitted = true;
    }
    return installed;
}

#ifndef KEO_SAVE_TRANSFORM_TEST

struct GameContract {
    save_gog_native::Window instructions[3];
    uintptr_t readSlot, filebuf, bulkReadThunk, bulkReadSlot;
    uintptr_t lockThunk, unlockThunk;
};

static const GameContract steam = {
    {{0x6c1d55, 59, 0xbead1fc9dfd2ff25ULL}, {0x12fb10, 48, 0x291ad94ff575a343ULL}, {0x12fb40, 48, 0xb370c7957bd5b34bULL}},
    0x2244258, 0x168ef18, 0xed6fb6, 0x2244518, 0x3f076, 0x4d437
};
static const GameContract gog = {
    {{0x6c1665, 59, 0x4d449a4a85734f3cULL}, {0x12fa30, 48, 0xd424a77f376b8a93ULL}, {0x12fa60, 48, 0x38151ccaa5d2091bULL}},
    0x22421c8, 0x168df18, 0xed66b6, 0x2242488, 0x3f0e4, 0x4d4af
};
static const save_gog_native::Window crtProof[] = {
    {0x21434, 228, 0x369787cf4925ec94ULL}, // istream::read, including gcount and sentry behavior
    {0xfc20, 178, 0xfd467d7871209186ULL},  // streambuf::xsgetn and the buffered get-area layout
};

static bool ForwardingLock(const unsigned char* thunk, const unsigned char* target, const void* nativeLock)
{
    // Both pinned lock wrappers only forward the embedded FILE when nonnull.
    return thunk[0] == 0xe9 && thunk + 5 + Member<int32_t>(thunk, 1) == target && nativeLock &&
           Member<const void*>(target + 22 + Member<int32_t>(target, 18), 0) == nativeLock;
}

static const GameContract* Admit(unsigned char* game, unsigned char* crt)
{
    __try {
        DWORD stamp = 0, imageSize = 0;
        if (!game || !crt || !ReadModuleImageId(reinterpret_cast<HMODULE>(game), &stamp, &imageSize))
            return NULL;
        const GameContract* contract = NULL;
        if (save_gog_native::IsGogImage(stamp, imageSize))
            contract = &gog;
        else if (stamp == 0x65d604d7 && imageSize == 0x232c000)
            contract = &steam;
        if (!contract || !save_gog_native::Match(game, contract->instructions) ||
            !ReadModuleImageId(reinterpret_cast<HMODULE>(crt), &stamp, &imageSize) ||
            stamp != 0x580f7c45 || imageSize != 0x98000 || !save_gog_native::Match(crt, crtProof))
            return NULL;

        const void* nativeRead = crt + 0x21434;
        const void* nativeBulkRead = crt + 0xfc20;
        const unsigned char* thunk = game + contract->bulkReadThunk;
        const unsigned char* const table = game + contract->filebuf;
        const unsigned char* const lock = game + contract->lockThunk;
        const unsigned char* const unlock = game + contract->unlockThunk;
        const HMODULE runtime = GetModuleHandleA("MSVCR100.dll");
        if (Member<const void*>(game, static_cast<int>(contract->readSlot)) != nativeRead ||
            !runtime || Member<const void*>(table, 8) != lock || Member<const void*>(table, 0x10) != unlock ||
            !ForwardingLock(lock, game + contract->instructions[1].rva, reinterpret_cast<const void*>(GetProcAddress(runtime, "_lock_file"))) ||
            !ForwardingLock(unlock, game + contract->instructions[2].rva, reinterpret_cast<const void*>(GetProcAddress(runtime, "_unlock_file"))) ||
            Member<const void*>(table, 0x40) != thunk ||
            thunk[0] != 0xff || thunk[1] != 0x25 ||
            thunk + 6 + Member<int32_t>(thunk, 2) != game + contract->bulkReadSlot ||
            Member<const void*>(game, static_cast<int>(contract->bulkReadSlot)) != nativeBulkRead)
            return NULL;

        binding.readSlot = reinterpret_cast<ReadFn const volatile*>(game + contract->readSlot);
        binding.nativeRead = reinterpret_cast<ReadFn>(crt + 0x21434);
        binding.filebufVtable = table;
        binding.enabled = &fixes::g_fixesCfg.saveReadBufferEnabled;
        binding.ownerThread = GetCurrentThreadId();
        binding.bulkReadSlot = reinterpret_cast<const void* const volatile*>(game + contract->bulkReadSlot);
        binding.nativeBulkRead = nativeBulkRead;
        binding.bulkReadThunk = thunk;
        binding.lockThunk = lock;
        binding.unlockThunk = unlock;
        binding.readEntry = Member<uint64_t>(nativeRead, 0);
        binding.bulkReadEntry = Member<uint64_t>(nativeBulkRead, 0);
        return contract;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return NULL;
    }
}

static_assert(sizeof(std::streamsize) == 8 && sizeof(std::ifstream) == 0x110 && sizeof(std::filebuf) == 0xa0,
              "native x64 VC10 stream layout");

static void Install()
{
    static bool tried = false;
    if (tried || !IsMainThread())
        return;
    tried = true;
    unsigned char* game = reinterpret_cast<unsigned char*>(GetModuleHandleA(NULL));
    unsigned char* crt = reinterpret_cast<unsigned char*>(GetModuleHandleA("MSVCP100.dll"));
    const GameContract* contract = Admit(game, crt);
    const uintptr_t pair = contract ? contract->instructions[0].rva : 0;
    if (!contract || !code_patch_threads::Prepare((GetDLLDirectory() + OPTIMIZER_LOG_NAME).c_str()) ||
        !Arm(game + pair + 17, game + pair + 40))
    {
        LogError("Save transform reading: native parser or stream contract refused");
        return;
    }
    KEO_DETAIL("Save transform reading: paired instance reads admitted");
}
#endif

} // namespace save_transform_read

#ifndef KEO_SAVE_TRANSFORM_TEST
void InstallSaveTransformRead(int*, int*)
{
    save_transform_read::Install();
}
#endif
