#ifndef RESOURCE_STARTUP_BOUNDARY_TEST
#include "resources/resource_startup_native.h"
#include "resources/resource_cache.h"
#include "resources/resource_cache_store.h"
#include "base/core.h"
#include "base/log_admission.h"
#include "game/klib_bindings.h"
#include <cstring>
#include <cstdio>
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
static LONGLONG menuFrameAt=0,menuSlowAt=0;static bool menuFrameSeen=false,menuSlow=false;
static LONGLONG menuSeedAt=0;static bool menuSeedSeen=false;
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
static void SeedPhase(const char*phase){
#ifndef RESOURCE_STARTUP_BOUNDARY_TEST
    if(DetailedLogEnabled()){
        char line[160];sprintf_s(line,"Resource startup seed: %s groups=%u groupsVisited=%u locationsVisited=%u",phase,unsigned(seedGroups.size()),unsigned(seedGroup),seedVisits);LogMsg(line);
    }
#endif
}
static void SeedExisting(){
    // Only fills the gap before our registration hook. Share32 NEW entries
    // across at most8 group attempts per heartbeat. Empty/removed groups must
    // not each burn a separate250ms wait. Whole seed:8192 visits/256 groups.
    if(seeded&&(seedGroup>=seedGroups.size()||seedVisits>=8192))return;
    Ogre::ResourceGroupManager*m=Ogre::ResourceGroupManager::getSingletonPtr();if(!m)return;
    if(!seeded){seedGroups=m->getResourceGroups();if(seedGroups.size()>256)seedGroups.resize(256);seeded=true;SeedPhase("begin");}
    if(seedGroup>=seedGroups.size()||seedVisits>=8192)return;
    unsigned count=0,groups=0;const LONGLONG begin=QpcNow();
    while(seedGroup<seedGroups.size()&&seedVisits<8192&&count<32&&groups<8){
      // Soft deadline between groups; never abandon rewind-to-offset midway
      // through a large list, which could starve it once offset exceeds a cap.
      if(groups&&QpcDeltaUs(begin,QpcNow())>=1000)break;
      ++groups;
      try{
        const Ogre::ResourceGroupManager::LocationList&list=m->getResourceLocationList(seedGroups[seedGroup]);
        Ogre::ResourceGroupManager::LocationList::const_iterator it=list.begin();size_t skip=0;
        for(;it!=list.end()&&skip<seedOffset;++it,++skip){}
        while(it!=list.end()&&count<32&&seedVisits<8192){
            const Ogre::ResourceGroupManager::ResourceLocation*r=*it;
            if(r&&r->archive)Remember(r->archive->getName(),r->archive->getType(),r->recursive,true);
            ++it;++count;++seedVisits;++seedOffset;
            // Numeric progress is committed before yielding. This budget is
            // never applied to rewind-to-offset, which must make progress.
            if(QpcDeltaUs(begin,QpcNow())>=1000)break;
        }
        if(it==list.end()){++seedGroup;seedOffset=0;}
      }catch(...){++seedGroup;seedOffset=0;} // removed groups count towards the8attempt budget
    }
    if(seedGroup>=seedGroups.size()||seedVisits>=8192){SeedPhase("complete");Ogre::StringVector empty;seedGroups.swap(empty);}
}
static void Heartbeat(){
    if(!IsMainThread())return;
    if(InterlockedCompareExchange(&g_navMeshStopSeen,0,0)){resource_cache::SetStartupMenu(false);return;}
    DWORD now=GetTickCount();
    bool responsive=false;LONGLONG frameNow=0;
    if(resource_cache::StartupEnabled()&&ResourceStartupTitleVisible()){
        // GetTickCount can quantize a steady60Hz frame to32ms. Use the native
        // high-resolution clock for responsiveness; DWORD clock is lease only.
        frameNow=QpcNow();
        if(menuFrameSeen&&QpcDeltaUs(menuFrameAt,frameNow)>=20000){menuSlowAt=frameNow;menuSlow=true;}
        menuFrameAt=frameNow;menuFrameSeen=true;
        responsive=!menuSlow||QpcDeltaUs(menuSlowAt,frameNow)>=1000000;
    }else {menuFrameSeen=false;menuSlow=false;menuSeedSeen=false;}
    resource_cache::SetStartupMenu(responsive);
    // Only in a responsive title menu: prepare copied location metadata at
    // most once/16ms. Each slice retains32entries/8groups/soft1ms bounds.
    // Gameplay/slow-menu slices and ResourceCacheTick retain250ms cadence.
    if(responsive&&(!menuSeedSeen||QpcDeltaUs(menuSeedAt,frameNow)>=16000)){
        menuSeedAt=frameNow;menuSeedSeen=true;
        try{SeedExisting();}catch(...){}
    }
    if(ticked&&DWORD(now-lastTick)<250)return;ticked=true;lastTick=now;
    if(!responsive)try{SeedExisting();}catch(...){} // optional inventory failure never replaces native loading
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
