#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <sstream>
#include <algorithm>

#ifndef STOCK_TRANSFORM_FIXTURE
#define KEO_SAVE_TRANSFORM_TEST
#include "fixes/world/save_transform_read.cpp"
#endif

typedef void* (*FixtureReadFn)(std::istream*, char*, std::streamsize);
extern "C" void* FixturePair(std::istream*, char*);
extern "C" void* FixtureStockPair(std::istream*, char*);
extern "C" unsigned char FixturePositionCall, FixtureRotationCall;
extern "C" void* FixtureHeaderPair(std::istream*, char*);
extern "C" void* FixtureStockHeaderPair(std::istream*, char*);
extern "C" unsigned char FixtureTypeCall, FixtureIdCall;
extern "C" void* FixtureReferencePair(std::istream*, char*);
extern "C" void* FixtureStockReferencePair(std::istream*, char*);
extern "C" unsigned char FixtureRefStartCall, FixtureRefEndCall;

enum FixtureKind { FK_TRANSFORM, FK_HEADER, FK_REFERENCE, FK_COUNT };
typedef void* (*PairFn)(std::istream*, char*);
struct FixtureCase {
    PairFn patched, stock;
    int offset, firstBytes, secondBytes;
};
static const FixtureCase cases[] = {
    { &FixturePair, &FixtureStockPair, 4, 12, 16 },
    { &FixtureHeaderPair, &FixtureStockHeaderPair, 24, 4, 4 },
    { &FixtureReferencePair, &FixtureStockReferencePair, 4, 4, 4 },
};

static int checks, reads;
static std::streamsize sizes[4];
static bool armDuringStockRead = false;

static void Check(bool yes, const char* name)
{
    ++checks;
    if (!yes) { std::printf("FAIL %s\n", name); std::exit(1); }
}

static void* Read(std::istream* stream, char* out, std::streamsize size)
{
    if (reads < 4) sizes[reads] = size;
    ++reads;
    void* result = &stream->read(out, size);
#ifndef STOCK_TRANSFORM_FIXTURE
    if (armDuringStockRead && reads == 1)
        save_transform_read::admitted = true;
#endif
    return result;
}

extern "C" { FixtureReadFn FixtureRead = &Read; }

struct Result {
    char bytes[48];
    std::streamsize count;
    std::ios::iostate state;
    std::streamoff end;
    bool threw;
    int calls;
    std::streamsize request[2];
};

static Result Capture(std::istream& stream, bool stock = false, int kind = FK_TRANSFORM)
{
    Result result = {};
    std::memset(result.bytes, 0xa5, sizeof(result.bytes));
    reads = 0;
    sizes[0] = sizes[1] = 0;
    try {
        const FixtureCase& fixture = cases[kind];
        const PairFn pair = stock ? fixture.stock : fixture.patched;
        const void* returned = pair(&stream, result.bytes + fixture.offset);
        Check(returned == &stream, "returns the original stream");
    }
    catch (const std::ios_base::failure&) { result.threw = true; }
    result.count = stream.gcount();
    result.state = stream.rdstate();
    result.calls = reads;
    result.request[0] = sizes[0];
    result.request[1] = sizes[1];
    stream.clear();
    result.end = stream.tellg();
    return result;
}

static void Same(const Result& stock, const Result& candidate)
{
    Check(!std::memcmp(stock.bytes, candidate.bytes, sizeof(stock.bytes)), "identical bytes and untouched padding");
    Check(stock.count == candidate.count && stock.state == candidate.state && stock.end == candidate.end &&
          stock.threw == candidate.threw, "same flags, gcount, cursor and exception behavior");
}

static Result RunFile(const wchar_t* path, std::streamoff offset, bool warm, bool throwOnFailure, bool stock = false, int kind = FK_TRANSFORM)
{
    std::ifstream stream(path, std::ios::binary);
    Check(stream.good(), "open actual input read-only");
    stream.seekg(offset - (warm ? 1 : 0));
    if (warm) {
        char ignored = 0;
        stream.read(&ignored, 1);
    }
    Check(stream.good(), "seek to the first field");
    if (throwOnFailure) stream.exceptions(std::ios::failbit | std::ios::badbit);
    return Capture(stream, stock, kind);
}

static void PairOfReads(const Result& result, int kind = FK_TRANSFORM)
{
    Check(result.calls == 2 && result.request[0] == cases[kind].firstBytes &&
          result.request[1] == cases[kind].secondBytes, "split native reads");
}

