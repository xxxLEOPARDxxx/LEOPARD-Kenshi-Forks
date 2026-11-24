#include "base/log_admission.h"
// hook_manifest.cpp — the build gate table and the per-row install state, both
// expanded from plugin/hook_manifest_rows.inc so they share one index, and
// InstallHooks, which runs every startup install in a fixed order.

#ifndef UNICODE
#define UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "fixes/world/destroy_list_defer.h"
#include "game/game.h"
#include "base/config.h"   // ZONEHAND_STEP: the rows are gated like their installs
#include "plugin/hook_manifest.h"
#include "plugin/independent_hook_batch.h"
#include "game/native_profile.h"
#include "render/module_hooks.h"
#include <windows.h>
#include <sstream>
#include <string>
#include <cstring>
#include "base/klib_include.h"
#include <core/Functions.h>
#include "base/klib_include_end.h"
#include "zone/transition_hook.h"
#include "zone/readiness/readiness_hook.h"
#include "zone/camera_zone_hook.h"
#include "zone/preload/preload_order.h"
#include "zone/preload/preload.h"
#include "zone/zone_pause.h"
#include "zone/handoff/zone_lifecycle_hooks.h"
#include "navmesh/nm_workers.h"
#include "navmesh/cache/nm_force_rebuild.h"
#include "navmesh/scheduling/nm_adjacency.h"
#include "navmesh/construction/wall_splice.h"
#include "navmesh/construction/splice_seam.h"
#include "bench/bench_runner.h"
#include "fixes/world/corpse_pin.h"
#include "fixes/world/blackboard_empty.h"
#include "fixes/world/skeleton_reserve.h"
#include "fixes/world/mesh_weights.h"
#include "fixes/world/bone_import.h"
#include "fixes/world/bone_index.h"
#include "fixes/world/edge_triangles.h"
#include "fixes/world/model_clone_reserve.h"
#include "fixes/world/material_reserve.h"
#include "fixes/world/equipment_field_patch.h"
#include "fixes/world/equipment_animation_patch.h"
#include "fixes/world/animation_dirty_patch.h"
#include "fixes/world/save_field_key_patch.h"
#include "fixes/world/save_read_buffer.h"
#include "fixes/world/save_transform_read.h"
#include "fixes/world/save_string_transfer.h"
#include "fixes/world/save_state_list_transfer.h"
#include "fixes/world/save_instance_lookup.h"
#include "fixes/world/morph_pose_scope.h"
#include "fixes/world/faction_relations.h"
#include "inventory/backpack_first.h"
#include "inventory/carried_weight.h"
#include "jobs/steel_jobs.h"
#include "fixes/world/town_claim.h"
#include "fixes/world/throwout.h"
#include "inventory/backpack_sidecar.h"
#include "inventory/backpack_food.h"
#include "inventory/operator_trips.h"
#include "inventory/job_counters.h"
#include "inventory/backpack_window.h"
#if ZONEHAND_STEP >= 2
#include "fixes/world/nest_validation.h"
#endif
#include "fixes/stitch/unstitch_guard.h"
#include "fixes/world/onscreen_stagger.h"
#include "fixes/world/paused_skip.h"
#include "fixes/stitch/stitch_source.h"
#include "fixes/search/graph_visitor_guard.h"
#include "fixes/search/graph_expand_guard.h"
#include "fixes/search/graph_position_guard.h"
#include "fixes/search/graph_heuristic_guard.h"
#include "fixes/streaming/create_instance_guard.h"
#include "fixes/physx/hull_queue_guard.h"
#include "fixes/physx/hull_same_skip.h"
#include "fixes/streaming/mesh_face_guard.h"
#include "fixes/streaming/navmesh_life.h"
#include "fixes/stitch/unstitch_probe.h"
#include "render/scene_levers.h"
#include "render/d3d_state_skip.h"
#include "fixes/streaming/section_key_probe.h"
#include "diag/physx_pool_probe.h"
#include "render/ogre_join_spin.h"

