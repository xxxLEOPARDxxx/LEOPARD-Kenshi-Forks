#pragma warning(push)
#pragma warning(disable : 4091)
#include <kenshi/Inventory.h>
#include <kenshi/RootObject.h>
#include <kenshi/gui/InventoryGUI.h>
#pragma warning(pop)

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_MouseButton.h>

#include <vector>
#include <sstream>

#include "StackSortUI.h"
#include "Packer.h"
#include "ResultCache.h"
#include "SortWorker.h"
#include "StackSort.h"
#include "TargetMemory.h"
#include "StackSortConfig.h"
#include "StackSortSettings.h"
#include "SectionEligibility.h"
#include "McmSettings.h"
#include "Translate.h"
#include "Log.h"

class InventoryGUIAccess : public InventoryGUI
{
  public:
    using InventoryGUI::refreshAllSections;
};

void StackSortUI::ForceRefreshAllSections(InventoryGUI* gui)
{
    ((InventoryGUIAccess*)gui)->refreshAllSections();
}

static std::string MakeCaption(int target)
{
    // Caption is dim-agnostic in the number; the dim toggle button shows H/W.
    return FormatToken(Tr(TR_SORT_BUTTON), "{n}", IntToStr(target));
}

struct UIState
{
    InventoryGUI* gui;            // GUI whose ArrangeButton we hid
    InventoryGUI* ownerGUI;       // parent body GUI (== gui for body/container)
    MyGUI::Button* backButton;    // [<]
    MyGUI::Button* sortButton;    // [StackSort [N]] — score cycling + right-click revert
    MyGUI::Button* dimButton;     // [H|W]
    MyGUI::Button* forwardButton; // [>]
    int currentTarget;            // 0 = unsorted, else current target value
    bool initialized;
    bool stacksConsolidated;
    int topTargets[TOP_TARGETS_MAX];
    int topTargetCount; // 0..the top_targets setting
    int topTargetIdx;   // -1 = not active, else 0..topTargetCount-1

    TargetKey memKey;       // session memory key; valid only while hasMemKey
    bool hasMemKey;         // false when the GUI has no callbackObject
    std::string ownerLabel; // callbackObject->displayName, for the restore log
    const char* slotName;   // "body" / "backpack" / "container", for the restore log

    // Copied into every button's ButtonTag; a fresh value per allocation.
    unsigned int generation;
    // gui->layoutMgr at init. Our buttons are children of that layout, so a
    // different layoutMgr under the same GUI pointer means they died with the
    // old one (or the address now belongs to a different GUI).
    InventoryLayout* layout;
    // Whether teardown re-shows the vanilla ArrangeButton: it was visible at
    // init, or hidden only by an earlier slot whose buttons InitializeUI
    // found and replaced.
    bool restoreVanilla;

    // Liveness stamps from this GUI's (for a backpack, its owner's) OnUpdate.
    // Only stamped slots are reclaimed: a slot created from a click on a GUI
    // that never reaches OnUpdate is left to OnClose / OnDestroyed.
    bool updateStamped;
    DWORD lastSeenTick;
    DWORD lastSeenPoll;

    // Work on another GUI's slot is deferred to that GUI's own OnUpdate: only
    // there, or in a click on its own buttons, is it provably alive.
    bool recallPending; // dim toggled: re-key and recall the target
    bool captionDirty;
};

// Stored in each button's userData. The generation pins a button to the slot
// allocation that created it: a slot freed and handed to another GUI gets a
// new generation, so a surviving old button resolves to nothing.
struct ButtonTag
{
    int idx;
    unsigned int generation;
};

static const char* const BUTTON_NAMES[4] = {"StackSortBack", "StackSortSort", "StackSortDim", "StackSortForward"};

// The owning object's handle plus slot and dim. A GUI with no callbackObject
// gets no memory at all rather than colliding with every other such GUI.
// Dereferences ownerGUI: InitializeUI only, where it is provably alive.
static bool MakeKey(InventoryGUI* gui, InventoryGUI* ownerGUI, TargetKey& outKey, std::string& outLabel)
{
    if (!ownerGUI || !ownerGUI->callbackObject) return false;

    const hand& h          = ownerGUI->callbackObject->handle;
    outKey.index           = h.index;
    outKey.serial          = h.serial;
    outKey.container       = h.container;
    outKey.containerSerial = h.containerSerial;
    outKey.type            = (int)h.type;
    outKey.slot            = (gui == ownerGUI) ? 0 : 1;
    outKey.dim             = (int)SortWorker::GetDim();
    outLabel               = ownerGUI->callbackObject->displayName;
    return true;
}

static UIState s_uiStates[MAX_GUIS];

static unsigned int s_nextGeneration = 1;
static void ResetSlot(UIState& ui);

void StackSortUI::OnWorldReset()
{
    for (int i = 0; i < MAX_GUIS; ++i) ResetSlot(s_uiStates[i]);
    TargetMemory::Clear();
}

// Latched after the first "UI slots full" error, cleared when a slot frees,
// so a full table logs once per episode instead of every frame.
static bool s_uiFullLogged = false;

