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

The new one-time planner reserves all visible direct siblings intersecting the
button row, clamps the strip to its live parent and uses measured text widths.
It first retains full words, then a localized compact caption, then `[N]` with
a localized native tooltip. It preserves all four controls, font size and
the original Arrange widget/name/coordinates/delegates. It never moves/hides
KEO's checkbox or destroys foreign widgets.

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
The native integer/charge merge and solver tests remain separate.

This is source/geometry/model verification. Runtime acceptance with the actual
game UI, Dark UI, KEO hover/click handling, resize and MCM disable/re-enable is
still required. An exotic row too narrow even for the minimum four controls
deliberately retains native sorting rather than clipping interactive controls.