// `extern` is required: a const object at namespace scope has internal
// linkage in C++ without it, and plugin_entry.cpp needs this one.
extern const HookPrologue g_hookPrologues[] =
{
#define HOOK_ROW(id, name, rva, kind, installer, want, caps, b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15) \
	{ name, rva, { b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15 }, kind == HOOK_DIAGNOSTIC },
#include "plugin/hook_manifest_rows.inc"
#undef HOOK_ROW
};
static_assert(sizeof(g_hookPrologues) / sizeof(g_hookPrologues[0]) == HOOK_ROW_COUNT, "one gate row per manifest row");

extern const int g_hookPrologueCount = (int)(sizeof(g_hookPrologues) / sizeof(g_hookPrologues[0]));

static const HookWant s_wants[] =
{
#define HOOK_ROW(id, name, rva, kind, installer, want, ...) want,
#include "plugin/hook_manifest_rows.inc"
#undef HOOK_ROW
};
static_assert(sizeof(s_wants) / sizeof(s_wants[0]) == HOOK_ROW_COUNT, "one want per manifest row");

static volatile LONG s_installed[HOOK_ROW_COUNT] = { 0 };
// Written once per row by the startup gate on the main thread before any
// install; read by HookInstallRow on any thread.
static volatile LONG s_gatePassed[HOOK_ROW_COUNT] = { 0 };

static HookBatch s_independentBatch;
static bool s_independentPhase=false;
static int* s_independentCount=NULL; // bounded to InstallHooks stack lifetime
static HookInstallCompletion s_independentDone[HOOK_ROW_COUNT]={0};
static bool s_independentClaimed[HOOK_ROW_COUNT]={0};
static bool s_allowFurtherNativeBatches=true;

static bool IndependentRow(HookRowId id){
 return id==HOOK_CHARACTER_UPDATE_ONSCREEN_CHECK||id==HOOK_CHARACTER_PAUSED_UPDATE
     ||id==HOOK_ACTIVEPLATOON_CALC_POS||id==HOOK_FACTION_RELATIONS_UPDATE
     ||id==HOOK_GRAPH_POSITION_LOOKUP||id==HOOK_NAVMESH_FACE_AABB
     ||id==HOOK_SCORE_FIND_FOOD_ON_GROUND
     ||id==HOOK_RESET_UNLOAD_ZONES||id==HOOK_PROCESS_LOADING;
}

bool BeginNativeHookBatch(HookBatch& batch){
 // Public SDK wrappers choose different module IDs. Resolve the admitted
 // native exports directly and pass ONE caller ID to both operations.
 if(!s_allowFurtherNativeBatches||!NativeProfileVerifySdk())return false;
 HMODULE sdk=GetModuleHandleW(L"KenshiLib.dll"),caller=NULL;DWORD stamp=0,size=0;
 if(!ReadModuleImageId(sdk,&stamp,&size)||stamp!=0x6a8e4636u||size!=0xec000u)return false;
 void*q=(void*)GetProcAddress(sdk,"?QueueHookInternal@@YA?AW4HookStatus@KenshiLib@@_KPEAX1PEAPEAX@Z");
 void*a=(void*)GetProcAddress(sdk,"?ApplyQueuedHooksInternal@@YA?AW4HookStatus@KenshiLib@@_K@Z");
 static const unsigned char qb[16]={0x40,0x53,0x57,0x48,0x81,0xec,0xb8,0,0,0,0x48,0xc7,0x44,0x24,0x20,0xfe};
 static const unsigned char ab[16]={0x40,0x53,0x48,0x81,0xec,0xb0,0,0,0,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff};
 if(q!=(char*)sdk+0x2540||a!=(char*)sdk+0x26a0||memcmp(q,qb,16)||memcmp(a,ab,16))return false;
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
     (LPCWSTR)&BeginNativeHookBatch,&caller))return false;
 return batch.Begin((size_t)caller,(HookBatch::Queue)q,(HookBatch::Apply)a);
}

void FinishNativeHookBatch(HookBatch& batch){
 // A partial apply may leave pending rows under this DLL's owner ID. A
 // later batch must not let the SDK retry those unconfirmed installations.
 s_allowFurtherNativeBatches=false;
 s_allowFurtherNativeBatches=batch.Finish();
}

static bool BeginIndependentHooks(){
 return BeginNativeHookBatch(s_independentBatch);
}

