#include <Windows.h>
#include <float.h>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <deque>
#include "LootScootExecuteJobsPolicy.h"
typedef int TaskType;
enum itemType { CHARACTER=1, STATS=25 };
class Character;class AI;class GameData;class GameSaveState;class GameDataContainer;class PosRotPair;
namespace Ogre {
struct Vector3 {float x,y,z;Vector3(float a=0,float b=0,float c=0):x(a),y(b),z(c){}
 Vector3 operator-(const Vector3& p)const{return Vector3(x-p.x,y-p.y,z-p.z);}
 float squaredLength()const{return x*x+y*y+z*z;}static const Vector3 ZERO;};
const Vector3 Vector3::ZERO;
}
static DWORD testNow=1000;
#define GetTickCount() testNow
static int checks=0;
#define CHECK(x) do {++checks;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static Character* registry[256]={};
static unsigned handleReads=0,medicalReads=0;
struct hand {
 itemType type;unsigned container,containerSerial,index,serial;
 hand(unsigned n=0,unsigned s=1,itemType t=CHARACTER,unsigned c=1,unsigned cs=1):type(t),container(c),containerSerial(cs),index(n),serial(s){}
 Character* getCharacter()const;bool isNull()const{return index==0;}
};
template<class T> struct lektor {
 unsigned count,maxSize;T* stuff;std::vector<T> values;
 lektor():count(0),maxSize(0),stuff(0){}unsigned size()const{return count;}
 T operator[](unsigned n)const{return stuff[n];}
 void push(T t){values.push_back(t);count=static_cast<unsigned>(values.size());maxSize=count;stuff=&values[0];}
 void clear(){values.clear();count=maxSize=0;stuff=0;}
};
struct RootObject {Ogre::Vector3 position;virtual ~RootObject(){}};
enum PermajobType {NOT_A_PERMAJOB,PERMAJOB_NORMAL};
struct TaskData {TaskType key;bool hasAction,_resultsAlwaysFalse;bool fixed,permaJob_FixedTarget,requirementsAlwaysFalse,needsTarget;PermajobType permaJob;int numRequirements,numResults;void* scoreFunction;std::string caption;
 TaskData(TaskType n=0):key(n),hasAction(false),_resultsAlwaysFalse(false),fixed(true),permaJob_FixedTarget(true),requirementsAlwaysFalse(false),needsTarget(false),permaJob(NOT_A_PERMAJOB),numRequirements(0),numResults(0),scoreFunction(0){}
 void setPermaJob(PermajobType p,bool f,TaskType,TaskType){fixed=permaJob_FixedTarget=f;permaJob=p;}};
struct Tasker {TaskData* taskData;hand subject;int priority;float weight;Tasker(TaskData* d=0):taskData(d),priority(2),weight(1){}};
struct TaskMatch {TaskData* taskData;TaskMatch():taskData(0){}};
struct OrdersReceiver {
 lektor<Tasker*> permajobs;bool enabled,manual;unsigned updates;
 TaskMatch currentGoal;float currentGoalScore;int currentGoalPriority;struct {std::deque<Tasker*> list;}orders;
 OrdersReceiver():enabled(true),manual(false),updates(0),currentGoalScore(0),currentGoalPriority(0){}
 const TaskMatch& getCurrentGoal(){return currentGoal;}Tasker* getFirstOrder(){return orders.list.empty()?0:orders.list.front();}
 bool isJobsEnabled()const{return enabled;}bool hasPlayerOrders()const{return manual;}
 void setNeedGOAP(){++updates;}
};
struct AITaskSytem:OrdersReceiver {void callForUpdate(bool){++updates;}};
class AI {public:Character* c;AITaskSytem* tasks;Character* getCharacter(){return c;}AITaskSytem* getTaskSystem(){return tasks;}float scoreNone(const hand&,const Ogre::Vector3&){return 1.0f;}};
struct CharBody {Character* character;Tasker* currentAction;unsigned completions;CharBody():character(0),currentAction(0),completions(0){}
 void notifyBodyTaskComplete(){++completions;currentAction=0;}};
