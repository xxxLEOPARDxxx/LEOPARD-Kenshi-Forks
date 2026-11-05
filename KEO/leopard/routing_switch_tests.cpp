#include <Windows.h>
#include "base/config_values.h"
#include "plugin/hook_manifest_policy.h"
#include "pathfind/player_repath_tier_policy.h"
#include <cstdio>
#include <cstring>
#include <new>
#include <cstdlib>
static unsigned checks=0,logs=0,allocations=0;static bool track=false;
void*operator new(size_t n){if(track)++allocations;void*p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}void operator delete(void*p){std::free(p);}void*operator new[](size_t n){return ::operator new(n);}void operator delete[](void*p){::operator delete(p);}
static void Check(bool b,const char*s){++checks;if(!b){std::fprintf(stderr,"FAIL %s\n",s);throw 1;}}
static void Log(const std::string&){++logs;}
static void Defaults(){zone::g_zoneCfg=zone::kZoneDefaults;navmesh::g_navmeshCfg=navmesh::kNavMeshDefaults;pathfind::g_pathfindCfg=pathfind::kPathfindDefaults;movement::g_movementCfg=movement::kMovementDefaults;fixes::g_fixesCfg=fixes::kFixesDefaults;planner::g_plannerCfg=planner::kPlannerDefaults;keo_inventory::g_inventoryCfg=keo_inventory::kInventoryDefaults;keo_gui::g_guiCfg=keo_gui::kGuiDefaults;}
struct Snapshot{zone::ZoneConfig zone;navmesh::NavMeshConfig nav;pathfind::PathfindConfig path;movement::MovementConfig move;fixes::FixesConfig fix;planner::PlannerConfig planner;keo_inventory::InventoryConfig inv;keo_gui::GuiConfig gui;Snapshot():zone(zone::g_zoneCfg),nav(navmesh::g_navmeshCfg),path(pathfind::g_pathfindCfg),move(movement::g_movementCfg),fix(fixes::g_fixesCfg),planner(planner::g_plannerCfg),inv(keo_inventory::g_inventoryCfg),gui(keo_gui::g_guiCfg){}bool Same()const{return !std::memcmp(&zone,&zone::g_zoneCfg,sizeof(zone))&&!std::memcmp(&nav,&navmesh::g_navmeshCfg,sizeof(nav))&&!std::memcmp(&path,&pathfind::g_pathfindCfg,sizeof(path))&&!std::memcmp(&move,&movement::g_movementCfg,sizeof(move))&&!std::memcmp(&fix,&fixes::g_fixesCfg,sizeof(fix))&&!std::memcmp(&planner,&planner::g_plannerCfg,sizeof(planner))&&!std::memcmp(&inv,&keo_inventory::g_inventoryCfg,sizeof(inv))&&!std::memcmp(&gui,&keo_gui::g_guiCfg,sizeof(gui));}};
// Owned dependency seam for the actual retained connectivity-hook body. The
// game answer is the observation; no native game function is executed.
const unsigned PLAYER_FAIL_RING=8;enum{WAVE_NPC=0,WAVE_PLAYER=1,WAVE_UNK=2};
namespace pathfind{struct Failure{int goalX,goalZ;LONG status,cause,iterCount,valid;};struct Diag{LONG diagConnAttempts,diagConnFail,playerFailWriteIdx,diagConnRejectByGate[3],diagWaveStale;Failure playerFailRing[PLAYER_FAIL_RING];Diag(){std::memset(this,0,sizeof(*this));}}g_pathDiag;}
static char originalAnswer=0;static unsigned originalCalls=0,probes=0;static bool currentRequestIsPlayer=false;static int t_waveTag=0,t_waveConsumed=0;
static char Original(void*,unsigned,unsigned){++originalCalls;return originalAnswer;}
namespace game{struct Originals{char(*orig_csCheckFaceConn)(void*,unsigned,unsigned);Originals():orig_csCheckFaceConn(Original){}}g_hookOrig;}
static int ConnGateOwner(){return WAVE_PLAYER;}static void RecordConnProbe(unsigned,char){++probes;}
#include "connectivity.extract.h"
static void Disabled(){
 using namespace movement;using namespace pathfind;
 Check(!g_pathfindCfg.pathfindDiagEnabled,"master OFF must suppress route-changing path hooks");
 Check(g_pathfindCfg.clusterGraphBypassMode==CGB_ORIGINAL,"master OFF restores original connectivity verdict");
 Check(!g_pathfindCfg.playerRepathTierEnabled,"master OFF disables repath tier writes");
 Check(g_pathfindCfg.playerHierarchicalMode==AHIER_OFF&&!g_movementCfg.islandFixEnabled&&!g_movementCfg.groupCohesionEnabled&&!g_movementCfg.cfg_islandFarSpan&&!g_movementCfg.islandEdgeRingEnabled&&!g_movementCfg.islandDeletedReissueEnabled&&g_movementCfg.cfg_k7PostDeathHold==K7_HOLD_OFF&&!g_movementCfg.k7DestReadyGateEnabled&&!g_movementCfg.k7ArrivalTriggerEnabled&&!g_movementCfg.formationGatherPaceEnabled&&planner::g_plannerCfg.mode==planner::PLANNER_OFF&&!fixes::g_fixesCfg.clusterCrossCostOn,"all existing master routing overrides remain disabled");
 HookWantInputs in=HookWantInputsFromConfig();const HookWant route[]={HOOK_WANT_PATHFIND_DIAG,HOOK_WANT_FIND_PATH_FULL,HOOK_WANT_PATH_EXTRACT,HOOK_WANT_CLUSTER_CROSS_COST,HOOK_WANT_PLANNER,HOOK_WANT_GATHER_PACE,HOOK_WANT_PLANNER_PRE_ARRIVAL};
 for(unsigned i=0;i<sizeof(route)/sizeof(route[0]);++i)Check(!HookWantEval(route[i],in),"disabled routing hooks are never requested at startup");
 Check(HookWantEval(HOOK_WANT_GRAPH_HEURISTIC,in)==in.graphHeuristicGuard,"independent graph crash guard retains its own switch");
 Check(!PlayerRepathTierDecide(0,true,g_pathfindCfg.playerRepathTierEnabled).tier,"matched player rerequest cannot write a new tier");
 for(unsigned answer=0;answer<2;++answer){originalAnswer=char(answer);unsigned before=originalCalls;t_waveTag=0;Check(hook_csCheckFaceConn(NULL,17,23)==originalAnswer&&originalCalls==before+1&&!t_waveTag,"even retained partial connectivity hook calls original exactly once without bypass");}
}
int main(){try{
 Defaults();Snapshot on;movement::ApplyRoutingSwitch(&Log);Check(on.Same()&&!logs,"master ON preserves defaults and emits no override log");
 const ConfigKey*key=movement::g_movementConfigKeys;while(key->name&&std::strcmp(key->name,"keoRouting"))++key;Check(key->name&&key->kind==CK_BOOL&&!key->live,"routing master is startup-only, never a live settings write");
 for(int graph=CGB_BYPASS;graph<=CGB_PLAYER;++graph)for(int hierarchical=AHIER_OFF;hierarchical<=AHIER_ON;++hierarchical)for(unsigned diag=0;diag<2;++diag){
  Defaults();pathfind::g_pathfindCfg.clusterGraphBypassMode=graph;pathfind::g_pathfindCfg.playerHierarchicalMode=hierarchical;pathfind::g_pathfindCfg.pathfindDiagEnabled=diag!=0;
  Snapshot customOn;movement::ApplyRoutingSwitch(NULL);Check(customOn.Same(),"master ON preserves explicit individual overrides too");
  movement::g_movementCfg.routingEnabled=false;zone::ZoneConfig zone=zone::g_zoneCfg;navmesh::NavMeshConfig nav=navmesh::g_navmeshCfg;keo_inventory::InventoryConfig inv=keo_inventory::g_inventoryCfg;keo_gui::GuiConfig gui=keo_gui::g_guiCfg;
  const bool visitor=fixes::g_fixesCfg.graphVisitorGuardEnabled,expand=fixes::g_fixesCfg.graphExpandGuardEnabled;movement::ApplyRoutingSwitch(NULL);Disabled();
  Check(!std::memcmp(&zone,&zone::g_zoneCfg,sizeof(zone))&&!std::memcmp(&nav,&navmesh::g_navmeshCfg,sizeof(nav))&&!std::memcmp(&inv,&keo_inventory::g_inventoryCfg,sizeof(inv))&&!std::memcmp(&gui,&keo_gui::g_guiCfg,sizeof(gui))&&fixes::g_fixesCfg.graphVisitorGuardEnabled==visitor&&fixes::g_fixesCfg.graphExpandGuardEnabled==expand,"master leaves loading/navmesh/inventory/GUI/crash guards untouched");
  Snapshot off;movement::ApplyRoutingSwitch(NULL);Check(off.Same(),"OFF remains idempotent including partial path hook refusal");
 }
 Defaults();movement::g_movementCfg.routingEnabled=false;allocations=0;track=true;movement::ApplyRoutingSwitch(NULL);track=false;Check(!allocations,"startup scalar clamp without logger allocates no heap resources");
 movement::ApplyRoutingSwitch(&Log);Check(logs==1,"one startup override summary when caller requests logging");
 std::printf("actual routing switch %u checks PASS\n",checks);return 0;
}catch(...){return 1;}}