static void BatchRead(const Result& result, int kind = FK_TRANSFORM)
{
    Check(result.calls == 1 && result.request[0] == cases[kind].firstBytes + cases[kind].secondBytes,
          kind == FK_TRANSFORM ? "one 28-byte native read" : "one 8-byte native read");
}

#ifndef STOCK_TRANSFORM_FIXTURE
static void ArmFixture()
{
    const save_transform_read::CallSites sites[] = {
        { &FixturePositionCall, &FixtureRotationCall },
        { &FixtureTypeCall, &FixtureIdCall },
        { &FixtureRefStartCall, &FixtureRefEndCall },
    };
    Check(save_transform_read::Arm(sites), "arm all fixture call pairs");
}

static void BindFixture(std::ifstream& stream, FixtureReadFn nativeRead)
{
    using namespace save_transform_read;
    static const void* liveBulkRead;
    binding.filebufVtable = Member<const void*>(stream.rdbuf(), 0);
    binding.readSlot = &FixtureRead;
    binding.nativeRead = nativeRead;
    binding.enabled = &testEnabled;
    binding.ownerThread = GetCurrentThreadId();
    binding.bulkReadThunk = Member<const void*>(binding.filebufVtable, 0x40);
    binding.lockThunk = Member<const void*>(binding.filebufVtable, 8);
    binding.unlockThunk = Member<const void*>(binding.filebufVtable, 0x10);
    liveBulkRead = binding.bulkReadThunk;
    binding.bulkReadSlot = &liveBulkRead;
    binding.nativeBulkRead = liveBulkRead;
    binding.readEntry = Member<uint64_t>(reinterpret_cast<const void*>(nativeRead), 0);
    binding.bulkReadEntry = Member<uint64_t>(liveBulkRead, 0);
}

static double TimedRead(const wchar_t* path, int blocks, bool stock, int kind)
{
    std::ifstream stream(path, std::ios::binary);
    char ignored = 0, output[48];
    stream.read(&ignored, 1);
    const FixtureCase& fixture = cases[kind];
    const PairFn pair = stock ? fixture.stock : fixture.patched;
    LARGE_INTEGER frequency, begin, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&begin);
    for (int i = 0; i < blocks; ++i) pair(&stream, output + fixture.offset);
    QueryPerformanceCounter(&end);
    Check(stream.good() && stream.gcount() == fixture.secondBytes && static_cast<std::streamoff>(stream.tellg()) ==
          1 + static_cast<std::streamoff>(fixture.firstBytes + fixture.secondBytes) * blocks,
          "timed file ends at the expected position");
    return (end.QuadPart - begin.QuadPart) * 1e9 / frequency.QuadPart / blocks;
}

static void Benchmark(int kind)
{
    const wchar_t* path = L"transform-benchmark.bin";
    const int blocks = 65536, trials = 21;
    char data[1792] = {};
    {
        std::ofstream file(path, std::ios::binary);
        file.put(0);
        const int bytes = cases[kind].firstBytes + cases[kind].secondBytes;
        for (int i = 0; i < blocks / 64; ++i) file.write(data, bytes * 64);
    }
    FixtureRead = reinterpret_cast<FixtureReadFn>(GetProcAddress(GetModuleHandleA("MSVCP100.dll"),
        "?read@?$basic_istream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@PEAD_J@Z"));
    Check(FixtureRead != NULL, "actual VC10 reader for benchmark");
    std::ifstream stream(path, std::ios::binary);
    BindFixture(stream, FixtureRead);
    ArmFixture();
    double stock[trials], active[trials], disabled[trials];
    for (int i = -2; i < trials; ++i) {
        double ns[3];
        for (int j = 0; j < 3; ++j) {
            const int mode = (i + j + 3) % 3;
            save_transform_read::testEnabled = mode == 1;
            ns[mode] = TimedRead(path, blocks, mode == 0, kind);
        }
        if (i >= 0) { stock[i] = ns[0]; active[i] = ns[1]; disabled[i] = ns[2]; }
    }
    std::sort(stock, stock + trials);
    std::sort(active, active + trials);
    std::sort(disabled, disabled + trials);
    std::printf("Pair %d median ns: native %.2f, paired %.2f, disabled %.2f\n", kind,
                stock[trials / 2], active[trials / 2], disabled[trials / 2]);
}
#endif

