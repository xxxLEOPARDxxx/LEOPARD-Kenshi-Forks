#define NOMINMAX
#include <windows.h>
#include <stdint.h>
#include <cstring>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include "pathfind/path_budget_policy.h"
static unsigned checks=0,qpcs=0,hists=0,slow=0,priorities=0,guards=0,nativePass=0,nativeEnq=0,nativeDeq=0;
static bool debugOn=false;
static LONGLONG clockTicks=100;
#define C(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";exit(1);}}while(0)
static bool DetailedLogEnabled(){return debugOn;}
static BOOL SpyQpc(LARGE_INTEGER* p){++qpcs;p->QuadPart=(clockTicks+=10);return TRUE;}
#define QueryPerformanceCounter SpyQpc
static LONGLONG QpcToUs(LONGLONG n){return n;}
static double QpcToMs(LONGLONG n){return (double)n/1000;}
#define KLIB_MEMBER(g,b,m,o) ((uintptr_t)(b)+(o))
#define KEO_DETAIL(x) do{(void)(x);}while(0)
static void* fakeReturn=(void*)ASTAR_RET_CHARACTER_FINDPATH;
#define _ReturnAddress() fakeReturn
namespace fixes{struct Cfg{bool navMeshUpdateGuardEnabled;}g_fixesCfg={true};}
static bool isTransitionActive=false;
static volatile DWORD g_pathThreadId=0;
static char* manager=0;static char* request=0;static int passMode=0,passDepth=0;
static char NativePass(void*);static void* NativeDeq(void*);static void NativeEnq(void*,void**);
static char (*orig_contentStream)(void*)=NativePass;
static void* (*orig_dequeueWork)(void*)=NativeDeq;
static void (*orig_enqueueThreadSafe)(void*,void**)=NativeEnq;
static __int64 NativeGate(void*){return 0x123456789LL;}
static __int64 (*orig_gatesUpdateCodes)(void*)=NativeGate;
static char NavMeshUpdateGuardCall(void*p,char(*fn)(void*)){++guards;return fn(p);}
static void PathPriorityBeforeEnqueue(void*q,void**item,const void*){++priorities;if(q==manager+0xb8&&item&&*item)*(int*)((char*)*item+44)=45;}
static void GatePassBegin(void*,LONGLONG){}static void GatePassEnd(LONGLONG){}
namespace pool {
// POOL_TYPES
namespace path_pool_detail {
const size_t PATHQ_INPUT_OFFSET=0xb8,PATHQ_RESULT_OFFSET=0xf8,PATHQ_DEPTH_OFFSET=0x1c0;
static volatile LONG g_gateOutsideStreamWarned=0;
static void PPHistAdd(PPHist*,LONGLONG){++hists;}
static void PPSlowConsider(LONGLONG,LONG,LONG,LONG,float,float,float,float){++slow;}
}
// POOL_PRODUCTION
}
static void* NativeDeq(void*q){++nativeDeq;C(q==manager+0xb8);return request;}
static void NativeEnq(void*q,void**item){++nativeEnq;C(q==manager+0xb8||q==manager+0xf8);C(item&&*item==request);if(q==manager+0xb8)C(*(int*)(request+44)==45);}
static char NativePass(void*p)
{
 ++nativePass;C(p==manager);C(pool::path_pool_detail::t_inContentStreamPass==1);
 if(passMode==1)throw 71;
 if(passMode==2&&passDepth==0){LONGLONG before=pool::path_pool_detail::g_passStartTicks.QuadPart;++passDepth;C(pool::hook_contentStream(p)==7);--passDepth;C(pool::path_pool_detail::t_inContentStreamPass==1);C(pool::path_pool_detail::g_passStartTicks.QuadPart==before);}
 if(passMode==4)debugOn=true;
 if(passMode==3||passMode==4){pool::hook_dequeueWork(manager+0xb8);void*item=request;pool::hook_enqueueThreadSafe(manager+0xf8,&item);}
 return 7;
}
namespace full {
static __declspec(thread) PathBudgetRequest*t_pathBudgetRequest=0;
static __declspec(thread) void*g_pathPoolLastCsFindPathReq=0;
static __declspec(thread) bool currentRequestIsPlayer=false;
static __declspec(thread) int t_waveTag=0,t_waveConsumed=0,t_waveAuth=0;
enum {WAVE_NPC,WAVE_PLAYER,WAVE_UNK};
namespace pathfind{struct Diag{volatile LONG diagAstarAttempts,diagWaveStarted[3],diagWaveStartedByGate[3],diagWaveStartedByTag[2],diagWaveLabelDisagree;}g_pathDiag={};}
static unsigned inputs=0,probes=0,samples=0,outcomes=0,hier=0,cap=0,traces=0;
static int nested=0,findDepth=0;static bool throwFind=false,opaque=false;static void*expectedIn=0,*expectedOut=0;
static AstarCallerClass AstarResolveCallerClass(void*ret,int player){return AstarClassifyCaller((unsigned)(uintptr_t)ret,true,player);}
static void AstarHierSearch(void*,void*,void*,void*,int);
// FULL_TYPES
// FULL_HELPERS
// CONN_OWNER
static void ReadFindPathInput(void*s,FindPathFullCtx&c){++inputs;ReadInputActual(s,c);}
static void ProbeFindPathInput(void*){++probes;}
static void CaptureFindPathProbe(void*,FindPathFullCtx&c){++probes;c.probeSlot=-1;}
static void RecordFindPathSamples(const FindPathFullCtx&){++samples;}
static void RecordFindPathOutcome(void*,const FindPathFullCtx&){++outcomes;}
static void NpcCapRequesterAfter(void*,void*,AstarCallerClass,int,int,void*,int*,int*){++cap;}
const int OFF_REQ_RESULTBUF_SLOT=128;static volatile LONG fallbackInvocations=0;
namespace game{struct Orig{char(*orig_csFindPathFallback)(void*,unsigned,void*,unsigned,void*,float,float,char,void*);}g_hookOrig;}
static void PathResultTraceCopy(void*,void*,void*,bool,bool){++traces;}
// FULL_HOOK
// FALLBACK
static void AstarHierSearch(void*coll,void*in,void*out,void*ret,int player)
{
 ++hier;C(coll==(void*)3&&in==expectedIn&&out==expectedOut);C(ret==fakeReturn);
 if(throwFind)throw 91;
 if(nested&&findDepth==0){++findDepth;void*previous=fakeReturn;fakeReturn=(void*)ASTAR_RET_GATE_FINDPATH;hook_findPathFull(coll,in,out);fakeReturn=previous;--findDepth;C(t_pathBudgetRequest&&t_pathBudgetRequest->consumed);}
 if(!opaque){C(!t_waveAuth);C(player==(ret==(void*)ASTAR_RET_CHARACTER_FINDPATH&&findDepth==0?1:-1));*(int*)((char*)out+48)=12;*((char*)out+60)=1;*((char*)out+61)=0;}
}
static char NativeFallback(void*m,unsigned sf,void*sp,unsigned gf,void*gp,float radius,float cost,char npc,void*result)
{
 C(m==(void*)3&&sf==2&&sp==(void*)4&&gf==5&&gp==(void*)6&&radius==7&&cost==8&&npc==0&&result==request+128);
 hook_findPathFull(m,expectedIn,expectedOut);return 9;
}
static char Call(){return hook_csFindPathFallback((void*)3,2,(void*)4,5,(void*)6,7,8,0,request+128);}
}
namespace cost {
static uintptr_t gameBase=0;static uintptr_t NativeProfileCanonical(uintptr_t n){return n;}
namespace pathfind {struct Cfg{bool pathCostLinesEnabled;}g_pathfindCfg={true};}
enum{HOOK_FIND_PATH_FULL};static bool HookRowInstalled(int){return true;}
// COST_TYPES
// COST_PRODUCTION
}
int main()
{
 unsigned char mgr[512]={0},req[256]={0};manager=(char*)mgr;request=(char*)req;*(int*)(mgr+0x1c0)=3;
 debugOn=false;passMode=3;*(LONGLONG*)req=77;C(pool::hook_contentStream(mgr)==7);C(qpcs==0&&hists==0&&slow==0&&guards==1&&nativePass==1);C(*(LONGLONG*)req==0&&priorities==1&&nativeEnq==1);
 void*item=req;pool::hook_enqueueThreadSafe(mgr+0xb8,&item);C(priorities==2&&nativeEnq==2&&*(int*)(req+44)==45&&qpcs==0);
 debugOn=true;passMode=3;unsigned before=qpcs;C(pool::hook_contentStream(mgr)==7);C(qpcs==before+4&&hists>0&&slow==1&&*(LONGLONG*)req>0);
 // An OFF dequeue clears a prior ON stamp, so enabling before completion cannot reuse it.
 debugOn=false;pool::hook_dequeueWork(mgr+0xb8);C(*(LONGLONG*)req==0);debugOn=true;before=qpcs;pool::hook_enqueueThreadSafe(mgr+0xf8,&item);C(qpcs==before);
 // Turning ON inside a pass begun OFF may not sample its stale serve-start.
 debugOn=false;passMode=0;C(pool::hook_contentStream(mgr)==7);debugOn=true;before=qpcs;pool::hook_dequeueWork(mgr+0xb8);C(*(LONGLONG*)req==0&&qpcs==before);
 debugOn=false;passMode=4;before=qpcs;*(LONGLONG*)req=99;C(pool::hook_contentStream(mgr)==7);C(debugOn&&qpcs==before&&*(LONGLONG*)req==0);
 debugOn=false;passMode=0;fixes::g_fixesCfg.navMeshUpdateGuardEnabled=false;unsigned oldGuards=guards;C(pool::hook_contentStream(mgr)==7&&guards==oldGuards);fixes::g_fixesCfg.navMeshUpdateGuardEnabled=true;
 for(int on=0;on<2;++on){debugOn=on!=0;passMode=2;C(pool::hook_contentStream(mgr)==7);C(pool::path_pool_detail::t_inContentStreamPass==0);passMode=1;try{pool::hook_contentStream(mgr);C(false);}catch(int n){C(n==71&&pool::path_pool_detail::t_inContentStreamPass==0);}}
 void*opaqueReq=VirtualAlloc(0,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);C(opaqueReq!=0);debugOn=false;request=(char*)opaqueReq;item=opaqueReq;before=qpcs;pool::hook_enqueueThreadSafe(mgr+0xf8,&item);C(qpcs==before);request=(char*)req;VirtualFree(opaqueReq,0,MEM_RELEASE);item=req;
 unsigned char in[192]={0},out[80]={0};float goal[4]={3,4,5,0};unsigned goalKey=7;*(float**)(in+32)=goal;*(int*)(in+40)=1;*(unsigned**)(in+56)=&goalKey;*(int*)(in+64)=1;
 full::expectedIn=in;full::expectedOut=out;full::game::g_hookOrig.orig_csFindPathFallback=full::NativeFallback;*(int*)(req+44)=45;
 for(int on=0;on<2;++on){debugOn=on!=0;full::g_pathPoolLastCsFindPathReq=req;C(full::ConnGateOwner()==full::WAVE_PLAYER);*(int*)(req+44)=10;C(full::ConnGateOwner()==full::WAVE_NPC);full::g_pathPoolLastCsFindPathReq=0;C(full::ConnGateOwner()==full::WAVE_UNK);*(int*)(req+44)=45;}
 debugOn=false;before=qpcs;full::t_waveAuth=77;full::t_waveTag=1;full::nested=1;C(full::Call()==9);C(qpcs==before&&full::inputs==0&&full::probes==0&&full::samples==0&&full::outcomes==0&&full::cap==0);C(*(int*)(in+156)==524288&&*(int*)(in+160)==2361344&&full::hier==2&&!full::t_pathBudgetRequest&&full::t_waveAuth==77&&full::t_waveConsumed==1);
 C(full::pathfind::g_pathDiag.diagAstarAttempts==0);full::nested=0;debugOn=true;C(full::Call()==9);C(full::inputs==1&&full::samples==1&&full::outcomes==1&&full::cap==1&&full::pathfind::g_pathDiag.diagAstarAttempts==1);
 debugOn=false;full::throwFind=true;try{full::Call();C(false);}catch(int n){C(n==91&&!full::t_pathBudgetRequest&&full::t_waveAuth==77);}full::throwFind=false;
 // No diagnostic input/output dereference on a native-accepted opaque gate call.
 void*denied=VirtualAlloc(0,4096,MEM_COMMIT|MEM_RESERVE,PAGE_NOACCESS);C(denied!=0);full::opaque=true;full::expectedIn=full::expectedOut=denied;fakeReturn=(void*)ASTAR_RET_GATE_FINDPATH;before=qpcs;full::hook_findPathFull((void*)3,denied,denied);C(qpcs==before);VirtualFree(denied,0,MEM_RELEASE);
 // Public cost recorder itself must reject OFF before touching a diagnostic payload.
 cost::AstarCostSample s={};s.status=3;s.cause=3;s.iterations=100;s.ticks=100;debugOn=false;cost::AstarCostNote(&s,ASTAR_CALLER_CHARACTER_PLAYER);cost::AstarCostOnTransitionClosed();C(cost::g_slowCount==0&&cost::g_capCumulative[1][0][0]==0&&cost::g_transitionsCompleted==0);
 debugOn=true;cost::AstarCostNote(&s,ASTAR_CALLER_CHARACTER_PLAYER);cost::AstarCostOnTransitionClosed();C(cost::g_slowCount==1&&cost::g_capCumulative[1][0][0]==1&&cost::g_transitionsCompleted==1);
 std::cout<<"PASS "<<checks<<" actual path telemetry/guard/priority/budget/TLS controls\n";return 0;
}
