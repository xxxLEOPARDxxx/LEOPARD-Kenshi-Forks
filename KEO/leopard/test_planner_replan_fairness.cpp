// Actual production PlannerTick and slot loop, extracted by the runner. Only
// external dependencies/slot effects are substituted; no mirrored scheduler.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdint.h>
namespace planner {
enum {PLANNER_OFF=0,PLANNER_ON=2};
struct PlayerChar {uintptr_t character,cm;};
static int PLAN_SLOTS=64;
static const int REPLANS_PER_TICK=4;
static const int PLAYER_SCAN_MAX=200;
static const double TICK_SECONDS=0.25;
static double s_lastTick=0;static unsigned s_storeGen=1;
static int s_replanCursor=0;
static double s_waitFeedAt[4];
static bool present[64],due[64],failure[64],paused=false;
static unsigned keys[64],visits[64],attempts[64],mode=PLANNER_ON,gen=1;
static unsigned resets=0,preFrames=0,snapshots=0,reports=0,water=0,merges=0;
static int playerCount=1;
static double BehaviorNow(double now){return now;}
static int PlanStoreMode(){return mode;}
static void PlannerPreArrivalFrame(){++preFrames;}
static unsigned CgStoreGen(){return gen;}
static void PlanStoreReset(){++resets;memset(present,0,sizeof(present));}
static bool ZonePauseIsPaused(){return paused;}
static void PlannerReportTick(double){++reports;}
static void TakeSnapshot(){++snapshots;}
static int ReadPlayers(PlayerChar*){return playerCount;}
static uintptr_t PlanStoreKey(int slot){++keys[slot];return present[slot]?slot+1:0;}
static bool TickSlot(int slot,const PlayerChar*,int,double,bool mayReplan){
 ++visits[slot];if(!mayReplan||!due[slot])return false;
 ++attempts[slot];if(!failure[slot])due[slot]=false;return true;
}
static void PlannerWaterRefresh(){++water;}
static void MergeTick(double){++merges;}
// PRODUCTION_HELPER
// PRODUCTION_TICK
}
using namespace planner;
static unsigned checks=0;static double wall=1;
#define C(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line%u %s\n",__LINE__,#x);return 1;}}while(0)
static void Step(){PlannerTick(NULL,wall);wall+=TICK_SECONDS;}
static void Reset(){
 memset(present,0,sizeof(present));memset(due,0,sizeof(due));memset(failure,0,sizeof(failure));
 memset(keys,0,sizeof(keys));memset(visits,0,sizeof(visits));memset(attempts,0,sizeof(attempts));
 PLAN_SLOTS=64;s_lastTick=0;s_replanCursor=0;s_storeGen=gen=1;mode=PLANNER_ON;paused=false;playerCount=1;wall=1;
 resets=preFrames=snapshots=reports=water=merges=0;
}
static unsigned Sum(const unsigned*a){unsigned n=0;for(int i=0;i<PLAN_SLOTS;++i)n+=a[i];return n;}
int main(){
 Reset();for(int i=0;i<5;++i){present[i]=due[i]=true;failure[i]=i<4;}
 Step();C(attempts[0]==1&&attempts[3]==1&&attempts[4]==0&&Sum(attempts)==4);
 Step();C(attempts[4]==1); // RED before fix: fixed-index low failures monopolize every tick.
 C(Sum(keys)==128&&Sum(visits)==10&&Sum(attempts)==8);
 C(!due[4]&&due[0]&&due[3]&&snapshots==2&&water==2&&merges==2);
 for(int i=0;i<64;++i)C(keys[i]==2); // still all slots, including the empty ones, exactly once/tick
 const int capacities[]={1,7,63,64};
 for(unsigned c=0;c<sizeof(capacities)/sizeof(*capacities);++c){
  Reset();PLAN_SLOTS=capacities[c];for(int i=0;i<PLAN_SLOTS;++i)present[i]=due[i]=failure[i]=true;
  int ticks=(PLAN_SLOTS+3)/4;for(int t=0;t<ticks;++t)Step();
  for(int i=0;i<PLAN_SLOTS;++i)C(attempts[i]>0&&keys[i]==unsigned(ticks)&&visits[i]==unsigned(ticks));
  C(Sum(attempts)==unsigned(ticks*(PLAN_SLOTS<4?PLAN_SLOTS:4))&&s_replanCursor>=0&&s_replanCursor<PLAN_SLOTS);
 }
 Reset();present[63]=due[63]=failure[63]=true;Step();C(attempts[63]==1&&s_replanCursor==0&&Sum(keys)==64);
 present[0]=due[0]=true;Step();C(attempts[0]==1&&attempts[63]==2&&Sum(visits)==3&&s_replanCursor==0);
#ifdef HAVE_PRODUCTION_HELPER
 Reset();for(int i=0;i<64;++i)present[i]=due[i]=failure[i]=true;
 s_replanCursor=31;TickPlanSlots(NULL,1,1,0);C(Sum(keys)==64&&Sum(visits)==64&&Sum(attempts)==0&&s_replanCursor==31);
#endif
 Reset();present[0]=due[0]=failure[0]=true;Step();unsigned prior=Sum(keys);int cursor=s_replanCursor;
 PlannerTick(NULL,wall-0.01);C(Sum(keys)==prior&&s_replanCursor==cursor); // <250ms
 mode=PLANNER_OFF;Step();C(Sum(keys)==prior&&s_replanCursor==cursor);
 mode=PLANNER_ON;paused=true;Step();C(Sum(keys)==prior&&s_replanCursor==cursor);
 paused=false;playerCount=-1;Step();C(Sum(keys)==prior&&s_replanCursor==cursor);
 playerCount=1;s_replanCursor=63;gen=2;s_waitFeedAt[0]=99.0;Step();C(resets==1&&s_storeGen==2&&s_replanCursor==0&&Sum(visits)==1);
 C(s_waitFeedAt[0]==0.0);
 s_replanCursor=41;gen=3;paused=true;Step();C(resets==2&&s_replanCursor==0);
 std::printf("PASS %u actual production planner scheduling controls (Debug-independent)\n",checks);return 0;
}
