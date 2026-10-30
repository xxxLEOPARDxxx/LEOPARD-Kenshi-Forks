# Arrange replacement and KEO coexistence, 2026-10-31

Source/native evidence before changes:

- StackSort `InitForUpdate` resolves each body's own Arrange and its backpack's
  Arrange separately, after native `_NV_update` completes. Both previously
  passed to `InitializeUI`, which hid the original and made four custom buttons.
- Steam native `InventoryGUI::autoArrangeButton` at **0x70EB70–0x70EBCF** gets
  this panel's inventory through vtable slot 0x70, calls native arrange once,
  refreshes and plays the click sound. The StackSort detour returns after its
  own handled click; disabled, trader and ineligible paths call the saved
  native handler once. No second delegate is attached to original Arrange.
- Native `_NV_update` **0x712CE0–0x712DC6** calls refreshAllSections (when dirty),
  updateDatapanel, section updates and stealingGUIInfoUpdate. Native
  refreshAllSections **0x70E7F0–0x70EA45** updates title/data/sections. No direct
  Arrange re-show was found in either inspected body.
- KEO `src/inventory/backpack_window.cpp` creates only a BackpackFirst checkbox.
  It never sorts or changes Arrange visibility/delegates. Its post-hook on
  setupSections places the checkbox at original Arrange.right+4, with the
  same top/height. It completes synchronously before native update returns,
  hence before StackSort's post-update initializer, regardless of hook order.
- The demonstrated geometric defect: a fixed 286px strip centered on a
  narrower original Arrange can intersect this checkbox or existing body
  equipment widgets. Changing the original anchor would also disturb KEO.

The one-time planner reserves visible direct siblings intersecting the button
row, clamps the strip to its live parent and uses measured text widths.
It first retains full words, then a localized compact caption, then `[N]` with
a localized native tooltip. It preserves all four controls, font size and
the original Arrange widget/name/coordinates/delegates. Body placement is
unchanged. Backpack placement centers the actual fitted width of all four
controls plus the visible exact same-prefix `keo_backpackFirst` checkbox as
one row around the actual inventory grid center; without that checkbox it centers
four controls. Other widgets remain reserved and are never moved/destroyed.
The old Arrange can be off-center to leave room for other controls and is not
the backpack centering reference. Init/resize reads at most 64 native section
records and corresponding `inventorySections` entries, includes eligible visible
grid widgets that descend from this parent, and converts their absolute spans
into parent-local coordinates. Without a usable grid, the Content client span
is the fallback. This lookup runs only during initialization/rebuild.
Read-only unpacked Steam native verification: `InventorySectionGUI::getWidget`
at **0x70C0A0** is `mov rax,[rcx]; ret`; its constructor at **0x70D9B0** stores
the supplied widget with `mov [rcx],rdx` at **0x70D9DE**. Native
`getItemAbsolutePosition` at **0x70CA10** reads the same widget and its absolute
position fields. These confirm the SDK's section-widget field at offset zero.
The installed `Kenshi_InventoryGenericFixedWindow.layout` separately identifies
`backpack_content` and an off-center `ArrangeButton`, consistent with using the
grid rather than the old button's span. `getAbsoluteCoord()` is the SDK's inline
read of MyGUI absolute position and current widget size, not an extra native call.

KEO coordination changes only left/top after all four custom controls are ready.
Static shared `WidgetRef` leases for checkbox and parent are nulled by MyGUI
unlink notifications; no raw foreign widget pointer survives initialization.
No checkbox name, size, selection, owner userstring or delegate is changed.
On close, disable or rebuild the original position is restored plus the delta
between current/applied positions, preserving external movement and Right/Bottom
resize. Reparented widgets release the lease without applying coordinates from
their old parent. Forgotten UI slots/world reset retain only these weak leases;
surviving layouts restore them on next initialization or close/disable, while
destroyed widgets automatically clear the references. World reset itself still
does not dereference native GUI objects. No per-frame tree/child scans were added.
Close/disable also removes orphan StackSort buttons and restores the exact
original Arrange visibility on provably live layouts whose slot was forgotten,
using only StackSort's namespaced visibility baseline as the ownership marker.
An unmarked native layout and another live slot's buttons are left alone.

Four hidden custom widgets are prepared before the replacement commits. Failure
or an exceptionally narrow row retains the native Arrange control and its
single native callback. Fallback is cached per live layout, with one debug
reason. A cached O(1) size check permits one re-plan after actual resizing;
steady frames do not scan templates, children, measure text or create widgets.
Tooltip registration is cleared before destroying our sort button.

Tests compile the actual production InitializeUI, OnClose, MaintainSlots,
RemoveOurButtons and autoArrangeButton bodies against widget/failure models.
They cover full/compact/numeric captions, original identity/geometry/delegate,
both rectangle sides, parent clamping, original hidden state, stale leftovers,
creation failures before/after tooltip registration, resize out of and back
into fallback, and exactly one native callback in each native fallback case.
Backpack tests include screenshot-sized 390px parents, measured RU/EN/ZH caption
widths, checkbox false/true selection, foreign movement/resize, reparenting,
unlink, lost tracking slot and rebuild/disable restoration.
The native integer/charge merge and solver tests remain separate.

This is source/geometry/model verification. Runtime acceptance with the actual
game UI, Dark UI, KEO hover/click handling, resize and MCM disable/re-enable is
still required. An exotic row too narrow even for the minimum four controls
deliberately retains native sorting rather than clipping interactive controls.