// Advanced once per StackSortUI::OnUpdate call, i.e. once per update of each
// visible top-level non-trader GUI. Stands still while nothing updates, so
// "polls missed" counts updates a slot sat out while other GUIs demonstrably
// kept updating. Unsigned subtraction survives the wrap. Main thread only.
static DWORD s_uiPollCount = 0;

// Same bounds as the worker's tracking reclaim (SortWorker.cpp), and both
// must be exceeded: wall time alone would free a live GUI's slot on the first
// update after a long stall, polls alone a GUI that died while nothing polled.
static const LONG UI_RECLAIM_MS     = 30000;
static const DWORD UI_RECLAIM_POLLS = 600;

// Latched after the first exception caught at a UI entry point, so a
// persistent failure logs once instead of every frame.
static bool s_uiExceptionLogged = false;

// Main thread, inside a catch(...). Never throws.
// Every UI entry point the game or MyGUI calls (OnUpdate, OnClose, the click
// delegates) catches here so nothing unwinds into game or MyGUI frames. A
// click that throws mid-apply runs no vanilla sort afterwards: items may
// already have moved, and re-sorting over a partial apply is worse than
// leaving it.
static void ReportUIException(const char* where)
{
    if (s_uiExceptionLogged) return;
    s_uiExceptionLogged = true;
    try
    {
        LogError(std::string("[StackSort] Exception in UI ") + where + " (further UI exceptions not logged)");
    }
    catch (...)
    {
    }
}

// Forgets the slot without touching its GUI or widgets. Never throws.
static void ResetSlot(UIState& ui)
{
    ui.gui                = NULL;
    ui.ownerGUI           = NULL;
    ui.backButton         = NULL;
    ui.sortButton         = NULL;
    ui.dimButton          = NULL;
    ui.forwardButton      = NULL;
    ui.currentTarget      = 0;
    ui.initialized        = false;
    ui.stacksConsolidated = false;
    ui.topTargetCount     = 0;
    ui.topTargetIdx       = -1;
    ui.hasMemKey          = false;
    ui.memKey             = TargetKey();
    ui.ownerLabel.clear();
    ui.slotName       = "container";
    ui.generation     = 0;
    ui.layout         = NULL;
    ui.restoreVanilla = false;
    ui.updateStamped  = false;
    ui.lastSeenTick   = 0;
    ui.lastSeenPoll   = 0;
    ui.recallPending  = false;
    ui.captionDirty   = false;

    s_uiFullLogged = false;
}

static bool IsOurButtonName(const std::string& name)
{
    for (int i = 0; i < 4; ++i)
    {
        if (name == BUTTON_NAMES[i]) return true;
    }
    return false;
}

static bool IsSlotButton(MyGUI::Widget* w)
{
    for (int i = 0; i < MAX_GUIS; ++i)
    {
        const UIState& ui = s_uiStates[i];
        if (!ui.initialized) continue;
        if ((MyGUI::Widget*)ui.backButton == w || (MyGUI::Widget*)ui.sortButton == w ||
            (MyGUI::Widget*)ui.dimButton == w || (MyGUI::Widget*)ui.forwardButton == w)
            return true;
    }
    return false;
}

// Destroys every StackSort button among parent's direct children that no
// initialized slot owns: the closing slot's own (reset by the caller first)
// and stacked leftovers. Buttons a live slot still owns are spared, so a
// parent shared by two GUIs' groups never loses the other group.
// parent must be provably alive. Never call from a click delegate of one of
// these buttons: MyGUI must not destroy the sender mid-dispatch.
// Returns true if any were destroyed.
static bool RemoveOurButtons(MyGUI::Widget* parent)
{
    std::vector<MyGUI::Widget*> doomed;
    size_t n = parent->getChildCount();
    for (size_t i = 0; i < n; ++i)
    {
        MyGUI::Widget* w = parent->getChildAt(i);
        if (w && IsOurButtonName(w->getName()) && !IsSlotButton(w)) doomed.push_back(w);
    }
    if (doomed.empty()) return false;

    MyGUI::Gui* mygui = MyGUI::Gui::getInstancePtr();
    if (!mygui) return false;
    for (size_t d = 0; d < doomed.size(); ++d)
        mygui->destroyWidget(doomed[d]);
    return true;
}

// Removes our buttons from liveGui's layout and re-shows the vanilla
// ArrangeButton. liveGui must be provably alive (its own show/update, or the
// childInventory of one that is), and its layoutMgr the one the buttons were
// created on: the caller compares it against UIState::layout first.
static void TeardownWidgets(InventoryGUI* liveGui, bool restoreVanilla)
{
    if (!liveGui || !liveGui->layoutMgr) return;
    MyGUI::Widget* arrangeBtn = liveGui->layoutMgr->getWidget("ArrangeButton");
    if (!arrangeBtn) return;
    MyGUI::Widget* parent = arrangeBtn->getParent();
    if (parent) RemoveOurButtons(parent);
    if (restoreVanilla) arrangeBtn->setVisible(true);
}

// The slot a button belongs to, or -1 when the button outlived its slot
// (closed, reclaimed, or reallocated to another GUI since).
static int ResolveSender(MyGUI::Widget* sender)
{
    if (!sender) return -1;
    ButtonTag* tag = sender->getUserData<ButtonTag>(false);
    if (!tag || tag->idx < 0 || tag->idx >= MAX_GUIS) return -1;

    const UIState& ui = s_uiStates[tag->idx];
    if (!ui.initialized || !ui.gui || ui.generation != tag->generation) return -1;
    return tag->idx;
}

