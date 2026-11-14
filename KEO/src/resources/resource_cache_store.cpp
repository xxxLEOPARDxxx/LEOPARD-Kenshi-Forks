#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "resources/resource_cache_store.h"
#include <map>
#include <set>
#include <deque>
#include <list>
#include <algorithm>
#include <cstring>
namespace resource_cache {
namespace {
const size_t MaxEntries=262144, MaxQueue=128, Chunk=256*1024;
const size_t MaxRoots=8192, MaxDirectories=65536, MaxStartupFiles=262144;
const unsigned __int64 MaxFile=256ULL*1024*1024, MiB=1024ULL*1024;
__declspec(align(8)) volatile LONGLONG allocated=0,hits=0,misses=0,queued=0,rejected=0,readBytes=0;
CRITICAL_SECTION lock;
bool initialized=false;
HANDLE eventHandle=NULL, threadHandle=NULL;
volatile LONG stopping=0, enabled=0, generation=0, rate=32;
volatile LONG backgroundAllowed=1, startupEnabled=0, startupGeneration=0;
__declspec(align(8)) volatile LONGLONG ceiling=0;
struct PathLess {bool operator()(const std::string&a,const std::string&b)const{return _stricmp(a.c_str(),b.c_str())<0;}};
typedef std::list<std::string> LruList;
struct Entry {FilePtr file;LruList::iterator lru;};
typedef std::map<std::string,Entry,PathLess> FileMap;
FileMap files;
LruList lru;
std::deque<std::string> requests;
std::string active;
struct Guard {Guard(){EnterCriticalSection(&lock);}~Guard(){LeaveCriticalSection(&lock);}};
LONG Load(volatile LONG& v){return InterlockedCompareExchange(&v,0,0);}
LONGLONG Load64(volatile LONGLONG&v){return InterlockedCompareExchange64(&v,0,0);}
bool On(){return Load(enabled)!=0 && Load(stopping)==0;}
// All startup inventory/cursors belong to the IO worker. Other threads change
// only the copied root table, epoch, and small published counters under lock.
std::map<std::string,bool,PathLess> roots;
bool rootsCapped=false;
DWORD rootsChanged=0;
Stats progress={0};
struct Directory {std::string path;bool recursive;Directory(const std::string&p,bool r):path(p),recursive(r){}};
struct StartupWork {
    LONG epoch;
    bool started, scanned, capped;
    std::vector<Directory> directories;
    std::map<std::string,bool,PathLess> visited;
    std::vector<std::string> inventory;
    std::set<std::string,PathLess> seenFiles;
    size_t directory, next;
    HANDLE find;
    WIN32_FIND_DATAA item;
    bool hasItem;
    StartupWork():epoch(-1),started(false),scanned(false),capped(false),directory(0),next(0),find(INVALID_HANDLE_VALUE),hasItem(false){}
    ~StartupWork(){if(find!=INVALID_HANDLE_VALUE)FindClose(find);}
    void Reset(LONG e){
        if(find!=INVALID_HANDLE_VALUE)FindClose(find);find=INVALID_HANDLE_VALUE;
        std::vector<Directory>().swap(directories);std::map<std::string,bool,PathLess>().swap(visited);
        std::vector<std::string>().swap(inventory);std::set<std::string,PathLess>().swap(seenFiles);
        epoch=e;started=scanned=capped=hasItem=false;directory=next=0;
    }
};
void ResetProgress(){progress=Stats();progress.startupRoots=unsigned(roots.size());}
bool Texture(const std::string&p){
    size_t at=p.find_last_of('.');if(at==std::string::npos)return false;
    const char* s=p.c_str()+at;
    return !_stricmp(s,".dds")||!_stricmp(s,".png")||!_stricmp(s,".tga")||!_stricmp(s,".jpg")||!_stricmp(s,".jpeg")||!_stricmp(s,".bmp");
}
bool Eligible(const std::string&p){
    if(p.size()<7||p.size()>=MAX_PATH||p[1]!=':'||(p[2]!='\\'&&p[2]!='/'))return false;
    size_t at=p.find_last_of('.');if(at==std::string::npos)return false;
    return Texture(p)||!_stricmp(p.c_str()+at,".mesh")||!_stricmp(p.c_str()+at,".skeleton");
}
bool NormalizeRoot(const std::string&p,std::string&out){
    if(p.size()<3||p.size()>=MAX_PATH||p[1]!=':'||(p[2]!='\\'&&p[2]!='/')||p.find(':',2)!=std::string::npos)return false;
    char full[MAX_PATH];DWORD n=GetFullPathNameA(p.c_str(),MAX_PATH,full,NULL);
    if(!n||n>=MAX_PATH)return false;out.assign(full,n);
    for(size_t i=0;i<out.size();++i)if(out[i]=='/')out[i]='\\';
    while(out.size()>3&&out[out.size()-1]=='\\')out.resize(out.size()-1);
    char drive[4]={out[0],':','\\',0};return GetDriveTypeA(drive)==DRIVE_FIXED;
}
bool SafeLocal(const std::string&p,bool directory){
    if(p.size()<3||p.size()>=MAX_PATH||p[1]!=':'||p[2]!='\\')return false;
    char drive[4]={p[0],':','\\',0};if(GetDriveTypeA(drive)!=DRIVE_FIXED)return false;
    // Check every ancestor, including a root registered through a junction.
    for(size_t i=3;i<=p.size();++i)if(i==p.size()||p[i]=='\\'){
        if(i==3)continue;
        DWORD attr=GetFileAttributesA(p.substr(0,i).c_str());
        if(attr==INVALID_FILE_ATTRIBUTES||(attr&FILE_ATTRIBUTE_REPARSE_POINT))return false;
        if((i<p.size()||directory)&&!(attr&FILE_ATTRIBUTE_DIRECTORY))return false;
        if(i==p.size()&&!directory&&(attr&FILE_ATTRIBUTE_DIRECTORY))return false;
    }
    return true;
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
bool StartupCancelled(LONG e){return !Load(startupEnabled)||Load(startupGeneration)!=e;}
bool ReadCancelled(LONG e,LONG se){return Cancelled(e)||(se>=0&&StartupCancelled(se));}
bool HasRequests(){Guard g;return !requests.empty();}
bool MemoryRoom(unsigned __int64 bytes){
    MEMORYSTATUSEX m={sizeof(m)};
    if(!GlobalMemoryStatusEx(&m))return false;
    unsigned __int64 reserve=(std::max)(2ULL*1024*MiB,m.ullTotalPhys/5);
    return m.ullAvailPhys>reserve && bytes<=m.ullAvailPhys-reserve && m.ullAvailPageFile>bytes+256*MiB;
}
void Erase(FileMap::iterator it){lru.erase(it->second.lru);files.erase(it);}
void DropFiles(bool clearRequests){
    // Destruct potentially thousands of buffers outside the callback lock.
    FileMap retired;LruList retiredLru;
    {Guard g;files.swap(retired);lru.swap(retiredLru);if(clearRequests)requests.clear();}
}
void Clear(){DropFiles(true);}
void TrimPressure(){
    if(!MemoryRoom(0))DropFiles(false); // retained native streams still own/charge their bytes
}
bool MakeRoom(unsigned __int64 bytes,bool evict=true){
    unsigned __int64 cap=unsigned __int64(Load64(ceiling));
    if(bytes>cap)return false;
    if(!bytes){
        // A live cap reduction may release the whole index. Never wait for
        // pinned engine streams or repeatedly walk them looking for victims.
        if(unsigned __int64(Load64(allocated))>cap)DropFiles(false);
        return unsigned __int64(Load64(allocated))<=cap;
    }
    if(!evict){Guard g;cap=unsigned __int64(Load64(ceiling));return bytes<=cap&&unsigned __int64(Load64(allocated))<=cap-bytes&&files.size()<MaxEntries;}
    std::vector<FilePtr> retired;retired.reserve(512);
    Guard g;
    cap=unsigned __int64(Load64(ceiling));if(bytes>cap)return false;
    // One LRU walk, including pinned readers. Large cap reductions must not
    // repeatedly scan the entire map for every file being removed.
    LruList::iterator oldest=lru.begin();
    unsigned inspected=0;unsigned __int64 released=0;
    while(evict&&oldest!=lru.end()&&inspected++<512&&(unsigned __int64(Load64(allocated))-released>cap-bytes||files.size()>=MaxEntries)){
        FileMap::iterator it=files.find(*oldest++);
        if(it->second.file.use_count()==1){released+=it->second.file->charged;retired.push_back(it->second.file);Erase(it);}
    }
    // Guard dies before retired: release the buffers outside the lock. If a
    // huge request needs more than one bounded batch, native IO remains authoritative.
    return unsigned __int64(Load64(allocated))-released<=cap-bytes && files.size()<MaxEntries;
}
struct Handle {HANDLE h;Handle(HANDLE v):h(v){}~Handle(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}};
enum ReadResult {ReadSkipped,ReadLoaded,ReadCapped,ReadDeferred,ReadCancelledResult};
ReadResult ReadOne(const std::string&p,LONG epoch,LONG startupEpoch=-1){
    const bool startup=startupEpoch>=0;
    char root[4]={p[0],':','\\',0};if(GetDriveTypeA(root)!=DRIVE_FIXED)return ReadSkipped;
    WIN32_FILE_ATTRIBUTE_DATA before;
    if(ReadCancelled(epoch,startupEpoch))return ReadCancelledResult;
    if((startup&&!SafeLocal(p,false))||!Info(p,before))return ReadSkipped;
    unsigned __int64 bytes=Size(before);
    if(!bytes||bytes>MaxFile)return ReadSkipped;
    if(!MemoryRoom(bytes)||!MakeRoom(bytes,!startup)){InterlockedIncrement64(&rejected);return ReadCapped;}
    Handle h(CreateFileA(p.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN|(startup?FILE_FLAG_OPEN_REPARSE_POINT:0),NULL));
    if(h.h==INVALID_HANDLE_VALUE)return ReadSkipped;
    BY_HANDLE_FILE_INFORMATION identity;
    if(!GetFileInformationByHandle(h.h,&identity)||(startup&&(identity.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)))return ReadSkipped;
    if(identity.nFileSizeHigh!=before.nFileSizeHigh||identity.nFileSizeLow!=before.nFileSizeLow
        ||identity.ftLastWriteTime.dwHighDateTime!=before.ftLastWriteTime.dwHighDateTime
        ||identity.ftLastWriteTime.dwLowDateTime!=before.ftLastWriteTime.dwLowDateTime)return ReadSkipped;
    std::shared_ptr<CachedFile> f(new CachedFile);
    f->charged=bytes;InterlockedExchangeAdd64(&allocated,LONGLONG(bytes));
    f->data.resize(size_t(bytes));
    f->sizeHigh=before.nFileSizeHigh;f->sizeLow=before.nFileSizeLow;
    f->timeHigh=before.ftLastWriteTime.dwHighDateTime;f->timeLow=before.ftLastWriteTime.dwLowDateTime;
    f->volume=identity.dwVolumeSerialNumber;f->indexHigh=identity.nFileIndexHigh;f->indexLow=identity.nFileIndexLow;
    size_t offset=0;
    while(offset<f->data.size()){
        if(ReadCancelled(epoch,startupEpoch))return ReadCancelledResult;
        if(!MemoryRoom(0))return ReadCapped;
        if(startup&&HasRequests())return ReadDeferred;
        while(!Load(backgroundAllowed)&&!ReadCancelled(epoch,startupEpoch)){
            if(!MemoryRoom(0))return ReadCapped;
            if(startup&&HasRequests())return ReadDeferred;
            Sleep(10);
        }
        if(ReadCancelled(epoch,startupEpoch))return ReadCancelledResult;
        DWORD count=DWORD((std::min)(Chunk,f->data.size()-offset)),got=0;
        DWORD start=GetTickCount();
        if(!ReadFile(h.h,&f->data[offset],count,&got,NULL)||got!=count)return ReadSkipped;
        offset+=got;InterlockedExchangeAdd64(&readBytes,got);
        if(startup){Guard g;if(!StartupCancelled(startupEpoch))progress.startupReadBytes+=got;}
        unsigned speed=(std::max)(1L,Load(rate));
        DWORD span=DWORD((unsigned __int64(count)*1000+speed*MiB-1)/(speed*MiB));
        DWORD elapsed=GetTickCount()-start;
        if(span>elapsed){
            DWORD remaining=span-elapsed;
            while(remaining&&!ReadCancelled(epoch,startupEpoch)){
                if(startup&&HasRequests())return ReadDeferred;
                DWORD wait=(std::min)(remaining,DWORD(10));Sleep(wait);remaining-=wait;
            }
        }
    }
    WIN32_FILE_ATTRIBUTE_DATA after;
    if(ReadCancelled(epoch,startupEpoch))return ReadCancelledResult;
    if(!Info(p,after)||!Matches(*f,after)||(startup&&!SafeLocal(p,false)))return ReadSkipped;
    Guard g;
    if(ReadCancelled(epoch,startupEpoch))return ReadCancelledResult;
    if(unsigned __int64(Load64(allocated))>unsigned __int64(Load64(ceiling)))return ReadCapped;
    FileMap::iterator existing=files.find(p);if(existing!=files.end())Erase(existing);
    lru.push_back(p);
    try{Entry e={f,--lru.end()};files.insert(std::make_pair(p,e));}
    catch(...){lru.pop_back();throw;}
    return ReadLoaded;
}
void AddDirectory(StartupWork&w,const std::string&p,bool recursive){
    std::map<std::string,bool,PathLess>::iterator it=w.visited.find(p);
    if(it!=w.visited.end()&&(!recursive||it->second))return;
    if(w.directories.size()>=MaxDirectories){w.capped=true;return;}
    w.visited[p]=recursive;w.directories.push_back(Directory(p,recursive));
}
void ScanStep(StartupWork&w){
    // Limit work per dispatch; observed engine requests run before the next slice.
    for(unsigned slice=0;slice<64&&!StartupCancelled(w.epoch)&&!HasRequests();++slice){
        if(w.directory>=w.directories.size()){w.scanned=true;Guard g;if(!StartupCancelled(w.epoch)){progress.startupScanning=false;progress.startupCapped=w.capped||rootsCapped;}return;}
        const std::string path=w.directories[w.directory].path;
        const bool recursive=w.directories[w.directory].recursive;
        if(w.find==INVALID_HANDLE_VALUE){
            if(path.size()+2>=MAX_PATH||!SafeLocal(path,true)){++w.directory;continue;}
            w.find=FindFirstFileA((path+"\\*").c_str(),&w.item);w.hasItem=w.find!=INVALID_HANDLE_VALUE;
            if(!w.hasItem){++w.directory;continue;}
        }
        if(!w.hasItem){FindClose(w.find);w.find=INVALID_HANDLE_VALUE;++w.directory;continue;}
        WIN32_FIND_DATAA item=w.item;
        w.hasItem=FindNextFileA(w.find,&w.item)!=FALSE;
        if(!strcmp(item.cFileName,".")||!strcmp(item.cFileName,"..")||(item.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))continue;
        std::string full=path+"\\"+item.cFileName;if(full.size()>=MAX_PATH)continue;
        if(item.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){if(recursive)AddDirectory(w,full,true);continue;}
        if(!Texture(full)||w.seenFiles.find(full)!=w.seenFiles.end())continue;
        if(w.inventory.size()>=MaxStartupFiles){w.capped=true;w.scanned=true;if(w.find!=INVALID_HANDLE_VALUE)FindClose(w.find);w.find=INVALID_HANDLE_VALUE;Guard g;if(!StartupCancelled(w.epoch)){progress.startupScanning=false;progress.startupCapped=true;}return;}
        w.seenFiles.insert(full);w.inventory.push_back(full);
        unsigned __int64 size=(unsigned __int64(item.nFileSizeHigh)<<32)|item.nFileSizeLow;
        Guard g;if(!StartupCancelled(w.epoch)){++progress.startupFiles;progress.startupTotalBytes+=size;}
    }
}
bool StartupStep(StartupWork&w,LONG cacheEpoch){
    LONG e=Load(startupGeneration);
    if(w.epoch!=e)w.Reset(e);
    if(!Load(startupEnabled)||!On()){if(w.started)w.Reset(e);return false;}
    {Guard g;if(progress.startupDone||DWORD(GetTickCount()-rootsChanged)<2000)return false;}
    if(!MemoryRoom(0)){
        {Guard g;if(!StartupCancelled(e)){progress.startupDone=true;progress.startupCapped=true;progress.startupScanning=false;}}
        w.Reset(e);return false;
    }
    if(!w.started){
        Guard g;if(StartupCancelled(e))return false;
        for(std::map<std::string,bool,PathLess>::const_iterator it=roots.begin();it!=roots.end();++it)AddDirectory(w,it->first,it->second);
        w.started=true;w.capped=rootsCapped;progress.startupScanning=true;
    }
    if(!w.scanned){ScanStep(w);return true;}
    if(StartupCancelled(e))return false;
    if(w.next>=w.inventory.size()){
        {Guard g;if(!StartupCancelled(e)){progress.startupDone=true;progress.startupCapped=w.capped||rootsCapped;}}
        // Release inventory strings/maps once terminal. Preserve the epoch marker.
        w.Reset(e);return false;
    }
    const std::string p=w.inventory[w.next];
    bool present=false;{Guard g;present=files.find(p)!=files.end();if(!present)active=p;}
    ReadResult result=present?ReadSkipped:ReadOne(p,cacheEpoch,e);
    bool terminal=false;
    {Guard g;active.clear();if(StartupCancelled(e)||Cancelled(cacheEpoch))return false;
        if(result==ReadDeferred||result==ReadCancelledResult)return true;
        ++w.next;++progress.startupProcessed;
        if(result==ReadLoaded)++progress.startupLoaded;else ++progress.startupSkipped;
        if(result==ReadCapped){progress.startupCapped=true;progress.startupDone=true;terminal=true;}
    }
    if(terminal){w.Reset(e);return false;}
    return true;
}
DWORD WINAPI Worker(void*){
    SetThreadPriority(GetCurrentThread(),THREAD_MODE_BACKGROUND_BEGIN);
    StartupWork startup;
    for(;;){
        WaitForSingleObject(eventHandle,250);
        if(Load(stopping))break;
        try{
            if(!On()){Clear();startup.Reset(Load(startupGeneration));continue;}
            if(startup.epoch!=Load(startupGeneration)||!Load(startupEnabled))startup.Reset(Load(startupGeneration));
            TrimPressure();MakeRoom(0);
            if(!Load(backgroundAllowed))continue;
            std::string p;LONG epoch=Load(generation);
            {Guard g;if(!requests.empty()){p=requests.front();requests.pop_front();active=p;}}
            bool more=false;
            if(!p.empty())ReadOne(p,epoch);else more=StartupStep(startup,epoch);
            {Guard g;active.clear();if(more||!requests.empty())SetEvent(eventHandle);}
        }catch(...){InterlockedIncrement64(&rejected);{Guard g;active.clear();if(Load(startupEnabled)){progress.startupDone=true;progress.startupCapped=true;progress.startupScanning=false;}}startup.Reset(Load(startupGeneration));}
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
    bytes=(std::min)(bytes,32ULL*1024*MiB);
    Guard g;
    bool change=(Load(enabled)!=int(on))||unsigned __int64(Load64(ceiling))!=bytes;
    InterlockedExchange64(&ceiling,LONGLONG(bytes));
    InterlockedExchange(&rate,LONG((std::max)(1u,(std::min)(speed,128u))));
    InterlockedExchange(&enabled,on?1:0);
    if(change){InterlockedIncrement(&generation);InterlockedIncrement(&startupGeneration);ResetProgress();rootsChanged=GetTickCount();}
    SetEvent(eventHandle);
}
void RegisterTextureRoot(const std::string&p,bool recursive){
    if(!initialized)return;std::string normalized;if(!NormalizeRoot(p,normalized))return;
    Guard g;
    std::map<std::string,bool,PathLess>::iterator it=roots.find(normalized);
    if(it!=roots.end()&&(!recursive||it->second))return;
    if(it==roots.end()&&roots.size()>=MaxRoots){rootsCapped=true;progress.startupCapped=true;return;}
    roots[normalized]=recursive;InterlockedIncrement(&startupGeneration);ResetProgress();rootsChanged=GetTickCount();SetEvent(eventHandle);
}
void ConfigureStartup(bool on){
    if(!initialized)return;Guard g;
    if(Load(startupEnabled)==int(on))return;
    InterlockedExchange(&startupEnabled,on?1:0);InterlockedIncrement(&startupGeneration);ResetProgress();rootsChanged=GetTickCount();SetEvent(eventHandle);
}
FilePtr Find(const std::string&p){
    if(!On()||!Eligible(p))return FilePtr();
    FilePtr f;
    if(TryEnterCriticalSection(&lock)){
        FileMap::iterator it=files.find(p);
        if(it!=files.end()){f=it->second.file;lru.splice(lru.end(),lru,it->second.lru);}
        LeaveCriticalSection(&lock);
    }
    if(f&&Identity(p,*f)){InterlockedIncrement64(&hits);return f;}
    if(f&&TryEnterCriticalSection(&lock)){
        FileMap::iterator it=files.find(p);
        if(it!=files.end()&&it->second.file==f)Erase(it);
        LeaveCriticalSection(&lock);
    }
    InterlockedIncrement64(&misses);return FilePtr();
}
void Observe(const std::string&p){
    if(!On()||!Eligible(p)||!TryEnterCriticalSection(&lock))return;
    try{
        bool pending=false;for(std::deque<std::string>::const_iterator it=requests.begin();it!=requests.end();++it)if(!_stricmp(it->c_str(),p.c_str())){pending=true;break;}
        if(On()&&requests.size()<MaxQueue && files.find(p)==files.end() && _stricmp(active.c_str(),p.c_str()) && !pending){
            requests.push_back(p);InterlockedIncrement64(&queued);SetEvent(eventHandle);
        }
    }catch(...){LeaveCriticalSection(&lock);throw;}
    LeaveCriticalSection(&lock);
}
Stats Snapshot(){
    Stats s={0};
    s.allocatedBytes=unsigned __int64(Load64(allocated));s.hits=unsigned __int64(Load64(hits));s.misses=unsigned __int64(Load64(misses));
    s.queued=unsigned __int64(Load64(queued));s.rejected=unsigned __int64(Load64(rejected));s.readBytes=unsigned __int64(Load64(readBytes));
    if(initialized&&TryEnterCriticalSection(&lock)){
        s.entries=unsigned(files.size());s.pending=unsigned(requests.size());s.active=!active.empty();
        s.startupRoots=progress.startupRoots;s.startupFiles=progress.startupFiles;s.startupProcessed=progress.startupProcessed;s.startupLoaded=progress.startupLoaded;s.startupSkipped=progress.startupSkipped;
        s.startupTotalBytes=progress.startupTotalBytes;s.startupReadBytes=progress.startupReadBytes;s.startupScanning=progress.startupScanning;s.startupDone=progress.startupDone;s.startupCapped=progress.startupCapped;
        LeaveCriticalSection(&lock);
    }return s;
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
        if(WaitForSingleObject(threadHandle,250)!=WAIT_OBJECT_0)return;
        CloseHandle(threadHandle);threadHandle=NULL;
    }
    // Event remains alive for late callbacks that passed On before SignalStop.
}
}
