#include "base/log_admission.h"
#include "render/render_levers.h"
#include "render/render_config.h"
#include "render/module_hooks.h"
#include "render/gpu_param_cache.h"
#include "game/klib_member_contract.h"
#include "base/core.h"

// Ogre::GpuProgramParameters::getConstantDefinition, a string-keyed map
// lookup the D3D11 render system calls once per bound constant while setting
// up a pass. Patched through the D3D11 DLL's own import slot for the OgreMain
// export, rather than a code hook, since the call sites are in the D3D11 DLL
// (three `call [rip+X]` through the IAT slot) and only the callee itself is
// in OgreMain.
//
// The cache maps (params' GpuNamedConstants, name object) to the definition
// the original returned. Each name is a string owned by the D3D11 program's
// constant-buffer description, so its address is stable while the program
// lives; a definition is a map node's value, stable while its
// GpuNamedConstants lives. The admitted native control block deletes through
// the OgreMain function watched below. SDK/custom control blocks retain
// conservative assignment invalidation rather than assuming that delete path.

#ifdef KEO_DEBUG
static const bool DEV_BUILD = true;
#else
static const bool DEV_BUILD = false;
#endif

typedef const void* (*GetConstDef_t)(const void* params, const void* name);
typedef void        (*SetNamedConstants_t)(void* params, const void* namedConstantsPtr);
typedef void*       (*DeleteNamedConstants_t)(void* namedConstants, int flags);

static GetConstDef_t          s_origGetConstDef = NULL;
static SetNamedConstants_t    s_origSetNamed    = NULL;
static DeleteNamedConstants_t s_origDelete      = NULL;
static int                    s_patchState      = -1;    // -1 untried, 0 refused, 1 patched
static int                    s_watchState      = -1;    // the two invalidation hooks: -1 untried, 0 refused, 1 live
static bool                   s_cacheHooked     = false; // invalidation hooks and patch both live
// Only this admitted native control block is known to delete its map through
// our DAC30 hook. SDK/custom deleters can call another native destructor.
static const void*            s_nativeNamedInfoVtable = NULL;

// Counts GpuNamedConstants deletions, raised before the object is freed.
static volatile LONG s_namedConstantsDeleted = 0;

// Main thread only, apart from s_flushPending: another thread that deletes or
// replaces a GpuNamedConstants cannot touch the table, so it asks for a full
// clear before the next lookup instead.
static GpuParamCache s_cache;
static volatile LONG s_flushPending = 0;

static const char* const D3D11_MODULE = "RenderSystem_Direct3D11_x64.dll";
static const char* const OGRE_MODULE  = "OgreMain_x64.dll";
static const char* const GET_CONST_DEF_SYMBOL =
	"?getConstantDefinition@GpuProgramParameters@Ogre@@QEBAAEBUGpuConstantDefinition@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z";

static const ModuleSite s_setNamedSite =
{
	"GpuProgramParameters::_setNamedConstants", "OgreMain_x64.dll",
	"?_setNamedConstants@GpuProgramParameters@Ogre@@QEAAXAEBV?$SharedPtr@UGpuNamedConstants@Ogre@@@2@@Z",
	0xE89F0,
	{ 0x48,0x89,0x5C,0x24,0x10,0x57,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9,0x48,0x81,0xC1 }
};

// GpuNamedConstants' deleting destructor (this, flags), not exported. Both
// deleters a GpuNamedConstantsPtr is created with end here.
static const ModuleSite s_deleteSite =
{
	"GpuNamedConstants deleting destructor", "OgreMain_x64.dll", NULL,
	0xDAC30,
	{ 0x40,0x57,0x48,0x83,0xEC,0x30,0x48,0xC7,0x44,0x24,0x20,0xFE,0xFF,0xFF,0xFF,0x48 }
};

static const void* NamedConstantsOf(const void* params)
{
	return *(const void* const*)((const char*)params + KLIB_OFF_GpuProgramParameters_namedConstants);
}