// Never dereferences a slot's gui or widgets: a slot stops being stamped
// exactly when its GUI may have been destroyed without show(false).
static void ReclaimStaleUIStates(DWORD now)
{
    for (int i = 0; i < MAX_GUIS; ++i)
    {
        UIState& ui = s_uiStates[i];
        if (!ui.initialized || !ui.updateStamped) continue;
        if ((LONG)(now - ui.lastSeenTick) <= UI_RECLAIM_MS) continue;
        if ((DWORD)(s_uiPollCount - ui.lastSeenPoll) <= UI_RECLAIM_POLLS) continue;

        ResetSlot(ui);
        LogInfo("[StackSort] Reclaimed UI slot " + IntToStr(i) + " (GUI stopped updating)");
    }
}

int StackSortUI::FindUIState(InventoryGUI* gui)
{
    for (int i = 0; i < MAX_GUIS; ++i)
    {
        if (s_uiStates[i].initialized && s_uiStates[i].gui == gui) return i;
    }
    return -1;
}

int StackSortUI::AllocUIState(InventoryGUI* gui)
{
    for (int i = 0; i < MAX_GUIS; ++i)
    {
        if (!s_uiStates[i].initialized)
        {
            UIState& ui = s_uiStates[i];
            ResetSlot(ui); // initialized stays false until the caller's full init
            ui.gui        = gui;
            ui.generation = s_nextGeneration++;
            if (s_nextGeneration == 0) s_nextGeneration = 1;
            return i;
        }
    }
    if (!s_uiFullLogged)
    {
        s_uiFullLogged = true;
        LogError("[StackSort] All UI slots full (max " + IntToStr(MAX_GUIS) +
                 "); further inventories keep the vanilla button");
    }
    return -1;
}

void StackSortUI::PlayClickSound()
{
    StackSort_PlayClickSound();
}

// Touches this slot's own buttons: only where they are provably alive (the
// GUI's own update, or a click on one of them).
void StackSortUI::UpdateCaption(int uiIdx)
{
    UIState& ui = s_uiStates[uiIdx];
    if (ui.sortButton) ui.sortButton->setCaption(MakeCaption(ui.currentTarget));
    if (ui.dimButton) ui.dimButton->setCaption(Tr((SortWorker::GetDim() == Packer::TARGET_H) ? TR_DIM_H : TR_DIM_W));
    ui.captionDirty = false;
}

void StackSortUI::CommitTarget(int uiIdx)
{
    UpdateCaption(uiIdx);

    UIState& ui = s_uiStates[uiIdx];

    // A dim toggle only re-keys the clicked slot; other slots re-key in MaintainSlots, which
    // never runs without the _NV_update hook. Committing a fresh target supersedes the pending
    // recall, so re-key here to avoid storing it under the previous dim.
    if (ui.recallPending)
    {
        ui.recallPending = false;
        if (ui.hasMemKey) ui.memKey.dim = (int)SortWorker::GetDim();
    }

    if (ui.hasMemKey) TargetMemory::Remember(ui.memKey, ui.currentTarget);
}

void StackSortUI::ReKeyAndRecall(int uiIdx, bool logRestore)
{
    UIState& ui      = s_uiStates[uiIdx];
    ui.recallPending = false;
    if (!ui.hasMemKey) return;

    // The owner part of the key was cached at init; only the dim changes.
    ui.memKey.dim = (int)SortWorker::GetDim();

    int remembered = 0;
    if (!TargetMemory::Recall(ui.memKey, remembered) || remembered <= 0) return;

    int maxTarget = FindMaxTarget(ui.gui);
    if (remembered > maxTarget) remembered = maxTarget;
    if (remembered <= 0) return;

    ui.currentTarget = remembered;
    if (logRestore)
        LogInfo("[StackSort] Restored target=" + IntToStr(remembered) + " for '" + ui.ownerLabel + "' (" + ui.slotName +
                ")");
}

void StackSortUI::RefreshAllCaptions()
{
    for (int i = 0; i < MAX_GUIS; ++i)
        if (s_uiStates[i].initialized) s_uiStates[i].captionDirty = true;
}

bool StackSortUI::HasEligibleSections(Inventory* inv)
{
    if (!inv) return false;

    lektor<InventorySection*>& sections = inv->getAllSections();
    for (lektor<InventorySection*>::iterator it = sections.begin(); it != sections.end(); ++it)
    {
        InventorySection* section = *it;
        if (!StackSort_EligibleSection(section)) continue;
        if (!section->enabled) continue;
        if (section->getItems().empty()) continue;
        return true;
    }
    return false;
}

