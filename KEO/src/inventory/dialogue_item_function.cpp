#include "game/native_profile.h"
#include "base/log_admission.h"
// dialogue_item_function.cpp - checkConditions' one hasItemFunction call is retargeted to a
// near-page thunk that adds the tested character (r12) as a third argument and jumps to a wrapper;
// the wrapper asks the main inventory and, when that says no, the worn backpack. The patch is
// written once on the main thread at startPlugin, before any world exists, and never undone.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "inventory/dialogue_item_function.h"
#include "inventory/dialogue_thunk_policy.h"
#include "inventory/backpack_policy.h"
#include "inventory/backpack_reader.h"
#include "inventory/byte_check_policy.h"
#include "inventory/inventory_config.h"
#include "fixes/near_page.h"
#include "base/fixed_log_buf.h"
#include "game/game.h"
#include "base/core.h"
#include <windows.h>
#include <string.h>

namespace dialogue_item_function_detail {
typedef bool (__fastcall *hasItemFunction_t)(const void* inventory, int type);
} // namespace dialogue_item_function_detail
using namespace dialogue_item_function_detail;

namespace keo_inventory {

static hasItemFunction_t fn_hasItemFunction = NULL;
static volatile LONG s_dialogCalls = 0, s_dialogBackpackHits = 0;
// Inventory::hasItemFunction's first bytes in this build: the patch is written only when they pass
// as a callee head (the wrapper only calls the function; another plugin's detour over a matching
// tail passes).
static const unsigned char kHasItemFunctionHead[16] =
	{ 0x44,0x8B,0x49,0x18,0x45,0x33,0xC0,0x45,0x85,0xC9,0x74,0x1B,0x48,0x8B,0x49,0x20 };
static const size_t kThunkPageSize = 4096;
static const uintptr_t kFurnaceGuardRva = 0x5A3D12;
static const uintptr_t kFurnaceLeadRva = 0x5A3D1D;
static const uintptr_t kFurnaceCallRva = 0x5A3D36;
static const int kSeveredLimb = 17;
static const unsigned char kFurnaceGuard[11] =
	{ 0xFF,0x90,0xF0,0x02,0x00,0x00,0x83,0xF8,0x0E,0x75,0x23 };
static const unsigned char kFurnaceLead[30] =
	{ 0x49,0x8B,0x8C,0x24,0xF8,0x02,0x00,0x00,0x48,0x8B,0x01,
	  0xFF,0x90,0x60,0x01,0x00,0x00,0x48,0x8B,0xC8,0xBA,0x11,0x00,0x00,0x00,
	  0xE8,0xD7,0x7A,0xAA,0xFF };

// The furnace eligibility branch only looked in AI::me's main inventory.
// The native delivery task already transfers from both inventories.
static bool __fastcall KEO_FurnaceHasItemFunction(const void* inv, int type, const void* ai)
{
	const bool mainHas = fn_hasItemFunction(inv, type);
	if (mainHas || !ai || type != kSeveredLimb || !g_inventoryCfg.backpackFixesEnabled)
		return mainHas;
	// The verified lead above reads this same character from r12 before the call.
	void* who = *reinterpret_cast<void* const*>(static_cast<const char*>(ai) + 0x2F8);
	void* bag = WornBackpackInventory(who);
	return bag && bag != inv && fn_hasItemFunction(bag, type);
}

// The main thread or the AI back thread, from checkConditions through the thunk, with the tested
// character in `who`. Vanilla's answer, or the worn backpack's when the main inventory says no.
// No lock, no allocation, no logging.
static bool __fastcall KEO_DialogHasItemFunction(const void* inv, int type, void* who)
{
	InterlockedIncrement(&s_dialogCalls);
	const bool mainHas = fn_hasItemFunction(inv, type);
	void* bag = (!mainHas && who && g_inventoryCfg.backpackFixesEnabled) ? WornBackpackInventory(who) : NULL;
	if (!DialogUseBackpack(mainHas, bag != NULL))
		return mainHas;
	const bool r = fn_hasItemFunction(bag, type);
	if (r)
		InterlockedIncrement(&s_dialogBackpackHits);
	return r;
}

// Standalone and POD-only: a function holding __try may not hold an object needing unwinding.
static bool SafeReadBytes(const void* addr, void* out, size_t n)
{
	bool ok = true;
	GuardEnter();
	__try
	{
		memcpy(out, addr, n);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		ok = false;
	}
	GuardLeave();
	return ok;
}

// Main thread, startPlugin. Verifies the site and the callee, builds the thunk and rewrites the
// call's rel32. NULL on success with *thunkOut set; otherwise the reason, with the site untouched.
// rows records the callee head's judgement; a refusal's reason is its text.
static const char* TryArm(unsigned __int64* thunkOut, ByteRowLog* rows, bool furnace)
{
	unsigned __int64 navmesh = 0;
	if (!SafeReadBytes(GameAddr(RVA_GLOBAL_SECTION_MGR), &navmesh, sizeof(navmesh)))
		return "pauseState.navmesh unreadable";
	if (navmesh != 0)
		return "a world already exists";

	if (furnace)
	{
		unsigned char guard[sizeof(kFurnaceGuard)];
		if (!SafeReadBytes(GameAddr(kFurnaceGuardRva), guard, sizeof(guard)) ||
		    !NativeProfileMatches(kFurnaceGuardRva, guard, kFurnaceGuard, sizeof(guard)))
			return "the furnace branch bytes differ";
	}
	const uintptr_t leadRva = furnace ? kFurnaceLeadRva : kDialogLeadRva;
	const uintptr_t callRva = furnace ? kFurnaceCallRva : kDialogCallRva;
	const size_t leadLen = furnace ? sizeof(kFurnaceLead) : kDialogLeadLen;
	const unsigned char* expectedLead = furnace ? kFurnaceLead : kDialogLeadBytes;
	const unsigned __int64 leadAddr = (unsigned __int64)(uintptr_t)GameAddr(leadRva);
	const unsigned __int64 callAddr = (unsigned __int64)(uintptr_t)GameAddr(callRva);
	unsigned char lead[sizeof(kFurnaceLead)];
	if (!SafeReadBytes((const void*)(uintptr_t)leadAddr, lead, leadLen))
		return "the call site is unreadable";
	if (!NativeProfileMatches(leadRva,lead,expectedLead,leadLen))
		return "call site bytes differ";

	unsigned char head[16];
	if (!SafeReadBytes(GameAddr(RVA_INVENTORY_HAS_ITEM_FUNCTION), head, sizeof(head)))
		return "hasItemFunction head unreadable";
	if (!ByteRowCheck(rows, "hasItemFunction", BYTE_CHECK_CALLEE_HEAD, head, NativeProfileBytes(RVA_INVENTORY_HAS_ITEM_FUNCTION,kHasItemFunctionHead,16), 16))
		return rows->why;

	// The wrapper calls what the original call reached: the j_ thunk, itself a jump to
	// hasItemFunction, so a thunk retargeted after arming is still honoured; at arming it must
	// reach hasItemFunction.
	const unsigned __int64 target = ItemFunctionCallTarget(callAddr, lead + leadLen - 5);
	unsigned char thunkBytes[5];
	unsigned __int64 thunkTarget = 0;
	if (target != (unsigned __int64)(uintptr_t)GameAddr(RVA_J_INVENTORY_HAS_ITEM_FUNCTION)
	 || !SafeReadBytes((const void*)(uintptr_t)target, thunkBytes, sizeof(thunkBytes))
	 || !DialogJumpTarget(target, thunkBytes, &thunkTarget)
	 || thunkTarget != (unsigned __int64)(uintptr_t)GameAddr(RVA_INVENTORY_HAS_ITEM_FUNCTION))
		return "thunk";

	unsigned char* page = AllocateNearPages((unsigned __int64)gameBase, kThunkPageSize);
	if (!page)
		return "no free page within rel32 reach of the exe";
	const unsigned __int64 thunkAddr = (unsigned __int64)(uintptr_t)page;
	const uintptr_t wrapper = furnace ? (uintptr_t)&KEO_FurnaceHasItemFunction : (uintptr_t)&KEO_DialogHasItemFunction;
	if (!DialogThunkBuild(page, kThunkPageSize, (unsigned __int64)wrapper))
	{
		VirtualFree(page, 0, MEM_RELEASE);
		return "the thunk could not be built";
	}
	DWORD oldPage = 0;
	if (!VirtualProtect(page, kThunkPageSize, PAGE_EXECUTE_READ, &oldPage))
	{
		VirtualFree(page, 0, MEM_RELEASE);
		return "the thunk page could not be made executable";
	}
	FlushInstructionCache(GetCurrentProcess(), page, kThunkPageSize);

	int rel = 0;
	if (!DialogCallRel32(callAddr, thunkAddr, &rel))
	{
		VirtualFree(page, 0, MEM_RELEASE);
		return "the thunk is out of rel32 reach of the call";
	}

	// Before the write: the thunk reaches the wrapper as soon as the call does.
	if (!fn_hasItemFunction)
		fn_hasItemFunction = (hasItemFunction_t)(uintptr_t)target;

	void* relAddr = (void*)(uintptr_t)(callAddr + 1);
	DWORD oldSite = 0;
	if (!VirtualProtect(relAddr, 4, PAGE_EXECUTE_READWRITE, &oldSite))
	{
		VirtualFree(page, 0, MEM_RELEASE);
		return "VirtualProtect on the call failed";
	}
	memcpy(relAddr, &rel, 4);
	DWORD ignore = 0;
	VirtualProtect(relAddr, 4, oldSite, &ignore);
	FlushInstructionCache(GetCurrentProcess(), relAddr, 4);

	*thunkOut = thunkAddr;
	return NULL;
}

static void InstallItemFunctionPatch(bool gateOk, bool furnace)
{
	// The reader binds idempotently; unbound, the wrapper could never see a backpack.
	const char* reader = NULL;   // set only when the reader refuses
	unsigned __int64 thunk = 0;
	ByteRowLog rows;
	ByteRowLogReset(&rows);
	const char* why = !gateOk ? "the build gate refused" :
		(BackpackReaderInit(&reader) ? TryArm(&thunk, &rows, furnace) : "reader refused: ");
	FixedLogBuf o; FlbInit(&o);
	FlbStr(&o, furnace ? "FurnaceBackpack: " : "DialogueItemFunction: ");
	if (!why)
	{
		FlbStr(&o, "armed at exe+"); FlbHex(&o, furnace ? kFurnaceCallRva : kDialogCallRva);
		FlbStr(&o, " thunk="); FlbHex(&o, thunk);
		FlbStr(&o, " shared="); FlbStr(&o, ByteRowShared(&rows));
		KEO_DETAIL(FlbDone(&o));
	}
	else
	{
		FlbStr(&o, "not armed ("); FlbStr(&o, why); FlbStr(&o, reader);
		FlbStr(&o, "); the condition stays vanilla");
		LogError(FlbDone(&o));
	}
}

void InstallBackpackItemFunctionPatches(bool gateOk)
{
	if (g_inventoryCfg.backpackDialogueFunctionEnabled)
		InstallItemFunctionPatch(gateOk, false);
	InstallItemFunctionPatch(gateOk, true);
}

void DialogueItemFunctionCounters(long* calls, long* backpackHits)
{
	if (calls)
		*calls = InterlockedCompareExchange(&s_dialogCalls, 0, 0);
	if (backpackHits)
		*backpackHits = InterlockedCompareExchange(&s_dialogBackpackHits, 0, 0);
}

} // namespace keo_inventory
