#ifndef RESOURCE_STARTUP_BOUNDARY_TEST
#include "resources/resource_startup_native.h"
#include "resources/resource_cache.h"
#include "resources/resource_cache_store.h"
#include "base/core.h"
#include "base/log_admission.h"
#include "game/klib_bindings.h"
#include <cstring>
#include "render/module_hooks.h"
#include "base/klib_include.h"
#include <ogre/OgreRoot.h>
#include <kenshi/gui/TitleScreen.h>
#include <ogre/OgreResourceGroupManager.h>
#include <ogre/OgreArchive.h>
#include "base/klib_include_end.h"
#endif
// No engine pointer crosses the IO boundary. Registration runs after the native
// call succeeds and only copies path/recursion, never enumerating disk here.
namespace resource_startup_native {
typedef void(*AddFn)(Ogre::ResourceGroupManager*,const Ogre::String&,const Ogre::String&,const Ogre::String&,bool,bool);
typedef bool(*FrameFn)(Ogre::Root*);
typedef bool(*TimedFn)(Ogre::Root*,float);
AddFn originalAdd=NULL;FrameFn originalFrame=NULL;TimedFn originalTimed=NULL;
static DWORD lastTick=0;static bool ticked=false;
static void Remember(const Ogre::String&name,const Ogre::String&type,bool recursive,bool readOnly){
    if(type!="FileSystem"||!readOnly||name.empty()||name.size()>=MAX_PATH)return;
    char full[MAX_PATH];DWORD len=GetFullPathNameA(name.c_str(),MAX_PATH,full,NULL);
    if(!len||len>=MAX_PATH||len<3||full[1]!=':')return;
    for(DWORD i=0;i<len;++i){if(full[i]=='/')full[i]='\\';if(full[i]>='A'&&full[i]<='Z')full[i]+=char('a'-'A');}
    resource_cache::RegisterTextureRoot(full,recursive);
}
void Add(Ogre::ResourceGroupManager*self,const Ogre::String&name,const Ogre::String&type,const Ogre::String&group,bool recursive,bool readOnly){
    originalAdd(self,name,type,group,recursive,readOnly); // native failures propagate untouched
    try{Remember(name,type,recursive,readOnly);}catch(...){}
}
static Ogre::StringVector seedGroups;static size_t seedGroup=0,seedOffset=0;static unsigned seedVisits=0;static bool seeded=false;
static void SeedExisting(){
    // Only fills the gap before our registration hook. Limit each heartbeat
    // to 32 entries and the WHOLE seed to 8192 visits/256 groups.
    Ogre::ResourceGroupManager*m=Ogre::ResourceGroupManager::getSingletonPtr();if(!m)return;
    if(!seeded){seedGroups=m->getResourceGroups();if(seedGroups.size()>256)seedGroups.resize(256);seeded=true;}
    if(seedGroup>=seedGroups.size()||seedVisits>=8192)return;
    try{
        const Ogre::ResourceGroupManager::LocationList&list=m->getResourceLocationList(seedGroups[seedGroup]);
        Ogre::ResourceGroupManager::LocationList::const_iterator it=list.begin();size_t skip=0;
        for(;it!=list.end()&&skip<seedOffset;++it,++skip){}
        unsigned count=0;
        for(;it!=list.end()&&count<32&&seedVisits<8192;++it,++count,++seedVisits,++seedOffset){
            const Ogre::ResourceGroupManager::ResourceLocation*r=*it;
            if(r&&r->archive)Remember(r->archive->getName(),r->archive->getType(),r->recursive,true);
        }
        if(it==list.end()){++seedGroup;seedOffset=0;}
    }catch(...){++seedGroup;seedOffset=0;} // a removed group never prevents later seeds
    if(seedGroup>=seedGroups.size()||seedVisits>=8192){Ogre::StringVector empty;seedGroups.swap(empty);}
}
static void Heartbeat(){
    if(!IsMainThread()||InterlockedCompareExchange(&g_navMeshStopSeen,0,0))return;
    DWORD now=GetTickCount();if(ticked&&DWORD(now-lastTick)<250)return;ticked=true;lastTick=now;
    try{SeedExisting();}catch(...){} // optional inventory failure never replaces native loading
    ResourceCacheTick();
}
bool Frame(Ogre::Root*self){bool ok=originalFrame(self);if(ok)Heartbeat();return ok;}
bool Timed(Ogre::Root*self,float elapsed){bool ok=originalTimed(self,elapsed);if(ok)Heartbeat();return ok;}
}
#ifndef RESOURCE_STARTUP_BOUNDARY_TEST
bool ResourceStartupTitleVisible(){
    if(!IsMainThread()||g_navMeshStopSeen)return false;
    typedef TitleScreen*(*Getter)();static Getter get=NULL;static bool tried=false;
    if(!tried){
        tried=true;uintptr_t fn=uintptr_t(KlibRealAddress(&TitleScreen::getSingleton));
        // Actual official SDK slot6627: Steam911F00/GOG911620. This getter is
        // exactly MOV RAX,[RIP+rel32]; RET, with no singleton construction.
        // Refuse unknown code instead of calling an unverified helper.
        uintptr_t base=uintptr_t(GetModuleHandleA(NULL));const IMAGE_NT_HEADERS64*nt=
          reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
        if(fn>=base&&fn-base<nt->OptionalHeader.SizeOfImage-8){const unsigned char*b=reinterpret_cast<const unsigned char*>(fn);
            if(b[0]==0x48&&b[1]==0x8b&&b[2]==0x05&&b[7]==0xc3)get=reinterpret_cast<Getter>(fn);
        }
    }
    if(!get)return false;
    TitleScreen*title=get();return title&&title->mMainWidget&&title->mMainWidget->getVisible();
}
void ResourceStartupNativeInit(){
    using namespace resource_startup_native;
    static bool tried=false;if(tried)return;tried=true;
    const ModuleSite add={"RAM startup resource locations","OgreMain_x64.dll",
    "?addResourceLocation@ResourceGroupManager@Ogre@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@00_N1@Z",0,
    {0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xac}};
    const ModuleSite frame={"RAM startup menu heartbeat","OgreMain_x64.dll","?renderOneFrame@Root@Ogre@@QEAA_NXZ",0,
    {0x40,0x56,0x48,0x83,0xec,0x40,0x48,0x8b,0xf1,0xe8,0x22,0xfb,0xff,0xff,0x84,0xc0}};
    const ModuleSite timed={"RAM startup timed heartbeat","OgreMain_x64.dll","?renderOneFrame@Root@Ogre@@QEAA_NM@Z",0,
    {0x40,0x56,0x48,0x83,0xec,0x40,0x48,0x8b,0xf1,0x48,0x8b,0x89,0x60,0x01,0x00,0x00}};
    bool a=InstallModuleHook(add,(void*)&Add,(void**)&originalAdd,NULL);
    bool f=InstallModuleHook(frame,(void*)&Frame,(void**)&originalFrame,NULL);
    bool t=InstallModuleHook(timed,(void*)&Timed,(void**)&originalTimed,NULL);
    KEO_DETAIL(std::string("Resource startup: locations=")+(a?"capture":"seed only")+" menuTick="+(f||t?"installed":"scene only"));
}
#endif
