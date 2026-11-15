#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include "movement/mover_policy.h"
#include "tracking_struct.inc"
static const int MAX_WATCHED=64,ZONE_GRID_MAX=63,OWNER_CHARACTER=1;
static const float EDGE_THRESHOLD=1800;
static const int ORDER_DX[9]={0},ORDER_DY[9]={0};
namespace movement {struct Config {bool playerCharRegistryEnabled;} g_movementCfg={true};}
namespace planner {bool PlannerRouteReplacesAhead(uintptr_t){return false;}}
static WatchedCharacter watchedChars[MAX_WATCHED];static int numWatched;
static uintptr_t live[64],playerSlot=1,pathPtr=1;static unsigned liveCount;
static float positions[64][2];static int enqueued,aheadCalls,reads,checks;
static bool validGrid=true,zonePresent=false;
static void Check(bool ok,const char*why){++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",why);exit(1);}}
static void*GameAddr(int){return &playerSlot;}
static const int RVA_GLOBAL_PLAYER=1,OFF_CMOV_HAVOK_CHAR=0;
#define KLIB_MEMBER(id,obj,name,off) ((uintptr_t)(obj))
#define KEO_DETAIL(x) ((void)0)
static bool DetailedLogEnabled(){return false;}
static unsigned GetPlayerCharCount(uintptr_t){return liveCount;}
static uintptr_t*GetPlayerCharStuff(uintptr_t){return live;}
static float GetCharPosX(uintptr_t id){++reads;return positions[id-1][0];}
static float GetCharPosZ(uintptr_t id){++reads;return positions[id-1][1];}
static bool WorldToZoneGrid(float x,float y,int*gx,int*gy){if(!validGrid)return false;*gx=int(x/10);*gy=int(y/10);return true;}
static void RemoveWatchedCharacter(int i){watchedChars[i]=watchedChars[--numWatched];}
static bool EnqueueCharacterZone(int,int){++enqueued;return true;}
static void EnqueueAheadZones(int,int,int,int,int){++aheadCalls;}
static void*GetZoneEntry(void*,int,int){return zonePresent?reinterpret_cast<void*>(1):NULL;}
static float GetZoneCenterX(void*){return 0;}
static float GetZoneCenterZ(void*){return 0;}
#include "tracking_functions.inc"
static void Reset(bool ordered,bool registry=true){
 numWatched=1;liveCount=1;live[0]=1;playerSlot=1;pathPtr=1;validGrid=true;zonePresent=false;
 enqueued=aheadCalls=reads=0;movement::g_movementCfg.playerCharRegistryEnabled=registry;
 WatchedCharacter c={};c.character=1;c.charMovement=reinterpret_cast<uintptr_t>(&pathPtr);
 c.hasMoveOrder=ordered;c.currentZoneX=2;c.currentZoneY=3;c.destZoneX=23;c.destZoneY=24;
 watchedChars[0]=c;positions[0][0]=230;positions[0][1]=240;
}
static void RetainsOnly(int x,int y){
 int gx[4]={-1},gy[4]={-1};bool moving[4]={true,true,true,true};
 int n=CollectMoverRetainZones(gx,gy,moving,4);
 Check(n==1&&gx[0]==x&&gy[0]==y&&!moving[0],"retention follows only current baseline cell");
}
int main(){
 Reset(false);PollActiveMovers(NULL,10);
 Check(watchedChars[0].currentZoneX==23&&watchedChars[0].currentZoneY==24,"baseline current cell follows actual position");
 Check(numWatched==1&&!watchedChars[0].hasMoveOrder&&enqueued==0&&aheadCalls==0,"baseline polling adds no move order or preload");RetainsOnly(23,24);
 for(int n=0;n<20;++n){positions[0][0]=float((25+n)*10);positions[0][1]=float((28+n)*10);PollActiveMovers(NULL,11+n);RetainsOnly(25+n,28+n);}
 Check(enqueued==0&&aheadCalls==0,"AI or follow movement never retains old cells or queues ahead");
 Reset(true);PollActiveMovers(NULL,40);Check(!watchedChars[0].hasMoveOrder&&numWatched==1,"arrived mover downgrades to baseline");RetainsOnly(23,24);Check(enqueued==0,"arrival remains an early return");
 Reset(true);watchedChars[0].destZoneX=29;watchedChars[0].destZoneY=28;PollActiveMovers(NULL,41);
 Check(watchedChars[0].currentZoneX==23&&watchedChars[0].currentZoneY==24&&watchedChars[0].hasMoveOrder,"active mover publishes current position");
 Check(enqueued==3&&aheadCalls==0,"active mover keeps exact existing directional preloads");
 Reset(true,false);PollActiveMovers(NULL,42);Check(numWatched==0&&enqueued==0,"registry off still removes arrived mover");
 Reset(false,false);PollActiveMovers(NULL,43);Check(numWatched==0&&enqueued==0,"legacy baseline lifetime unchanged");
 Reset(false);liveCount=0;PollActiveMovers(NULL,44);Check(numWatched==0&&reads==0,"removed character is rejected before position access");
 Reset(false);pathPtr=0;PollActiveMovers(NULL,45);Check(numWatched==0&&reads==0,"missing movement path is rejected before position access");
 Reset(false);validGrid=false;PollActiveMovers(NULL,46);Check(numWatched==0&&enqueued==0,"invalid coordinates removed without preload");
 printf("PASS %d actual tracking production controls\n",checks);return 0;
}
