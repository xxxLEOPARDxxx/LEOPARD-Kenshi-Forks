#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#include <Windows.h>

#include <core/Functions.h>

#pragma warning(push)
#pragma warning(disable : 4091) // '__declspec(dllimport)' ignored (BaseLayout.h)
#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/SaveFileSystem.h>
#include <kenshi/Inventory.h>
#include <kenshi/gui/InventoryGUI.h>
// InventoryTraderGUI detection is via vtable pointer comparison (no header needed)
#pragma warning(pop)

#include <mygui/MyGUI_Widget.h>

#include <string>
#include <vector>

#include "ResultCache.h"
#include "SortWorker.h"
#include "StackSort.h"
#include "StackSortUI.h"
#include "ModPath.h"
#include "StackSortSettings.h"
#include "McmSettings.h"
#include "Translate.h"
#include "Version.h"
#include "Log.h"

#ifdef STACKSORT_VERBOSE
#include "CatalogDump.h"
#endif

#ifdef STACKSORT_ENABLE_ROTATION
typedef int (*KR_ApiVersionFn)();
typedef int (*KR_IsRotatedFn)(void*);
typedef int (*KR_CanRotateFn)(void*);
typedef int (*KR_SetRotatedFn)(void*, int, void*);
typedef void (*KR_RefreshVisualsFn)(void*);

static KR_ApiVersionFn s_krApiVersion         = NULL;
static KR_IsRotatedFn s_krIsRotated           = NULL;
static KR_CanRotateFn s_krCanRotate           = NULL;
static KR_SetRotatedFn s_krSetRotated         = NULL;
static KR_RefreshVisualsFn s_krRefreshVisuals = NULL;
static bool s_krAvailable                     = false;
#endif

// Resolved from kenshi_x64.exe at startup.

typedef unsigned long (*AK_PostEvent_t)(const char*,      // eventName
                                        unsigned __int64, // gameObjectID
                                        unsigned long,    // flags
                                        void*,            // callback
                                        void*,            // cookie
                                        unsigned long,    // numExternals
                                        void*,            // externalSources
                                        unsigned long     // playingID
);

static AK_PostEvent_t s_akPostEvent = NULL;
static bool s_subsystemsReady = false;
static bool s_lifecycleReady = false;
static volatile LONG s_worldWorkDepth = 0;
static void (*s_origReset)(GameWorld*) = NULL;
static void (*s_origLoad)(SaveFileSystem*, const std::string&) = NULL;
static void (*s_origNew)(SaveFileSystem*) = NULL;
static void CancelWorldWork()
{
    if (!s_subsystemsReady) return;
    SortWorker::OnWorldReset();
    StackSortUI::OnWorldReset();
}
struct WorldWorkGuard
{
    WorldWorkGuard() { InterlockedIncrement(&s_worldWorkDepth); CancelWorldWork(); }
    ~WorldWorkGuard() { InterlockedDecrement(&s_worldWorkDepth); }
};
static void HookWorldReset(GameWorld* world)
{
    WorldWorkGuard guard;
    s_origReset(world);
}
static void HookWorldLoad(SaveFileSystem* fs, const std::string& path)
{
    WorldWorkGuard guard;
    s_origLoad(fs, path);
}
static void HookWorldNew(SaveFileSystem* fs)
{
    WorldWorkGuard guard;
    s_origNew(fs);
}

// The base InventoryGUI vtable, latched from a GUI that is provably not a
// trader subclass (see LearnBaseVtable). Any GUI with a different vtable is a
// subclass (trader); containers share the base vtable. Fails closed: until it
// is known every GUI counts as a trader, so a trader panel that updates first
// is never handed our buttons. Main thread only (both callers are game hooks).
static void** s_baseVtable = NULL;

// Real (game-process) address of the body of InventoryGUI::getInventory,
// resolved at startup. InventoryTraderGUI overrides getInventory, so a GUI
// whose vtable slot leads to this body is the base class. NULL when
// unresolved, which only disables that evidence source.
static void* s_baseGetInventory = NULL;

// getInventory sits at vtable offset 0x70 (InventoryGUI.h), 8-byte slots.
static const int GETINVENTORY_VTABLE_SLOT = 0x70 / 8;

