#include "base/core.h"
#include "base/ini_names.h"
#include "base/log_admission.h"
#include "fixes/code_patch_batch.h"
#include "fixes/near_page.h"
#include "render/module_hooks.h"
#include "render/ogre_worker_policy.h"
#include "render/render_config.h"
#include "render/render_levers.h"
#include "render/scene_lever_policy.h"
#include <cstring>
#include <iomanip>
#include <sstream>

// Ogre queues the old skeletons after culling each view, including shadows and
// reflections. Requests 5 and 6 process those queues. Skip only a proven empty
// fork/join pair, inside Ogre's function, so its tag points, callbacks, and queue
// cleanup still run in the original order.
namespace
{
typedef void (*BarrierSync)(void*);
static BarrierSync s_sync = NULL;
static ForkPairState s_pairs[2];
static bool s_tried = false;
static bool s_installed = false;
static code_patch_threads::Set s_threads = {0};

static LONG s_calls = 0;
static LONG s_idle = 0;
static LONGLONG s_checkTicks = 0;

static const size_t kFirstWorkerRva = 0x2CA860;
static const size_t kSecondWorkerRva = 0x2CA8C0;
static const size_t kPhaseRva = 0x2CAE07;
static const size_t kCallOffsets[] = {20, 33, 58, 71};
static const size_t kStubStride = 32;
static const size_t kPageSize = 4096;

static const unsigned char kBarrierEntry[] =
{
	0x48,0x83,0xEC,0x28,0x48,0x8B,0x41,0x08,0x4C,0x8B,0xC9,0x48,0x89,0x44,0x24,0x30
};
static const unsigned char kDispatchRead[] =
{
	0x48,0x63,0x83,0x18,0x4B,0x00,0x00,0x83,0xF8,0x0C,0x0F,0x87,0xF1,0x00,0x00,0x00,
	0x41,0x8B,0x8C,0x84,0xB0,0xD7,0x2C,0x00,0x49,0x03,0xCC,0xFF,0xE1
};
static const unsigned char kDispatch[] =
{
	0x48,0x8B,0xD7,0x48,0x8B,0xCB,0xE8,0xA6,0xD1,0xFF,0xFF,0xEB,0x5A,
	0x48,0x8B,0xD7,0x48,0x8B,0xCB,0xE8,0xF9,0xD1,0xFF,0xFF,0xEB,0x4D
};
static const uint32_t kDispatchEntries[] = {0x2CD6AF, 0x2CD6BC};
static const unsigned char kFirstWorker[] =
{
	0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x48,0x8B,
	0x81,0x78,0x4B,0x00,0x00,0x4C,0x8D,0x04,0x52,0x4A,0x8B,0x1C,
	0xC0,0x4A,0x8B,0x4C,0xC0,0x08,0x48,0x8D,0x3C,0xCB,0x48,0x3B,
	0xDF,0x74,0x24,0x66,0x0F,0x1F,0x84,0x00,0x00,0x00,0x00,0x00,
	0x48,0x8B,0x0B,0x48,0x83,0xB9,0x00,0x03,0x00,0x00,0x00,0x74,
	0x05,0xE8,0x7E,0x6D,0xDF,0xFF,0x48,0x83,0xC3,0x08,0x48,0x3B,
	0xDF,0x75,0xE5,0x48,0x8B,0x5C,0x24,0x30,0x48,0x83,0xC4,0x20,0x5F,0xC3
};
static const unsigned char kPhases[] =
{
	0x41,0xC7,0x84,0x24,0x18,0x4B,0x00,0x00,0x05,0x00,0x00,0x00,
	0x49,0x8B,0x8C,0x24,0x20,0x4B,0x00,0x00,0xE8,0x20,0x51,0x11,
	0x00,0x49,0x8B,0x8C,0x24,0x20,0x4B,0x00,0x00,0xE8,0x13,0x51,
	0x11,0x00,0x41,0xC7,0x84,0x24,0x18,0x4B,0x00,0x00,0x06,0x00,
	0x00,0x00,0x49,0x8B,0x8C,0x24,0x20,0x4B,0x00,0x00,0xE8,0xFA,
	0x50,0x11,0x00,0x49,0x8B,0x8C,0x24,0x20,0x4B,0x00,0x00,0xE8,
	0xED,0x50,0x11,0x00
};

static int CurrentMode()
{
#ifdef KEO_DEBUG
	if (g_renderCfg.oldAnimDiag)
		return 2;
#endif
	return g_renderCfg.oldAnimSkip ? 1 : 0;
}

static void SyncOldAnimation(void* barrier, const unsigned char* scene, int site)
{
	if (IsMainThread() && s_installed)
	{
		ForkPairState& pair = s_pairs[site / 2];
		if (site & 1)
		{
			// A wait follows its own fire even if a setting changed.
			if (ForkWait(&pair, true) == FORK_SKIP)
				return;
		}
		else
		{
			const int mode = CurrentMode();
			if (mode)
			{
				LARGE_INTEGER start, end;
				if (mode == 2)
					QueryPerformanceCounter(&start);
				size_t parties = 0;
				if (barrier)
					memcpy(&parties, (const unsigned char*)barrier + OGRE_BARRIER_PARTIES, sizeof(parties));
				const bool idle = ForkOldAnimationIdle(scene, parties, site >= 2);
				if (mode == 2)
				{
					QueryPerformanceCounter(&end);
					s_checkTicks += end.QuadPart - start.QuadPart;
				}
				++s_calls;
				if (ForkFire(&pair, mode == 1, true, idle) == FORK_SKIP)
				{
					++s_idle;
					return;
				}
				if (mode == 2 && idle)
					++s_idle;
			}
		}
	}
	s_sync(barrier);
}

static bool Admit(HMODULE ogre)
{
	DWORD stamp = 0, size = 0;
	if (!IsMainThread() || !ReadModuleImageId(ogre, &stamp, &size) ||
	    stamp != OGRE_SCENE_BUILD_STAMP || size != OGRE_SCENE_BUILD_SIZE)
		return false;

	const unsigned char* base = (const unsigned char*)ogre;
	// These functions are exported by ordinal only in the admitted DLL.
	if ((const unsigned char*)GetProcAddress(ogre, MAKEINTRESOURCEA(8934)) != base + OGRE_BARRIER_SYNC_RVA ||
	    (const unsigned char*)GetProcAddress(ogre, MAKEINTRESOURCEA(9121)) != base + kFirstWorkerRva ||
	    (const unsigned char*)GetProcAddress(ogre, MAKEINTRESOURCEA(9122)) != base + kSecondWorkerRva)
		return false;

	unsigned char second[sizeof(kFirstWorker)];
	memcpy(second, kFirstWorker, sizeof(second));
	second[13] = 0x90; // read the second queue at SceneManager+0x4B90
	second[62] = 0x1E; // the second worker calls the same pose routine from 0x60 bytes later
	return memcmp(base + OGRE_BARRIER_SYNC_RVA, kBarrierEntry, sizeof(kBarrierEntry)) == 0 &&
	       memcmp(base + kFirstWorkerRva, kFirstWorker, sizeof(kFirstWorker)) == 0 &&
	       memcmp(base + kSecondWorkerRva, second, sizeof(second)) == 0 &&
	       memcmp(base + 0x2CD615, kDispatchRead, sizeof(kDispatchRead)) == 0 &&
	       memcmp(base + 0x2CD6AF, kDispatch, sizeof(kDispatch)) == 0 &&
	       memcmp(base + 0x2CD7C4, kDispatchEntries, sizeof(kDispatchEntries)) == 0 &&
	       memcmp(base + kPhaseRva, kPhases, sizeof(kPhases)) == 0;
}

static bool Arm(HMODULE ogre)
{
	const uintptr_t base = (uintptr_t)ogre;
	unsigned char* page = AllocateNearPages(base, kPageSize);
	if (!page)
		return false;

	code_patch_batch::Edit edits[4] = {};
	for (int i = 0; i < 4; ++i)
	{
		unsigned char* stub = page + kStubStride * i;
		code_patch_batch::Edit& edit = edits[i];
		edit.address = (unsigned char*)(base + kPhaseRva + kCallOffsets[i]);
		edit.size = 5;
		memcpy(edit.before, kPhases + kCallOffsets[i], edit.size);
		int32_t rel = 0;
		if (BuildForkStub(stub, kStubStride, FORK_REG_R12, i, (uintptr_t)&SyncOldAnimation) != FORK_STUB_BYTES ||
		    !Rel32To((uintptr_t)edit.address + edit.size, (uintptr_t)stub, &rel))
		{
			VirtualFree(page, 0, MEM_RELEASE);
			return false;
		}
		edit.after[0] = 0xE8;
		memcpy(edit.after + 1, &rel, sizeof(rel));
	}

	DWORD previous = 0;
	if (!VirtualProtect(page, kPageSize, PAGE_EXECUTE_READ, &previous) ||
	    !FlushInstructionCache(GetCurrentProcess(), page, 4 * kStubStride))
	{
		VirtualFree(page, 0, MEM_RELEASE);
		return false;
	}

	s_sync = (BarrierSync)(base + OGRE_BARRIER_SYNC_RVA);
	bool mayPublish = false;
	const bool armed = code_patch_batch::Apply(edits, 4, &s_threads, &mayPublish);
	if (!mayPublish)
		VirtualFree(page, 0, MEM_RELEASE);
	// Keep a possibly referenced stub page for the lifetime of Ogre. Until
	// successful admission every stub unconditionally forwards to Barrier::sync.
	if (armed)
		s_pairs[0].live = s_pairs[1].live = true;
	return armed;
}
}

