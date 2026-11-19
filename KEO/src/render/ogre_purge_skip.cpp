#include "base/log_admission.h"
#include "render/render_levers.h"
#include "render/render_config.h"
#include "render/module_hooks.h"
#include "render/ogre_purge_policy.h"
#include "zone/handoff/zone_handoff.h"
#include "zone/transition.h"
#include "base/core.h"
#include "game/native_profile.h"
#include <iomanip>
#include <sstream>
#include <intrin.h>
#include <cstring>
#pragma intrinsic(_ReturnAddress)

// ZoneManager::processLoading's 3->4 phase branch purges Ogre's unreferenced
// resources once per loading cycle: four Ogre::ResourceGroupManager group
// unloads (General, foliage, Landscape, Overlaymaps) and, through the same
// base-class implementation, one TextureManager and one MeshManager
// unloadUnreferencedResources call. A cycle the handoff mechanism raised to
// move a cohort of prepared cells into the game's own tracking runs this
// same purge for no player-visible reason; skipping it there, with a timed
// fallback so the skip can never run forever, is this lever's whole job.
// What the purge does when it runs is untouched -- the original is called
// exactly as the game would have.

typedef void (*UnloadGroup_t)(void* mgr, const void* groupName, bool reloadableOnly);
typedef void (*UnloadUnreferenced_t)(void* mgr, bool reloadableOnly);

static const ModuleSite s_groupSite =
{
	"ResourceGroupManager::unloadUnreferencedResourcesInGroup", "OgreMain_x64.dll",
	"?unloadUnreferencedResourcesInGroup@ResourceGroupManager@Ogre@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N@Z",
	0,
	{ 0x48,0x8B,0xC4,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x48,0x81,0xEC,0x20,0x02 }
};

// ResourceManager's own implementation, exported once and shared by every
// resource manager that does not override it -- TextureManager and
// MeshManager both call in here for their 3->4 unload.
static const ModuleSite s_unrefSite =
{
	"ResourceManager::unloadUnreferencedResources", "OgreMain_x64.dll",
	"?unloadUnreferencedResources@ResourceManager@Ogre@@UEAAX_N@Z",
	0,
	{ 0x40,0x57,0x48,0x83,0xEC,0x40,0x48,0xC7,0x44,0x24,0x20,0xFE,0xFF,0xFF,0xFF,0x48 }
};

static UnloadGroup_t        s_origGroup = NULL;
static UnloadUnreferenced_t s_origUnref = NULL;
static bool                 s_tried     = false;
static bool                 s_installed = false;

