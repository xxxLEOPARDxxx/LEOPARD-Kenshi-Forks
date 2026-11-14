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
    Stats stats;bool on,ticked;U64 cap,world,clock;unsigned device,read,count;DWORD lastTick;
    Candidate pending[MaxPending];Textures textures;const Ogre::Resource*refreshAfter;
    State():on(false),ticked(false),cap(GiB),world(0),clock(0),device(0),read(0),count(0),lastTick(0),refreshAfter(NULL){ZeroMemory(&stats,sizeof(stats));}
};
// Intentionally process-lifetime. Clear releases Ogre references while the
// engine lives; a missed callback must never destruct them in DLL detach.
static State*state=NULL;
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
    if(!state||!IsMainThread())return;
    state->on=false;state->stats.enabled=false;state->stats.pressure=false;state->read=state->count=0;
    state->stats.evicted+=state->textures.size();state->textures.clear();state->stats.heldBytes=0;state->refreshAfter=NULL;
    resource_gpu::TryTextureRetentionReservation(0,state->cap);Publish();
}
static void Observe(Ogre::Resource*r,bool wasLoaded=true){
    if(!IsMainThread()||!state||!state->on||!r)return;
    Ogre::TextureManager*m=Ogre::TextureManager::getSingletonPtr();
    // Resource::load is shared by all resource classes: reject before any
    // texture cast, metadata access, or native controlling pointer creation.
    if(!m||r->getCreator()!=m)return;
    ++state->stats.observed;
    Textures::iterator i=state->textures.find(r);
    if(i!=state->textures.end()){
        if(wasLoaded){i->second.touch=++state->clock;if(r->isLoaded())++state->stats.reused;return;}
        // Native explicitly reloaded a held resource. Drop our old reference
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
    const unsigned device=resource_gpu::DeviceGeneration();
    if(!on||!state->stats.admitted||state->world!=world||state->device!=device){
        if(state->on||state->count||!state->textures.empty())Clear();state->world=world;state->device=device;
    }
    state->cap=(std::max)(GiB,(std::min)(cap,8*GiB));state->on=on&&state->stats.admitted;state->stats.enabled=state->on;
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
typedef void(*LoadFn)(Ogre::Resource*,bool);
static LoadFn originalLoad=NULL;
static void Load(Ogre::Resource*r,bool background){
    const bool owner=IsMainThread(); // workers never read mutable owner state
    const bool wasLoaded=owner&&state&&state->on&&r&&r->isLoaded();
    originalLoad(r,background); // exactly once; native exceptions propagate unchanged
    if(owner)try{Observe(r,wasLoaded);}catch(...){} // optional observation cannot replace native success
}
void Init(){
    if(!IsMainThread()||state)return;state=new State;
#ifdef RESOURCE_TEXTURE_RETENTION_TEST
    state->stats.admitted=true;
#else
    if(NativeProfileIsGog()){KEO_DETAIL("Texture retention: native Steam contract not admitted on GOG");return;}
    HMODULE ogre=GetModuleHandleA("OgreMain_x64.dll");DWORD stamp=0,size=0;
    void*target=ogre?reinterpret_cast<void*>(GetProcAddress(ogre,"?load@Resource@Ogre@@UEAAX_N@Z")):NULL;
    static const unsigned char bytes[32]={0x48,0x8b,0xc4,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x81,0xec,0x00,0x03,0x00,0x00,0x48,0xc7,0x44,0x24,0x78,0xfe,0xff,0xff,0xff,0x48,0x89,0x58,0x10};
    if(!target||!ReadModuleImageId(ogre,&stamp,&size)||stamp!=0x5ca5f929||size!=0x9c9000||
       reinterpret_cast<uintptr_t>(target)-reinterpret_cast<uintptr_t>(ogre)!=0x270650||memcmp(target,bytes,sizeof(bytes))){
        LogError("Texture retention: Ogre Resource::load contract refused; ordinary native loading retained");return;
    }
    if(KenshiLib::AddHook(target,reinterpret_cast<void*>(&Load),reinterpret_cast<void**>(&originalLoad))!=KenshiLib::SUCCESS){
        originalLoad=NULL;LogError("Texture retention: Ogre Resource::load hook failed");return;
    }
    state->stats.admitted=true;KEO_DETAIL("Texture retention: verified native loaded DDS observation admitted (revisits only)");
#endif
}
}
