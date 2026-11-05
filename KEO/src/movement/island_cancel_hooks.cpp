// island_cancel_hooks.cpp - main-thread PlayerInterface cancel detours.
// Stop and job call the original before bookkeeping; nearest snapshots first.
// The detours take no lock, allocate nothing and log nothing.
#include "movement/islands.h"
#include "movement/islands_internal.h"
#include "zone/preload/preload.h"
#include "movement/formation.h"
#include "pathfind/player_task_policy.h"   // PT_OFF_* task-system offsets (for K7)
#include "movement/k7_swap_policy.h"       // K7ClassifySwap / K7SigOnsetStep
#include "movement/k7_arrival_policy.h"    // K7ArrivalShouldArm / K7ArrivalPoll
#include "zone/readiness/zone_readiness_classify.h"  // ClassifyZoneReadiness, ZR_*
#include "movement/order_outcome.h"            // OrderOutcomeNoteReissueSent
#include "movement/island_span_policy.h"       // IslandCellSpan (K7 arrival arm line)
#include "zone/zone_pause.h"               // ZonePauseIsPaused (pause gate)
#include <intrin.h>
#include <cstring>
#include "movement/islands_reissue_internal.h"
#include "movement/islands_reissue_counters.h"
#include "planner/plan_store.h"
#include "planner/planner_tick.h"
using namespace order_tracker_detail;
namespace island_cancel_hooks_detail {
// addTaskNearestSelectedCharacter orders one selected character, unknown in
// advance (or a carried one's player-owned carrier, which need not be
// selected), so the detour snapshots every tracked live character's order
// state before the original and IslandNoteCancelNearestTask compares after.
struct K7NearSnap {
	uintptr_t     character;
	K7OrderState  st;
};
}
using namespace island_cancel_hooks_detail;
namespace order_tracker_detail {
// Which cancel hooks hook_manifest.cpp installed (IslandSetCancelHooksInstalled).
bool g_cancelStopInstalled = false;
bool g_cancelJobInstalled  = false;
bool g_cancelTaskInstalled = false;
static K7NearSnap g_k7NearSnap[MAX_ISLAND_ORDERS];
static int        g_k7NearSnapCount = 0;
static int        g_k7NearDepth     = 0;   // the snapshot belongs to the outermost call
} // namespace order_tracker_detail

// =========================================================================
// K7: PlayerInterface cancel hooks (main thread only)
// =========================================================================
//
// Each detour runs the original with its arguments forwarded unchanged, then
// the matching IslandNoteCancel*, which drops the tracked IslandOrder of
// every character the player's action cancelled. No logging, no allocation,
// no locks. The order-tracker drop in the stop and add==0 job detours is
// still gated on K7FormOn (islandDeletedReissue=false leaves the tracker
// without the deleted-order form), but both also detach the selected characters
// from any formation group unconditionally, whether or not the deleted-order
// form is on. The installed flags are defined here; the K7 cancel counters
// have their definitions with the tracker state in islands_reissue.cpp.

namespace order_tracker_detail {
static void K7SnapshotTracked()
{
	g_k7NearSnapCount = 0;
	if (!K7FormOn() || g_orderCount == 0) return;
	uintptr_t* stuff;
	unsigned int count;
	if (!TrackerPlayerList(&stuff, &count)) return;
	for (int i = 0; i < g_orderCount && g_k7NearSnapCount < MAX_ISLAND_ORDERS; ++i)
	{
		if (!g_orders[i].active) continue;
		uintptr_t ch = g_orders[i].character;
		if (!TrackerListHas(stuff, count, ch)) continue;
		K7NearSnap& s = g_k7NearSnap[g_k7NearSnapCount];
		if (!K7ReadOrders(ch, &s.st)) continue;
		s.character = ch;
		g_k7NearSnapCount++;
	}
}
}
using namespace order_tracker_detail;

void IslandSetCancelHooksInstalled(bool stop, bool job, bool task)
{
	g_cancelStopInstalled = stop;
	g_cancelJobInstalled  = job;
	g_cancelTaskInstalled = task;
}

bool IslandCancelHooksLive()
{
	return g_cancelStopInstalled && g_cancelJobInstalled && g_cancelTaskInstalled;
}

