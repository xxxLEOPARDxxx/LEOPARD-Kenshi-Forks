"""Actual fallback/resolve/budget bodies at native boundaries; no game calls.
Submit setup uses the actual prepublication policy; native wrappers are verified
separately by test_path_priority_publication.py. No post-submit write assumption.
"""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/leopard-budget-tests';OUT.mkdir(parents=True,exist_ok=True)
VC=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');SDK=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
s=(ROOT/'src/pathfind/pathfind_hooks.cpp').read_text(encoding='utf-8')
def body(marker):
 a=s.index(marker);b=s.index('{',a);e=b+1;d=1
 while d:d+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
a=s.index('currentRequestIsPlayer = false;',s.index('char hook_csFindPath('));b=s.index('// Multi-call probe:',a);tag=s[a:b]
code=r"""
#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include "pathfind/path_budget_policy.h"
#include "pathfind/path_priority_publication.h"
typedef UINT_PTR uintptr_t;
static __declspec(thread) PathBudgetRequest*t_pathBudgetRequest=NULL;
static __declspec(thread) bool currentRequestIsPlayer=false;
static __declspec(thread) void*g_pathPoolLastCsFindPathReq=NULL;
static __declspec(thread) int t_waveAuth=0,squadBoostTier=0;
static __declspec(thread) bool squadBoostFromRepathTier=false;
namespace pathfind{struct Diag{volatile LONG playerRequestsInFlight;};Diag g_pathDiag={0};}
volatile LONG p12DiagSubmitBoostsState6=0,p12DiagSubmitBoostsOrder=0,fallbackInvocations=0,originalCalls=0,traceCalls=0,checks=0;
namespace planner{void PlannerWaterOnSubmit(float*){}}
void PlayerRepathTierNoteTiered(){}
const int OFF_REQ_RESULTBUF_SLOT=128,OFF_REQ_WATER_COST_MULT=0x74;
AstarCallerClass AstarResolveCallerClass(void*addr,int player){UINT_PTR a=reinterpret_cast<UINT_PTR>(addr);return AstarClassifyCaller(unsigned(a),a<0x10000000,player);}
namespace game{struct Orig{char(*orig_csFindPathFallback)(void*,unsigned,void*,unsigned,void*,float,float,char,void*);void(*orig_pathReqSubmit)(void*,void*,bool);};Orig g_hookOrig;}
void PathResultTraceCopy(void*,void*,void*,bool,bool){InterlockedIncrement(&traceCalls);}
CONTEXT;
CAPTURE
RESOLVE
BOOST
FALLBACK
void TagNext(){TAG}
void Check(bool yes,const char*what){InterlockedIncrement(&checks);if(!yes){std::printf("FAIL %s\n",what);throw 1;}}
struct Request{unsigned char bytes[256];Request(int pri=45){std::memset(bytes,0,256);*reinterpret_cast<int*>(bytes+44)=pri;}void*Result(){return bytes+128;}int Priority(){return *reinterpret_cast<int*>(bytes+44);}};
struct Outcome{int owner,open,state;void*request;bool boosted;};
static __declspec(thread) int mode=0,depth=0,nativeOwnerFlag=0;
static __declspec(thread) Outcome last;
static __declspec(thread) Request*child=NULL;
Outcome Invoke(unsigned rva){FindPathFullCtx c={};c.astarReturnAddr=reinterpret_cast<void*>(UINT_PTR(rva));ResolveFindPathRequest(c);c.astarCallerClass=AstarResolveCallerClass(c.astarReturnAddr,c.pathPoolPlayerByReq);unsigned char input[192];std::memset(input,0xa5,sizeof(input));int initialOpen=131072,initialState=590336;std::memcpy(input+156,&initialOpen,4);std::memcpy(input+160,&initialState,4);unsigned char before[192];std::memcpy(before,input,192);BoostFindPathBudget(input,c);Outcome out={c.pathPoolPlayerByReq,*reinterpret_cast<int*>(input+156),*reinterpret_cast<int*>(input+160),c.request,c.pathPoolBoosted!=0};std::memcpy(before+156,input+156,8);Check(!std::memcmp(before,input,192),"traversal/water/hierarchy bytes unchanged");return out;}
char Call(Request&r,int which,int npc=0);
char NativeFallback(void*manager,unsigned start,void*startPos,unsigned goal,void*goalPos,float radius,float cost,char npc,void*result){InterlockedIncrement(&originalCalls);Check(manager==reinterpret_cast<void*>(1)&&start==2&&startPos==reinterpret_cast<void*>(3)&&goal==4&&goalPos==reinterpret_cast<void*>(5)&&radius==6&&cost==7,"all native fallback arguments forwarded");Check(t_waveAuth==(npc?1:2),"native owner stamp current inside original");
 if(mode==1)throw std::runtime_error("expected");if(mode==2)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,NULL);if(mode==3)return 0;
 if(mode==4){Outcome gate=Invoke(ASTAR_RET_GATE_FINDPATH);Check(gate.owner==-1&&!gate.request&&!gate.boosted,"nested gate cannot borrow outer request");}
 if(mode==5&&depth==0){++depth;int saved=mode;mode=0;char r=Call(*child,0,1);mode=saved;--depth;Check(r==1&&!last.boosted&&last.owner==0,"nested NPC retains native caps");Check(t_waveAuth==(npc?1:2),"nested owner stamp restored");}
 last=Invoke(ASTAR_RET_CHARACTER_FINDPATH);if(mode==6){Outcome second=Invoke(ASTAR_RET_CHARACTER_FINDPATH);Check(second.owner==-1&&!second.boosted&&!second.request,"current request consumed once");}
 return 1;}

char Call(Request&r,int which,int npc){mode=which;return hook_csFindPathFallback(reinterpret_cast<void*>(1),2,reinterpret_cast<void*>(3),4,reinterpret_cast<void*>(5),6,7,char(npc),r.Result());}
bool CallSeh(Request&r){__try{Call(r,2);return false;}__except(GetExceptionCode()==EXCEPTION_ACCESS_VIOLATION?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}}
void Submit(Request&r,bool repath){
 // Native10/20 followed by the real pre-publication policy, not a late write.
 *reinterpret_cast<int*>(r.bytes+44)=repath?10:20;void*item=r.bytes;unsigned char queue;
 PathPriorityContext*slot=NULL;PathPriorityFrame frame;PathPriorityDecision decision;
 PathPriorityEnter(&frame,slot,r.bytes,&queue,(void*)0x3AAF60,45,repath,!repath,true);
 Check(PathPriorityApply(slot,&queue,&item,(void*)0x3AAF60,&decision)&&r.Priority()==45,"submit setup applies tier before publication");
 if(!decision.repath)InterlockedIncrement(&pathfind::g_pathDiag.playerRequestsInFlight);
 PathPriorityLeave(&frame,slot);
}
struct ThreadInput{bool npc;volatile LONG*failed;};
DWORD WINAPI Thread(void*arg){ThreadInput*t=static_cast<ThreadInput*>(arg);try{for(int i=0;i<128;++i){Request r(t->npc?10:45);currentRequestIsPlayer=t->npc;Call(r,4,t->npc?1:0);Check(last.boosted==!t->npc&&!t_pathBudgetRequest,"thread-local owner isolation");}}catch(...){InterlockedIncrement(t->failed);}return 0;}
int main(){try{game::g_hookOrig.orig_csFindPathFallback=NativeFallback;
 Request old,fresh;Submit(old,true);Submit(fresh,false);TagNext();Check(currentRequestIsPlayer,"older repath stole old diagnostic token");Call(old,0);Check(last.request==old.bytes&&last.boosted&&last.open==524288&&last.state==2361344,"older current player repath gets original expanded caps");TagNext();Check(!currentRequestIsPlayer,"fresh order has no old diagnostic token");Call(fresh,0);Check(last.request==fresh.bytes&&last.boosted,"fresh current player budget independent of stolen token");
 currentRequestIsPlayer=true;g_pathPoolLastCsFindPathReq=reinterpret_cast<void*>(1);Outcome outside=Invoke(ASTAR_RET_CHARACTER_FINDPATH);Check(!outside.boosted&&!outside.request&&outside.owner==-1,"no fallback context ignores leftover diagnostic pointers/tags");
 const unsigned other[]={ASTAR_RET_GATE_FINDPATH,ASTAR_RET_GENERATION_EDGECOST,ASTAR_RET_HAVOK_CE62B0,0x123456};for(unsigned k=0;k<4;++k){g_pathPoolLastCsFindPathReq=reinterpret_cast<void*>(1);outside=Invoke(other[k]);Check(!outside.boosted&&!outside.request,"unowned/generation/gate calls retain native sizes");}
 Call(fresh,4);Check(last.boosted&&!t_pathBudgetRequest,"gate leaves outer current request unconsumed");Call(fresh,6);Check(!t_pathBudgetRequest,"one-shot scope retired");Request npc(10);child=&npc;Call(fresh,5);Check(last.boosted&&last.request==fresh.bytes&&!t_pathBudgetRequest,"nested fallback restores exact outer player");
 Call(fresh,0,1);Check(!last.boosted,"native NPC flag vetoes tier45");Request low(10);Call(low,0,0);Check(last.boosted&&last.owner==0,"native-positive low-priority player gets budget without changing priority label");
 t_waveAuth=77;try{Call(fresh,1);Check(false,"native C++ exception forwarded");}catch(const std::runtime_error&){}Check(!t_pathBudgetRequest&&t_waveAuth==77,"C++ exception restores current request and owner stamp");Check(CallSeh(fresh)&&!t_pathBudgetRequest&&t_waveAuth==77,"native unwind also restores POD scope");
 Call(fresh,3);Check(!t_pathBudgetRequest,"cancellation/no search leaves no context");*reinterpret_cast<int*>(fresh.bytes+44)=10;Call(fresh,0,1);Check(!last.boosted&&last.request==fresh.bytes,"same address reuse cannot inherit old player identity");
 void*captured=reinterpret_cast<void*>(1);int pri=99;Check(!CapturePathBudgetRequest(NULL,&captured,&pri)&&!captured&&!pri,"null context is unavailable");Check(!CapturePathBudgetRequest(reinterpret_cast<void*>(1),&captured,&pri)&&!captured&&!pri,"underflow is unavailable");void*page=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_NOACCESS);Check(page!=NULL,"inaccessible owned fixture page");Check(!CapturePathBudgetRequest(static_cast<char*>(page)+128,&captured,&pri)&&!captured&&!pri,"unreadable current header refuses whole context");VirtualFree(page,0,MEM_RELEASE);
 mode=0;hook_csFindPathFallback(reinterpret_cast<void*>(1),2,reinterpret_cast<void*>(3),4,reinterpret_cast<void*>(5),6,7,0,NULL);Check(!last.boosted&&!last.request&&!t_pathBudgetRequest,"missing result buffer does not borrow legacy TLS");
 volatile LONG failed=0;ThreadInput args[4];HANDLE threads[4];for(unsigned i=0;i<4;++i){args[i].npc=(i&1)!=0;args[i].failed=&failed;threads[i]=CreateThread(NULL,0,Thread,args+i,0,NULL);Check(threads[i]!=NULL,"host thread created");}Check(WaitForMultipleObjects(4,threads,TRUE,30000)==WAIT_OBJECT_0&&!failed,"simultaneous current player/NPC contexts never cross threads");for(unsigned i=0;i<4;++i)CloseHandle(threads[i]);
 Check(originalCalls==fallbackInvocations,"original called exactly once per fallback including exceptions/cancel");Check(!t_pathBudgetRequest&&t_waveAuth==77,"main context remains clear after worker fixtures");std::printf("Actual path budget caller %ld checks PASS\n",checks);return 0;}catch(...){return 1;}}
"""
for name,marker in [('CONTEXT','struct FindPathFullCtx'),('CAPTURE','static bool CapturePathBudgetRequest('),('RESOLVE','static void ResolveFindPathRequest('),('BOOST','static void BoostFindPathBudget('),('FALLBACK','char hook_csFindPathFallback(')]:code=code.replace(name,body(marker))
code=code.replace('TAG',tag)
env=dict(os.environ,INCLUDE=f'{VC}/include;{SDK}/Include;{ROOT}/src',LIB=f'{VC}/lib/amd64;{SDK}/Lib/x64')
policy=(ROOT/'src/pathfind/path_budget_policy.cpp').read_text()
def run(name,cpp,pol=policy,passes=True):
 folder=OUT/name;folder.mkdir(exist_ok=True);unit=folder/'caller.cpp';unit.write_text(cpp,encoding='utf-8');p=folder/'policy.cpp';p.write_text(pol);r=subprocess.run([str(VC/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/W3',str(unit),str(p),str(ROOT/'src/pathfind/astar_cost_policy.cpp'),str(ROOT/'src/pathfind/path_priority_publication.cpp'),'/Fo'+str(folder)+'/', '/Fe'+str(folder/'test.exe')],env=env,capture_output=True,text=True);(folder/'build.log').write_text(r.stdout+r.stderr)
 if r.returncode:raise RuntimeError(r.stdout+r.stderr)
 r=subprocess.run([str(folder/'test.exe')],capture_output=True,text=True);(folder/'run.log').write_text(r.stdout+r.stderr)
 if (r.returncode==0)!=passes:raise AssertionError(name+': '+r.stdout+r.stderr)
 print(name,r.stdout.strip())
run('actual',code)
for name,old,new,target in [
 ('global-token','PathBudgetApply(searchState, c.pathBudgetEligible)','PathBudgetApply(searchState, currentRequestIsPlayer)','caller'),
 ('borrow-caller','AstarResolveCallerClass(c.astarReturnAddr, -1)','ASTAR_CALLER_CHARACTER_UNKNOWN','caller'),
 ('no-consume','current->consumed ||','false ||','policy'),
 ('no-restore','slot = frame->previous;','slot = NULL;','policy'),
 ('no-leave','PathBudgetLeave(&budgetFrame, t_pathBudgetRequest);','','caller'),
 ('npc-expanded','out->expand = !current->nativeNpc;','out->expand = true;','policy'),
 ('priority-gated-player','out->expand = !current->nativeNpc;','out->expand = current->priority >= 20 && !current->nativeNpc;','policy'),
 ('priority-label','current->priority >= 20','current->priority >= 0','policy'),
 ('caps-doubled','131072 * 4','131072 * 8','policy'),
 ('lost-auth-restore','t_waveAuth = previousAuth;','t_waveAuth = 0;','caller'),
]:
 text=code if target=='caller' else policy;assert old in text,name
 run(name,code.replace(old,new) if target=='caller' else code,policy.replace(old,new) if target=='policy' else policy,False)
print('10 compiled actual budget caller/policy mutants rejected')
