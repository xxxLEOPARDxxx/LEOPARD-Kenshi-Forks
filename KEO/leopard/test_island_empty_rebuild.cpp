#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <intrin.h>
static int checks=0;
#define C(x) do { ++checks; if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";exit(1);} }while(0)
const int ZONE_GRID_COUNT=4096,ZONE_NEIGHBOR_COUNT=4;
const int OFF_ZM_SET_B=0,OFF_SET_SIZE=0,OFF_SET_BUCKET_COUNT=8,OFF_SET_BUCKETS=16,OFF_SET_NODE_VALUE=8;
const int OFF_LEKTOR_COUNT=8,OFF_LEKTOR_CAPACITY=12,OFF_LEKTOR_DATA=0;
const int OFF_NAVMESH_ZONE_SIZE=0,OFF_ZONE_AABB_CENTER=0,OFF_ZONE_AABB_HALF=0,OFF_ZONE_CONTENT=0;
const int RVA_GLOBAL_SECTION_MGR=0;
#define KLIB_MEMBER(g,b,m,o) ((uintptr_t)(b)+(o))
#define KLIB_ZONE_NEIGHBOR(g,z,k) ((uintptr_t)(z)+16+8*(k))
struct Zone { void* content; unsigned char access; unsigned char pad[7]; uintptr_t neighbors[4]; int label; };
struct Set { unsigned long long size,bucketCount; uintptr_t buckets; };
struct Manager { Set set; Zone zones[4096]; };
struct Node { uintptr_t next,value; };
static bool debugOn=false;
static unsigned atCalls=0,accessCalls=0,indexCalls=0;
static bool DetailedLogEnabled(){return debugOn;}
#define KEO_DETAIL(x) do { (void)(x); }while(0)
static float zoneStepX=8192,zoneStepZ=8192;
static uintptr_t emptyGlobal=0;
static void* GameAddr(int){return &emptyGlobal;}
static float GetZoneCenterX(void*){return 0;}
static float GetZoneCenterZ(void*){return 0;}
namespace game { struct Fn { void (*fn_lektorReserve)(void*,unsigned); } g_gameFn={0}; }
static bool IslandHooksLive(){return true;}
namespace islands_detail {
static uintptr_t ZoneAt(uintptr_t zm,int idx){++atCalls;return (uintptr_t)&((Manager*)zm)->zones[idx];}
static bool ZoneAccess(uintptr_t z){++accessCalls;return ((Zone*)z)->access!=0;}
static int ZoneLabel(uintptr_t z){return ((Zone*)z)->label;}
static int ZoneIndexOf(uintptr_t zm,uintptr_t z){++indexCalls;uintptr_t b=(uintptr_t)&((Manager*)zm)->zones[0];return zm&&z>=b&&(z-b)%sizeof(Zone)==0&&(z-b)/sizeof(Zone)<4096?(int)((z-b)/sizeof(Zone)):-1;}
static int IdxGX(int i){return i/64;} static int IdxGY(int i){return i%64;}
}
// PRODUCTION
static Manager manager;
static void CheckEmpty(uintptr_t zm,unsigned gen)
{
 using namespace islands_detail;
 C(g_curValid&&g_curCompCount==0&&g_curModZones==0&&g_compOverflow==0);
 C(g_snapGen==gen);
 const IslandSnapshot& s=g_snap[g_activeSnap&1];
 C(s.valid==1&&s.zm==zm&&s.gen==gen&&s.compCount==0&&(s.seq&1)==0);
 for(int i=0;i<4096;++i) C(g_curComp[i]==-1&&s.comp[i]==-1&&g_curIsMod[i]==0);
 for(int i=0;i<64;++i) C(s.memberStart[i]==0&&s.memberCount[i]==0);
 int ca=99,cb=99,comp=99;uintptr_t outZm=0;unsigned short members[1]={123};
 C(SnapReadComps((uintptr_t)&manager.zones[0],(uintptr_t)&manager.zones[4095],&ca,&cb)&&ca==-1&&cb==-1);
 C(SnapCopyMembers((uintptr_t)&manager.zones[1],&comp,&outZm,members,1)==0&&comp==-1&&outZm==zm&&members[0]==123);
}
int main()
{
 using namespace islands_detail;
 uintptr_t zm=(uintptr_t)&manager;
 manager.zones[0].access=manager.zones[1].access=manager.zones[4095].access=1;
 manager.zones[0].neighbors[0]=(uintptr_t)&manager.zones[1];
 manager.zones[1].neighbors[0]=(uintptr_t)&manager.zones[0];
 manager.zones[0].label=manager.zones[1].label=7;
 Node nodes[3]={{(uintptr_t)&nodes[1],(uintptr_t)&manager.zones[0]},{(uintptr_t)&nodes[2],(uintptr_t)&manager.zones[1]},{0,(uintptr_t)&manager.zones[4095]}};
 uintptr_t buckets[2]={0,(uintptr_t)&nodes[0]};manager.set.size=3;manager.set.bucketCount=1;manager.set.buckets=(uintptr_t)buckets;
 WalkSetB(zm);unsigned sig=g_setBSig;C(g_setBCount==3&&g_setBAccessible==3);
 C(sig==(3u*0x9E3779B1u ^ 4096u*0x85EBCA6Bu ^ 3u*0xC2B2AE35u));
 // Startup without ResetBuilder must still publish a valid all-minus-one snapshot.
 unsigned gen=g_snapGen;g_builderZm=zm;atCalls=accessCalls=indexCalls=0;
 Rebuild(zm,1.0);C(atCalls==0&&accessCalls==0&&indexCalls==0);CheckEmpty(zm,gen+1);C(g_lastEligibility==1.0);
 gen=g_snapGen;LONG active=g_activeSnap;Rebuild(zm,1.25);C(g_snapGen==gen&&g_activeSnap==active&&g_lastEligibility==1.25);C(g_setBSig==sig);
 // A later walk/signature change must stay visible even with empty components.
 manager.zones[1].access=0;WalkSetB(zm);C(g_setBAccessible==2&&g_setBSig!=sig);sig=g_setBSig;
 atCalls=accessCalls=indexCalls=0;debugOn=true;Rebuild(zm,2.0);C(atCalls==0&&accessCalls==0&&indexCalls==0&&g_setBSig==sig);CheckEmpty(zm,gen);
 ResetBuilder();C(!g_curValid&&g_lastEligibility==-1&&g_setBCount==0&&!g_haveSig&&!g_rebuildRequested);
 C(g_snapGen==gen+1&&g_snap[g_activeSnap&1].valid==0&&g_snap[g_activeSnap&1].zm==0);
 for(int i=0;i<4096;++i) C(g_snap[g_activeSnap&1].comp[i]==-1);
 atCalls=accessCalls=indexCalls=0;Rebuild(zm,3.0);C(atCalls==0&&accessCalls==0&&indexCalls==0);CheckEmpty(zm,gen+2);
 unsigned short out[3]={0};WalkSetB(zm);C(RouterList(zm,(uintptr_t)&manager.zones[0],out,3)==2&&out[0]==0&&out[1]==1);
 ResetBuilder();Rebuild(zm,4.0);CheckEmpty(zm,gen+4);
 std::cout<<"PASS "<<checks<<" actual island rebuild/snapshot/SetB controls\n";
}