static const void* VerifiedNativeNamedInfoVtable()
{
	HMODULE ogre = GetModuleHandleA(OGRE_MODULE);
	DWORD stamp = 0, size = 0;
	if (!ReadModuleImageId(ogre, &stamp, &size) || stamp != 0x5ca5f929 || size != 0x9c9000)
		return NULL;
	const uintptr_t base = (uintptr_t)ogre;
	static const unsigned char prefix[32] = {0x48,0x89,0x4c,0x24,0x08,0x57,0x48,0x83,0xec,0x30,0x48,0xc7,0x44,0x24,0x20,0xfe,0xff,0xff,0xff,0x48,0x89,0x5c,0x24,0x48,0x8b,0xfa,0x48,0x8b,0xd9,0x48,0x8d,0x05};
	static const unsigned char deletion[19] = {0x48,0x8b,0x49,0x28,0x48,0x85,0xc9,0x74,0x0b,0xba,0x01,0x00,0x00,0x00,0xe8,0x46,0x00,0x00,0x00};
	if (*(const uintptr_t*)(base + 0x684050) != base + 0xdabb0 ||
	    memcmp((const void*)(base + 0xdabb0), prefix, sizeof(prefix)) ||
	    memcmp((const void*)(base + 0xdabd7), deletion, sizeof(deletion)))
		return NULL;
	return (const void*)(base + 0x684050);
}

// Any thread.
static void InvalidateNamedConstants(const void* namedConstants)
{
	if (!namedConstants)
		return;
	if (IsMainThread())
		s_cache.DropMap(namedConstants);
	else
		InterlockedExchange(&s_flushPending, 1);
}

// Main thread: no logging, allocation or lock. The original throws when
// the name is missing; nothing is stored then, and this frame holds nothing
// to unwind. The hit and miss counts are touched only on this thread.
static const void* CachedLookup(const void* params, const void* name)
{
	if (!IsMainThread())
	{
		InterlockedIncrement(&g_renderStats.lookupOffThread);
		return s_origGetConstDef(params, name);
	}
	// The map is read before the flush check: a map freed and reallocated at
	// the same address was freed before params could point at it, so its
	// flush request is already visible here.
	const void* map = NamedConstantsOf(params);
	if (s_flushPending)
	{
		InterlockedExchange(&s_flushPending, 0);
		s_cache.Clear();
	}
	if (!map)
		return s_origGetConstDef(params, name);
	const MsvcString* key = (const MsvcString*)name;
	const void* def = s_cache.Find(map, key);
	if (def)
	{
		++g_renderStats.lookupHits;
		return def;
	}
	def = s_origGetConstDef(params, name);
	++g_renderStats.lookupMisses;
	if (def)
		s_cache.Store(map, key, def);
	return def;
}

static const void* Lookup(const void* params, const void* name)
{
	if (s_cacheHooked && g_renderCfg.gpuParamCache)
		return CachedLookup(params, name);
	return s_origGetConstDef(params, name);
}

static const void* hook_GetConstantDefinition(const void* params, const void* name)
{
	if (!DEV_BUILD || !g_renderCfg.gpuParamLookupDiag)
		return Lookup(params, name);
	LARGE_INTEGER t0, t1;
	QueryPerformanceCounter(&t0);
	const void* r = Lookup(params, name);
	QueryPerformanceCounter(&t1);
	InterlockedExchangeAdd64(&g_renderStats.lookupTicks, t1.QuadPart - t0.QuadPart);
	InterlockedIncrement(&g_renderStats.lookupCalls);
	return r;
}

// Definitions are keyed by the shared map, not by this params object. Native
// assignment replaces only the SharedPtr and grows params' value buffers; an
// old map retained by another owner stays valid. Skip invalidation only for the
// admitted native deleter whose actual final free is watched. SDK/custom control
// blocks and same-map rebinds retain existing invalidation, including external
// callers that edit a map in place before linking it again.
static void hook_SetNamedConstants(void* params, const void* namedConstantsPtr)
{
	const void* oldMap = NamedConstantsOf(params);
	const void* newMap = namedConstantsPtr ? *(const void* const*)namedConstantsPtr : NULL;
	if (oldMap)
	{
		bool invalidate = oldMap == newMap || !s_nativeNamedInfoVtable;
		if (!invalidate)
		{
			const void* info = *(const void* const*)((const char*)params + KLIB_OFF_GpuProgramParameters_namedConstants + sizeof(void*));
			invalidate = !info || *(const void* const*)info != s_nativeNamedInfoVtable;
		}
		if (invalidate)
			InvalidateNamedConstants(oldMap);
	}
	s_origSetNamed(params, namedConstantsPtr);
}