int wmain(int argc, wchar_t** argv)
{
#ifndef STOCK_TRANSFORM_FIXTURE
    const int kindToTest = argc > 1 ? _wtoi(argv[1]) : FK_TRANSFORM;
    Check(kindToTest >= 0 && kindToTest < FK_COUNT, "known pair selector");
    if (argc == 3 && !std::wcscmp(argv[2], L"--bench")) { Benchmark(kindToTest); return 0; }
#endif
    const wchar_t* path = L"transform-test.bin";
    char data[2048];
    for (unsigned i = 0; i < sizeof(data); ++i) data[i] = static_cast<char>(i);
    { std::ofstream file(path, std::ios::binary); file.write(data, sizeof(data)); }
    const Result baseline = RunFile(path, 1, true, false);
    PairOfReads(baseline);
    Check(argc == 1 || (argc - 2) % 3 == 0, "provide a pair selector followed by file/offset/size triples");
    for (int i = 2; i < argc; i += 3) Check(_wtoi(argv[i + 2]) >= 0, "nonnegative retained bytes");
#ifndef STOCK_TRANSFORM_FIXTURE
    std::ifstream stream(path, std::ios::binary);
    BindFixture(stream, &Read);
    ArmFixture();
#endif
    const Result active = RunFile(path, 1, true, false);
    Same(baseline, active);
    BatchRead(active);
    for (int kind = FK_HEADER; kind < FK_COUNT; ++kind) {
        Same(RunFile(path, 1, true, false, true, kind), RunFile(path, 1, true, false, false, kind));
        BatchRead(RunFile(path, 1, true, false, false, kind), kind);
    }
#ifndef STOCK_TRANSFORM_FIXTURE
    save_transform_read::admitted = false;
    armDuringStockRead = true;
    const Result inFlight = RunFile(path, 1, true, false);
    armDuringStockRead = false;
    Check(save_transform_read::admitted, "admission published between native reads");
    Same(baseline, inFlight);
    PairOfReads(inFlight);
    save_transform_read::testEnabled = false;
    const Result disabled = RunFile(path, 1, true, false);
    save_transform_read::testEnabled = true;
    Same(baseline, disabled);
    PairOfReads(disabled);
    const DWORD ownerThread = save_transform_read::binding.ownerThread;
    save_transform_read::binding.ownerThread = 0;
    const Result foreignThread = RunFile(path, 1, true, false);
    save_transform_read::binding.ownerThread = ownerThread;
    Same(baseline, foreignThread);
    PairOfReads(foreignThread);
    ++save_transform_read::patched[save_transform_read::PK_TRANSFORM].displacement;
    const Result replacedRotation = RunFile(path, 1, true, false);
    --save_transform_read::patched[save_transform_read::PK_TRANSFORM].displacement;
    Same(baseline, replacedRotation);
    PairOfReads(replacedRotation);
    const Result cold = RunFile(path, 1, false, false);
    Same(baseline, cold);
    PairOfReads(cold);
    std::istringstream foreign(std::string(data + 1, 28));
    const Result foreignResult = Capture(foreign);
    PairOfReads(foreignResult);
    Check(!std::memcmp(foreignResult.bytes, baseline.bytes, sizeof(baseline.bytes)), "foreign stream keeps its native output");

    int fullBlocks = 0, cutBlocks = 0;
    const int width = cases[kindToTest].firstBytes + cases[kindToTest].secondBytes;
    for (int i = 2; i < argc; i += 3) {
        const std::streamoff offset = _wtoi64(argv[i + 1]);
        const int retained = _wtoi(argv[i + 2]);
        const Result stock = RunFile(argv[i], offset, true, false, true, kindToTest);
        const Result patched = RunFile(argv[i], offset, true, false, false, kindToTest);
        Same(stock, patched);
        PairOfReads(stock, kindToTest);
        if (retained >= width) {
            Check(stock.state == std::ios::goodbit && stock.count == cases[kindToTest].secondBytes, "complete old pair");
            BatchRead(patched, kindToTest);
            ++fullBlocks;
        } else {
            Check(stock.state == (std::ios::failbit | std::ios::eofbit), "truncated pair reports native EOF and fail");
            PairOfReads(patched, kindToTest);
            ++cutBlocks;
            const Result stockThrows = RunFile(argv[i], offset, true, true, true, kindToTest);
            const Result patchedThrows = RunFile(argv[i], offset, true, true, false, kindToTest);
            Same(stockThrows, patchedThrows);
            Check(patchedThrows.threw, "native short read exception crosses the real callsite");
        }
    }
    std::printf("Pair %d: %d complete old blocks, %d truncated blocks\n", kindToTest, fullBlocks, cutBlocks);
#endif
    std::printf("PASS %d save read checks\n", checks);
}
