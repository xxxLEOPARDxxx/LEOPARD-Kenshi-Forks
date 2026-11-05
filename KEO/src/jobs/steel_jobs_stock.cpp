// Bounded actor-local stock/gather semantics adapted from Steel/KenshiQOL.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "jobs/steel_jobs_stock.h"
#include "jobs/steel_jobs.h"
#include "jobs/steel_jobs_policy.h"
#include "jobs/steel_jobs_transfer.h"
#include "inventory/inventory_config.h"
#include "base/log_admission.h"
#include "base/klib_include.h"
#include <kenshi/Character.h>
#include <kenshi/CharBody.h>
#include <kenshi/AI/AI.h>
#include <kenshi/AI/AITaskSystem.h>
#include <kenshi/Tasker.h>
#include <kenshi/Inventory.h>
#include <kenshi/Faction.h>
#include <kenshi/Building/Building.h>
#include <kenshi/Building/ProductionBuilding.h>
#include "base/klib_include_end.h"
#include <new>
namespace keo_jobs {
namespace {
enum { JOB_LIMIT=256, ITEM_LIMIT=4096, TYPE_LIMIT=64, TRANSFER_LIMIT=8 };
__declspec(align(8)) volatile LONG64 s_stockFault=0,s_gatherFault=0;
bool Faulted(volatile LONG64&flag){uint64_t epoch=SteelJobsEpoch();return epoch&&(uint64_t)InterlockedCompareExchange64(&flag,0,0)==epoch;}
ActorId Id(const hand&h){ActorId a={{(uint32_t)h.type,h.container,h.containerSerial,h.index,h.serial}};return a;}
int Kind(const Tasker*t){return t&&t->taskData?(int)t->taskData->key:-1;}
bool Enabled(){return keo_inventory::g_inventoryCfg.stockCounterJobEnabled&&SteelJobsAdmitted()&&!Faulted(s_stockFault);}
void StockFault(){if((uint64_t)InterlockedExchange64(&s_stockFault,(LONG64)SteelJobsEpoch())!=SteelJobsEpoch())LogErrorDeferrable("Steel stock jobs: guarded native failure; stocking paused until the next world");}
bool Owned(Building*b){Faction*f=b?b->getFaction():NULL;return f&&f->isPlayer&&b->isAShop()&&b->getInventory();}
bool Player(Character*c){return c&&c->isPlayerCharacter()&&!c->isAnimal();}
struct Types {GameData*data[TYPE_LIMIT];unsigned count;};
bool Has(const Types&t,GameData*d){for(unsigned i=0;i<t.count;++i)if(t.data[i]==d)return true;return false;}
bool Add(Types&t,GameData*d){if(!d||Has(t,d))return true;if(t.count==TYPE_LIMIT)return false;t.data[t.count++]=d;return true;}
struct Sources {Inventory*inv[2];unsigned count;};
bool SourcesFor(Character*c,Sources&out){
 out.count=0;if(!c||!c->inventory)return false;out.inv[out.count++]=c->inventory;
 ContainerItem*bag=c->hasABackpackOn();Inventory*b=bag?bag->getInventory():NULL;
 if(b&&b!=out.inv[0])out.inv[out.count++]=b;return true;
}
// Independently bound each inventory/section pass, before native collectors.
// No native getAllItemsOfType/getItems collection temporary is constructed.
bool InventoryBounds(Inventory*inv,unsigned&items,unsigned&sections,unsigned&sectionItems){
 if(!inv)return false;const lektor<Item*>&all=inv->_allItems;
 if(all.count>ITEM_LIMIT-items||all.count>all.maxSize||(all.count&&!all.stuff))return false;items+=all.count;
 const lektor<InventorySection*>&ss=inv->sectionsInSearchOrder;
 if(ss.count>ITEM_LIMIT-sections||ss.count>ss.maxSize||(ss.count&&!ss.stuff))return false;sections+=ss.count;
 for(unsigned i=0;i<ss.count;++i){InventorySection*s=ss.stuff[i];if(!s)return false;
  const Ogre::vector<InventorySection::SectionItem>::type&v=s->getItems();
  if(v.size()>ITEM_LIMIT-sectionItems)return false;sectionItems+=(unsigned)v.size();
 }return true;
}
bool SourceBounds(const Sources&s){unsigned items=0,sections=0,sectionItems=0;for(unsigned i=0;i<s.count;++i)if(!InventoryBounds(s.inv[i],items,sections,sectionItems))return false;return true;}
struct Query {Character*actor;Tasker*task;CharBody*body;AI*ai;Building*shop;Inventory*destination;uint64_t epoch;ActorId actorId,subject;};
bool Valid(const Query&q){
 if(!Enabled()||SteelJobsEpoch()!=q.epoch)return false;
 if(!Player(q.actor)||!SameActor(Id(q.actor->handle),q.actorId)||q.actor->getAI()!=q.ai)return false;
 if(q.body&&(q.body->ai!=q.ai||q.body->currentAction!=q.task))return false;
 if(Kind(q.task)!=STOCK_COUNTER)return false;
 return SameActor(Id(q.task->subject),q.subject)&&q.task->subject.getBuilding()==q.shop&&Owned(q.shop)&&q.shop->getInventory()==q.destination;
}
bool Begin(Character*c,Tasker*t,CharBody*b,Query&q){
 if(!Enabled()||!Player(c)||Kind(t)!=STOCK_COUNTER)return false;
 q.actor=c;q.task=t;q.body=b;q.ai=c->getAI();q.epoch=SteelJobsEpoch();
 q.actorId=Id(c->handle);q.subject=Id(t->subject);q.shop=t->subject.getBuilding();
 q.destination=q.shop?q.shop->getInventory():NULL;
 return q.ai&&q.destination&&Valid(q);
}
// The SDK collector owns a temporary unordered_set. Preflight below bounds all
// distinct carried types to64. Explicit finally destroys it even on native SEH
// with /EHsc; no GameData pointer or bucket table is retained between calls.
typedef ogre_unordered_set<GameData*>::type ResourceSet;
bool CopyResourceTypes(ResourceSet*set,Types&out){
 if(set->size()>TYPE_LIMIT)return false;
 for(ResourceSet::const_iterator i=set->begin();i!=set->end();++i)if(!Add(out,*i))return false;return true;
}
ResourceSet* ConstructResourceSet(void*storage){return new(storage)ResourceSet;}
void DestroyResourceSet(ResourceSet*set){set->~ResourceSet();}
bool NativeResourceTypes(const Sources&sources,Types&out){
 __declspec(align(16)) unsigned char storage[sizeof(ResourceSet)];ResourceSet*set=NULL;bool ok=false;
 __try {
  set=ConstructResourceSet(storage);
  for(unsigned i=0;i<sources.count;++i)sources.inv[i]->getResourceItems(*set,true);
  ok=CopyResourceTypes(set,out);
 } __finally {if(set)DestroyResourceSet(set);}
 return ok;
}
bool Collect(const Query&q,const Sources&sources,Types&out){
 out.count=0;if(!SourceBounds(sources))return false;
 OrdersReceiver*orders=q.actor->getOrdersReciever();if(!orders)return false;
 const lektor<Tasker*>&jobs=orders->permajobs;
 if(jobs.count>JOB_LIMIT||jobs.count>jobs.maxSize||(jobs.count&&!jobs.stuff))return false;
 bool gathers=false;unsigned outputItems=0;
 for(unsigned j=0;j<jobs.count;++j){Tasker*t=jobs.stuff[j];if(Kind(t)!=GATHER_OUTPUT)continue;gathers=true;
  Building*b=t->subject.getBuilding();ProductionBuilding*p=b?b->getProductionBuilding():NULL;if(!p)continue;
  if(!Add(out,p->getProductionItemData()))return false;InventorySection*s=p->outSection;if(!s)continue;
  const Ogre::vector<InventorySection::SectionItem>::type&items=s->getItems();
  if(items.size()>ITEM_LIMIT-outputItems)return false;outputItems+=(unsigned)items.size();
  for(unsigned i=0;i<items.size();++i)if(items[i].item&&!Add(out,items[i].item->getGameData()))return false;
 }
 if(gathers)return Valid(q); // Existing empty291 rows never enable unrelated resources.
 Types all={};for(unsigned n=0;n<sources.count;++n){const lektor<Item*>&items=sources.inv[n]->_allItems;
  for(unsigned i=0;i<items.count;++i)if(items.stuff[i]&&!Add(all,items.stuff[i]->getGameData()))return false;
 }
 return NativeResourceTypes(sources,out)&&Valid(q);
}
struct Candidate {Inventory*source;Item*item;ActorId id;};
bool Attempted(const ActorId*seen,unsigned count,const ActorId&id){for(unsigned i=0;i<count;++i)if(SameActor(seen[i],id))return true;return false;}
bool Find(const Query&q,const Types&types,const ActorId*seen,unsigned count,unsigned&inspected,Candidate&out){
 Sources sources={};if(!Valid(q)||!SourcesFor(q.actor,sources)||!SourceBounds(sources))return false;
 for(unsigned n=0;n<sources.count;++n){Inventory*inv=sources.inv[n];const lektor<Item*>&items=inv->_allItems;
  for(unsigned i=0;i<items.count;++i){if(inspected==ITEM_LIMIT)return false;++inspected;Item*item=items.stuff[i];
   if(!item||item->isEquipped||!Has(types,item->getGameData()))continue;
   ActorId id=Id(item->handle);if(Attempted(seen,count,id))continue;
   if(CanTransferStockItem(inv,q.destination,item)&&Valid(q)){out.source=inv;out.item=item;out.id=id;return true;}
  }
 }return false;
}
bool EligibleImpl(Character*c,Tasker*t){
 Query q={};if(!Begin(c,t,NULL,q))return false;Sources sources={};if(!SourcesFor(c,sources))return false;
 Types types={};if(!Collect(q,sources,types)||!types.count)return false;ActorId seen[TRANSFER_LIMIT]={};unsigned inspected=0;Candidate candidate={};return Find(q,types,seen,0,inspected,candidate);
}
void RunImpl(Tasker*t,CharBody*b){
 if(!Enabled()||!b||b->currentAction!=t)return;Character*c=b->getCharacter();Query q={};if(!Begin(c,t,b,q))return;
 Sources sources={};Types types={};ActorId attempted[TRANSFER_LIMIT]={};unsigned count=0,inspected=0;
 if(SourcesFor(c,sources)&&Collect(q,sources,types)){
  while(count<TRANSFER_LIMIT&&Valid(q)){
   Candidate candidate={};if(!Find(q,types,attempted,count,inspected,candidate))break;
   attempted[count++]=candidate.id;
   StockTransferResult result=TransferStockItem(candidate.source,q.destination,candidate.item);
   // Partial native merges are truthful movedUnits, never retried as unchanged.
   // Transfer module owns bounded quarantine; pointer here is diagnostic only.
   if(result.fault||result.quarantineItem){StockFault();return;}
  }
 }
 // Completion belongs to the public wrapper, including rejected targets and
 // OFF/fault paths; the permanent queued job is never removed here.
}
void CompleteCurrent(uint64_t epoch,Tasker*task,CharBody*body){
 __try {
  // The global gate and epoch must precede every old-world object read. A
  // callback may replace/retire Tasker, so compare currentAction before Kind.
  if(!SteelJobsAdmitted()||SteelJobsEpoch()!=epoch||!body||body->currentAction!=task)return;
  if(Kind(task)==STOCK_COUNTER)body->notifyBodyTaskComplete();
 }__except(EXCEPTION_EXECUTE_HANDLER){StockFault();}
}
}
bool SteelOwnedCounter(void*p){
 if(!SteelJobsAdmitted()||Faulted(s_stockFault))return false;
 __try{return Owned((Building*)p);}__except(EXCEPTION_EXECUTE_HANDLER){StockFault();return false;}
}
bool SteelGatherTarget(void*p){
 if(!SteelJobsAdmitted()||Faulted(s_gatherFault))return false;
 __try{Building*b=(Building*)p;ProductionBuilding*production=b?b->getProductionBuilding():NULL;return production&&production->outSection&&b->getUseableStuff();}
 __except(EXCEPTION_EXECUTE_HANDLER){if((uint64_t)InterlockedExchange64(&s_gatherFault,(LONG64)SteelJobsEpoch())!=SteelJobsEpoch())LogErrorDeferrable("Steel gather jobs: guarded target failure; assignments paused until the next world");return false;}
}
bool SteelStockEligible(void*c,void*t){
 if(!Enabled())return false;
 __try{return EligibleImpl((Character*)c,(Tasker*)t);}__except(EXCEPTION_EXECUTE_HANDLER){StockFault();return false;}
}
void SteelStockRun(void*t,void*b){
 if(!SteelJobsAdmitted())return;
 const uint64_t epoch=SteelJobsEpoch();Tasker*task=(Tasker*)t;CharBody*body=(CharBody*)b;
 __try {
  if(!SteelJobsAdmitted()||SteelJobsEpoch()!=epoch||!body||body->currentAction!=task||Kind(task)!=STOCK_COUNTER)return;
  // A disabled/refused custom goal must yield its own active Blank body;
  // queued rows survive and can run again after their independent toggle is ON.
  if(Enabled())RunImpl(task,body);
 }__except(EXCEPTION_EXECUTE_HANDLER){StockFault();}
 CompleteCurrent(epoch,task,body);
}
}
