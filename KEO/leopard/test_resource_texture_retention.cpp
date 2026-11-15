// Real Ogre resource/manager/TexturePtr subclasses; only reservation/owner are
// substituted. No renderer or GPU creation, file IO or native purge override.
#define RESOURCE_TEXTURE_RETENTION_TEST
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <iostream>
#include <ogre/OgreRoot.h>
#include <ogre/OgreTextureManager.h>
#include <ogre/OgreMeshManager.h>
#include <ogre/OgreHardwarePixelBuffer.h>
#include "resources/resource_texture_retention.h"
#include "resources/resource_gpu_store.h"
static bool mainOwner=true,budgetAllowed=true;
static resource_gpu::U64 reserved=0;
static unsigned deviceGeneration=1,ownerChecks=0;
bool IsMainThread(){++ownerChecks;return mainOwner;}
namespace resource_gpu {
bool TryTextureRetentionReservation(U64 n,U64 cap){if(n>reserved&&(!budgetAllowed||n>cap))return false;reserved=n;return true;}
bool TextureRetentionStandbyAllowed(U64 cap){return budgetAllowed&&reserved<=cap;}
U64 TextureRetentionReservation(){return reserved;}
unsigned DeviceGeneration(){return deviceGeneration;}
}
#include "../src/resources/resource_texture_retention.cpp"
static bool inspectImpl=false,loadedInsideImpl=true;
static unsigned pendingInsideImpl=0,texturesInsideImpl=0;
static resource_gpu::U64 bytesInsideImpl=0;
static const Ogre::TexturePtr*watchOutput=NULL;
static unsigned maxObservedRefs=0;
struct Texture:Ogre::Texture {
    bool fail,failObserve,failDescribe;unsigned loads,unloads;
    Texture(Ogre::ResourceManager*m,const Ogre::String&n,Ogre::ResourceHandle h,const Ogre::String&g,bool manual,Ogre::ManualResourceLoader*l):
        Ogre::Texture(m,n,h,g,manual,l),fail(false),failObserve(false),failDescribe(false),loads(0),unloads(0){setWidth(16);setHeight(8);setDepth(1);setFormat(Ogre::PF_A8R8G8B8);setNumMipmaps(0);setTextureType(Ogre::TEX_TYPE_2D);setUsage(Ogre::TU_STATIC_WRITE_ONLY);}
    Ogre::ResourceManager*getCreator(){if(failObserve)throw 92;return Ogre::Resource::getCreator();}
    size_t getSize()const{if(watchOutput&&watchOutput->get()==this)maxObservedRefs=(std::max)(maxObservedRefs,watchOutput->useCount());if(failDescribe)throw 93;return Ogre::Resource::getSize();}
    void SetSize(size_t n){mSize=n;}
    void prepareImpl(){}void unprepareImpl(){}
    void Body(){++loads;if(fail)throw 91;}
    void loadImpl(){resource_texture_retention::LoadImpl(this);if(inspectImpl){loadedInsideImpl=isLoaded();resource_texture_retention::Stats s=resource_texture_retention::Snapshot();pendingInsideImpl=s.pending;texturesInsideImpl=s.textures;bytesInsideImpl=s.heldBytes;}}
    void unloadImpl(){++unloads;}
    void createInternalResourcesImpl(){}void freeInternalResourcesImpl(){}
    Ogre::HardwarePixelBufferSharedPtr getBuffer(size_t,size_t){assert(false);return Ogre::HardwarePixelBufferSharedPtr();}
    size_t NativeCalculateSize(){return Ogre::Texture::calculateSize();}
};
struct Manager:Ogre::TextureManager {
    const Ogre::String*nameSeen,*groupSeen;Ogre::TextureType typeSeen;int mipsSeen;float gammaSeen;bool alphaSeen,hwSeen,manualSeen;Ogre::PixelFormat formatSeen;
    Manager():nameSeen(NULL),groupSeen(NULL),typeSeen(Ogre::TEX_TYPE_1D),mipsSeen(0),gammaSeen(0),alphaSeen(false),hwSeen(false),manualSeen(true),formatSeen(Ogre::PF_UNKNOWN){}
    ResourceCreateOrRetrieveResult createOrRetrieve(const Ogre::String&n,const Ogre::String&g,bool manual,Ogre::ManualResourceLoader*l,const Ogre::NameValuePairList*p,Ogre::TextureType type,int mips,Ogre::Real gamma,bool alpha,Ogre::PixelFormat format,bool hw){
        nameSeen=&n;groupSeen=&g;manualSeen=manual;typeSeen=type;mipsSeen=mips;gammaSeen=gamma;alphaSeen=alpha;formatSeen=format;hwSeen=hw;
        return Ogre::TextureManager::createOrRetrieve(n,g,manual,l,p,type,mips,gamma,alpha,format,hw);
    }
    Ogre::Resource*createImpl(const Ogre::String&n,Ogre::ResourceHandle h,const Ogre::String&g,bool manual,Ogre::ManualResourceLoader*l,const Ogre::NameValuePairList*){return OGRE_NEW Texture(this,n,h,g,manual,l);}
    Ogre::PixelFormat getNativeFormat(Ogre::TextureType,Ogre::PixelFormat f,int){return f;}
    bool isHardwareFilteringSupported(Ogre::TextureType,Ogre::PixelFormat,int,bool){return true;}
};
struct Loader:Ogre::ManualResourceLoader {void loadResource(Ogre::Resource*){}};
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"line "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(0)
static void TickNow(bool on=true,resource_texture_retention::U64 world=1,resource_texture_retention::U64 cap=1ULL<<30){
    resource_texture_retention::state->ticked=false;resource_texture_retention::Tick(on,cap,world);
}
static void LoadNative(Ogre::Resource*r,bool background){
    typedef void(*F)(Ogre::Resource*,bool);
    static F f=reinterpret_cast<F>(GetProcAddress(GetModuleHandleA("OgreMain_x64.dll"),"?load@Resource@Ogre@@UEAAX_N@Z"));f(r,background);
}
static unsigned forwarded=0;
static resource_texture_retention::ManagerLoadFn nativeManagerLoad=NULL;
static Ogre::TexturePtr*ManagerSpy(Ogre::TextureManager*self,Ogre::TexturePtr*out,const Ogre::String&name,const Ogre::String&group,Ogre::TextureType type,int mips,float gamma,bool alpha,Ogre::PixelFormat format,bool hw){
    ++forwarded;return nativeManagerLoad(self,out,name,group,type,mips,gamma,alpha,format,hw);
}
static void ImplSpy(Ogre::Texture*r){static_cast<Texture*>(r)->Body();}
struct OutSlot {
    resource_gpu::U64 before;
    __declspec(align(16)) unsigned char storage[sizeof(Ogre::TexturePtr)];
    resource_gpu::U64 after;
    OutSlot():before(0x1122334455667788ULL),after(0x8877665544332211ULL){memset(storage,0xcc,sizeof(storage));}
    Ogre::TexturePtr*ptr(){return reinterpret_cast<Ogre::TexturePtr*>(storage);}
    void Destroy(){typedef Ogre::TexturePtr P;ptr()->~P();}
};
static Ogre::TexturePtr*Request(Manager&m,OutSlot&out,const Ogre::String&name,const Ogre::String&group){
    return resource_texture_retention::ManagerLoad(&m,out.ptr(),name,group,Ogre::TEX_TYPE_2D,3,1.375f,true,Ogre::PF_A8R8G8B8,true);
}
static bool WorkerStateProbe(Texture*r,Manager*m,OutSlot*out,const Ogre::String*name,const Ogre::String*group){
    // A worker must not even read owner-only on/containers. This protected
    // State makes accidental owner-state reads observable without a race.
    __try{resource_texture_retention::Observe(r);resource_texture_retention::LoadImpl(r);Request(*m,*out,*name,*group);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static LONG WINAPI Crash(EXCEPTION_POINTERS*p){std::cerr<<"crash RIP="<<std::hex<<p->ContextRecord->Rip<<" image="<<(void*)GetModuleHandle(NULL)<<" Ogre="<<(void*)GetModuleHandleA("OgreMain_x64.dll")<<"\n";return EXCEPTION_EXECUTE_HANDLER;}
static void CompleteWithoutLoadImpl(Texture*t){
    // External image completion calculates size then publishes LOADED, without
    // Resource.load or either capture hook. Use actual native calculation and
    // native state publication; GPU work itself is outside this fixture.
    t->SetSize(t->NativeCalculateSize());
    typedef void(*F)(Ogre::Resource*);
    static F loaded=reinterpret_cast<F>(GetProcAddress(GetModuleHandleA("OgreMain_x64.dll"),"?setToLoaded@Resource@Ogre@@UEAAXXZ"));loaded(t);
}
static bool LoadedResultProbe(void*out){
    __try{resource_texture_retention::ObserveLoadedResult(out);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static int TestLoadedResult(Manager&manager){
    resource_texture_retention::Clear();TickNow();
    Ogre::TexturePtr completed=manager.create("loader-output.dds","General");Texture*t=static_cast<Texture*>(completed.get());
    resource_texture_retention::Stats baseline=resource_texture_retention::Snapshot();
    CompleteWithoutLoadImpl(t);CHECK(t->isLoaded()&&t->loads==0);
    CHECK(resource_texture_retention::Snapshot().managerRequests==baseline.managerRequests&&resource_texture_retention::Snapshot().loadCompletions==baseline.loadCompletions);
    unsigned refs=completed.useCount();resource_texture_retention::ObserveLoadedResult(&completed);
    CHECK(completed.useCount()==refs&&resource_texture_retention::Snapshot().pending==1&&resource_texture_retention::Snapshot().textures==0&&reserved==0);
    CHECK(resource_texture_retention::Snapshot().loaderCompletions==baseline.loaderCompletions+1);
    resource_texture_retention::ObserveLoadedResult(&completed);CHECK(completed.useCount()==refs&&resource_texture_retention::Snapshot().pending==1);
    TickNow();CHECK(resource_texture_retention::Snapshot().textures==1&&completed.useCount()==refs+1&&reserved>0);
    resource_texture_retention::U64 priorHits=resource_texture_retention::Snapshot().reused,oldBytes=reserved;
    watchOutput=&completed;maxObservedRefs=0;
    resource_texture_retention::ObserveLoadedResult(&completed);
    watchOutput=NULL;CHECK(maxObservedRefs==refs+1); // no transient SharedPtr copy, either
    CHECK(resource_texture_retention::Snapshot().reused==priorHits+1&&completed.useCount()==refs+1&&reserved==oldBytes);
    t->setWidth(1024);t->setHeight(1024);t->setNumMipmaps(10);CompleteWithoutLoadImpl(t);
    resource_texture_retention::ObserveLoadedResult(&completed);
    CHECK(reserved==0&&resource_texture_retention::Snapshot().textures==0&&resource_texture_retention::Snapshot().pending==1&&completed.useCount()==refs);
    CHECK(resource_texture_retention::Snapshot().reused==priorHits+1);
    TickNow();CHECK(reserved>oldBytes&&completed.useCount()==refs+1);
    t->setWidth(16384);t->setHeight(16384);CompleteWithoutLoadImpl(t);resource_texture_retention::ObserveLoadedResult(&completed);
    CHECK(reserved==0&&resource_texture_retention::Snapshot().pending==1&&completed.useCount()==refs);
    TickNow();CHECK(reserved==0&&resource_texture_retention::Snapshot().textures==0);
    t->setWidth(16);t->setHeight(8);t->setNumMipmaps(0);CompleteWithoutLoadImpl(t);resource_texture_retention::ObserveLoadedResult(&completed);TickNow();CHECK(reserved>0);
    t->setUsage(Ogre::TU_DYNAMIC_WRITE_ONLY);CompleteWithoutLoadImpl(t);resource_texture_retention::ObserveLoadedResult(&completed);
    CHECK(reserved==0&&completed.useCount()==refs);TickNow();CHECK(resource_texture_retention::Snapshot().textures==0);
    t->setUsage(Ogre::TU_STATIC_WRITE_ONLY);CompleteWithoutLoadImpl(t);resource_texture_retention::ObserveLoadedResult(&completed);TickNow();CHECK(reserved>0);
    t->setTextureType(Ogre::TEX_TYPE_2D_ARRAY);resource_texture_retention::ObserveLoadedResult(&completed);CHECK(reserved==0);TickNow();CHECK(resource_texture_retention::Snapshot().textures==0);t->setTextureType(Ogre::TEX_TYPE_2D);
    CompleteWithoutLoadImpl(t);resource_texture_retention::ObserveLoadedResult(&completed);TickNow();CHECK(reserved>0);
    t->failDescribe=true;bool noThrow=true;try{resource_texture_retention::ObserveLoadedResult(&completed);}catch(...){noThrow=false;}t->failDescribe=false;
    CHECK(noThrow&&reserved==0&&completed.useCount()==refs);
    t->failObserve=true;try{resource_texture_retention::ObserveLoadedResult(&completed);}catch(...){noThrow=false;}t->failObserve=false;
    CHECK(noThrow&&resource_texture_retention::Snapshot().pending==0&&completed.useCount()==refs);
    resource_texture_retention::U64 captured=resource_texture_retention::Snapshot().loaderCompletions;
    resource_texture_retention::ObserveLoadedResult(NULL);Ogre::TexturePtr empty;resource_texture_retention::ObserveLoadedResult(&empty);
    CHECK(resource_texture_retention::Snapshot().loaderCompletions==captured&&completed.useCount()==refs);
    resource_texture_retention::State*saved=resource_texture_retention::state;
    void*inaccessible=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);CHECK(inaccessible!=NULL);
    mainOwner=false;resource_texture_retention::state=static_cast<resource_texture_retention::State*>(inaccessible);
    bool ownerSafe=LoadedResultProbe(inaccessible);resource_texture_retention::state=saved;mainOwner=true;CHECK(ownerSafe);
    resource_texture_retention::Clear();unsigned beforeOwnerChecks=ownerChecks;
    resource_texture_retention::state=static_cast<resource_texture_retention::State*>(inaccessible);
    bool offSafe=LoadedResultProbe(inaccessible);resource_texture_retention::state=saved;CHECK(offSafe&&ownerChecks==beforeOwnerChecks);
    VirtualFree(inaccessible,0,MEM_RELEASE);
    CHECK(resource_texture_retention::Snapshot().loaderCompletions==captured&&completed.useCount()==refs);
    Ogre::ResourceHandle handle=completed->getHandle();completed.setNull();manager.remove(handle);TickNow();return 0;
}
int main(){
    SetUnhandledExceptionFilter(Crash);
    Ogre::Root root("","","");Manager manager;
    resource_texture_retention::originalLoadImpl=&ImplSpy;
    nativeManagerLoad=reinterpret_cast<resource_texture_retention::ManagerLoadFn>(GetProcAddress(GetModuleHandleA("OgreMain_x64.dll"),"?load@TextureManager@Ogre@@UEAA?AV?$SharedPtr@VTexture@Ogre@@@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0W4TextureType@2@HM_NW4PixelFormat@2@2@Z"));
    resource_texture_retention::originalManagerLoad=&ManagerSpy;
    Ogre::TexturePtr a=manager.create("native-A.dds","General");a->load();
    Texture*t=static_cast<Texture*>(a.get());
    CHECK(a->isLoaded());CHECK(a->getSize()==t->NativeCalculateSize());
    CHECK(a->getSize()>=512);CHECK(t->loads==1&&t->unloads==0);
    resource_texture_retention::Init();TickNow();
    CHECK(TestLoadedResult(manager)==0);
    resource_texture_retention::U64 initialReuse=resource_texture_retention::Snapshot().reused;
    unsigned before=a.useCount();resource_texture_retention::Observe(a.get());
    CHECK(a.useCount()==before);
    CHECK(resource_texture_retention::Snapshot().pending==1);
    resource_texture_retention::Observe(a.get());CHECK(resource_texture_retention::Snapshot().pending==1);
    TickNow();
    CHECK(resource_texture_retention::Snapshot().textures==1);
    CHECK(a.useCount()==before+1);CHECK(reserved==a->getSize());
    resource_texture_retention::Observe(a.get());CHECK(resource_texture_retention::Snapshot().reused==initialReuse+1);
    // Actual manager purge compares native useCount against baseline3. Our
    // retained TexturePtr is exactly one controlling reference, not a mock.
    typedef void(*Purge)(Ogre::ResourceManager*,bool);
    Purge purge=reinterpret_cast<Purge>((*reinterpret_cast<void***>(&manager))[0x68/8]);
    Ogre::Texture*raw=a.get();a.setNull();purge(&manager,false);CHECK(raw->isLoaded());
    budgetAllowed=false;TickNow();CHECK(reserved==0&&resource_texture_retention::Snapshot().textures==0);
    purge(&manager,false);CHECK(!raw->isLoaded());budgetAllowed=true;
    a=manager.getByName("native-A.dds");a->load();t=static_cast<Texture*>(a.get());
    // Actual TextureManager.load ABI: name/group in R8/R9, all six stack
    // arguments including float/bools/enums reach real createOrRetrieve.
    resource_texture_retention::Clear();TickNow();forwarded=0;
    Ogre::String requestName="native-A.dds",requestGroup="General";OutSlot out;
    before=a.useCount();CHECK(Request(manager,out,requestName,requestGroup)==out.ptr());
    CHECK(forwarded==1&&out.ptr()->get()==t&&a.useCount()==before+1);
    CHECK(manager.nameSeen==&requestName&&manager.groupSeen==&requestGroup&&!manager.manualSeen);
    CHECK(manager.typeSeen==Ogre::TEX_TYPE_2D&&manager.mipsSeen==3&&manager.gammaSeen==1.375f&&manager.alphaSeen&&manager.formatSeen==Ogre::PF_A8R8G8B8&&manager.hwSeen);
    CHECK(out.before==0x1122334455667788ULL&&out.after==0x8877665544332211ULL);
    CHECK(resource_texture_retention::Snapshot().managerRequests==1&&resource_texture_retention::Snapshot().pending==1);
    out.Destroy();CHECK(a.useCount()==before);TickNow();
    OutSlot hit;resource_texture_retention::U64 priorHits=resource_texture_retention::Snapshot().reused;
    CHECK(Request(manager,hit,requestName,requestGroup)==hit.ptr());hit.Destroy();
    CHECK(resource_texture_retention::Snapshot().reused==priorHits+1);
    OutSlot alternate;before=a.useCount();
    CHECK(resource_texture_retention::ManagerLoad(&manager,alternate.ptr(),requestName,requestGroup,Ogre::TEX_TYPE_CUBE_MAP,-1,0.8125f,false,Ogre::PF_DXT5,false)==alternate.ptr());
    CHECK(manager.typeSeen==Ogre::TEX_TYPE_CUBE_MAP&&manager.mipsSeen==-1&&manager.gammaSeen==0.8125f&&!manager.alphaSeen&&manager.formatSeen==Ogre::PF_DXT5&&!manager.hwSeen);
    CHECK(a.useCount()==before+1);alternate.Destroy();
    // One million real generic Resource.load calls on an already loaded
    // texture never enter either narrow capture hook or touch its counters.
    resource_texture_retention::Stats hotBefore=resource_texture_retention::Snapshot();unsigned nativeLoads=t->loads;
    for(unsigned n=0;n<1000000;++n)LoadNative(t,false);
    resource_texture_retention::Stats hotAfter=resource_texture_retention::Snapshot();
    CHECK(hotAfter.observed==hotBefore.observed&&hotAfter.reused==hotBefore.reused&&hotAfter.managerRequests==hotBefore.managerRequests&&hotAfter.loadCompletions==hotBefore.loadCompletions&&hotAfter.loaderCompletions==hotBefore.loaderCompletions&&t->loads==nativeLoads);
    resource_texture_retention::Clear();TickNow();t->failObserve=true;OutSlot captureFault;
    CHECK(Request(manager,captureFault,requestName,requestGroup)==captureFault.ptr());captureFault.Destroy();t->failObserve=false;
    CHECK(resource_texture_retention::Snapshot().pending==0);
    a->unload();t->fail=true;bool threw=false;OutSlot failed;
    resource_texture_retention::Stats failureBefore=resource_texture_retention::Snapshot();unsigned priorForwarded=forwarded,failRefs=a.useCount();
    try{Request(manager,failed,requestName,requestGroup);}catch(int v){threw=v==91;}
    CHECK(threw&&forwarded==priorForwarded+1);
    CHECK(a.useCount()==failRefs);
    CHECK(resource_texture_retention::Snapshot().loadCompletions==failureBefore.loadCompletions&&resource_texture_retention::Snapshot().managerRequests==failureBefore.managerRequests&&resource_texture_retention::Snapshot().pending==0);
    t->fail=false;a->load();resource_texture_retention::Clear();TickNow();
    resource_texture_retention::State*savedState=resource_texture_retention::state;
    void*inaccessible=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);CHECK(inaccessible!=NULL);
    resource_texture_retention::state=static_cast<resource_texture_retention::State*>(inaccessible);mainOwner=false;
    OutSlot workerOut;bool workerSafe=WorkerStateProbe(t,&manager,&workerOut,&requestName,&requestGroup);mainOwner=true;resource_texture_retention::state=savedState;
    VirtualFree(inaccessible,0,MEM_RELEASE);CHECK(workerSafe);
    workerOut.Destroy();
    // Disabled wrappers do not query the owner or read State at all.
    resource_texture_retention::Clear();unsigned ownerBefore=ownerChecks;OutSlot disabled;
    CHECK(Request(manager,disabled,requestName,requestGroup)==disabled.ptr());disabled.Destroy();
    CHECK(ownerChecks==ownerBefore);TickNow();
    // Native explicit reload is not a hit on an already loaded texture. The
    // old persistent pin/accounting must disappear before the wrapper returns,
    // then bounded Tick separately admits the new actual loaded byte size.
    resource_texture_retention::Observe(t);TickNow();CHECK(reserved>0);
    resource_texture_retention::U64 priorReused=resource_texture_retention::Snapshot().reused;
    t->unload();t->setWidth(2048);t->setHeight(2048);unsigned reloadedRefs=a.useCount();
    inspectImpl=true;LoadNative(t,false);inspectImpl=false;
    CHECK(!loadedInsideImpl&&pendingInsideImpl==1&&texturesInsideImpl==0&&bytesInsideImpl==0);
    CHECK(t->isLoaded()&&reserved==0&&resource_texture_retention::Snapshot().textures==0);
    CHECK(resource_texture_retention::Snapshot().pending==1&&a.useCount()==reloadedRefs-1);
    CHECK(resource_texture_retention::Snapshot().reused==priorReused);
    TickNow();resource_texture_retention::U64 resized=0;
    CHECK(resource_texture_retention::Describe(t,&resized)&&reserved==resized&&resized>4ULL*1024*1024);
    t->unload();t->setWidth(16384);t->setHeight(16384);LoadNative(t,false);
    CHECK(reserved==0&&resource_texture_retention::Snapshot().pending==1);
    TickNow();CHECK(reserved==0&&resource_texture_retention::Snapshot().textures==0);
    CHECK(resource_texture_retention::Snapshot().reused==priorReused);
    t->unload();t->setWidth(16);t->setHeight(8);t->load();resource_texture_retention::Clear();TickNow();
    // Nontexture Resource::load belongs to a different manager and is rejected
    // before any Texture-only operation or cast.
    Ogre::MeshPtr mesh=Ogre::MeshManager::getSingleton().createManual("not-texture","General");
    unsigned observed=unsigned(resource_texture_retention::Snapshot().observed);
    resource_texture_retention::Observe(mesh.get());CHECK(resource_texture_retention::Snapshot().observed==observed);mesh.setNull();
    // An observation may disappear from the manager before its bounded tick.
    Ogre::TexturePtr stale=manager.create("stale.dds","General");stale->load();
    unsigned staleRefs=stale.useCount();resource_texture_retention::Observe(stale.get());CHECK(stale.useCount()==staleRefs);
    manager.remove(stale->getHandle());TickNow();CHECK(resource_texture_retention::Snapshot().textures==0);CHECK(resource_texture_retention::Snapshot().identityMismatch>0);
    stale.setNull();
    // Pending resources do not block native purging, even a loaded candidate.
    Ogre::TexturePtr pending=manager.create("pending.dds","General");pending->load();
    resource_texture_retention::Observe(pending.get());raw=pending.get();pending.setNull();purge(&manager,false);CHECK(!raw->isLoaded());
    TickNow();CHECK(resource_texture_retention::Snapshot().pending==1&&reserved==0);
    resource_texture_retention::state->pending[resource_texture_retention::state->read].time=GetTickCount()-30000;
    TickNow();CHECK(resource_texture_retention::Snapshot().pending==0);CHECK(resource_texture_retention::Snapshot().loadTimeout==1);
    resource_texture_retention::Observe(a.get());
    resource_texture_retention::state->pending[resource_texture_retention::state->read].id=raw;
    unsigned mismatches=unsigned(resource_texture_retention::Snapshot().identityMismatch);TickNow();
    CHECK(resource_texture_retention::Snapshot().identityMismatch==mismatches+1&&reserved==0);
    // createManual's actual native manual=true argument is verified offline.
    // Calling that renderer entrypoint needs a real renderer; use the same
    // native manager.create manual flag here without inventing a renderer.
    resource_texture_retention::U64 bytes=0;
    Loader loader;Ogre::TexturePtr generated=manager.create("generated.dds","General",true,&loader);generated->load();
    CHECK(generated->isManuallyLoaded());CHECK(!resource_texture_retention::Describe(generated.get(),&bytes));
    CHECK(resource_texture_retention::Describe(a.get(),&bytes));
    t->setUsage(Ogre::TU_DYNAMIC_WRITE_ONLY);CHECK(!resource_texture_retention::Describe(t,&bytes));
    t->setUsage(Ogre::TU_RENDERTARGET);CHECK(!resource_texture_retention::Describe(t,&bytes));t->setUsage(Ogre::TU_STATIC_WRITE_ONLY);
    t->setFSAA(2,"",false);CHECK(!resource_texture_retention::Describe(t,&bytes));t->setFSAA(0,"",false);
    t->setTextureType(Ogre::TEX_TYPE_CUBE_MAP);CHECK(!resource_texture_retention::Describe(t,&bytes));t->setTextureType(Ogre::TEX_TYPE_2D);
    t->setWidth(0);CHECK(!resource_texture_retention::Describe(t,&bytes));t->setWidth(16);
    size_t realSize=t->getSize();t->SetSize(0);CHECK(!resource_texture_retention::Describe(t,&bytes));t->SetSize(realSize);
    Ogre::TexturePtr png=manager.create("other.png","General");png->load();CHECK(!resource_texture_retention::Describe(png.get(),&bytes));png.setNull();
    // Mips matter: native calculateSize includes just base pixels plus CPU
    // overhead, while the conservative mip chain can be larger.
    t->setWidth(1024);t->setHeight(1024);t->setNumMipmaps(10);t->unload();t->load();
    CHECK(resource_texture_retention::Describe(t,&bytes));CHECK(bytes==Ogre::Image::calculateSize(10,1,1024,1024,1,Ogre::PF_A8R8G8B8));CHECK(bytes>t->getSize());
    resource_texture_retention::Observe(t);TickNow();CHECK(reserved==bytes);
    t->setWidth(512);TickNow();CHECK(reserved<bytes&&reserved>0); // native size metadata change refresh
    t->unload();TickNow();CHECK(reserved==0);t->load();resource_texture_retention::Observe(t);TickNow();CHECK(reserved>0);
    manager.remove(t->getHandle());TickNow();CHECK(reserved==0);a.setNull();
    a=manager.create("lifecycle.dds","General");a->load();
    resource_texture_retention::Observe(a.get());TickNow();CHECK(reserved>0);
    mainOwner=false;resource_texture_retention::Clear();resource_texture_retention::Observe(a.get());mainOwner=true;CHECK(reserved>0);
    ++deviceGeneration;TickNow();CHECK(reserved==0&&resource_texture_retention::Snapshot().pending==0);
    resource_texture_retention::Observe(a.get());TickNow();CHECK(reserved>0);
    TickNow(true,2);CHECK(reserved==0);resource_texture_retention::Observe(a.get());TickNow(true,2);CHECK(reserved>0);
    TickNow(false,2);CHECK(reserved==0);resource_texture_retention::Observe(a.get());CHECK(resource_texture_retention::Snapshot().pending==0);
    TickNow();
    // A single native loaded surface larger than the shared ceiling is never
    // pinned, and a cap-saturated admission uses bounded LRU reclamation.
    Ogre::TexturePtr giant=manager.create("giant.dds","General");giant->setWidth(16384);giant->setHeight(16384);giant->load();
    resource_texture_retention::Observe(giant.get());TickNow();CHECK(reserved==0&&resource_texture_retention::Snapshot().textures==0);giant.setNull();
    // At most four promotions and two refreshes each250ms. Native resource
    // pointers are never stored in the queue; repeat enqueue remains bounded.
    for(unsigned n=0;n<140;++n){char name[40];sprintf_s(name,"queue-%u.dds",n);Ogre::TexturePtr p=manager.create(name,"General");p->load();resource_texture_retention::Observe(p.get());}
    CHECK(resource_texture_retention::Snapshot().pending==128);CHECK(resource_texture_retention::Snapshot().queueFull>=12);
    TickNow();CHECK(resource_texture_retention::Snapshot().textures==4&&resource_texture_retention::Snapshot().pending==124);
    resource_texture_retention::Tick(true,1ULL<<30,1);CHECK(resource_texture_retention::Snapshot().textures==4);
    TickNow(true,3);CHECK(reserved==0&&resource_texture_retention::Snapshot().pending==0);TickNow();
    //1024 bounded loaded TexturePtrs, LRU eviction retains a freshly reused
    //entry; pressure releases <=32 own references each heartbeat.
    for(unsigned n=0;n<1030;++n){char name[40];sprintf_s(name,"cap-%u.dds",n);Ogre::TexturePtr p=manager.create(name,"General");p->load();resource_texture_retention::Observe(p.get());if(n%4==3)TickNow();}
    TickNow();CHECK(resource_texture_retention::Snapshot().textures==1024);
    Ogre::TexturePtr oldest=manager.getByName("cap-6.dds");CHECK(resource_texture_retention::state->textures.count(oldest.get())==1);
    resource_texture_retention::Observe(oldest.get());
    Ogre::TexturePtr extra=manager.create("cap-extra.dds","General");extra->load();resource_texture_retention::Observe(extra.get());TickNow();
    CHECK(resource_texture_retention::Snapshot().textures==1024&&resource_texture_retention::state->textures.count(oldest.get())==1);
    resource_gpu::U64 fullReserved=reserved;budgetAllowed=false;TickNow();CHECK(resource_texture_retention::Snapshot().textures==992&&resource_texture_retention::Snapshot().pressure&&reserved<fullReserved);
    resource_texture_retention::Tick(true,1ULL<<30,1);CHECK(resource_texture_retention::Snapshot().textures==992);
    TickNow();CHECK(resource_texture_retention::Snapshot().textures==960);budgetAllowed=true;
    oldest.setNull();extra.setNull();generated.setNull();
    resource_texture_retention::Clear();a.setNull();manager.removeAll();
    std::cout<<"texture actual Ogre checks="<<checks<<"\n";return 0;
}
