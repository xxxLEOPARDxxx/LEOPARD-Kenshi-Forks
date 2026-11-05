// Steel/KenshiQOL jobs, adapted to KEO's single input owner and bounded state.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "jobs/steel_jobs.h"
#include "jobs/steel_jobs_policy.h"
#include "jobs/steel_jobs_factory.h"
#include "jobs/steel_jobs_stock.h"
#include "jobs/steel_jobs_transfer.h"
#include "inventory/inventory_config.h"
#include "game/game.h"
#include "game/klib_bindings.h"
#include "base/log_admission.h"
#include "base/keo_tr.h"
#include "base/klib_include.h"
#include <core/Functions.h>
#include <kenshi/Tasker.h>
#include <kenshi/Character.h>
#include <kenshi/CharBody.h>
#include <kenshi/AI/AI.h>
#include <kenshi/AI/AITaskSystem.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/InputHandler.h>
#include <kenshi/gui/TitleScreen.h>
#include <kenshi/gui/OrdersPanel.h>
#include <kenshi/gui/ForgottenGUI.h>
#include <kenshi/Building/Building.h>
#include <kenshi/globals.h>
#include "base/klib_include_end.h"
#include <ois/OISKeyboard.h>
#include <string.h>
#include <stdio.h>

namespace keo_jobs {
namespace {
enum { ACTOR_CAP=512, PENDING_CAP=16, SELECTED_CAP=128, JOB_CAP=256 };
const float kCoordinateLimit=2000000.0f;
SRWLOCK s_lock=SRWLOCK_INIT;
__declspec(align(8)) volatile LONG64 s_epoch=1;
volatile LONG s_ready=0,s_loading=0,s_conflict=0,s_worldHooks=0;
bool s_installed=false;
bool s_metadataReady=false,s_templateAttempted=false;
double s_nextCompatibilityCheck=0;
unsigned long long s_clock=0;
struct RouteSlot {
 bool used; unsigned long long usedAt; RouteState state;
 RoutePoint points[MAX_ROUTE_POINTS]; unsigned count;
};
RouteSlot s_routes[ACTOR_CAP]={};
struct Pending {uint64_t epoch; RoutePoint point; ActorId actors[SELECTED_CAP];unsigned count;};
Pending s_pending[PENDING_CAP]={};unsigned s_pendingHead=0,s_pendingCount=0;
bool s_pressAccepted=false;unsigned long long s_inputFrame=0,s_pressFrame=0;
Switches SwitchesNow(){Switches s={keo_inventory::g_inventoryCfg.gatherOutputJobEnabled,
 keo_inventory::g_inventoryCfg.stockCounterJobEnabled,keo_inventory::g_inventoryCfg.waypointRouteJobEnabled};return s;}
ActorId Identity(const hand&h){ActorId a={{(uint32_t)h.type,h.container,h.containerSerial,h.index,h.serial}};return a;}
hand NativeHand(const ActorId&a){return hand(a.word[3],a.word[4],(itemType)a.word[0],a.word[1],a.word[2]);}
WaypointSubject Payload(const hand&h){WaypointSubject p={(uint32_t)h.type,h.container,h.containerSerial,h.index,h.serial};return p;}
bool Decode(const hand&h,RoutePoint&p){return DecodeWaypoint(Payload(h),(uint32_t)____XXX___,kCoordinateLimit,p);}
bool Custom(int type){return type>=GATHER_OUTPUT&&type<=STOCK_COUNTER;}
int Type(const Tasker*t){return t&&t->taskData?(int)t->taskData->key:-1;}
bool Ours(const Tasker*t,int type){void*data=FactoryTaskData(type);return data&&t&&t->taskData==data;}
OrdersReceiver* Orders(Character*c){return c?c->getOrdersReciever():NULL;}
bool Player(Character*c){return c&&c->isPlayerCharacter()&&!c->isAnimal();}
bool ReadRoute(OrdersReceiver*orders,RoutePoint*points,unsigned*count){
 *count=0;if(!orders)return false;const lektor<Tasker*>&rows=orders->permajobs;
 if(rows.count>JOB_CAP||rows.count>rows.maxSize||(rows.count&&!rows.stuff))return false;
 for(unsigned i=0;i<rows.count;++i){const Tasker*t=rows.stuff[i];if(Type(t)!=WAYPOINT_ROUTE||!Ours(t,WAYPOINT_ROUTE))continue;
  if(*count==MAX_ROUTE_POINTS||!Decode(t->subject,points[*count]))return false;
  for(unsigned j=0;j<*count;++j)if(points[j].serial==points[*count].serial)return false;
  ++*count;
 }return true;
}
bool Snapshot(Character*c,RoutePoint*points,RouteView*view,RouteState*state){
 unsigned count=0;if(!Player(c)||!ReadRoute(Orders(c),points,&count))return false;
 const ActorId actor=Identity(c->getHandle());const uint64_t epoch=SteelJobsEpoch();
 AcquireSRWLockExclusive(&s_lock);
 unsigned slot=ACTOR_CAP,oldest=0;
 for(unsigned i=0;i<ACTOR_CAP;++i){if(s_routes[i].used&&s_routes[i].state.epoch==epoch&&SameActor(s_routes[i].state.actor,actor)){slot=i;break;}
  if(!s_routes[i].used)oldest=i;else if(s_routes[oldest].used&&s_routes[i].usedAt<s_routes[oldest].usedAt)oldest=i;
 }
 if(slot==ACTOR_CAP){slot=oldest;memset(&s_routes[slot],0,sizeof(RouteSlot));}
 RouteSlot&r=s_routes[slot];bool changed=!r.used||r.count!=count||memcmp(r.points,points,count*sizeof(RoutePoint))!=0;
 if(changed){++r.state.revision;if(!r.state.revision)++r.state.revision;r.count=count;memcpy(r.points,points,count*sizeof(RoutePoint));}
 r.used=true;r.usedAt=++s_clock;view->points=points;view->count=count;view->revision=r.state.revision;
 const bool ok=SyncRoute(r.state,epoch,actor,*view);*state=r.state;
 ReleaseSRWLockExclusive(&s_lock);return ok;
}
void Commit(Character*c,const JobToken&token,uint32_t next){
 RoutePoint points[MAX_ROUTE_POINTS];RouteView view={};RouteState state={};
 if(!SteelJobsAdmitted()||!SwitchesNow().waypoints||!Snapshot(c,points,&view,&state))return;
 AcquireSRWLockExclusive(&s_lock);
 for(unsigned i=0;i<ACTOR_CAP;++i)if(s_routes[i].used&&s_routes[i].state.epoch==token.epoch&&SameActor(s_routes[i].state.actor,token.actor)){
  CommitWaypoint(s_routes[i].state,token,view,next);break;
 }ReleaseSRWLockExclusive(&s_lock);
}
struct BodyContext {Tasker*task;CharBody*body;Character*actor;AITaskSytem*tasks;};
bool ValidBody(void*opaque,const JobToken&t){
 BodyContext&c=*(BodyContext*)opaque;
 if(!SteelJobsAdmitted()||SteelJobsEpoch()!=t.epoch||!TaskEnabled(t.type,SwitchesNow())
  ||!c.body||c.body->currentAction!=c.task||c.body->character!=c.actor||!c.body->ai||c.body->ai->getTaskSystem()!=c.tasks)return false;
 if(!Player(c.actor)||!SameActor(Identity(c.actor->getHandle()),t.actor)||!c.tasks||!c.tasks->isJobsEnabled()||Type(c.task)!=t.type)return false;
 if(t.type==WAYPOINT_ROUTE){RoutePoint p;if(!Decode(c.task->subject,p)||p.serial!=t.serial)return false;
  RoutePoint points[MAX_ROUTE_POINTS];RouteView view={};RouteState state={};
  return Snapshot(c.actor,points,&view,&state)&&state.revision==t.revision&&state.currentSerial==t.serial;
 }return true;
}
bool Completed(void*opaque){BodyContext&c=*(BodyContext*)opaque;return c.tasks&&c.tasks->_taskCompletedFlag;}
void NativeBody(void*opaque){BodyContext&c=*(BodyContext*)opaque;void(*fn)(void*,void*)=FactoryNativeBody(Type(c.task));if(fn)fn(c.task,c.body);}
void RunNativeBody(void*task,void*runner){
 if(!SteelJobsAdmitted())return;
 Tasker*t=(Tasker*)task;CharBody*b=(CharBody*)runner;Character*c=b?b->character:NULL;
 if(!t||!b||!Player(c))return;
 if(!TaskEnabled(Type(t),SwitchesNow())){if(b->currentAction==t)b->notifyBodyTaskComplete();return;}
 BodyContext ctx={t,b,c,b->ai?b->ai->getTaskSystem():NULL};
 JobToken token={SteelJobsEpoch(),Identity(c->getHandle()),Type(t),0,0};
 RoutePoint points[MAX_ROUTE_POINTS];RouteView view={};RouteState state={};
 if(token.type==WAYPOINT_ROUTE){RoutePoint p;if(!Decode(t->subject,p)||!Snapshot(c,points,&view,&state))return;
  token.serial=p.serial;token.revision=view.revision;t->setLocation(Ogre::Vector3(p.x,p.y,p.z));
 }
 BodyOps ops={&ctx,ValidBody,Completed,NativeBody};
 BodyResult result=RunBody(SwitchesNow(),token,token.type==WAYPOINT_ROUTE?&state:NULL,view,ops);
 if(result.status==BODY_ADVANCE&&ValidBody(&ctx,token))Commit(c,token,result.nextSerial);
}
float(*s_score)(Tasker*,AI*)=NULL;
float HookScore(Tasker*t,AI*ai){
 const int type=Type(t);if(!Custom(type)||!Ours(t,type))return s_score(t,ai);
 if(!SteelJobsAdmitted()||!TaskEnabled(type,SwitchesNow()))return 0;
 Character*c=ai?ai->getCharacter():NULL;if(!Player(c))return 0;
 bool counter=type!=STOCK_COUNTER||SteelStockEligible(c,t);
 RoutePoint points[MAX_ROUTE_POINTS];RouteView view={};RouteState state={};
 JobToken token={SteelJobsEpoch(),Identity(c->getHandle()),type,0,0};
 if(type==WAYPOINT_ROUTE){RoutePoint p;if(!Decode(t->subject,p)||!Snapshot(c,points,&view,&state))return 0;
  token.serial=p.serial;token.revision=view.revision;
  if(state.currentSerial!=p.serial)return 0;t->setLocation(Ogre::Vector3(p.x,p.y,p.z));
 }
 if(!counter)return 0;
 return FilterScore(type,s_score(t,ai),SwitchesNow(),token,type==WAYPOINT_ROUTE?&state:NULL,counter);
}
void RestoreLocations(Character*c){OrdersReceiver*o=Orders(c);if(!o||o->permajobs.count>JOB_CAP||o->permajobs.count>o->permajobs.maxSize||(o->permajobs.count&&!o->permajobs.stuff))return;
 for(unsigned i=0;i<o->permajobs.count;++i){Tasker*t=o->permajobs.stuff[i];RoutePoint p;
  if(Type(t)==WAYPOINT_ROUTE&&Ours(t,WAYPOINT_ROUTE)&&Decode(t->subject,p))t->setLocation(Ogre::Vector3(p.x,p.y,p.z));
 }}
void(*s_load)(Character*,GameSaveState*)=NULL;
void HookLoad(Character*c,GameSaveState*state){s_load(c,state);RestoreLocations(c);}
void(*s_title)(TitleScreen*,bool)=NULL;
void FactoryLog(const char*msg){LogError(std::string("Steel jobs: ")+msg);}
void TryPrepareJobs(bool reportFailure){
 if(!s_metadataReady||s_templateAttempted||InterlockedCompareExchange(&s_ready,0,0)
  ||InterlockedCompareExchange(&s_conflict,0,0)||!InterlockedCompareExchange(&s_worldHooks,0,0))return;
 // Native task metadata must exist before saved orders are deserialised. The
 // vanilla template records themselves may be created later than TitleScreen.
 // Attempt once here and once after an actual world-load transition, not per frame.
 s_templateAttempted=true;
 if(PrepareFactoryTasks()){
  InterlockedExchange(&s_ready,1);
  KEO_DETAIL("Steel jobs: registered three native task types; independent live switches ready");
 }else if(reportFailure)FactoryLog("native task templates unavailable after loading; custom assignments disabled");
 else KEO_DETAIL("Steel jobs: metadata registered; waiting for native world task templates");
}
void HookTitle(TitleScreen*self,bool on){s_title(self,on);if(!on||!s_installed||!InterlockedCompareExchange(&s_worldHooks,0,0)||GetModuleHandleW(L"KenshiQOL.dll"))return;
 if(RegisterFactoryTask(GATHER_OUTPUT,KeoTr("Gather output"))&&RegisterFactoryTask(WAYPOINT_ROUTE,KeoTr("Waypoint route"))
  &&RegisterFactoryTask(STOCK_COUNTER,KeoTr("Stock shop counter"))&&InstallFactory()){
  s_metadataReady=true;s_templateAttempted=false;TryPrepareJobs(false);
 }else FactoryLog("registration refused; custom assignments disabled");
}
void*(*s_caption)(std::string*,int,void*)=NULL;
void* HookCaption(std::string*out,int type,void*target){
 // Hidden std::string return storage is unconstructed at entry. Native must
 // construct it before any assignment (including reused/dead stack storage).
 void*result=s_caption(out,type,target);
 if(SteelJobsAdmitted()&&Custom(type)&&FactoryTaskData(type)){*out=KeoTr(type==GATHER_OUTPUT?"Gather output":type==WAYPOINT_ROUTE?"Waypoint route":"Stock shop counter");return out;}
 return result;
}
void(*s_context)(void*,lektor<int>*,std::string*,char)=NULL;
void HookContext(void*self,lektor<int>*orders,std::string*name,char offset){
 if(SteelJobsAdmitted()&&self&&orders&&orders->count<orders->maxSize&&orders->maxSize<=512&&orders->stuff){
  const hand&target=*(const hand*)((const char*)self+0xA8);Building*b=target.type==BUILDING?target.getBuilding():NULL;
  int add=0;if(b&&SwitchesNow().stock&&SteelOwnedCounter(b))add=STOCK_COUNTER;
  else if(b&&SwitchesNow().gather&&SteelGatherTarget(b))add=GATHER_OUTPUT;
  if(add){bool exists=false;for(unsigned i=0;i<orders->count;++i)if(orders->stuff[i]==add)exists=true;
   if(!exists)orders->stuff[orders->count++]=add;
  }
 }s_context(self,orders,name,offset);
}
void(*s_playerMove)(PlayerInterface*,const Ogre::Vector3&,Building*)=NULL;
void HookPlayerMove(PlayerInterface*pi,const Ogre::Vector3&v,Building*b){if(SteelJobsConsumeMove(pi,b,NULL,&v.x,false))return;s_playerMove(pi,v,b);}
void(*s_orderText)(OrderData*)=NULL;
void HookOrderText(OrderData*data){s_orderText(data);if(!data||Type(data->task)!=WAYPOINT_ROUTE)return;
 RoutePoint point;if(!Decode(data->task->subject,point))return;
 const std::string name=KeoTr("Waypoint route");size_t at=data->text.find(name);if(at==std::string::npos)return;
 char suffix[96];sprintf_s(suffix," (%.0f, %.0f)",point.x,point.z);
 data->text.insert(at+name.size(),suffix);
}
template<class Fn,class Hook> bool InstallTyped(Fn fn,Hook hook,Hook*original){
 const uintptr_t target=(uintptr_t)KlibRealAddress(fn);
 return target>=gameBase&&target<gameBase+0x1800000&&KenshiLib::AddHook((void*)target,hook,original)==KenshiLib::SUCCESS;
}
void ApplyPending(const Pending&p){
 if(!SteelJobsAdmitted()||!SwitchesNow().waypoints||p.epoch!=SteelJobsEpoch())return;
 for(unsigned i=0;i<p.count;++i){hand h=NativeHand(p.actors[i]);Character*c=h.getCharacter();if(!Player(c))continue;
  OrdersReceiver*o=Orders(c);RoutePoint points[MAX_ROUTE_POINTS];unsigned n=0;
  if(!ReadRoute(o,points,&n)||n==MAX_ROUTE_POINTS)continue;
  RoutePoint point=p.point;point.serial=1;
  for(unsigned j=0;j<n;++j)if(points[j].serial>=point.serial)point.serial=points[j].serial+1;
  if(!point.serial)continue;WaypointSubject encoded={};if(!EncodeWaypoint((uint32_t)____XXX___,point,kCoordinateLimit,encoded))continue;
  hand target(encoded.index,encoded.serial,(itemType)encoded.type,encoded.container,encoded.containerSerial);
  o->addJob((TaskType)WAYPOINT_ROUTE,target,Ogre::Vector3(point.x,point.y,point.z),true);
 }
 if(gui)gui->updateToOrdersPanel();
}
}
uint64_t SteelJobsEpoch(){return (uint64_t)InterlockedCompareExchange64(&s_epoch,0,0);}
bool SteelJobsAdmitted(){return InterlockedCompareExchange(&s_ready,0,0)&&InterlockedCompareExchange(&s_worldHooks,0,0)&&!InterlockedCompareExchange(&s_loading,0,0)&&!InterlockedCompareExchange(&s_conflict,0,0);}
void SteelJobsWorldHooksReady(bool ready){InterlockedExchange(&s_worldHooks,ready?1:0);}
void SteelJobsWorldReset(){InterlockedIncrement64(&s_epoch);InterlockedExchange(&s_loading,1);
 s_templateAttempted=false;
 AcquireSRWLockExclusive(&s_lock);memset(s_routes,0,sizeof(s_routes));s_pendingHead=s_pendingCount=0;s_pressAccepted=false;ReleaseSRWLockExclusive(&s_lock);
 ResetStockTransfer();
}
bool SteelJobsConsumeMove(void*player,void*building,void*subject,const float*location,bool shift){
 if(!SteelJobsAdmitted()||!SwitchesNow().waypoints||building||subject||!location||!player)return false;
 PlayerInterface*pi=(PlayerInterface*)player;
 if(!key||!key->mRight){s_pressAccepted=false;return false;}
 OIS::Keyboard*keyboard=key?key->keyboard:NULL;
 if(!shift&&(!keyboard||(!keyboard->isKeyDown(OIS::KC_LSHIFT)&&!keyboard->isKeyDown(OIS::KC_RSHIFT))))return false;
 // Use the game's actual button edge, not an OS key or timer guessed to mean
 // a click. Both entry points share one accepted-press latch.
 if(s_pressAccepted){
  if(pi->mRightDown&&s_pressFrame!=s_inputFrame)s_pressAccepted=false;
  else return true;
 }
 if(!pi->mRightDown)return false;
 Pending p={};p.epoch=SteelJobsEpoch();p.point.serial=1;p.point.x=location[0];p.point.y=location[1];p.point.z=location[2];
 WaypointSubject proof={};if(!EncodeWaypoint((uint32_t)____XXX___,p.point,kCoordinateLimit,proof))return false;
 if(pi->selectedCharacters.size()>SELECTED_CAP)return false;
 for(ogre_unordered_set<hand>::type::const_iterator it=pi->selectedCharacters.begin();it!=pi->selectedCharacters.end();++it)
  if(it->type==CHARACTER&&Player(it->getCharacter())&&p.count<SELECTED_CAP)p.actors[p.count++]=Identity(*it);
 if(!p.count)return false;
 bool accepted=false;AcquireSRWLockExclusive(&s_lock);
 if(s_pendingCount<PENDING_CAP){s_pending[(s_pendingHead+s_pendingCount)%PENDING_CAP]=p;++s_pendingCount;s_pressAccepted=true;s_pressFrame=s_inputFrame;accepted=true;}
 ReleaseSRWLockExclusive(&s_lock);return accepted;
}
void SteelJobsMainTick(bool loading){
 InterlockedExchange(&s_loading,loading?1:0);if(!s_installed)return;
 ++s_inputFrame;if(!key||!key->mRight)s_pressAccepted=false;
 const double now=ElapsedSec();if(now>=s_nextCompatibilityCheck){s_nextCompatibilityCheck=now+1;
  if(GetModuleHandleW(L"KenshiQOL.dll")&&!InterlockedExchange(&s_conflict,1))FactoryLog("KenshiQOL.dll loaded; custom jobs stopped to avoid duplicate ownership");
 }
 if(!loading)TryPrepareJobs(true);
 if(!SteelJobsAdmitted())return;
 // One bounded click per frame, no world/character scan.
 Pending p={};bool have=false;AcquireSRWLockExclusive(&s_lock);
 if(s_pendingCount){p=s_pending[s_pendingHead];s_pendingHead=(s_pendingHead+1)%PENDING_CAP;--s_pendingCount;have=true;}
 ReleaseSRWLockExclusive(&s_lock);if(have)ApplyPending(p);
}
void InstallSteelJobs(int*installed,int*total){
 if(total)++*total;
 if(GetModuleHandleW(L"KenshiQOL.dll")){FactoryLog("KenshiQOL.dll loaded; custom jobs not installed");return;}
 FactoryCallbacks callbacks={RunNativeBody,RunNativeBody,SteelStockRun};
 if(!PrepareStockTransfer()){FactoryLog("stock inventory contract refused; custom jobs not installed");return;}
 if(!InitializeFactory(gameBase,callbacks,FactoryLog))return;
 bool ok=InstallTyped(&Tasker::score,HookScore,&s_score)
  &&InstallTyped(&Character::_NV_loadFromSerialisePostCreationStage,HookLoad,&s_load)
  &&InstallTyped(&PlayerInterface::playerMove,HookPlayerMove,&s_playerMove)
  &&InstallTyped(&OrderData::updateText,HookOrderText,&s_orderText)
  &&InstallTyped(&TitleScreen::_NV_show,HookTitle,&s_title);
 if(ok)ok=KenshiLib::AddHook((void*)FactoryCaptionAddress(),HookCaption,&s_caption)==KenshiLib::SUCCESS;
 if(ok)ok=KenshiLib::AddHook((void*)FactoryContextMenuAddress(),HookContext,&s_context)==KenshiLib::SUCCESS;
 if(!ok){FactoryLog("hook installation refused; custom assignments remain disabled");return;}
 s_installed=true;if(installed)++*installed;
}
}