class Character:public RootObject {public:
 hand id;bool player,down,dead,carried,ragdoll,combat,underAttack,enemy;int attackers;
 AITaskSytem tasks;AI ai;CharBody body;
 Character(unsigned n):id(n),player(true),down(false),dead(false),carried(false),ragdoll(false),combat(false),underAttack(false),enemy(false),attackers(0){registry[n]=this;ai.c=this;ai.tasks=&tasks;body.character=this;}
 const hand& getHandle(){++handleReads;return id;}bool isPlayerCharacter(){return player;}bool isDead(){return dead;}bool isUnconcious(){return down;}
 bool isDown(){return down;}bool isBeingCarried(){return carried;}bool isRagdoll(){return ragdoll;}
 bool isInCombatMode(bool,bool){return combat;}bool isLiterallyUnderMeleeAttackRightNowForSure(){return underAttack;}
 int getAllAttackersCount(){return attackers;}OrdersReceiver* getOrdersReciever(){return &tasks;}AI* getAI(){return &ai;}CharBody* getBody(){return &body;}
};
Character* hand::getCharacter()const {Character* c=index<256?registry[index]:0;return c&&c->id.serial==serial&&c->id.container==container&&c->id.containerSerial==containerSerial?c:0;}
struct Config {bool enabled,enableExecuteAll,ignoreExecuteAllianceCheck;Config():enabled(true),enableExecuteAll(true),ignoreExecuteAllianceCheck(true){}}g_config;
static bool g_effectiveEnableExecuteAction=true;
struct World {bool paused;bool isPaused(){return paused;}}world={false};static World* ou=&world;
struct ExecuteJob {bool queuedActive;uintptr_t actorPtr;hand targetHandle;int permanentMode;ExecuteJob():queuedActive(false),actorPtr(0),permanentMode(0){}};
static const unsigned kExecuteJobMax=50,kExecuteAllWorldQueryMaxTargets=128;
static ExecuteJob g_executeJobs[50];static unsigned g_executeJobIndex=0;
static unsigned queued=0,queries=0,forwardedScores=0,forwardedActions=0;
static lektor<RootObject*> queryResults;
struct CanExecuteDiagnostics {bool targetIsEnemy;};
static CanExecuteDiagnostics MakeCanExecuteDiagnostics(){CanExecuteDiagnostics d={false};return d;}
static bool CanExecuteFromNativeMenuSelection(Character*,RootObject* root,CanExecuteDiagnostics* d,bool) {
 Character* t=static_cast<Character*>(root);if(!t)return false;d->targetIsEnemy=t->enemy;
 return !t->player&&t->down&&!t->dead&&(t->enemy||g_config.ignoreExecuteAllianceCheck);
}
static bool CanActOnExecuteOrder(Character* c){return c&&!c->dead&&!c->down&&!c->carried;}
static bool debugEnabled=false;
static std::vector<std::string> debugLogs;
static bool ShouldLogExecuteDebug(){return debugEnabled;}
static void PluginLog(const char* message){debugLogs.push_back(message);}
static bool DebounceWindowElapsed(DWORD now,DWORD then,DWORD interval){return DWORD(now-then)>=interval;}
static bool TryReadRootObjectPosition(RootObject* r,Ogre::Vector3* p){if(!r)return false;*p=r->position;return true;}
static bool TryGetRootObjectTypeForExecutePredicate(RootObject*,itemType* t){*t=CHARACTER;return true;}
static bool IsCharacterDataType(itemType t){return t==CHARACTER;}
static float ComputeExecuteAllRadiusUnits(){return 10.0f;}
static bool TryGetCharactersWithinSphere(lektor<RootObject*>* out,const Ogre::Vector3&,float,RootObject*,size_t cap){
 ++queries;for(unsigned i=0;i<queryResults.size()&&i<cap;++i)out->push(queryResults[i]);return true;
}
static bool QueueExecuteFromNativeMenuSelection(Character* c,RootObject* target,bool){
 ++queued;g_executeJobIndex=0;g_executeJobs[0].queuedActive=true;g_executeJobs[0].actorPtr=(uintptr_t)c;
 g_executeJobs[0].targetHandle=static_cast<Character*>(target)->id;c->tasks.manual=true;return true;
}
enum HitMaterialType {HIT_FLESH};typedef int CutDirection;class CombatTechniqueData{};class Harpoon{};
struct Damages {bool ko;Damages(bool k=false):ko(k){}};
struct MedicalSystem {Character* me;bool armed;MedicalSystem(Character* c):me(c),armed(false){}bool isDead(){++medicalReads;return me->dead;}bool isUnconcious(){++medicalReads;return me->down;}};
class GameData {public:bool hasOwner;hand owner;GameData():hasOwner(false){}void storeHandle(const hand& h,const std::string&,bool redirect){CHECK(!redirect);hasOwner=true;owner=h;}
 bool getHandle(hand& h,const std::string&){if(!hasOwner)return false;h=owner;return true;}};
