from pathlib import Path
import os,subprocess
base=Path(__file__).resolve().parent/'_generated';base.mkdir(exist_ok=True)
src=Path(r'D:\DEV\Kenshi\forks\LEOPARD-Kenshi-Forks\BetterLooting\Source\BetterLooting.cpp').read_text(encoding='utf-8-sig')
def extract(name,structure=False):
 start=src.index(('struct '+name+'\n') if structure else name+'(');start=src.rfind('\n',0,start)+1;opening=src.index('{',start);depth=1;i=opening+1
 while depth:depth+=(src[i]=='{')-(src[i]=='}');i+=1
 return src[start:i+(1 if structure else 0)]
code=r'''
#include <cstdio>
#include <vector>
#include <set>
#include <functional>
#include <algorithm>
#include "D:/DEV/Kenshi/KenshiLib-src/Include/kenshi/Enums.h"
#include "D:/DEV/Kenshi/forks/LEOPARD-Kenshi-Forks/BetterLooting/Source/BetterLootingRules.h"
#include "D:/DEV/Kenshi/forks/LEOPARD-Kenshi-Forks/BetterLooting/Source/BetterLootingRequiredItems.h"
typedef unsigned int uint32_t;
namespace Ogre{struct Vector3{float x,y,z;Vector3(float a=0,float b=0,float c=0):x(a),y(b),z(c){}Vector3 operator-(const Vector3& v)const{return Vector3(x-v.x,y-v.y,z-v.z);}float squaredLength()const{return x*x+y*y+z*z;}};}
namespace MyGUI{struct InputManager{static bool focused;static InputManager*getInstancePtr(){static InputManager i;return &i;}void*getKeyFocusWidget(){return focused?(void*)1:NULL;}};bool InputManager::focused=false;}
class Item;class Character;class Building;class RootObject;
struct hand{RootObject*p;hand(RootObject*v=NULL):p(v){}bool isNull()const{return p==NULL;}bool operator<(const hand& h)const{return std::less<RootObject*>()(p,h.p);}RootObject*getRootObject()const;Item*getItem()const;Character*getCharacter()const;Building*getBuilding()const;};
struct GameData{std::string name,stringID;};struct Faction{bool player;Faction():player(false){}bool isThePlayer(){return player;}};
class RootObject{public:bool valid;Ogre::Vector3 pos;GameData*data;RootObject():valid(true),data(NULL){}virtual~RootObject(){}virtual itemType getDataType(){return ITEM;}virtual Faction*getFaction(){return NULL;}virtual bool typeIsAnItem(){return false;}bool isValid(){return valid;}Ogre::Vector3 getPosition(){return pos;}hand getHandle(){return hand(this);}GameData*getGameData(){return data;}std::string getName(){return data?data->name:std::string();}};
struct Inventory{int count;RootObject*owner;RootObject*callback;Inventory():count(0),owner(NULL),callback(NULL){}int getNumItems(){return count;}RootObject*getOwner(){return owner;}RootObject*getCallbackObject(){return callback;}};
class Building:public RootObject{public:bool player;Faction*faction;Building*parent;Building():player(false),faction(NULL),parent(NULL){}itemType getDataType(){return BUILDING;}Faction*getFaction(){return faction;}bool isThePlayer(){return player;}Building*furnitureParentBuilding(){return parent;}};
class Item:public RootObject{public:bool ground,isInInventory,isEquipped,stolen;Inventory*contents;hand owner,indoors;int value;float weight;itemType type;int function;Item():ground(true),isInInventory(false),isEquipped(false),stolen(false),contents(NULL),value(100),weight(1),type(ITEM),function(ITEM_NO_FUNCTION){}bool typeIsAnItem(){return true;}bool onGround(){return ground;}Inventory*getInventory(){return contents;}bool isStolen(bool){return stolen;}const hand&getProperOwner(){return owner;}const hand&isIndoors(){return indoors;}itemType getItemType(){return type;}int getValueSingle(bool){return value;}float getItemWeightSingle(){return weight;}};
class Character:public RootObject{public:std::vector<Item*>orders;bool badOrder,badFlags;Character():badOrder(false),badFlags(false){}bool isPlayerCharacter(){return true;}bool isDead(){return false;}bool isUnconcious(){return false;}bool checkPlayerOrderForProblems(TaskType,RootObject*){return badOrder;}void addOrder(Building*,TaskType t,RootObject*item,bool shift,bool clear,const Ogre::Vector3&){if(t!=PICKUP||shift||clear)badFlags=true;orders.push_back(static_cast<Item*>(item));}};
RootObject*hand::getRootObject()const{return p&&p->valid?p:NULL;}Item*hand::getItem()const{return dynamic_cast<Item*>(getRootObject());}Character*hand::getCharacter()const{return dynamic_cast<Character*>(getRootObject());}Building*hand::getBuilding()const{return dynamic_cast<Building*>(getRootObject());}
template<class T>struct lektor{std::vector<T>items;void clear(){items.clear();}uint32_t size()const{return (uint32_t)items.size();}T&operator[](uint32_t i){return items[i];}};
struct GameWorld{int queries,maxRequest;std::vector<RootObject*>world;GameWorld():queries(0),maxRequest(0){}void getObjectsWithinSphere(lektor<RootObject*>& out,const Ogre::Vector3&,float,itemType,int maximum,RootObject*){++queries;if(maximum>maxRequest)maxRequest=maximum;for(size_t i=0;i<world.size()&&(int)out.size()<maximum;++i)out.items.push_back(world[i]);}};
GameWorld game;GameWorld*ou=&game;BetterLootingRules g_betterLootingRules={};BetterLootingRequiredItems g_requiredLoot;bool g_betterLootingDebugLog=false;void BlTrace(const char*){}
int GetBetterLootingItemFunctionValue(Item*i){return i->function;}int GetBetterLootingEffectiveItemFunctionValue(Item*,int f){return f;}
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
'''
for n in ['IsBetterLootingRequiredItem','IsBetterLootingPlayerBuildingSource','IsBetterLootingBlockedLootInventory']:
 code+='\n'+extract(n)
