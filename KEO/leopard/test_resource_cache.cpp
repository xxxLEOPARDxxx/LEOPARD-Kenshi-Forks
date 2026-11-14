#include "resources/resource_cache_store.h"
#include <windows.h>
#include <cassert>
#include <fstream>
#include <iostream>
static volatile LONG lowMemory=0;
static BOOL MemoryProbe(LPMEMORYSTATUSEX m){
    if(!InterlockedCompareExchange(&lowMemory,0,0))return GlobalMemoryStatusEx(m);
    m->ullTotalPhys=16ULL*1024*1024*1024;m->ullAvailPhys=512ULL*1024*1024;m->ullAvailPageFile=8ULL*1024*1024*1024;return TRUE;
}
// Supply only the memory-pressure input; exercise the real store/worker code.
#define GlobalMemoryStatusEx MemoryProbe
#include "../src/resources/resource_cache_store.cpp"
#undef GlobalMemoryStatusEx
using namespace resource_cache;
static void writeFile(const std::string& p, const char* s) {
    std::ofstream out(p.c_str(), std::ios::binary|std::ios::trunc); out << s;
}
static FilePtr awaitFile(const std::string& p) {
    for (int i=0;i<150;++i) {FilePtr f=Find(p);if(f)return f;Sleep(20);} return FilePtr();
}
int main() {
    char dir[MAX_PATH]; GetFullPathNameA("build\\resource_cache_test\\",MAX_PATH,dir,NULL);
    char pid[32];sprintf_s(pid,"%lu",GetCurrentProcessId());
    std::string p=std::string(dir)+"keo-cache-test-"+pid+".dds";
    writeFile(p,"original bytes");
    assert(Start()); Configure(true,1024*1024,64); Observe(p);
    FilePtr f=awaitFile(p); assert(f && f->data.size()==14 && f->data[0]=='o');
    FilePtr f2=Find(p); assert(f2==f); // same bytes, independent stream positions live above this API
    Sleep(25);writeFile(p,"changed bytes, different size");
    assert(!Find(p)); // never give old bytes to the engine after a file edit
    Observe(p);FilePtr changed=awaitFile(p);assert(changed && changed->data.size()==29);
    Configure(false,1024*1024,64);
    assert(!Find(p)); // immediate off, even before worker frees index
    assert(f->data[0]=='o'); // eviction/off cannot invalidate an engine reader
    for(int i=0;i<100 && Snapshot().entries;++i)Sleep(20);
    assert(Snapshot().entries==0);
    f.reset();f2.reset();changed.reset();
    Configure(true,8,64);Observe(p);Sleep(100);assert(!Find(p));
    assert(Snapshot().allocatedBytes==0); // no oversize allocation
    Configure(true,1024*1024,64);Observe(p);changed=awaitFile(p);assert(changed);
    InterlockedExchange(&lowMemory,1);
    changed.reset();
    for(int i=0;i<100 && Snapshot().entries;++i)Sleep(20);
    assert(Snapshot().entries==0 && Snapshot().allocatedBytes==0); // trim already-filled cache
    Observe(p);Sleep(100);assert(!Find(p)); // no refill while pressure persists
    InterlockedExchange(&lowMemory,0);
    // Byte budget includes readers even after their index entry is evicted.
    Configure(true,40,64);Observe(p);changed=awaitFile(p);assert(changed);
    std::string second=p+".dds";writeFile(second,"other original payload, 29byte");
    Observe(second);Sleep(150);assert(!Find(second));assert(Snapshot().allocatedBytes==29);
    changed.reset();Observe(second);FilePtr replacement=awaitFile(second);assert(replacement);
    replacement.reset();
    // Atomic replacement with same size and timestamp must invalidate the cache.
    HANDLE old=CreateFileA(second.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
    FILETIME stamp;assert(GetFileTime(old,NULL,NULL,&stamp));CloseHandle(old);
    std::string swap=second+".new";writeFile(swap,"NEWER original payload, 29byte");
    HANDLE fresh=CreateFileA(swap.c_str(),FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(SetFileTime(fresh,NULL,NULL,&stamp));CloseHandle(fresh);
    bool moved=false;for(int i=0;i<100&&!moved;++i){moved=MoveFileExA(swap.c_str(),second.c_str(),MOVEFILE_REPLACE_EXISTING)!=FALSE;if(!moved)Sleep(10);}
    if(!moved)std::cerr<<"replace error "<<GetLastError()<<" path="<<second<<"\n";
    assert(moved);assert(!Find(second));
    Configure(true,16*1024*1024,1);
    {std::ofstream large(p.c_str(),std::ios::binary|std::ios::trunc);std::string megabyte(1024*1024,'x');for(int i=0;i<8;++i)large<<megabyte;}
    Observe(p);Sleep(60);DWORD begin=GetTickCount();Configure(false,16*1024*1024,1);
    for(int i=0;i<100 && Snapshot().allocatedBytes;++i)Sleep(10);
    assert(Snapshot().allocatedBytes==0 && GetTickCount()-begin<1000); // cancels in-progress read/pacing
    DeleteFileA(second.c_str());
    Stop(); assert(!Find(p)); DeleteFileA(p.c_str());
    std::cout<<"resource cache IO/lifetime tests passed\n";
}