// kenshi_x64.exe is incrementally linked: vtable slots hold `jmp rel32` (E9)
// thunks, never the function body KenshiLib's RVAs name. One hop reaches the
// body on both Steam and GOG builds; the slack covers a second thunk layer.
static const int MAX_THUNK_HOPS = 3;

static bool s_vtableMismatchLogged = false;

static bool isTraderGUI(InventoryGUI* gui)
{
    if (!s_baseVtable) return true;
    return *(void***)gui != s_baseVtable;
}

// True when code at entry is target, or reaches it through a chain of E9
// thunks. Compares before following each hop, so an inline detour another
// mod placed on the target body does not carry the walk past it.
// Target = target + 5 + rel32 (rel32 is signed, relative to the next
// instruction). SEH-guarded: reads only bytes at code addresses the game's own
// vtable points at, but a crash here would take the game down for a heuristic.
// No C++ objects with destructors may live in this function (__try rule).
static bool CodeReachesTarget(void* entry, void* target)
{
    __try
    {
        const unsigned char* p = (const unsigned char*)entry;
        for (int hop = 0; hop <= MAX_THUNK_HOPS; ++hop)
        {
            if (p == (const unsigned char*)target) return true;
            if (p == NULL || p[0] != 0xE9) return false;
            p = p + 5 + *(const int*)(p + 1);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return false;
}

// gui must be a live top-level GUI (ownerInventory == NULL) seen from its own
// update or a click on it. Latches the base vtable only from definite
// evidence, never from "first GUI seen":
//   1. its getInventory slot leads to the base implementation (a trader
//      overrides it), which also covers characters without a backpack and
//      containers;
//   2. only when the getInventory body could not be resolved: it is a body GUI,
//      a callback object plus a backpack child GUI that names it as owner (a
//      trader panel shows its stock in a scroll view).
// Main thread only.
static void LearnBaseVtable(InventoryGUI* gui)
{
    void** vt = *(void***)gui;

    bool bodyShaped =
        gui->callbackObject != NULL && gui->childInventory != NULL && gui->childInventory->ownerInventory == gui;

    if (s_baseVtable)
    {
        if (bodyShaped && vt != s_baseVtable && !s_vtableMismatchLogged)
        {
            s_vtableMismatchLogged = true;
            LogError("[StackSort] A body GUI has a different vtable than the captured base; it is treated as a trader");
        }
        return;
    }

    // When the getInventory body is resolved its verdict is authoritative: hooks
    // detour the body, not the vtable slot, so a slot that does not reach it is
    // a trader override even if the GUI happens to look body-shaped.
    if (s_baseGetInventory)
    {
        if (CodeReachesTarget(vt[GETINVENTORY_VTABLE_SLOT], s_baseGetInventory))
        {
            s_baseVtable = vt;
            LogInfo("[StackSort] Base InventoryGUI vtable learned (getInventory evidence)");
        }
    }
    else if (bodyShaped)
    {
        s_baseVtable = vt;
        LogInfo("[StackSort] Base InventoryGUI vtable learned (body GUI evidence)");
    }
}

#ifdef STACKSORT_ENABLE_ROTATION
void StackSort_InitRotateAPI()
{
    HMODULE hMod = GetModuleHandleA("KenshiRotate.dll");
    if (!hMod)
    {
        LogInfo("[StackSort] KenshiRotate not detected (rotation disabled)");
        return;
    }

    s_krApiVersion     = (KR_ApiVersionFn)GetProcAddress(hMod, "KenshiRotate_ApiVersion");
    s_krIsRotated      = (KR_IsRotatedFn)GetProcAddress(hMod, "KenshiRotate_IsRotated");
    s_krCanRotate      = (KR_CanRotateFn)GetProcAddress(hMod, "KenshiRotate_CanRotate");
    s_krSetRotated     = (KR_SetRotatedFn)GetProcAddress(hMod, "KenshiRotate_SetRotated");
    s_krRefreshVisuals = (KR_RefreshVisualsFn)GetProcAddress(hMod, "KenshiRotate_RefreshVisuals");

    if (!s_krApiVersion || !s_krIsRotated || !s_krCanRotate || !s_krSetRotated)
    {
        LogError("[StackSort] KenshiRotate found but missing API exports (rotation disabled)");
        return;
    }

    int ver = s_krApiVersion();
    if (ver < 1)
    {
        LogError("[StackSort] KenshiRotate API version " + IntToStr(ver) + " < 1 (rotation disabled)");
        return;
    }

    s_krAvailable = true;
    LogInfo("[StackSort] KenshiRotate detected (API v" + IntToStr(ver) + ", rotation enabled)");
}

bool StackSort_RotateAvailable()
{
    return s_krAvailable;
}

bool StackSort_CanRotate(Item* item)
{
    return s_krAvailable && s_krCanRotate((void*)item) != 0;
}

bool StackSort_IsRotated(Item* item)
{
    return s_krAvailable && s_krIsRotated((void*)item) != 0;
}

bool StackSort_SetRotated(Item* item, bool rotated)
{
    return s_krAvailable && s_krSetRotated((void*)item, rotated ? 1 : 0, NULL) != 0;
}

void StackSort_RefreshVisuals(InventoryGUI* gui)
{
    if (s_krAvailable && s_krRefreshVisuals) s_krRefreshVisuals((void*)gui);
}
#endif

// Startup problems the player can act on. Kenshi has no GUI yet during
// startPlugin, so the message is deferred to the first inventory frame.
// Main thread only (both call sites are game hooks).
static std::vector<std::string> s_failedHooks;
static int s_configWarnings = 0;
static bool s_startupWarned = false;

static void WarnStartupIssuesOnce()
{
    if (s_startupWarned) return;

    // Latch only once a message could actually be shown: ou is NULL until the
    // game world exists, and latching there would permanently disable both this
    // call site and the hook_autoArrangeButton fallback.
    if (!ou) return;
    s_startupWarned = true;

    if (!s_failedHooks.empty())
    {
        std::string names;
        for (size_t i = 0; i < s_failedHooks.size(); ++i)
        {
            if (i) names += ", ";
            names += s_failedHooks[i];
        }
        LogError("[StackSort] Warning the player: hooks not installed (" + names + ")");
        ou->showPlayerAMessage(Tr(TR_WARN_HOOKS_FAILED), false);
    }

    if (s_configWarnings > 0)
        ou->showPlayerAMessage(FormatToken(Tr(TR_WARN_CONFIG), "{n}", IntToStr(s_configWarnings)), false);
}

void StackSort_PlayClickSound()
{
    if (s_akPostEvent) s_akPostEvent("General_Click", 0x6E, 0, NULL, NULL, 0, NULL, 0);
}

// Main thread, inside a catch(...). Never throws. Latched per hook so a
// persistent failure logs once instead of every frame.
static void ReportHookException(const char* hook, bool& latched)
{
    if (latched) return;
    latched = true;
    try
    {
        LogError(std::string("[StackSort] Exception in ") + hook + " hook (further exceptions in it not logged)");
    }
    catch (...)
    {
    }
}

typedef void (*autoArrangeButton_t)(InventoryGUI*, MyGUI::Widget*);
static autoArrangeButton_t orig_autoArrangeButton = NULL;
static bool s_autoArrangeExceptionLogged          = false;

// Exception barrier: ineligible panels use the original native handler. Once
// our operation begins, an exception stops this click; a second arrange must
// never run over an ambiguous partial native mutation.
static void hook_autoArrangeButton(InventoryGUI* thisptr, MyGUI::Widget* sender)
{
    if (InterlockedCompareExchange(&s_worldWorkDepth, 0, 0) != 0) return;
    if (!s_subsystemsReady || !s_lifecycleReady || !StackSortMcm::Enabled())
    {
        if (orig_autoArrangeButton) orig_autoArrangeButton(thisptr, sender);
        return;
    }
    bool handled = false;

    try
    {
        // Fallback path: if _NV_update never fired (its own hook is one of the ones
        // that can fail), the player still gets told on their first sort click.
        WarnStartupIssuesOnce();

        // Learning here too keeps the click path working when the _NV_update
        // hook (the usual learner) failed to install.
        if (thisptr->ownerInventory == NULL) LearnBaseVtable(thisptr);

        // Trader: fall through to vanilla sort.
        // Only check for non-child GUIs (child backpack GUIs share the base
        // InventoryGUI vtable but we don't want to risk misclassification).
        if (!(thisptr->ownerInventory == NULL && isTraderGUI(thisptr)))
        {
            Inventory* inv = thisptr->_NV_getInventory();

            // OnForwardClick returns false if it can't handle the click
            // (no UIState, no eligible sections) — fall through to vanilla
            if (inv) handled = StackSortUI::OnForwardClick(thisptr, sender);
        }
    }
    catch (...)
    {
        handled = true; // a partial native operation must never trigger another sort
        ReportHookException("autoArrangeButton", s_autoArrangeExceptionLogged);
    }

    if (!handled && orig_autoArrangeButton) orig_autoArrangeButton(thisptr, sender);
}

typedef void (*show_t)(InventoryGUI*, bool);
static show_t orig_show           = NULL;
static bool s_showExceptionLogged = false;

// Exception barrier: orig_show is outside the tries and always runs once.
static void hook_show(InventoryGUI* thisptr, bool on)
{
    if (!on)
    {
        try
        {
            if (SortWorker::HasContext(thisptr))
            {
                LogVerbose("[StackSort] Inventory closed");
                SortWorker::OnInventoryClosed(thisptr);
            }
        }
        catch (...)
        {
            ReportHookException("show", s_showExceptionLogged);
        }

        // Unconditional: backpack GUIs, and GUIs whose UI came from a click
        // while tracking was unavailable, hold UI slots with no worker
        // context. Runs before orig_show, while thisptr and its layout are
        // provably alive, so our buttons can be destroyed and the vanilla
        // ArrangeButton re-shown. Also covers thisptr's backpack slot. Its own
        // try: a worker-teardown failure above must not leave buttons behind.
        try
        {
            StackSortUI::OnClose(thisptr);
        }
        catch (...)
        {
            ReportHookException("show", s_showExceptionLogged);
        }
    }

    orig_show(thisptr, on);
}

typedef void (*dtor_t)(InventoryGUI*);
static dtor_t orig_dtor      = NULL;
static bool s_dtorSeenLogged = false;

// ~InventoryGUI, the non-deleting destructor (subclass destructors chain into
// it). Runs before the original, so every slot keyed by thisptr is freed
// before the address can be reused. Neither callee dereferences thisptr; no
// GUI or MyGUI call is made here because the object may be mid-teardown (a
// subclass destructor has already run) and at game exit MyGUI may be gone.
// Our buttons are children of the GUI's layout and go with it.
// Main thread only (GUI objects are created and destroyed by the game's GUI
// code on the main thread).
static void hook_dtor(InventoryGUI* thisptr)
{
    try
    {
        if (!s_dtorSeenLogged)
        {
            s_dtorSeenLogged = true;
            LogVerbose("[StackSort] ~InventoryGUI hook active");
        }
        StackSortUI::OnDestroyed(thisptr);
        if (SortWorker::HasContext(thisptr)) SortWorker::OnInventoryClosed(thisptr);
    }
    catch (...)
    {
    }

    orig_dtor(thisptr);
}

typedef void (*update_t)(InventoryGUI*);
static update_t orig_update         = NULL;
static bool s_updateExceptionLogged = false;

static void UpdateStackSort(InventoryGUI* thisptr)
{
    if (!s_subsystemsReady || !s_lifecycleReady || InterlockedCompareExchange(&s_worldWorkDepth, 0, 0) != 0) return;
    // Skip child GUIs (same pattern as KenshiRotate)
    if (thisptr->ownerInventory != NULL) return;

    // Skip non-visible GUIs. During rapid inventory cycling, _NV_update can
    // fire for a GUI on the same frame as show(false), or on the frame after
    // before the game stops calling update. Gating on visible prevents
    // creating stale contexts or initializing UI on closing GUIs.
    if (!thisptr->visible) return;

    if (!StackSortMcm::Enabled())
    {
        StackSortUI::OnClose(thisptr);
        if (SortWorker::HasContext(thisptr)) SortWorker::OnInventoryClosed(thisptr);
        return;
    }
    WarnStartupIssuesOnce();

#ifdef STACKSORT_VERBOSE
    // Runs BEFORE the trader skip below so trader stock contributes to the
    // catalog. Dedup makes per-frame cost negligible.
    CatalogDump::Scan(thisptr);
#endif

    LearnBaseVtable(thisptr);

    // Skip trader GUIs (different vtable than base InventoryGUI), and every
    // GUI until the base vtable is known
    if (isTraderGUI(thisptr)) return;

    // Detect stale context: Kenshi reuses InventoryGUI objects when switching
    // characters without calling show(false)/show(true). The underlying
    // inventory changes but our context still references the old one.
    if (SortWorker::HasContext(thisptr) && !SortWorker::ContextMatchesInventory(thisptr))
    {
        LogInfo("[StackSort] Stale context detected, resetting");
        SortWorker::OnInventoryClosed(thisptr);
        // The GUI is visible and its layout is kept, so OnClose must remove
        // our buttons and re-show the vanilla one: OnUpdate below re-inits
        // only if the new inventory has something to sort.
        StackSortUI::OnClose(thisptr);
    }

    // Create context for this GUI if it doesn't have one yet
    if (!SortWorker::HasContext(thisptr)) SortWorker::OnInventoryOpened(thisptr);

    // Liveness stamp + equipped-backpack change detection. No-op when the
    // GUI has no context (tracking table full). Must run before
    // PollWorkerStart so this GUI counts as live in the same poll.
    SortWorker::OnTrackedGUIUpdate(thisptr);

    // Check if a debounced worker spawn is due
    SortWorker::PollWorkerStart();

    // Attempt early UI initialization (caption + back button)
    StackSortUI::OnUpdate(thisptr);
}

// Exception barrier: the original runs first and outside the try, so a failure
// in our logic never skips or repeats the game's own update.
static void hook_update(InventoryGUI* thisptr)
{
    // Let the game process needItemsUpdate first — this populates sections
    // with items on the frame the inventory opens. Must run before we snapshot.
    orig_update(thisptr);

    try
    {
        UpdateStackSort(thisptr);
    }
    catch (...)
    {
        ReportHookException("update", s_updateExceptionLogged);
    }
}

typedef void (*sectionAddCb_t)(Inventory*, Item*);
static sectionAddCb_t orig_sectionAddCb = NULL;

// Both item callbacks: OnInventoryMutated is an exception barrier, so nothing
// thrown on our side unwinds into the game's callback frame (orig has run).
static void hook_sectionAddItemCallback(Inventory* thisptr, Item* item)
{
    orig_sectionAddCb(thisptr, item);
    if (!SortWorker::IsApplying()) SortWorker::OnInventoryMutated(thisptr);
}

typedef void (*sectionRemoveCb_t)(Inventory*, Item*);
static sectionRemoveCb_t orig_sectionRemoveCb = NULL;

static void hook_sectionRemoveItemCallback(Inventory* thisptr, Item* item)
{
    orig_sectionRemoveCb(thisptr, item);
    if (!SortWorker::IsApplying()) SortWorker::OnInventoryMutated(thisptr);
}

// PGO builds: /LTCG:PGO can generate local import thunks for &Class::Method
// even with /GL, causing GetRealAddress to assert the address is outside
// KenshiLib.dll's FUNC_BEGIN..FUNC_END range. Resolve via GetProcAddress
// to always get the KenshiLib.dll export address.
#ifdef STACKSORT_PGO_DLL
static intptr_t GetHookTargetByName(const char* mangledName)
{
    static HMODULE klib = GetModuleHandle(L"KenshiLib.dll");
    if (!klib) return 0;
    void* addr = (void*)GetProcAddress(klib, mangledName);
    if (!addr) return 0;
    return KenshiLib::GetRealAddress(addr);
}
#define HOOK_TARGET(mangled, cls, method) GetHookTargetByName(mangled)
#else
#define HOOK_TARGET(mangled, cls, method) KenshiLib::GetRealAddress(&cls::method)
#endif

__declspec(dllexport) void startPlugin()
{
    // Settings first so the log level is in force for every later line, and
    // because SortWorker::Init reads them. Written before any thread exists.
    SettingsReport settings = StackSortSettingsStore::Load(ModPath::Dir() + "StackSort.cfg");
    // MCM startup sets the atomic runtime log level (errors always).

    LogInfo(std::string("[StackSort] Starting plugin v") + STACKSORT_VERSION + "...");

    if (!settings.fileFound)
    {
        LogInfo("[StackSort] No StackSort.cfg, using defaults");
    }
    else
    {
        LogInfo("[StackSort] Loaded StackSort.cfg");
        for (size_t i = 0; i < settings.overrides.size(); ++i)
            LogInfo("[StackSort] cfg: " + settings.overrides[i]);
    }
    s_configWarnings = (int)settings.warnings.size();
    for (size_t i = 0; i < settings.warnings.size(); ++i)
        LogError("[StackSort] cfg: " + settings.warnings[i]);

    // Shared Localization.h loads ru_RU/en_GB/zh_CN on first UI access.

    // Hook 1: autoArrangeButton — replace vanilla sort
    intptr_t addr1 =
        HOOK_TARGET("?autoArrangeButton@InventoryGUI@@QEAAXPEAVWidget@MyGUI@@@Z", InventoryGUI, autoArrangeButton);
    if (!addr1 || KenshiLib::SUCCESS !=
                      KenshiLib::AddHook((void*)addr1, (void*)hook_autoArrangeButton, (void**)&orig_autoArrangeButton))
    {
        LogError("[StackSort] FATAL: Failed to hook autoArrangeButton");
        return;
    }
    LogInfo("[StackSort] Hooked autoArrangeButton OK");

    static const int TOTAL_HOOKS = 5;
    int installedHooks           = 1;
    bool mutationHooksOk         = true;

    // Hook 2: show — inventory close detection
    intptr_t addr2 = HOOK_TARGET("?_NV_show@InventoryGUI@@QEAAX_N@Z", InventoryGUI, _NV_show);
    if (!addr2 || KenshiLib::SUCCESS != KenshiLib::AddHook((void*)addr2, (void*)hook_show, (void**)&orig_show))
    {
        LogError("[StackSort] WARNING: Failed to hook show (non-fatal)");
        s_failedHooks.push_back("show");
    }
    else
    {
        LogInfo("[StackSort] Hooked show OK");
        ++installedHooks;
    }

    // Hook 3: _NV_update — open detection + worker poll
    intptr_t addr3 = HOOK_TARGET("?_NV_update@InventoryGUI@@QEAAXXZ", InventoryGUI, _NV_update);
    if (!addr3 || KenshiLib::SUCCESS != KenshiLib::AddHook((void*)addr3, (void*)hook_update, (void**)&orig_update))
    {
        LogError("[StackSort] WARNING: Failed to hook _NV_update (non-fatal)");
        s_failedHooks.push_back("_NV_update");
    }
    else
    {
        LogInfo("[StackSort] Hooked _NV_update OK");
        ++installedHooks;
    }

    // Hook 4: _sectionAddItemCallback — mutation detection
    intptr_t addr4 = HOOK_TARGET("?_NV__sectionAddItemCallback@Inventory@@QEAAXPEAVItem@@@Z", Inventory,
                                 _NV__sectionAddItemCallback);
    if (!addr4 || KenshiLib::SUCCESS !=
                      KenshiLib::AddHook((void*)addr4, (void*)hook_sectionAddItemCallback, (void**)&orig_sectionAddCb))
    {
        LogError("[StackSort] WARNING: Failed to hook sectionAddItemCallback (non-fatal)");
        s_failedHooks.push_back("sectionAddItemCallback");
        mutationHooksOk = false;
    }
    else
    {
        LogInfo("[StackSort] Hooked sectionAddItemCallback OK");
        ++installedHooks;
    }

    // Hook 5: _sectionRemoveItemCallback — mutation detection
    intptr_t addr5 = HOOK_TARGET("?_NV__sectionRemoveItemCallback@Inventory@@QEAAXPEAVItem@@@Z", Inventory,
                                 _NV__sectionRemoveItemCallback);
    if (!addr5 || KenshiLib::SUCCESS != KenshiLib::AddHook((void*)addr5, (void*)hook_sectionRemoveItemCallback,
                                                           (void**)&orig_sectionRemoveCb))
    {
        LogError("[StackSort] WARNING: Failed to hook sectionRemoveItemCallback (non-fatal)");
        s_failedHooks.push_back("sectionRemoveItemCallback");
        mutationHooksOk = false;
    }
    else
    {
        LogInfo("[StackSort] Hooked sectionRemoveItemCallback OK");
        ++installedHooks;
    }

    // ~InventoryGUI: lifetime backstop, not counted in TOTAL_HOOKS and not
    // reported to the player. Without it a GUI destroyed without show(false)
    // is still caught by the layout check and the timeout reclaim.
    intptr_t addrDtor = HOOK_TARGET("?_DESTRUCTOR@InventoryGUI@@QEAAXXZ", InventoryGUI, _DESTRUCTOR);
    if (!addrDtor || KenshiLib::SUCCESS != KenshiLib::AddHook((void*)addrDtor, (void*)hook_dtor, (void**)&orig_dtor))
        LogError("[StackSort] WARNING: Failed to hook ~InventoryGUI (non-fatal, stale slots reclaimed by timeout)");
    else LogInfo("[StackSort] Hooked ~InventoryGUI OK");

    // Not hooked, only resolved: the base implementation's address is evidence
    // for LearnBaseVtable. A failed resolve leaves the body-GUI evidence only.
    s_baseGetInventory =
        (void*)HOOK_TARGET("?_NV_getInventory@InventoryGUI@@QEAAPEAVInventory@@XZ", InventoryGUI, _NV_getInventory);

    // Hooks 4 and 5 are the only source of cache invalidation: without both,
    // a precomputed result could describe items that have since moved.
    SortWorker::SetMutationTrackingAvailable(mutationHooksOk);
    if (!mutationHooksOk)
        LogError("[StackSort] Mutation hooks missing: precomputed sorts disabled, every sort runs synchronously");

    if (s_failedHooks.empty())
    {
        LogInfo("[StackSort] Plugin loaded successfully (" + IntToStr(installedHooks) + "/" + IntToStr(TOTAL_HOOKS) +
                " hooks)");
    }
    else
    {
        std::string names;
        for (size_t i = 0; i < s_failedHooks.size(); ++i)
        {
            if (i) names += ", ";
            names += s_failedHooks[i];
        }
        LogError("[StackSort] Plugin loaded with " + IntToStr(installedHooks) + "/" + IntToStr(TOTAL_HOOKS) +
                 " hooks (failed: " + names + ")");
    }

    // Initialize subsystems
    const bool resetOk = KenshiLib::SUCCESS == KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&GameWorld::resetGame), HookWorldReset, &s_origReset);
    const bool loadOk = KenshiLib::SUCCESS == KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&SaveFileSystem::loadGame), HookWorldLoad, &s_origLoad);
    const bool newOk = KenshiLib::SUCCESS == KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&SaveFileSystem::newGame), HookWorldNew, &s_origNew);
    s_lifecycleReady = resetOk && loadOk && newOk;
    if (!s_lifecycleReady)
    {
        LogError("StackSort: world lifecycle hooks failed; custom sorting disabled");
        return;
    }
    ResultCache::Init();
    SortWorker::Init();
    s_subsystemsReady = true;
#ifdef STACKSORT_VERBOSE
    CatalogDump::Init();
#endif
    // No-op unless STACKSORT_ENABLE_ROTATION is defined at compile time.
    StackSort_InitRotateAPI();

    // Resolve AK::SoundEngine::PostEvent from kenshi_x64.exe
    {
        HMODULE hGame = GetModuleHandleA(NULL);
        if (hGame)
        {
            s_akPostEvent =
                (AK_PostEvent_t)GetProcAddress(hGame, "?PostEvent@SoundEngine@AK@@YAKPEBD_KKP6AXW4AkCallbackType@@"
                                                      "PEAUAkCallbackInfo@@@ZPEAXKPEAUAkExternalSourceInfo@@K@Z");
            if (s_akPostEvent)
            {
                LogInfo("[StackSort] AK::SoundEngine::PostEvent resolved");
            }
            else
            {
                LogError("[StackSort] WARNING: AK::SoundEngine::PostEvent not found (no click sound)");
            }
        }
    }
}