class GameSaveState {public:GameData data;GameData* getState(itemType t){CHECK(t==STATS);return &data;}};
class GameDataContainer{};class PosRotPair{};class TitleScreen{};
static const char* Tr(const char* x){return x;}static unsigned errors=0;static void ErrorLog(const char*){++errors;}

// PRODUCTION_CODE is generated from the actual runtime source, not a copy.
#include "production_jobs_runtime.inc"

using namespace lse_jobs;using namespace lse_jobs_runtime;
static float NativeScore(Tasker*,AI*){++forwardedScores;return 0.73f;}
static void NativeBlank(Tasker*,CharBody*){++forwardedActions;}
static float nativeGoapResult=0.5f;
static float NativeGoap(AITaskSytem*,Tasker*,bool,bool){return nativeGoapResult;}
static bool nativeSetResult=true;
static bool NativeSet(CharBody* body,Tasker* task){if(nativeSetResult)body->currentAction=task;return nativeSetResult;}
static void NativeMedical(MedicalSystem* m,float){if(m->armed)m->me->down=true;}
static void NativeKnockout(MedicalSystem* m,float){m->armed=true;}
static HitMaterialType NativeMelee(Character* v,CutDirection,Damages& damage,Character*,CombatTechniqueData*,int){if(damage.ko)v->down=true;return HIT_FLESH;}
static bool NativeRanged(Character* v,Character*,Harpoon*,bool){v->down=true;return true;}
static MedicalSystem* stealthMedical=0;
static void NativeStealth(Tasker*,CharBody*){Knockout(stealthMedical,1.0f);}
static bool saveFirst=true;
static GameSaveState* NativeSave(void* first,void* second,GameDataContainer*,GameData*,PosRotPair*){return static_cast<GameSaveState*>(saveFirst?first:second);}
static void NativeLoad(Character*,GameSaveState*){}
static void NativeTitle(TitleScreen*,bool){}
static TaskData* registered[2]={0,0};static unsigned registrations=0;
static const TaskData* NativeGetData(TaskType type){return type==OWN_DEFEATS?registered[0]:registered[1];}
static TaskData* NativeRegister(TaskType type,float(AI::*score)(const hand&,const Ogre::Vector3&),bool action,bool needs,const std::string& caption,unsigned flags) {
 CHECK(score!=0&&!action&&!needs&&flags==1);++registrations;TaskData* d=new TaskData(type);d->caption=caption;registered[type==OWN_DEFEATS?0:1]=d;return d;
}
static void Clean(Character& c) {
 Reset();InterlockedExchange(&ready,1);InterlockedExchange(&live,1);
 c.tasks.manual=false;c.tasks.enabled=true;c.combat=false;c.underAttack=false;c.attackers=0;c.body.currentAction=0;
 for(unsigned i=0;i<50;++i)g_executeJobs[i]=ExecuteJob();queries=queued=0;testNow+=1000;
}
int main(){
 TaskData ownData(OWN_DEFEATS),allData(ALL_ENEMIES),otherData(45),impostorData(OWN_DEFEATS);
 metadata[0]=&ownData;metadata[1]=&allData;
 Tasker own(&ownData),all(&allData),other(&otherData),impostor(&impostorData);
 scoreOriginal=NativeScore;blankActionOriginal=NativeBlank;medicalOriginal=NativeMedical;
 knockoutOriginal=NativeKnockout;meleeOriginal=NativeMelee;rangedOriginal=NativeRanged;stealthOriginal=NativeStealth;
 Character c(1),victim(2),otherKiller(3);victim.player=false;victim.down=true;victim.enemy=true;
 c.tasks.permajobs.push(&own);c.tasks.permajobs.push(&all);queryResults.push(&victim);
 Clean(c);MedicalSystem idleMedical(&otherKiller);handleReads=medicalReads=0;Medical(&idleMedical,0);CHECK(handleReads==0&&medicalReads==0);
 victim.down=false;Damages openingKO(true);Melee(&victim,0,openingKO,&c,0,0);CHECK(ledgerCount==1);
 handleReads=medicalReads=0;Medical(&idleMedical,0);CHECK(handleReads==1&&medicalReads==0); // tracked ledger, unrelated NPC
 Clean(c);CHECK(ObserveActor(&c));
 CHECK(Score(&own,&c.ai)==0.0f);CHECK(Score(&other,&c.ai)==0.73f);
 CHECK(Score(&impostor,&c.ai)==0.73f);BlankAction(&impostor,&c.body);CHECK(forwardedActions==1);
 Tick();CHECK(queries==1);CHECK(queued==0);CHECK(Score(&own,&c.ai)==0.0f);CHECK(Score(&all,&c.ai)==0.73f);
 for(unsigned i=0;i<100;++i)Tick();CHECK(queries==1);CHECK(queued==0);
 c.body.currentAction=&all;BlankAction(&all,&c.body);CHECK(queued==0);Tick();CHECK(queued==1);
 CHECK(g_executeJobs[0].permanentMode==ALL_ENEMIES);CHECK(AutomaticQueueValid(&c,&victim,ALL_ENEMIES));
 c.combat=true;CHECK(!AutomaticQueueValid(&c,&victim,ALL_ENEMIES));CHECK(Score(&all,&c.ai)==0.0f);
 c.combat=false;c.tasks.enabled=false;CHECK(!AutomaticQueueValid(&c,&victim,ALL_ENEMIES));c.tasks.enabled=true;
 victim.enemy=false;CHECK(!AutomaticQueueValid(&c,&victim,ALL_ENEMIES));victim.enemy=true;
 c.tasks.permajobs.clear();CHECK(!AutomaticQueueValid(&c,&victim,ALL_ENEMIES));c.tasks.permajobs.push(&own);c.tasks.permajobs.push(&all);
 Clean(c);ObserveActor(&c);ledger.Record(Id(victim.id),Id(c.id),false,testNow);victim.enemy=false;
 Tick();CHECK(Score(&own,&c.ai)==0.73f);CHECK(Score(&all,&c.ai)==0.0f);
 c.body.currentAction=&own;BlankAction(&own,&c.body);victim.down=false;Tick();CHECK(queued==0);CHECK(!ledger.Owns(Id(victim.id),Id(c.id)));
 victim.down=true;Clean(c);ObserveActor(&c);victim.enemy=true;Tick();c.body.currentAction=&all;BlankAction(&all,&c.body);
 c.tasks.manual=true;Tick();CHECK(queued==0); // direct order wins over a pending native request
 Clean(c);ObserveActor(&c);Tick();c.body.currentAction=&all;BlankAction(&all,&c.body);c.body.currentAction=&other;Tick();CHECK(queued==0);
 Clean(c);ObserveActor(&c);c.tasks.manual=true;Tick();CHECK(queries==0);c.tasks.manual=false;c.combat=true;testNow+=250;Tick();CHECK(queries==0);
 c.combat=false;c.attackers=1;testNow+=250;Tick();CHECK(queries==0);c.attackers=0;
 Clean(c);ObserveActor(&c);Tick();testNow+=21000;CHECK(Score(&all,&c.ai)==0.0f);
 Clean(c);for(unsigned i=0;i<ACTOR_CAP;++i){Character* n=new Character(10+i);CHECK(ObserveActor(n));}
 Character overflow(99);CHECK(!ObserveActor(&overflow));
 Clean(c);victim.down=false;Damages scratch(false),ko(true);
 Melee(&victim,0,scratch,&c,0,0);CHECK(!ledger.Owns(Id(victim.id),Id(c.id)));
 Melee(&victim,0,ko,&c,0,0);CHECK(ledger.Owns(Id(victim.id),Id(c.id)));
 Melee(&victim,0,ko,&otherKiller,0,0);CHECK(!ledger.Owns(Id(victim.id),Id(otherKiller.id)));CHECK(ledger.Owns(Id(victim.id),Id(c.id)));
 MedicalSystem medical(&victim);victim.down=false;Medical(&medical,0);CHECK(!ledger.Owns(Id(victim.id),Id(c.id)));
 Ranged(&victim,&otherKiller,0,true);CHECK(ledger.Owns(Id(victim.id),Id(otherKiller.id)));
 victim.down=false;Medical(&medical,0);stealthMedical=&medical;own.subject=victim.id;
 Knockout(&medical,1);CHECK(!ledger.Find(Id(victim.id)));medical.armed=false; // no real native action context
 Stealth(&own,&c.body);CHECK(!ledger.Owns(Id(victim.id),Id(c.id)));Medical(&medical,0);CHECK(ledger.Owns(Id(victim.id),Id(c.id)));
 CHECK(damageActor==0&&damageVictim==0);
 medical.armed=false;victim.down=false;Medical(&medical,0);CHECK(!ledger.Owns(Id(victim.id),Id(c.id)));
 // Freed Tasker storage reused for an unrelated action cannot consume a handoff.
 Clean(c);ObserveActor(&c);victim.down=true;victim.enemy=true;Tick();c.body.currentAction=&all;BlankAction(&all,&c.body);
 all.taskData=&otherData;Tick();CHECK(queued==0);all.taskData=&allData;
 // Removing all our rows releases the bounded opted-in slot on the next scan.
 Clean(c);ObserveActor(&c);c.tasks.permajobs.clear();Tick();CHECK(Find(Id(c.id))<0);c.tasks.permajobs.push(&own);
 // Confirmed ownership is native saved state; pending actions and awakened
 // victims cannot manufacture it. Both native sret register orders are tested.
 saveOriginal=NativeSave;loadOriginal=NativeLoad;
 Clean(c);victim.down=false;Melee(&victim,0,ko,&c,0,0);GameSaveState saved;
 Save(&saved,&victim,0,0,0);CHECK(saved.data.hasOwner);CHECK(saved.data.owner.index==c.id.index);
 Reset();InterlockedExchange(&ready,1);Load(&victim,&saved);CHECK(ledger.Owns(Id(victim.id),Id(c.id)));
 victim.down=false;Medical(&medical,0);GameSaveState awakeSave;Save(&awakeSave,&victim,0,0,0);CHECK(!awakeSave.data.hasOwner);
 victim.down=false;Load(&victim,&saved);CHECK(!ledger.Owns(Id(victim.id),Id(c.id)));
 victim.down=true;saveFirst=false;Load(&victim,&saved);GameSaveState reversed;Save(&victim,&reversed,0,0,0);CHECK(reversed.data.hasOwner);saveFirst=true;
 Reset();InterlockedExchange(&ready,1);Load(&c,&awakeSave);CHECK(Find(Id(c.id))>=0); // row survives load
 // Registration refuses a pre-owned ID before touching either task type.
 titleOriginal=NativeTitle;getData=NativeGetData;registerData=NativeRegister;
 InterlockedExchange(&ready,0);registrationAttempted=false;registered[1]=&otherData;Title(0,true);CHECK(registrations==0);CHECK(!ready);CHECK(errors==1);
 Title(0,true);CHECK(errors==1); // one collision diagnostic, no repeated attempts
 registered[1]=0;registrationAttempted=false;Title(0,true);CHECK(registrations==2);CHECK(ready);
 CHECK(metadata[0]->key==OWN_DEFEATS&&metadata[1]->key==ALL_ENEMIES);
 CHECK(metadata[0]->hasAction&&metadata[0]->_resultsAlwaysFalse&&!metadata[0]->fixed);
 CHECK(metadata[0]->caption=="Execute own defeats"&&metadata[1]->caption=="Execute enemies");
 // Diagnostics exercise extracted production hooks and scheduler boundaries.
 // Disabled diagnostics perform no output; enabling starts a bounded window.
 metadata[0]=&ownData;metadata[1]=&allData;c.tasks.permajobs.clear();c.tasks.permajobs.push(&all);
 Clean(c);ObserveActor(&c);Tick();CHECK(debugLogs.empty());
 debugEnabled=true;testNow+=250;Tick();CHECK(debugLogs.empty());
 CHECK(ObserveActor(&c));
 runGoapOriginal=NativeGoap;setActionOriginal=NativeSet;
 CHECK(RunGoap(&c.tasks,&all,true,false)==0.5f);nativeGoapResult=0;CHECK(RunGoap(&c.tasks,&all,true,false)==0);
 CHECK(SetAction(&c.body,&all));nativeSetResult=false;CHECK(!SetAction(&c.body,&all));nativeSetResult=true;
 CHECK(Score(&all,&c.ai)==0.73f);
 c.body.currentAction=&all;BlankAction(&all,&c.body);c.tasks.manual=true;Tick();CHECK(queued==0);
 c.tasks.manual=false;c.combat=true;testNow+=250;Tick();c.combat=false;
 c.tasks.enabled=false;testNow+=250;Tick();c.tasks.enabled=true;
 c.tasks.manual=true;testNow+=250;Tick();c.tasks.manual=false;
 testNow+=250;Tick();CHECK(queries>0);
 // Offer transitions may log promptly, summaries remain at most once/2s.
 unsigned summaries=0;for(unsigned i=0;i<debugLogs.size();++i)if(debugLogs[i].find("permanent_job_scheduler")!=std::string::npos)++summaries;
 CHECK(summaries==0);testNow+=1000;Tick();
 summaries=0;std::string summary;for(unsigned i=0;i<debugLogs.size();++i)if(debugLogs[i].find("permanent_job_scheduler")!=std::string::npos){++summaries;summary=debugLogs[i];}
 CHECK(summaries==1);CHECK(summary.find("score=1")!=std::string::npos);CHECK(summary.find("blank=1")!=std::string::npos);
 CHECK(summary.find("reject_not_ready=1")!=std::string::npos);CHECK(summary.find("combat=2")!=std::string::npos);
 CHECK(summary.find("jobs_off=2")!=std::string::npos);CHECK(summary.find("manual=3")!=std::string::npos);
 CHECK(summary.find("observed=1")!=std::string::npos);CHECK(summary.find("query=2")!=std::string::npos);
 CHECK(summary.find("candidates=2")!=std::string::npos);CHECK(summary.find("offers=2")!=std::string::npos);
 CHECK(summary.find("score_positive=1")!=std::string::npos);CHECK(summary.find("last_score=0.73")!=std::string::npos);
 CHECK(summary.find("goap=2")!=std::string::npos);CHECK(summary.find("goap_positive=1")!=std::string::npos);CHECK(summary.find("goap_failed=1")!=std::string::npos);
 CHECK(summary.find("set_action=2")!=std::string::npos);CHECK(summary.find("set_action_accepted=1")!=std::string::npos);
 CHECK(summary.find("mode1=[key:295,action:")!=std::string::npos);CHECK(summary.find(",row:0,priority:2,weight:1]")!=std::string::npos);
 unsigned transitions=0;for(unsigned i=0;i<debugLogs.size();++i)if(debugLogs[i].find("permanent_job_offer")!=std::string::npos)++transitions;CHECK(transitions==1);
 for(unsigned i=0;i<100;++i)Tick();summaries=0;for(unsigned i=0;i<debugLogs.size();++i)if(debugLogs[i].find("permanent_job_scheduler")!=std::string::npos)++summaries;CHECK(summaries==1);
 const size_t beforeDisabled=debugLogs.size();debugEnabled=false;testNow+=3000;Tick();Score(&all,&c.ai);CHECK(debugLogs.size()==beforeDisabled);
 printf("%d extracted jobs runtime checks passed\n",checks);return 0;
}