// Independent clocks for the six processLoading purge call sites. The first
// fallback must not reset the deadline of the other five resources. Keys are
// immutable native return addresses; no group strings or manager pointers live
// beyond a call. Unknown callers and unsupported images always run native.
struct PurgeCaller { uintptr_t returnRva; bool group; unsigned char before[32]; };
static const PurgeCaller s_steamCallers[6] = {
 {0xA0EB6B,false,{0x33,0xd2,0x48,0x8d,0x0d,0x5c,0x45,0x72,0x01,0xe8,0xa6,0x2a,0x64,0xff,0xff,0x15,0x89,0x92,0x83,0x01,0x4c,0x8b,0x00,0x33,0xd2,0x48,0x8b,0xc8,0x41,0xff,0x50,0x68}},
 {0xA0EB7D,false,{0x83,0x01,0x4c,0x8b,0x00,0x33,0xd2,0x48,0x8b,0xc8,0x41,0xff,0x50,0x68,0xff,0x15,0x37,0x81,0x83,0x01,0x4c,0x8b,0x00,0xb2,0x01,0x48,0x8b,0xc8,0x41,0xff,0x50,0x68}},
 {0xA0EBBA,true,{0x48,0x8d,0x4d,0xe7,0xe8,0x22,0xec,0x63,0xff,0x90,0xff,0x15,0xd6,0x80,0x83,0x01,0x48,0x8b,0xc8,0x41,0xb0,0x01,0x48,0x8d,0x55,0xe7,0xff,0x15,0x66,0x76,0x83,0x01}},
 {0xA0EC08,true,{0x48,0x8d,0x4d,0xe7,0xe8,0xd4,0xeb,0x63,0xff,0x90,0xff,0x15,0x88,0x80,0x83,0x01,0x48,0x8b,0xc8,0x41,0xb0,0x01,0x48,0x8d,0x55,0xe7,0xff,0x15,0x18,0x76,0x83,0x01}},
 {0xA0EC56,true,{0x48,0x8d,0x4d,0xe7,0xe8,0x86,0xeb,0x63,0xff,0x90,0xff,0x15,0x3a,0x80,0x83,0x01,0x48,0x8b,0xc8,0x41,0xb0,0x01,0x48,0x8d,0x55,0xe7,0xff,0x15,0xca,0x75,0x83,0x01}},
 {0xA0ECA4,true,{0x48,0x8d,0x4d,0xe7,0xe8,0x38,0xeb,0x63,0xff,0x90,0xff,0x15,0xec,0x7f,0x83,0x01,0x48,0x8b,0xc8,0x41,0xb0,0x01,0x48,0x8d,0x55,0xe7,0xff,0x15,0x7c,0x75,0x83,0x01}}
};
// GOG 1.0.65 uses the same group/manager order at different native addresses.
static const PurgeCaller s_gogCallers[6] = {
 {0xA0E28B,false,{0x33,0xd2,0x48,0x8d,0x0d,0xac,0x2d,0x72,0x01,0xe8,0xfe,0x33,0x64,0xff,0xff,0x15,0xb1,0x7a,0x83,0x01,0x4c,0x8b,0x00,0x33,0xd2,0x48,0x8b,0xc8,0x41,0xff,0x50,0x68}},
 {0xA0E29D,false,{0x83,0x01,0x4c,0x8b,0x00,0x33,0xd2,0x48,0x8b,0xc8,0x41,0xff,0x50,0x68,0xff,0x15,0x8f,0x69,0x83,0x01,0x4c,0x8b,0x00,0xb2,0x01,0x48,0x8b,0xc8,0x41,0xff,0x50,0x68}},
 {0xA0E2DA,true,{0x48,0x8d,0x4d,0xe7,0xe8,0x7a,0xf5,0x63,0xff,0x90,0xff,0x15,0x2e,0x69,0x83,0x01,0x48,0x8b,0xc8,0x41,0xb0,0x01,0x48,0x8d,0x55,0xe7,0xff,0x15,0xb6,0x5e,0x83,0x01}},
 {0xA0E328,true,{0x48,0x8d,0x4d,0xe7,0xe8,0x2c,0xf5,0x63,0xff,0x90,0xff,0x15,0xe0,0x68,0x83,0x01,0x48,0x8b,0xc8,0x41,0xb0,0x01,0x48,0x8d,0x55,0xe7,0xff,0x15,0x68,0x5e,0x83,0x01}},
 {0xA0E376,true,{0x48,0x8d,0x4d,0xe7,0xe8,0xde,0xf4,0x63,0xff,0x90,0xff,0x15,0x92,0x68,0x83,0x01,0x48,0x8b,0xc8,0x41,0xb0,0x01,0x48,0x8d,0x55,0xe7,0xff,0x15,0x1a,0x5e,0x83,0x01}},
 {0xA0E3C4,true,{0x48,0x8d,0x4d,0xe7,0xe8,0x90,0xf4,0x63,0xff,0x90,0xff,0x15,0x44,0x68,0x83,0x01,0x48,0x8b,0xc8,0x41,0xb0,0x01,0x48,0x8d,0x55,0xe7,0xff,0x15,0xcc,0x5d,0x83,0x01}}
};
static const PurgeCaller* s_callers = NULL;
static uintptr_t s_callerBase = 0;
static double s_lastPurgeAt[6] = {0,0,0,0,0,0};

static bool AdmitPurgeCallers()
{
    HMODULE exe=GetModuleHandleA(NULL);DWORD stamp=0,size=0;
    if(!ReadModuleImageId(exe,&stamp,&size))return false;
    const bool gog=NativeProfileIsGog();
    if(gog?(stamp!=0x65d60519||size!=0x232a000):(stamp!=0x65d604d7||size!=0x232c000))return false;
    const PurgeCaller* callers=gog?s_gogCallers:s_steamCallers;
    const uintptr_t base=reinterpret_cast<uintptr_t>(exe);
    for(unsigned i=0;i<6;++i)
        if(callers[i].returnRva>size||callers[i].returnRva<32||
           memcmp(reinterpret_cast<const void*>(base+callers[i].returnRva-32),callers[i].before,32))return false;
    s_callers=callers;
    s_callerBase=base;
    return true;
}

