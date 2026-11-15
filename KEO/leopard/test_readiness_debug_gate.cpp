#include <windows.h>
#include <stdint.h>
#include <iostream>
#include <sstream>
#include <intrin.h>
#include "zone/readiness/zone_readiness_contract.h"
#include "zone/zone_ledger_core.h"
static int checks=0;
#define C(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";exit(1);}}while(0)
#define KLIB_MEMBER(g,b,m,o) ((uintptr_t)(b)+(o))
#define KEO_DETAIL(x) do{(void)(x);}while(0)
const int OFF_RDY_SM_PENDING_SECTIONS=0,OFF_ZM_LOADING=0;
enum{RC_POLL4,RC_MAIN,RC_OFF,RC_COUNT};enum{RZ_NO_SECTION,RZ_OUTDOOR_MISSING,RZ_BUILDINGS_PENDING,RZ_NOT_IN_WORLD,RZ_UNKNOWN,RZ_COUNT};
const int RDY_OFF_SAMPLE_MASK=15;
struct Counters{volatile LONG g_rdyCls[RC_COUNT][RZ_COUNT],g_rdyBypass,g_rdyRuleReady,g_rdyRuleWait,g_rdyOffSample;};static Counters g_rdy;
struct ContractCounters{volatile LONG g_rdyContractPrivate,g_rdyContractAdopted,g_rdyContractGlobal;};static ContractCounters g_rdyContract;
namespace zone{struct Cfg{bool deferralEnabled,readinessOverridesEnabled,islandReadinessRuleEnabled;}g_zoneCfg;}
static bool mainThread=true,debugOn=false,nativeResult=false,throwNative=false,isTransitionActive=false,prioritizedThisTransition=false;
static int nativeCalls=0,priorityCalls=0,classifyCalls=0,readinessCalls=0,bridgeCalls=0,byClass=0,byTrace=0,deferredFrameCount=0;
static void* g_cachedZoneMgr=0;
static bool IsMainThread(){return mainThread;}
static bool DetailedLogEnabled(){return debugOn;}
static void CallPrioritizeNavMeshQueue(){++priorityCalls;}
static bool Native(void*,void*){++nativeCalls;if(throwNative)throw 173;return nativeResult;}
namespace game{struct Hook{bool(*orig_isContentPending)(void*,void*);}g_hookOrig={Native};}
static void IslandCountReadiness(bool,bool){++readinessCalls;}
static int ZoneReadinessBridgeClassOf(int,int){++bridgeCalls;return byClass;}
static bool IsStatePoll4(){return false;}
static int ClassifyZoneReadiness(uintptr_t,const int*,bool){++classifyCalls;return RZ_OUTDOOR_MISSING;}
static void ReadinessNoteBypass(uintptr_t,const int*,bool,int,int,int,bool,int){++byTrace;}
static double ElapsedSec(){return 1;}
// PRODUCTION
static void ClearCounts(){nativeCalls=priorityCalls=classifyCalls=readinessCalls=bridgeCalls=byTrace=0;memset(&g_rdy,0,sizeof(g_rdy));memset(&g_rdyContract,0,sizeof(g_rdyContract));}
int main()
{
 int manager=0,pos[2]={1,2};unsigned char loading=0;g_cachedZoneMgr=&loading;
 for(int debug=0;debug<2;++debug)for(int owner=0;owner<2;++owner)for(int original=0;original<2;++original)
 for(int def=0;def<2;++def)for(int overrides=0;overrides<2;++overrides)for(int rule=0;rule<2;++rule)
 for(int load=0;load<2;++load)for(int cls=0;cls<4;++cls)for(int sections=0;sections<2;++sections)
 {
  debugOn=debug!=0;mainThread=owner!=0;nativeResult=original!=0;loading=(unsigned char)load;byClass=cls;manager=sections;
  zone::g_zoneCfg.deferralEnabled=def!=0;zone::g_zoneCfg.readinessOverridesEnabled=overrides!=0;zone::g_zoneCfg.islandReadinessRuleEnabled=rule!=0;ClearCounts();
  C(hook_isContentPending(&manager,pos)==nativeResult&&nativeCalls==1);
  if(!debug){C(classifyCalls==0&&readinessCalls==0&&bridgeCalls==0&&byTrace==0);C(g_rdy.g_rdyOffSample==0&&g_rdyContract.g_rdyContractGlobal==0&&g_rdyContract.g_rdyContractPrivate==0&&g_rdyContract.g_rdyContractAdopted==0);}
  else{C(readinessCalls==1&&bridgeCalls==1);C(classifyCalls==(!original&&def&&(owner||rule)?1:0));C(g_rdyContract.g_rdyContractGlobal==(load?1:0)&&g_rdyContract.g_rdyContractPrivate==(!load&&cls==1?1:0)&&g_rdyContract.g_rdyContractAdopted==(!load&&cls==2?1:0));}
 }
 // Sampled off-owner diagnostics still run once per16 Debug calls. Off spans do not advance them.
 mainThread=false;nativeResult=false;loading=0;manager=1;debugOn=true;zone::g_zoneCfg.deferralEnabled=true;zone::g_zoneCfg.islandReadinessRuleEnabled=false;ClearCounts();
 for(int i=0;i<15;++i) C(!hook_isContentPending(&manager,pos));C(classifyCalls==0&&g_rdy.g_rdyOffSample==15);
 debugOn=false;for(int i=0;i<1000;++i) C(!hook_isContentPending(&manager,pos));C(classifyCalls==0&&g_rdy.g_rdyOffSample==15);
 debugOn=true;C(!hook_isContentPending(&manager,pos)&&classifyCalls==1&&g_rdy.g_rdyOffSample==16);
 // Original may accept opaque arguments on the fast path: no diagnostic field read is allowed.
 void* denied=VirtualAlloc(0,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);C(denied!=0);debugOn=false;g_cachedZoneMgr=denied;
 for(int owner=0;owner<2;++owner)for(int original=0;original<2;++original){mainThread=owner!=0;nativeResult=original!=0;ClearCounts();C(hook_isContentPending(denied,denied)==nativeResult&&nativeCalls==1&&classifyCalls==0&&readinessCalls==0);}
 mainThread=true;isTransitionActive=true;prioritizedThisTransition=false;nativeResult=false;ClearCounts();
 C(!hook_isContentPending(denied,denied)&&priorityCalls==1&&prioritizedThisTransition);C(!hook_isContentPending(denied,denied)&&priorityCalls==1&&nativeCalls==2);isTransitionActive=false;
 throwNative=true;ClearCounts();try{hook_isContentPending(denied,denied);C(false);}catch(int value){C(value==173&&nativeCalls==1&&readinessCalls==0&&classifyCalls==0);}throwNative=false;
 VirtualFree(denied,0,MEM_RELEASE);std::cout<<"PASS "<<checks<<" actual readiness hook controls\n";return 0;
}
