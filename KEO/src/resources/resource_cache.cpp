#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "resources/resource_cache.h"
#include "resources/resource_geometry.h"
#include "resources/resource_cache_store.h"
#include "resources/resource_forward.h"
#include "resources/resource_load_probe.h"
#include "resources/resource_warm_hud.h"
#include "resources/resource_startup_native.h"
#include "render/module_hooks.h"
#include "zone/zone_config.h"
#include "base/core.h"
#include "base/log_admission.h"
#include "base/klib_include.h"
#include <ogre/OgreFileSystem.h>
#include <ogre/OgreDataStream.h>
#include "resources/resource_cache_stream.h"
#include "base/klib_include_end.h"
#include <sstream>

namespace {
// Member-return ABI is NOT the C++ free-function return ABI: native RCX=this,
// RDX=return storage, R8=filename, R9b=readOnly (confirmed at OgreMain+D1BF1).
typedef Ogre::DataStreamPtr* (*ArchiveOpen)(Ogre::FileSystemArchive*,Ogre::DataStreamPtr*,const Ogre::String&,bool);
ArchiveOpen original=NULL;
bool installed=false;
bool uiTicksAdmitted=false; // Init runs before the title's font/resource refresh.
volatile LONG active=0;
const ModuleSite site={
    "FileSystemArchive::open RAM cache","OgreMain_x64.dll",
    "?open@FileSystemArchive@Ogre@@UEAA?AV?$SharedPtr@VDataStream@Ogre@@@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N@Z",
    0,{0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xac,0x24}
};
std::string Path(Ogre::FileSystemArchive*a,const Ogre::String&name){
    std::string p=a->getName()+"/"+name;
    if(p.size()>=MAX_PATH)return std::string();
    char full[MAX_PATH];DWORD n=GetFullPathNameA(p.c_str(),MAX_PATH,full,NULL);
    if(!n||n>=MAX_PATH||n<3||full[1]!=':')return std::string();
    for(DWORD i=0;i<n;++i)if(full[i]>='A'&&full[i]<='Z')full[i]+=char('a'-'A');
    return full;
}
Ogre::DataStreamPtr* Open(Ogre::FileSystemArchive*a,Ogre::DataStreamPtr*out,const Ogre::String&name,bool readOnly){
    if(!readOnly||!InterlockedCompareExchange(&active,0,0))return original(a,out,name,readOnly);
    std::string path;
    try{
        path=Path(a,name);
        resource_cache::FilePtr f=resource_cache::Find(path);
        if(f){
            std::unique_ptr<resource_cache::CachedStream> stream(new resource_cache::CachedStream(name,f));
            new(out) Ogre::DataStreamPtr(stream.get());stream.release();return out;
        }
    }catch(...){path.clear();}
    Ogre::DataStreamPtr* result=original(a,out,name,readOnly); // native exceptions remain native
    if(!result->isNull()&&!path.empty()){
        try{resource_cache::Observe(path);}catch(...){}
    }
    return result;
}
}
void ResourceCacheInit(){
    if(installed)return;
    if(!resource_cache::Start()){LogError("Resource RAM cache: IO worker could not start");return;}
    installed=InstallModuleHook(site,(void*)&Open,(void**)&original,NULL);
    if(!installed){resource_cache::Stop();return;}
    KEO_DETAIL("Resource RAM cache: verified native archive boundary installed (DDS/mesh/skeleton)");
    ResourceStartupNativeInit();
    ResourceCacheTick(); // honor opt-in INI before the first scene loads
    ResourceForwardInit();
    ResourceLoadProbeInit();
    uiTicksAdmitted=true;
}
void ResourceCacheTick(){
    if(!installed)return;
    static double next=0,lastReport=0;double now=ElapsedSec();if(now<next)return;next=now+1;
    ResourceLoadProbeReport();
    const zone::ZoneConfig&c=zone::g_zoneCfg;
    resource_cache::Configure(c.cfg_resourceRamCache,unsigned __int64(c.cfg_resourceRamGiB)*1024*1024*1024,c.cfg_resourceReadMBps);
    if(!c.cfg_resourcePrefetch||ResourceStartupTitleVisible())resource_cache::SetBackgroundAllowed(true); // title has no scene ticks to release a prior pause
    resource_cache::ConfigureStartup(c.cfg_resourceRamCache&&c.cfg_resourceStartupTextures);
    resource_geometry::Tick(c.cfg_resourceRamCache&&c.cfg_resourceGeometryWarm,unsigned __int64(c.cfg_resourceGpuGiB)*1024*1024*1024,ResourceWorldGeneration());
    InterlockedExchange(&active,c.cfg_resourceRamCache?1:0);
    resource_warm_hud::Snapshot hud={};
    if(uiTicksAdmitted&&c.cfg_resourceWarmHud){
        resource_cache::Stats r=resource_cache::Snapshot();resource_gpu::Stats g=resource_gpu::Snapshot();ResourceGpuProgress p=ResourceGpuBridgeProgress();
        hud.startupOn=c.cfg_resourceRamCache&&c.cfg_resourceStartupTextures;hud.startupScanning=r.startupScanning;hud.startupDone=r.startupDone;hud.startupCapped=r.startupCapped;
        hud.startupFiles=r.startupFiles;hud.startupProcessed=r.startupProcessed;hud.startupLoaded=r.startupLoaded;hud.startupSkipped=r.startupSkipped;
        resource_geometry::Stats geometry=resource_geometry::Snapshot();
        hud.geometryOn=c.cfg_resourceRamCache&&c.cfg_resourceGeometryWarm;hud.geometryBytes=geometry.heldBytes;hud.geometryModels=geometry.models;hud.geometryReused=geometry.reused;hud.geometryAdmitted=geometry.admitted;
        hud.ramBytes=r.allocatedBytes;hud.ramCap=unsigned __int64(c.cfg_resourceRamGiB)*1024*1024*1024;hud.ramReadBytes=r.readBytes;
        hud.ramFiles=r.entries;hud.ramPending=r.pending+(r.active?1:0);hud.ramActive=r.active;
        hud.gpuBytes=g.allocatedBytes;hud.gpuCap=unsigned __int64(c.cfg_resourceGpuGiB)*1024*1024*1024;
        hud.gpuReady=g.entries;hud.gpuPending=g.pending+(g.active?1:0);hud.gpuCreated=g.created;hud.gpuAdopted=p.adopted;hud.gpuSkipUploads=p.skipped;
        hud.workPaused=!resource_cache::BackgroundAllowed();hud.ramOn=c.cfg_resourceRamCache;hud.gpuOn=c.cfg_resourceRamCache&&c.cfg_resourcePrefetch&&c.cfg_resourceGpuWarm&&p.admitted;
    }
    if(uiTicksAdmitted)ResourceWarmHudTick(c.cfg_resourceWarmHud,hud);
    if(DetailedLogEnabled()&&now-lastReport>=10){
        lastReport=now;resource_cache::Stats s=resource_cache::Snapshot();std::ostringstream out;
        out<<"Resource RAM cache: on="<<c.cfg_resourceRamCache<<" bytes="<<s.allocatedBytes
           <<" capGiB="<<c.cfg_resourceRamGiB<<" files="<<s.entries<<" pending="<<s.pending
           <<" hits="<<s.hits<<" misses="<<s.misses<<" queued="<<s.queued<<" rejected="<<s.rejected<<" readBytes="<<s.readBytes
           <<" startupRoots="<<s.startupRoots<<" startupFiles="<<s.startupFiles<<" startupProcessed="<<s.startupProcessed
           <<" startupLoaded="<<s.startupLoaded<<" startupSkipped="<<s.startupSkipped<<" startupTotalBytes="<<s.startupTotalBytes
           <<" startupReadBytes="<<s.startupReadBytes<<" startupScanning="<<s.startupScanning<<" startupDone="<<s.startupDone<<" startupCapped="<<s.startupCapped;
        resource_geometry::Stats geometry=resource_geometry::Snapshot();
        out<<" geometryAdmitted="<<geometry.admitted<<" geometryOn="<<geometry.enabled<<" geometryBytes="<<geometry.heldBytes<<" geometryCpuShadow="<<geometry.cpuShadowBytes
           <<" geometryModels="<<geometry.models<<" geometryPending="<<geometry.pending<<" geometryObserved="<<geometry.observed<<" geometryRetained="<<geometry.retained
           <<" geometryReferenceHits="<<geometry.reused<<" geometryEvicted="<<geometry.evicted<<" geometryRejected="<<geometry.rejected<<" geometryPressure="<<geometry.pressure;
        LogDetail(out.str());
    }
}