code+='\n'+extract('BetterLootingGroundCandidate',True)
for n in ['BetterLootingGroundNeedsTheft','IsBetterLootingGroundItemAllowed','QueueBetterLootingGroundPickups']:
 code+='\n'+extract(n)
code+=r'''
int main(){Character actor;actor.orders.push_back(NULL);GameData data;data.name="Animal Skin";data.stringID="skin-id";Item item;item.data=&data;item.pos=Ogre::Vector3(10,0,0);game.world.push_back(&item);g_betterLootingRules.lootRadius=20;g_betterLootingRules.lootMaterials=true;g_betterLootingRules.lootOther=true;
CHECK(QueueBetterLootingGroundPickups(&actor)==0);CHECK(game.queries==0);CHECK(actor.orders.size()==1);
g_betterLootingRules.pickupGroundItems=true;g_betterLootingRules.ignorePlayerStorage=true;CHECK(IsBetterLootingGroundItemAllowed(&item,&actor));
RootObject foreign;item.owner=hand(&foreign);g_requiredLoot.Parse("id:skin-id");CHECK(!IsBetterLootingGroundItemAllowed(&item,&actor));item.owner=hand();item.isInInventory=true;CHECK(!IsBetterLootingGroundItemAllowed(&item,&actor));item.isInInventory=false;
Building chest,parent;chest.parent=&parent;parent.player=true;Inventory source;source.owner=&chest;CHECK(IsBetterLootingBlockedLootInventory(&source));item.owner=hand(&chest);CHECK(!IsBetterLootingGroundItemAllowed(&item,&actor));item.owner=hand();
Inventory contents;contents.count=1;item.contents=&contents;CHECK(!IsBetterLootingGroundItemAllowed(&item,&actor));item.contents=NULL;
item.pos=Ogre::Vector3(200.1f,0,0);CHECK(!IsBetterLootingGroundItemAllowed(&item,&actor));item.pos=Ogre::Vector3(200,0,0);CHECK(IsBetterLootingGroundItemAllowed(&item,&actor));
g_betterLootingRules.lootMaterials=false;g_betterLootingRules.minValue=1000;CHECK(IsBetterLootingGroundItemAllowed(&item,&actor));
CHECK(QueueBetterLootingGroundPickups(&actor)==1);CHECK(actor.orders.size()==2);CHECK(actor.orders[0]==NULL);CHECK(!actor.badFlags);CHECK(game.maxRequest<=64);CHECK(game.queries<=7);
game.world.clear();std::vector<Item> many(400);for(size_t i=0;i<many.size();++i){many[i].data=&data;many[i].pos=Ogre::Vector3(1,0,0);game.world.push_back(&many[i]);}game.queries=0;
const size_t before=actor.orders.size();CHECK(QueueBetterLootingGroundPickups(&actor)==32);CHECK(actor.orders.size()==before+32);CHECK(game.queries<=7);CHECK(game.maxRequest<=64);
actor.badOrder=true;CHECK(QueueBetterLootingGroundPickups(&actor)==0);CHECK(!actor.badFlags);
actor.badOrder=false;const int queries=game.queries;g_betterLootingRules.lootRadius=100000;CHECK(QueueBetterLootingGroundPickups(&actor)==0);CHECK(game.queries==queries);g_betterLootingRules.lootRadius=20;MyGUI::InputManager::focused=true;CHECK(QueueBetterLootingGroundPickups(&actor)==0);CHECK(game.queries==queries);
puts("PASS: ground off performs zero searches; exact whitelist cannot override theft/base/storage/nonempty-container/radius; repeated query hits deduplicate; max64 per query/max256 examined/max32 temporary orders; existing queue preserved and clear/shift false");return 0;}
'''
p=base/'ground_test.cpp';p.write_text(code,encoding='utf-8');vc=Path(r'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC');sdk=Path(r'C:\Program Files\Microsoft SDKs\Windows\v7.1');env=os.environ.copy();env['INCLUDE']=f'{vc/"include"};{sdk/"Include"}';env['LIB']=f'{vc/"lib/amd64"};{sdk/"Lib/x64"}';env['PATH']=str(vc/'bin/amd64')+';'+env['PATH']
r=subprocess.run([str(vc/'bin/amd64/cl.exe'),'/nologo','/EHsc','/MD','/O2','/wd4482',str(p),'/Fo'+str(base/'ground_test.obj'),'/Fe'+str(base/'ground_test.exe')],env=env,capture_output=True,text=True)
if r.returncode:print(r.stdout,r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(base/'ground_test.exe')],capture_output=True,text=True);print(r.stdout);raise SystemExit(r.returncode)
