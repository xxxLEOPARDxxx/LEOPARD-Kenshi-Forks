#include "base/log_admission.h"
#ifndef UNICODE
#define UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "zone/handoff/zone_lifecycle_hooks.h"

#if ZONEHAND_STEP >= 2

#include "zone/handoff/zone_handoff.h"
#include "zone/handoff/zone_lifecycle_diag.h"
#include <sstream>
#include "zone/retention/zone_retention.h"
#include "zone/retention/zone_expiry_guard.h"
#include "zone/retention/zone_expiry_guard_policy.h"
#include "navmesh/nm_workers.h"
#include "game/game.h"
#include "base/core.h"
#include "base/ini_names.h"
#include "fixes/code_patch_batch.h"
#include "fixes/near_page.h"
#include "render/module_hooks.h"
#include "plugin/hook_manifest.h"
#include "base/config.h"
#include "base/klib_include.h"
#include <core/Functions.h>
#include "base/klib_include_end.h"

namespace {
bool WriteTerrainBranch(unsigned char* displacement, uintptr_t destination)
{
	const __int64 relative = __int64(destination) - __int64(uintptr_t(displacement + 4));
	if (relative < -2147483647LL - 1 || relative > 2147483647LL)
		return false;
	const int offset = int(relative);
	memcpy(displacement, &offset, sizeof(offset));
	return true;
}

bool EncodeTerrainDestroyStub(unsigned char* code, uintptr_t manager, uintptr_t normal, uintptr_t release)
{
	const unsigned char bytes[] = {
		0x48,0x8b,0x1d,0,0,0,0, // mov rbx, [rip + ZoneManager]
		0x48,0x85,0xdb,         // test rbx, rbx
		0x0f,0x85,0,0,0,0,     // jnz native callback body
		0x48,0x8b,0xd5,         // mov rdx, rbp (original MaterialPtr parameter)
		0xe9,0,0,0,0            // jmp native release-only path
	};
	memcpy(code, bytes, sizeof(bytes));
	return WriteTerrainBranch(code + 3, manager) &&
	       WriteTerrainBranch(code + 12, normal) &&
	       WriteTerrainBranch(code + 20, release);
}

void InstallTerrainDestroyGuard()
{
	// Only the running Steam RE_Kenshi image contains this exact body.
	HMODULE game = GetModuleHandleW(NULL);
	DWORD stamp = 0, size = 0;
	if (!ReadModuleImageId(game, &stamp, &size) || stamp != 0x65d604d7u || size != 0x232c000u)
		return;
	unsigned char* base = reinterpret_cast<unsigned char*>(game);
	const uintptr_t manager = uintptr_t(base) + 0x2133960;
	unsigned char* site = base + 0xa1624f;
	unsigned char* release = base + 0xa1620c;
	const unsigned char original[] = {0x48,0x8b,0x1d,0x0a,0xd7,0x71,0x01};
	const unsigned char callback[] = {
		0x48,0x89,0x54,0x24,0x10,0x56,0x57,0x41,0x54,0x48,0x83,0xec,0x40,0x48,0xc7,0x44,
		0x24,0x20,0xfe,0xff,0xff,0xff,0x48,0x89,0x5c,0x24,0x60,0x48,0x89,0x6c,0x24,0x78,
		0x48,0x8b,0xea
	};
	const unsigned char releaseBytes[] = {
		0x48,0x83,0x3a,0x00,0x74,0x2a,0x48,0x8b,0x42,0x08,0x83,0xc9,0xff,0xf0,0x0f,0xc1,
		0x48,0x20,0xff,0xc9,0x75,0x1a,0x48,0x8b,0x4a,0x08,0x48,0x85,0xc9,0x74,0x11,
		0x48,0x8b,0x01,0x33,0xd2,0xff,0x10,0x48,0x8b,0x4d,0x08,0xff,0x15,0xcc,0x15,0x83,0x01,
		0x45,0x33,0xe4,0xe9,0x90,0x01,0x00,0x00
	};
	const unsigned char epilogue[] = {
		0x4c,0x89,0x65,0x08,0x4c,0x89,0x65,0x00,0x48,0x8b,0x5c,0x24,0x60,0x48,0x8b,0x6c,
		0x24,0x78,0x48,0x83,0xc4,0x40,0x41,0x5c,0x5f,0x5e,0xc3
	};
	const unsigned char resume[] = {0xf3,0x0f,0x10,0x50,0x08,0xf3,0x0f,0x10,0x08,0x48,0x8d,0x4c,0x24,0x70};
	if (memcmp(site, original, sizeof(original)) ||
	    memcmp(base + 0xa161e0, callback, sizeof(callback)) ||
	    memcmp(release, releaseBytes, sizeof(releaseBytes)) ||
	    memcmp(base + 0xa163d4, epilogue, sizeof(epilogue)) ||
	    memcmp(site + sizeof(original), resume, sizeof(resume))) {
		LogError("Terrain material shutdown guard: native callback contract refused");
		return;
	}
	if (!code_patch_threads::Prepare((GetDLLDirectory() + OPTIMIZER_LOG_NAME).c_str())) {
		LogError("Terrain material shutdown guard: emergency log unavailable");
		return;
	}
	unsigned char* code = AllocateNearPages(uintptr_t(site), 4096);
	if (!code) {
		LogError("Terrain material shutdown guard: could not allocate branch island");
		return;
	}
	code_patch_batch::Edit edit = {};
	edit.address = site;
	edit.size = sizeof(original);
	memcpy(edit.before, original, sizeof(original));
	edit.after[0] = 0xe9;
	edit.after[5] = edit.after[6] = 0x90;
	// Encode against the real instruction address, not the patch buffer.
	const __int64 displacement = __int64(uintptr_t(code)) - __int64(uintptr_t(site + 5));
	DWORD old = 0;
	if (displacement < -2147483647LL - 1 || displacement > 2147483647LL ||
	    !EncodeTerrainDestroyStub(code, manager, uintptr_t(site + sizeof(original)), uintptr_t(release)) ||
	    !VirtualProtect(code, 4096, PAGE_EXECUTE_READ, &old) ||
	    !FlushInstructionCache(GetCurrentProcess(), code, 24)) {
		VirtualFree(code, 0, MEM_RELEASE);
		LogError("Terrain material shutdown guard: could not seal branch island");
		return;
	}
	const int relative = int(displacement);
	memcpy(edit.after + 1, &relative, sizeof(relative));
	code_patch_threads::Set threads = {};
	bool mayPublish = false;
	const bool armed = code_patch_batch::Apply(&edit, 1, &threads, &mayPublish);
	if (!mayPublish)
		VirtualFree(code, 0, MEM_RELEASE);
	if (!armed)
		LogError("Terrain material shutdown guard: callback patch failed");
	else
		KEO_DETAIL("Terrain material shutdown guard: native release-only path admitted");
}
}