static void IndependentComplete(int row,const char*why){
 if(!why){InterlockedExchange(&s_installed[row],1);if(s_independentCount)++*s_independentCount;}
 if(s_independentDone[row])s_independentDone[row](why);
}

struct IndependentStartupScope {
 explicit IndependentStartupScope(int*count){s_independentPhase=BeginIndependentHooks();s_independentCount=s_independentPhase?count:NULL;}
 ~IndependentStartupScope(){s_independentCount=NULL;s_independentPhase=false;}
 void Finish(){if(s_independentPhase)FinishNativeHookBatch(s_independentBatch);s_independentCount=NULL;s_independentPhase=false;}
private:
 IndependentStartupScope(const IndependentStartupScope&);IndependentStartupScope&operator=(const IndependentStartupScope&);
};

const char* HookInstallRow(HookRowId id, void* detour, void** orig, int* installed,
                           bool reverify)
{
	uintptr_t rva = g_hookPrologues[id].rva;
	bool reverifyOk = reverify && VerifyPrologueByRva(rva);
	if (HookInstallAdmit(reverify, reverifyOk,
	                     InterlockedCompareExchange(&s_gatePassed[id], 0, 0) != 0) != HOOK_ADMIT)
	{
		*orig = NULL;
		return "prologue";
	}
	if (KenshiLib::AddHook(GameAddr(rva), detour, orig) != KenshiLib::SUCCESS)
	{
		*orig = NULL;
		return "hook";
	}
	InterlockedExchange(&s_installed[id], 1);
	if (installed)
		++*installed;
	return NULL;
}

bool HookRowInstalled(HookRowId id)
{
	return InterlockedCompareExchange(&s_installed[id], 0, 0) != 0;
}

void HookInstallIndependentRow(HookRowId id,void*detour,void**orig,int*installed,bool verify,HookInstallCompletion done){
 if(!done)return;
 // Reject reentry BEFORE prologue reads, callback replacement or original
 // mutation: a previous partial apply may already dispatch this trampoline.
 if(!IndependentRow(id)){done("batch-row");return;}
 if(s_independentClaimed[id]||(s_independentPhase&&!s_independentBatch.Collecting())){done("batch-reentrant");return;}
 s_independentClaimed[id]=true;
 if(!s_independentPhase){
  done(HookInstallRow(id,detour,orig,installed,verify));return;
 }
 const uintptr_t rva=g_hookPrologues[id].rva;
 const bool fresh=verify&&VerifyPrologueByRva(rva);
 if(HookInstallAdmit(verify,fresh,InterlockedCompareExchange(&s_gatePassed[id],0,0)!=0)!=HOOK_ADMIT){*orig=NULL;done("prologue");return;}
 s_independentDone[id]=done;
 if(!s_independentBatch.Add(id,GameAddr(rva),detour,orig,IndependentComplete))done("batch-admission");
}

void HookRowNoteGateVerdict(HookRowId id, bool passed)
{
	InterlockedExchange(&s_gatePassed[id], passed ? 1 : 0);
}

HookWantInputs HookWantInputsNow()
{
	return HookWantInputsFromConfig();
}

bool HookRowWanted(HookRowId id)
{
	return HookWantEval(s_wants[id], HookWantInputsNow());
}


// =========================================================================
// The startup groups
// =========================================================================
//
// Main thread, startPlugin, once each, in kInstallSteps' order. A row whose
// want gates its install reads HookRowWanted at that moment, so a key an
// earlier failure cleared still skips the install; the banner's total is
// taken before any of them runs (InstallHooks).

static void InstallLoadingHooks(int* installed, int*)
{
	// Loading-message and readiness-bypass hooks
	if (HookInstall(HOOK_SHOW_LOADING_MESSAGE, hook_showLoadingMessage,
			&game::g_hookOrig.orig_showLoadingMessage, installed, false) != NULL)
		LogError("FAILED to hook showLoadingMessage");

	if (HookInstall(HOOK_IS_CONTENT_PENDING, hook_isContentPending,
			&game::g_hookOrig.orig_isContentPending, installed, false) != NULL)
	{
		LogError("FAILED to hook isContentPending");
		zone::g_zoneCfg.preloadEnabled = false;  // registration readiness requires isContentPending
	}

	// Camera-zone orchestrator hook
	if (HookInstall(HOOK_UPDATE_CAMERA_ZONE, hook_updateCameraZone,
			&game::g_hookOrig.orig_updateCameraZone, installed, false) != NULL)
		LogError("FAILED to hook updateCameraZone");
}