// Whichever thread drops the last reference: entries go before the nodes do.
static void* hook_DeleteNamedConstants(void* namedConstants, int flags)
{
	InterlockedIncrement(&s_namedConstantsDeleted);
	InvalidateNamedConstants(namedConstants);
	return s_origDelete(namedConstants, flags);
}

// Idempotent: the diagnostic and the cache share the one patch.
static bool PatchLookupSlot()
{
	if (s_patchState >= 0)
		return s_patchState == 1;
	s_patchState = 0;
	HMODULE d3d = GetModuleHandleA(D3D11_MODULE);
	HMODULE ogre = GetModuleHandleA(OGRE_MODULE);
	if (!d3d || !ogre)
	{
		LogError("Render: gpuParams: RenderSystem_Direct3D11_x64.dll or OgreMain_x64.dll not loaded");
		return false;
	}
	void* current = (void*)GetProcAddress(ogre, GET_CONST_DEF_SYMBOL);
	if (!current)
	{
		LogError("Render: gpuParams: OgreMain export missing: getConstantDefinition");
		return false;
	}
	// Set before the patch, not after: the CAS guarantees prev == current on
	// success, so the hook (which can fire the instant the slot is swapped)
	// never sees a NULL original.
	s_origGetConstDef = (GetConstDef_t)current;
	// PatchImportSlot's compare-and-swap refuses (returns NULL) when the slot
	// isn't found, or when it no longer holds the OgreMain export -- another
	// plugin (ReShade, say) patched it first. Either way this never chains
	// onto an unknown function.
	void* prev = PatchImportSlot(d3d, OGRE_MODULE, GET_CONST_DEF_SYMBOL,
	                              (void*)&hook_GetConstantDefinition, current);
	if (!prev)
	{
		s_origGetConstDef = NULL;
		LogError("Render: gpuParams: import slot missing or already patched, not installed");
		return false;
	}
	s_patchState = 1;
	return true;
}

bool InstallGpuParamsDiag()
{
	return PatchLookupSlot();
}

bool InstallGpuNamedConstantsWatch()
{
	if (s_watchState >= 0)
		return s_watchState == 1;
	s_watchState = 0;
	if (!InstallModuleHook(s_deleteSite, (void*)&hook_DeleteNamedConstants, (void**)&s_origDelete, NULL))
		return false;
	// Immutable metadata is ready before the setter wrapper can be called.
	// Unknown module/control-block contracts retain conservative invalidation.
	s_nativeNamedInfoVtable = VerifiedNativeNamedInfoVtable();
	if (!InstallModuleHook(s_setNamedSite, (void*)&hook_SetNamedConstants, (void**)&s_origSetNamed, NULL))
		return false;
	s_watchState = 1;
	return true;
}

LONG GpuNamedConstantsDeleted()
{
	return s_namedConstantsDeleted;
}

// The invalidation hooks go in before the cache can store anything, so no
// entry outlives its map unseen.
bool InstallGpuParamCache()
{
	if (s_cacheHooked)
		return true;
	if (!InstallGpuNamedConstantsWatch())
		return false;
	if (!PatchLookupSlot())
		return false;
	s_cacheHooked = true;
	return true;
}

bool GpuParamCacheActive()
{
	return s_cacheHooked && g_renderCfg.gpuParamCache;
}

// Main thread: zeroes the diagnostic's counters when it is switched off, and
// empties the table when the cache is, so switching it back on starts clean.
void GpuParams_MainThreadTick()
{
	static int diagWasOn = -1;   // -1 before the first tick
	static int cacheWasOn = -1;
	if (s_patchState != 1)
		return;
	int diagOn = g_renderCfg.gpuParamLookupDiag ? 1 : 0;
	if (diagOn != diagWasOn)
	{
		if (diagWasOn == 1)
		{
			InterlockedExchange(&g_renderStats.lookupCalls, 0);
			InterlockedExchange64(&g_renderStats.lookupTicks, 0);
		}
		diagWasOn = diagOn;
	}
	int cacheOn = GpuParamCacheActive() ? 1 : 0;
	if (cacheOn != cacheWasOn)
	{
		if (cacheWasOn == 1)
			s_cache.Clear();
		cacheWasOn = cacheOn;
	}
}
