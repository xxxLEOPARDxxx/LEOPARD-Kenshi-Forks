// Actual production hook bodies, included by test_ogre_purge_wrappers.py after
// removing include/intrinsic declarations. Native calls/owner/clock are substituted.
#define NOMINMAX
#include <windows.h>
#include <stdint.h>
#include <cstring>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include "render/render_config.h"
#include "render/module_hooks.h"
#include "render/ogre_purge_policy.h"
static double testNow=0;
static bool adoption=true,owner=true;
bool isTransitionActive=false;
RenderConfig g_renderCfg={};
bool ZoneHandoffAdoptionCycleInFlight(){return adoption;}
bool IsMainThread(){return owner;}
double ElapsedSec(){return testNow;}
bool DetailedLogEnabled(){return false;}
void LogError(const std::string&){}
#define KEO_DETAIL(x) do {} while(0)
static uintptr_t testCaller=0;
static void* PurgeTestReturnAddress(){return reinterpret_cast<void*>(testCaller);}
#define _ReturnAddress PurgeTestReturnAddress
static unsigned groupCalls=0,managerCalls=0;static bool throwNative=false;
static const void*seenName=NULL;static void*seenManager=NULL;static bool seenReload=false;
static void NativeGroup(void*m,const void*n,bool r){++groupCalls;seenName=n;seenManager=m;seenReload=r;if(throwNative)throw 71;}
static void NativeManager(void*m,bool r){++managerCalls;seenManager=m;seenReload=r;if(throwNative)throw 71;}
static const uintptr_t returns[6]={/* RETURN_RVAS */ 0xA0EB6B,0xA0EB7D,0xA0EBBA,0xA0EC08,0xA0EC56,0xA0ECA4};
static std::vector<unsigned char> image(0x232c000);
static const DWORD profileStamp=/* PROFILE_STAMP */ 0x65d604d7, profileSize=/* PROFILE_SIZE */ 0x232c000;
static const bool profileGog=/* PROFILE_GOG */ false;
static DWORD imageStamp=profileStamp,imageSize=profileSize;
static bool verifiedGog=profileGog;
static unsigned hookInstalls=0;static bool failManagerHook=false;
static HMODULE TestModule(const char*){return reinterpret_cast<HMODULE>(&image[0]);}
#define GetModuleHandleA TestModule
bool ReadModuleImageId(HMODULE,DWORD*stamp,DWORD*size){*stamp=imageStamp;*size=imageSize;return true;}
bool NativeProfileIsGog(){return verifiedGog;}
bool InstallModuleHook(const ModuleSite&site,void*,void**original,bool*){
    ++hookInstalls;const bool group=std::strstr(site.name,"InGroup")!=NULL;
    if(!group&&failManagerHook)return false;
    *original=group?reinterpret_cast<void*>(&NativeGroup):reinterpret_cast<void*>(&NativeManager);return true;
}
// PRODUCTION_BODY
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){std::printf("FAIL line%u %s\n",__LINE__,#x);return 1;}}while(0)
static void Invoke(unsigned i){
    testCaller=reinterpret_cast<uintptr_t>(&image[0])+returns[i];
    if(i<2)hook_UnloadUnreferenced(reinterpret_cast<void*>(0x100+i),i==1);
    else hook_UnloadGroup(reinterpret_cast<void*>(0x200),reinterpret_cast<void*>(0x300+i),true);
}
int main(){
    // SITE_BYTES: runner fills known caller windows from the pinned disk EXE.
    g_renderCfg.adoptOgrePurgeSkip=true;g_renderCfg.adoptOgrePurgeMaxSkipSeconds=120;
    C(InstallOgrePurgeSkip());
    testNow=10;for(unsigned i=0;i<6;++i)Invoke(i);C(groupCalls+managerCalls==0);
    testNow=121;for(unsigned i=0;i<6;++i)Invoke(i);
    C(managerCalls==2&&groupCalls==4); // RED: shared clock forwards only first.
    testNow=121.001;for(unsigned i=0;i<6;++i)Invoke(i);C(managerCalls==2&&groupCalls==4);
    testNow=242;for(unsigned i=0;i<6;++i)Invoke(i);C(managerCalls==4&&groupCalls==8);
    testNow=243;adoption=false;for(unsigned i=0;i<6;++i)Invoke(i);C(managerCalls==6&&groupCalls==12);
    adoption=true;testNow=244;for(unsigned i=0;i<6;++i)Invoke(i);C(managerCalls==6&&groupCalls==12);
    isTransitionActive=true;for(unsigned i=0;i<6;++i)Invoke(i);C(managerCalls==8&&groupCalls==16);isTransitionActive=false;
    g_renderCfg.adoptOgrePurgeSkip=false;for(unsigned i=0;i<6;++i)Invoke(i);C(managerCalls==10&&groupCalls==20);
    C(seenName==reinterpret_cast<void*>(0x305)&&seenManager==reinterpret_cast<void*>(0x200)&&seenReload);
    g_renderCfg.adoptOgrePurgeSkip=true;
    // Independent deadlines even when the target calls are interleaved.
    testNow=365;Invoke(0);C(managerCalls==11);
    testNow=370;for(unsigned i=1;i<6;++i)Invoke(i);C(managerCalls==12&&groupCalls==24);
    testNow=400;Invoke(0);C(managerCalls==12);
    // A different original caller may not advance any native channel clock.
    double clock0=s_lastPurgeAt[0];testCaller=reinterpret_cast<uintptr_t>(&image[0])+0x111;
    hook_UnloadUnreferenced(reinterpret_cast<void*>(0x777),false);C(managerCalls==13&&s_lastPurgeAt[0]==clock0);
    C(seenManager==reinterpret_cast<void*>(0x777)&&!seenReload);
    testCaller=reinterpret_cast<uintptr_t>(&image[0])+returns[0];
    hook_UnloadGroup(reinterpret_cast<void*>(0x888),reinterpret_cast<void*>(0x999),false);
    C(groupCalls==25&&s_lastPurgeAt[0]==clock0); // wrong hook kind is unknown
    C(seenName==reinterpret_cast<void*>(0x999)&&seenManager==reinterpret_cast<void*>(0x888)&&!seenReload);
    owner=false;Invoke(0);owner=true;C(managerCalls==14&&s_lastPurgeAt[0]==clock0);
    // Strict boundary and explicitly disabled fallback retain old arithmetic.
    testNow=485;Invoke(0);C(managerCalls==14);
    testNow=485.001;Invoke(0);C(managerCalls==15);
    g_renderCfg.adoptOgrePurgeMaxSkipSeconds=0;testNow=1e9;for(unsigned i=0;i<6;++i)Invoke(i);
    C(managerCalls==15&&groupCalls==25);g_renderCfg.adoptOgrePurgeMaxSkipSeconds=120;
    // Native exception and arguments are unchanged; no duplicate call.
    testNow=600;throwNative=true;bool caught=false;try{Invoke(2);}catch(int v){caught=v==71;}throwNative=false;
    C(caught&&groupCalls==26&&seenName==reinterpret_cast<void*>(0x302));
    // Installation time, not process time, starts all cold skip windows.
    s_tried=false;testNow=2000;C(InstallOgrePurgeSkip());
    testNow=2100;for(unsigned i=0;i<6;++i)Invoke(i);C(managerCalls==15&&groupCalls==26);
    // One mismatched caller byte refuses the whole optional lever before hooks.
    unsigned installs=hookInstalls;s_tried=false;s_installed=false;
    image[returns[5]-1]^=1;C(!InstallOgrePurgeSkip());C(hookInstalls==installs);image[returns[5]-1]^=1;
    s_tried=false;imageStamp=0x6602d5e3;imageSize=0x232c000;
    C(!InstallOgrePurgeSkip()&&hookInstalls==installs); // unsupported second exe
    s_tried=false;imageStamp=profileStamp;imageSize=profileSize;verifiedGog=!profileGog;
    C(!InstallOgrePurgeSkip()&&hookInstalls==installs); // mismatched authenticated platform
    verifiedGog=profileGog;
    // Partial hook installation remains pass-through, never active skipping.
    s_tried=false;failManagerHook=true;C(!InstallOgrePurgeSkip());
    unsigned calls=groupCalls;Invoke(2);C(groupCalls==calls+1);failManagerHook=false;
    s_tried=false;C(InstallOgrePurgeSkip());
    std::printf("PASS %u actual purge-wrapper controls\n",checks);return 0;
}