// GameWorld::destroyListOE inserter. Two jobs on one hook: the race
// mitigation (destroyListDefer — queue off-main inserts and replay them on
// the main thread, core.h) and the thread-id diagnostic (destroyListDiag).
// Either key alone justifies the install; the mitigation is on by default.
// VerifyPrologue before the patch, and the table row is fatal.
static void InstallDestroyListHook(int* installed, int*)
{
	SetDestroyListDefer(fixes::g_fixesCfg.destroyListDeferEnabled);
	if (HookRowWanted(HOOK_DESTROYLIST_INSERT))
	{
		if (HookInstall(HOOK_DESTROYLIST_INSERT, hook_destroyListInsert,
				&orig_destroyListInsert, installed, true) == NULL)
		{
			std::ostringstream dls;
			dls << "destroyListOE inserter hook installed: defer="
			    << (fixes::g_fixesCfg.destroyListDeferEnabled ? "on" : "off")
			    << " diag=" << (fixes::g_fixesCfg.destroyListDiagEnabled ? "on" : "off");
			KEO_DETAIL(dls.str());
		}
		else
		{
			orig_destroyListInsert = NULL;
			SetDestroyListDefer(false);
			LogError("FAILED to hook destroyListOE inserter (sub_140799BE0)");
		}
	}
}

// The save-load reset's Set A/B unload (sub_14036C1E0). The
// hook unloads the mod's zones the reset would otherwise leave alive with a
// wiped handle registry, then clears the
// mod's state there. Installed in every build: the zones it unloads exist
// only when the mod preloads, but with none it only scans and logs. The
// INI key saveLoadUnload chooses unload vs count-only, not the install.
// VerifyPrologue before the patch, and the table row is fatal.
static void ResetUnloadInstallComplete(const char* why)
{
	if (!why)
	{
		KEO_DETAIL(std::string("Save-load reset hook installed: saveLoadUnload=")
		       + (zone::g_zoneCfg.saveLoadUnloadEnabled ? "on" : "off"));
	}
	else
	{
		LogError(std::string("Save-load reset unload hook unconfirmed (") + why
		         + "); the ZM+8 reset edge and registry guard remain available");
	}
}

static void InstallResetUnloadHook(int* installed, int*)
{
	HookInstallIndependent(HOOK_RESET_UNLOAD_ZONES, hook_resetUnloadZones,
	                       &orig_resetUnloadZones, installed, true, ResetUnloadInstallComplete);
}

// Movement-order tracking hook
static void InstallOrderHook(int* installed, int*)
{
	if (HookRowWanted(HOOK_ADD_ORDER_SELECTED))
	{
		if (HookInstall(HOOK_ADD_ORDER_SELECTED, hook_addOrderSelected,
				&game::g_hookOrig.orig_addOrderSelected, installed, false) != NULL)
			LogError("FAILED to hook addOrderSelectedCharacters");
	}
}

static void OrderAbortStep(int*, int*)
{
	BenchRunnerSetOrderAbort(game::g_hookOrig.orig_addOrderSelected != NULL);
}

static void InstallCacheHook(int* installed, int*)
{
	if (HookRowWanted(HOOK_DISPATCH_JOB))
	{
		const char* cacheHookResult = HookInstall(HOOK_DISPATCH_JOB, hook_dispatchJob,
				&game::g_hookOrig.orig_dispatchJob, installed, false);
		if (cacheHookResult == NULL)
		{
			std::ostringstream ds;
			ds << "dispatchJob hook installed"
			   << " orig=" << (void*)game::g_hookOrig.orig_dispatchJob
			   << " detour=" << (void*)hook_dispatchJob;
			KEO_DETAIL(ds.str());
		}
		else
		{
			LogError("FAILED to hook dispatchJob");
			navmesh::g_navmeshCfg.cachingEnabled = false;
		}

		// The populate hook is no longer installed here. It
		// installs lazily beside the edgeProcess clone-guard on the first
		// dispatch (nm_lazy_hooks.cpp), where it captures each job's input
		// triangle count for the zero-face rule.
	}
}

