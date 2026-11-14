#include "resources/resource_gpu_store.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <map>
#ifdef RESOURCE_GPU_INCLUDE_STORE
// Extract the actual private reservation code for small-unit cap regression;
// no duplicated eviction algorithm or gigabyte-sized test allocation.
#include "../src/resources/resource_gpu_store.cpp"
#endif
// Isolated immutable byte provider. The GPU decoder/store/driver path is real;
// disk IO and cache identity have their independent RAM-store production suites.
static std::map<std::string,resource_cache::FilePtr> fixtureFiles;
static volatile LONG fixtureAvailable=1,prefetches=0,fixturePressure=0,fixtureRamAllowed=1;
namespace resource_cache {
CachedFile::CachedFile():sizeHigh(0),sizeLow(0),timeHigh(0),timeLow(0),volume(0),indexHigh(0),indexLow(0),charged(0){}
CachedFile::~CachedFile(){}
FilePtr Find(const std::string&p){if(!InterlockedCompareExchange(&fixtureAvailable,0,0))return FilePtr();std::map<std::string,FilePtr>::const_iterator i=fixtureFiles.find(p);return i==fixtureFiles.end()?FilePtr():i->second;}
void Prefetch(const std::string&){InterlockedIncrement(&prefetches);}
bool StandbyAllowed(){return InterlockedCompareExchange(&fixtureRamAllowed,0,0)!=0;}
}
static void Check(bool b,const char*s){if(!b){std::printf("FAIL %s\n",s);std::exit(1);}}
static void Put(std::vector<unsigned char>&v,size_t o,unsigned n){std::memcpy(&v[o],&n,4);}
static resource_cache::FilePtr Source(unsigned type,unsigned layer){
    std::shared_ptr<resource_cache::CachedFile>f(new resource_cache::CachedFile);std::vector<unsigned char>&v=f->data;v.resize(128);Put(v,0,0x20534444);Put(v,4,124);Put(v,8,0xa1007);Put(v,12,256);Put(v,16,256);Put(v,28,9);Put(v,76,32);Put(v,80,4);Put(v,84,type?0x31545844:0x35545844);Put(v,108,0x401008);
    for(unsigned m=0,side=256;m<9;++m,side=side>1?side/2:1){unsigned row=((side+3)/4)*(type?8:16),bytes=row*((side+3)/4);size_t at=v.size();v.resize(at+bytes);for(unsigned i=0;i<bytes;++i)v[at+i]=unsigned char((i*7+layer*29+m*11)&255);}return f;
}
#ifdef RESOURCE_GPU_INCLUDE_STORE
static void EvictionTests(ID3D11Device*d){
    using namespace resource_gpu;
    if(!initialized){InitializeCriticalSection(&lock);initialized=true;}
    InterlockedExchange(&enabled,1);InterlockedExchange(&stopping,0);ceiling=300;
    resource_cache::FilePtr files[6];for(unsigned l=0;l<6;++l)files[l]=Source(0,l);
    for(unsigned i=0;i<3;++i){Job j;j.key=char('a'+i);ReadyPtr r;std::string error;Check(BuildTexture(d,j,files,&r,&error),"eviction WARP texture");r->gpuBytes=100;StoreAccess::Charge(r);InterlockedExchangeAdd64(&allocated,100);Entry e={r,i,Read(epoch)};entries[j.key]=e;}
    Check(entries.size()==3&&Read64(allocated)==300,"three real standby entries at synthetic accounting cap");
    Check(Reserve(100),"reserve one additional accounting unit");
    Check(entries.size()==2&&entries.find("a")==entries.end()&&entries.find("b")!=entries.end()&&entries.find("c")!=entries.end(),"one-victim eviction preserves other standby entries");
    Check(Read64(allocated)==300&&Read64(reserved)==100,"eviction reservation accounting balanced");
    {Reservation releaseReservation(100);}
    ClearStandby();Check(Read64(allocated)==0&&Read64(reserved)==0,"eviction fixture released all GPU objects");
    InterlockedExchange(&enabled,0);ceiling=1ULL<<30;
}
#endif
static bool Probe(ID3D11Device*,resource_gpu::U64*b,resource_gpu::U64*u,void*){*b=4ULL<<30;*u=InterlockedCompareExchange(&fixturePressure,0,0)?*b:0;return true;}
struct BlockingProbe {HANDLE entered,release;};
static bool BlockProbe(ID3D11Device*,resource_gpu::U64*b,resource_gpu::U64*u,void*p){BlockingProbe*x=static_cast<BlockingProbe*>(p);SetEvent(x->entered);WaitForSingleObject(x->release,5000);*b=4ULL<<30;*u=0;return true;}
static resource_gpu::ReadyPtr WaitReady(const std::string&key,resource_gpu::U64 gen){DWORD end=GetTickCount()+6000;resource_gpu::ReadyPtr r;while(!r&&LONG(GetTickCount()-end)<0){r=resource_gpu::TakeReady(key,gen);if(!r)Sleep(10);}return r;}
static bool QueueRetry(const resource_gpu::Job&j){DWORD end=GetTickCount()+500;do{if(resource_gpu::Queue(j))return true;Sleep(1);}while(LONG(GetTickCount()-end)<0);return false;}
static void WorkerTests(ID3D11Device*d){
    Check(resource_gpu::Start(),"worker start");Check(resource_gpu::SetDevice(d),"concurrent WARP admitted");resource_gpu::SetBudgetProbe(&Probe,NULL);resource_gpu::Configure(true,1ULL<<30,7);
    resource_gpu::Job j;j.key="worker-one";j.type=0;j.worldGeneration=7;for(unsigned i=0;i<6;++i){char path[80];std::sprintf(path,"D:/gpu-fixture/layer%u.dds",i);j.paths[i]=path;fixtureFiles[path]=Source(0,i);}
    InterlockedExchange(&fixtureAvailable,0);Check(QueueRetry(j),"queue accepts");Check(!resource_gpu::Queue(j),"queue deduplicates");Sleep(350);Check(prefetches==0&&resource_gpu::Snapshot().pending==1,"closed work gate preserves queued job without processing");resource_gpu::SetWorkAllowed(true);Sleep(350);Check(prefetches>0&&resource_gpu::Snapshot().retries>0,"missing bytes request prefetch and bounded retry");
    InterlockedExchange(&fixtureAvailable,1);resource_gpu::ReadyPtr r=WaitReady(j.key,7);Check(bool(r),"worker created native candidate");Check(resource_gpu::Snapshot().allocatedBytes==r->gpuBytes,"taken ready stays budget charged");ID3D11Texture2D*native=r->AddRefTexture();resource_gpu::MarkAdopted(r);Check(resource_gpu::Snapshot().allocatedBytes==0&&r->sources[0],"adoption detaches GPU budget and retains bytes");D3D11_TEXTURE2D_DESC desc;native->GetDesc(&desc);Check(desc.ArraySize==6,"native ownership survives detach");native->Release();r.reset();
    j.key="standby";Check(QueueRetry(j),"queue standby");DWORD end=GetTickCount()+6000;while(!resource_gpu::Snapshot().entries&&LONG(GetTickCount()-end)<0)Sleep(10);Check(resource_gpu::Snapshot().entries==1,"standby ready");
    InterlockedExchange(&fixtureRamAllowed,0);end=GetTickCount()+3000;while(resource_gpu::Snapshot().allocatedBytes&&LONG(GetTickCount()-end)<0)Sleep(10);Check(resource_gpu::Snapshot().entries==0&&resource_gpu::Snapshot().allocatedBytes==0,"RAM pressure releases standby source pins with healthy GPU budget");
    end=GetTickCount()+1000;while(fixtureFiles[j.paths[0]].use_count()!=1&&LONG(GetTickCount()-end)<0)Sleep(1);Check(fixtureFiles[j.paths[0]].use_count()==1,"RAM pressure drops retained DDS FilePtr pins");
    resource_gpu::Job ramPaused=j;ramPaused.key="RAM-paused";Check(QueueRetry(ramPaused),"RAM pressure preserves new queued job");Sleep(350);Check(resource_gpu::Snapshot().pending==1&&resource_gpu::Snapshot().allocatedBytes==0,"RAM pressure pauses new GPU allocation");
    InterlockedExchange(&fixtureRamAllowed,1);r=WaitReady(ramPaused.key,7);Check(bool(r),"RAM recovery resumes preserved queued job");r.reset();Check(QueueRetry(j),"queue after RAM recovery");end=GetTickCount()+6000;while(!resource_gpu::Snapshot().entries&&LONG(GetTickCount()-end)<0)Sleep(10);Check(resource_gpu::Snapshot().entries==1,"standby ready after RAM recovery");
    resource_gpu::Job longKey=j;longKey.key="Terrain0_"+std::string(255,'S')+"|1234567890abcdef";for(unsigned i=0;i<6;++i)longKey.paths[i]="D:/"+std::string(170,'p')+"/terrain_layer"+char('0'+i)+".dds";
    resource_gpu::SetWorkAllowed(false);Check(longKey.key.size()>256&&longKey.key.size()<512&&QueueRetry(longKey),"realistic long native-name hash key accepted");longKey.key=std::string(513,'x');Check(!resource_gpu::Queue(longKey),"key over512 rejected");resource_gpu::SetWorkAllowed(true);
    resource_gpu::SetWorkAllowed(false);Sleep(350);Check(resource_gpu::Snapshot().entries==1,"closed work gate preserves ready standby");resource_gpu::SetWorkAllowed(true);resource_gpu::Configure(true,1ULL<<30,8);Check(!resource_gpu::TakeReady(j.key,7),"world generation cancellation");end=GetTickCount()+3000;while(resource_gpu::Snapshot().allocatedBytes&&LONG(GetTickCount()-end)<0)Sleep(10);Check(resource_gpu::Snapshot().allocatedBytes==0,"cancelled standby released");
    j.worldGeneration=8;j.key="pressure";Check(QueueRetry(j),"queue pressure target");end=GetTickCount()+6000;while(!resource_gpu::Snapshot().entries&&LONG(GetTickCount()-end)<0)Sleep(10);Check(resource_gpu::Snapshot().entries==1,"pressure standby created");InterlockedExchange(&fixturePressure,1);end=GetTickCount()+3000;while(resource_gpu::Snapshot().allocatedBytes&&LONG(GetTickCount()-end)<0)Sleep(10);Check(resource_gpu::Snapshot().pressure&&resource_gpu::Snapshot().allocatedBytes==0,"device pressure releases standby");
    for(unsigned n=0;n<32;++n){char key[50];std::sprintf(key,"bounded-%u",n);j.key=key;Check(QueueRetry(j),"bounded queue accepts 32");}j.key="overflow";Check(!resource_gpu::Queue(j),"queue overflow refused");resource_gpu::Configure(false,1ULL<<30,8);Check(!resource_gpu::Queue(j)&&resource_gpu::Snapshot().pending==0,"disable clears queue");
    InterlockedExchange(&fixturePressure,0);BlockingProbe block={CreateEvent(NULL,TRUE,FALSE,NULL),CreateEvent(NULL,TRUE,FALSE,NULL)};resource_gpu::SetBudgetProbe(&BlockProbe,&block);resource_gpu::Configure(true,1ULL<<30,9);Check(WaitForSingleObject(block.entered,3000)==WAIT_OBJECT_0,"blocked probe entered");DWORD start=GetTickCount();Check(!resource_gpu::Stop(250)&&GetTickCount()-start<600,"bounded stop leaves blocked worker pinned");SetEvent(block.release);Check(resource_gpu::Stop(250),"worker drains and joins");Check(resource_gpu::Snapshot().allocatedBytes==0,"shutdown releases all GPU candidates");CloseHandle(block.entered);CloseHandle(block.release);
}
int main(){
    Check(resource_gpu::BudgetRoom(4ULL<<30,2ULL<<30,64<<20),"budget room");Check(!resource_gpu::BudgetRoom(4ULL<<30,4ULL<<30,0),"budget pressure");Check(!resource_gpu::BudgetRoom(100,101,0),"usage over budget");
    Check(resource_gpu::CacheRoom(1024,1000,24),"exact cap admitted");Check(!resource_gpu::CacheRoom(1024,1000,25),"over cap refused");Check(!resource_gpu::CacheRoom(1024,0,~0ULL),"cap subtraction overflow refused");Check(!resource_gpu::BudgetRoom(~0ULL,~0ULL-1,8),"budget subtraction overflow refused");
    ID3D11Device*d=NULL;ID3D11DeviceContext*c=NULL;D3D_FEATURE_LEVEL level;HRESULT hr=D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_WARP,NULL,0,NULL,0,D3D11_SDK_VERSION,&d,&level,&c);Check(SUCCEEDED(hr),"WARP creation");