bool InstallOldAnimLever()
{
	if (s_tried)
		return s_installed;
	s_tried = true;

	HMODULE ogre = GetModuleHandleA("OgreMain_x64.dll");
	if (!ogre || !Admit(ogre))
	{
		LogError("Render: oldAnimSkip: native animation worker contract refused");
		return false;
	}
	if (!code_patch_threads::Prepare((GetDLLDirectory() + OPTIMIZER_LOG_NAME).c_str()) || !Arm(ogre))
	{
		LogError("Render: oldAnimSkip: could not install paired animation worker hooks");
		return false;
	}
	s_installed = true;
	return true;
}

void OldAnim_MainThreadTick()
{
	static int previousMode = -1;
	const int mode = CurrentMode();
	if (!s_installed || previousMode == mode)
		return;
	previousMode = mode;
	s_calls = s_idle = 0;
	s_checkTicks = 0;
}

std::string OldAnimStatsToken(LONG loops)
{
	const LONG calls = s_calls, idle = s_idle;
	const LONGLONG ticks = s_checkTicks;
	s_calls = s_idle = 0;
	s_checkTicks = 0;
	const int mode = CurrentMode();
	if (!DetailedLogEnabled() || !s_installed || !mode)
		return std::string();
	const double perLoop = loops > 0 ? 1.0 / loops : 0.0;
	std::ostringstream ss;
	ss.setf(std::ios::fixed);
	ss << std::setprecision(1) << " oldAnim=" << calls * perLoop << "/" << idle * perLoop << "/loop";
	if (mode == 2)
	{
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		const double us = freq.QuadPart && calls > 0 ? (double)ticks * 1e6 / freq.QuadPart / calls : 0.0;
		ss << " scan=" << us << "us";
	}
	return ss.str();
}