int StackSortUI::FindMaxTarget(InventoryGUI* gui)
{
    Inventory* inv = gui->_NV_getInventory();
    if (!inv) return 0;

    Packer::TargetDim dim               = SortWorker::GetDim();
    int maxTarget                       = 0;
    lektor<InventorySection*>& sections = inv->getAllSections();
    for (lektor<InventorySection*>::iterator it = sections.begin(); it != sections.end(); ++it)
    {
        InventorySection* section = *it;
        if (!StackSort_EligibleSection(section)) continue;
        if (!section->enabled) continue;
        if (section->getItems().empty()) continue;

        int sMaxTarget = SortWorker::GetMaxTarget(section);
        if (sMaxTarget == 0) sMaxTarget = ((dim == Packer::TARGET_H) ? section->height : section->width) - 1;
        if (sMaxTarget > maxTarget) maxTarget = sMaxTarget;
    }
    return maxTarget;
}

// Per-inventory scope: this GUI's own inventory sections only.
int StackSortUI::PeekAppliedLerSide(InventoryGUI* gui, int target)
{
    Inventory* inv = gui->_NV_getInventory();
    if (!inv) return 0;

    Packer::TargetDim dim = SortWorker::GetDim();
    int maxLerSide        = 0;

    lektor<InventorySection*>& sections = inv->getAllSections();
    for (lektor<InventorySection*>::iterator it = sections.begin(); it != sections.end(); ++it)
    {
        InventorySection* section = *it;

        if (!StackSort_EligibleSection(section)) continue;
        if (!section->enabled) continue;
        if (section->getItems().empty()) continue;

        int gridSide   = (dim == Packer::TARGET_H) ? section->height : section->width;
        int sMaxTarget = gridSide - 1;

        int clamped = target;
        if (clamped > sMaxTarget) clamped = sMaxTarget;
        if (clamped < 1) clamped = 1;

        int sourceTarget = 0;
        Packer::Result r;
        if (SortWorker::GetBestResult(section, clamped, r, &sourceTarget) && r.allPlaced)
        {
            int lerSide = (dim == Packer::TARGET_H) ? r.lerHeight : r.lerWidth;
            if (lerSide > maxLerSide) maxLerSide = lerSide;
        }
    }

    return maxLerSide;
}

