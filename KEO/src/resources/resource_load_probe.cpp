#include "resources/resource_load_probe.h"
#ifndef RESOURCE_LOAD_PROBE_TEST
#include "base/core.h"
#include "base/log_admission.h"
#include "game/native_profile.h"
#endif

namespace resource_load_probe_detail {
typedef void(*NativeFn)(void*);
typedef void*(*FactoryProcessFn)(void*,void*);
typedef void(*SetMeshDataFn)(void*,const void*);
typedef void(*EntityInitialiseFn)(void*,bool);
static NativeFn originalFactory=NULL,originalResource=NULL;
static FactoryProcessFn originalFactoryProcess=NULL;
static SetMeshDataFn originalSetMeshData=NULL;
static EntityInitialiseFn originalEntityInitialise=NULL;
struct Stats {
    unsigned __int64 calls,totalUs,maxUs,over2ms,over20ms;
    Stats():calls(0),totalUs(0),maxUs(0),over2ms(0),over20ms(0){}
};
static Stats factory,resource;
static Stats factoryProcess,setMeshData,entityInitialise;
static bool factoryInstalled=false,resourceInstalled=false;
static bool factoryProcessInstalled=false,setMeshDataInstalled=false,entityInitialiseInstalled=false;
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
static void* FactoryProcess(void*self,void*item){
    if(!DetailedLogEnabled()||!IsMainThread())return originalFactoryProcess(self,item);
    const LONGLONG begin=QpcNow();
    void*result=originalFactoryProcess(self,item);
    Sample(factoryProcess,QpcDeltaUs(begin,QpcNow()));return result;
}
static void SetMeshData(void*entity,const void*data){
    if(!DetailedLogEnabled()||!IsMainThread()){originalSetMeshData(entity,data);return;}
    const LONGLONG begin=QpcNow();originalSetMeshData(entity,data);
    Sample(setMeshData,QpcDeltaUs(begin,QpcNow()));
}
static void EntityInitialise(void*entity,bool force){
    if(!DetailedLogEnabled()||!IsMainThread()){originalEntityInitialise(entity,force);return;}
    const LONGLONG begin=QpcNow();originalEntityInitialise(entity,force);
    Sample(entityInitialise,QpcDeltaUs(begin,QpcNow()));
}
// Startup-only admission. Catch unreadable module headers/bytes, never native
// wrapper exceptions. Function contracts contain the full 32-byte prefix.
static bool ImageMatches(HMODULE module,unsigned long stamp,unsigned long size){
    __try{
        if(!module)return false;
        const unsigned char*base=reinterpret_cast<const unsigned char*>(module);
        const IMAGE_DOS_HEADER*dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>1024*1024)return false;
        const IMAGE_NT_HEADERS64*nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
        return nt->Signature==IMAGE_NT_SIGNATURE&&nt->FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64
            &&nt->OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR64_MAGIC
            &&nt->FileHeader.TimeDateStamp==stamp&&nt->OptionalHeader.SizeOfImage==size;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static void* AdmitModule(HMODULE module,unsigned long stamp,unsigned long size,uintptr_t rva,const unsigned char*bytes){
    if(!ImageMatches(module,stamp,size)||size<32||rva>size-32)return NULL;
    __try{
        unsigned char*target=reinterpret_cast<unsigned char*>(module)+rva;
        return memcmp(target,bytes,32)==0?target:NULL;
    }__except(EXCEPTION_EXECUTE_HANDLER){return NULL;}
}
static void Append(std::ostringstream&out,const char*name,const Stats&s,bool installed){
    out<<" "<<name<<"={installed="<<(installed?1:0)<<" calls="<<s.calls
       <<" totalUs="<<s.totalUs<<" maxUs="<<s.maxUs
       <<" over2ms="<<s.over2ms<<" over20ms="<<s.over20ms<<"}";
}
#ifndef RESOURCE_LOAD_PROBE_TEST
static bool InstallAdmitted(const char*name,void*target,void*detour,void**original){
    if(!target){LogError(std::string("Resource loading probe: ")+name+" module/function contract refused; diagnostic unavailable");return false;}
    if(KenshiLib::AddHook(target,detour,original)!=KenshiLib::SUCCESS){
        *original=NULL;LogError(std::string("Resource loading probe: ")+name+" hook failed; diagnostic unavailable");return false;
    }
    return true;
}
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
    static const unsigned char processBytes[32]={0x48,0x8b,0xc4,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xa8,0xa8,0xfe,0xff,0xff,0x48,0x81,0xec,0x20,0x02,0x00,0x00,0x48,0xc7,0x45,0xc0};
    static const unsigned char meshBytes[32]={0x48,0x8b,0xc4,0x57,0x48,0x81,0xec,0x80,0x00,0x00,0x00,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff,0x48,0x89,0x58,0x18,0x48,0x89,0x70,0x20,0x48,0x8b,0x05,0x45};
    static const unsigned char entityBytes[32]={0x48,0x8b,0xc4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x68,0xa1,0x48,0x81,0xec,0xe0,0x00,0x00,0x00,0x48,0xc7,0x45,0x97,0xfe,0xff,0xff,0xff,0x48};
    HMODULE exe=GetModuleHandleA(NULL);
    const bool steam=ImageMatches(exe,0x65d604d7,0x232c000);
    factoryProcessInstalled=InstallAdmitted("factoryProcess",steam?AdmitModule(exe,0x65d604d7,0x232c000,0x580CE0,processBytes):NULL,reinterpret_cast<void*>(&FactoryProcess),reinterpret_cast<void**>(&originalFactoryProcess));
    setMeshDataInstalled=InstallAdmitted("setMeshData",steam?AdmitModule(exe,0x65d604d7,0x232c000,0x447850,meshBytes):NULL,reinterpret_cast<void*>(&SetMeshData),reinterpret_cast<void**>(&originalSetMeshData));
    HMODULE ogre=GetModuleHandleA("OgreMain_x64.dll");
    void*entity=steam?AdmitModule(ogre,0x5ca5f929,0x9c9000,0xBFAC0,entityBytes):NULL;
    if(entity&&reinterpret_cast<void*>(GetProcAddress(ogre,"?_initialise@Entity@Ogre@@QEAAX_N@Z"))!=entity)entity=NULL;
    entityInitialiseInstalled=InstallAdmitted("entityInitialise",entity,reinterpret_cast<void*>(&EntityInitialise),reinterpret_cast<void**>(&originalEntityInitialise));
    KEO_DETAIL(std::string("Resource loading probe: factoryDrain=")+(factoryInstalled?"installed":"unavailable")+" resourceFinish="+(resourceInstalled?"installed":"unavailable")+" factoryProcess="+(factoryProcessInstalled?"installed":"unavailable")+" setMeshData="+(setMeshDataInstalled?"installed":"unavailable")+" entityInitialise="+(entityInitialiseInstalled?"installed":"unavailable")+" (completed calls; inclusive elapsed, not CPU time; generic factory/entities, not NPC-only; overlapping totals must not be summed)");
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
    Append(out,"factoryProcess",factoryProcess,factoryProcessInstalled);
    Append(out,"setMeshData",setMeshData,setMeshDataInstalled);
    Append(out,"entityInitialise",entityInitialise,entityInitialiseInstalled);
    LogMsg(out.str());
}
