#define RESOURCE_LOAD_PROBE_TEST
#define RESOURCE_LOAD_PROBE_INIT_TEST
#include <windows.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include "fixes/world/morph_pose_scope.h"
static bool enabled=false,onMain=true,gog=false,failTextureContract=false,failSwitchContract=false,reenter=false,reenterTiming=false,inCapture=false;
static bool morphCreateReady=false,failGogTail=false;
static unsigned clocks=0,elapsedReads=0,errors=0,checks=0,earlyTiming=0;
static double seconds=0;
static std::string failHook,output;
static std::vector<std::string> attempts;
bool DetailedLogEnabled(){return enabled;}
bool IsMainThread(){return onMain;}
bool NativeProfileIsGog(){return gog;}
LONGLONG QpcNow(){++clocks;return 0;}
long QpcDeltaUs(LONGLONG,LONGLONG){return 0;}
double ElapsedSec(){++elapsedReads;return seconds;}
void LogMsg(const std::string&s){output+=s;output+='\n';}
void LogError(const std::string&){++errors;}
#define KEO_DETAIL(s) do{if(DetailedLogEnabled())LogMsg(s);}while(0)
namespace resource_texture_retention {void ObserveLoadedResult(void*){}}
namespace resource_geometry {void ObserveFinalEntity(void*){}}
namespace morph_pose_scope {
void* Create(CreateFn fn,const void*name,void*entity,void*data,bool shaved){return fn(name,entity,data,shaved);}
void SetCreateHookReady(bool ready){morphCreateReady=ready;}
}
void ResourceLoadProbeReport();
namespace resource_load_probe_detail {
static HMODULE TestGetModuleHandleA(const char*name){return reinterpret_cast<HMODULE>(name?0x20000000:0x10000000);}
static FARPROC TestGetProcAddress(HMODULE module,const char*name){
    uintptr_t rva=strstr(name,"_initialise")?0xbfac0:0x1a9be0;
    return reinterpret_cast<FARPROC>(reinterpret_cast<uintptr_t>(module)+rva);
}
static bool TestImageMatches(HMODULE module,unsigned long stamp,unsigned long size){
    return reinterpret_cast<uintptr_t>(module)!=0x10000000||
        (gog?(stamp==0x65d60519&&size==0x232a000):(stamp==0x65d604d7&&size==0x232c000));
}
static void* TestAdmitModule(HMODULE module,unsigned long stamp,unsigned long size,uintptr_t rva,const unsigned char*){
    if(!TestImageMatches(module,stamp,size))return NULL;
    if((failTextureContract&&(rva==0x44acd0||rva==0x44a8f0))||(failSwitchContract&&rva==0x71920)||
        (failGogTail&&rva==0x44aa20))return NULL;
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(module)+rva);
}
static bool InstallAdmitted(const char*name,void*target,void*detour,void**original){
    attempts.push_back(name);
    if(inCapture&&strcmp(name,"texture2D")&&strcmp(name,"morphSwitch")&&strcmp(name,"morphCreate"))++earlyTiming;
    if(reenter){reenter=false;inCapture=true;ResourceLoadProbeReport();inCapture=false;}
    if(reenterTiming&&strcmp(name,"texture2D")&&strcmp(name,"morphSwitch")&&strcmp(name,"morphCreate")){reenterTiming=false;ResourceLoadProbeReport();}
    if(!target||failHook==name){*original=NULL;return false;}
    *original=detour;return true;
}
static bool Install(const char*name,uintptr_t,const unsigned char*,void(*detour)(void*),void(**original)(void*)){
    return InstallAdmitted(name,reinterpret_cast<void*>(1),reinterpret_cast<void*>(detour),reinterpret_cast<void**>(original));
}
}
// Runner compiles the production initializer/report with only native backend
// dependency names rebound; wrapper bodies and initialization logic are exact.
#include "startup_probe_production.cpp"
static void Check(bool ok,const char*name){++checks;if(!ok){printf("FAIL %s\n",name);exit(1);}}
static unsigned Count(const char*name){unsigned n=0;for(size_t i=0;i<attempts.size();++i)if(attempts[i]==name)++n;return n;}
int main(int argc,char**argv){
    using namespace resource_load_probe_detail;
    std::string mode=argc>1?argv[1]:"off";
    if(mode.compare(0,3,"gog")==0)gog=true;
    if(mode=="gog-tail-fail")failGogTail=true;
    if(mode=="gog-create-hook-fail")failHook="morphCreate";
    if(mode=="on"||mode=="early"||mode=="reentrant")enabled=true;
    if(mode=="capture-fail")failTextureContract=true;
    if(mode=="capture-hook-fail")failHook="texture2D";
    if(mode=="switch-hook-fail")failHook="morphSwitch";
    if(mode=="switch-contract-fail")failSwitchContract=true;
    if(mode=="timing-fail")failHook="morphLookup";
    if(mode=="fallback")failHook="meshCompletionCallback";
    if(mode=="early"){
        ResourceLoadProbeReport();Check(attempts.empty(),"report before mandatory init cannot install timing hooks");
    }
    if(mode=="worker"){
        onMain=false;ResourceLoadProbeInit();ResourceLoadProbeReport();Check(attempts.empty(),"worker cannot claim owner initialization");onMain=true;
    }
    if(mode=="reentrant")reenter=reenterTiming=true;
    ResourceLoadProbeInit();
    if(gog){
        Check(attempts.size()==3&&Count("texture2D")==1&&Count("morphSwitch")==1&&Count("morphCreate")==1,
            "GOG installs only three existing functional hooks");
        Check(texture2DInstalled==!failGogTail&&morphSwitchInstalled,"GOG refusal is local to its texture contract");
        Check(morphCreateInstalled==(failHook.empty())&&morphCreateReady==morphCreateInstalled,
            "GOG pose scope enabled only after successful Create hook");
        enabled=true;ResourceLoadProbeReport();ResourceLoadProbeInit();
        Check(attempts.size()==3,"GOG heartbeat/toggle never retries or enables optional hooks");
    }else{
        Check(Count("texture2D")==1,"mandatory texture completion hook attempted exactly once");
        Check(Count("morphSwitch")==1&&attempts[0]=="texture2D"&&attempts[1]=="morphSwitch","mandatory final mesh hook second and exactly once");
        Check(morphSwitchInstalled==(!failSwitchContract&&failHook!="morphSwitch"),"mandatory final mesh capture honors refusal independently");
        Check(texture2DInstalled==(!failTextureContract&&failHook!="texture2D"),"mandatory capture honors contract/hook refusal independent of Debug");
        Check(Count("morphCreate")==1&&morphCreateReady,"functional morph scope armed once");
        if(!enabled)Check(attempts.size()==3,"Debug OFF installs only mandatory capture");
        else Check(attempts.size()==14,"Debug ON init installs three mandatory plus eleven timing seams");
        unsigned prior=unsigned(attempts.size());ResourceLoadProbeInit();
        Check(Count("texture2D")==1&&attempts.size()==prior,"repeated init never reinstalls capture");
        enabled=false;unsigned reads=elapsedReads;
        ResourceLoadProbeReport();Check(attempts.size()==prior&&clocks==0&&elapsedReads==reads,"Debug OFF owner report does no metric clock or installs");
        enabled=true;seconds=0.5;ResourceLoadProbeReport();
        Check(attempts.size()==(mode=="fallback"?15:14),"first Debug ON heartbeat installs timing before report throttle");
        Check(Count("texture2D")==1&&Count("morphSwitch")==1&&Count("factoryDrain")==1&&Count("poseBlend")==1,"capture and timing attempts remain separate and once");
        if(mode=="fallback")Check(Count("appearanceBody")==1&&!meshCallbackInstalled&&bodyInstalled,"existing callback refusal keeps body timing fallback");
        if(mode=="timing-fail")Check(!morphLookupInstalled&&morphCreateInstalled,"one timing refusal leaves other diagnostics admitted");
        prior=unsigned(attempts.size());ResourceLoadProbeReport();ResourceLoadProbeInit();
        enabled=false;ResourceLoadProbeReport();enabled=true;seconds=100;ResourceLoadProbeReport();
        Check(attempts.size()==prior,"failures and repeated OFF ON never retry installed or refused seams");
        Check(clocks==0,"initialization/report never take measurement QPC");
        Check(earlyTiming==0,"reentrant report cannot install timing during mandatory capture attempt");
    }
    printf("PASS %u production startup-init controls %s\n",checks,mode.c_str());return 0;
}
