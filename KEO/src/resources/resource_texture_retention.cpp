#include "resources/resource_texture_retention.h"
#include "resources/resource_gpu_store.h"
#ifndef RESOURCE_TEXTURE_RETENTION_TEST
#include "base/core.h"
#include "base/log_admission.h"
#include "game/native_profile.h"
#include "render/module_hooks.h"
#include "base/klib_include.h"
#endif
#include <ogre/OgreTextureManager.h>
#include <map>
#include <algorithm>
#ifndef RESOURCE_TEXTURE_RETENTION_TEST
#include "base/klib_include_end.h"
#endif
namespace resource_texture_retention {
static const unsigned MaxPending=128,MaxTextures=1024;
static const U64 GiB=1024ULL*1024*1024;
// Weak observations contain no controlling native references. The raw value
// serves only as an identity comparison after manager handle resolution.
struct Candidate {Ogre::ResourceHandle handle;const Ogre::Resource*id;DWORD time;};
struct Entry {Ogre::TexturePtr texture;Ogre::ResourceHandle handle;U64 bytes,touch;};
typedef std::map<const Ogre::Resource*,Entry> Textures;
struct State {
    Stats stats;bool on,ticked,contractRefused,admissionTried;U64 cap,world,clock;unsigned device,read,count;DWORD lastTick,lastAdmissionTick;
    Candidate pending[MaxPending];Textures textures;const Ogre::Resource*refreshAfter;
    State():on(false),ticked(false),contractRefused(false),admissionTried(false),cap(GiB),world(0),clock(0),device(0),read(0),count(0),lastTick(0),lastAdmissionTick(0),refreshAfter(NULL){ZeroMemory(&stats,sizeof(stats));}
};
// Intentionally process-lifetime. Clear releases Ogre references while the
// engine lives; a missed callback must never destruct them in DLL detach.
static State*state=NULL;
static volatile LONG captureEnabled=0;
static bool CaptureEnabled(){return InterlockedCompareExchange(&captureEnabled,0,0)!=0;}
static void TryInstallHooks();
static void Publish(){state->stats.textures=unsigned(state->textures.size());state->stats.pending=state->count;}
static void Release(Textures::iterator i){
    state->stats.heldBytes-=i->second.bytes;state->textures.erase(i);++state->stats.evicted;
    resource_gpu::TryTextureRetentionReservation(state->stats.heldBytes,state->cap);
}
static void Oldest(){
    Textures::iterator oldest=state->textures.begin();
    for(Textures::iterator i=state->textures.begin();i!=state->textures.end();++i)if(i->second.touch<oldest->second.touch)oldest=i;
    if(oldest!=state->textures.end())Release(oldest);
}
static bool DdsName(const Ogre::String&name){
    const size_t n=name.size();if(n<5)return false;
    return name[n-4]=='.'&&(name[n-3]=='d'||name[n-3]=='D')&&(name[n-2]=='d'||name[n-2]=='D')&&(name[n-1]=='s'||name[n-1]=='S');
}
static bool Describe(const Ogre::Texture*t,U64*bytes){
    if(!t||!t->isLoaded()||t->isManuallyLoaded()||!DdsName(t->getName())||t->getTextureType()!=Ogre::TEX_TYPE_2D||
       (t->getUsage()&(Ogre::TU_DYNAMIC|Ogre::TU_RENDERTARGET))||t->getFSAA()!=0)return false;
    const unsigned w=t->getWidth(),h=t->getHeight(),depth=t->getDepth(),mips=t->getNumMipmaps();
    if(!w||!h||w>32768||h>32768||depth!=1||mips>15||t->getFormat()==Ogre::PF_UNKNOWN)return false;
    // Nonmanual native DDS loading is the file-backed path. Do not search
    // archive roots here: resourceExists can fan out across hundreds of roots.
    // Native Texture::calculateSize includes object/CPU metadata but counts
    // only a base pixel surface. Include actual loaded mip dimensions too.
    // No getBuffer(), load(), format conversion or unsupported virtual slot.
    U64 pixels=Ogre::Image::calculateSize(mips,1,w,h,depth,t->getFormat());
    U64 accounted=t->getSize();*bytes=(std::max)(pixels,accounted);
    return pixels&&accounted&&*bytes<=8*GiB;
}
static bool Promote(const Ogre::ResourcePtr&r,U64 bytes,U64 touch){
    if(state->textures.size()>=MaxTextures||!bytes||bytes>8*GiB||state->stats.heldBytes>8*GiB-bytes||
       !resource_gpu::TryTextureRetentionReservation(state->stats.heldBytes+bytes,state->cap))return false;
    // Only the admitted entry owns a TexturePtr. Handle resolution above may
    // briefly own a ResourcePtr, but no queued candidate pins native state.
    Entry e;e.texture=r.staticCast<Ogre::Texture>();e.handle=r->getHandle();e.bytes=bytes;e.touch=touch;
    state->textures.insert(std::make_pair(r.get(),e));state->stats.heldBytes+=bytes;return true;
}
void Clear(){
    if(!IsMainThread()||!state)return;
    InterlockedExchange(&captureEnabled,0);
    state->on=false;state->stats.enabled=false;state->stats.pressure=false;state->read=state->count=0;
    state->stats.evicted+=state->textures.size();state->textures.clear();state->stats.heldBytes=0;state->refreshAfter=NULL;
    resource_gpu::TryTextureRetentionReservation(0,state->cap);Publish();
}
static void Observe(Ogre::Resource*r,bool wasLoaded=true){
    if(!IsMainThread()||!state||!state->on||!r)return;
    Ogre::TextureManager*m=Ogre::TextureManager::getSingletonPtr();
    // Defend manager ownership before any texture cast or controlling pointer.
    if(!m||r->getCreator()!=m)return;
    ++state->stats.observed;
    Textures::iterator i=state->textures.find(r);
    if(i!=state->textures.end()){
        if(wasLoaded){i->second.touch=++state->clock;if(r->isLoaded())++state->stats.reused;return;}
        // Native loadImpl actually rebuilt a held resource. Drop our old reference
        // and stale accounting immediately, then re-admit the new size only
        // through bounded Tick. The native caller still owns its resource.
        Release(i);
    }
    const Ogre::ResourceHandle handle=r->getHandle();
    for(unsigned n=0;n<state->count;++n){const Candidate&c=state->pending[(state->read+n)%MaxPending];if(c.handle==handle&&c.id==r)return;}
    if(state->count>=MaxPending){++state->stats.rejected;++state->stats.queueFull;return;}
    Candidate&c=state->pending[(state->read+state->count)%MaxPending];c.handle=handle;c.id=r;c.time=GetTickCount();++state->count;
}
void Tick(bool on,U64 cap,U64 world){
    if(!state||!IsMainThread())return;
    if(!state->stats.admitted&&!state->contractRefused)TryInstallHooks();
    const unsigned device=resource_gpu::DeviceGeneration();
    if(!on||!state->stats.admitted||state->world!=world||state->device!=device){
        if(state->on||state->count||!state->textures.empty())Clear();state->world=world;state->device=device;
    }
    state->cap=(std::max)(GiB,(std::min)(cap,8*GiB));
    const bool enabled=on&&state->stats.admitted;
    if(state->on!=enabled){state->on=enabled;InterlockedExchange(&captureEnabled,enabled?1:0);}
    state->stats.enabled=state->on;
    if(!state->on){Publish();return;}
    DWORD now=GetTickCount();if(state->ticked&&DWORD(now-state->lastTick)<250){Publish();return;}
    state->ticked=true;state->lastTick=now;
    try{
        state->stats.pressure=!resource_gpu::TextureRetentionStandbyAllowed(state->cap);
        if(state->stats.pressure){
            for(unsigned n=0;n<32&&!state->textures.empty();++n)Oldest();
            state->read=state->count=0;Publish();return;
        }
        // Native explicit unload/reload, manager removal and new byte sizes
        // stay authoritative. Two entries per heartbeat bound metadata work.
        for(unsigned n=0;n<2&&!state->textures.empty();++n){
            Textures::iterator i=state->textures.upper_bound(state->refreshAfter);if(i==state->textures.end())i=state->textures.begin();
            state->refreshAfter=i->first;
            Ogre::TextureManager*m=Ogre::TextureManager::getSingletonPtr();
            Ogre::ResourcePtr r=m?m->getByHandle(i->second.handle):Ogre::ResourcePtr();
            if(r.isNull()||r.get()!=i->first||r->getCreator()!=m){++state->stats.identityMismatch;Release(i);continue;}
            U64 bytes=0;if(!Describe(static_cast<Ogre::Texture*>(r.get()),&bytes)){Release(i);continue;}
            if(bytes!=i->second.bytes){U64 touch=i->second.touch;Release(i);if(!Promote(r,bytes,touch))++state->stats.rejected;}
        }
        unsigned work=(std::min)(state->count,4u);
        for(unsigned n=0;n<work;++n){
            Candidate c=state->pending[state->read];state->read=(state->read+1)%MaxPending;--state->count;
            Ogre::TextureManager*m=Ogre::TextureManager::getSingletonPtr();
            Ogre::ResourcePtr r=m?m->getByHandle(c.handle):Ogre::ResourcePtr();
            if(r.isNull()||r.get()!=c.id||r->getCreator()!=m){++state->stats.identityMismatch;++state->stats.rejected;continue;}
            // Only after creator and actual manager identity have both passed.
            Ogre::Texture*t=static_cast<Ogre::Texture*>(r.get());
            if(!t->isLoaded()){
                if(DWORD(now-c.time)<30000){state->pending[(state->read+state->count)%MaxPending]=c;++state->count;}
                else{++state->stats.rejected;++state->stats.loadTimeout;}continue;
            }
            if(state->textures.find(r.get())!=state->textures.end())continue;
            U64 bytes=0;if(!Describe(t,&bytes)){++state->stats.rejected;++state->stats.unsupported;continue;}
            if(state->textures.size()>=MaxTextures)Oldest();
            bool ok=Promote(r,bytes,++state->clock);
            for(unsigned e=0;!ok&&e<2&&!state->textures.empty();++e){Oldest();ok=Promote(r,bytes,++state->clock);}
            if(ok)++state->stats.retained;else ++state->stats.rejected;
        }
    }catch(...){
        Clear();
#ifndef RESOURCE_TEXTURE_RETENTION_TEST
        LogError("Texture retention: metadata/allocation exception; own references cleared");
#endif
    }
    Publish();
}
Stats Snapshot(){Stats s={};if(state&&IsMainThread()){Publish();s=state->stats;}return s;}
// Raw native x64 ABI: hidden TexturePtr storage is argument2, and RAX returns
// that same address. No C++ value return or wrapper-owned SharedPtr copy.
typedef Ogre::TexturePtr*(*ManagerLoadFn)(Ogre::TextureManager*,Ogre::TexturePtr*,const Ogre::String&,const Ogre::String&,Ogre::TextureType,int,float,bool,Ogre::PixelFormat,bool);
typedef void(*LoadImplFn)(Ogre::Texture*);
static ManagerLoadFn originalManagerLoad=NULL;
static LoadImplFn originalLoadImpl=NULL;
static Ogre::TexturePtr*ManagerLoad(Ogre::TextureManager*self,Ogre::TexturePtr*out,const Ogre::String&name,const Ogre::String&group,Ogre::TextureType type,int mips,float gamma,bool alpha,Ogre::PixelFormat format,bool hwGamma){
    Ogre::TexturePtr*result=originalManagerLoad(self,out,name,group,type,mips,gamma,alpha,format,hwGamma);
    if(CaptureEnabled()&&IsMainThread())try{
        ++state->stats.managerRequests;
        if(result==out&&out&&!out->isNull())Observe(out->get());
    }catch(...){} // optional observation cannot replace native success
    return result;
}
static void LoadImpl(Ogre::Texture*r){
    originalLoadImpl(r); // exactly once; native exceptions propagate unchanged
    if(CaptureEnabled()&&IsMainThread())try{
        ++state->stats.loadCompletions;Observe(r,false);
    }catch(...){}
    // Resource::load has not published LOADED/mSize yet. Pending stays weak;
    // Tick must verify loaded and actual bytes before any persistent pin.
}
static void TryInstallHooks(){
#ifdef RESOURCE_TEXTURE_RETENTION_TEST
    state->stats.admitted=true;
#else
    const DWORD now=GetTickCount();
    if(state->admissionTried&&DWORD(now-state->lastAdmissionTick)<250)return;
    state->admissionTried=true;state->lastAdmissionTick=now;
    if(NativeProfileIsGog()){state->contractRefused=true;KEO_DETAIL("Texture retention: native Steam contract not admitted on GOG");return;}
    HMODULE ogre=GetModuleHandleA("OgreMain_x64.dll"),renderer=GetModuleHandleA("RenderSystem_Direct3D11_x64.dll");
    if(!ogre||!renderer)return; // normal startup wait; bounded Tick retries
    DWORD stamp=0,size=0,rendererStamp=0,rendererSize=0;
    void*managerTarget=reinterpret_cast<void*>(GetProcAddress(ogre,"?load@TextureManager@Ogre@@UEAA?AV?$SharedPtr@VTexture@Ogre@@@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0W4TextureType@2@HM_NW4PixelFormat@2@2@Z"));
    const uintptr_t rendererBase=reinterpret_cast<uintptr_t>(renderer),ogreBase=reinterpret_cast<uintptr_t>(ogre);
    void*implTarget=reinterpret_cast<void*>(rendererBase+0x47d20);
    static const unsigned char managerBytes[32]={0x4c,0x8b,0xdc,0x49,0x89,0x53,0x10,0x57,0x48,0x81,0xec,0xa0,0x00,0x00,0x00,0x48,0xc7,0x44,0x24,0x78,0xfe,0xff,0xff,0xff,0x49,0x89,0x5b,0x18,0x48,0x8b,0xda,0x33};
    static const unsigned char implBytes[32]={0x48,0x8b,0xc4,0x57,0x41,0x54,0x41,0x55,0x48,0x83,0xec,0x50,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff,0x48,0x89,0x58,0x10,0x48,0x89,0x68,0x18,0x48,0x89,0x70};
    if(!managerTarget||!ReadModuleImageId(ogre,&stamp,&size)||stamp!=0x5ca5f929||size!=0x9c9000||
       reinterpret_cast<uintptr_t>(managerTarget)-ogreBase!=0x379890||memcmp(managerTarget,managerBytes,sizeof(managerBytes))||
       !ReadModuleImageId(renderer,&rendererStamp,&rendererSize)||rendererStamp!=0x5ca5f9e9||rendererSize!=0xbf000||
       memcmp(implTarget,implBytes,sizeof(implBytes))||
       *reinterpret_cast<const uintptr_t*>(ogreBase+0x6a69a8+0x128)!=reinterpret_cast<uintptr_t>(managerTarget)||
       *reinterpret_cast<const uintptr_t*>(rendererBase+0x7ce08+0x58)!=reinterpret_cast<uintptr_t>(implTarget)||
       *reinterpret_cast<const uintptr_t*>(rendererBase+0x7ce00)!=rendererBase+0x88248||
       *reinterpret_cast<const DWORD*>(rendererBase+0x88248)!=1||
       *reinterpret_cast<const DWORD*>(rendererBase+0x88248+12)!=0xb22c8||
       *reinterpret_cast<const DWORD*>(rendererBase+0x88248+20)!=0x88248||
       strcmp(reinterpret_cast<const char*>(rendererBase+0xb22c8+16),".?AVD3D11Texture@Ogre@@")){
        state->contractRefused=true;LogError("Texture retention: narrow native texture contracts refused; ordinary native loading retained");return;
    }
    // Both full contracts passed before either hook. A partial hook failure
    // leaves the atomic capture gate OFF and wrappers forwarding only.
    if(KenshiLib::AddHook(managerTarget,reinterpret_cast<void*>(&ManagerLoad),reinterpret_cast<void**>(&originalManagerLoad))!=KenshiLib::SUCCESS){
        state->contractRefused=true;LogError("Texture retention: native TextureManager::load hook failed");return;
    }
    if(KenshiLib::AddHook(implTarget,reinterpret_cast<void*>(&LoadImpl),reinterpret_cast<void**>(&originalLoadImpl))!=KenshiLib::SUCCESS){
        state->contractRefused=true;LogError("Texture retention: native D3D11Texture::loadImpl hook failed");return;
    }
    state->stats.admitted=true;KEO_DETAIL("Texture retention: narrow manager requests and real texture load completions admitted");
#endif
}
void Init(){if(!IsMainThread()||state)return;state=new State;TryInstallHooks();}
}
