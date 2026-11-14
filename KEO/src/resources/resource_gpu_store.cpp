#include "resources/resource_gpu_store.h"
#include <dxgi.h>
#include <map>
#include <deque>
#include <algorithm>
#include <sstream>
#include <cstring>
// Shared cache API is implemented by the independently integrated RAM store.
namespace resource_cache {void Prefetch(const std::string&);bool StandbyAllowed();}

// SDK7.1 has IDXGIAdapter1. These four appended ABI methods through
// QueryVideoMemoryInfo match local SDK10 dxgi1_2.h/dxgi1_4.h. Only QI and the
// final method are invoked; old systems fail QI and warmup stays inactive.
namespace {
struct VideoMemoryInfo {UINT64 Budget,CurrentUsage,AvailableForReservation,CurrentReservation;};
struct __declspec(uuid("645967A4-1392-4310-A798-8053CE3E93FD")) BudgetAdapter : IDXGIAdapter1 {
    virtual HRESULT STDMETHODCALLTYPE GetDesc2(void*)=0;
    virtual HRESULT STDMETHODCALLTYPE RegisterHardwareContentProtectionTeardownStatusEvent(HANDLE,DWORD*)=0;
    virtual void STDMETHODCALLTYPE UnregisterHardwareContentProtectionTeardownStatus(DWORD)=0;
    virtual HRESULT STDMETHODCALLTYPE QueryVideoMemoryInfo(UINT,int,VideoMemoryInfo*)=0;
};
template<class T>struct Com {T*p;Com():p(NULL){}~Com(){if(p)p->Release();}private:Com(const Com&);Com&operator=(const Com&);};
}
namespace resource_gpu {
namespace {
const U64 MiB=1024ULL*1024,GiB=1024*MiB;
const size_t MaxQueue=32,MaxEntries=128;
__declspec(align(8)) volatile LONGLONG allocated=0,reserved=0,created=0,rejected=0,retries=0;
volatile LONG stopping=0,enabled=0,epoch=0,workAllowed=0;
CRITICAL_SECTION lock;bool initialized=false;
HANDLE eventHandle=NULL,threadHandle=NULL;
ID3D11Device* device=NULL;
U64 ceiling=GiB,world=0,touch=0;
U64 geometryBytes=0,geometryCeiling=GiB;unsigned deviceGeneration=0;
BudgetProbe probe=&ProbeLocalBudget;void*probeContext=NULL;
bool pressure=false;
std::string activeKey,lastError;
struct Pending {Job job;LONG epoch;unsigned attempts;DWORD due;};
struct Entry {ReadyPtr ready;U64 touch;LONG epoch;};
std::deque<Pending> requests;
std::map<std::string,Entry> entries;
struct Guard {Guard(){EnterCriticalSection(&lock);}~Guard(){LeaveCriticalSection(&lock);}};
LONG Read(volatile LONG&v){return InterlockedCompareExchange(&v,0,0);}
U64 Read64(volatile LONGLONG&v){return U64(InterlockedCompareExchange64(&v,0,0));}
bool On(){return Read(enabled)&&!Read(stopping);}
bool Cancelled(LONG token){return !On()||Read(epoch)!=token;}
bool MemoryAllowsStandby(){return resource_cache::StandbyAllowed();}
void Error(const std::string&s){Guard g;lastError=s;}
bool Plan(unsigned type,const resource_cache::FilePtr*f,D3D11_TEXTURE2D_DESC*desc,std::vector<Span>*spans,U64*bytes,std::string*error){
    if(!f||type>1){if(error)*error="missing DDS sources or unsupported type";return false;}
    Dds ds[6];
    for(unsigned l=0;l<6;++l){
        if(!f[l]||!ParseDds(f[l]->data,type,&ds[l],error))return false;
        if(l&&(ds[l].width!=ds[0].width||ds[l].height!=ds[0].height||ds[l].levels!=ds[0].levels||ds[l].format!=ds[0].format)){if(error)*error="array source shapes differ";return false;}
    }
    ZeroMemory(desc,sizeof(*desc));desc->Width=ds[0].width;desc->Height=ds[0].height;desc->ArraySize=6;desc->MipLevels=ds[0].levels;
    desc->Format=DXGI_FORMAT(ds[0].format);desc->SampleDesc.Count=1;desc->Usage=D3D11_USAGE_DEFAULT;desc->BindFlags=D3D11_BIND_SHADER_RESOURCE;
    spans->clear();*bytes=0;
    for(unsigned l=0;l<6;++l)for(unsigned m=0;m<ds[l].levels;++m){const DdsMip&p=ds[l].mips[m];Span s={&f[l]->data[p.offset],p.bytes,p.rowPitch,p.width,p.height};spans->push_back(s);*bytes+=p.bytes;}
    return true;
}
void ClearStandby(){std::map<std::string,Entry> drop;{Guard g;entries.swap(drop);}}
void ClearStaleStandby(LONG keep){std::vector<ReadyPtr>drop;{Guard g;for(std::map<std::string,Entry>::iterator i=entries.begin();i!=entries.end();){if(i->second.epoch!=keep){drop.push_back(i->second.ready);entries.erase(i++);}else ++i;}}}
bool Reserve(U64 bytes){
    // Release one victim outside the lock, then recheck the real accounting.
    // Keeping victim references until loop end would evict the entire cache.
    for(;;){
        ReadyPtr drop;
        {Guard g;
            if(!On()||bytes>ceiling)return false;
            const U64 cap=geometryBytes?(std::min)(ceiling,geometryCeiling):ceiling;
            if(CacheRoom(cap,geometryBytes,bytes)&&CacheRoom(cap-geometryBytes,Read64(allocated),bytes)&&entries.size()<MaxEntries){
                InterlockedExchangeAdd64(&allocated,LONGLONG(bytes));InterlockedExchangeAdd64(&reserved,LONGLONG(bytes));return true;
            }
            if(entries.empty())return false;
            std::map<std::string,Entry>::iterator oldest=entries.begin();
            for(std::map<std::string,Entry>::iterator it=entries.begin();it!=entries.end();++it)if(it->second.touch<oldest->second.touch)oldest=it;
            drop=oldest->second.ready;entries.erase(oldest);
        }
        drop.reset();
    }
}
struct Reservation {
    U64 bytes;Reservation(U64 b):bytes(b){}
    ~Reservation(){if(bytes){InterlockedExchangeAdd64(&reserved,-LONGLONG(bytes));InterlockedExchangeAdd64(&allocated,-LONGLONG(bytes));}}
    void HandOff(){InterlockedExchangeAdd64(&reserved,-LONGLONG(bytes));bytes=0;}
};
bool JobValid(const Job&j){
    if(j.key.empty()||j.key.size()>512||j.type>1)return false;
    for(unsigned i=0;i<6;++i){const std::string&p=j.paths[i];if(p.size()<7||p.size()>=MAX_PATH||p[1]!=':'||(p[2]!='/'&&p[2]!='\\')||_stricmp(p.c_str()+p.size()-4,".dds"))return false;}return true;
}
}
struct StoreAccess {static void Charge(const ReadyPtr&r){r->charged_=true;}};
Ready::Ready():worldGeneration(0),gpuBytes(0),texture_(NULL),charged_(false){ZeroMemory(&desc,sizeof(desc));}
Ready::~Ready(){if(texture_)texture_->Release();if(charged_)InterlockedExchangeAdd64(&allocated,-LONGLONG(gpuBytes));}
ID3D11Texture2D* Ready::AddRefTexture()const{if(texture_)texture_->AddRef();return texture_;}
bool BuildTexture(ID3D11Device*d,const Job&j,const resource_cache::FilePtr*f,ReadyPtr*out,std::string*error){
    if(!out)return false;out->reset();if(!d)return false;
    ReadyPtr r(new Ready);
    if(!Plan(j.type,f,&r->desc,&r->subresources,&r->gpuBytes,error))return false;
    UINT support=0;if(FAILED(d->CheckFormatSupport(r->desc.Format,&support))||(support&(D3D11_FORMAT_SUPPORT_TEXTURE2D|D3D11_FORMAT_SUPPORT_SHADER_SAMPLE))!=(D3D11_FORMAT_SUPPORT_TEXTURE2D|D3D11_FORMAT_SUPPORT_SHADER_SAMPLE)){if(error)*error="DDS format unsupported by device";return false;}
    std::vector<D3D11_SUBRESOURCE_DATA> initial(r->subresources.size());
    for(size_t s=0;s<initial.size();++s){initial[s].pSysMem=r->subresources[s].data;initial[s].SysMemPitch=r->subresources[s].rowPitch;initial[s].SysMemSlicePitch=UINT(r->subresources[s].bytes);}
    HRESULT hr=d->CreateTexture2D(&r->desc,&initial[0],&r->texture_);
    if(FAILED(hr)){if(error){std::ostringstream s;s<<"CreateTexture2D failed HRESULT="<<std::hex<<unsigned(hr);*error=s.str();}return false;}
    r->key=j.key;r->worldGeneration=j.worldGeneration;for(unsigned l=0;l<6;++l)r->sources[l]=f[l];
    *out=r;if(error)error->clear();return true;
}
bool DeviceCanCreateConcurrently(ID3D11Device*d){
    if(!d||(d->GetCreationFlags()&D3D11_CREATE_DEVICE_SINGLETHREADED)||FAILED(d->GetDeviceRemovedReason()))return false;
    D3D11_FEATURE_DATA_THREADING t={};return SUCCEEDED(d->CheckFeatureSupport(D3D11_FEATURE_THREADING,&t,sizeof(t)))&&t.DriverConcurrentCreates;
}
bool BudgetRoom(U64 budget,U64 usage,U64 extra){
    U64 reserve=(std::max)(256*MiB,budget/10);
    return budget>reserve&&usage<=budget-reserve&&extra<=budget-reserve-usage;
}
bool CacheRoom(U64 cap,U64 used,U64 extra){return extra<=cap&&used<=cap-extra;}
bool TryGeometryReservation(U64 bytes,U64 sharedCap){
    if(!initialized)return bytes==0;
    sharedCap=(std::max)(GiB,(std::min)(sharedCap,8*GiB));
    Guard g;
    // Lowering/releasing a reservation must work even after removal/pressure.
    if(bytes==0||bytes<geometryBytes){geometryBytes=bytes;geometryCeiling=sharedCap;return true;}
    U64 budget=0,usage=0;
    if(Read(stopping)||!device||!MemoryAllowsStandby()
       ||FAILED(device->GetDeviceRemovedReason())
       ||!CacheRoom(sharedCap,Read64(allocated),bytes)
       ||!probe||!probe(device,&budget,&usage,probeContext)||!BudgetRoom(budget,usage,0))return false;
    geometryBytes=bytes;geometryCeiling=sharedCap;return true;
}
U64 GeometryReservation(){if(!initialized)return 0;Guard g;return geometryBytes;}
unsigned DeviceGeneration(){if(!initialized)return 0;Guard g;return deviceGeneration;}
bool GeometryStandbyAllowed(U64 cap){
    if(!initialized)return false;Guard g;U64 budget=0,usage=0;
    return !Read(stopping)&&device&&MemoryAllowsStandby()&&SUCCEEDED(device->GetDeviceRemovedReason())
      &&CacheRoom(cap,Read64(allocated),geometryBytes)&&probe
      &&probe(device,&budget,&usage,probeContext)&&BudgetRoom(budget,usage,0);
}
bool ProbeLocalBudget(ID3D11Device*d,U64*b,U64*u,void*){
    if(!d||!b||!u)return false;
    Com<IDXGIDevice> xd;Com<IDXGIAdapter>a;Com<BudgetAdapter>a3;
    if(FAILED(d->QueryInterface(__uuidof(IDXGIDevice),reinterpret_cast<void**>(&xd.p)))||FAILED(xd.p->GetAdapter(&a.p))||FAILED(a.p->QueryInterface(__uuidof(BudgetAdapter),reinterpret_cast<void**>(&a3.p))))return false;
    VideoMemoryInfo i={};if(FAILED(a3.p->QueryVideoMemoryInfo(0,0,&i)))return false;*b=i.Budget;*u=i.CurrentUsage;return true;
}
namespace {
DWORD WINAPI Worker(void*){
    SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_LOWEST);
    LONG observedEpoch=-1,reportedRemovedEpoch=-1;DWORD nextCreate=0;
    for(;;){
        WaitForSingleObject(eventHandle,250);if(Read(stopping))break;
        try{
            LONG nowEpoch=Read(epoch);
            if(!On())ClearStandby();else if(nowEpoch!=observedEpoch)ClearStaleStandby(nowEpoch);
            observedEpoch=nowEpoch;
            if(!On())continue;
            Com<ID3D11Device>d;BudgetProbe p;void*context;
            {Guard g;d.p=device;if(d.p)d.p->AddRef();p=probe;context=probeContext;}
            HRESULT health=d.p?d.p->GetDeviceRemovedReason():E_POINTER;
            if(d.p&&FAILED(health)&&reportedRemovedEpoch!=nowEpoch){std::ostringstream s;s<<"GPU device removed HRESULT="<<std::hex<<unsigned(health);Error(s.str());reportedRemovedEpoch=nowEpoch;}
            U64 budget=0,usage=0;bool room=d.p&&SUCCEEDED(health)&&MemoryAllowsStandby()&&p&&p(d.p,&budget,&usage,context)&&BudgetRoom(budget,usage,0);
            {Guard g;pressure=!room;}
            if(!room){ClearStandby();continue;}
            if(!Read(workAllowed))continue;
            DWORD now=GetTickCount();if(LONG(now-nextCreate)<0)continue;
            Pending work;bool found=false;
            {Guard g;for(size_t i=0,n=requests.size();i<n;++i){Pending q=requests.front();requests.pop_front();if(!found&&LONG(now-q.due)>=0){work=q;found=true;activeKey=q.job.key;}else requests.push_back(q);}}
            if(!found)continue;
            resource_cache::FilePtr f[6];bool complete=true;
            for(unsigned i=0;i<6;++i){f[i]=resource_cache::Find(work.job.paths[i]);if(!f[i]){resource_cache::Prefetch(work.job.paths[i]);complete=false;}}
            if(!Cancelled(work.epoch)&&!complete&&++work.attempts<120){
                work.due=now+250;Guard g;if(!Cancelled(work.epoch)&&requests.size()<MaxQueue){requests.push_back(work);InterlockedIncrement64(&retries);}
            }else if(!Cancelled(work.epoch)&&complete){
                D3D11_TEXTURE2D_DESC desc;std::vector<Span>spans;U64 bytes=0;std::string error;
                if(Plan(work.job.type,f,&desc,&spans,&bytes,&error)&&BudgetRoom(budget,usage,bytes)&&Reserve(bytes)){
                    Reservation reservation(bytes);ReadyPtr r;
                    nextCreate=GetTickCount()+250; // one whole driver allocation per interval, never a frame time claim
                    if(!Read(workAllowed)||!MemoryAllowsStandby()){
                        work.due=GetTickCount()+250;Guard g;if(!Cancelled(work.epoch)&&requests.size()<MaxQueue)requests.push_back(work);
                    }else if(BuildTexture(d.p,work.job,f,&r,&error)){
                        StoreAccess::Charge(r);reservation.HandOff();
                        if(!Cancelled(work.epoch)&&MemoryAllowsStandby()){
                            Guard g;const U64 cap=geometryBytes?(std::min)(ceiling,geometryCeiling):ceiling;
                            if(!Cancelled(work.epoch)&&CacheRoom(cap,geometryBytes,Read64(allocated))&&entries.size()<MaxEntries){Entry e={r,++touch,work.epoch};entries[work.job.key]=e;InterlockedIncrement64(&created);}
                        }
                    }else{InterlockedIncrement64(&rejected);if(error.find("CreateTexture2D failed")==0)Error(error);}
                }else InterlockedIncrement64(&rejected);
            }else if(complete==false)InterlockedIncrement64(&rejected);
            {Guard g;activeKey.clear();}
        }catch(...){InterlockedIncrement64(&rejected);Error("GPU resource worker exception; current job dropped");Guard g;activeKey.clear();}
    }
    ClearStandby();std::deque<Pending>drop;ID3D11Device*old=NULL;
    {Guard g;requests.swap(drop);activeKey.clear();old=device;device=NULL;}
    if(old)old->Release();return 0;
}
}
bool Start(){
    if(threadHandle)return !Read(stopping);
    if(!initialized){InitializeCriticalSection(&lock);initialized=true;}
    HMODULE module=NULL;if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCSTR>(&Worker),&module))return false;
    if(!eventHandle)eventHandle=CreateEventA(NULL,FALSE,FALSE,NULL);if(!eventHandle)return false;
    {Guard g;probe=&ProbeLocalBudget;probeContext=NULL;}
    InterlockedExchange(&workAllowed,0);InterlockedExchange(&stopping,0);ResetEvent(eventHandle);threadHandle=CreateThread(NULL,0,Worker,NULL,0,NULL);return threadHandle!=NULL;
}
bool SetDevice(ID3D11Device*d){
    if(!initialized)return false;
    bool valid=DeviceCanCreateConcurrently(d);if(valid)d->AddRef();ID3D11Device*old=NULL;
    {Guard g;if(device==d&&valid){d->Release();return true;}old=device;device=valid?d:NULL;++deviceGeneration;requests.clear();InterlockedIncrement(&epoch);}
    if(old)old->Release();SetEvent(eventHandle);return valid;
}
void SetBudgetProbe(BudgetProbe p,void*c){if(!initialized)return;{Guard g;probe=p?p:&ProbeLocalBudget;probeContext=c;InterlockedIncrement(&epoch);requests.clear();}SetEvent(eventHandle);}
void Configure(bool on,U64 bytes,U64 generation){
    if(!initialized)return;bytes=(std::max)(GiB,(std::min)(bytes,8*GiB));
    {Guard g;if(Read(enabled)==int(on)&&ceiling==bytes&&world==generation)return;ceiling=bytes;world=generation;requests.clear();InterlockedExchange(&enabled,on?1:0);InterlockedIncrement(&epoch);}
    SetEvent(eventHandle);
}
void SetWorkAllowed(bool allow){LONG before=InterlockedExchange(&workAllowed,allow?1:0);if(allow&&!before&&eventHandle)SetEvent(eventHandle);}
bool Queue(const Job&j){
    if(!initialized||!On()||!JobValid(j)||!TryEnterCriticalSection(&lock))return false;bool accepted=false;
    try{
        if(On()&&device&&j.worldGeneration==world&&requests.size()<MaxQueue&&activeKey!=j.key&&entries.find(j.key)==entries.end()){
            bool exists=false;for(size_t n=0;n<requests.size();++n)if(requests[n].job.key==j.key){exists=true;break;}
            if(!exists){Pending p={j,Read(epoch),0,GetTickCount()};requests.push_back(p);accepted=true;}
        }
    }catch(...){LeaveCriticalSection(&lock);throw;}
    LeaveCriticalSection(&lock);if(accepted)SetEvent(eventHandle);return accepted;
}
ReadyPtr TakeReady(const std::string&key,U64 generation){
    ReadyPtr r;if(!initialized||!On()||!TryEnterCriticalSection(&lock))return r;
    if(generation==world){std::map<std::string,Entry>::iterator i=entries.find(key);if(i!=entries.end()&&i->second.epoch==Read(epoch)&&i->second.ready->worldGeneration==generation){r=i->second.ready;entries.erase(i);}}
    LeaveCriticalSection(&lock);return r;
}
void MarkAdopted(const ReadyPtr&r){
    // Main-thread consumer must first have transferred one AddRef to native.
    // A taken Ready retains only source bytes after this; native owns the GPU.
    if(!r)return;ID3D11Texture2D*t=r->texture_;r->texture_=NULL;if(t)t->Release();
    if(r->charged_){r->charged_=false;InterlockedExchangeAdd64(&allocated,-LONGLONG(r->gpuBytes));}
}
Stats Snapshot(){
    Stats s={Read64(allocated),Read64(reserved),Read64(created),Read64(rejected),Read64(retries),0,0,false,false};
    if(initialized&&TryEnterCriticalSection(&lock)){s.entries=unsigned(entries.size());s.pending=unsigned(requests.size());s.deviceReady=device!=NULL;s.pressure=pressure;s.active=!activeKey.empty();LeaveCriticalSection(&lock);}return s;
}
bool TakeError(std::string*out){if(!initialized||!out||!TryEnterCriticalSection(&lock))return false;bool any=!lastError.empty();if(any){out->clear();out->swap(lastError);}LeaveCriticalSection(&lock);return any;}
void SignalStop(){InterlockedExchange(&enabled,0);InterlockedExchange(&stopping,1);InterlockedIncrement(&epoch);if(eventHandle)SetEvent(eventHandle);}
bool Stop(unsigned timeoutMs){SignalStop();if(!threadHandle)return true;if(WaitForSingleObject(threadHandle,(std::min)(timeoutMs,250u))!=WAIT_OBJECT_0)return false;CloseHandle(threadHandle);threadHandle=NULL;return true;}
}
