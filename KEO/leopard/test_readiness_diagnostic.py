from pathlib import Path
import os,subprocess,sys,hashlib
R=Path('D:/DEV/Kenshi/forks/KEO');O=Path('D:/SteamLibrary/steamapps/common/Kenshi/_codex_work/20261105-squin-readiness-diag');O.mkdir(parents=True,exist_ok=True)
candidate='--candidate' in sys.argv
P=O/'readiness_hook.cpp' if candidate else R/'src/zone/readiness/readiness_hook.cpp'
s=P.read_text()
legacy_contract='#include "zone/readiness/zone_readiness_contract.h"\n#include "zone/zone_ledger_core.h"\n\nZoneReadinessAnswer ZoneReadinessContractDecide(int cellClass, bool globalGameOwnedBypass)\n{\n\tif (globalGameOwnedBypass)\n\t\treturn ZONE_READY_ANSWER_ORIGINAL;\n\n\tswitch (cellClass)\n\t{\n\tcase ZONE_CLASS_PRIVATE:\n\tcase ZONE_CLASS_ADOPTED:\n\t\treturn ZONE_READY_ANSWER_ORIGINAL;\n\tdefault:   // ZONE_CLASS_NONE, or a value the enum does not have: the game\'s own to load\n\t\treturn ZONE_READY_ANSWER_TODAY_BYPASS;\n\t}\n}\n\nZoneReadinessBucket ZoneReadinessContractBucket(int cellClass, bool globalGameOwnedBypass)\n{\n\tif (globalGameOwnedBypass)\n\t\treturn ZONE_READY_BUCKET_GLOBAL;\n\n\tswitch (cellClass)\n\t{\n\tcase ZONE_CLASS_PRIVATE: return ZONE_READY_BUCKET_PRIVATE;\n\tcase ZONE_CLASS_ADOPTED: return ZONE_READY_BUCKET_ADOPTED;\n\tdefault:                 return ZONE_READY_BUCKET_NONE;\n\t}\n}\n'
legacy='bool hook_isContentPending(void* manager, void* zonePos)\n{\n\tbool onMainThread = IsMainThread();\n\n\t// Prioritize navmesh queue once per transition, at start of state 4.\n\t// By state 4, processState3 has finished registerZoneSections ->\n\t// contentStream has submitted all navmesh jobs for this transition.\n\t// Reordering now promotes the transition zone\'s jobs to the front.\n\tif (onMainThread && isTransitionActive && !prioritizedThisTransition)\n\t{\n\t\tCallPrioritizeNavMeshQueue();\n\t\tprioritizedThisTransition = true;\n\t}\n\n\tbool result = game::g_hookOrig.orig_isContentPending(manager, zonePos);\n\tint sectionCount = *(int*)(KLIB_MEMBER(4, (uintptr_t)manager, NavMesh_addList_count, OFF_RDY_SM_PENDING_SECTIONS));\n\tIslandCountReadiness(!result, !result && sectionCount > 0);\n\n#if ZONEHAND_STEP >= 2\n\t// The contract\'s two inputs, read for every call the hook is asked\n\t// about -- ahead of the answers that are already settled (the original\n\t// said ready, or deferral and the overrides switch are off). Both reads\n\t// are single aligned loads with no side effect and no lock, so no answer\n\t// on any path changes by taking them here; what changes is that the\n\t// counters report the class of every cell the hook was asked about,\n\t// instead of only the cells whose answer the contract got as far as\n\t// deciding. A cell class of zero means the hook was asked about a cell\n\t// the ledger does not know, which is most calls and takes no atomic.\n\t//\n\t// ZM+8 is read live rather than from a per-frame mirror: a save load\n\t// sets the flag partway through its own synchronous body, with no frame\n\t// boundary inside that call for a sample to observe it at, so a mirror\n\t// can read stale for the whole span of the load. Reading it once here\n\t// and using that value at the decision point below keeps a single call\'s\n\t// two uses consistent with each other.\n\tint gxContract = *(const int*)KLIB_MEMBER(5, (const int*)zonePos, iVector2_x, 0);\n\tint gyContract = *(const int*)KLIB_MEMBER(5, (const int*)zonePos, iVector2_y, 4);\n\tuintptr_t zmForContract = (uintptr_t)g_cachedZoneMgr;\n\tbool justLoadedAGame = zmForContract != 0\n\t\t&& *(unsigned char*)(KLIB_MEMBER(2, zmForContract, ZoneManager_justLoadedAGame, OFF_ZM_LOADING)) != 0;\n\tint cellClassContract = ZoneReadinessBridgeClassOf(gxContract, gyContract);\n\tswitch (ZoneReadinessContractBucket(cellClassContract, justLoadedAGame))\n\t{\n\tcase ZONE_READY_BUCKET_GLOBAL:  InterlockedIncrement(&g_rdyContract.g_rdyContractGlobal);  break;\n\tcase ZONE_READY_BUCKET_ADOPTED: InterlockedIncrement(&g_rdyContract.g_rdyContractAdopted); break;\n\tcase ZONE_READY_BUCKET_PRIVATE: InterlockedIncrement(&g_rdyContract.g_rdyContractPrivate); break;\n\tcase ZONE_READY_BUCKET_NONE:    break;   // no counter: the bypass count already covers these\n\t}\n#endif\n\n\tif (result)\n\t\treturn true;\n\n\tif (!zone::g_zoneCfg.deferralEnabled)\n\t\treturn false;\n\n\t// Classification. The caller class uses the thread and, on the main\n\t// thread, the zone manager state (never read off it, where the class is\n\t// always "off"). Main-thread calls are always classified, and so is every\n\t// call while the rule is on (its answer depends on the class). Off the\n\t// main thread with the rule off, only 1 call in 16 is classified; the other\n\t// 15 take neither lock and are not counted, and their answer (today\'s\n\t// rule, below) does not depend on the class.\n\tconst int* pos = (const int*)zonePos;\n\tint caller = RC_OFF;\n\tif (onMainThread)\n\t\tcaller = IsStatePoll4() ? RC_POLL4 : RC_MAIN;\n\tbool classify = onMainThread || zone::g_zoneCfg.islandReadinessRuleEnabled\n\t             || (InterlockedIncrement(&g_rdy.g_rdyOffSample) & RDY_OFF_SAMPLE_MASK) == 0;\n\t// The +0x1E0 split (diagnostic) runs on the main thread and in the sampled\n\t// rule-off calls; off the main thread with the rule on it is skipped, so\n\t// the rule path adds no contention against the game\'s blocking map lookups.\n\tbool splitMap = onMainThread || !zone::g_zoneCfg.islandReadinessRuleEnabled;\n\tint cls = RZ_UNKNOWN;\n\tif (classify)\n\t{\n\t\tcls = ClassifyZoneReadiness((uintptr_t)manager, pos, splitMap);\n\t\tInterlockedIncrement(&g_rdy.g_rdyCls[caller][cls]);\n\t}\n\n\t// readinessOverrides off: the original\'s answer for every caller. The queue\n\t// reprioritization above has already run.\n\tif (!zone::g_zoneCfg.readinessOverridesEnabled)\n\t\treturn false;\n\n#if ZONEHAND_STEP >= 2\n\t// The readiness contract: the per-cell class wins over islandReadinessRule\n\t// outright, for every cell -- see zone_readiness_contract.h. Below step 2\n\t// the contract does not exist at all (the #else arm below), and the rule\n\t// alone decides.\n\t//\n\t// The inputs were read at the top of this call and are used here as read:\n\t// neither depends on `classify`/`caller` (thread- and sample-dependent\n\t// diagnostics) or on which thread calls, so the same (gx, gy, classWord,\n\t// ZM+8) always gives the same answer. classWord is published once per\n\t// main-thread frame (lock-free, no lock), so a call on another thread can\n\t// see last frame\'s class until the next publish -- never a torn one.\n\t// Adopted and private cells, and every cell while ZM+8 is set, answer\n\t// false here (the original already said so, unmodified); every other cell\n\t// drops straight to today\'s sections==0 bypass below.\n\tif (ZoneReadinessContractDecide(cellClassContract, justLoadedAGame) == ZONE_READY_ANSWER_ORIGINAL)\n\t\treturn false;\n\t// Today\'s bypass otherwise: fall to the sections==0 tail below.\n\t// islandReadinessRule is not consulted at this step -- see the #else arm.\n#else\n\t// The per-caller rule: every caller but the state-4 poll (main thread in\n\t// state 4, any zone) is ready exactly when the zone\'s outdoor instance is\n\t// in the world, whatever the global section count. noSection,\n\t// outdoorMissing and notInWorld all answer 0; only RZ_UNKNOWN (the outdoor\n\t// fact was not read) falls through to today\'s rule.\n\tif (zone::g_zoneCfg.islandReadinessRuleEnabled && classify && caller != RC_POLL4 && cls != RZ_UNKNOWN)\n\t{\n\t\tbool ready = (cls == RZ_BUILDINGS_PENDING);\n\t\tInterlockedIncrement(ready ? &g_rdy.g_rdyRuleReady : &g_rdy.g_rdyRuleWait);\n\t\treturn ready;\n\t}\n#endif\n\n\tif (sectionCount > 0)\n\t\treturn false;\n\tInterlockedIncrement(&g_rdy.g_rdyBypass);\n\n\t// Sections drained: return ready regardless of navmesh state.\n\t// Macro-transitions: the original navmesh-wait deferral (95% faster state 4).\n\t// Micro-transitions (platoon activation bumps state 0->1 without\n\t// showLoadingMessage): prevents NavMeshGenerator idle stall from\n\t// causing state 4 pause. NPCs use fallback pathfinding briefly.\n\tif (!onMainThread)\n\t\treturn true;\n\n\tif (isTransitionActive)\n\t\tdeferredFrameCount++;\n\telse\n\t{\n\t\tstatic double lastMicroDeferLog = 0.0;\n\t\tdouble t = ElapsedSec();\n\t\tif (t - lastMicroDeferLog > 5.0)\n\t\t{\n\t\t\tKEO_DETAIL("Micro-transition deferral: sections=0, skipping navmesh wait");\n\t\t\tlastMicroDeferLog = t;\n\t\t}\n\t}\n\treturn true;\n}'
def body(text,marker):
 a=text.index(marker);b=text.index('{',a);n=1;e=b+1
 while n:n+=(text[e]=='{')-(text[e]=='}');e+=1
 return text[a:e]