void StackSortUI::InitializeUI(InventoryGUI* gui, MyGUI::Widget* arrangeBtn, InventoryGUI* ownerGUI)
{
    // Already initialized for this GUI
    if (FindUIState(gui) >= 0) return;

    MyGUI::Widget* parent = arrangeBtn->getParent();
    if (!parent) return;

    // Buttons left on this layout by an earlier slot (closed without
    // show(false), reclaimed, or a stale-context reset that could not reach
    // them) go first, so a layout never carries more than one group. Before
    // AllocUIState, which may hand out the very slot that pointed at them.
    bool hadLeftovers = RemoveOurButtons(parent);

    int idx = AllocUIState(gui);
    if (idx < 0)
    {
        if (hadLeftovers) arrangeBtn->setVisible(true);
        return;
    }

    UIState& ui       = s_uiStates[idx];
    ui.ownerGUI       = ownerGUI;
    ui.layout         = gui->layoutMgr;
    ui.restoreVanilla = hadLeftovers || arrangeBtn->getVisible();

    // Anchor on the vanilla ArrangeButton's own coordinates — Kenshi's layout
    // system places it near the inventory grid, so we mirror that position.
    // Parent dimensions aren't reliable: for character body panels,
    // parent->getCoord() includes sidebar elements and reports a larger width
    // than the inventory grid.
    MyGUI::IntCoord ac = arrangeBtn->getCoord();
    MyGUI::IntCoord pc = parent->getCoord();

    // Body GUIs (character inventory with a child backpack) need a small
    // leftward nudge — the vanilla ArrangeButton is centered under the wider
    // body panel, but we want the new group under the main inventory grid.
    bool isBodyGUI               = (gui == ownerGUI && gui->childInventory != NULL);
    const StackSortSettings& cfg = StackSortSettingsStore::Get();

#ifdef STACKSORT_PACKER_GRID_LOG
    LogDebug(std::string("[StackSort] Layout anchor: arrangeBtn (") + IntToStr(ac.left) + "," + IntToStr(ac.top) + " " +
             IntToStr(ac.width) + "x" + IntToStr(ac.height) + ")" + ", parent (" + IntToStr(pc.left) + "," +
             IntToStr(pc.top) + " " + IntToStr(pc.width) + "x" + IntToStr(pc.height) + ")" +
             (isBodyGUI ? " [body]" : ""));
#endif

    // Hide the vanilla ArrangeButton — the new 4-button group replaces it.
    // OnClose and the stale-context reset re-show it (TeardownWidgets) in case
    // the layout outlives this open.
    arrangeBtn->setVisible(false);

    ButtonTag tag;
    tag.idx        = idx;
    tag.generation = ui.generation;

    // Layout: [<] [Stacksort [N]] [H|W] [>]
    // Horizontally centered on the vanilla button's own center.
    // Vertically aligned with the vanilla button's top.
    const int btnH      = ac.height;
    const int arrowBtnW = cfg.arrowButtonWidth;
    const int dimBtnW   = cfg.dimButtonWidth;
    const int sortBtnW  = cfg.sortButtonWidth;
    const int gap       = cfg.buttonGap;

    const int totalW         = arrowBtnW + gap + sortBtnW + gap + dimBtnW + gap + arrowBtnW;
    const int vanillaCenterX = ac.left + ac.width / 2;
    const int startX         = vanillaCenterX - totalW / 2 + (isBodyGUI ? cfg.bodyXOffset : 0);
    const int startY         = ac.top;

    const int xBack = startX;
    const int xSort = xBack + arrowBtnW + gap;
    const int xDim  = xSort + sortBtnW + gap;
    const int xFwd  = xDim + dimBtnW + gap;

    // Back button
    ui.backButton = parent->createWidget<MyGUI::Button>(
        "Kenshi_Button1", MyGUI::IntCoord(xBack, startY, arrowBtnW, btnH), MyGUI::Align::Default, "StackSortBack");
    ui.backButton->setCaption(Tr(TR_BACK_ARROW));
    ui.backButton->setTextAlign(MyGUI::Align::Center);
    ui.backButton->setUserData(MyGUI::Any(tag));
    ui.backButton->eventMouseButtonClick += MyGUI::newDelegate(OnBackClick);

    // Sort button (middle — score cycling + right-click revert)
    ui.sortButton = parent->createWidget<MyGUI::Button>(
        "Kenshi_Button1", MyGUI::IntCoord(xSort, startY, sortBtnW, btnH), MyGUI::Align::Default, "StackSortSort");
    ui.sortButton->setTextAlign(MyGUI::Align::Center);
    ui.sortButton->setUserData(MyGUI::Any(tag));
    ui.sortButton->eventMouseButtonClick += MyGUI::newDelegate(OnSortCycleClick);
    ui.sortButton->eventMouseButtonPressed += MyGUI::newDelegate(OnSortRightClick);

    // Dim toggle
    ui.dimButton = parent->createWidget<MyGUI::Button>("Kenshi_Button1", MyGUI::IntCoord(xDim, startY, dimBtnW, btnH),
                                                       MyGUI::Align::Default, "StackSortDim");
    ui.dimButton->setTextAlign(MyGUI::Align::Center);
    ui.dimButton->setUserData(MyGUI::Any(tag));
    ui.dimButton->eventMouseButtonClick += MyGUI::newDelegate(OnDimClick);

    // Forward button
    ui.forwardButton = parent->createWidget<MyGUI::Button>(
        "Kenshi_Button1", MyGUI::IntCoord(xFwd, startY, arrowBtnW, btnH), MyGUI::Align::Default, "StackSortForward");
    ui.forwardButton->setCaption(Tr(TR_FORWARD_ARROW));
    ui.forwardButton->setTextAlign(MyGUI::Align::Center);
    ui.forwardButton->setUserData(MyGUI::Any(tag));
    ui.forwardButton->eventMouseButtonClick += MyGUI::newDelegate(OnForwardArrowClick);

    // Key and label are cached now, while ownerGUI is provably alive, so a
    // later dim toggle can re-key every slot without dereferencing any GUI.
    ui.hasMemKey = MakeKey(gui, ownerGUI, ui.memKey, ui.ownerLabel);
    ui.slotName  = (gui != ownerGUI) ? "backpack" : (isBodyGUI ? "body" : "container");

    ui.initialized = true;

    // Caption-only restore: the inventory is physically still in its last
    // applied layout, so nothing is re-packed or re-placed behind the user.
    ReKeyAndRecall(idx, true);

    UpdateCaption(idx);

    LogInfo("[StackSort] UI initialized (target=" + IntToStr(ui.currentTarget) +
            ", dim=" + ((SortWorker::GetDim() == Packer::TARGET_H) ? "H" : "W") + ")");
}

// Pre-sort setup for every sort click. Stack consolidation runs once per open
// (stacksConsolidated); the revert capture is retried on every click because
// the worker drops a section's capture when the player hand-edits it or
// changes the backpack, and CaptureOriginalPositions skips sections that
// still hold one. Per-inventory scope — only touches sections of ui.gui's
// inventory.
void StackSortUI::FirstClickInit(int uiIdx)
{
    UIState& ui = s_uiStates[uiIdx];

    if (!ui.stacksConsolidated)
    {
        Inventory* inv = ui.gui->_NV_getInventory();

        bool anyConsolidated = false;
        if (inv && StackSortMcm::MergeStacks())
        {
            lektor<InventorySection*>& sections = inv->getAllSections();
            for (lektor<InventorySection*>::iterator it = sections.begin(); it != sections.end(); ++it)
            {
                InventorySection* section = *it;
                if (!StackSort_EligibleSection(section)) continue;
                if (!section->enabled) continue;
                if (section->getItems().empty()) continue;
                if (ConsolidateStacks(section)) anyConsolidated = true;
            }
        }

        ui.stacksConsolidated = true;

        if (anyConsolidated)
        {
            LogInfo("[StackSort] Consolidated stacks, triggering re-snapshot");
            SortWorker::OnMutation(ui.ownerGUI);
            ForceRefreshAllSections(ui.gui);
        }
    }

    // Must run before the click's ApplySort: an absent capture here means the
    // layout on screen is the player's, not ours, unless an earlier apply
    // marked the section sorted-without-capture (no entry yet, or the apply
    // dropped the capture itself); CaptureOriginalPositions skips those.
    CaptureOriginalPositions(ui.gui);
}

