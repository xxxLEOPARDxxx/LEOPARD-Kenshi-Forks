// backpack_table.cpp - The per-character backpack-first table: fixed slots, each guarded by its
// own sequence. Readers on any thread take no lock; the writers run on the main thread only, so
// two writes never overlap and only a reader can observe a write in progress.
#include <windows.h>
#include "inventory/backpack_table.h"
#include "inventory/backpack_policy.h"
#include "inventory/operator_policy.h"
#include "game/hand_key.h"

namespace backpack_table_detail {
// One slot. Written only between the two increments of seq; read through volatile accesses.
struct BackpackSlot
{
	volatile LONG seq;
	volatile LONG used;
	volatile LONG on;
	volatile LONG fill;
	volatile unsigned key[5];   // type, container, containerSerial, index, serial
};
} // namespace backpack_table_detail
using namespace backpack_table_detail;

namespace keo_inventory {

static BackpackSlot s_slots[BACKPACK_TABLE_CAP];
static volatile LONG s_highWater = 0;
static volatile LONG s_full = 0;
static volatile LONG s_raced = 0;
static volatile LONG s_personalFillCount = 0;
static volatile LONG s_generation = 0;

// Any thread: at most two attempts per slot; a slot that raced both is skipped (counted). Plain
// volatile reads: on x64, MSVC's volatile loads are acquire (/volatile:ms, the x64 default), so
// the field copies stay between the two reads of seq.
static int FindSetting(const game::HandKey& k, bool fill)
{
	const LONG n = s_highWater;
	for (LONG i = 0; i < n; ++i)
	{
		const BackpackSlot& s = s_slots[i];
		for (int attempt = 0; attempt < 2; ++attempt)
		{
			LONG before = s.seq;
			LONG used = s.used;
			LONG setting = fill ? s.fill : s.on;
			game::HandKey copy = { s.key[0], s.key[1], s.key[2], s.key[3], s.key[4] };
			LONG after = s.seq;
			if (!BackpackSlotReadable(before, after))
			{
				if (attempt == 1)
					InterlockedIncrement(&s_raced);
				continue;
			}
			if (used && game::HandKeyEqual(copy, k))
				return setting;
			break;
		}
	}
	return fill ? 0 : -1;
}

int BackpackFirstGet(const game::HandKey& k) { return FindSetting(k, false); }

int BackpackOperatorFillGet(const game::HandKey& k)
{
	return BackpackHasOperatorFillOverrides() ? FindSetting(k, true) : 0;
}

bool BackpackHasOperatorFillOverrides() { return s_personalFillCount != 0; }

// Main thread. The only store into a slot: the sequence is odd from the first increment to the
// second, and the interlocked increments are full barriers, so the three stores land between them.
static void SlotWrite(int i, LONG used, const game::HandKey& k, LONG on, LONG fill)
{
	BackpackSlot& s = s_slots[i];
	const LONG change = (fill ? 1 : 0) - (s.fill ? 1 : 0);
	InterlockedIncrement(&s.seq);
	s.used = used;
	s.on = on;
	s.fill = fill;
	s.key[0] = k.type;
	s.key[1] = k.container;
	s.key[2] = k.containerSerial;
	s.key[3] = k.index;
	s.key[4] = k.serial;
	InterlockedIncrement(&s.seq);
	if (change)
		InterlockedExchangeAdd(&s_personalFillCount, change);
}

// Main thread: the slot's own fields, read without the sequence (no other writer exists).
static bool SlotHolds(int i, const game::HandKey& k)
{
	const BackpackSlot& s = s_slots[i];
	if (!s.used)
		return false;
	game::HandKey copy = { s.key[0], s.key[1], s.key[2], s.key[3], s.key[4] };
	return game::HandKeyEqual(copy, k);
}

static game::HandKey FreeKey()
{
	game::HandKey k = { game::HAND_KEY_NULL_TYPE, 0, 0, 0, 0 };
	return k;
}

static bool SetSetting(const game::HandKey& k, int value, bool fill)
{
	if (game::HandKeyIsNull(k))
		return false;
	const int n = (int)s_highWater;
	int freeSlot = -1;
	for (int i = 0; i < n; ++i)
	{
		if (SlotHolds(i, k))
		{
			const LONG on = fill ? s_slots[i].on : value;
			const LONG limit = fill ? value : s_slots[i].fill;
			SlotWrite(i, on >= 0 || limit > 0, k, on, limit);
			return true;
		}
		if (freeSlot < 0 && !s_slots[i].used)
			freeSlot = i;
	}
	if (fill && !value)
		return true;
	if (freeSlot < 0)
	{
		if (n >= BACKPACK_TABLE_CAP)
		{
			InterlockedIncrement(&s_full);
			return false;
		}
		freeSlot = n;
		// The scan bound rises before the slot is filled: a reader that reaches the slot early
		// sees it free or mid-write, never a stale entry.
		InterlockedExchange(&s_highWater, (LONG)(n + 1));
	}
	SlotWrite(freeSlot, 1, k, fill ? -1 : value, fill ? value : 0);
	return true;
}

bool BackpackFirstSet(const game::HandKey& k, int on) { return SetSetting(k, on ? 1 : 0, false); }

bool BackpackOperatorFillSet(const game::HandKey& k, int percent)
{
	return (percent == 0 || OperatorFillPercentValid(percent)) && SetSetting(k, percent, true);
}

bool BackpackFirstEntry(int i, game::HandKey* k, int* on, int* percent)
{
	if (i < 0 || i >= (int)s_highWater || !s_slots[i].used)
		return false;
	const BackpackSlot& s = s_slots[i];
	if (k)
	{
		game::HandKey copy = { s.key[0], s.key[1], s.key[2], s.key[3], s.key[4] };
		*k = copy;
	}
	if (on)
		*on = s.on;
	if (percent)
		*percent = s.fill;
	return true;
}

void BackpackFirstRekey(int i, const game::HandKey& k)
{
	const int n = (int)s_highWater;
	if (i < 0 || i >= n || !s_slots[i].used)
		return;
	for (int j = 0; j < n; ++j)
	{
		if (j != i && SlotHolds(j, k))
		{
			const LONG on = s_slots[j].on >= 0 ? s_slots[j].on : s_slots[i].on;
			const LONG fill = s_slots[j].fill ? s_slots[j].fill : s_slots[i].fill;
			SlotWrite(j, 1, k, on, fill);
			SlotWrite(i, 0, FreeKey(), -1, 0);
			return;
		}
	}
	SlotWrite(i, 1, k, s_slots[i].on, s_slots[i].fill);
}

void BackpackFirstErase(int i)
{
	if (i < 0 || i >= (int)s_highWater)
		return;
	SlotWrite(i, 0, FreeKey(), -1, 0);
}

// The scan bound stays where it is: a reader in flight scans freed slots and finds nothing.
void BackpackFirstClear()
{
	const int n = (int)s_highWater;
	for (int i = 0; i < n; ++i)
		SlotWrite(i, 0, FreeKey(), -1, 0);
	InterlockedIncrement(&s_generation);
}

unsigned BackpackSettingsGeneration() { return (unsigned)s_generation; }

int BackpackFirstSlotCount()
{
	return (int)s_highWater;
}

int BackpackFirstEntryCount()
{
	const int n = (int)s_highWater;
	int used = 0;
	for (int i = 0; i < n; ++i)
		if (s_slots[i].used)
			++used;
	return used;
}

long BackpackFirstFullCount()
{
	return s_full;
}

long BackpackFirstRaceCount()
{
	return s_raced;
}

} // namespace keo_inventory
