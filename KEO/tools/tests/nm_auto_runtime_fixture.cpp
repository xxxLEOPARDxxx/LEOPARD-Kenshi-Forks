// The verifier includes the actual production translation unit after replacing
// ONLY its dependency includes with these fake native boundaries. No game runs.
#include <windows.h>
#include <stdint.h>
#include <string>
#include <cstring>
#include "check.h"
#include "base/fixed_log_buf.h"
#include "navmesh/cache/nm_auto_rebuild_policy.h"
#include "navmesh/cache/nm_force_rebuild_policy.h"
#define KLIB_MEMBER(stage,p,field,offset) ((uintptr_t)(p)+(offset))
enum { OFF_ZONE_COUNTDOWNS=192, OFF_ZONE_TERRAIN_COLLISION=184, OFF_MGR_NAVMESH_GEN=656, RVA_QUEUES_ARE_CLEAR_MT=1 };
static unsigned char zoneData[300],contentData[300],meshData[800],generatorData[300];
static void* g_cachedZoneMgr=(void*)1234;
static uintptr_t g_navMeshGen=(uintptr_t)generatorData;
static bool isMain=true,world=true,adopt=false,physics=true,setB=true,ready=true,loaded=true;
static bool accessible=true,loading=false,workers=true,pj=true,bindingAllowed=true,conflict=false,detailed=false;
static int phase=0,queried=0,requested=0,walked=0,errors=0,logs=0,bindCalls=0,physicsCalls=0,pjLocks=0,queueLocks=0,doneLocks=0;
static int forceVerdict=1;
static double clockNow=0,forceSeconds=0;
static double ElapsedSec(){return clockNow;}
namespace navmesh {
struct Config { bool autoNavmeshRepairEnabled,cachingEnabled; } g_navmeshCfg={false,true};
typedef bool (*queuesAreClearMT_t)(void*);
struct SpliceGateSnapshot { int mainCount,backCount;bool queuesClear,worldOk;void*zoneMgr;uintptr_t sectionMgr; };
static SpliceGateSnapshot snapshot={0,0,true,true,(void*)1234,(uintptr_t)meshData};
bool SpliceWorldOk() { return world; }
SpliceGateSnapshot SpliceGateTake(queuesAreClearMT_t fn) { ++physicsCalls;Check(fn!=NULL,"physics binding before call");return snapshot; }
bool SpliceCellReady(const SpliceGateSnapshot&,int,int) { ++queried;return ready; }
}
static bool PhysicsClear(void*){return physics;}
static uintptr_t GameAddr(int){return (uintptr_t)&PhysicsClear;}
static bool NativeProfileMatches(int,uintptr_t,const unsigned char*,int){++bindCalls;return bindingAllowed;}
// Avoid replacing the actual Windows API declaration in windows.h.
static HMODULE FakeGetModuleHandleW(const wchar_t*){return conflict?(HMODULE)1:NULL;}
#define GetModuleHandleW FakeGetModuleHandleW
static bool IsMainThread(){return isMain;}
static bool DetailedLogEnabled(){return detailed;}
static void Detail(const std::string&){++logs;}
#define KEO_DETAIL(x) do { if (detailed) Detail(x); } while (0)
static void LogError(const std::string&){++errors;}
static bool NmForceRebuildBindQueue(const char**why){if(!bindingAllowed)*why="fake bindingAllowed refusal";return bindingAllowed;}
static bool NavMeshWorkersIdle(){return workers;}
enum NavMeshPjLockResult { NM_PJLOCK_NONE,NM_PJLOCK_HELD,NM_PJLOCK_TIMEOUT };
static NavMeshPjLockResult NavMeshTryLockProcessJobFor(DWORD ms,DWORD*){Check(ms==0,"never waits for processJob lock");if(pj){++pjLocks;return NM_PJLOCK_HELD;}return NM_PJLOCK_TIMEOUT;}
static void NavMeshUnlockProcessJob(){--pjLocks;}
struct NmQueueLock { NmQueueLock(uintptr_t){Check(pjLocks==0,"release PJ before queue");++queueLocks;}~NmQueueLock(){--queueLocks;} };
namespace game {
static void* Init(void*){return NULL;}
static void Finalize(void*,void*){Check(queueLocks==1,"queue before done");++doneLocks;}
static void Unlock(void*){--doneLocks;}
struct Fn {void*(*fn_pathBuilderInit)(void*);void(*fn_pathBuilderFinalize)(void*,void*);void(*fn_readerUnlock)(void*); } g_gameFn={Init,Finalize,Unlock};
}
static int GetZoneState(void*){return phase;}
static bool ZoneHandoffAdoptedThisFrame(){return adopt;}
static void* GetZoneEntry(void*,int x,int y){return x==0&&y==0?zoneData:NULL;}
static bool IsZoneLoading(void*){return loading;}
static bool IsZoneAccessible(void*){return accessible;}
static bool ReadContentLifeFlags(void*,int*ft,int*ld,int*act){*ft=0;*ld=loaded;*act=1;return true;}
static bool ZoneInSetB(void*,void*){++walked;return setB;}
enum { NM_FORCE_CELL_NONE,NM_FORCE_CELL_QUEUED,NM_FORCE_CELL_COVERED };
struct NmForceCellResult {void*zone;bool haveZone,terrain;int skip,verdict;};
static int NmForceRebuildCells(void*mesh,const NmRebuildCell*c,int n,NmForceCellResult*out){Check(mesh==meshData&&c->gx==0&&c->gy==0&&n==1,"one cell and native navmesh identity");Check(queueLocks==0&&doneLocks==0&&pjLocks==0,"no locks held when requesting");++requested;clockNow+=forceSeconds;out->verdict=forceVerdict;return forceVerdict?1:0;}
#include "nm_auto_runtime.inc"