// Group cohesion: binary-patch scatter branch
// These are independent read-only observers. Neither drives path priority,
// adjacency, extraction safety or game/world lifetime. OFF sessions need no
// detours; an explicit later log enable admits them from the owner heartbeat.
// Main-thread state; publish the attempt before AddHook can reenter reporting.
static bool s_sectionDiagnosticsInitializationReached = false;
static bool s_sectionDiagnosticsAttempted = false;

static void ObserveNavmeshInstance(void* collection, __int64 sectionData,
                                   __int64 param3, __int64 param4, int param5)
{
	game::g_hookOrig.orig_addInstance(collection, sectionData, param3, param4, param5);
	if (!DetailedLogEnabled()) return;
	NavMeshLifeOnAdd(collection, sectionData);
	StitchSourceOnAdd(collection, sectionData, param4);
}

static void InstallSectionDiagnosticHooks(int* installed, int*)
{
	s_sectionDiagnosticsInitializationReached = true;
	if (!DetailedLogEnabled() || s_sectionDiagnosticsAttempted) return;
	s_sectionDiagnosticsAttempted = true;
	InstallNavMeshLife(installed, NULL);
	if (HookRowWanted(HOOK_ADD_INSTANCE))
	{
		if (HookInstall(HOOK_ADD_INSTANCE, ObserveNavmeshInstance,
				&game::g_hookOrig.orig_addInstance, installed, true) == NULL)
		{
			StitchSourceNoteAddObserver();
			KEO_DETAIL("sectionStamp: addInstance count hook installed");
		}
		else
			LogError("FAILED to hook hkaiStreamingCollection::addInstance");
	}
	else
		KEO_DETAIL("addInstance not hooked: sectionStamp and navMeshLife are both off, "
		       "so neither the insert count nor the lifecycle rows exist this session");
}

void InstallSectionDiagnosticsOnDemand()
{
	if (s_sectionDiagnosticsInitializationReached && DetailedLogEnabled() && !s_sectionDiagnosticsAttempted)
		InstallSectionDiagnosticHooks(NULL, NULL);
}

