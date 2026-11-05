"""Actual production input, queue, world gates and caption bodies over owned SDK mocks.
No game process/native calls. Real Windows SRW/Interlocked; VC10 Od/O2.
"""
from pathlib import Path
import os,subprocess
R=Path(__file__).resolve().parents[2]
O=Path('D:/SteamLibrary/steamapps/common/Kenshi/_codex_work/20261105-steel-jobs/runtime-tests');O.mkdir(parents=True,exist_ok=True)
s=(R/'src/jobs/steel_jobs.cpp').read_text(encoding='utf-8-sig')
def function(marker):
 start=s.index(marker);brace=s.index('{',start);depth=1;at=brace+1
 while depth:
  if s[at]=='{':depth+=1
  elif s[at]=='}':depth-=1
  at+=1
 return s[start:at]
globals=s[s.index('enum { ACTOR_CAP'):s.index('Switches SwitchesNow()')]
names=['Switches SwitchesNow()','ActorId Identity(','hand NativeHand(','WaypointSubject Payload(','bool Decode(','bool Custom(','int Type(','bool Ours(','OrdersReceiver* Orders(','bool Player(','bool ReadRoute(','bool Snapshot(','float HookScore(','void RestoreLocations(','void ApplyPending(','uint64_t SteelJobsEpoch()','bool SteelJobsAdmitted()','void SteelJobsWorldHooksReady(','void SteelJobsWorldReset()','bool SteelJobsConsumeMove(','void TryPrepareJobs(', 'void SteelJobsMainTick(','void HookPlayerMove(','void* HookCaption(']
production='\n'.join(function(n)for n in names)
order=(R/'src/pathfind/order_hook.cpp').read_text(encoding='utf-8-sig')
a=order.index('void hook_addOrderSelected(');b=order.index('BenchNotifyPlayerOrder();',a)
order_gate=order[a:b];order_gate=order_gate[order_gate.index('{')+1:]
assert 'SteelJobsConsumeMove' in order_gate
prefix=r"""
#define NOMINMAX
#define UNICODE
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <set>
#include <new>
#include "jobs/steel_jobs_policy.h"
static int checks=0,nativeMoves=0,nativeOrders=0,resetCalls=0,captionCalls=0,translatedCalls=0,errors=0;
static bool templatesAvailable=false;static int prepareCalls=0;
#define KEO_DETAIL(x) ((void)0)
static bool captionConstructed=false;static double now=0;static int scoreCalls=0,stockChecks=0;
void C(bool b,const char*s){++checks;if(!b){printf("FAIL %s\n",s);exit(1);}}
namespace OIS{enum KeyCode{KC_LSHIFT,KC_RSHIFT};struct Keyboard{bool shift;Keyboard():shift(true){}bool isKeyDown(KeyCode){return shift;}};}
namespace Ogre{struct Vector3{float x,y,z;Vector3(float a=0,float b=0,float c=0):x(a),y(b),z(c){}};}
enum itemType{NULL_ITEM=0,CHARACTER=1,BUILDING=2,____XXX___=113};typedef int TaskType;
struct Character;
struct hand{itemType type;unsigned container,containerSerial,index,serial;
 hand(unsigned i=0,unsigned s=0,itemType t=NULL_ITEM,unsigned c=0,unsigned cs=0):type(t),container(c),containerSerial(cs),index(i),serial(s){}
 Character*getCharacter()const;bool operator<(const hand&h)const{if(type!=h.type)return type<h.type;if(container!=h.container)return container<h.container;if(containerSerial!=h.containerSerial)return containerSerial<h.containerSerial;if(index!=h.index)return index<h.index;return serial<h.serial;}};
template<class T>struct lektor{unsigned count,maxSize;T*stuff;};
struct TaskData{int key;};static TaskData ownedData[3]={{291},{292},{293}};static bool factoryPresent[3]={true,true,true};struct Tasker{TaskData*taskData;hand subject;Ogre::Vector3 location;void setLocation(const Ogre::Vector3&v){location=v;}};
struct OrdersReceiver{lektor<Tasker*>permajobs;Tasker rows[256];Tasker*ptrs[256];TaskData data;unsigned added;
 OrdersReceiver(){permajobs.count=0;permajobs.maxSize=256;permajobs.stuff=ptrs;data.key=292;added=0;}
 void addJob(TaskType t,const hand&h,const Ogre::Vector3&v,bool shift){C(t==292&&shift,"pending native addJob exact custom key/append");C(permajobs.count<256,"owned rows bounded");Tasker&r=rows[permajobs.count];r.taskData=&ownedData[1];r.subject=h;r.location=v;ptrs[permajobs.count++]=&r;++added;}};
struct Character{hand id;bool player,animal;OrdersReceiver orders;Character():player(true),animal(false){}bool isPlayerCharacter(){return player;}bool isAnimal(){return animal;}OrdersReceiver*getOrdersReciever(){return &orders;}const hand&getHandle(){return id;}};
struct AI{Character*actor;Character*getCharacter(){return actor;}};
static Character actors[130];
Character*hand::getCharacter()const{for(unsigned i=0;i<130;++i)if(!(*this<actors[i].id)&&!(actors[i].id<*this))return &actors[i];return NULL;}
template<class T>struct ogre_unordered_set{typedef std::set<T>type;};
struct PlayerInterface{ogre_unordered_set<hand>::type selectedCharacters;bool mRightDown;PlayerInterface():mRightDown(true){}};
struct Building{};
struct InputHandler{OIS::Keyboard*keyboard;bool mRight;};static OIS::Keyboard keyboard;static InputHandler input={&keyboard,true};static InputHandler*key=&input;
struct Gui{unsigned updates;Gui():updates(0){}void updateToOrdersPanel(){++updates;}};static Gui guiObject;static Gui*gui=&guiObject;
namespace keo_inventory{struct Config{bool gatherOutputJobEnabled,stockCounterJobEnabled,waypointRouteJobEnabled;};static Config g_inventoryCfg={false,false,true};}
double ElapsedSec(){return now;}void FactoryLog(const char*){++errors;}
std::string KeoTr(const char*s){C(captionConstructed,"native constructs poisoned return storage BEFORE assignment");++translatedCalls;return std::string("translated:")+s;}
void NativeMove(PlayerInterface*,const Ogre::Vector3&,Building*){++nativeMoves;}
void* NativeCaption(std::string*out,int,void*){C(!captionConstructed,"native caption constructor exactly once");new(out)std::string("native caption");captionConstructed=true;++captionCalls;return (void*)0x9876;}
namespace keo_jobs{
uint64_t SteelJobsEpoch();bool SteelJobsAdmitted();bool SteelJobsConsumeMove(void*,void*,void*,const float*,bool);
bool PrepareFactoryTasks(){++prepareCalls;return templatesAvailable;}
void ResetStockTransfer(){++resetCalls;}
void*FactoryTaskData(int type){return type>=291&&type<=293&&factoryPresent[type-291]?&ownedData[type-291]:NULL;}
bool SteelStockEligible(Character*,Tasker*){++stockChecks;return true;}
float NativeScore(Tasker*,AI*){++scoreCalls;return 42.5f;}
float(*s_score)(Tasker*,AI*)=&NativeScore;
void(*s_playerMove)(PlayerInterface*,const Ogre::Vector3&,Building*)=&NativeMove;
void*(*s_caption)(std::string*,int,void*)=&NativeCaption;
GLOBALS
PRODUCTION
void SelectedEntry(PlayerInterface*pi,int task,const float*location,bool shift){void*thisPI=pi;void*destIndoors=NULL;void*subject=NULL;ORDER_GATE
 ++nativeOrders;
}
}
using namespace keo_jobs;
static void Fresh(PlayerInterface&pi){SteelJobsWorldReset();InterlockedExchange(&s_loading,0);InterlockedExchange(&s_ready,1);InterlockedExchange(&s_worldHooks,1);InterlockedExchange(&s_conflict,0);s_installed=true;s_nextCompatibilityCheck=1000000;now=0;s_inputFrame=10;s_pressFrame=0;s_pressAccepted=false;input.mRight=true;input.keyboard=&keyboard;key=&input;keyboard.shift=true;pi.mRightDown=true;pi.selectedCharacters.clear();for(unsigned i=0;i<130;++i){actors[i].id=hand(i+1,200+i,CHARACTER,7,8);actors[i].player=true;actors[i].animal=false;actors[i].orders.permajobs.count=actors[i].orders.added=0;}pi.selectedCharacters.insert(actors[0].id);keo_inventory::g_inventoryCfg.gatherOutputJobEnabled=keo_inventory::g_inventoryCfg.stockCounterJobEnabled=false;keo_inventory::g_inventoryCfg.waypointRouteJobEnabled=true;for(unsigned i=0;i<3;++i)factoryPresent[i]=true;nativeMoves=nativeOrders=0;}
static bool Click(PlayerInterface&pi,const Ogre::Vector3&p){return SteelJobsConsumeMove(&pi,NULL,NULL,&p.x,false);}
static void CaptionCase(int type,const char*expected,bool custom){__declspec(align(16))unsigned char storage[sizeof(std::string)];memset(storage,0xDD,sizeof(storage));captionConstructed=false;int before=captionCalls,tr=translatedCalls;std::string*out=(std::string*)storage;void*ret=HookCaption(out,type,NULL);C(captionCalls==before+1&&captionConstructed,"caption native constructor preserved");C(*out==expected,"caption exact contents");C(ret==(custom?(void*)out:(void*)0x9876),"caption return convention preserved");C(translatedCalls==tr+(custom?1:0),"only custom captions replaced");out->~basic_string();captionConstructed=false;memset(storage,0xA7,sizeof(storage));}
static void DeferredCase(){
 SteelJobsWorldReset();InterlockedExchange(&s_ready,0);InterlockedExchange(&s_conflict,0);InterlockedExchange(&s_worldHooks,1);s_installed=true;s_metadataReady=true;s_templateAttempted=false;s_nextCompatibilityCheck=1000000;templatesAvailable=false;prepareCalls=0;
 TryPrepareJobs(false);C(prepareCalls==1&&!SteelJobsAdmitted()&&s_templateAttempted,"early metadata exists but missing native templates stays dormant");
 for(unsigned i=0;i<30;++i)SteelJobsMainTick(false);C(prepareCalls==1,"missing templates are not polled every frame");
 templatesAvailable=true;SteelJobsWorldReset();C(!s_templateAttempted,"world transition rearms one native preparation attempt");
 SteelJobsMainTick(true);C(prepareCalls==1,"no template preparation while world loading");
 SteelJobsMainTick(false);C(prepareCalls==2&&SteelJobsAdmitted(),"postload templates activate existing metadata exactly once");
 for(unsigned i=0;i<30;++i)SteelJobsMainTick(false);C(prepareCalls==2,"admitted jobs never rebuild native templates per frame");
 SteelJobsWorldReset();InterlockedExchange(&s_ready,0);InterlockedExchange(&s_conflict,1);TryPrepareJobs(true);C(prepareCalls==2,"competing original prevents deferred activation");
 InterlockedExchange(&s_conflict,0);InterlockedExchange(&s_worldHooks,0);TryPrepareJobs(true);C(prepareCalls==2,"missing world hooks prevent deferred activation");
 s_metadataReady=false;InterlockedExchange(&s_worldHooks,1);TryPrepareJobs(true);C(prepareCalls==2,"partial registration prevents deferred activation");
}
int main(){DeferredCase();PlayerInterface pi;Ogre::Vector3 p(50,2,-70);Fresh(pi);
 CaptionCase(291,"translated:Gather output",true);CaptionCase(292,"translated:Waypoint route",true);CaptionCase(293,"translated:Stock shop counter",true);CaptionCase(29,"native caption",false);CaptionCase(242,"native caption",false);
 // Caption IDs alone are insufficient: registration and active admission own it.
 InterlockedExchange(&s_ready,0);for(int type=291;type<=293;++type)CaptionCase(type,"native caption",false);InterlockedExchange(&s_ready,1);
 for(int type=291;type<=293;++type){factoryPresent[type-291]=false;CaptionCase(type,"native caption",false);factoryPresent[type-291]=true;}
 // Foreign metadata with the same numerical IDs must reach native exactly once,
 // even while unadmitted and with no AI (our actor checks must not run first).
 AI ai={&actors[0]};TaskData foreignData[3]={{291},{292},{293}};Tasker foreign[3],ours[3];
 for(int i=0;i<3;++i){foreign[i].taskData=&foreignData[i];ours[i].taskData=&ownedData[i];}
 InterlockedExchange(&s_ready,0);
 for(int i=0;i<3;++i){int calls=scoreCalls;C(HookScore(&foreign[i],NULL)==42.5f&&scoreCalls==calls+1,"foreign custom ID score passes through once while unadmitted");calls=scoreCalls;C(HookScore(&ours[i],&ai)==0&&scoreCalls==calls,"owned unadmitted score disabled without native call");}
 InterlockedExchange(&s_ready,1);keo_inventory::g_inventoryCfg.waypointRouteJobEnabled=false;
 for(int i=0;i<3;++i){int calls=scoreCalls;C(HookScore(&ours[i],&ai)==0&&scoreCalls==calls,"own OFF task score zero without native score");calls=scoreCalls;C(HookScore(&foreign[i],NULL)==42.5f&&scoreCalls==calls+1,"foreign custom ID score unaffected by our OFF toggles");factoryPresent[i]=false;calls=scoreCalls;C(HookScore(&ours[i],NULL)==42.5f&&scoreCalls==calls+1,"unprepared factory metadata is not owned");factoryPresent[i]=true;}
 TaskData ordinaryData={29};Tasker ordinary;ordinary.taskData=&ordinaryData;int calls=scoreCalls;C(HookScore(&ordinary,NULL)==42.5f&&scoreCalls==calls+1,"unrelated native score passes through");
 keo_inventory::g_inventoryCfg.gatherOutputJobEnabled=true;calls=scoreCalls;C(HookScore(&ours[0],&ai)==42.5f&&scoreCalls==calls+1,"enabled owned gather retains native score");keo_inventory::g_inventoryCfg.stockCounterJobEnabled=true;calls=scoreCalls;int stocks=stockChecks;C(HookScore(&ours[2],&ai)==42.5f&&scoreCalls==calls+1&&stockChecks==stocks+1,"owned stock alone invokes eligibility");
 // Route reader/restorer must ignore a foreign292 row, even malformed payload.
 RoutePoint one={1,10,20,30};WaypointSubject encodedOne={};C(EncodeWaypoint(____XXX___,one,kCoordinateLimit,encodedOne),"owned route setup");OrdersReceiver&ro=actors[0].orders;ro.addJob(292,hand(encodedOne.index,encodedOne.serial,(itemType)encodedOne.type,encodedOne.container,encodedOne.containerSerial),p,true);
 ro.ptrs[1]=&foreign[1];ro.permajobs.count=2;foreign[1].subject=hand();foreign[1].location=Ogre::Vector3(800,900,1000);ro.rows[0].location=Ogre::Vector3();RoutePoint read[MAX_ROUTE_POINTS];unsigned count=0;C(ReadRoute(&ro,read,&count)&&count==1&&read[0].serial==1,"foreign292 malformed payload excluded from our route");RestoreLocations(&actors[0]);C(ro.rows[0].location.x==10&&ro.rows[0].location.y==20&&ro.rows[0].location.z==30,"owned route location restored");C(foreign[1].location.x==800&&foreign[1].location.y==900&&foreign[1].location.z==1000,"foreign route location untouched");
 Fresh(pi);

 HookPlayerMove(&pi,p,NULL);SelectedEntry(&pi,29,&p.x,true);C(s_pendingCount==1&&!nativeMoves&&!nativeOrders,"dual entry same native press queues once before native move observers");
 pi.mRightDown=false;C(Click(pi,p)&&s_pendingCount==1,"accepted held press consumed without another row");SteelJobsMainTick(false);C(actors[0].orders.added==1&&!s_pendingCount,"main drains accepted click once");C(Click(pi,p)&&!s_pendingCount,"held after drain still consumed without duplicate");
 input.mRight=false;SteelJobsMainTick(false);C(!Click(pi,p)&&!s_pressAccepted,"release clears accepted press");input.mRight=true;pi.mRightDown=true;C(Click(pi,p)&&s_pendingCount==1,"new press after release accepted");
 Fresh(pi);C(Click(pi,p),"first quick click");SteelJobsMainTick(false);pi.mRightDown=true;C(Click(pi,p)&&s_pendingCount==1,"new native edge next frame admitted even release between ticks");
 Fresh(pi);s_pendingCount=PENDING_CAP;HookPlayerMove(&pi,p,NULL);C(nativeMoves==1&&s_pendingCount==PENDING_CAP&&!s_pressAccepted,"full queue preserves original move without latching");
 Fresh(pi);float nanValue;unsigned nanBits=0x7fc00000;memcpy(&nanValue,&nanBits,4);Ogre::Vector3 bad(nanValue,0,0);C(!Click(pi,bad)&&!s_pendingCount,"NaN point not consumed");bad.x=3000000;C(!Click(pi,bad),"out of world bound not consumed");
 Fresh(pi);pi.selectedCharacters.clear();C(!Click(pi,p),"empty selection not consumed");pi.selectedCharacters.insert(actors[0].id);actors[0].player=false;C(!Click(pi,p),"NPC selection not consumed");actors[0].player=true;actors[0].animal=true;C(!Click(pi,p),"animal selection not consumed");
 Fresh(pi);pi.selectedCharacters.clear();for(unsigned i=0;i<128;++i)pi.selectedCharacters.insert(actors[i].id);C(Click(pi,p)&&s_pending[0].count==128,"128 selected players bounded snapshot accepted");
 Fresh(pi);for(unsigned i=0;i<129;++i)pi.selectedCharacters.insert(actors[i].id);C(!Click(pi,p)&&!s_pendingCount,"129 selection refuses unchanged");
 Fresh(pi);keo_inventory::g_inventoryCfg.waypointRouteJobEnabled=false;C(!Click(pi,p),"route OFF never consumes");keo_inventory::g_inventoryCfg.waypointRouteJobEnabled=true;input.mRight=false;C(!Click(pi,p),"no native physical press never consumes");input.mRight=true;pi.mRightDown=false;C(!Click(pi,p),"held but never accepted is original move");pi.mRightDown=true;keyboard.shift=false;C(!Click(pi,p),"plain click never consumed");
 Fresh(pi);C(!SteelJobsConsumeMove(&pi,(void*)1,NULL,&p.x,true)&&!SteelJobsConsumeMove(&pi,NULL,(void*)1,&p.x,true)&&!SteelJobsConsumeMove(&pi,NULL,NULL,NULL,true),"building/subject/missingpoint preserve native");
 Fresh(pi);C(Click(pi,p),"queue snapshot actorA");pi.selectedCharacters.clear();pi.selectedCharacters.insert(actors[1].id);SteelJobsMainTick(false);C(actors[0].orders.added==1&&!actors[1].orders.added,"pending selection snapshot cannot retarget to new selection");RoutePoint decoded={};C(Decode(actors[0].orders.rows[0].subject,decoded)&&decoded.x==p.x&&decoded.y==p.y&&decoded.z==p.z,"queued payload actual policy roundtrip");
 Fresh(pi);Click(pi,p);InterlockedIncrement64(&s_epoch);SteelJobsMainTick(false);C(!actors[0].orders.added&&!s_pendingCount,"old epoch pending dropped without assignment");
 Fresh(pi);Click(pi,p);keo_inventory::g_inventoryCfg.waypointRouteJobEnabled=false;SteelJobsMainTick(false);C(!actors[0].orders.added&&!s_pendingCount,"OFF at drain drops pending");
 Fresh(pi);Click(pi,p);int resets=resetCalls;SteelJobsWorldReset();C(!s_pendingCount&&!s_pressAccepted&&resetCalls==resets+1&&!SteelJobsAdmitted(),"world reset invalidates pending/latch and transfers before new world");
 Fresh(pi);SteelJobsWorldHooksReady(false);C(!Click(pi,p)&&!SteelJobsAdmitted(),"unpaired world hooks fail closed");SteelJobsWorldHooksReady(true);InterlockedExchange(&s_conflict,1);C(!Click(pi,p),"QOL conflict no consumption");
 Fresh(pi);OrdersReceiver&o=actors[0].orders;for(unsigned i=1;i<=64;++i){RoutePoint r={i,(float)i,0,0};WaypointSubject w={};C(EncodeWaypoint(____XXX___,r,kCoordinateLimit,w),"owned64 valid route");o.addJob(292,hand(w.index,w.serial,(itemType)w.type,w.container,w.containerSerial),p,true);}unsigned added=o.added;Click(pi,p);SteelJobsMainTick(false);C(o.permajobs.count==64&&o.added==added,"64 waypoint cap never adds65th row");
 printf("PASS %d extracted production input/caption controls\n",checks);return 0;}
"""
code=prefix.replace('GLOBALS',globals).replace('PRODUCTION',production).replace('ORDER_GATE',order_gate)
V=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');W=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1')
env=dict(os.environ,INCLUDE=f'{V}/include;{W}/Include',LIB=f'{V}/lib/amd64;{W}/Lib/x64')
# Mutate extracted production only, never mocks or repository source. REDs run first.
variants=[('caption-red',code.replace('void*result=s_caption(out,type,target);','void*result=out;'), 'native constructs poisoned return storage BEFORE assignment'),('input-red',code.replace(order_gate,'\n'),'dual entry same native press queues once before native move observers'),('caption-ownership-red',code.replace('if(SteelJobsAdmitted()&&Custom(type)&&FactoryTaskData(type))','if(Custom(type))'),'caption exact contents'),('score-ownership-red',code.replace('if(!Custom(type)||!Ours(t,type))return s_score(t,ai);','if(!Custom(type))return s_score(t,ai);'),'foreign custom ID score passes through once while unadmitted'),('green',code,None)]
for variant,body,failure in variants:
 assert body!=code or variant=='green'
 for opt in ('Od','O2'):
  file=O/(variant+'-'+opt+'.cpp');file.write_text(body,encoding='utf-8');exe=O/(variant+'-'+opt+'.exe')
  cmd=[str(V/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MT','/'+opt,'/I'+str(R/'src'),str(file),str(R/'src/jobs/steel_jobs_policy.cpp'),'/Fo'+str(O)+'/', '/Fe'+str(exe)]
  q=subprocess.run(cmd,env=env,capture_output=True,text=True);(O/(variant+'-'+opt+'-build.log')).write_text(q.stdout+q.stderr,encoding='utf-8');assert q.returncode==0,q.stdout+q.stderr
  q=subprocess.run([str(exe)],capture_output=True,text=True);(O/(variant+'-'+opt+'-run.log')).write_text(q.stdout+q.stderr,encoding='utf-8');print(variant,opt,q.stdout.strip())
  if failure:assert q.returncode==1 and failure in q.stdout,q.stdout+q.stderr
  else:assert q.returncode==0,q.stdout+q.stderr
