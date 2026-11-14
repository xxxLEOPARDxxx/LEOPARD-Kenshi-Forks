#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "resources/resource_cache_store.h"
#include <map>
#include <deque>
#include <algorithm>
#include <cstring>
namespace resource_cache {
namespace {
const size_t MaxEntries=4096, MaxQueue=128, Chunk=256*1024;
const unsigned __int64 MaxFile=256ULL*1024*1024, MiB=1024ULL*1024;
__declspec(align(8)) volatile LONGLONG allocated=0,hits=0,misses=0,queued=0,rejected=0,readBytes=0;
CRITICAL_SECTION lock;
bool initialized=false;
HANDLE eventHandle=NULL, threadHandle=NULL;
volatile LONG stopping=0, enabled=0, generation=0, rate=32;
volatile LONG backgroundAllowed=1;
__declspec(align(8)) volatile LONGLONG ceiling=0;
struct Entry {FilePtr file;unsigned __int64 touch;};
std::map<std::string,Entry> files;
std::deque<std::string> requests;
std::string active;
unsigned __int64 clockValue=0;
struct Guard {Guard(){EnterCriticalSection(&lock);}~Guard(){LeaveCriticalSection(&lock);}};
LONG Load(volatile LONG& v){return InterlockedCompareExchange(&v,0,0);}
LONGLONG Load64(volatile LONGLONG&v){return InterlockedCompareExchange64(&v,0,0);}
bool On(){return Load(enabled)!=0 && Load(stopping)==0;}
bool Eligible(const std::string&p){
    if(p.size()<7||p.size()>=MAX_PATH||p[1]!=':'||(p[2]!='\\'&&p[2]!='/'))return false;
    size_t at=p.find_last_of('.');if(at==std::string::npos)return false;
    return !_stricmp(p.c_str()+at,".dds")||!_stricmp(p.c_str()+at,".mesh")||!_stricmp(p.c_str()+at,".skeleton");
}
bool Info(const std::string&p,WIN32_FILE_ATTRIBUTE_DATA&i){
    return GetFileAttributesExA(p.c_str(),GetFileExInfoStandard,&i)!=FALSE && !(i.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY);
}
unsigned __int64 Size(const WIN32_FILE_ATTRIBUTE_DATA&i){return (unsigned __int64(i.nFileSizeHigh)<<32)|i.nFileSizeLow;}
bool Matches(const CachedFile&f,const WIN32_FILE_ATTRIBUTE_DATA&i){return f.sizeHigh==i.nFileSizeHigh&&f.sizeLow==i.nFileSizeLow&&f.timeHigh==i.ftLastWriteTime.dwHighDateTime&&f.timeLow==i.ftLastWriteTime.dwLowDateTime;}
bool Identity(const std::string&p,const CachedFile&f){
    HANDLE h=CreateFileA(p.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
    if(h==INVALID_HANDLE_VALUE)return false;
    BY_HANDLE_FILE_INFORMATION i;bool ok=GetFileInformationByHandle(h,&i)!=FALSE;CloseHandle(h);
    return ok&&f.volume==i.dwVolumeSerialNumber&&f.indexHigh==i.nFileIndexHigh&&f.indexLow==i.nFileIndexLow
        &&f.sizeHigh==i.nFileSizeHigh&&f.sizeLow==i.nFileSizeLow&&f.timeHigh==i.ftLastWriteTime.dwHighDateTime&&f.timeLow==i.ftLastWriteTime.dwLowDateTime;
}
bool Cancelled(LONG epoch){return !On()||Load(generation)!=epoch;}
bool MemoryRoom(unsigned __int64 bytes){
    MEMORYSTATUSEX m={sizeof(m)};
    if(!GlobalMemoryStatusEx(&m))return false;
    unsigned __int64 reserve=(std::max)(2ULL*1024*MiB,m.ullTotalPhys/5);
    return m.ullAvailPhys>reserve && bytes<=m.ullAvailPhys-reserve && m.ullAvailPageFile>bytes+256*MiB;
}
void Clear(){Guard g;files.clear();requests.clear();}
void TrimPressure(){
    if(MemoryRoom(0))return;
    Guard g;
    for(std::map<std::string,Entry>::iterator it=files.begin();it!=files.end();){
        if(it->second.file.use_count()==1)files.erase(it++);else ++it;
    }
}
bool MakeRoom(unsigned __int64 bytes){
    Guard g;
    unsigned __int64 cap=unsigned __int64(Load64(ceiling));
    if(bytes>cap)return false;
    while(!files.empty()&&(unsigned __int64(Load64(allocated))>cap-bytes||files.size()>=MaxEntries)){
        std::map<std::string,Entry>::iterator oldest=files.end();
        for(std::map<std::string,Entry>::iterator it=files.begin();it!=files.end();++it)
            if(it->second.file.use_count()==1&&(oldest==files.end()||it->second.touch<oldest->second.touch))oldest=it;
        if(oldest==files.end())break; // in-use streams count toward cap and remain valid
        files.erase(oldest);
    }
    return unsigned __int64(Load64(allocated))<=cap-bytes && files.size()<MaxEntries;
}
struct Handle {HANDLE h;Handle(HANDLE v):h(v){}~Handle(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}};
void ReadOne(const std::string&p,LONG epoch){
    char root[4]={p[0],':','\\',0};if(GetDriveTypeA(root)!=DRIVE_FIXED)return;
    WIN32_FILE_ATTRIBUTE_DATA before;
    if(Cancelled(epoch)||!Info(p,before))return;
    unsigned __int64 bytes=Size(before);
    if(!bytes||bytes>MaxFile||!MemoryRoom(bytes)||!MakeRoom(bytes)){InterlockedIncrement64(&rejected);return;}
    Handle h(CreateFileA(p.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL));
    if(h.h==INVALID_HANDLE_VALUE)return; // content can be in use by a writer: native remains authoritative
    BY_HANDLE_FILE_INFORMATION identity;
    if(!GetFileInformationByHandle(h.h,&identity))return;
    if(identity.nFileSizeHigh!=before.nFileSizeHigh||identity.nFileSizeLow!=before.nFileSizeLow
        ||identity.ftLastWriteTime.dwHighDateTime!=before.ftLastWriteTime.dwHighDateTime
        ||identity.ftLastWriteTime.dwLowDateTime!=before.ftLastWriteTime.dwLowDateTime)return;
    std::shared_ptr<CachedFile> f(new CachedFile);
    f->charged=bytes;InterlockedExchangeAdd64(&allocated,LONGLONG(bytes));
    f->data.resize(size_t(bytes));
    f->sizeHigh=before.nFileSizeHigh;f->sizeLow=before.nFileSizeLow;
    f->timeHigh=before.ftLastWriteTime.dwHighDateTime;f->timeLow=before.ftLastWriteTime.dwLowDateTime;
    f->volume=identity.dwVolumeSerialNumber;f->indexHigh=identity.nFileIndexHigh;f->indexLow=identity.nFileIndexLow;
    size_t offset=0;
    while(offset<f->data.size()){
        if(Cancelled(epoch)||!MemoryRoom(0))return;
        while(!Load(backgroundAllowed)&&!Cancelled(epoch)){if(!MemoryRoom(0))return;Sleep(10);}
        if(Cancelled(epoch))return;
        DWORD count=DWORD((std::min)(Chunk,f->data.size()-offset)),got=0;
        DWORD start=GetTickCount();
        if(!ReadFile(h.h,&f->data[offset],count,&got,NULL)||got!=count)return;
        offset+=got;InterlockedExchangeAdd64(&readBytes,got);
        unsigned speed=(std::max)(1L,Load(rate));
        DWORD span=DWORD((unsigned __int64(count)*1000+speed*MiB-1)/(speed*MiB));
        DWORD elapsed=GetTickCount()-start;
        if(span>elapsed){ // bounded interruptible pacing, no unbounded full-file IO burst
            DWORD remaining=span-elapsed;
            while(remaining&&!Cancelled(epoch)){DWORD wait=(std::min)(remaining,DWORD(10));Sleep(wait);remaining-=wait;}
        }
    }
    WIN32_FILE_ATTRIBUTE_DATA after;
    if(Cancelled(epoch)||!Info(p,after)||!Matches(*f,after))return;
    Guard g;
    if(Cancelled(epoch)||unsigned __int64(Load64(allocated))>unsigned __int64(Load64(ceiling)))return;
    Entry e={f,++clockValue};files[p]=e;
}
DWORD WINAPI Worker(void*){
    SetThreadPriority(GetCurrentThread(),THREAD_MODE_BACKGROUND_BEGIN);
    for(;;){
        WaitForSingleObject(eventHandle,250);
        if(Load(stopping))break;
        try{
            if(!On()){Clear();continue;}
            TrimPressure();
            MakeRoom(0);
            if(!Load(backgroundAllowed))continue;
            std::string p;LONG epoch=Load(generation);
            {Guard g;if(!requests.empty()){p=requests.front();requests.pop_front();active=p;}}
            if(!p.empty())ReadOne(p,epoch);
            {Guard g;active.clear();if(!requests.empty())SetEvent(eventHandle);}
        }catch(...){InterlockedIncrement64(&rejected);Guard g;active.clear();}
    }
    Clear();return 0;
}
}
CachedFile::CachedFile():sizeHigh(0),sizeLow(0),timeHigh(0),timeLow(0),volume(0),indexHigh(0),indexLow(0),charged(0){}
CachedFile::~CachedFile(){std::vector<unsigned char>().swap(data);if(charged)InterlockedExchangeAdd64(&allocated,-LONGLONG(charged));}
bool Start(){
    if(threadHandle)return true;
    if(!initialized){InitializeCriticalSection(&lock);initialized=true;}
    if(!eventHandle)eventHandle=CreateEventA(NULL,FALSE,FALSE,NULL);
    if(!eventHandle)return false;ResetEvent(eventHandle);
    InterlockedExchange(&stopping,0);
    threadHandle=CreateThread(NULL,0,Worker,NULL,0,NULL);
    if(!threadHandle){CloseHandle(eventHandle);eventHandle=NULL;return false;}return true;
}
void Configure(bool on,unsigned __int64 bytes,unsigned speed){
    if(!initialized)return;
    bool change=(Load(enabled)!=int(on))||unsigned __int64(Load64(ceiling))!=bytes;
    InterlockedExchange64(&ceiling,LONGLONG((std::min)(bytes,32ULL*1024*MiB)));
    InterlockedExchange(&rate,LONG((std::max)(1u,(std::min)(speed,128u))));
    InterlockedExchange(&enabled,on?1:0);
    if(change)InterlockedIncrement(&generation);
    SetEvent(eventHandle);
}
FilePtr Find(const std::string&p){
    if(!On()||!Eligible(p))return FilePtr();
    FilePtr f;
    if(TryEnterCriticalSection(&lock)){
        std::map<std::string,Entry>::iterator it=files.find(p);
        if(it!=files.end()){f=it->second.file;it->second.touch=++clockValue;}
        LeaveCriticalSection(&lock);
    }
    if(f&&Identity(p,*f)){InterlockedIncrement64(&hits);return f;}
    if(f&&TryEnterCriticalSection(&lock)){
        std::map<std::string,Entry>::iterator it=files.find(p);
        if(it!=files.end()&&it->second.file==f)files.erase(it);
        LeaveCriticalSection(&lock);
    }
    InterlockedIncrement64(&misses);return FilePtr();
}
void Observe(const std::string&p){
    if(!On()||!Eligible(p)||!TryEnterCriticalSection(&lock))return;
    try{
        if(On()&&requests.size()<MaxQueue && files.find(p)==files.end() && active!=p && std::find(requests.begin(),requests.end(),p)==requests.end()){
            requests.push_back(p);InterlockedIncrement64(&queued);SetEvent(eventHandle);
        }
    }catch(...){LeaveCriticalSection(&lock);throw;}
    LeaveCriticalSection(&lock);
}
Stats Snapshot(){
    Stats s={unsigned __int64(Load64(allocated)),unsigned __int64(Load64(hits)),unsigned __int64(Load64(misses)),unsigned __int64(Load64(queued)),unsigned __int64(Load64(rejected)),unsigned __int64(Load64(readBytes)),0,0};
    if(initialized&&TryEnterCriticalSection(&lock)){s.entries=unsigned(files.size());s.pending=unsigned(requests.size());s.active=!active.empty();LeaveCriticalSection(&lock);}return s;
}
void Prefetch(const std::string&p){Observe(p);}
bool StandbyAllowed(){return On()&&Load64(allocated)<=Load64(ceiling)&&MemoryRoom(0);}
void SetBackgroundAllowed(bool allow){LONG before=InterlockedExchange(&backgroundAllowed,allow?1:0);if(allow&&!before&&eventHandle)SetEvent(eventHandle);}
bool BackgroundAllowed(){return Load(backgroundAllowed)!=0;}
void SignalStop(){InterlockedExchange(&enabled,0);InterlockedExchange(&stopping,1);if(eventHandle)SetEvent(eventHandle);}
void Stop(){
    SignalStop();
    if(threadHandle){
        typedef BOOL (WINAPI*CancelSync)(HANDLE);
        CancelSync cancel=reinterpret_cast<CancelSync>(GetProcAddress(GetModuleHandleA("kernel32.dll"),"CancelSynchronousIo"));
        if(cancel)cancel(threadHandle);
        // Some storage filters may ignore cancellation. Never freeze game shutdown;
        // KEO is pinned, and the worker uses only owned data, so it may drain later.
        if(WaitForSingleObject(threadHandle,250)!=WAIT_OBJECT_0)return;
        CloseHandle(threadHandle);threadHandle=NULL;
    }
    // Keep the one event alive until process exit: a late archive callback can
    // have passed On() before SignalStop. No handle reuse race with SetEvent.
}
}