// Forward-arrow cycling: increment target with forward-skip past covered values.
// Shared between the hook fallback (OnForwardClick) and the new > button.
void StackSortUI::CycleTargetForward(int uiIdx)
{
    UIState& ui = s_uiStates[uiIdx];
    if (!ui.initialized || !ui.gui) return;

    ui.topTargetIdx = -1; // exit cycling mode

    int maxTarget = FindMaxTarget(ui.gui);
    if (maxTarget < 1)
    {
        PlayClickSound();
        return;
    }

    FirstClickInit(uiIdx);

    ui.currentTarget++;
    if (ui.currentTarget > maxTarget) ui.currentTarget = 1;

    int appliedLerSide = 0;
    int sourceTarget   = 0;
    ApplySort(ui.gui, ui.currentTarget, appliedLerSide, sourceTarget);

    if (appliedLerSide > ui.currentTarget && appliedLerSide <= maxTarget) ui.currentTarget = appliedLerSide;

    PlayClickSound();
    CommitTarget(uiIdx);
}

bool StackSortUI::OnForwardClick(InventoryGUI* gui, MyGUI::Widget* sender)
{
    Inventory* inv = gui->_NV_getInventory();

    int idx = FindUIState(gui);

    // A slot left from a previous GUI at this address (no show(false), no
    // destructor hook): its buttons died with that GUI's layout.
    if (idx >= 0 && s_uiStates[idx].layout != gui->layoutMgr)
    {
        ResetSlot(s_uiStates[idx]);
        idx = -1;
    }

    // Initialize UI on first click from a GUI with eligible sections
    if (idx < 0 && inv && HasEligibleSections(inv))
    {
        InventoryGUI* owner = gui->ownerInventory ? gui->ownerInventory : gui;
        InitializeUI(gui, sender, owner);
        idx = FindUIState(gui);
    }

    if (idx < 0) return false; // not handled — caller should fall through to vanilla

    CycleTargetForward(idx);
    return true;
}

void StackSortUI::OnForwardArrowClick(MyGUI::Widget* sender)
{
    // Exception barrier: MyGUI click delegate.
    try
    {
        int idx = ResolveSender(sender);
        if (idx < 0) return;
        CycleTargetForward(idx);
    }
    catch (...)
    {
        ReportUIException("click");
    }
}

// Rank target values 1..maxTarget by aggregate GetBestResult score across
// this GUI's inventory sections. Top distinct scores in descending order,
// capped by the top_targets setting.
// Per-inventory scope: body button → body sections, backpack → backpack.
void StackSortUI::BuildTopTargets(int uiIdx)
{
    UIState& ui       = s_uiStates[uiIdx];
    ui.topTargetCount = 0;

    Inventory* inv = ui.gui->_NV_getInventory();
    if (!inv) return;

    Packer::TargetDim dim = SortWorker::GetDim();

    const int MAX_T = 64;
    long long scoreByTarget[MAX_T];
    int validByTarget[MAX_T];
    for (int i = 0; i < MAX_T; ++i)
    {
        scoreByTarget[i] = 0;
        validByTarget[i] = 0;
    }

    int eligibleSections                = 0;
    lektor<InventorySection*>& sections = inv->getAllSections();
    for (lektor<InventorySection*>::iterator it = sections.begin(); it != sections.end(); ++it)
    {
        InventorySection* section = *it;
        if (!StackSort_EligibleSection(section)) continue;
        if (!section->enabled) continue;
        if (section->getItems().empty()) continue;
        ++eligibleSections;

        int sMax = ((dim == Packer::TARGET_H) ? section->height : section->width) - 1;
        if (sMax >= MAX_T) sMax = MAX_T - 1;
        for (int t = 1; t <= sMax; ++t)
        {
            Packer::Result r;
            if (SortWorker::GetBestResult(section, t, r, NULL) && r.allPlaced)
            {
                scoreByTarget[t] += r.score;
                ++validByTarget[t];
            }
        }
    }

    if (eligibleSections == 0) return;

    // Collect candidate targets where all sections returned a valid result.
    int candidateT[MAX_T];
    int numCandidates = 0;
    for (int t = 1; t < MAX_T; ++t)
    {
        if (validByTarget[t] == eligibleSections) candidateT[numCandidates++] = t;
    }

    // Insertion sort by score descending.
    for (int i = 1; i < numCandidates; ++i)
    {
        int tmp            = candidateT[i];
        long long tmpScore = scoreByTarget[tmp];
        int j              = i - 1;
        while (j >= 0 && scoreByTarget[candidateT[j]] < tmpScore)
        {
            candidateT[j + 1] = candidateT[j];
            --j;
        }
        candidateT[j + 1] = tmp;
    }

    // Take the top distinct scores. Adjacent targets often share the same
    // underlying best result (GetBestResult scan semantics) — dedup by score.
    // Clamped here rather than trusting the setting's declared range: this is
    // the array bound, and the range lives in another file.
    int maxEntries = StackSortMcm::Solutions();
    if (maxEntries > TOP_TARGETS_MAX) maxEntries = TOP_TARGETS_MAX;
    if (maxEntries < 1) maxEntries = 1;
    long long lastScore = -1;
    for (int i = 0; i < numCandidates && ui.topTargetCount < maxEntries; ++i)
    {
        int t       = candidateT[i];
        long long s = scoreByTarget[t];
        if (ui.topTargetCount > 0 && s == lastScore) continue;
        ui.topTargets[ui.topTargetCount++] = t;
        lastScore                          = s;
    }
}

