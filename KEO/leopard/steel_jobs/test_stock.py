"""Execute actual stock.cpp bodies against owned SDK boundaries; no game calls."""
from pathlib import Path
import os,subprocess,sys
R=Path(__file__).resolve().parents[2];O=R/'build/steel-jobs-tests/stock';O.mkdir(parents=True,exist_ok=True)
s=(R/'src/jobs/steel_jobs_stock.cpp').read_text(encoding='utf-8-sig');s=s[s.index('namespace keo_jobs {'):]
if '--red-control'in sys.argv:
 old='return f&&f->isPlayer&&b->isAShop()&&b->getInventory();';assert old in s;s=s.replace(old,'return f&&b->isAShop()&&b->getInventory();')
if '--retired-red'in sys.argv:
 old='if(q.body&&(q.body->ai!=q.ai||q.body->currentAction!=q.task))return false;\n if(Kind(q.task)!=STOCK_COUNTER)return false;'
 new='if(Kind(q.task)!=STOCK_COUNTER)return false;\n if(q.body&&(q.body->ai!=q.ai||q.body->currentAction!=q.task))return false;'
 assert old in s;s=s.replace(old,new)
code=r'''
#define NOMINMAX
#define UNICODE
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>
#include <new>
#include "jobs/steel_jobs_policy.h"
#include "jobs/steel_jobs_transfer.h"
#include "jobs/steel_jobs_stock.h"
static int checks=0,allocs=0,frees=0,setMade=0,setDestroyed=0,resourceCalls=0,txCalls=0,errors=0,nativeReads=0;
static bool admitted=true,raiseResource=false,raiseOwner=false,raiseGather=false,cancelTx=false,disableTx=false,partialTx=false,faultTx=false;
static uint64_t epoch=1;
void*operator new(size_t n){++allocs;void*p=malloc(n);if(!p)throw std::bad_alloc();return p;}void operator delete(void*p){++frees;free(p);}
void C(bool b,const char*t){++checks;if(!b){printf("FAIL %s\n",t);exit(1);}}
void LogErrorDeferrable(const char*){++errors;}
namespace keo_inventory{struct Cfg{bool stockCounterJobEnabled;};static Cfg g_inventoryCfg={true};}
class Building;
struct hand{unsigned type,container,containerSerial,index,serial;Building*building;hand():type(1),container(2),containerSerial(3),index(4),serial(5),building(NULL){}Building*getBuilding()const{++nativeReads;return building;}};
template<class T>struct lektor{unsigned count,maxSize;T*stuff;lektor():count(0),maxSize(0),stuff(NULL){}};
struct GameData{int id;bool resource;};
class Inventory;
struct Item{hand handle;bool isEquipped;int quantity;GameData*data;Inventory*inside;Item():isEquipped(false),quantity(1),data(NULL),inside(NULL){}GameData*getGameData(){++nativeReads;return data;}Inventory*getInventory(){return inside;}};
typedef Item ContainerItem;
namespace Ogre{template<class T>struct vector{typedef std::vector<T>type;};}
struct InventorySection{struct SectionItem{Item*item;};std::vector<SectionItem>items;const std::vector<SectionItem>&getItems(){return items;}};
struct ResourceSet{typedef std::set<GameData*>::const_iterator const_iterator;std::set<GameData*>values;ResourceSet(){++setMade;}~ResourceSet(){++setDestroyed;}size_t size()const{return values.size();}const_iterator begin()const{return values.begin();}const_iterator end()const{return values.end();}void insert(GameData*d){values.insert(d);}};
template<class T>struct ogre_unordered_set{typedef ResourceSet type;};
class Inventory{public:lektor<Item*>_allItems;lektor<InventorySection*>sectionsInSearchOrder;bool room;Inventory():room(true){}void getResourceItems(ResourceSet&out,bool buildingMats){C(buildingMats,"native fallback includes building mats");++resourceCalls;for(unsigned i=0;i<_allItems.count;++i){Item*item=_allItems.stuff[i];if(item&&item->data&&item->data->resource)out.insert(item->data);}if(raiseResource)RaiseException(0xe0123456,0,0,NULL);}};
struct ProductionBuilding{InventorySection*outSection;GameData*made;ProductionBuilding():outSection(NULL),made(NULL){}GameData*getProductionItemData(){return made;}};
struct Faction{void*isPlayer;Faction():isPlayer(NULL){}};
class Building{public:Faction*faction;bool shop,usable;Inventory*inventory;ProductionBuilding*production;Building():faction(NULL),shop(false),usable(false),inventory(NULL),production(NULL){}Faction*getFaction(){++nativeReads;if(raiseOwner)RaiseException(0xe0123457,0,0,NULL);return faction;}bool isAShop(){return shop;}Inventory*getInventory(){return inventory;}ProductionBuilding*getProductionBuilding(){if(raiseGather)RaiseException(0xe0123458,0,0,NULL);return production;}void*getUseableStuff(){return usable?(void*)1:NULL;}};
struct TaskData{int key;};struct Tasker{TaskData*taskData;hand subject;Tasker():taskData(NULL){}};
struct OrdersReceiver{lektor<Tasker*>permajobs;};struct AI{};
struct Character{hand handle;Inventory*inventory;ContainerItem*bag;AI*ai;OrdersReceiver*orders;bool player,animal;Character():inventory(NULL),bag(NULL),ai(NULL),orders(NULL),player(true),animal(false){}bool isPlayerCharacter(){++nativeReads;return player;}Character*isAnimal(){return animal?this:NULL;}ContainerItem*hasABackpackOn(){return bag;}AI*getAI(){return ai;}OrdersReceiver*getOrdersReciever(){return orders;}};
struct CharBody{AI*ai;Tasker*currentAction;Character*character;int notifications;CharBody():ai(NULL),currentAction(NULL),character(NULL),notifications(0){}Character*getCharacter(){return character;}void notifyBodyTaskComplete(){++notifications;}};
static CharBody*retireBody=NULL;static Tasker*retireTask=NULL;
namespace keo_jobs {uint64_t SteelJobsEpoch(){return epoch;}bool SteelJobsAdmitted(){return admitted;}bool CanTransferStockItem(void*,void*dst,void*raw){Item*item=(Item*)raw;return ((Inventory*)dst)->room&&item&&!item->isEquipped&&item->quantity>0;}StockTransferResult TransferStockItem(void*,void*,void*raw){++txCalls;Item*item=(Item*)raw;StockTransferResult r={0,false,NULL,epoch};if(faultTx){r.fault=true;return r;}if(partialTx)r.movedUnits=.5;else{r.movedUnits=item->quantity;item->quantity=0;}if(cancelTx)++epoch;if(disableTx)keo_inventory::g_inventoryCfg.stockCounterJobEnabled=false;if(retireBody){retireBody->currentAction=NULL;retireTask->taskData=(TaskData*)1;}return r;}}
PRODUCTION
struct Fixture{GameData data[70];Item items[70],bagItem;Item*carried[70];Inventory source,bag,destination;Faction faction;Building shop,machine;ProductionBuilding production;InventorySection output;InventorySection*sectionList[1];InventorySection::SectionItem si;Character actor;AI ai;OrdersReceiver orders;TaskData stockData,gatherData;Tasker stock,gather;Tasker*jobs[2];CharBody body;
 Fixture(){retireBody=NULL;retireTask=NULL;admitted=true;raiseResource=raiseOwner=raiseGather=cancelTx=disableTx=partialTx=faultTx=false;keo_inventory::g_inventoryCfg.stockCounterJobEnabled=true;txCalls=resourceCalls=0;++epoch;for(int i=0;i<70;++i){data[i].id=i;data[i].resource=true;items[i].data=&data[0];items[i].handle.index=i;items[i].handle.serial=100+i;carried[i]=&items[i];}source._allItems.count=1;source._allItems.maxSize=70;source._allItems.stuff=carried;faction.isPlayer=(void*)1;shop.faction=&faction;shop.shop=true;shop.inventory=&destination;machine.production=&production;machine.usable=true;actor.inventory=&source;actor.ai=&ai;actor.orders=&orders;stockData.key=293;gatherData.key=291;stock.taskData=&stockData;stock.subject.building=&shop;gather.taskData=&gatherData;gather.subject.building=&machine;jobs[0]=&stock;jobs[1]=&gather;orders.permajobs.count=1;orders.permajobs.maxSize=2;orders.permajobs.stuff=jobs;body.ai=&ai;body.currentAction=&stock;body.character=&actor;}
};
void Ownership(){Fixture f;f.faction.isPlayer=NULL;C(!keo_jobs::SteelOwnedCounter(&f.shop),"NPC shop cannot be stock counter");f.faction.isPlayer=(void*)1;C(keo_jobs::SteelOwnedCounter(&f.shop),"player shop counter accepted");f.shop.shop=false;C(!keo_jobs::SteelOwnedCounter(&f.shop),"player nonshop rejected");f.shop.shop=true;C(!keo_jobs::SteelOwnedCounter(NULL),"null target rejected");C(keo_jobs::SteelGatherTarget(&f.machine)==false,"no machine output rejected");f.production.outSection=&f.output;C(keo_jobs::SteelGatherTarget(&f.machine),"machine output accepted without faction restriction");f.machine.usable=false;C(!keo_jobs::SteelGatherTarget(&f.machine),"unusable output machine rejected");}
void Collection(){Fixture f;C(keo_jobs::SteelStockEligible(&f.actor,&f.stock)&&resourceCalls==1,"no291 rows uses native resource classification");C(setMade==setDestroyed,"temporary native set destroyed on success");f.actor.player=false;C(!keo_jobs::SteelStockEligible(&f.actor,&f.stock),"NPC actor rejected");f.actor.player=true;f.actor.animal=true;C(!keo_jobs::SteelStockEligible(&f.actor,&f.stock),"animal rejected");f.actor.animal=false;f.destination.room=false;C(!keo_jobs::SteelStockEligible(&f.actor,&f.stock),"full destination rejected without transfer");f.destination.room=true;
 f.orders.permajobs.count=2;resourceCalls=0;C(!keo_jobs::SteelStockEligible(&f.actor,&f.stock)&&!resourceCalls,"existing empty291 row never enables native-resource fallback");f.production.made=&f.data[0];C(keo_jobs::SteelStockEligible(&f.actor,&f.stock)&&!resourceCalls,"291 produced type used without native set");f.production.made=&f.data[1];f.production.outSection=&f.output;InventorySection::SectionItem si={&f.items[0]};f.output.items.push_back(si);C(keo_jobs::SteelStockEligible(&f.actor,&f.stock),"actual prior output section type included after crafting queue changes");f.items[0].isEquipped=true;C(!keo_jobs::SteelStockEligible(&f.actor,&f.stock),"never selects equipped gear");f.items[0].isEquipped=false;
 f.source._allItems.count=4097;f.source._allItems.maxSize=4097;C(!keo_jobs::SteelStockEligible(&f.actor,&f.stock),"4096 input cap before invalid pointer read");f.source._allItems.count=1;f.source._allItems.maxSize=70;f.orders.permajobs.count=257;f.orders.permajobs.maxSize=257;C(!keo_jobs::SteelStockEligible(&f.actor,&f.stock),"256 job cap before reading short job storage");f.orders.permajobs.count=1;f.orders.permajobs.maxSize=2;f.source._allItems.count=65;for(int i=0;i<65;++i)f.items[i].data=&f.data[i];resourceCalls=0;C(!keo_jobs::SteelStockEligible(&f.actor,&f.stock)&&!resourceCalls,"64 carried unique-type preflight bounds native set allocation");
 Fixture bag;bag.source._allItems.count=0;bag.bag._allItems.count=1;bag.bag._allItems.maxSize=70;bag.bag._allItems.stuff=bag.carried;bag.bagItem.inside=&bag.bag;bag.actor.bag=&bag.bagItem;C(keo_jobs::SteelStockEligible(&bag.actor,&bag.stock),"native resource fallback sees worn backpack inventory");}
void Running(){Fixture f;f.source._allItems.count=12;keo_jobs::SteelStockRun(&f.stock,&f.body);C(txCalls==8&&f.body.notifications==1,"eight distinct stack attempts max then one valid native completion");for(int i=0;i<12;++i)C(f.items[i].quantity==(i<8?0:1),"no ninth transfer or duplicate item retry");
 Fixture off;off.orders.permajobs.count=2;keo_inventory::g_inventoryCfg.stockCounterJobEnabled=false;int reads=nativeReads;Tasker*queued=off.jobs[0];unsigned queuedCount=off.orders.permajobs.count;off.body.character=(Character*)1;C(!keo_jobs::SteelStockEligible(&off.actor,&off.stock),"OFF score blocked");keo_jobs::SteelStockRun(&off.stock,&off.body);C(!txCalls&&off.body.notifications==1&&nativeReads==reads&&!resourceCalls,"OFF releases exactly its own active Blank body without inventory reads or transfers");C(off.orders.permajobs.count==queuedCount&&off.jobs[0]==queued&&off.jobs[1]==&off.gather&&off.stock.subject.building==&off.shop,"OFF completion preserves queued row and subject");
 Fixture missing;missing.stock.subject.building=NULL;keo_jobs::SteelStockRun(&missing.stock,&missing.body);C(missing.body.notifications==1&&!txCalls&&!resourceCalls&&missing.orders.permajobs.count==1&&missing.jobs[0]==&missing.stock,"invalid target releases current owned Blank body without deleting queued row");
 Fixture lostOwner;lostOwner.faction.isPlayer=NULL;keo_jobs::SteelStockRun(&lostOwner.stock,&lostOwner.body);C(lostOwner.body.notifications==1&&!txCalls&&!resourceCalls,"lost target ownership releases own current goal without inventory work");
 Fixture nativeFault;raiseResource=true;int beforeMade=setMade,beforeDestroyed=setDestroyed;keo_jobs::SteelStockRun(&nativeFault.stock,&nativeFault.body);C(nativeFault.body.notifications==1&&!txCalls&&setMade==beforeMade+1&&setDestroyed==beforeDestroyed+1&&nativeFault.orders.permajobs.count==1,"native collector SEH cleans temporary and releases only current owned body with queued row intact");
 Fixture loading;admitted=false;loading.stock.taskData=(TaskData*)1;loading.body.character=(Character*)1;int errorsBefore=errors;reads=nativeReads;keo_jobs::SteelStockRun(&loading.stock,&loading.body);C(!loading.body.notifications&&!txCalls&&nativeReads==reads&&errors==errorsBefore,"refused admission reads no stale task and never completes old-world body");
 Fixture foreign;keo_inventory::g_inventoryCfg.stockCounterJobEnabled=false;TaskData otherData={242};Tasker otherTask;otherTask.taskData=&otherData;foreign.body.currentAction=&otherTask;keo_jobs::SteelStockRun(&foreign.stock,&foreign.body);keo_jobs::SteelStockRun(&otherTask,&foreign.body);C(!foreign.body.notifications&&!txCalls&&!resourceCalls,"OFF neither touches a replaced task nor completes foreign native242 body");
 Fixture cancel;cancelTx=true;keo_jobs::SteelStockRun(&cancel.stock,&cancel.body);C(txCalls==1&&!cancel.body.notifications,"world epoch change during native transfer stops and cannot complete stale body");
 Fixture disable;disableTx=true;keo_jobs::SteelStockRun(&disable.stock,&disable.body);C(txCalls==1&&disable.body.notifications==1&&disable.orders.permajobs.count==1,"live OFF stops further transfers and releases owned body with queue intact");
 Fixture partial;partialTx=true;keo_jobs::SteelStockRun(&partial.stock,&partial.body);C(txCalls==1&&partial.items[0].quantity==1&&partial.body.notifications==1,"truthful partial transfer is not retried as unchanged");
 Fixture fault;faultTx=true;keo_jobs::SteelStockRun(&fault.stock,&fault.body);C(txCalls==1&&fault.body.notifications==1&&!keo_jobs::SteelStockEligible(&fault.actor,&fault.stock),"transfer fault pauses stock and releases active body without removing queued job");++epoch;faultTx=false;C(keo_jobs::SteelStockEligible(&fault.actor,&fault.stock),"newworld releases numeric fault latch without dereferencing old items");
 Fixture retired;retireBody=&retired.body;retireTask=&retired.stock;int oldErrors=errors;keo_jobs::SteelStockRun(&retired.stock,&retired.body);C(txCalls==1&&!retired.body.notifications&&errors==oldErrors,"retired current task never touched after callback replaces active action");
 Fixture changed;Tasker other;changed.body.currentAction=&other;keo_jobs::SteelStockRun(&changed.stock,&changed.body);C(!txCalls&&!changed.body.notifications,"wrong active action never runs or completes");
 Fixture noheap;noheap.orders.permajobs.count=2;noheap.production.made=&noheap.data[0];int a=allocs;C(keo_jobs::SteelStockEligible(&noheap.actor,&noheap.stock)&&allocs==a,"291 product eligibility does no heap allocation");}
void FaultCleanup(){Fixture f;raiseResource=true;int made=setMade,destroyed=setDestroyed,a=allocs,d=frees;C(!keo_jobs::SteelStockEligible(&f.actor,&f.stock),"native collector SEH safely refuses eligibility");C(setMade==made+1&&setDestroyed==destroyed+1&&allocs-a==frees-d,"explicit finally cleans native temporary nodes/buckets under SEH");int calls=resourceCalls;C(!keo_jobs::SteelStockEligible(&f.actor,&f.stock)&&resourceCalls==calls,"faulted epoch makes no repeated collector calls");raiseResource=false;++epoch;C(keo_jobs::SteelStockEligible(&f.actor,&f.stock),"new world can collect again");}
int main(){Ownership();Collection();Running();FaultCleanup();printf("PASS %d actual native stock helper controls\n",checks);}
'''.replace('PRODUCTION',s)
p=O/'actual.cpp';p.write_text(code)
V=Path('C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC');S=Path('C:/Program Files/Microsoft SDKs/Windows/v7.1');e=dict(os.environ,INCLUDE=f'{R}/src;{V}/include;{S}/Include',LIB=f'{V}/lib/amd64;{S}/Lib/x64')
for opt in ('Od','O2'):
 label=('RED-owner-'if '--red-control'in sys.argv else 'RED-retired-'if '--retired-red'in sys.argv else '')+opt;exe=O/(label+'.exe');r=subprocess.run([str(V/'bin/amd64/cl.exe'),'/nologo','/EHsc','/'+opt,'/MT',str(p),str(R/'src/jobs/steel_jobs_policy.cpp'),'/Fo'+str(O)+'/', '/Fe'+str(exe)],env=e,capture_output=True,text=True);(O/('build-'+label+'.log')).write_text(r.stdout+r.stderr);assert not r.returncode,r.stdout+r.stderr;r=subprocess.run([str(exe)],capture_output=True,text=True);(O/('run-'+label+'.log')).write_text(r.stdout+r.stderr);print(label,r.stdout.strip())
 if '--red-control'in sys.argv:assert r.returncode==1 and'NPC shop cannot'in r.stdout
 elif '--retired-red'in sys.argv:assert r.returncode==1 and'retired current task never touched'in r.stdout
 else:assert not r.returncode,r.stdout+r.stderr
