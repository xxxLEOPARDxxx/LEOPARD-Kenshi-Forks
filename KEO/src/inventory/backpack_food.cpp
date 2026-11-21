#include "game/native_profile.h"
#include "base/log_admission.h"
// backpack_food.cpp - The post-hook on AI::scoreFindFoodOnGround: vanilla scores walking to ground
// food only while the main inventory holds none the character can eat; this also asks the worn
// backpack. The install step runs on the main thread at startup; the post-hook runs wherever the
// AI scores its goals.
#include "inventory/backpack_food.h"
#include "inventory/backpack_policy.h"
#include "inventory/backpack_reader.h"
#include "inventory/byte_check_policy.h"
#include "inventory/dialogue_item_function.h"
#include "inventory/inventory_config.h"
#include "plugin/hook_manifest.h"
#include "game/game.h"
#include "game/klib_member_contract.h"
#include "base/core.h"
#include <string>

namespace backpack_food_detail {
typedef float (__fastcall *scoreFindFoodOnGround_t)(void* ai, const void* subject, const void* v);
typedef int   (__fastcall *getNumFoodItems_t)(void* inventory, void* race);
} // namespace backpack_food_detail
using namespace backpack_food_detail;

namespace keo_inventory {

static scoreFindFoodOnGround_t orig_scoreFindFoodOnGround = NULL;
static getNumFoodItems_t       fn_getNumFoodItems         = NULL;
static const size_t kAiMe = KLIB_OFF_AI_me;
// Inventory::getNumFoodItems's first bytes in this build. The binding is used when they match, or
// when another plugin's jump sits in front of a matching tail.
static const unsigned char kGetNumFoodItemsHead[16] =
	{ 0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x4C,0x8B,0xC2,0xBA,0x0F,0x00 };
static volatile LONG s_foodCalls = 0, s_foodZeroed = 0;

// AI back thread (the main thread with characterMultithreading off). Vanilla's score, or 0 when it
// is at least 1 (a walk to ground food; lower scores are kept) and the worn backpack holds food
// this character can eat. No lock, no allocation, no logging.
static float __fastcall hook_scoreFindFoodOnGround(void* ai, const void* subject, const void* v)
{
	float r = orig_scoreFindFoodOnGround(ai, subject, v);
	InterlockedIncrement(&s_foodCalls);
	if (r < 1.0f || !ai || !g_inventoryCfg.backpackFixesEnabled)
		return r;
	void* me = *(void* const*)((const char*)ai + kAiMe);
	void* bag = me ? WornBackpackInventory(me) : NULL;
	if (!FoodScoreZero(r, bag && fn_getNumFoodItems(bag, me) > 0))
		return r;
	InterlockedIncrement(&s_foodZeroed);
	return 0.0f;
}

// Startup only. The completion runs after the local preflight has returned.
static bool s_foodCountShared = false;

static void CompleteBackpackFood(const char* why)
{
	if (why)
	{
		LogError(std::string("BackpackFood: installation unconfirmed (") + why + ")");
		return;
	}
	KEO_DETAIL(std::string("BackpackFood: installed shared=") +
	           (s_foodCountShared ? "getNumFoodItems" : "none"));
}

void InstallBackpackFood(int* installed, int*)
{
	if (!HookRowWanted(HOOK_SCORE_FIND_FOOD_ON_GROUND)) return;
	const char* reader = NULL;
	if (!BackpackReaderInit(&reader))
	{
		LogError(std::string("BackpackFood: reader refused: ") + (reader ? reader : "unknown"));
		return;
	}
	// A callee detoured over a matching tail is still callable at its entry.
	ByteRowLog rows;
	ByteRowLogReset(&rows);
	if (!ByteRowCheck(&rows, "getNumFoodItems", BYTE_CHECK_CALLEE_HEAD,
	                  (const unsigned char*)GameAddr(RVA_INVENTORY_GET_NUM_FOOD_ITEMS),
	                  NativeProfileBytes(RVA_INVENTORY_GET_NUM_FOOD_ITEMS, kGetNumFoodItemsHead, 16), 16))
	{
		CompleteBackpackFood(rows.why);
		return;
	}
	fn_getNumFoodItems = (getNumFoodItems_t)GameAddr(RVA_INVENTORY_GET_NUM_FOOD_ITEMS);
	s_foodCountShared = rows.sharedLen != 0;
	HookInstallIndependent(HOOK_SCORE_FIND_FOOD_ON_GROUND, hook_scoreFindFoodOnGround,
	                       &orig_scoreFindFoodOnGround, installed, true, CompleteBackpackFood);
}

long BackpackFoodCalls()
{
	return InterlockedCompareExchange(&s_foodCalls, 0, 0);
}

void BackpackFoodCountersRead(long* foodZeroed, long* dialogCalls, long* dialogBackpackHits)
{
	long calls = 0, hits = 0;
	DialogueItemFunctionCounters(&calls, &hits);
	if (foodZeroed)
		*foodZeroed = InterlockedCompareExchange(&s_foodZeroed, 0, 0);
	if (dialogCalls)
		*dialogCalls = calls;
	if (dialogBackpackHits)
		*dialogBackpackHits = hits;
}

} // namespace keo_inventory
