#pragma once

#include <mygui/MyGUI_MouseButton.h>

class InventoryGUI;
class Inventory;
class InventorySection;

namespace MyGUI
{
class Widget;
}

class StackSortUI
{
  public:
    static void OnWorldReset(); // forget all slots without touching native UI
    // Called from hook_autoArrangeButton. Initializes UI on first eligible
    // call, applies sort at current H, increments H, plays sound.
    // Returns true if handled, false if caller should fall through to vanilla.
    static bool OnForwardClick(InventoryGUI* gui, MyGUI::Widget* sender);

    // Called from hook_update each frame for a visible top-level non-trader
    // GUI (gui is thisptr, so provably alive). Stamps and services the slots
    // gui vouches for (its own, and its current backpack's), drops a backpack
    // slot whose child GUI is gone or replaced, reclaims slots whose GUI
    // stopped updating, then attempts UI init via
    // the optional ArrangeButton in childInventory->layoutMgr or, for
    // containers, gui->layoutMgr (quietly skip layouts without that button).
    // A slot's gui/ownerGUI is dereferenced only from that GUI's own update
    // (or its owner's, for a backpack) or a click on its own buttons.
    // Main thread only. Exception barrier.
    static void OnUpdate(InventoryGUI* gui);

    // gui is closing and provably alive (hook_show(false) before the original
    // runs, or the hook_update stale-context reset). Frees every slot of gui
    // and of backpacks it owns; for gui and its current childInventory also
    // destroys our buttons and re-shows the vanilla ArrangeButton, so a
    // layout that outlives the close is left as vanilla had it.
    // Main thread only. Never throws.
    static void OnClose(InventoryGUI* gui);

    // gui is being destroyed (~InventoryGUI hook). Frees every slot of gui and
    // of backpacks it owns without touching any GUI or widget: the layout may
    // be mid-teardown, and at game exit MyGUI itself may be gone.
    // Main thread only. Never throws.
    static void OnDestroyed(InventoryGUI* gui);

    // Marks every open UI's caption for refresh; each is redrawn from its own
    // GUI's next OnUpdate (another GUI's buttons may already be destroyed).
    static void RefreshAllCaptions();

    // Bypass protected access on InventoryGUI::refreshAllSections. Defined in
    // StackSortUI.cpp so callers don't need the kenshi/gui/InventoryGUI.h header.
    static void ForceRefreshAllSections(InventoryGUI* gui);

  private:
    static void MaintainSlots(InventoryGUI* gui);
    static void InitForUpdate(InventoryGUI* gui);
    static void InitializeUI(InventoryGUI* gui, MyGUI::Widget* arrangeBtn, InventoryGUI* ownerGUI);
    static void OnBackClick(MyGUI::Widget* sender);
    static void OnDimClick(MyGUI::Widget* sender);
    static void OnForwardArrowClick(MyGUI::Widget* sender);
    static void OnSortCycleClick(MyGUI::Widget* sender);
    static void OnSortRightClick(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id);
    static void CaptureOriginalPositions(InventoryGUI* gui);

    // Revert all eligible sections to pre-sort positions. Returns true if
    // any sections were reverted. Caller handles UIState reset + caption.
    static bool RevertInventory(InventoryGUI* gui);

    // UpdateCaption plus a TargetMemory write. Called from the four user
    // mutations only, never from InitializeUI / RefreshAllCaptions / OnDimClick.
    static void CommitTarget(int uiIdx);

    // Rebuild the UI's memory key for the active dim and pull the remembered
    // target back into currentTarget, clamped to what this inventory can reach.
    // Caption only: nothing is re-laid-out behind the user. Reads the slot's
    // inventory: call only where its gui is provably alive.
    static void ReKeyAndRecall(int uiIdx, bool logRestore);

    static void FirstClickInit(int uiIdx);
    static void CycleTargetForward(int uiIdx);
    static void BuildTopTargets(int uiIdx);

    // Apply sort at target value for all eligible sections under the active
    // dim. Sets outAppliedLerSide to max lerSide across eligible sections
    // (for forward-skip) and outSourceTarget to the source slot for W-mode skip.
    static void ApplySort(InventoryGUI* gui, int target, int& outAppliedLerSide, int& outSourceTarget);

    static int FindMaxTarget(InventoryGUI* gui);
    static int PeekAppliedLerSide(InventoryGUI* gui, int target);
    static bool HasEligibleSections(Inventory* inv);
    static void PlayClickSound();
    static void UpdateCaption(int uiIdx);

    static int FindUIState(InventoryGUI* gui);
    static int AllocUIState(InventoryGUI* gui);

    // Merge stackable items in a section. Returns true if any merged.
    // Backpack sections only, main thread, wrapped in ApplyGuard+CallbackGuard.
    static bool ConsolidateStacks(InventorySection* section);
};
