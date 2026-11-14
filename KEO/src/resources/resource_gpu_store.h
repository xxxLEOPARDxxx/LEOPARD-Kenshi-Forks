#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <memory>
#include <string>
#include <vector>
#include "resources/resource_cache_store.h"
#include "resources/resource_dds.h"
namespace resource_gpu {
typedef unsigned __int64 U64;
struct Span {const unsigned char* data;size_t bytes;unsigned rowPitch,width,height;};
struct Job {std::string key,paths[6];U64 worldGeneration;unsigned type;Job():worldGeneration(0),type(0){}};
struct Ready;
typedef std::shared_ptr<Ready> ReadyPtr;
struct Ready {
    // Immutable after publication, except MarkAdopted detaches our GPU reference.
    std::string key;U64 worldGeneration,gpuBytes;
    D3D11_TEXTURE2D_DESC desc;
    resource_cache::FilePtr sources[6];std::vector<Span> subresources;
    ID3D11Texture2D* AddRefTexture() const; // owned result, main thread
    Ready();~Ready();
private:
    ID3D11Texture2D* texture_;bool charged_;
    Ready(const Ready&);Ready& operator=(const Ready&);
    friend bool BuildTexture(ID3D11Device*,const Job&,const resource_cache::FilePtr*,ReadyPtr*,std::string*);
    friend void MarkAdopted(const ReadyPtr&);
    friend struct StoreAccess;
};
// Reuses the exact production decoder/initial-data construction; no immediate
// context calls. Caller owns device and source lifetimes. Worker must admit the
// device and reserve budget BEFORE this indivisible driver call. Test fixtures
// can call this on their main thread, including WARP without concurrent creation.
bool BuildTexture(ID3D11Device*,const Job&,const resource_cache::FilePtr[6],ReadyPtr*,std::string* error);
bool DeviceCanCreateConcurrently(ID3D11Device*);
typedef bool (*BudgetProbe)(ID3D11Device*,U64* budget,U64* usage,void* context);
bool ProbeLocalBudget(ID3D11Device*,U64*,U64*,void*);
bool BudgetRoom(U64 budget,U64 usage,U64 extra);
bool CacheRoom(U64 cap,U64 allocated,U64 extra); // checked cap arithmetic shared by reservations/tests
struct Stats {U64 allocatedBytes,reservedBytes,created,rejected,retries;unsigned entries,pending;bool deviceReady,pressure,active;};
bool Start(); // main thread, before configuration; pins worker's module
bool SetDevice(ID3D11Device*); // main thread; holds AddRef, rejects SINGLETHREADED/nonconcurrent
void SetBudgetProbe(BudgetProbe,void*); // main thread; context must outlive Stop completion
void Configure(bool enabled,U64 bytes,U64 worldGeneration); // cap 1..8GiB, live, invalidates jobs
void SetWorkAllowed(bool); // atomic admission gate, initially false; preserves queues/cache
bool Queue(const Job&); // main thread; nonwaiting, <=32 queued, <=128 standby
ReadyPtr TakeReady(const std::string& key,U64 worldGeneration); // nonwaiting; transfers standby pin
void MarkAdopted(const ReadyPtr&); // after native received AddRef: detach our GPU pin/accounting
Stats Snapshot();
bool TakeError(std::string*); // main tick must log these failures even when detailed logging is off
void SignalStop(); // DllMain safe: flags/event only
bool Stop(unsigned timeoutMs=250); // bounded; false leaves pinned worker + event alive to drain
}
