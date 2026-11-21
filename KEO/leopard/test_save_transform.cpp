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
    if (armDuringStockRead && size == 12)
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

static Result Capture(std::istream& stream, bool stock = false)
{
    Result result = {};
    std::memset(result.bytes, 0xa5, sizeof(result.bytes));
    reads = 0;
    sizes[0] = sizes[1] = 0;
    try {
        const void* returned = stock ? FixtureStockPair(&stream, result.bytes + 4) : FixturePair(&stream, result.bytes + 4);
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

static Result RunFile(const wchar_t* path, std::streamoff offset, bool warm, bool throwOnFailure, bool stock = false)
{
    std::ifstream stream(path, std::ios::binary);
    Check(stream.good(), "open actual input read-only");
    stream.seekg(offset - (warm ? 1 : 0));
    if (warm) {
        char ignored = 0;
        stream.read(&ignored, 1);
    }
    Check(stream.good(), "seek to the instance transform");
    if (throwOnFailure) stream.exceptions(std::ios::failbit | std::ios::badbit);
    return Capture(stream, stock);
}

static void PairOfReads(const Result& result)
{
    Check(result.calls == 2 && result.request[0] == 12 && result.request[1] == 16, "split native 12/16 reads");
}

static void BatchRead(const Result& result)
{
    Check(result.calls == 1 && result.request[0] == 28, "one 28-byte native read");
}

#ifndef STOCK_TRANSFORM_FIXTURE
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

static double TimedRead(const wchar_t* path, int blocks, bool stock)
{
    std::ifstream stream(path, std::ios::binary);
    char ignored = 0, output[40];
    stream.read(&ignored, 1);
    void* (*pair)(std::istream*, char*) = stock ? &FixtureStockPair : &FixturePair;
    LARGE_INTEGER frequency, begin, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&begin);
    for (int i = 0; i < blocks; ++i) pair(&stream, output);
    QueryPerformanceCounter(&end);
    Check(stream.good() && stream.gcount() == 16 && static_cast<std::streamoff>(stream.tellg()) == 1 + 28LL * blocks,
          "timed file ends at the expected position");
    return (end.QuadPart - begin.QuadPart) * 1e9 / frequency.QuadPart / blocks;
}

static void Benchmark()
{
    const wchar_t* path = L"transform-benchmark.bin";
    const int blocks = 65536, trials = 21;
    char data[1792] = {};
    {
        std::ofstream file(path, std::ios::binary);
        file.put(0);
        for (int i = 0; i < blocks / 64; ++i) file.write(data, sizeof(data));
    }
    FixtureRead = reinterpret_cast<FixtureReadFn>(GetProcAddress(GetModuleHandleA("MSVCP100.dll"),
        "?read@?$basic_istream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@PEAD_J@Z"));
    Check(FixtureRead != NULL, "actual VC10 reader for benchmark");
    std::ifstream stream(path, std::ios::binary);
    BindFixture(stream, FixtureRead);
    Check(save_transform_read::Arm(&FixturePositionCall, &FixtureRotationCall), "arm actual benchmark stubs");
    double stock[trials], active[trials], disabled[trials];
    for (int i = -2; i < trials; ++i) {
        double ns[3];
        for (int j = 0; j < 3; ++j) {
            const int mode = (i + j + 3) % 3;
            save_transform_read::testEnabled = mode == 1;
            ns[mode] = TimedRead(path, blocks, mode == 0);
        }
        if (i >= 0) { stock[i] = ns[0]; active[i] = ns[1]; disabled[i] = ns[2]; }
    }
    std::sort(stock, stock + trials);
    std::sort(active, active + trials);
    std::sort(disabled, disabled + trials);
    std::printf("Median nanoseconds per transform: native %.2f, paired %.2f, disabled %.2f\n",
                stock[trials / 2], active[trials / 2], disabled[trials / 2]);
}
#endif

int wmain(int argc, wchar_t** argv)
{
#ifndef STOCK_TRANSFORM_FIXTURE
    if (argc == 2 && !std::wcscmp(argv[1], L"--bench")) { Benchmark(); return 0; }
#endif
    const wchar_t* path = L"transform-test.bin";
    char data[2048];
    for (unsigned i = 0; i < sizeof(data); ++i) data[i] = static_cast<char>(i);
    { std::ofstream file(path, std::ios::binary); file.write(data, sizeof(data)); }
    const Result baseline = RunFile(path, 1, true, false);
    PairOfReads(baseline);
    Check((argc - 1) % 3 == 0, "provide the file, transform offset and retained bytes in triples");
    for (int i = 1; i < argc; i += 3) Check(_wtoi(argv[i + 2]) >= 0, "nonnegative retained bytes");
#ifndef STOCK_TRANSFORM_FIXTURE
    std::ifstream stream(path, std::ios::binary);
    BindFixture(stream, &Read);
    Check(save_transform_read::Arm(&FixturePositionCall, &FixtureRotationCall), "arm both fixture CALLs");
#endif
    const Result active = RunFile(path, 1, true, false);
    Same(baseline, active);
    BatchRead(active);
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
    ++save_transform_read::rotationCallDisplacement;
    const Result replacedRotation = RunFile(path, 1, true, false);
    --save_transform_read::rotationCallDisplacement;
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
    for (int i = 1; i < argc; i += 3) {
        const std::streamoff offset = _wtoi64(argv[i + 1]);
        const int retained = _wtoi(argv[i + 2]);
        const Result stock = RunFile(argv[i], offset, true, false, true);
        const Result patched = RunFile(argv[i], offset, true, false);
        Same(stock, patched);
        PairOfReads(stock);
        if (retained >= 28) {
            Check(stock.state == std::ios::goodbit && stock.count == 16, "complete old transform");
            BatchRead(patched);
            ++fullBlocks;
        } else {
            Check(stock.state == (std::ios::failbit | std::ios::eofbit), "truncated transform reports native EOF and fail");
            PairOfReads(patched);
            ++cutBlocks;
            const Result stockThrows = RunFile(argv[i], offset, true, true, true);
            const Result patchedThrows = RunFile(argv[i], offset, true, true);
            Same(stockThrows, patchedThrows);
            Check(patchedThrows.threw, "native short read exception crosses the real callsite");
        }
    }
    std::printf("%d complete old blocks, %d truncated blocks\n", fullBlocks, cutBlocks);
#endif
    std::printf("PASS %d transform checks\n", checks);
}
