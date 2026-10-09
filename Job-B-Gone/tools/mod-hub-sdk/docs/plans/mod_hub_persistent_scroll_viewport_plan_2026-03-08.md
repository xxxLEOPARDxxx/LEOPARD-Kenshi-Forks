# Mod Hub Persistent Scroll Viewport Plan (2026-03-08)

## Summary
Replace the current rebuild-driven Mod Hub scrolling path with a persistent custom viewport/content container.

This plan is intentionally narrow:
- keep current wheel routing logic as the starting point
- stop using rebuilds for scrolling
- stop depending on the current native `ScrollView` path
- introduce persistent widgets before any row virtualization or fine-grained diffing

Primary outcome:
- wheel scrolling, scrollbar interaction, and row clicks no longer reset or visually jump the settings list

## Why This Plan
Current evidence shows the problem is architectural, not just input-related.

Observed from current logs and code:
1. Wheel events are reaching hub widgets, including hovered controls.
2. The current native `ScrollView` path is not functional in this menu context:
   - intended offset changes
   - actual viewport offset stays at `0`
   - no usable native vertical scrollbar is exposed
3. The manual fallback path still rebuilds the panel on wheel and many click actions, which causes rough motion and visual jumps.

Relevant code paths today:
- [src/hub_menu_bridge.cpp:1332](/mnt/i/Kenshi_modding/Emkejs-Mod-Core/src/hub_menu_bridge.cpp:1332) `OnHubMouseWheel(...)`
- [src/hub_menu_bridge.cpp:1433](/mnt/i/Kenshi_modding/Emkejs-Mod-Core/src/hub_menu_bridge.cpp:1433) `OnHubButtonClicked(...)`
- [src/hub_menu_bridge.cpp:2032](/mnt/i/Kenshi_modding/Emkejs-Mod-Core/src/hub_menu_bridge.cpp:2032) `ApplyHubScrollOffsetWithoutRebuild(...)`
- [src/hub_menu_bridge.cpp:2186](/mnt/i/Kenshi_modding/Emkejs-Mod-Core/src/hub_menu_bridge.cpp:2186) `RebuildHubPanelWidgets()`

## Scope
In scope:
1. Persistent viewport/content widget model for the settings list.
2. Persistent custom scrollbar track/thumb/buttons.
3. Scroll offset updates without full rebuilds.
4. Layout refresh without destroying widgets.
5. Offset preservation across structural changes.

Out of scope:
1. Row virtualization or recycling.
2. Revisiting native Kenshi/MyGUI `ScrollView` behavior in this phase.
3. Full row-diff engine or per-control partial rendering.
4. Search/filter behavior changes beyond preserving scroll/layout correctly.

## Non-Negotiable Invariants
1. Scrolling must never call `RebuildHubPanelWidgets()` directly or indirectly.
2. `g_hub_scroll_offset` remains the single source of truth for vertical position.
3. Scrollbar position and content position must both derive from the same clamped offset.
4. Structural changes may rebuild rows; pure scroll changes may not.
5. If content height changes, the existing offset must be clamped and reapplied, not reset by default.

## Current Architecture Problems
1. `OnHubMouseWheel(...)` still falls back to `SetHubScrollOffset(...)` plus `RebuildHubPanelWidgets()` when the native path does not apply.
2. Scrollbar/button actions in `OnHubButtonClicked(...)` also still rebuild the full panel.
3. `RebuildHubPanelWidgets()` mixes:
   - structure creation
   - search UI creation
   - viewport creation
   - scroll controls
   - row creation
   - offset restoration
4. The fallback scrollbar is rebuilt with the panel, so it cannot provide stable continuous motion.

## Target Architecture
Introduce a persistent scroll subtree under the active hub panel:

1. `viewport widget`
   - clips visible content area
2. `content widget`
   - child of viewport
   - holds all mod headers and row widgets
   - moves vertically based on `g_hub_scroll_offset`
3. `scrollbar widgets`
   - persistent line up/down buttons
   - persistent track
   - persistent thumb or scroll bar control

Conceptually:
```cpp
content_widget->setPosition(x, content_base_y - g_hub_scroll_offset);
```

This keeps scrolling continuous without rebuilding the widget tree.

## Required Operation Split
Replace the current single rebuild-heavy model with 4 explicit operations.

### 1. `BuildHubScrollUi()`
Create once per active panel:
- viewport widget
- content widget
- persistent scrollbar widgets

Responsibility:
- only create the scroll container and controls

### 2. `PopulateHubRows()`
Create row widgets for the current filtered namespace/mod structure.

Responsibility:
- create mod headers and row widgets under the persistent content widget
- store enough metadata to relayout them later

### 3. `SetHubScrollOffset(int offset)`
Single scroll state setter.

Responsibility:
- clamp offset
- update `g_hub_scroll_offset`
- move content widget
- sync scrollbar position

Must not:
- create widgets
- destroy widgets
- call full rebuild

### 4. `RefreshHubLayout()`
Recompute content height, row positions, and scrollbar range using existing widgets.

Responsibility:
- reposition persistent row widgets
- recompute max scroll
- clamp existing offset
- reapply offset visually

Use this when:
- row visibility changes
- collapse/expand changes content height
- search/filter changes which existing rows are shown

## Minimal Implementation Strategy
Do not start with per-row micro-refresh. First make the viewport persistent and stable.