// ZoneManager::activateZoneMap(this, map, centre, range, type, timer). The
// third argument is an iVector2 passed whole in a register and unused by the
// body; the last two arrive on the stack.
typedef bool (__fastcall *activateZoneMap_t)(void* zoneMgr, void* map, __int64 centre,
                                             int range, int type, float deactivationTimer);
static activateZoneMap_t orig_activateZoneMap = NULL;

// Two responsibilities, one detour, because every native activation in the
// image arrives here:
//
//   * a town refresh carrying a nonpositive timer is refused outright. The
//     value is the largest camera/player countdown over the town's coverage,
//     so a nonpositive one means nothing in that coverage is leased and the
//     engine would substitute a fixed default, renewing the whole coverage
//     from nothing, every frame, forever;
//   * a cell the mod is holding privately is taken over. The engine's own
//     activate refuses such a cell and returns 0, which leaves it outside
//     the pending set and so outside the game's reach; inserting it and
//     reporting success hands it back, and the caller's own scheduling
//     follows unchanged.
//
// The takeover inserts into the same set, through the same helper, that the
// original would have used on this very call, so it is safe exactly where
// the original is.
static bool __fastcall hook_activateZoneMap(void* zoneMgr, void* map, __int64 centre,
                                            int range, int type, float deactivationTimer)
{
	if (ZoneHandoffTownGuardRefuses(type, deactivationTimer))
	{
		// Debug off adds no clock/map/native reads. Never query a foreign-thread map.
		if (DetailedLogEnabled() && IsMainThread())
		{
			static ZoneLifecycleDiagBudget budget;
			int gx = map ? GetZoneGridX(map) : -1;
			int gy = map ? GetZoneGridY(map) : -1;
			unsigned epoch = ZoneHandoffWorldEpoch();
			if (budget.Admit(epoch, 0, gx, gy, ElapsedSec()))
			{
				std::ostringstream ss;
				ss << "ZoneTownRefuse: zone(" << gx << "," << gy << ") epoch=" << epoch
				   << " centre=" << centre << " range=" << range << " type=" << type
				   << " timer=" << deactivationTimer << " phase=" << GetZoneState(zoneMgr);
				KEO_DETAIL(ss.str());
			}
		}
		return false;
	}

	bool activated = orig_activateZoneMap(zoneMgr, map, centre, range, type, deactivationTimer);

	// The original's answer is true when any cell in its range succeeded, so
	// it cannot say whether this one did. The takeover decides from the
	// cell's own flags instead.
	if (map && ZoneHandoffTakeover(zoneMgr, map, type))
		activated = true;

	return activated;
}

#if ZONEHAND_STEP >= 3

// ZoneMap::update(ZoneMap*) -> false when the cell expired and left the
// active set.
typedef bool (__fastcall *zoneMapUpdate_t)(void* zoneEntry);
static zoneMapUpdate_t orig_zoneMapUpdate = NULL;