// Stop key: stopCharactersMovement halts and clearOrders every selected
// character, with no faction filter. Drop them all.
// The stop-key / job / nearest-task cancels must still drop a held entry:
// K7DropSelected sets IslandOrder.active = false directly, by pointer, with
// no read of k7PostDeathHold. A hold never reaches this decision -- the
// player's own cancel always wins.
void IslandNoteCancelStop(void* playerInterface)
{
	// Formation detach runs whether or not the deleted-order form is on
	// (K7FormOn only gates the order tracker below).
	IslandDetachSelectedFromFormation((uintptr_t)playerInterface);
	if (!K7FormOn()) return;
	g_k7CancelStop += K7DropSelected((uintptr_t)playerInterface);
}

// Job order: Character::addJob clears the orders only when `add`
// (addDontClear) is 0. With add != 0 the job queues behind the move order,
// which stays, so tracking stays too.
void IslandNoteCancelJob(void* playerInterface, bool add)
{
	if (add) return;
	IslandDetachSelectedFromFormation((uintptr_t)playerInterface);
	if (!K7FormOn()) return;
	g_k7CancelJob += K7DropSelected((uintptr_t)playerInterface);
}

// Nearest-character task: drop every tracked character whose order changed
// across the original (deque size, head Tasker, head type, or a current
// task 29 that is no longer 29) against the detour's snapshot. The
// original's early-outs (a NULL subject, an unconscious selection,
// checkPlayerOrderForProblems' veto) change nothing, so drop nothing. With
// shift + task 26 it fans out through the hooked addJobSelectedCharacters /
// addOrderSelectedCharacters, which drop first: those entries are gone and
// are skipped here.
void IslandNoteCancelNearestTask(void* playerInterface)
{
	(void)playerInterface;
	// Gated on K7FormOn like every K7 drop: the snapshot this compares
	// against (g_k7NearSnap, taken by hook_addTaskNearest below) only exists
	// while the deleted-order form is on. So is the OrderOutcomeCancel call
	// below -- a nearest-character task issued with islandDeletedReissue off
	// still reads as an unrecovered stall rather than a cancel, same as
	// every other K7-gated signal here.
	if (!K7FormOn()) return;
	for (int i = 0; i < g_k7NearSnapCount; ++i)
	{
		const K7NearSnap& s = g_k7NearSnap[i];
		IslandOrder* ord = FindOrderForCharacter(s.character);
		if (!ord) continue;
		K7OrderState post;
		if (!K7ReadOrders(s.character, &post)) continue;
		bool changed = post.size != s.st.size
		            || post.head != s.st.head
		            || post.headType != s.st.headType
		            || (s.st.curType == ORDER_TYPE_MOVE && post.curType != ORDER_TYPE_MOVE);
		if (changed)
		{
			if (ord->k7ArrivalWaitSince > 0.0) g_k7ArrivalResumed++;
			ord->active = false;
			g_k7CancelTask++;
			OrderOutcomeCancel(s.character, ElapsedSec());
		}
	}
	g_k7NearSnapCount = 0;
}

// Main thread, from the stop-key detour after the original: drops the route plan of every selected
// character, walking the selection as the order hook's non-move branch does. No lock; returns at
// once while the route planner is unarmed.
static void DropSelectedPlans(void* thisPI)
{
	if (planner::PlanStoreMode() == planner::PLANNER_OFF) return;
	uintptr_t pi = (uintptr_t)thisPI;
	if (!pi) return;
	uintptr_t count = *(uintptr_t*)(KLIB_MEMBER(3, pi, PlayerInterface_selected_count, OFF_PI_SEL_COUNT));
	if (count == 0) return;
	uintptr_t arrayPtr = *(uintptr_t*)(KLIB_MEMBER(3, pi, PlayerInterface_selected_buckets, OFF_PI_SEL_ARRAY));
	uintptr_t index    = *(uintptr_t*)(KLIB_MEMBER(3, pi, PlayerInterface_selected_bucketCount, OFF_PI_SEL_INDEX));
	if (!arrayPtr || index >= 1024) return;

	uintptr_t* node = *(uintptr_t**)(arrayPtr + 8 * index);
	void* sentinel = *(void**)((uintptr_t)GameAddr(RVA_HANDLE_SENTINEL));
	int maxIter = (int)count + 16;
	int iter = 0;
	while (node)
	{
		if (++iter > maxIter) break;
		int nodeType = *(int*)(KLIB_MEMBER(3, (uintptr_t)node, HandSetNode_handle_type, OFF_SEL_NODE_TYPE));
		if (nodeType == 1)
		{
			void* resolved = KlibSelectedCharacter((const void*)(KLIB_MEMBER(3, (uintptr_t)node, HandSetNode_value_base_, OFF_SEL_NODE_HANDLE)));
			uintptr_t character = (uintptr_t)resolved;
			if (character && resolved != sentinel)
				planner::PlannerDrop(character);
		}
		node = *(uintptr_t**)KLIB_MEMBER(3, node, HandSetNode_next_, 0);
	}
}