static int PurgeCallerIndex(const void*caller,bool group)
{
    const uintptr_t address=reinterpret_cast<uintptr_t>(caller);
    if(!s_callerBase||address<s_callerBase)return -1;
    const uintptr_t rva=address-s_callerBase;
    for(int i=0;i<6;++i)if(s_callers[i].returnRva==rva&&s_callers[i].group==group)return i;
    return -1;
}

static volatile LONG s_calls    = 0;   // every intercepted call, skip key on or off
static volatile LONG s_skipped  = 0;
static volatile LONG s_fallback = 0;

// True only while a real transition never joined the cycle now running: a
// real transition's purge always runs, whatever the ledger says about how
// the cycle started.
static bool Eligible()
{
	return ZoneHandoffAdoptionCycleInFlight() && !isTransitionActive;
}

// All six clocks start at install, preserving the cold eligible skip window.
// Each successful decision changes only its own target's clock.
static bool ShouldRun(double now,const void*caller,bool group)
{
	InterlockedIncrement(&s_calls);
	if(!IsMainThread()||!s_installed)return true;
	const int channel=PurgeCallerIndex(caller,group);
	if(channel<0)return true;
	if (!g_renderCfg.adoptOgrePurgeSkip)
	{
		s_lastPurgeAt[channel] = now;
		return true;
	}
	OgrePurgeDecision d = OgrePurgeSkipDecide(Eligible(), now, s_lastPurgeAt[channel],
	                                          (double)g_renderCfg.adoptOgrePurgeMaxSkipSeconds);
	if (d.action == OGREPURGE_SKIP)
	{
		InterlockedIncrement(&s_skipped);
		return false;
	}
	s_lastPurgeAt[channel] = now;
	if (d.countFallback)
		InterlockedIncrement(&s_fallback);
	return true;
}

static void hook_UnloadGroup(void* mgr, const void* groupName, bool reloadableOnly)
{
	if (ShouldRun(ElapsedSec(),_ReturnAddress(),true))
		s_origGroup(mgr, groupName, reloadableOnly);
}

static void hook_UnloadUnreferenced(void* mgr, bool reloadableOnly)
{
	if (ShouldRun(ElapsedSec(),_ReturnAddress(),false))
		s_origUnref(mgr, reloadableOnly);
}

bool InstallOgrePurgeSkip()
{
	if (s_tried)
		return s_installed;
	s_tried = true;
	if(!AdmitPurgeCallers())
	{
		LogError("Render: adoptOgrePurgeSkip: native purge callers not admitted; normal resource purging retained");
		return false;
	}
	HMODULE ogre = GetModuleHandleA(s_groupSite.module);
	if (!ogre)
	{
		LogError("Render: adoptOgrePurgeSkip: OgreMain not loaded");
		return false;
	}
	bool group = InstallModuleHook(s_groupSite, (void*)&hook_UnloadGroup, (void**)&s_origGroup, NULL);
	bool unref = InstallModuleHook(s_unrefSite, (void*)&hook_UnloadUnreferenced, (void**)&s_origUnref, NULL);
	s_installed = group && unref;
	if (s_installed)
	{
		const double now=ElapsedSec();
		for(int i=0;i<6;++i)s_lastPurgeAt[i]=now;
	}
	return s_installed;
}

std::string OgrePurgeStatsToken()
{
	LONG calls    = InterlockedExchange(&s_calls, 0);
	LONG skipped  = InterlockedExchange(&s_skipped, 0);
	LONG fallback = InterlockedExchange(&s_fallback, 0);
	if (!DetailedLogEnabled()) return std::string();
	if (!s_installed)
		return std::string();
	std::ostringstream ss;
	ss << " ogrePurge=" << calls << "/" << skipped << "/" << fallback;
	return ss.str();
}
