#include "resources/resource_load_probe.h"
#ifndef RESOURCE_LOAD_PROBE_TEST
#include "base/core.h"
#include "base/log_admission.h"
#include "game/native_profile.h"
#endif

namespace resource_load_probe_detail {
typedef void(*NativeFn)(void*);
static NativeFn originalFactory=NULL,originalResource=NULL;
struct Stats {
    unsigned __int64 calls,totalUs,maxUs,over2ms,over20ms;
    Stats():calls(0),totalUs(0),maxUs(0),over2ms(0),over20ms(0){}
};
static Stats factory,resource;
static bool factoryInstalled=false,resourceInstalled=false;
static double nextReport=0;
static unsigned __int64 AddBounded(unsigned __int64 a,unsigned __int64 b){
    const unsigned __int64 cap=~0ULL;return b>cap-a?cap:a+b;
}
static void Sample(Stats&s,long elapsedUs){
    const unsigned __int64 us=elapsedUs>0?static_cast<unsigned __int64>(elapsedUs):0;
    s.calls=AddBounded(s.calls,1);s.totalUs=AddBounded(s.totalUs,us);
    if(us>s.maxUs)s.maxUs=us;
    if(us>2000)s.over2ms=AddBounded(s.over2ms,1);
    if(us>20000)s.over20ms=AddBounded(s.over20ms,1);
}
static void Invoke(NativeFn native,void*self,Stats&s){
    // Cached admission only. No QPC/stat/log/heap/lock on the disabled path.
    if(!DetailedLogEnabled()||!IsMainThread()){native(self);return;}
    const LONGLONG begin=QpcNow();
    // Do not catch native C++/SEH exceptions or add cleanup to their unwind.
    // Only successfully completed calls contribute a sample.
    native(self);
    Sample(s,QpcDeltaUs(begin,QpcNow()));
}
static void Factory(void*self){Invoke(originalFactory,self,factory);}
static void Resource(void*self){Invoke(originalResource,self,resource);}
static void Append(std::ostringstream&out,const char*name,const Stats&s,bool installed){
    out<<" "<<name<<"={installed="<<(installed?1:0)<<" calls="<<s.calls
       <<" totalUs="<<s.totalUs<<" maxUs="<<s.maxUs
       <<" over2ms="<<s.over2ms<<" over20ms="<<s.over20ms<<"}";
}
#ifndef RESOURCE_LOAD_PROBE_TEST
static bool Install(const char*name,uintptr_t rva,const unsigned char*bytes,NativeFn detour,NativeFn*original){
    void*target=reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(GetModuleHandleA(NULL))+rva);
    // Exact bytes also refuse a prior profiler/other detour instead of replacing it.
    if(memcmp(target,bytes,32)!=0){
        LogError(std::string("Resource loading probe: ")+name+" native contract refused; diagnostic unavailable");return false;
    }
    if(KenshiLib::AddHook(target,reinterpret_cast<void*>(detour),reinterpret_cast<void**>(original))!=KenshiLib::SUCCESS){
        *original=NULL;LogError(std::string("Resource loading probe: ")+name+" hook failed; diagnostic unavailable");return false;
    }
    return true;
}
#endif
}

#ifndef RESOURCE_LOAD_PROBE_TEST
void ResourceLoadProbeInit(){
    using namespace resource_load_probe_detail;
    static bool attempted=false;
    if(attempted||!IsMainThread())return;attempted=true;
    if(NativeProfileIsGog()){
        KEO_DETAIL("Resource loading probe: optional native seams not admitted for GOG");return;
    }
    static const unsigned char factoryBytes[32]={0x48,0x89,0x5c,0x24,0x20,0x57,0x48,0x83,0xec,0x40,0x48,0x8b,0xd9,0x48,0x89,0x6c,0x24,0x58,0x48,0x8d,0x4c,0x24,0x50,0x33,0xed,0x8b,0xfd,0xe8,0x96,0xa8,0xab,0xff};
    static const unsigned char resourceBytes[32]={0x48,0x8b,0xc4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xa8,0xe8,0xfd,0xff,0xff,0x48,0x81,0xec,0xf0,0x02,0x00,0x00,0x48,0xc7,0x45,0x68,0xfe,0xff};
    factoryInstalled=Install("factoryDrain",0x582880,factoryBytes,&Factory,&originalFactory);
    resourceInstalled=Install("resourceFinish",0x44BBA0,resourceBytes,&Resource,&originalResource);
    KEO_DETAIL(std::string("Resource loading probe: factoryDrain=")+(factoryInstalled?"installed":"unavailable")+" resourceFinish="+(resourceInstalled?"installed":"unavailable")+" (completed calls; inclusive elapsed, not CPU time or total NPC spawn)");
}
#endif
void ResourceLoadProbeReport(){
    using namespace resource_load_probe_detail;
    if(!DetailedLogEnabled()||!IsMainThread())return;
    const double now=ElapsedSec();
    if(nextReport==0){nextReport=now+10;return;}
    if(now<nextReport)return;nextReport=now+10;
    std::ostringstream out;out<<"Resource loading probe: cumulative completed inclusiveElapsed";
    Append(out,"factoryDrain",factory,factoryInstalled);
    Append(out,"resourceFinish",resource,resourceInstalled);
    LogMsg(out.str());
}
