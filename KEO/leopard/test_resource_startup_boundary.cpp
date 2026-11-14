#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <stdexcept>
#include <cstdio>
#define RESOURCE_STARTUP_BOUNDARY_TEST
static unsigned checks=0;static bool mainThread=true,stop=false;static DWORD now=1000;static unsigned ticks=0,observed=0,nativeCalls=0;static bool failNative=false;
#define C(x) do{++checks;if(!(x)){printf("FAIL %u %s\n",__LINE__,#x);return 1;}}while(0)
DWORD Clock(){return now;}
#define GetTickCount Clock
bool IsMainThread(){return mainThread;}
LONG g_navMeshStopSeen=0;
void ResourceCacheTick(){++ticks;}
namespace resource_cache{void RegisterTextureRoot(const std::string&p,bool recursive){if(p.find("texture")!=std::string::npos&&recursive)++observed;}}
namespace Ogre{typedef std::string String;typedef std::vector<String> StringVector;struct Archive{String getName()const{return "D:/texture";}String getType()const{return "FileSystem";}};struct Root{};
struct ResourceGroupManager{struct ResourceLocation{Archive*archive;bool recursive;};typedef std::list<ResourceLocation*> LocationList;static ResourceGroupManager*instance;LocationList locs;static ResourceGroupManager*getSingletonPtr(){return instance;}StringVector getResourceGroups(){StringVector v;v.push_back("General");v.push_back("Removed");v.push_back("Later");return v;}const LocationList&getResourceLocationList(const String&s){if(s=="Removed")throw std::runtime_error("removed");return locs;}};}
Ogre::ResourceGroupManager*Ogre::ResourceGroupManager::instance=NULL;
#include "../src/resources/resource_startup_native.cpp"
static void NativeAdd(Ogre::ResourceGroupManager*,const Ogre::String&p,const Ogre::String&t,const Ogre::String&g,bool r,bool ro){++nativeCalls;if(failNative)throw std::runtime_error("native");if(p!="D:/texture"||t!="FileSystem"||g!="General"||!r||!ro)throw std::runtime_error("args");}
static bool NativeFrame(Ogre::Root*){++nativeCalls;return true;}
static bool NativeTimed(Ogre::Root*,float dt){++nativeCalls;if(dt!=.25f)throw std::runtime_error("time ABI");return false;}
int main(){using namespace resource_startup_native;originalAdd=NativeAdd;originalFrame=NativeFrame;originalTimed=NativeTimed;
 Add(NULL,"D:/texture","FileSystem","General",true,true);C(nativeCalls==1&&observed==1);
 failNative=true;try{Add(NULL,"D:/texture","FileSystem","General",true,true);C(false);}catch(const std::runtime_error&){}C(nativeCalls==2&&observed==1);failNative=false;
 C(Frame(NULL));C(ticks==1);now+=100;C(Frame(NULL));C(ticks==1);now+=150;C(Frame(NULL));C(ticks==2);
 C(!Timed(NULL,.25f));C(ticks==2);now+=300;mainThread=false;C(Frame(NULL));C(ticks==2);mainThread=true;
 g_navMeshStopSeen=1;C(Frame(NULL));C(ticks==2);C(nativeCalls==8);
 g_navMeshStopSeen=0;Ogre::ResourceGroupManager m;Ogre::Archive archive;Ogre::ResourceGroupManager::ResourceLocation loc={&archive,true};
 for(int i=0;i<40;++i)m.locs.push_back(&loc);Ogre::ResourceGroupManager::instance=&m;
 now+=250;C(Frame(NULL));C(seedVisits==32&&seedOffset==32&&observed==33);
 now+=250;C(Frame(NULL));C(seedVisits==40&&seedGroup==1);
 now+=250;C(Frame(NULL));C(seedGroup==2);
 now+=250;C(Frame(NULL));C(seedVisits==72);
 now+=250;C(Frame(NULL));C(seedVisits==80&&seedGroups.empty());
 now+=250;C(Frame(NULL));C(seedVisits==80); // completed list is not restarted every frame
 // A large registered library has one global seed ceiling, never per-group.
 seeded=false;seedGroup=seedOffset=seedVisits=0;seedGroups.clear();m.locs.clear();
 for(int i=0;i<8200;++i)m.locs.push_back(&loc);
 for(int i=0;i<260;++i){now+=250;Frame(NULL);}
 C(seedVisits==8192&&seedGroups.empty());unsigned held=observed;
 now+=250;Frame(NULL);C(observed==held);
 printf("startup native wrapper %u checks PASS\n",checks);return 0;}