static void Reset()
{
    NmAutoRebuildOnWorldReset();s_binding=0;s_physicsClear=NULL;
    memset(zoneData,0,sizeof(zoneData));memset(generatorData,0,sizeof(generatorData));
    *(void**)zoneData=contentData;*(uintptr_t*)(zoneData+184)=5678;
    *(float*)(zoneData+192)=1;*(uintptr_t*)(meshData+656)=g_navMeshGen;
    isMain=world=physics=setB=ready=loaded=accessible=workers=pj=bindingAllowed=true;
    adopt=loading=conflict=detailed=false;phase=queried=requested=walked=errors=logs=bindCalls=physicsCalls=0;
    pjLocks=queueLocks=doneLocks=0;forceVerdict=1;clockNow=forceSeconds=0;
    navmesh::g_navmeshCfg.autoNavmeshRepairEnabled=true;navmesh::g_navmeshCfg.cachingEnabled=true;
    navmesh::snapshot.worldOk=navmesh::snapshot.queuesClear=true;navmesh::snapshot.mainCount=navmesh::snapshot.backCount=0;
}
static void Tick(){NmAutoRebuildTick(g_cachedZoneMgr,0,false);}
int main()
{
    Reset();navmesh::g_navmeshCfg.autoNavmeshRepairEnabled=false;Tick();Check(!physicsCalls&&!bindCalls&&!requested,"OFF has no native work");
    Reset();navmesh::g_navmeshCfg.cachingEnabled=false;Tick();Check(!bindCalls&&!requested,"cache unavailable stands down");
    Reset();isMain=false;Tick();Check(!bindCalls&&!requested,"non-main stands down");
    Reset();NmAutoRebuildTick(g_cachedZoneMgr,0,true);Check(!bindCalls&&!requested,"save load stands down");
    Reset();phase=1;Tick();Check(!bindCalls&&!requested,"loader active stands down");
    Reset();adopt=true;Tick();Check(!bindCalls&&!requested,"fresh adoption stands down");
    Reset();world=false;Tick();Check(!bindCalls&&!requested,"transition reset or stop stands down");
    Reset();bindingAllowed=false;Tick();NmAutoRebuildTick(g_cachedZoneMgr,1,false);Check(errors==1&&!requested,"unsupported binding error once");
    Reset();conflict=true;Tick();Check(errors==1&&!requested,"QOL duplicate refused");
    Reset();navmesh::snapshot.mainCount=1;Tick();Check(!requested&&!walked,"physics waits");
    Reset();workers=false;Tick();Check(!requested&&!walked,"worker claims wait");
    Reset();pj=false;Tick();Check(!requested&&!walked,"forwarded native jobs wait without blocking");
    Reset();*(uintptr_t*)(generatorData+136)=1;Tick();Check(!requested&&!queried&&!queueLocks,"input queue busy and lock released");
    Reset();*(uintptr_t*)(generatorData+184)=1;Tick();Check(!requested&&!queried&&!queueLocks&&!doneLocks,"done queue busy and both locks released");
    Reset();setB=false;Tick();Check(walked==1&&!queried&&!requested,"private preload excluded by ownership");
    Reset();*(float*)(zoneData+192)=0;*(float*)(zoneData+200)=100;Tick();Check(!walked&&!queried&&!requested,"town retention is not demand");
    Reset();loaded=false;Tick();Check(!walked&&!requested,"uninitialized content excluded");
    Reset();ready=false;Tick();Check(walked==1&&queried==1&&!requested&&!NmAutoRequested(&s_auto,0),"readiness refusal never marks requested");
    Reset();forceVerdict=0;Tick();Check(requested==1&&!NmAutoRequested(&s_auto,0),"failed scheduling retries on later sweep");
    Reset();Tick();Check(requested==1&&NmAutoRequested(&s_auto,0)&&logs==0,"request accepted once with detailed request log suppressed");
    for(int i=1;i<1000;++i)NmAutoRebuildTick(g_cachedZoneMgr,i*.25,false);
    Check(requested==1&&!queueLocks&&!doneLocks&&!pjLocks,"no duplicates across repeated scans; locks balanced");
    navmesh::g_navmeshCfg.autoNavmeshRepairEnabled=false;NmAutoRebuildTick(g_cachedZoneMgr,300,false);
    navmesh::g_navmeshCfg.autoNavmeshRepairEnabled=true;NmAutoRebuildTick(g_cachedZoneMgr,301,false);
    Check(requested==1,"off/on does not reset visit history");
    NmAutoRebuildOnWorldReset();Tick();Check(requested==2,"world reset permits new request");
    Reset();forceVerdict=NM_FORCE_CELL_COVERED;Tick();Check(NmAutoRequested(&s_auto,0),"covered request accepted");
    Reset();forceSeconds=10;Tick();Check(s_auto.nextRequest==15,"cooldown starts after slow native admission");
    Check(!NmAutoBeginScan(&s_auto,14.99,true,true),"next frame cannot burst after a slow call");
    return CheckExit("nm_auto_runtime_fixture");
}