void StackSortUI::OnSortCycleClick(MyGUI::Widget* sender)
{
    // Exception barrier: MyGUI click delegate.
    try
    {
        int idx = ResolveSender(sender);
        if (idx < 0) return;

        UIState& ui = s_uiStates[idx];

        FirstClickInit(idx);

        BuildTopTargets(idx);
        if (ui.topTargetCount == 0)
        {
            // No precomputed results yet — fall back to forward cycling so the
            // user gets a response (ApplySort's sync fallback runs).
            CycleTargetForward(idx);
            return;
        }

        ui.topTargetIdx  = (ui.topTargetIdx + 1) % ui.topTargetCount;
        ui.currentTarget = ui.topTargets[ui.topTargetIdx];

        int appliedLerSide = 0;
        int sourceTarget   = 0;
        ApplySort(ui.gui, ui.currentTarget, appliedLerSide, sourceTarget);

        PlayClickSound();
        CommitTarget(idx);
    }
    catch (...)
    {
        ReportUIException("click");
    }
}

void StackSortUI::OnBackClick(MyGUI::Widget* sender)
{
    // Exception barrier: MyGUI click delegate.
    try
    {
        int idx = ResolveSender(sender);
        if (idx < 0) return;

        UIState& ui = s_uiStates[idx];

        ui.topTargetIdx = -1; // exit cycling mode

        int maxTarget = FindMaxTarget(ui.gui);
        if (maxTarget < 1) return;

        FirstClickInit(idx);

        // Reverse of forward (which counts up): decrement with backward scan.
        // A stop is a target where PeekAppliedLerSide(t) <= t — forward skip
        // would not have jumped past it.
        int oldTarget = ui.currentTarget;
        int newTarget = ui.currentTarget - 1;
        if (newTarget < 1) newTarget = maxTarget;

        if (newTarget < oldTarget)
        {
            while (newTarget > 1)
            {
                int peekLerSide = PeekAppliedLerSide(ui.gui, newTarget);
                if (peekLerSide <= newTarget) break;
                --newTarget;
            }
            if (newTarget == 1)
            {
                int peekLerSide = PeekAppliedLerSide(ui.gui, 1);
                if (peekLerSide > 1) newTarget = maxTarget;
            }
        }

        ui.currentTarget   = newTarget;
        int appliedLerSide = 0;
        int sourceTarget   = 0;
        ApplySort(ui.gui, ui.currentTarget, appliedLerSide, sourceTarget);
        PlayClickSound();
        CommitTarget(idx);
    }
    catch (...)
    {
        ReportUIException("click");
    }
}

void StackSortUI::OnDimClick(MyGUI::Widget* sender)
{
    // Exception barrier: MyGUI click delegate.
    try
    {
        int idx = ResolveSender(sender);
        if (idx < 0) return;

        // Toggle global dim. Aborts all jobs and re-enqueues under the new dim.
        SortWorker::ToggleDim();

        // Reset cycling state on all initialized UIs, then re-key: the memory key
        // embeds the dim, so each UI comes back with the target it last used under
        // the new dim rather than at 0. The recall clamps against the GUI's
        // inventory, so every other slot does it from its own next OnUpdate; only
        // the clicked GUI is provably alive here.
        // Do NOT reset stacksConsolidated — consolidation is per-inventory-open.
        for (int i = 0; i < MAX_GUIS; ++i)
        {
            UIState& ui = s_uiStates[i];
            if (!ui.initialized) continue;

            ui.currentTarget  = 0;
            ui.topTargetIdx   = -1;
            ui.topTargetCount = 0;
            ui.recallPending  = true;
            ui.captionDirty   = true;
        }

        ReKeyAndRecall(idx, false);
        UpdateCaption(idx);
        PlayClickSound();
    }
    catch (...)
    {
        ReportUIException("click");
    }
}

void StackSortUI::OnSortRightClick(MyGUI::Widget* sender, int, int, MyGUI::MouseButton id)
{
    // Exception barrier: MyGUI click delegate.
    try
    {
        if (id != MyGUI::MouseButton(MyGUI::MouseButton::Right)) return;

        int idx = ResolveSender(sender);
        if (idx < 0) return;

        UIState& ui = s_uiStates[idx];

        RevertInventory(ui.gui);

        ui.currentTarget      = 0;
        ui.stacksConsolidated = false;
        ui.topTargetIdx       = -1;
        ui.topTargetCount     = 0;
        CommitTarget(idx);
        PlayClickSound();
    }
    catch (...)
    {
        ReportUIException("click");
    }
}