static void (*const kInstallSteps[])(int*, int*) =
{
	InstallLoadingHooks,
	InstallDestroyListHook,
	InstallResetUnloadHook,

	// Present in every build: the fault it guards is measured, and with no
	// out-of-bounds record it calls the game's own walk unchanged.
	InstallUnstitchGuard,

	// Present in every build and whatever the guard's key says: a pass-through
	// on the stitch that records what it wrote, so the guard's drops can be
	// traced to their writer. stitchSourceLines sets verbosity only.
	InstallStitchSource,

	// Present in every build too, and for the same reason: the search fault it
	// prevents was recorded twice mid-session.
	InstallGraphVisitorGuard,

	// And its sibling: the node the guard above keeps in the search is
	// expanded through the cache slot beside the one it tests.
	InstallGraphExpandGuard,

	// A third copy of the same cache shape, on the cluster-graph search's own
	// node-position helper -- reached only when clusterGraphBypass actually
	// consults the graph, a call path neither guard above sits on.
	InstallGraphPositionGuard,

	// The hierarchical heuristic's three unchecked instance reads, only while wanted.
	InstallGraphHeuristicGuard,


	// And again: three records at one instruction, each with a valid key and a
	// valid slot, faulting one dereference below them.
	InstallMeshFaceGuard,

	// The add-list self-duplicate: createInstance frees a NavInstance that is
	// already queued and queues the freed pointer, and the drain reads it.
	InstallCreateInstanceGuard,

	// The hull destroy queue: an object queued twice is deleted twice by the
	// physics thread, the second time through freed memory.
	InstallHullQueueGuard,

	// The click-hull same-target skip swaps a verified slot for its live setting in every build.
	InstallHullSameSkip,

	// The OgreMain scene switches install for their live settings in every build; no manifest row.
	InstallSceneForkSkip,
	InstallInstEmptySkip,

	// The D3D11 state-object switch: installed whatever its
	// live key says; no manifest row.
	InstallD3dStateSkip,

	// The drain end of the navmesh adjacency window, whatever the key says:
	// the key chooses deferral or counting, never whether the window is seen.
	InstallNavMeshAdjacency,

	// The Ogre join spin installs for its live setting; its module site is outside this table.
	InstallOgreJoinSpin,

#ifdef KEO_DEBUG
	// Read-only diagnostic, off unless unstitchProbe is set; the entry itself
	// does not exist in a PROD build.
	InstallUnstitchProbe,
	// Read-only too, and on unless sectionKeyProbe is cleared: what it records
	// is only useful if it was already running when the fault arrived.
	InstallSectionKeyProbe,
	// Pass-through counter, no INI key: a concurrency reading is only worth
	// anything if it was running for the whole session, and the one control
	// that matters (preload on versus off) is another key entirely.
	InstallPhysXPoolProbe,
	// Pass-through counter of player haul sizes; DEV only, no INI key.
	keo_inventory::InstallJobCounters,
#endif

	InstallZonePauseGuard,
	// The far visibility-check stagger: installed whatever its
	// live key says.
	InstallOnScreenStagger,
	// The paused off-screen skip: installed whatever its live key
	// says.
	InstallPausedSkip,
	InstallZoneLifecycleHooks,
	InstallCorpsePin,
	InstallBlackboardEmpty,
	InstallEquipmentAnimationLookup,
	InstallAnimationDirtyReuse,
	InstallSkeletonReserve,
	InstallMeshWeights,
	InstallBoneImport,
	InstallBoneIndex,
	InstallEdgeTriangles,
	InstallModelCloneReserve,
	InstallMaterialReserve,
	InstallEquipmentFieldLookup,
	InstallSaveReadBuffer,
	InstallSaveTransformRead,
	InstallSaveFieldKeyReuse,
	InstallSaveStringTransfer,
	InstallSaveInstanceLookup,
	InstallSaveStateListTransfer,
	morph_pose_scope::Install,
	keo_inventory::InstallBackpackFirst,
	keo_inventory::InstallCarriedWeight,
	keo_jobs::InstallSteelJobs,
	fixes::InstallTownClaim,
	keo_inventory::InstallBackpackSidecar,
	keo_inventory::InstallBackpackWindow,
	// The faction relations lookup is installed for its live setting in every build.
	InstallFactionRelations,
	fixes::InstallThrowout,
#if ZONEHAND_STEP >= 2
	InstallNestValidationGuard,
#endif
	// The ground-food score sees the worn backpack, while backpackFoodScore is on.
	keo_inventory::InstallBackpackFood,

	// A finished or repaired wall's navmesh splice, deferred until physics has moved its hulls; only while wanted.
	navmesh::InstallWallSplice,
	// A box across a cell border regenerates its cells in full; after the wall splice, whose install
	// compares the same entry's unpatched head. Only while wanted.
	navmesh::InstallSpliceSeam,
	// Machine operators fill main inventory and backpack before delivering; only while wanted.
	keo_inventory::InstallOperatorTrips,
	InstallOrderHook,
	OrderAbortStep,
	InstallCacheHook,
	// The rebuild-navmesh key's detour, while caching is wanted; a refusal leaves the key as the game has it.
	InstallNavMeshRebuildKey,
	InstallSectionDiagnosticHooks,

};

void InstallHooks(int* installed, int* total)
{
	*installed = 0;
	*total = 0;

	// The total is the wanted set as the config stands before any install, so
	// a failed install lowers *installed and never the total, even where the
	// failure clears a key a later want reads.
	const HookWantInputs wantedAtStart = HookWantInputsNow();
	for (int i = 0; i < HOOK_ROW_COUNT; ++i)
	{
		if (HookWantEval(s_wants[i], wantedAtStart))
			++*total;
	}

	IndependentStartupScope independentScope(installed);
	for (size_t i = 0; i < sizeof(kInstallSteps) / sizeof(kInstallSteps[0]); ++i)
		kInstallSteps[i](installed, total);
	independentScope.Finish();
}
