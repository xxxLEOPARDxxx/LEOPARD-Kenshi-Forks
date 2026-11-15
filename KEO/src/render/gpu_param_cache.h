#pragma once
#include <stddef.h>

// The table behind the constant-definition cache. No Windows or game calls,
// so the host tests link it directly; the caller owns threading.

// A VS2010 x64 std::string as OgreMain and the D3D11 render system lay it
// out: the characters in place while res < 16, else a heap pointer; the
// allocator slot trails at +0x20, 40 bytes in all.
struct MsvcString
{
	union { char buf[16]; const char* ptr; } bx;
	size_t size;
	size_t res;
	const char* Data() const { return res < 16 ? bx.buf : bx.ptr; }
};

// std::map<std::string, GpuConstantDefinition>'s node holds the key string at
// +0x18 and the definition at +0x40, so the key sits this far before the
// definition getConstantDefinition returns.
static const size_t GPC_KEY_TO_DEF = 0x28;

class GpuParamCache
{
public:
	enum { SLOTS = 4096, PROBES = 16 };

	GpuParamCache();

	// The definition cached for (map, name), or NULL. A hit needs the
	// entry's recorded length and first bytes and the map node's own key to
	// equal *name in full, so a name object reused at the same address with
	// other text misses.
	const void* Find(const void* map, const MsvcString* name) const;
	// Records def, which must be the definition non-NULL map holds for *name. With no
	// free slot in the probe window it replaces the key's home slot.
	void Store(const void* map, const MsvcString* name, const void* def);
	// Drops every entry of map; returns how many.
	int DropMap(const void* map);
	void Clear();

private:
	struct Entry
	{
		const void*       map;
		const MsvcString* name;
		const void*       def;
		size_t            len;
		char              head[16];
	};
	static size_t Home(const void* map, const void* name);
	enum { MAP_BUCKETS = 1024 };
	static size_t MapBucket(const void* map);
	void UnlinkMapSlot(int index);
	Entry m_slots[SLOTS];
	// Encoded slot index +1, zero terminates. These fixed chains index only
	// occupied slots; they own no map/node references and never allocate.
	unsigned short m_mapHeads[MAP_BUCKETS];
	unsigned short m_mapNext[SLOTS];
	unsigned short m_mapPrev[SLOTS];
	int m_count;
};