// Per-slot upkeep for the slots this GUI vouches for: its own, and its
// backpack's while gui->childInventory still points at that backpack GUI.
void StackSortUI::MaintainSlots(InventoryGUI* gui)
{
    ++s_uiPollCount;
    DWORD now           = GetTickCount();
    InventoryGUI* child = gui->childInventory;

    for (int i = 0; i < MAX_GUIS; ++i)
    {
        UIState& ui = s_uiStates[i];
        if (!ui.initialized) continue;

        InventoryGUI* live = NULL;
        if (ui.gui == gui) live = gui;
        else if (ui.ownerGUI == gui && child && ui.gui == child) live = child;
        else if (ui.ownerGUI == gui)
        {
            // The backpack window closed, was destroyed or was replaced while
            // the body stayed open, without show(false) reaching us. The old
            // backpack GUI may be dead, so its widgets are left alone;
            // InitializeUI removes them if that layout is ever reused.
            ResetSlot(ui);
            LogVerbose("[StackSort] Dropped UI slot " + IntToStr(i) + " (backpack window gone)");
            continue;
        }
        else continue;

        // Same GUI pointer, different layout: our buttons went with the old
        // one, or the address now belongs to a new GUI. Forget, re-init below.
        if (live->layoutMgr != ui.layout)
        {
            ResetSlot(ui);
            continue;
        }

        ui.updateStamped = true;
        ui.lastSeenTick  = now;
        ui.lastSeenPoll  = s_uiPollCount;

        if (ui.recallPending)
        {
            ReKeyAndRecall(i, false);
            ui.captionDirty = true;
        }
        if (ui.captionDirty) UpdateCaption(i);
    }

    ReclaimStaleUIStates(now);
}

void StackSortUI::OnUpdate(InventoryGUI* gui)
{
    // Exception barrier: called from _NV_update.
    try
    {
        MaintainSlots(gui);
        InitForUpdate(gui);
    }
    catch (...)
    {
        ReportUIException("update");
    }
}

void StackSortUI::InitForUpdate(InventoryGUI* gui)
{
    if (!SortWorker::HasContext(gui)) return;

    // Path 1 (character): find ArrangeButton via childInventory. Checked even
    // when the body already has a slot, so a backpack GUI that replaced the
    // old one gets its own buttons.
    if (gui->childInventory != NULL)
    {
        InventoryGUI* bpGUI = gui->childInventory;
        if (FindUIState(bpGUI) < 0 && bpGUI->layoutMgr && HasEligibleSections(bpGUI->_NV_getInventory()))
        {
            MyGUI::Widget* btn = bpGUI->layoutMgr->getWidget("ArrangeButton");
            if (btn) InitializeUI(bpGUI, btn, gui);
        }

        // Path 1b (body): if body inventory itself has eligible non-equipped
        // sections, init its ArrangeButton too. Handles the 'main' section.
        if (FindUIState(gui) < 0 && gui->layoutMgr)
        {
            Inventory* bodyInv = gui->_NV_getInventory();
            if (bodyInv && HasEligibleSections(bodyInv))
            {
                MyGUI::Widget* bodyBtn = gui->layoutMgr->getWidget("ArrangeButton");
                if (bodyBtn) InitializeUI(gui, bodyBtn, gui);
            }
        }
        return;
    }

    // Path 2 (container): find ArrangeButton on the GUI itself.
    // Guard with HasEligibleSections — the body GUI can arrive here on the
    // first frame before childInventory is attached, and its sections are
    // all equipped. Without this guard we'd put a button group on a body
    // with nothing to sort.
    if (FindUIState(gui) < 0 && gui->layoutMgr)
    {
        Inventory* inv = gui->_NV_getInventory();
        if (inv && HasEligibleSections(inv))
        {
            MyGUI::Widget* btn = gui->layoutMgr->getWidget("ArrangeButton");
            if (btn) InitializeUI(gui, btn, gui);
        }
    }
}

void StackSortUI::OnClose(InventoryGUI* gui)
{
    if (!gui) return;

    InventoryGUI* child = gui->childInventory;

    // Clear every slot where gui matches directly (body/container) or as
    // ownerGUI (backpack owned by the closing body). Widgets are torn down
    // only for a GUI provably alive here: gui itself, or the backpack its
    // childInventory still points at. Any other backpack slot is forgotten.
    for (int i = 0; i < MAX_GUIS; ++i)
    {
        UIState& ui = s_uiStates[i];
        if (!ui.initialized) continue;
        if (ui.gui != gui && ui.ownerGUI != gui) continue;

        InventoryGUI* live = NULL;
        if (ui.gui == gui) live = gui;
        else if (child && ui.gui == child) live = child;

        InventoryLayout* layout = ui.layout;
        bool restoreVanilla     = ui.restoreVanilla;
        ResetSlot(ui);

        if (!live || live->layoutMgr != layout) continue;

        // Exception barrier per slot: the slot is already cleared, so a MyGUI
        // failure costs at most leftover buttons, which the next InitializeUI
        // on this layout removes.
        try
        {
            TeardownWidgets(live, restoreVanilla);
        }
        catch (...)
        {
            ReportUIException("close");
        }
    }
}

void StackSortUI::OnDestroyed(InventoryGUI* gui)
{
    if (!gui) return;

    for (int i = 0; i < MAX_GUIS; ++i)
    {
        UIState& ui = s_uiStates[i];
        if (!ui.initialized) continue;
        if (ui.gui != gui && ui.ownerGUI != gui) continue;
        ResetSlot(ui);
    }
}
