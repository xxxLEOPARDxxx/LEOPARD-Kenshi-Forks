"""Actual order/ownership handlers with owned native seams. No game calls.
Runs current production plus compiled old-overflow-behavior negative control.
"""
from pathlib import Path
import os,subprocess,hashlib
R=Path(__file__).resolve().parents[1];O=R/'build/order-ownership-tests';O.mkdir(parents=True,exist_ok=True)
def body(file,marker):
 s=(R/file).read_text(encoding='utf-8-sig');a=s.index(marker);b=s.index('{',a);n=1;e=b+1
 while n:
  n+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
hook=body('src/pathfind/order_hook.cpp','void hook_addOrderSelected(')
detach=body('src/movement/formation_query.cpp','void FormationDetachCharacters(')
collect=body('src/movement/formation.cpp','static int CollectTravelMembers(')
click=body('src/pathfind/player_task_snap.cpp','void StorePlayerClickDest(')
island=body('src/movement/island_orders.cpp','void IslandNoteOrder(')
diag=(R/'src/pathfind/pathfind_diag.h').read_text();a=diag.index('struct TrackedPlayerDest {');b=diag.index('};',a)+2;tracked=diag[a:b]
code=r'''
#define NOMINMAX
#include <Windows.h>
#include <stdint.h>
#include <stddef.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iomanip>
#include "movement/formation_members.h"
#include "planner/plan_store.h"
#define KEO_DETAIL(x) ((void)0)
const int MAX_FORMATION_MEMBERS=30,MAX_FORMATION_MEMBERS_LIMIT=64,MAX_FORMATION_GROUPS=8,MOVESPEED_GROUPED=3,FFR_ORDER=1,FFS_SEND=1;
struct Movement{int speed;};struct Actor{uintptr_t cm;float x,z;};
struct Node{Node*next;uintptr_t padding;Actor*actor;int type;};struct PI{uintptr_t count,index,array;};
#define KLIB_MEMBER(domain,base,name,offset) ((uintptr_t)(base)+(offset))
const size_t OFF_PI_SEL_COUNT=offsetof(PI,count),OFF_PI_SEL_INDEX=offsetof(PI,index),OFF_PI_SEL_ARRAY=offsetof(PI,array),OFF_SEL_NODE_TYPE=offsetof(Node,type),OFF_SEL_NODE_HANDLE=offsetof(Node,actor),OFF_CHAR_MOVEMENT=offsetof(Actor,cm),OFF_CMOV_SPEED_MODE=0;
const size_t RVA_HANDLE_SENTINEL=0;static uintptr_t gameBase=0,emptySentinel=0;
uintptr_t NativeProfileAddress(uintptr_t,size_t){return (uintptr_t)&emptySentinel;}void*KlibSelectedCharacter(const void*p){return *(void*const*)p;}
struct FormationMember{uintptr_t character,charMovement;bool alone,dispatched,holdAtCreation,inSomethingAtCreation;};
struct FormationGroup{FormationMember members[64];int count;bool active;};FormationGroup formationGroups[8];
void FormationFollowRelease(const uintptr_t*,int,int){}void DeactivateFormationGroup(int g){formationGroups[g].active=false;}
bool FormationOwnsFollower(uintptr_t,int){return false;}bool CharacterNewlyHeld(uintptr_t,bool,bool){return false;}
DETACH
COLLECT
static bool consumed=false;static unsigned checks=0;void C(bool b,const char*t){++checks;if(!b){printf("FAIL %s\n",t);fflush(stdout);ExitProcess(1);}}
namespace keo_jobs{bool SteelJobsConsumeMove(void*,void*,void*,const float*,bool){return consumed;}}
namespace zone{struct Config{bool preloadEnabled,movementAwareEnabled;};Config g_zoneCfg={false,false};}
namespace movement{struct Config{bool groupCohesionEnabled;};Config g_movementCfg={true};}
namespace navmesh{struct Config{bool reprioFastEnabled;};Config g_navmeshCfg={false};}
static int captureN=0,islandN=0,nativeN=0,hookOrderCount=0;static bool expectedShift=false,expectedAdd=false;static void*expectedPI=NULL;static const float*expectedDest=NULL;static volatile LONG reprioOrderFires=0;bool scatterPatchApplied=true,gridCalibrated=false;
void BenchNotifyPlayerOrder(){}bool WorldToZoneGrid(float,float,int*,int*){return false;}float GetCharPosX(uintptr_t a){return ((Actor*)a)->x;}float GetCharPosZ(uintptr_t a){return ((Actor*)a)->z;}
double ElapsedSec(){return 10;}
struct IslandOrder{bool active;uintptr_t character;float destX,destY,destZ;double orderTime,k7ArrivalWaitSince;};
const int MAX_ISLAND_ORDERS=64,ISLAND_OVERTAKEN_CLICK=1;static IslandOrder g_orders[64];static int g_orderCount=0,g_k7ArrivalResumed=0;
void IslandFlagReissueOvertaken(uintptr_t,int){++islandN;}
ISLAND
TRACKED
const int MAX_TRACKED_PLAYERS=256;
namespace pathfind{struct State{TrackedPlayerDest trackedPlayers[256];int trackedPlayerCount;};static State g_pathDiag={};}
void EdgeLegsForget(int){}
CLICK
bool AddWatchedCharacter(uintptr_t,uintptr_t,int,int,int,int,bool){return false;}
static void WritePlan(uintptr_t cm,float x){planner::PlanWrite w={};w.cm=cm;w.verdict=planner::PV_DIRECT;w.finalDest[0]=x;w.legCount=1;w.legs[0].isDestination=1;w.legs[0].point[0]=x;C(planner::PlanStoreWrite(w)>=0,"bounded actual planner store");}
namespace planner{void PlannerDrop(uintptr_t actor){PlanStoreDrop(((Actor*)actor)->cm);}int PlannerNoteOrder(const uintptr_t*chars,int n,const float*,void*,bool,bool,bool){captureN=n;for(int i=0;i<n;++i)WritePlan(((Actor*)chars[i])->cm,4000);return 0;}}
void OrderOutcomeBegin(const uintptr_t*,int,float,float,double){}
void CreateFormationGroup(const float*,uintptr_t*chars,int n){FormationDetachCharacters(chars,n);}
void FormationFollowAfterOrder(const uintptr_t*,int){}void DropSelectedCharacterOrders(void*){}void CallPrioritizeNavMeshQueue(){}
void Original(void*pi,void*building,int type,void*subject,bool shift,bool add,const float*loc){++nativeN;C(pi==expectedPI&&!building&&type==29&&!subject&&loc==expectedDest&&shift==expectedShift&&add==expectedAdd,"native MOVE arguments and queue flags unchanged");}
namespace game{struct Orig{void(*orig_addOrderSelected)(void*,void*,int,void*,bool,bool,const float*);}g_hookOrig={Original};}
HOOK
static Actor actors[64];static Movement moves[64];static Node nodes[64];
static void Case(int count,bool shift,bool add,bool grouped,bool cohesion,bool capacityFull=false){
 planner::PlanStoreReset();planner::PlanStoreArm(planner::PLANNER_ON);consumed=false;memset(formationGroups,0,sizeof(formationGroups));memset(g_orders,0,sizeof(g_orders));g_orderCount=0;memset(&pathfind::g_pathDiag,0,sizeof(pathfind::g_pathDiag));movement::g_movementCfg.groupCohesionEnabled=cohesion;
 for(int i=0;i<64;++i){moves[i].speed=grouped?MOVESPEED_GROUPED:2;actors[i].cm=(uintptr_t)&moves[i];actors[i].x=actors[i].z=0;nodes[i].next=i+1<count?&nodes[i+1]:NULL;nodes[i].actor=&actors[i];nodes[i].type=1;}
 FormationGroup&old=formationGroups[0];old.active=true;old.count=2;old.members[0].character=(uintptr_t)&actors[63];old.members[0].charMovement=actors[63].cm;old.members[1].character=(uintptr_t)&actors[count-1];old.members[1].charMovement=actors[count-1].cm;
 WritePlan(actors[63].cm,9000);WritePlan(actors[count-1].cm,9000);float oldDest[3]={9000,10,8000};StorePlayerClickDest((uintptr_t)&actors[count-1],oldDest,1);StorePlayerClickDest((uintptr_t)&actors[63],oldDest,1);IslandNoteOrder((uintptr_t)&actors[count-1],oldDest);
 if(capacityFull){pathfind::g_pathDiag.trackedPlayerCount=MAX_TRACKED_PLAYERS;for(int i=2;i<MAX_TRACKED_PLAYERS;++i){pathfind::g_pathDiag.trackedPlayers[i].character=(uintptr_t)(0x10000+i);pathfind::g_pathDiag.trackedPlayers[i].active=true;}}
 Node*first=&nodes[0];PI pi={(uintptr_t)count,0,(uintptr_t)&first};float dest[3]={4000,20,5000};captureN=islandN=nativeN=0;expectedPI=&pi;expectedDest=dest;expectedShift=shift;expectedAdd=add;
 hook_addOrderSelected(&pi,NULL,29,NULL,shift,add,dest);
 int sendable[64];int n=CollectTravelMembers(old,sendable);bool stale=false;for(int i=0;i<n;++i)if(old.members[sendable[i]].character==(uintptr_t)&actors[count-1])stale=true;
 C(islandN==count&&nativeN==1,"all selected Island orders updated, native once");C(captureN==(count>30?30:count),"planner/cohort capacity remains30");C(!stale&&!old.members[1].character,"every selected new MOVE retires old formation send ownership");C(old.active&&old.members[0].character==(uintptr_t)&actors[63],"unselected old leader retains its formation");
 planner::PlanView view;int slot=planner::PlanStoreFind(actors[count-1].cm);if(count>30)C(slot<0,"excess selected actor cannot retain an old route plan");else C(slot>=0&&planner::PlanStoreRead(slot,&view)&&view.finalDest[0]==4000,"cohort actor keeps new route plan");C(planner::PlanStoreFind(actors[63].cm)>=0,"unselected old route untouched");
 TrackedPlayerDest&click=pathfind::g_pathDiag.trackedPlayers[0];C(click.character==(uintptr_t)&actors[count-1]&&click.destX==4000&&click.destY==20&&click.destZ==5000&&click.clickTime==10,"existing click destination updated beyond cohort cap");C(pathfind::g_pathDiag.trackedPlayerCount<=MAX_TRACKED_PLAYERS,"click table stays bounded");C(pathfind::g_pathDiag.trackedPlayers[1].destX==9000,"unselected diagnostic destination untouched");
 C(g_orders[0].character==(uintptr_t)&actors[count-1]&&g_orders[0].destX==4000&&g_orders[0].destZ==5000,"actual IslandNoteOrder already owns latest target");
 printf("PASS selection=%d shift=%d add=%d grouped=%d cohesion=%d full=%d\n",count,shift,add,grouped,cohesion,capacityFull);
}
int main(){Case(30,false,false,true,true);for(int shift=0;shift<2;++shift)for(int add=0;add<2;++add)Case(31,shift!=0,add!=0,true,true);Case(31,true,true,false,true);Case(31,true,false,true,false);Case(31,false,true,true,true,true);
 // A consumed custom-input event represents an already accepted different route
 // owner; this early return must not retire its pending/queued native orders.
 consumed=true;int calls=nativeN;uintptr_t prior=formationGroups[0].members[0].character;PI pi={};float d[3]={1,2,3};hook_addOrderSelected(&pi,NULL,29,NULL,true,true,d);C(nativeN==calls&&formationGroups[0].members[0].character==prior,"consumed input returns before new-MOVE ownership bookkeeping");
 printf("PASS %u actual order ownership controls\n",checks);return 0;}

'''.replace('DETACH',detach).replace('COLLECT',collect).replace('HOOK',hook).replace('\nISLAND\n','\n'+island+'\n').replace('\nTRACKED\n','\n'+tracked+'\n').replace('\nCLICK\n','\n'+click+'\n')
V=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');W=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');env=dict(os.environ,INCLUDE=f'{R}/src;{V}/include;{W}/Include',LIB=f'{V}/lib/amd64;{W}/Lib/x64')
variants=[('green',code,False)]
if '// BEGIN excess MOVE ownership' in hook:
 start=hook.index('// BEGIN excess MOVE ownership');end=hook.index('// END excess MOVE ownership',start)+len('// END excess MOVE ownership')
 old=hook[:start]+hook[end:];variants.append(('old-overflow',code.replace(hook,old,1),True))
for name,source,negative in variants:
 cpp=O/(name+'.cpp');cpp.write_text(source,encoding='utf-8')
 for opt in ('Od','O2'):
  exe=O/(name+'-'+opt+'.exe');cmd=[str(V/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,str(cpp),str(R/'src/movement/formation_members.cpp')]+[str(R/'src/planner'/file)for file in ['plan_store.cpp','plan_policy.cpp']]+['/Fo'+str(O)+'/', '/Fe'+str(exe)]
  q=subprocess.run(cmd,env=env,capture_output=True,text=True);assert not q.returncode,q.stdout+q.stderr
  q=subprocess.run([str(exe)],capture_output=True,text=True);print(name,opt,q.stdout.strip())
  if negative:assert q.returncode==1 and'every selected new MOVE retires old formation send ownership'in q.stdout,q.stdout+q.stderr
  else:assert not q.returncode,q.stdout+q.stderr
print('actual order hook SHA256',hashlib.sha256(hook.encode()).hexdigest())
