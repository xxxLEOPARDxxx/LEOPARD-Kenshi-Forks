#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "resources/resource_cache.h"
#include "resources/resource_cache_store.h"
#include "resources/resource_forward.h"
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
    ResourceCacheTick(); // honor opt-in INI before the first scene loads
    ResourceForwardInit();
}
void ResourceCacheTick(){
    if(!installed)return;
    static double next=0,lastReport=0;double now=ElapsedSec();if(now<next)return;next=now+1;
    const zone::ZoneConfig&c=zone::g_zoneCfg;
    resource_cache::Configure(c.cfg_resourceRamCache,unsigned __int64(c.cfg_resourceRamGiB)*1024*1024*1024,c.cfg_resourceReadMBps);
    InterlockedExchange(&active,c.cfg_resourceRamCache?1:0);
    if(DetailedLogEnabled()&&now-lastReport>=10){
        lastReport=now;resource_cache::Stats s=resource_cache::Snapshot();std::ostringstream out;
        out<<"Resource RAM cache: on="<<c.cfg_resourceRamCache<<" bytes="<<s.allocatedBytes
           <<" capGiB="<<c.cfg_resourceRamGiB<<" files="<<s.entries<<" pending="<<s.pending
           <<" hits="<<s.hits<<" misses="<<s.misses<<" queued="<<s.queued<<" rejected="<<s.rejected<<" readBytes="<<s.readBytes;
        LogDetail(out.str());
    }
}
