#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ModConfigMenu.h>
#include "../TauntPolicy.h"
#include "../BonusTransaction.h"
#define CHECK(x) do { if(!(x)){printf("FAIL lifecycle line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
using TauntExpansion::Config;
Config g_config;
// The actual production globals and INI readers/LoadConfig are extracted below.
#include "lifecycle_state_under_test.h"
static int errors=0,resolves=0,installs=0,hooks=0,removes=0;
static const char* ini="D:/DEV/Kenshi/build/TauntExpansionTests/lifecycle.ini";
namespace PluginSettings {static std::string Path(){return ini;}}
static void DebugLog(const char*){}
static void ErrorLog(const char*){++errors;}
static const char* Tr(const char* s){return s;}
#include "lifecycle_config_under_test.h"
struct AI{void chooseBestAttackTarget(){}};
static float nativeBonus=2;
static intptr_t target=0x1000;
static bool installWorks=true;
static bool Original(){return true;}
static bool (*g_originalChooseBestAttackTarget)()=NULL;
static void HookedChooseBestAttackTarget(){}
struct Patch{float vanillaBonus;float* activeBonusAddress;Patch():vanillaBonus(2),activeBonusAddress(NULL){}}g_tauntPatch;
namespace KenshiLib {
    enum HookStatus{SUCCESS,FAIL};
    static HookStatus hookResult=SUCCESS;
    template<typename T> intptr_t GetRealAddress(T){++resolves;CHECK(g_config.tauntMultiplier==3);return target;}
    static HookStatus AddHook(intptr_t at,void*,bool(**original)()){
        ++hooks;CHECK(at==target);*original=hookResult==SUCCESS ? Original : NULL;return hookResult;
    }
}
static bool InstallTauntMultiplierPatch(intptr_t at){++installs;CHECK(at==target);g_tauntPatch.activeBonusAddress=&nativeBonus;return installWorks;}
static bool RemoveTauntMultiplierPatch(){++removes;g_transaction.address=NULL;return true;}
#include "lifecycle_entries_under_test.h"
static MCM_ApplyFn capturedApply=NULL;
static bool expectStarted=false;
static void Begin(MCM_Api*,const char* id,const char*,const char* path,MCM_ApplyFn apply){
    CHECK(!strcmp(id,"TauntExpansion") && !strcmp(path,ini));CHECK(g_started==expectStarted);
    CHECK(apply==LoadConfig);capturedApply=apply;
}
static void Section(MCM_Api*,const char*){}
static void Info(MCM_Api*,const char*){}
static void Toggle(MCM_Api*,const char*,const char*,const char*,const char*,int,unsigned){}
static void Number(MCM_Api*,const char*,const char*,const char*,const char*,float,float,float,int,unsigned){}
static void Integer(MCM_Api*,const char*,const char*,const char*,const char*,int,int,int,unsigned){}
typedef void(*Entry)();
typedef void(*Describe)(MCM_Api*);
static MCM_Api Api(){MCM_Api a={};a.version=6;a.beginMod=Begin;a.section=Section;a.info=Info;a.toggle=Toggle;a.number=Number;a.integer=Integer;return a;}
static void Settings(const char* multiplier){
    CHECK(WritePrivateProfileStringA("General","Enabled","0",ini));
    CHECK(WritePrivateProfileStringA("Taunt","Multiplier",multiplier,ini));
    CHECK(WritePrivateProfileStringA("Logging","Debug","0",ini));
}
static void Reset(){
    g_started=false;g_stopped=false;g_activeDepth=0;g_configPending=false;
    g_config=Config();g_transaction.Initialise(NULL);g_originalChooseBestAttackTarget=NULL;
    resolves=installs=hooks=removes=errors=0;target=0x1000;installWorks=true;KenshiLib::hookResult=KenshiLib::SUCCESS;
    Settings("3");
}
static void DescribeApply(Describe describe,bool started){
    expectStarted=started;MCM_Api api=Api();capturedApply=NULL;describe(&api);CHECK(capturedApply!=NULL);
    Settings("7");capturedApply();CHECK(g_config.tauntMultiplier==7 && !g_config.enabled);
}
int main(int argc,char** argv){
    // This assertion deterministically exposes the old uninitialized lock,
    // without hanging the process inside EnterCriticalSection's undefined use.
    if(argc>1 && !strcmp(argv[1],"early")){
        CHECK(g_transaction.lock.LockCount==-1);
        DescribeApply(&MCM_Describe,false);CHECK(resolves==0 && hooks==0 && !g_started);
        DeleteFileA(ini);puts("PASS: actual early MCM Describe/apply uses initialized lock before startup");return 0;
    }
    HMODULE module=GetModuleHandleA(NULL);
    Entry start=reinterpret_cast<Entry>(GetProcAddress(module,"?startPlugin@@YAXXZ"));
    Entry stop=reinterpret_cast<Entry>(GetProcAddress(module,"?stopPlugin@@YAXXZ"));
    Describe describe=reinterpret_cast<Describe>(GetProcAddress(module,"MCM_Describe"));
    CHECK(start && stop && describe);
    CHECK(g_transaction.lock.LockCount==-1);
    Reset();start();CHECK(g_started && resolves==1 && installs==1 && hooks==1 && errors==0);
    DescribeApply(describe,true);start();CHECK(resolves==1 && hooks==1);stop();CHECK(g_stopped && !g_config.enabled);
    Reset();target=0;start();CHECK(g_started && resolves==1 && installs==0 && hooks==0 && errors==1);
    DescribeApply(describe,true);
    Reset();installWorks=false;start();CHECK(installs==1 && hooks==0 && errors==1);DescribeApply(describe,true);
    Reset();KenshiLib::hookResult=KenshiLib::FAIL;start();CHECK(hooks==1 && removes==1 && !g_transaction.address && errors==1);
    DescribeApply(describe,true);
    DeleteFileA(ini);
    puts("PASS: exact RE loader GetProcAddress entry executes actual startup before Describe/apply; duplicate start and all startup failure callbacks safe");
    return 0;
}