// Retention wants this cell gone. The zone's navmesh goes with it, so no mod
// navmesh thread may be working on the cell and none may start while the
// teardown runs. This blocks briefly on the generator's queue mutex, which
// is why retention lets at most one cell per frame reach it.
static bool RetentionFencedUpdate(void* zoneEntry)
{
	NavMeshUnloadFence fence;
	NmFenceResult fr = fence.TryBegin(zoneEntry);
	if (fr == NM_FENCE_UNAVAILABLE)
	{
		// No such fence exists in this build or this session, and none ever
		// will. Holding on that would hold every cell forever, so the cell
		// expires with vanilla's own exposure, which is what it has today.
		bool kept = orig_zoneMapUpdate(zoneEntry);
		ZoneRetentionNoteReleased(zoneEntry, !kept, false);
		return kept;
	}
	if (fr == NM_FENCE_DEFER_PJ)
	{
		// A zero-wait try lost to a generation; the fence asked the entry
		// points to stand aside, so one of the next frames wins the lock.
		ZoneRetentionHoldInstead(zoneEntry, ZONE_RETENTION_DEFER_PJ);
		return orig_zoneMapUpdate(zoneEntry);
	}
	if (!NmFenceProceeds(fr))
	{
		ZoneRetentionHoldInstead(zoneEntry, ZONE_RETENTION_DEFER_NAV);
		return orig_zoneMapUpdate(zoneEntry);
	}

	bool kept = orig_zoneMapUpdate(zoneEntry);
	fence.Release();
	ZoneRetentionNoteReleased(zoneEntry, !kept, true);
	return kept;
}

// The original decides every cell's fate; this only decides whether the
// countdown it is about to read has run out, and under which fence. Holding
// is a write to the town countdown and nothing else, so the original's own
// expiry branch does every teardown there is — the deactivate, the set erase
// and the notification. The deactivate frees the cell's content, which a
// navmesh job that claimed the cell reads with no lock, so every expiry runs
// fenced wherever a fence can exist: retention's for a cell it wants gone,
// the expiry guard's for every other.
//
// Runs for every member of the active set, every frame, so a cell that is
// not about to expire costs a handful of reads and the call.
static bool __fastcall hook_zoneMapUpdate(void* zoneEntry)
{
	if (!ZoneExpiresThisFrame(zoneEntry))
		return orig_zoneMapUpdate(zoneEntry);

	switch (ZoneExpiryRouteFor(true, ZoneRetentionAnswerFor(zoneEntry)))
	{
	case ZONE_EXPIRY_ROUTE_RETENTION_FENCE:
		return RetentionFencedUpdate(zoneEntry);
	case ZONE_EXPIRY_ROUTE_CLAIMS_GUARD:
		return ZoneExpiryGuardUpdate(zoneEntry, orig_zoneMapUpdate);
	default:
		// Held by retention: the hold is already written.
		return orig_zoneMapUpdate(zoneEntry);
	}
}

static void InstallZoneMapUpdateHook(int* installed)
{
	if (HookInstall(HOOK_ZONEMAP_UPDATE, hook_zoneMapUpdate, &orig_zoneMapUpdate,
	                installed, true) == NULL)
	{
		KEO_DETAIL(std::string("Zone lifecycle: ZoneMap::update detour installed (retention=")
		       + (zone::g_zoneCfg.zoneRetentionEnabled ? "on" : "off") + " expiryGuard=on)");
		return;
	}
	orig_zoneMapUpdate = NULL;
	LogError("Zone lifecycle: ZoneMap::update detour NOT installed — nothing holds a cell the game has "
	         "taken over from the mod, so every one of them expires on the first pass that reaches it, "
	         "and no expiry waits for a navmesh job still working on its cell (the expiry guard is lost); "
	         "this build prepares cells that are then thrown away");
}

#endif // ZONEHAND_STEP >= 3

void InstallZoneLifecycleHooks(int* installed, int*)
{
	ZoneHandoffInit();
	InstallTerrainDestroyGuard();

	if (HookInstall(HOOK_ACTIVATE_ZONEMAP, hook_activateZoneMap, &orig_activateZoneMap,
	                installed, true) == NULL)
	{
		KEO_DETAIL(std::string("Zone lifecycle: activateZoneMap detour installed (townGuard=")
		       + (zone::g_zoneCfg.townGuardEnabled ? "on)" : "off)"));
	}
	else
	{
		orig_activateZoneMap = NULL;
		LogError("Zone lifecycle: activateZoneMap detour NOT installed — this build publishes no zone itself, "
		         "so every cell the mod prepares would stay out of the game's reach");
	}

#if ZONEHAND_STEP >= 3
	ZoneRetentionInit();
	InstallZoneMapUpdateHook(installed);
#endif
}

#else

void InstallZoneLifecycleHooks(int*, int*) {}

#endif // ZONEHAND_STEP >= 2
