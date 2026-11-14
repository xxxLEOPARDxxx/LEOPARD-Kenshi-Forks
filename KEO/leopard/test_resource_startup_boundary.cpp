#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <stdexcept>
#include <cstdio>
#define RESOURCE_STARTUP_BOUNDARY_TEST
static unsigned checks=0;static bool mainThread=true,stop=false;static DWORD now=1000;static unsigned ticks=0,observed=0,nativeCalls=0;static bool failNative=false;
#define C(x) do{++checks;if(!(x)){printf("FAIL %u %s\n",__LINE__,#x);return 1;}}while(0)
DWORD Clock(){return now;}
static LONGLONG preciseNow=1000000,qpcStep=0;LONGLONG QpcNow(){LONGLONG v=preciseNow;preciseNow+=qpcStep;return v;}long QpcDeltaUs(LONGLONG a,LONGLONG b){return b>a?long(b-a):0;}
#define GetTickCount Clock
bool IsMainThread(){return mainThread;}
LONG g_navMeshStopSeen=0;
void ResourceCacheTick(){++ticks;}
static bool menuLease=false,titleShown=false;static unsigned menuUpdates=0;
bool ResourceStartupTitleVisible(){return titleShown;}
static std::set<std::string>uniqueRoots;
namespace resource_cache{void RegisterTextureRoot(const std::string&p,bool recursive){if(p.find("texture")!=std::string::npos&&recursive){++observed;uniqueRoots.insert(p);}}bool StartupEnabled(){return true;}void SetStartupMenu(bool on){menuLease=on;++menuUpdates;}}
namespace Ogre{typedef std::string String;typedef std::vector<String> StringVector;struct Archive{String name;Archive():name("D:/texture"){}String getName()const{return name;}String getType()const{return "FileSystem";}};struct Root{};
struct ResourceGroupManager{struct ResourceLocation{Archive*archive;bool recursive;};typedef std::list<ResourceLocation*> LocationList;static ResourceGroupManager*instance;LocationList locs;StringVector customGroups;std::map<String,LocationList>customLists;unsigned groupReads;ResourceGroupManager():groupReads(0){}static ResourceGroupManager*getSingletonPtr(){return instance;}StringVector getResourceGroups(){if(!customGroups.empty())return customGroups;StringVector v;v.push_back("General");v.push_back("Removed");v.push_back("Later");return v;}const LocationList&getResourceLocationList(const String&s){++groupReads;if(s=="Removed")throw std::runtime_error("removed");return customGroups.empty()?locs:customLists[s];}};}
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
 now+=250;C(Frame(NULL));C(seedVisits==64&&seedGroup==2&&seedOffset==24);
 now+=250;C(Frame(NULL));C(seedVisits==80&&seedGroups.empty());
 now+=250;C(Frame(NULL));C(seedVisits==80);
 now+=250;C(Frame(NULL));C(seedVisits==80&&seedGroups.empty());
 now+=250;C(Frame(NULL));C(seedVisits==80); // completed list is not restarted every frame
 // A large registered library has one global seed ceiling, never per-group.
 seeded=false;seedGroup=seedOffset=seedVisits=0;seedGroups.clear();m.locs.clear();
 std::vector<Ogre::Archive>largeArchives(8200);std::vector<Ogre::ResourceGroupManager::ResourceLocation>largeLocations(8200);uniqueRoots.clear();
 for(int i=0;i<8200;++i){char name[48];sprintf_s(name,"D:/texture-%d",i);largeArchives[i].name=name;largeLocations[i].archive=&largeArchives[i];largeLocations[i].recursive=true;m.locs.push_back(&largeLocations[i]);}
 for(int i=0;i<260;++i){now+=250;Frame(NULL);}
 C(seedVisits==8192&&seedGroups.empty()&&uniqueRoots.size()==8192);unsigned held=observed;
 now+=250;Frame(NULL);C(observed==held);
 // Empty and missing groups must share one heartbeat, without increasing
 // the32-entry budget or walking every group in a single frame.
 seeded=false;seedGroup=seedOffset=seedVisits=0;seedGroups.clear();m.groupReads=0;
 for(int i=0;i<20;++i){char n[24];sprintf_s(n,"empty-%d",i);m.customGroups.push_back(n);}
 m.customGroups.push_back("payload");for(int i=0;i<24;++i)m.customLists["payload"].push_back(&loc);
 now+=250;Frame(NULL);C(seedVisits==0&&seedGroup==8&&m.groupReads==8);
 now+=250;Frame(NULL);C(seedVisits==0&&seedGroup==16&&m.groupReads==16);
 now+=250;Frame(NULL);C(seedVisits==24&&seedGroups.empty()&&m.groupReads==21);
 held=observed;now+=250;Frame(NULL);C(observed==held&&m.groupReads==21);
 seeded=false;seedGroup=seedOffset=seedVisits=0;seedGroups.clear();m.customGroups.clear();m.customLists.clear();
 for(int i=0;i<3;++i){char n[24];sprintf_s(n,"small-%d",i);m.customGroups.push_back(n);for(int k=0;k<20;++k)m.customLists[n].push_back(&loc);}
 now+=250;Frame(NULL);C(seedVisits==32&&seedGroup==1&&seedOffset==12);
 now+=250;Frame(NULL);C(seedVisits==60&&seedGroups.empty());
 // The soft deadline stops between groups; the current list's numeric
 // offset still advances normally, including large groups above128entries.
 seeded=false;seedGroup=seedOffset=seedVisits=0;seedGroups.clear();m.customGroups.clear();m.customLists.clear();
 for(int i=0;i<12;++i){char n[24];sprintf_s(n,"slow-empty-%d",i);m.customGroups.push_back(n);}
 qpcStep=750;now+=250;Frame(NULL);C(seedGroup==2&&seedVisits==0);qpcStep=0;
 seeded=false;seedGroup=seedOffset=seedVisits=0;seedGroups.clear();m.customGroups.clear();m.customLists.clear();
 m.customGroups.push_back("slow-records");for(int i=0;i<200;++i)m.customLists["slow-records"].push_back(&loc);
 qpcStep=750;now+=250;Frame(NULL);C(seedVisits==2&&seedOffset==2);
 now+=250;Frame(NULL);C(seedVisits==4&&seedOffset==4);qpcStep=0;
 // Menu metadata can advance between the250ms cache ticks, with the same
 // per-slice entry cap; duplicate nested heartbeat calls cannot double it.
 seeded=false;seedGroup=seedOffset=seedVisits=0;seedGroups.clear();m.customGroups.clear();m.customLists.clear();
 m.customGroups.push_back("menu-library");for(int i=0;i<65;++i)m.customLists["menu-library"].push_back(&loc);
 titleShown=true;menuFrameSeen=false;menuSlow=false;menuSeedSeen=false;now+=1000;preciseNow+=1000000;
 Frame(NULL);C(seedVisits==32);unsigned menuTickBefore=ticks;
 Frame(NULL);C(seedVisits==32&&ticks==menuTickBefore);
 now+=16;preciseNow+=16000;Frame(NULL);C(seedVisits==64&&ticks==menuTickBefore);
 now+=16;preciseNow+=16000;Frame(NULL);C(seedVisits==65&&seedGroups.empty()&&ticks==menuTickBefore);
 titleShown=false;
 menuFrameSeen=false;titleShown=true;now+=1000;C(Frame(NULL));C(menuLease);unsigned tickBefore=ticks;
 now+=5;preciseNow+=5000;C(Frame(NULL));C(menuLease&&ticks==tickBefore);titleShown=false;now+=5;preciseNow+=5000;C(Frame(NULL));C(!menuLease&&ticks==tickBefore);
 titleShown=true;now+=5;preciseNow+=5000;C(Frame(NULL));C(menuLease);
 now+=25;preciseNow+=25000;C(Frame(NULL));C(!menuLease);for(int i=0;i<201;++i){now+=5;preciseNow+=5000;Frame(NULL);}C(menuLease);
 menuFrameSeen=false;menuSlow=false;unsigned coarseStart=now;LONGLONG qpcStart=preciseNow;
 for(int i=0;i<600;++i){preciseNow=qpcStart+LONGLONG(i)*16667;now=coarseStart+DWORD((i*16667/15625)*15625/1000);C(Frame(NULL));C(menuLease);}
 g_navMeshStopSeen=1;C(Frame(NULL));C(!menuLease);
 printf("startup native wrapper %u checks PASS\n",checks);return 0;}