Phase 1 target behavior:
1. Wheel moves persistent content.
2. Scrollbar moves persistent content.
3. Clicking a toggle/value control does not reset the viewport.
4. Structural changes preserve a clamped offset after layout refresh.

That is enough to solve the current UX failure without overengineering the row update model.

## Mapping To Current Functions
### Keep but narrow
1. [src/hub_menu_bridge.cpp:1332](/mnt/i/Kenshi_modding/Emkejs-Mod-Core/src/hub_menu_bridge.cpp:1332) `OnHubMouseWheel(...)`
   - keep as scroll-input entry point
   - remove any rebuild fallback
   - make it call only `SetHubScrollOffset(...)`

2. [src/hub_menu_bridge.cpp:1433](/mnt/i/Kenshi_modding/Emkejs-Mod-Core/src/hub_menu_bridge.cpp:1433) `OnHubButtonClicked(...)`
   - keep for actions
   - scroll button actions should call only `SetHubScrollOffset(...)`
   - setting-toggle/value-change actions should avoid rebuilding the whole panel unless structure changes

3. [src/hub_menu_bridge.cpp:2186](/mnt/i/Kenshi_modding/Emkejs-Mod-Core/src/hub_menu_bridge.cpp:2186) `RebuildHubPanelWidgets()`
   - keep temporarily as the structural-reset path
   - narrow it to true structural resets only

### Replace or retire
1. [src/hub_menu_bridge.cpp:2032](/mnt/i/Kenshi_modding/Emkejs-Mod-Core/src/hub_menu_bridge.cpp:2032) `ApplyHubScrollOffsetWithoutRebuild(...)`
   - replace with a real persistent-offset path
   - stop making its behavior conditional on native scroll-view existence

2. Native `ScrollView` helpers and probes
   - retire from the active path in this phase
   - no more attempts to salvage the current native route

### Introduce
1. `BuildHubScrollUi()`
2. `PopulateHubRows()`
3. `RefreshHubLayout()`
4. `ClearHubRowWidgets()` or equivalent row-only teardown
5. Small row metadata structure:
   - widget pointers
   - row height
   - visible/hidden state
   - logical Y position

## Recommended Phases
## Phase A: Isolate scroll state from rebuild
1. Remove all `RebuildHubPanelWidgets()` calls from:
   - `OnHubMouseWheel(...)`
   - scroll button branches in `OnHubButtonClicked(...)`
   - scrollbar position changes
2. Route all scroll position changes through one setter.
3. Add one debug assertion or guard that scrolling code paths do not trigger full rebuild.

Exit criteria:
- wheel and scroll buttons no longer rebuild the panel

## Phase B: Introduce persistent viewport/content widgets
1. Create a persistent viewport widget inside the panel.
2. Create a persistent content widget as the scrollable child.
3. Move row/widget creation under the content widget instead of directly under the panel.
4. Move content by offset instead of rebuilding.

Exit criteria:
- content visibly scrolls by moving the content widget only

## Phase C: Make scrollbar persistent
1. Create persistent scrollbar controls once.
2. Sync scrollbar range/thumb from content height and viewport height.
3. Keep scrollbar visuals stable across row interactions.

Exit criteria:
- scrollbar remains visible and stable while scrolling and clicking rows

## Phase D: Split structural rebuild from layout refresh
1. Keep full rebuild only for:
   - namespace changes
   - search/filter structure changes
   - collapse/expand changes if row visibility cannot be updated in place yet
2. Add `RefreshHubLayout()` for:
   - recomputing heights
   - applying current offset
   - repositioning existing widgets

Exit criteria:
- most row interactions update in place without resetting the scroll context

## Phase E: Optional follow-up refinement
1. Replace remaining structural rebuild cases with hide/show and row relayout where practical.
2. Tune scrollbar visuals separately from architecture.
3. Consider virtualization only if row count proves large enough to justify it.

## Validation Plan
Manual validation:
1. Wheel scroll works over:
   - empty space
   - toggle buttons
   - numeric buttons
   - text inputs
2. Clicking a row control after scrolling down does not jump to the top.
3. Collapse/expand preserves a sensible clamped offset.
4. Search filtering preserves a sensible clamped offset.
5. Scrollbar thumb/position stays in sync with visible content.

Lightweight code validation:
1. `rg "RebuildHubPanelWidgets\\(" src/hub_menu_bridge.cpp`
   - verify scroll-only paths no longer call it
2. Build Debug package:
```bash
scripts/build-and-package.ps1 -Configuration Debug -Platform x64 -SkipSdkPackage
```

## Stop Conditions
Stop and reassess if any of these happen:
1. The persistent content widget cannot clip correctly inside the panel.
2. Existing row controls depend on rebuild-time recreation for correctness in ways not isolated yet.
3. Scrollbar synchronization requires invasive skin-specific behavior that exceeds this phase.

If any stop condition triggers, prefer a smaller intermediate step rather than returning to the current native `ScrollView` chase.

## Recommended First Cut
Smallest safe implementation order:
1. Remove rebuilds from wheel/scroll button paths.
2. Add persistent viewport + content widget.
3. Move rows under the persistent content widget.
4. Move content by offset.
5. Keep full structural rebuilds for search/namespace changes temporarily.

This is the highest-ROI cut because it directly targets the current breakage without committing to a larger rendering rewrite.