void hook_stopCharactersMovement(void* thisPI)
{
	game::g_hookOrig.orig_stopCharactersMovement(thisPI);
	IslandNoteCancelStop(thisPI);
	DropSelectedPlans(thisPI);
}

// BEGIN planner job cancellation
// A native add==false job can clear orders without changing the movement's
// cached destination. Polling that destination cannot identify this cancel:
// the planner deliberately preserves stopped/held move orders. Compare only
// existing plans around this synchronous Main call, independently of K7.
struct PlannerJobSnapshot
{
	uintptr_t character, cm;
	int slot;
	unsigned epoch;
	K7OrderState orders;
};

static int SnapshotPlannerJobs(PlannerJobSnapshot* out)
{
	if (planner::PlanStoreMode() == planner::PLANNER_OFF) return 0;
	uintptr_t* live;
	unsigned count;
	if (!TrackerPlayerList(&live, &count)) return 0; // existing list cap: 200
	int n = 0;
	for (unsigned i = 0; i < count && n < planner::PLAN_SLOTS; ++i)
	{
		uintptr_t ch = live[i];
		if (!ch) continue;
		uintptr_t cm = *(uintptr_t*)(KLIB_MEMBER(3, ch, Character_movement, OFF_CHAR_MOVEMENT));
		int slot = planner::PlanStoreFind(cm);
		planner::PlanView plan;
		if (slot < 0 || !planner::PlanStoreRead(slot, &plan) || plan.cm != cm) continue;
		K7OrderState orders;
		if (!K7ReadOrders(ch, &orders)) continue;
		out[n].character = ch;
		out[n].cm = cm;
		out[n].slot = slot;
		out[n].epoch = plan.epoch;
		out[n].orders = orders;
		++n;
	}
	return n;
}

static void DropChangedPlannerJobs(const PlannerJobSnapshot* before, int n)
{
	if (n == 0 || planner::PlanStoreMode() == planner::PLANNER_OFF) return;
	uintptr_t* live;
	unsigned count;
	if (!TrackerPlayerList(&live, &count)) return;
	for (int i = 0; i < n; ++i)
	{
		const PlannerJobSnapshot& s = before[i];
		// Do not dereference an actor removed by the original, and do not
		// invalidate a fresh route created by a nested order or world reset.
		if (!TrackerListHas(live, count, s.character)) continue;
		planner::PlanView plan;
		if (!planner::PlanStoreRead(s.slot, &plan) || plan.cm != s.cm || plan.epoch != s.epoch) continue;
		uintptr_t cm = *(uintptr_t*)(KLIB_MEMBER(3, s.character, Character_movement, OFF_CHAR_MOVEMENT));
		if (cm != s.cm) continue;
		K7OrderState after;
		if (!K7ReadOrders(s.character, &after)) continue;
		if (after.size != s.orders.size || after.head != s.orders.head || after.headType != s.orders.headType
		    || (s.orders.curType == ORDER_TYPE_MOVE && after.curType != ORDER_TYPE_MOVE))
			planner::PlannerDrop(s.character);
	}
}
// END planner job cancellation

void hook_addJobSelected(void* thisPI, int task, void* subject, bool shift,
                         bool add, const float* location)
{
	PlannerJobSnapshot before[planner::PLAN_SLOTS];
	const int n = add ? 0 : SnapshotPlannerJobs(before);
	game::g_hookOrig.orig_addJobSelected(thisPI, task, subject, shift, add, location);
	IslandNoteCancelJob(thisPI, add);
	DropChangedPlannerJobs(before, n);
}

void hook_addTaskNearest(void* thisPI, void* dest, int task, void* subject, bool shift,
                         const float* location, bool noAnimals)
{
	// The snapshot belongs to the outermost call (no known path re-enters this
	// function; the depth count keeps one from overwriting it).
	bool outer = (g_k7NearDepth++ == 0);
	if (outer) K7SnapshotTracked();
	game::g_hookOrig.orig_addTaskNearest(thisPI, dest, task, subject, shift, location, noAnimals);
	if (outer) IslandNoteCancelNearestTask(thisPI);
	g_k7NearDepth--;
}
