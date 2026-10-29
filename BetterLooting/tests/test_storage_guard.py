from pathlib import Path
import os,subprocess
base=Path(__file__).resolve().parent/'_generated';base.mkdir(exist_ok=True)
src=Path(r'D:\DEV\Kenshi\forks\LEOPARD-Kenshi-Forks\BetterLooting\Source\BetterLooting.cpp').read_text(encoding='utf-8-sig')
def function(name):
 start=src.index(name+'(')
 # Skip a forward declaration when it exists.
 while src.find(';',start)<src.find('{',start):start=src.index(name+'(',start+len(name)+1)
 start=src.rfind('\n',0,start)+1;opening=src.index('{',start);depth=1;i=opening+1
 while depth:depth+=(src[i]=='{')-(src[i]=='}');i+=1
 return src[start:i]
code=r'''
#include <cstdio>
enum itemType{BUILDING,CHARACTER,ITEM};enum BuildingFunction{BF_GENERAL_STORAGE=1,BF_RESOURCE_STORAGE=2};
struct Faction{bool player;Faction():player(false){}bool isThePlayer(){return player;}};
static int ownerReads=0;
struct Inventory;struct RootObject{virtual~RootObject(){}virtual itemType getDataType(){return ITEM;}virtual Faction*getFaction(){return NULL;}};
struct Building:RootObject{bool valid,player;Building*parent;Faction*faction;Inventory*inventory;Building():valid(true),player(false),parent(NULL),faction(NULL),inventory(NULL){}bool isValid(){return valid;}bool isThePlayer(){return player;}Faction*getFaction(){return faction;}Building*furnitureParentBuilding(){return parent;}itemType getDataType(){return BUILDING;}BuildingFunction getSpecialFunction(){return BF_GENERAL_STORAGE;}Inventory*getInventory(){return inventory;}};
struct Inventory{RootObject*owner;RootObject*callback;Inventory():owner(NULL),callback(NULL){}int getNumItems(){return 3;}RootObject*getOwner(){++ownerReads;return owner;}RootObject*getCallbackObject(){++ownerReads;return callback;}};
struct Rules{bool ignorePlayerStorage;Rules():ignorePlayerStorage(true){}}g_betterLootingRules;
struct InventoryGUI{};struct BetterLootingCandidate{void*item;};bool g_betterLootingTransferFault=false;void BlTrace(const char*){}
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
'''
for name in ['IsBetterLootingPlayerBuildingSource','IsBetterLootingBlockedLootInventory']:
 if name+'(' in src:code+='\n'+function(name)
code+='\n'+function('IsBetterLootingBuildingTarget')
code+='\n'+function('ExecuteLootCandidate').split('    GameData* itemData =')[0]+'}\n'
code+='\n'+function('EvaluateTargetInventory').split('    const lektor<InventorySection*>& sections =')[0]+'}\n'
code+=r'''
int main(){Faction player;player.player=true;Inventory inventory;Building chest;chest.player=true;chest.faction=&player;chest.inventory=&inventory;inventory.owner=&chest;
CHECK(!IsBetterLootingBuildingTarget(&chest));g_betterLootingRules.ignorePlayerStorage=false;CHECK(IsBetterLootingBuildingTarget(&chest));
g_betterLootingRules.ignorePlayerStorage=true;InventoryGUI destination;BetterLootingCandidate candidate;candidate.item=(void*)1;g_betterLootingTransferFault=true;ExecuteLootCandidate((Inventory*)1,&destination,candidate,0);EvaluateTargetInventory(NULL,(Inventory*)1,&destination);CHECK(ownerReads==0);g_betterLootingTransferFault=false;ExecuteLootCandidate(NULL,&destination,candidate,0);EvaluateTargetInventory(NULL,NULL,&destination);CHECK(ownerReads==0);ExecuteLootCandidate(&inventory,&destination,candidate,0);CHECK(ownerReads>0);
puts("PASS: protected player storage excluded from radial loot; live opt-out restores original storage target behavior");return 0;}
'''
p=base/'storage_guard_test.cpp';p.write_text(code,encoding='utf-8');vc=Path(r'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC');sdk=Path(r'C:\Program Files\Microsoft SDKs\Windows\v7.1');env=os.environ.copy();env['INCLUDE']=f'{vc/"include"};{sdk/"Include"}';env['LIB']=f'{vc/"lib/amd64"};{sdk/"Lib/x64"}';env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
r=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2',str(p),'/Fo'+str(base/'storage_guard_test.obj'),'/Fe'+str(base/'storage_guard_test.exe')],env=env,capture_output=True,text=True)
if r.returncode:print(r.stdout,r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(base/'storage_guard_test.exe')],capture_output=True,text=True);print(r.stdout);raise SystemExit(r.returncode)
