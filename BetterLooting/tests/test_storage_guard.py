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
#include <vector>
enum itemType{BUILDING,CHARACTER,ITEM};enum BuildingFunction{BF_GENERAL_STORAGE=1,BF_RESOURCE_STORAGE=2};
struct Faction{bool player;Faction():player(false){}bool isThePlayer(){return player;}};
static int ownerReads=0, transferStarts=0, evaluations=0;
struct Inventory;struct RootObject{virtual~RootObject(){}virtual itemType getDataType(){return ITEM;}virtual Faction*getFaction(){return NULL;}};
struct Building:RootObject{bool valid,player;Building*parent;Faction*faction;Inventory*inventory;Building():valid(true),player(false),parent(NULL),faction(NULL),inventory(NULL){}bool isValid(){return valid;}bool isThePlayer(){return player;}Faction*getFaction(){return faction;}Building*furnitureParentBuilding(){return parent;}itemType getDataType(){return BUILDING;}BuildingFunction getSpecialFunction(){return BF_GENERAL_STORAGE;}Inventory*getInventory(){return inventory;}};
struct Character;
struct hand {
    Character* carried;
    Character* getCharacter() const { return carried; }
};
struct Character : RootObject {
    bool player, dead, unconscious, carried, isCarryingSomething;
    Inventory* inventory;
    hand carrying;
    Character() : player(false), dead(false), unconscious(false), carried(false),
                  isCarryingSomething(false), inventory(NULL) { carrying.carried = NULL; }
    itemType getDataType() { return CHARACTER; }
    bool isPlayerCharacter() const { return player; }
    bool isDead() const { return dead; }
    bool isUnconcious() const { return unconscious; }
    bool isBeingCarried() const { return carried; }
    Inventory* getInventory() { return inventory; }
    hand getCarryingObject() const { return carrying; }
};
template<typename T> struct lektor : std::vector<T> {};
struct PlayerInterface {
    lektor<Character*> members;
    int reads;
    PlayerInterface() : reads(0) {}
    const lektor<Character*>& getAllPlayerCharacters() { ++reads; return members; }
};
struct World { PlayerInterface* player; };
static World* ou = NULL;
struct Inventory{RootObject*owner;RootObject*callback;Inventory():owner(NULL),callback(NULL){}int getNumItems(){return 3;}RootObject*getOwner(){++ownerReads;return owner;}RootObject*getCallbackObject(){++ownerReads;return callback;}};
struct Rules{bool ignorePlayerStorage;Rules():ignorePlayerStorage(true){}}g_betterLootingRules;
struct InventoryGUI{};struct BetterLootingDestinations{Inventory* first;};struct BetterLootingCandidate{void*item;};bool g_betterLootingTransferFault=false;void BlTrace(const char*){}
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
'''
for name in [
    'IsBetterLootingPlayerBuildingSource',
    'IsBetterLootingPlayerCarriedCharacter',
    'IsBetterLootingBlockedLootSource',
    'IsBetterLootingBlockedLootInventory',
    'IsBetterLootingCharacterTarget',
]:
 if name+'(' in src:code+='\n'+function(name)
code+='\n'+function('IsBetterLootingBuildingTarget')
code+='\n'+function('ExecuteLootCandidate').split('    GameData* itemData =')[0]+'    ++transferStarts;\n}\n'
code+='\n'+function('EvaluateTargetInventory').split('    const lektor<InventorySection*>& sections =')[0]+'    ++evaluations;\n}\n'
code+=r'''
int main(){Faction player;player.player=true;Inventory inventory;Building chest;chest.player=true;chest.faction=&player;chest.inventory=&inventory;inventory.owner=&chest;
CHECK(!IsBetterLootingBuildingTarget(&chest));g_betterLootingRules.ignorePlayerStorage=false;CHECK(IsBetterLootingBuildingTarget(&chest));
g_betterLootingRules.ignorePlayerStorage=true;BetterLootingDestinations destination; destination.first=&inventory;BetterLootingCandidate candidate;candidate.item=(void*)1;g_betterLootingTransferFault=true;ExecuteLootCandidate((Inventory*)1,destination,candidate,0);EvaluateTargetInventory((Inventory*)1,destination);CHECK(ownerReads==0);g_betterLootingTransferFault=false;ExecuteLootCandidate(NULL,destination,candidate,0);EvaluateTargetInventory(NULL,destination);CHECK(ownerReads==0);ExecuteLootCandidate(&inventory,destination,candidate,0);CHECK(ownerReads>0);

Building furniture;
furniture.parent = &chest;
Inventory nested;
nested.owner = &furniture;
CHECK(IsBetterLootingBlockedLootInventory(&nested));

PlayerInterface playerInterface;
World world = { &playerInterface };
ou = &world;
Character collector, teammate, foreignCarrier, bounty;
collector.player = teammate.player = true;
playerInterface.members.push_back(&collector);
playerInterface.members.push_back(NULL);
playerInterface.members.push_back(&teammate);
Inventory body;
body.owner = &bounty;
body.callback = &bounty;
bounty.inventory = &body;
bounty.unconscious = true;
CHECK(IsBetterLootingCharacterTarget(&bounty));
CHECK(!IsBetterLootingBlockedLootInventory(&body));
CHECK(playerInterface.reads == 0);

bounty.carried = true;
teammate.isCarryingSomething = true;
teammate.carrying.carried = &bounty;
CHECK(!IsBetterLootingCharacterTarget(&bounty));
CHECK(IsBetterLootingBlockedLootInventory(&body));
const int previousTransfers = transferStarts, previousEvaluations = evaluations;
ExecuteLootCandidate(&body, destination, candidate, 0);
EvaluateTargetInventory(&body, destination);
CHECK(transferStarts == previousTransfers && evaluations == previousEvaluations);
g_betterLootingRules.ignorePlayerStorage = false;
CHECK(IsBetterLootingBlockedLootInventory(&body));
body.callback = NULL;
CHECK(IsBetterLootingBlockedLootInventory(&body));
body.callback = &bounty;
body.owner = NULL;
CHECK(IsBetterLootingBlockedLootInventory(&body));
bounty.dead = true;
bounty.unconscious = false;
CHECK(!IsBetterLootingCharacterTarget(&bounty));
CHECK(IsBetterLootingBlockedLootInventory(&body));

teammate.isCarryingSomething = false;
foreignCarrier.isCarryingSomething = true;
foreignCarrier.carrying.carried = &bounty;
CHECK(IsBetterLootingCharacterTarget(&bounty));
CHECK(!IsBetterLootingBlockedLootInventory(&body));

collector.isCarryingSomething = true;
collector.carrying.carried = &bounty;
CHECK(!IsBetterLootingCharacterTarget(&bounty));
CHECK(IsBetterLootingBlockedLootInventory(&body));
bounty.carried = false;
CHECK(IsBetterLootingCharacterTarget(&bounty));
CHECK(!IsBetterLootingBlockedLootInventory(&body));
ExecuteLootCandidate(&body, destination, candidate, 0);
EvaluateTargetInventory(&body, destination);
CHECK(transferStarts == previousTransfers + 1 && evaluations == previousEvaluations + 1);
bounty.carried = true;
ou = NULL;
CHECK(!IsBetterLootingCharacterTarget(&bounty));
CHECK(IsBetterLootingBlockedLootInventory(&body));
puts("PASS: player storage and carried-body protections cover live targets and inventory sources");
return 0;}

'''
p=base/'storage_guard_test.cpp';p.write_text(code,encoding='utf-8');vc=Path(r'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC');sdk=Path(r'C:\Program Files\Microsoft SDKs\Windows\v7.1');env=os.environ.copy();env['INCLUDE']=f'{vc/"include"};{sdk/"Include"}';env['LIB']=f'{vc/"lib/amd64"};{sdk/"Lib/x64"}';env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
r=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2',str(p),'/Fo'+str(base/'storage_guard_test.obj'),'/Fe'+str(base/'storage_guard_test.exe')],env=env,capture_output=True,text=True)
if r.returncode:print(r.stdout,r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(base/'storage_guard_test.exe')],capture_output=True,text=True);print(r.stdout);raise SystemExit(r.returncode)