hook=body(s,'bool hook_isContentPending(')
record=body(s,'static void ReadinessNoteBypass(')
report=body(s,'static void ReadinessBypassReportTick(')
mainreport=body(s,'void ReadinessReportTick(')
assert mainreport.index('ReadinessBypassReportTick(now)')<mainreport.index('if (!DetailedLogEnabled())')<mainreport.index('now - lastLog')
fixture=r"""
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sstream>
#include <iomanip>
#include "zone/readiness/zone_readiness_contract.h"
#include "readiness_bypass_queue.h"
#define ZONEHAND_STEP 3
#define EXPECT_NATIVE 1
#define KLIB_MEMBER(category,base,name,offset) ((uintptr_t)(base)+(offset))
#define OFF_RDY_SM_PENDING_SECTIONS 0
#define OFF_ZM_LOADING 8
enum {RC_POLL4,RC_MAIN,RC_OFF,RC_COUNT};
enum {RZ_UNKNOWN,RZ_NO_SECTION,RZ_OUTDOOR_MISSING,RZ_NOT_IN_WORLD,RZ_BUILDINGS_PENDING,RZ_COUNT};
enum {RDY_OFF_SAMPLE_MASK=15};
static unsigned checks=0;static volatile LONG allocations=0;
void* operator new(size_t n){InterlockedIncrement(&allocations);return malloc(n);}void operator delete(void*p){free(p);}
void C(bool b,const char*why){++checks;if(!b){printf("FAIL %s\n",why);fflush(stdout);ExitProcess(1);}}
namespace zone {struct Cfg{bool deferralEnabled,readinessOverridesEnabled,islandReadinessRuleEnabled;}g_zoneCfg;}
static bool nativeResult,mainThread,isTransitionActive,prioritizedThisTransition,debugOn=true;
static int calls,prioritized,deferredFrameCount,cellClass,classifyCalls,logCalls,bypassLines;
static unsigned char world[16];static void*g_cachedZoneMgr=world;static uintptr_t gameBase=0;
static int GetZoneState(void*){C(mainThread,"state is never read off Main");return world[12];}
static bool Original(void*,void*){++calls;return nativeResult;}
namespace game {struct Orig{bool(*orig_isContentPending)(void*,void*);}g_hookOrig={Original};}
static struct {volatile LONG g_rdyContractGlobal,g_rdyContractAdopted,g_rdyContractPrivate;}g_rdyContract;
static struct {volatile LONG g_rdyOffSample,g_rdyCls[RC_COUNT][RZ_COUNT],g_rdyRuleReady,g_rdyRuleWait,g_rdyBypass,g_rdyScans;volatile LONG64 g_rdyScanTicks,g_rdyScanMax,g_rdyScanSlots;}g_rdy;
static bool IsMainThread(){return mainThread;}static bool DetailedLogEnabled(){return debugOn;}
static void CallPrioritizeNavMeshQueue(){++prioritized;}static void IslandCountReadiness(bool,bool){}
static int ZoneReadinessBridgeClassOf(int,int){return cellClass;}
static bool IsStatePoll4(){return world[12]==4;}
static int ClassifyZoneReadiness(uintptr_t,const int*,bool){++classifyCalls;return RZ_OUTDOOR_MISSING;}
static double ElapsedSec(){return 0.;}
static ReadinessBypassQueue s_bypassQueue={0};
static void LogDebug(const std::string&s){C(s_bypassQueue.gate==0,"format/log after queue release");++logCalls;if(s.find("ReadinessBypass: ")==0)++bypassLines;}
#define KEO_DETAIL(message) do{if(DetailedLogEnabled())LogDebug(message);}while(0)
static LARGE_INTEGER qpcFrequency={0};static void FlushPendingTransitionLines(){}
static LONG ReadCounter(volatile LONG*p){return InterlockedCompareExchange(p,0,0);}
@RECORD@
@REPORT@
@MAINREPORT@
@LEGACY@
@HOOK@
static ReadinessBypassEvent E(int x,int y=0){ReadinessBypassEvent e={0};e.gx=x;e.gy=y;e.tid=3;e.firstQpc=e.lastQpc=x+1;e.returnRva=0x3ac822;return e;}
static void QueueCases(){
 ReadinessBypassQueue q={0};ReadinessBypassEvent e=E(0),out[128];
 q.gate=1;C(!ReadinessBypassPush(&q,e,false)&&q.dropped==0&&q.gate==1,"OFF touches no gate/counters");C(!ReadinessBypassPush(&q,e,true)&&q.dropped==1,"busy producer drops immediately");C(ReadinessBypassDrain(&q,out,16)==0&&q.gate==1,"busy drain returns without releasing owner");q.gate=0;
 for(int i=0;i<128;++i)C(ReadinessBypassPush(&q,E(i),true),"fill capacity");C(!ReadinessBypassPush(&q,E(128),true)&&q.dropped==2,"full queue drops");e=E(127);e.lastQpc=999;C(ReadinessBypassPush(&q,e,true)&&q.coalesced==1&&q.count==128,"full adjacent duplicate coalesces");
 C(ReadinessBypassDrain(&q,out,128)==16&&out[0].gx==0&&out[15].gx==15,"drain cap16 FIFO");for(int i=128;i<144;++i)C(ReadinessBypassPush(&q,E(i),true),"ring wraps after drain");int expected=16;while(q.count){unsigned n=ReadinessBypassDrain(&q,out,16);for(unsigned i=0;i<n;++i){C(out[i].gx==expected,"ring FIFO after wrap");if(expected==127)C(out[i].repeats==2&&out[i].firstQpc==128&&out[i].lastQpc==999,"coalescing keeps first/last/repeat");++expected;}}C(expected==144&&q.gate==0,"empty drain releases gate");
 ReadinessBypassPush(&q,E(9),true);ReadinessBypassDiscard(&q);C(q.count==0&&q.discarded==1,"OFF lifecycle discards pending records");ReadinessBypassPush(&q,E(10),true);C(ReadinessBypassDrain(&q,out,16)==1&&out[0].gx==10,"re-enable contains only fresh event");
}
static ReadinessBypassQueue concurrent={0};static volatile LONG accepted=0;
static DWORD WINAPI Producer(void*p){int id=(int)(uintptr_t)p;for(int i=0;i<2000;++i){ReadinessBypassEvent e=E(i,id);e.tid=id+1;e.contractClass=i^id;if(ReadinessBypassPush(&concurrent,e,true))InterlockedIncrement(&accepted);}return 0;}
static void ConcurrentCases(){HANDLE threads[4];static bool seen[4][2000]={0};for(int i=0;i<4;++i){threads[i]=CreateThread(NULL,0,Producer,(void*)(uintptr_t)i,0,NULL);C(threads[i]!=NULL,"create bounded producer");}unsigned received=0;bool done=false;do{ReadinessBypassEvent rows[16];unsigned n=ReadinessBypassDrain(&concurrent,rows,16);for(unsigned i=0;i<n;++i){ReadinessBypassEvent&e=rows[i];C(e.gx>=0&&e.gx<2000&&e.gy>=0&&e.gy<4&&e.tid==(DWORD)e.gy+1&&e.contractClass==(e.gx^e.gy)&&e.repeats==1,"concurrent POD not torn");C(!seen[e.gy][e.gx],"concurrent no duplicate delivery");seen[e.gy][e.gx]=true;++received;}done=WaitForMultipleObjects(4,threads,TRUE,0)==WAIT_OBJECT_0;SwitchToThread();}while(!done||concurrent.count);C(received==(unsigned)accepted&&accepted+concurrent.dropped==8000,"every concurrent attempt delivered or counted dropped");for(int i=0;i<4;++i)CloseHandle(threads[i]);}
static void HookCases(){int pos[2]={5,8};unsigned cases=0;ReadinessBypassEvent rows[16];int classes[]={0,1,2,3,-1,99,(-2147483647-1),2147483647};
 for(int phase=0;phase<2;++phase)for(int debug=0;debug<2;++debug)for(int thread=0;thread<2;++thread)for(int original=0;original<2;++original)for(int sections=0;sections<3;++sections)for(int ci=0;ci<8;++ci)for(int loading=0;loading<2;++loading)for(int def=0;def<2;++def)for(int overrides=0;overrides<2;++overrides)for(int rule=0;rule<2;++rule){
  int cls=classes[ci];mainThread=thread!=0;debugOn=debug!=0;nativeResult=original!=0;cellClass=cls;world[8]=(unsigned char)loading;world[12]=phase?4:0;
  zone::g_zoneCfg.deferralEnabled=def!=0;zone::g_zoneCfg.readinessOverridesEnabled=overrides!=0;zone::g_zoneCfg.islandReadinessRuleEnabled=rule!=0;isTransitionActive=false;
  calls=classifyCalls=0;memset(&g_rdy,0,sizeof(g_rdy));int manager=sections;bool before=baseline_hook(&manager,pos);int baselineClassify=classifyCalls;
  ReadinessBypassDiscard(&s_bypassQueue);calls=classifyCalls=0;memset(&g_rdy,0,sizeof(g_rdy));memset(&g_rdyContract,0,sizeof(g_rdyContract));LONG priorAlloc=allocations;bool result=hook_isContentPending(&manager,pos);
  C(before==result,"actual hook result parity with original source");C(calls==1,"exactly one native original");C(classifyCalls==baselineClassify,"no extra Classify calls");C(allocations==priorAlloc,"hook diagnostic performs no C++ allocation");
  bool expected=EXPECT_NATIVE ? original!=0 : original||(def&&overrides&&!loading&&cls!=1&&cls!=2&&sections==0);C(result==expected,"current contract preserves native readiness");C(g_rdy.g_rdyBypass==(!original&&result?1:0),"bypass counter follows actual answer");C(g_rdyContract.g_rdyContractGlobal==(loading?1:0)&&g_rdyContract.g_rdyContractPrivate==(!loading&&cls==1?1:0)&&g_rdyContract.g_rdyContractAdopted==(!loading&&cls==2?1:0),"classification buckets preserved even when native/off settles answer");unsigned n=ReadinessBypassDrain(&s_bypassQueue,rows,16);bool trace=debug&&!original&&result;
  C(n==(trace?1u:0u),"ONLY Debug nativefalse to KEOtrue recorded");if(trace){ReadinessBypassEvent&e=rows[0];C(e.gx==5&&e.gy==8&&e.contractClass==cls&&!e.justLoaded&&e.sections==0&&e.onMain==thread,"existing hook fields preserved");C(e.mainState==(thread?world[12]:-1)&&e.tid==GetCurrentThreadId()&&e.firstQpc>0&&e.firstQpc==e.lastQpc,"timestamp thread and Main-only state");C(e.readyClass==((thread||rule)?RZ_OUTDOOR_MISSING:-1),"unclassified sample explicitly minus1");}++cases;
 }
 printf("PASS %u actual hook parity cases\n",cases);
 mainThread=true;nativeResult=false;isTransitionActive=true;prioritizedThisTransition=false;prioritized=0;zone::g_zoneCfg.deferralEnabled=false;zone::g_zoneCfg.readinessOverridesEnabled=false;int manager=0;calls=0;
 C(!hook_isContentPending(&manager,pos)&&prioritized==1&&calls==1,"native false with feature flags OFF still reprioritizes once");hook_isContentPending(&manager,pos);C(prioritized==1&&calls==2,"repeat query does not reprioritize twice");mainThread=false;prioritizedThisTransition=false;hook_isContentPending(&manager,pos);C(prioritized==1&&calls==3,"off Main original once and no queue reprioritization");isTransitionActive=false;
 // Main drain runs before the aggregate30s gate, is bounded and releases before format.
 debugOn=true;mainThread=true;ReadinessBypassDiscard(&s_bypassQueue);logCalls=bypassLines=0;for(int i=0;i<20;++i)ReadinessBypassPush(&s_bypassQueue,E(i),true);ReadinessReportTick(0.01);C(bypassLines==16&&s_bypassQueue.count==4,"main drain precedes aggregate interval and caps16");ReadinessReportTick(0.1);C(bypassLines==16,"main drain respects0.5sec");ReadinessReportTick(0.51);C(bypassLines==20&&s_bypassQueue.count==0,"next bounded drain");ReadinessBypassPush(&s_bypassQueue,E(9),true);debugOn=false;int oldLogs=logCalls;ReadinessReportTick(0.6);C(!s_bypassQueue.count&&logCalls==oldLogs,"Debug OFF clears pending without logging");debugOn=true;ReadinessBypassPush(&s_bypassQueue,E(10),true);ReadinessReportTick(0.61);C(bypassLines==21,"re-enable drains fresh event immediately");
}
int main(){QueueCases();ConcurrentCases();HookCases();printf("PASS %u diagnostic controls\n",checks);return 0;}
"""
fixture=fixture.replace('@RECORD@',record).replace('@REPORT@',report).replace('@MAINREPORT@',mainreport).replace('@LEGACY@',legacy.replace('hook_isContentPending(','baseline_hook(',1)).replace('@HOOK@',hook)
V=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');S=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
headerDir=O if candidate else R/'src/zone/readiness'
env=dict(os.environ,INCLUDE=f'{V}/include;{S}/Include;{R}/src;{headerDir}',LIB=f'{V}/lib/amd64;{S}/Lib/x64')
fixture=fixture.replace('#include "readiness_bypass_queue.h"','#include "'+str(headerDir/'readiness_bypass_queue.h')+'"')
legacy_fixture=fixture.replace('#define EXPECT_NATIVE 1','#define EXPECT_NATIVE 0')
removed=legacy_fixture.replace('ReadinessNoteBypass((uintptr_t)_ReturnAddress(),pos,onMainThread,caller,classify ? cls : -1,','ReadinessNoteBypass_DISABLED((uintptr_t)_ReturnAddress(),pos,onMainThread,caller,classify ? cls : -1,',1)
removed=removed.replace('static void ReadinessNoteBypass(', 'static void ReadinessNoteBypass_DISABLED(uintptr_t,const int*,bool,int,int,int,bool,int) {}\nstatic void ReadinessNoteBypass(',1)
# Preserve explicit legacy diagnostic capture coverage after current policy retires bypass.
legacy_cpp=O/'legacy-readiness-contract.cpp';legacy_cpp.write_text(legacy_contract)
variants=[('actual',fixture,False,R/'src/zone/readiness/zone_readiness_contract.cpp',''),('legacy-diagnostic',legacy_fixture,False,legacy_cpp,''),('old-policy-refused',fixture,True,legacy_cpp,'current contract preserves native readiness'),('record-removed',removed,True,legacy_cpp,'ONLY Debug nativefalse to KEOtrue recorded')]
if '--expect-current-red' in sys.argv:variants=[('actual-old-policy-red',fixture,True,R/'src/zone/readiness/zone_readiness_contract.cpp','current contract preserves native readiness')]
outputs=[]
for name,text,negative,policy,reason in variants:
 for opt in ['Od','O2']:
  cpp=O/(name+'-'+opt+'.cpp');exe=O/(name+'-'+opt+'.exe');cpp.write_text(text)
  q=subprocess.run([str(V/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,str(cpp),str(policy),'/Fo'+str(O)+'/', '/Fe'+str(exe)],env=env,cwd=O,capture_output=True,text=True);assert not q.returncode,q.stdout+q.stderr
  q=subprocess.run([str(exe)],capture_output=True,text=True);out=name+' '+opt+' '+q.stdout.strip();print(out);outputs.append(out)
  if negative:assert q.returncode==1 and reason in q.stdout,q.stdout+q.stderr
  else:assert not q.returncode,q.stdout+q.stderr
(O/'diagnostic-tests.txt').write_text(('CANDIDATE ONLY\n'if candidate else'ACTUAL PRODUCTION\n')+'\n'.join(outputs)+'\nsourceSHA256 '+hashlib.sha256(s.encode()).hexdigest()+'\n')
