// Optional once-per-world exterior rebuild requests. Discovery is bounded and
// deliberately excludes speculative preloads and retention-only town holds.
#include "base/log_admission.h"
#include "navmesh/cache/nm_auto_rebuild.h"
#include "navmesh/cache/nm_auto_rebuild_policy.h"
#include "navmesh/cache/nm_force_rebuild.h"
#include "navmesh/construction/wall_splice_internal.h"
#include "navmesh/jobs/nm_queue_lock.h"
#include "navmesh/nm_workers.h"
#include "zone/handoff/zone_handoff.h"
#include "zone/preload/preload_internal.h"
#include "game/native_profile.h"
#include "base/fixed_log_buf.h"

static NmAutoState s_auto;
// Binding lifetime is the process; request history is the loaded world.
static int s_binding = 0;
static navmesh::queuesAreClearMT_t s_physicsClear = NULL;
static const unsigned char kPhysicsHead[16] =
    { 0x40,0x53,0x48,0x83,0xEC,0x20,0x83,0xB9,0xB0,0x01,0x00,0x00,0x00,0x48,0x8B,0xD9 };

static bool Bind()
{
    if (s_binding) return s_binding > 0;
    const char* why = NULL;
    // Live setting: admit callees lazily on the main thread, without a new hook.
    if (GetModuleHandleW(L"KenshiQOL.dll")) why = "KenshiQOL already loaded";
    else if (!NativeProfileMatches(RVA_QUEUES_ARE_CLEAR_MT, GameAddr(RVA_QUEUES_ARE_CLEAR_MT), kPhysicsHead, 16))
        why = "physics queue binding refused";
    else if (NmForceRebuildBindQueue(&why))
    {
        s_physicsClear = (navmesh::queuesAreClearMT_t)GameAddr(RVA_QUEUES_ARE_CLEAR_MT);
        s_binding = 1;
        KEO_DETAIL("AutoNavmesh: ready; bounded exterior requests once per loaded world");
        return true;
    }
    s_binding = -1;
    LogError(std::string("AutoNavmesh: disabled (") + (why ? why : "queue binding refused") + ")");
    return false;
}

// Advisory idle snapshot. Same queue -> done lock order as the drain observer;
// release both before any section query or addJob. No queue traversal here.
static bool QueueIdle(uintptr_t nmg)
{
    // Native forwarded interior/splice jobs do not raise workerBusyCount.
    // Probe without waiting or holding the queue mutex; this is advisory,
    // not a fence against work arriving after either snapshot.
    if (!NavMeshWorkersIdle() || NavMeshTryLockProcessJobFor(0,NULL) != NM_PJLOCK_HELD)
        return false;
    NavMeshUnlockProcessJob();
    NmQueueLock q(nmg);
    if (!NavMeshWorkersIdle() || *(uintptr_t*)KLIB_MEMBER(4,nmg,NavMeshGenerator_queue_front,136))
        return false;
    char initBuf[16];
    void* t = game::g_gameFn.fn_pathBuilderInit(initBuf);
    game::g_gameFn.fn_pathBuilderFinalize((void*)KLIB_MEMBER(4,nmg,NavMeshGenerator_done_mutex,200),t);
    const bool idle = *(uintptr_t*)KLIB_MEMBER(4,nmg,NavMeshGenerator_done_front,184) == 0;
    game::g_gameFn.fn_readerUnlock((void*)KLIB_MEMBER(4,nmg,NavMeshGenerator_done_mutex,200));
    return idle;
}

static bool CheapCandidate(void* zone)
{
    if (!zone || IsZoneLoading(zone) || !IsZoneAccessible(zone)) return false;
    const float* countdown = (const float*)KLIB_MEMBER(2,(uintptr_t)zone,ZoneMap_activatedCountdown,OFF_ZONE_COUNTDOWNS);
    if (!NmAutoDemand(countdown[0],countdown[1])) return false;
    void* content = *(void**)KLIB_MEMBER(4,(uintptr_t)zone,ZoneMap_mapContent,0);
    if (!content || !*(uintptr_t*)KLIB_MEMBER(4,(uintptr_t)zone,ZoneMap_terrainCollision,OFF_ZONE_TERRAIN_COLLISION))
        return false;
    int ft = 0, loaded = 0, activation = 0;
    return ReadContentLifeFlags(content,&ft,&loaded,&activation) && loaded != 0;
}

void NmAutoRebuildOnWorldReset() { NmAutoReset(&s_auto); }

void NmAutoRebuildTick(void* zoneMgr, double now, bool saveLoading)
{
    if (!IsMainThread() || !navmesh::g_navmeshCfg.autoNavmeshRepairEnabled ||
        !navmesh::g_navmeshCfg.cachingEnabled) return;
    const bool worldReady = !saveLoading && zoneMgr && zoneMgr == g_cachedZoneMgr &&
        GetZoneState(zoneMgr) == 0 && navmesh::SpliceWorldOk() &&
        !ZoneHandoffAdoptedThisFrame();
    if (!NmAutoBeginScan(&s_auto,now,true,worldReady) || !Bind()) return;
    const navmesh::SpliceGateSnapshot gate = navmesh::SpliceGateTake(s_physicsClear);
    if (!gate.worldOk || gate.mainCount || gate.backCount || !gate.queuesClear) return;
    // Legacy sectionMgr name: this global is pauseState.navmesh, the same
    // pointer the native rebuild-key caller passes to NavMesh::generate.
    void* navMesh = (void*)gate.sectionMgr;
    const uintptr_t nmg = *(uintptr_t*)KLIB_MEMBER(4,(uintptr_t)navMesh,NavMesh_generator,OFF_MGR_NAVMESH_GEN);
    if (!nmg || nmg != g_navMeshGen || !QueueIdle(nmg)) return;
    for (int walked = 0; walked < NM_AUTO_SCAN_BUDGET; ++walked)
    {
        const int cell = NmAutoNextCell(&s_auto);
        if (NmAutoRequested(&s_auto,cell)) continue;
        const int x = cell / 64, y = cell % 64;
        void* zone = GetZoneEntry(zoneMgr,x,y);
        if (!CheapCandidate(zone)) continue;
        // At most one active-set walk and section query per tick; advancing
        // past rejected candidates keeps a speculative cell from starving all.
        if (!ZoneInSetB(zoneMgr,zone) || !navmesh::SpliceCellReady(gate,x,y)) return;
        NmRebuildCell input = { x,y };
        NmForceCellResult result = { 0 };
        if (NmForceRebuildCells(navMesh,&input,1,&result) == 1 &&
            (result.verdict == NM_FORCE_CELL_QUEUED || result.verdict == NM_FORCE_CELL_COVERED))
        {
            // Native admission/locks may have consumed time since this tick's
            // sample. Space from completion, never from a stale frame clock.
            NmAutoAccepted(&s_auto,cell,ElapsedSec());
            if (DetailedLogEnabled())
            {
                FixedLogBuf o; FlbInit(&o);
                FlbStr(&o,"AutoNavmesh: requested exterior cell (");
                FlbDec(&o,x); FlbChar(&o,','); FlbDec(&o,y); FlbChar(&o,')');
                KEO_DETAIL(FlbDone(&o));
            }
        }
        return;
    }
}
