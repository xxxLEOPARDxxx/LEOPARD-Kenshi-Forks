#include "base/log_admission.h"
#include "render/module_hooks.h"
#include "base/core.h"
#include "plugin/hook_manifest.h"
#include <cstdio>
#include <cstring>

void* ResolveModuleSite(const ModuleSite& site)
{
	HMODULE m = GetModuleHandleA(site.module);
	if (!m)
		return NULL;
	if (site.symbol)
		return (void*)GetProcAddress(m, site.symbol);
	return (void*)((uintptr_t)m + site.rva);
}

static bool CheckModuleHookSite(const ModuleSite& site, void* target, bool* sharedOut, char (&where)[96])
{
	if (sharedOut) *sharedOut = false;
	if (!target)
	{
		char line[160];
		_snprintf_s(line, sizeof(line), _TRUNCATE, "Render gate: %s not found in %s", site.name, site.module);
		LogError(line);
		return false;
	}
	_snprintf_s(where, sizeof(where), _TRUNCATE, "%s+0x%llx", site.module,
	            (unsigned long long)((uintptr_t)target - (uintptr_t)GetModuleHandleA(site.module)));
	return VerifyPrologueAt(target, site.bytes, site.name, where, sharedOut);
}

bool AdmitModuleHookSite(const ModuleSite& site, void* target)
{
	char where[96];
	return CheckModuleHookSite(site, target, NULL, where);
}

static bool EnableModuleHook(const ModuleSite& site, void* target, void* detour,
                             void** original, const char* where)
{
	if (KenshiLib::AddHook(target, detour, original) != KenshiLib::SUCCESS)
	{
		char line[160];
		_snprintf_s(line, sizeof(line), _TRUNCATE, "Render gate: %s AddHook failed at %s", site.name, where);
		LogError(line);
		return false;
	}
	return true;
}

bool InstallModuleHook(const ModuleSite& site, void* detour, void** orig, bool* sharedOut)
{
	char where[96];
	void* target = ResolveModuleSite(site);
	return CheckModuleHookSite(site, target, sharedOut, where)
	    && EnableModuleHook(site, target, detour, orig, where);
}

void InstallStartupModuleHook(StartupAuxHookId id, const ModuleSite& site,
                              void* detour, void** original, bool* sharedOut,
                              StartupAuxHookCompletion done)
{
	char where[96];
	void* target = ResolveModuleSite(site);
	if (!CheckModuleHookSite(site, target, sharedOut, where))
	{
		done(id, "hook");
		return;
	}
	if (QueueStartupAuxHook(id, target, detour, original, done))
		return;
	const bool installed = EnableModuleHook(site, target, detour, original, where);
	done(id, installed ? (*original ? NULL : "original") : "hook");
}

void* PatchImportSlot(HMODULE importer, const char* exporterDll, const char* symbol,
                      void* replacement, void* expectedCurrent)
{
	void** slot = FindImportSlot(importer, exporterDll, symbol);
	if (!slot)
		return NULL;

	MEMORY_BASIC_INFORMATION mbi;
	if (VirtualQuery(slot, &mbi, sizeof(mbi)) != sizeof(mbi))
		return NULL;
	bool executable = (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
	                                   PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
	DWORD writable = executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;

	DWORD old = 0;
	if (!VirtualProtect(slot, sizeof(void*), writable, &old))
		return NULL;

	void* prev;
	if (expectedCurrent)
		prev = InterlockedCompareExchangePointer(slot, replacement, expectedCurrent);
	else
		prev = InterlockedExchangePointer(slot, replacement);

	DWORD ignored = 0;
	VirtualProtect(slot, sizeof(void*), old, &ignored);

	if (expectedCurrent && prev != expectedCurrent)
		return NULL;
	return prev;
}

bool ReadModuleImageId(HMODULE m, DWORD* stamp, DWORD* size)
{
	if (!m)
		return false;
	const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)m;
	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
		return false;
	const IMAGE_NT_HEADERS64* nt = (const IMAGE_NT_HEADERS64*)((const char*)m + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE)
		return false;
	if (stamp) *stamp = nt->FileHeader.TimeDateStamp;
	if (size) *size = nt->OptionalHeader.SizeOfImage;
	return true;
}
