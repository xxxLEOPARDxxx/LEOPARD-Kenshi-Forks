#include "zone/preload/preload_order.h"
#include "zone/grid.h"
#include "movement/tracking.h"
#include "game/native_profile.h"
#include "game/game.h"
#include "base/config.h"
#include "bench/bench_runner.h"
#include "jobs/steel_jobs.h"

namespace {
const int MoveTo = 29;

// The destination is a read-only hint for zone loading. The game owns every
// ordinary order and selects its own path and group behavior.
void ObserveStreamingTarget(void* player, const float* location)
{
	if (!zone::g_zoneCfg.preloadEnabled || !zone::g_zoneCfg.movementAwareEnabled || !player || !location)
		return;
	int targetX, targetY;
	if (!WorldToZoneGrid(location[0], location[2], &targetX, &targetY)) return;

	const uintptr_t pi = (uintptr_t)player;
	const uintptr_t count = *(const uintptr_t*)KLIB_MEMBER(3, pi, PlayerInterface_selected_count, OFF_PI_SEL_COUNT);
	const uintptr_t array = *(const uintptr_t*)KLIB_MEMBER(3, pi, PlayerInterface_selected_buckets, OFF_PI_SEL_ARRAY);
	const uintptr_t index = *(const uintptr_t*)KLIB_MEMBER(3, pi, PlayerInterface_selected_bucketCount, OFF_PI_SEL_INDEX);
	if (!count || count > 1024 || !array || index >= 1024) return;

	const void* sentinel = *(void* const*)NativeProfileAddress(gameBase, RVA_HANDLE_SENTINEL);
	uintptr_t node = ((const uintptr_t*)array)[index];
	for (uintptr_t visited = 0; node && visited < count + 16; ++visited) {
		if (*(const int*)KLIB_MEMBER(3, node, HandSetNode_handle_type, OFF_SEL_NODE_TYPE) == 1) {
			void* selected = KlibSelectedCharacter((const void*)KLIB_MEMBER(3, node, HandSetNode_value_base_, OFF_SEL_NODE_HANDLE));
			if (selected && selected != sentinel) {
				const uintptr_t actor = (uintptr_t)selected;
				int x, y;
				if (WorldToZoneGrid(GetCharPosX(actor), GetCharPosZ(actor), &x, &y) &&
				    (x != targetX || y != targetY)) {
					const uintptr_t movement = *(const uintptr_t*)KLIB_MEMBER(3, actor, Character_movement, OFF_CHAR_MOVEMENT);
					if (movement) AddWatchedCharacter(actor, movement, targetX, targetY, x, y, true);
				}
			}
		}
		node = *(const uintptr_t*)KLIB_MEMBER(3, node, HandSetNode_next_, 0);
	}
	++hookOrderCount;
}
}

void hook_addOrderSelected(void* player, void* building, int task, void* subject,
                           bool shift, bool dontClear, const float* location)
{
	if (task == MoveTo && keo_jobs::SteelJobsConsumeMove(player, building, subject, location, shift)) return;
	try {
		BenchNotifyPlayerOrder();
		if (task == MoveTo) ObserveStreamingTarget(player, location);
	} catch (...) {
		try { LogErrorDeferrable("Zone preloading: could not observe order"); }
		catch (...) {} // Observation and logging must not prevent the native order.
	}
	game::g_hookOrig.orig_addOrderSelected(player, building, task, subject, shift, dontClear, location);
}
