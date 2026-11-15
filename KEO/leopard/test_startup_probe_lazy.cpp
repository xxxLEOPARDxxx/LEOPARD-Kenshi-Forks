#define RESOURCE_LOAD_PROBE_TEST
#define RESOURCE_LOAD_PROBE_INIT_TEST
#include <windows.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <sstream>
#include <cstdio>
#include <cstdlib>
static bool enabled=false,onMain=true,gog=false,failTextureContract=false,failSwitchContract=false,reenter=false,reenterTiming=false,inCapture=false;
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
void ResourceLoadProbeReport();
namespace resource_load_probe_detail {
static HMODULE TestGetModuleHandleA(const char*name){return reinterpret_cast<HMODULE>(name?0x20000000:0x10000000);}
static FARPROC TestGetProcAddress(HMODULE module,const char*name){
    uintptr_t rva=strstr(name,"_initialise")?0xbfac0:0x1a9be0;
    return reinterpret_cast<FARPROC>(reinterpret_cast<uintptr_t>(module)+rva);
}
static bool TestImageMatches(HMODULE,unsigned long,unsigned long){return true;}
static void* TestAdmitModule(HMODULE module,unsigned long,unsigned long,uintptr_t rva,const unsigned char*){
    return ((failTextureContract&&rva==0x44acd0)||(failSwitchContract&&rva==0x71920))?NULL:reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(module)+rva);
}
static bool InstallAdmitted(const char*name,void*target,void*detour,void**original){
    attempts.push_back(name);
    if(inCapture&&strcmp(name,"texture2D")&&strcmp(name,"morphSwitch"))++earlyTiming;
    if(reenter){reenter=false;inCapture=true;ResourceLoadProbeReport();inCapture=false;}
    if(reenterTiming&&strcmp(name,"texture2D")&&strcmp(name,"morphSwitch")){reenterTiming=false;ResourceLoadProbeReport();}
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
    if(mode=="gog")gog=true;
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
        Check(attempts.empty(),"GOG refuses mandatory and optional native seams");enabled=true;ResourceLoadProbeReport();ResourceLoadProbeInit();
        Check(attempts.empty(),"GOG heartbeat/toggle never bypasses native refusal");
    }else{
        Check(Count("texture2D")==1,"mandatory texture completion hook attempted exactly once");
        Check(Count("morphSwitch")==1&&attempts[0]=="texture2D"&&attempts[1]=="morphSwitch","mandatory final mesh hook second and exactly once");
        Check(morphSwitchInstalled==(!failSwitchContract&&failHook!="morphSwitch"),"mandatory final mesh capture honors refusal independently");
        Check(texture2DInstalled==(!failTextureContract&&failHook!="texture2D"),"mandatory capture honors contract/hook refusal independent of Debug");
        if(!enabled)Check(attempts.size()==2,"Debug OFF installs only mandatory capture");
        else Check(attempts.size()==14,"Debug ON init installs two mandatory plus twelve timing seams");
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