#ifdef RESOURCE_GPU_INCLUDE_STORE
    EvictionTests(d);
#endif
    for(unsigned type=0;type<2;++type){
        resource_gpu::Job j;j.key=type?"test-normal":"test-diffuse";j.worldGeneration=7;j.type=type;resource_cache::FilePtr files[6];for(unsigned l=0;l<6;++l)files[l]=Source(type,l);
        resource_gpu::ReadyPtr r;std::string error;Check(resource_gpu::BuildTexture(d,j,files,&r,&error),error.c_str());Check(r->desc.ArraySize==6&&r->desc.MipLevels==4,"six layers native mip count");
        ID3D11Texture2D*t=r->AddRefTexture();Check(t!=NULL,"owned texture");D3D11_TEXTURE2D_DESC sd=r->desc;sd.Usage=D3D11_USAGE_STAGING;sd.BindFlags=0;sd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ID3D11Texture2D*s=NULL;Check(SUCCEEDED(d->CreateTexture2D(&sd,NULL,&s)),"staging creation");c->CopyResource(s,t);
        for(unsigned sub=0;sub<r->subresources.size();++sub){
            D3D11_MAPPED_SUBRESOURCE m={};Check(SUCCEEDED(c->Map(s,sub,D3D11_MAP_READ,0,&m)),"map readback");
            unsigned layer=sub/4,mip=sub%4,side=256>>mip,pitch=((side+3)/4)*(type?8:16);const resource_gpu::Span&sp=r->subresources[sub];
            Check(sp.width==side&&sp.height==side&&sp.rowPitch==pitch,"independent native span shape");Check(m.RowPitch>=pitch,"readback pitch");
            for(unsigned row=0;row<(side+3)/4;++row)for(unsigned col=0;col<pitch;++col){unsigned char expected=unsigned char(((row*pitch+col)*7+layer*29+mip*11)&255);Check(static_cast<const unsigned char*>(m.pData)[size_t(row)*m.RowPitch+col]==expected,"independent expected BC byte each layer/mip");Check(sp.data[size_t(row)*pitch+col]==expected,"expected source spans match independent bytes");}
            c->Unmap(s,sub);
        }
        s->Release();t->Release();r.reset();
        files[5]=Source(1-type,5);Check(!resource_gpu::BuildTexture(d,j,files,&r,&error),"mixed source formats refused");
    }
    std::printf("WARP concurrent-creation admission=%d\n",int(resource_gpu::DeviceCanCreateConcurrently(d)));
    if(resource_gpu::DeviceCanCreateConcurrently(d))WorkerTests(d);else{std::puts("FAIL WARP does not expose concurrent creation; worker lifecycle fixture unavailable");return 1;}
    resource_gpu::U64 budget=0,usage=0;std::printf("WARP real adapter3 local-budget probe=%d\n",int(resource_gpu::ProbeLocalBudget(d,&budget,&usage,NULL)));
    ID3D11Device*single=NULL;ID3D11DeviceContext*singleContext=NULL;Check(SUCCEEDED(D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_WARP,NULL,D3D11_CREATE_DEVICE_SINGLETHREADED,NULL,0,D3D11_SDK_VERSION,&single,&level,&singleContext)),"single-threaded WARP fixture");Check(!resource_gpu::DeviceCanCreateConcurrently(single),"SINGLETHREADED device rejected");singleContext->Release();single->Release();
    c->Release();d->Release();std::puts("GPU production DDS initialization WARP readback PASS");return 0;
}
