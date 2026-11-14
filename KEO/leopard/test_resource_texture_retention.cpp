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
static unsigned deviceGeneration=1;
bool IsMainThread(){return mainOwner;}
namespace resource_gpu {
bool TryTextureRetentionReservation(U64 n,U64 cap){if(n>reserved&&(!budgetAllowed||n>cap))return false;reserved=n;return true;}
bool TextureRetentionStandbyAllowed(U64 cap){return budgetAllowed&&reserved<=cap;}
U64 TextureRetentionReservation(){return reserved;}
unsigned DeviceGeneration(){return deviceGeneration;}
}
#include "../src/resources/resource_texture_retention.cpp"
struct Texture:Ogre::Texture {
    bool fail,failObserve;unsigned loads,unloads;
    Texture(Ogre::ResourceManager*m,const Ogre::String&n,Ogre::ResourceHandle h,const Ogre::String&g,bool manual,Ogre::ManualResourceLoader*l):
        Ogre::Texture(m,n,h,g,manual,l),fail(false),failObserve(false),loads(0),unloads(0){setWidth(16);setHeight(8);setDepth(1);setFormat(Ogre::PF_A8R8G8B8);setNumMipmaps(0);setTextureType(Ogre::TEX_TYPE_2D);setUsage(Ogre::TU_STATIC_WRITE_ONLY);}
    Ogre::ResourceManager*getCreator(){if(failObserve)throw 92;return Ogre::Resource::getCreator();}
    void SetSize(size_t n){mSize=n;}
    void prepareImpl(){}void unprepareImpl(){}
    void loadImpl(){++loads;if(fail)throw 91;}
    void unloadImpl(){++unloads;}
    void createInternalResourcesImpl(){}void freeInternalResourcesImpl(){}
    Ogre::HardwarePixelBufferSharedPtr getBuffer(size_t,size_t){assert(false);return Ogre::HardwarePixelBufferSharedPtr();}
    size_t NativeCalculateSize(){return Ogre::Texture::calculateSize();}
};
struct Manager:Ogre::TextureManager {
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
static unsigned forwarded=0;static bool seenBackground=false;
static void LoadSpy(Ogre::Resource*r,bool background){++forwarded;seenBackground=background;LoadNative(r,background);}
static bool WorkerStateProbe(Ogre::Resource*r){
    // A worker must not even read owner-only on/containers. This protected
    // State makes accidental owner-state reads observable without a race.
    __try{resource_texture_retention::Observe(r);resource_texture_retention::Load(r,false);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static LONG WINAPI Crash(EXCEPTION_POINTERS*p){std::cerr<<"crash RIP="<<std::hex<<p->ContextRecord->Rip<<" image="<<(void*)GetModuleHandle(NULL)<<" Ogre="<<(void*)GetModuleHandleA("OgreMain_x64.dll")<<"\n";return EXCEPTION_EXECUTE_HANDLER;}
int main(){
    SetUnhandledExceptionFilter(Crash);
    Ogre::Root root("","","");Manager manager;
    Ogre::TexturePtr a=manager.create("native-A.dds","General");a->load();
    Texture*t=static_cast<Texture*>(a.get());
    CHECK(a->isLoaded());CHECK(a->getSize()==t->NativeCalculateSize());
    CHECK(a->getSize()>=512);CHECK(t->loads==1&&t->unloads==0);
    resource_texture_retention::Init();TickNow();
    unsigned before=a.useCount();resource_texture_retention::Observe(a.get());
    CHECK(a.useCount()==before);
    CHECK(resource_texture_retention::Snapshot().pending==1);
    resource_texture_retention::Observe(a.get());CHECK(resource_texture_retention::Snapshot().pending==1);
    TickNow();
    CHECK(resource_texture_retention::Snapshot().textures==1);
    CHECK(a.useCount()==before+1);CHECK(reserved==a->getSize());
    resource_texture_retention::Observe(a.get());CHECK(resource_texture_retention::Snapshot().reused==1);
    // Actual manager purge compares native useCount against baseline3. Our
    // retained TexturePtr is exactly one controlling reference, not a mock.
    typedef void(*Purge)(Ogre::ResourceManager*,bool);
    Purge purge=reinterpret_cast<Purge>((*reinterpret_cast<void***>(&manager))[0x68/8]);
    Ogre::Texture*raw=a.get();a.setNull();purge(&manager,false);CHECK(raw->isLoaded());
    budgetAllowed=false;TickNow();CHECK(reserved==0&&resource_texture_retention::Snapshot().textures==0);
    purge(&manager,false);CHECK(!raw->isLoaded());budgetAllowed=true;
    a=manager.getByName("native-A.dds");a->load();t=static_cast<Texture*>(a.get());
    // The hook forwards exactly once, background argument unchanged; a native
    // exception propagates unchanged and enqueues no successful observation.
    resource_texture_retention::originalLoad=&LoadSpy;forwarded=0;
    resource_texture_retention::Load(a.get(),true);CHECK(forwarded==1&&seenBackground);CHECK(resource_texture_retention::Snapshot().pending==1);
    a->unload();t->fail=true;bool threw=false;
    try{resource_texture_retention::Load(a.get(),false);}catch(int v){threw=v==91;}
    CHECK(threw&&forwarded==2&&!seenBackground);CHECK(resource_texture_retention::Snapshot().pending==1);t->fail=false;a->load();TickNow();
    resource_texture_retention::Clear();TickNow();
    t->failObserve=true;resource_texture_retention::Load(t,false);t->failObserve=false;
    CHECK(forwarded==3&&resource_texture_retention::Snapshot().pending==0);
    mainOwner=false;resource_texture_retention::Load(t,false);mainOwner=true;
    CHECK(forwarded==4&&resource_texture_retention::Snapshot().pending==0);
    resource_texture_retention::State*savedState=resource_texture_retention::state;
    void*inaccessible=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);CHECK(inaccessible!=NULL);
    resource_texture_retention::state=static_cast<resource_texture_retention::State*>(inaccessible);mainOwner=false;
    bool workerSafe=WorkerStateProbe(t);mainOwner=true;resource_texture_retention::state=savedState;
    VirtualFree(inaccessible,0,MEM_RELEASE);CHECK(workerSafe);
    // Native explicit reload is not a hit on an already loaded texture. The
    // old persistent pin/accounting must disappear before the wrapper returns,
    // then bounded Tick separately admits the new actual loaded byte size.
    resource_texture_retention::Observe(t);TickNow();CHECK(reserved>0);
    resource_texture_retention::U64 priorReused=resource_texture_retention::Snapshot().reused;
    t->unload();t->setWidth(2048);t->setHeight(2048);unsigned reloadedRefs=a.useCount();
    resource_texture_retention::Load(t,false);
    CHECK(t->isLoaded()&&reserved==0&&resource_texture_retention::Snapshot().textures==0);
    CHECK(resource_texture_retention::Snapshot().pending==1&&a.useCount()==reloadedRefs-1);
    CHECK(resource_texture_retention::Snapshot().reused==priorReused);
    TickNow();resource_texture_retention::U64 resized=0;
    CHECK(resource_texture_retention::Describe(t,&resized)&&reserved==resized&&resized>4ULL*1024*1024);
    t->unload();t->setWidth(16384);t->setHeight(16384);resource_texture_retention::Load(t,false);
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
