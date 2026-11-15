#include "render/gpu_param_cache.h"
#include <string.h>

GpuParamCache::GpuParamCache()
{
	Clear();
}

// Both keys are heap addresses in regular strides with idle low bits; the
// mix spreads every input bit over the index.
size_t GpuParamCache::Home(const void* map, const void* name)
{
	unsigned long long x = (unsigned long long)(size_t)map * 0x9E3779B97F4A7C15ull ^ (unsigned long long)(size_t)name;
	x ^= x >> 31;
	x *= 0xBF58476D1CE4E5B9ull;
	x ^= x >> 29;
	return (size_t)x & (SLOTS - 1);
}

size_t GpuParamCache::MapBucket(const void* map)
{
	return Home(map, NULL) & (MAP_BUCKETS - 1);
}

void GpuParamCache::UnlinkMapSlot(int index)
{
	// Store's home-slot replacement must stay fixed work even when many
	// maps share a bucket. Only DropMap needs to walk a chain.
	unsigned short previous = m_mapPrev[index];
	unsigned short next = m_mapNext[index];
	if (previous)
		m_mapNext[previous - 1] = next;
	else
		m_mapHeads[MapBucket(m_slots[index].map)] = next;
	if (next)
		m_mapPrev[next - 1] = previous;
}

const void* GpuParamCache::Find(const void* map, const MsvcString* name) const
{
	size_t h = Home(map, name);
	for (int i = 0; i < PROBES; ++i)
	{
		const Entry& e = m_slots[(h + i) & (SLOTS - 1)];
		if (e.map != map || e.name != name)
			continue;
		size_t len = name->size;
		if (len != e.len)
			return NULL;
		const char* text = name->Data();
		if (memcmp(text, e.head, len < 16 ? len : 16) != 0)
			return NULL;
		const MsvcString* key = (const MsvcString*)((const char*)e.def - GPC_KEY_TO_DEF);
		if (key->size != len || memcmp(key->Data(), text, len) != 0)
			return NULL;
		return e.def;
	}
	return NULL;
}

void GpuParamCache::Store(const void* map, const MsvcString* name, const void* def)
{
	size_t h = Home(map, name);
	Entry* slot = NULL;
	for (int i = 0; i < PROBES && !slot; ++i)
	{
		Entry& e = m_slots[(h + i) & (SLOTS - 1)];
		if (e.map == map && e.name == name)
			slot = &e;
	}
	for (int i = 0; i < PROBES && !slot; ++i)
	{
		Entry& e = m_slots[(h + i) & (SLOTS - 1)];
		if (!e.map)
			slot = &e;
	}
	if (!slot)
		slot = &m_slots[h];
	if (slot->map != map)
	{
		int index = (int)(slot - &m_slots[0]);
		if (slot->map)
			UnlinkMapSlot(index);
		else
			++m_count;
		size_t bucket = MapBucket(map);
		m_mapNext[index] = m_mapHeads[bucket];
		m_mapPrev[index] = 0;
		if (m_mapHeads[bucket])
			m_mapPrev[m_mapHeads[bucket] - 1] = (unsigned short)(index + 1);
		m_mapHeads[bucket] = (unsigned short)(index + 1);
	}
	size_t len = name->size;
	slot->map = map;
	slot->name = name;
	slot->def = def;
	slot->len = len;
	memset(slot->head, 0, sizeof(slot->head));
	memcpy(slot->head, name->Data(), len < 16 ? len : 16);
}

int GpuParamCache::DropMap(const void* map)
{
	// NULL denotes empty storage, not an admitted map. Preserve the legacy
	// return count for that input without putting empty slots in the index.
	if (!map)
		return SLOTS - m_count;
	if (!m_count)
		return 0;
	int dropped = 0;
	unsigned short* link = &m_mapHeads[MapBucket(map)];
	// Every occupied slot appears once in one chain (at most SLOTS visits).
	// Different maps sharing the bucket stay linked and are never dereferenced.
	while (*link)
	{
		int i = *link - 1;
		Entry& e = m_slots[i];
		if (e.map != map)
		{
			link = &m_mapNext[i];
			continue;
		}
		*link = m_mapNext[i];
		if (m_mapNext[i])
			m_mapPrev[m_mapNext[i] - 1] = m_mapPrev[i];
		m_mapNext[i] = 0;
		m_mapPrev[i] = 0;
		memset(&e, 0, sizeof(e));
		--m_count;
		++dropped;
	}
	return dropped;
}

void GpuParamCache::Clear()
{
	memset(m_slots, 0, sizeof(m_slots));
	memset(m_mapHeads, 0, sizeof(m_mapHeads));
	memset(m_mapNext, 0, sizeof(m_mapNext));
	memset(m_mapPrev, 0, sizeof(m_mapPrev));
	m_count = 0;
}
