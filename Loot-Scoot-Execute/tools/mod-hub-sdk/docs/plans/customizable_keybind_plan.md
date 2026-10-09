# Emkejs Mod Core: Customizable Keybind Plan

## Goal
Add a customizable hotkey to the Emkejs Mod Core plugin menu and persist it to `mod-config.json` with safe validation.

## Constraints (Must Not Change)
- Keep current stability-first behavior and fail-safe defaults.
- Keep hot path allocation-free and O(1) in `PlayerInterface::updateUT` flow.
- Keep the feature independently disableable (`enabled` toggle remains authoritative).
- Preserve compatibility with current supported Kenshi versions (`1.0.65`, `1.0.68`).
- Do not introduce per-frame key scans beyond single configured key edge detection.

## Hook Points
1. `OptionsWindowInitHook` in `Emkejs-Mod-Core.cpp` for plugin menu UI controls.
2. `OptionsWindowSaveHook` in `Emkejs-Mod-Core.cpp` for validation and persistence.
3. `HandleHotkeyAction` in `Emkejs-Mod-Core.cpp` for runtime input handling and capture processing.

## Design Overview
### 1) Runtime State
Introduce minimal global state:
- `g_hotkeyPrimary` (`OIS::KeyCode`) default `OIS::KC_X`
- `g_prevHotkeyDown` (`bool`) for edge detection
- `g_pendingHotkeyPrimary` (`OIS::KeyCode`) for UI capture before apply

### 2) Config Model
Extend `mod-config.json`:
```json
{
  "enabled": true,
  "hotkey": "X"
}
```
- Store key as canonical string (stable, readable, forward-safe).
- On load, unknown/invalid key falls back to `X` and logs a warning.
- Backward compatibility: if `hotkey` is missing, default to `X`.

### 3) UI Model (Plugin Tab)
Current implementation:
- Native keybind row is attempted first (game-native control path).
- If native row creation fails, fallback controls are injected:
  - Left label: `Dismantle Hotkey`
  - Right value button: current key / `Press key...`
  - Right reset button: `Reset (X)`
- Rebind and reset apply immediately and persist immediately (no Save step required).

### 4) Validation (No Overlap Detection)
Validate candidate key before apply/save:
- Hard block list: disallowed/system-risk keys.
- Mod-reserved keys: keys already reserved by this plugin family.
- No game-wide overlap checks.

List management strategy:
- Hard block list: code-defined static/constexpr key list in `Emkejs-Mod-Core.cpp`.
- Mod-reserved list: code-defined set/list in `Emkejs-Mod-Core.cpp` initialized once.
- Keep both lists documented next to `ValidateHotkey` and update them as plugin features grow.
- Do not make these lists user-configurable in this phase.

### 5) Apply Semantics
- During capture: show capture-active state (`Press key...`).
- On key capture:
  - valid key -> commit runtime key + persist immediately.
  - blocked key -> keep previous key.
- Never leave runtime in an invalid key state.

### 6) Capture Flow State Machine
Use a small explicit state machine to avoid accidental captures:
- `Idle`: normal options behavior.
- `AwaitKey`: entered by `Rebind Hotkey` action.
- While in `AwaitKey`, capture the next key-down edge once, validate it, set pending key, then return to `Idle`.
- `Esc` during `AwaitKey` is intended to cancel rebind and return to `Idle` with no change.
- Suppression mechanism: `HandleHotkeyAction()` returns early while capture state is `AwaitKey`, so gameplay action cannot fire during rebinding.

AwaitKey UX requirements:
- Show clear capture-active indicator (`Press key...` in rebind button caption).

## Implementation Steps
1. Refactor hotkey handling [Completed 2026-02-10]
- Rename `HandleHotkeyX` to generic `HandleHotkeyAction`.
- Replace hardcoded `OIS::KC_X` with `g_hotkeyPrimary`.
- Replace `g_prevXDown` with `g_prevHotkeyDown`.
- Add early return in runtime action handler when capture state is `AwaitKey`.

2. Add key conversion helpers [Completed 2026-02-10]
- `bool TryParseKeyCode(const std::string&, OIS::KeyCode*)`
- `const char* KeyCodeToName(OIS::KeyCode)`
- Keep lookup table static and compact.
- Parsing should trim and be case-insensitive for config text.
- `TryParseKeyCode` should only accept names present in the supported map; no numeric enum parsing from arbitrary strings.
- `KeyCodeToName` must be total: return `"UNKNOWN"` for unmapped values.
- Key map coverage for this phase: A-Z, 0-9, function keys, common control keys (Tab/Return/Space/Escape), and common punctuation/modifier keys used by players.
- Add `IsSupportedKeyCode(OIS::KeyCode)` helper and reject unsupported codes as `Invalid`.

3. Extend config I/O [Completed 2026-02-10]
- Add `ReadHotkeyFromFile` and include hotkey in existing save.
- Keep robust fallbacks and explicit warning logs.
- On load, if persisted key is unsupported, mark invalid, log warning, and fallback to default.

4. Add validation core [Completed 2026-02-10]
- `enum HotkeyValidationResult { Ok, BlockedSystem, BlockedReserved, Invalid }`
- `HotkeyValidationResult ValidateHotkey(OIS::KeyCode, std::string* reason)`
- Zero allocations in runtime path; validation only in options flow.

5. Build menu controls [Completed 2026-02-10]
- Add rebind/reset controls in `OptionsWindowInitHook`.
- Bind controls to pending key state.
- Ensure tab creation remains idempotent and safe if reopened.
- Updated implementation note: custom checkbox/dropdown capture was removed and replaced with native keybind-row attempt + fallback buttons.

6. Save/apply integration [Completed 2026-02-10]
- Validate pending/captured key and apply policy.
- Persist both `enabled` and `hotkey` in one write.
- Add runtime guard before key polling: if active hotkey is unsupported, fallback to default and log once.
- Updated behavior: rebind/reset apply and persist immediately; Save no longer required for hotkey changes.

7. Logging policy [Completed 2026-02-10]
- `WARN`: parse failures or invalid config values with fallback detail.
- `INFO`: successful key apply/save actions.
- `ERROR`: config write failures or unrecoverable menu wiring failures.
- No logging in per-frame hotkey runtime path.

## Actual Implementation Snapshot (Current)
1. Native keybind row call path is wired, but currently returns null on supported builds; fallback controls are used in practice.
2. Fallback rebind/reset controls are visible and functional.
3. Config persists:
```json
{
  "enabled": true,
  "hotkey": "X"
}
```
4. Runtime action path uses `g_hotkeyPrimary` edge detection and honors `enabled`.
5. ESC capture-cancel behavior is still not reliable in options menu due engine-level ESC handling; current code no longer includes ESC swallow workaround.

## Input/Layout Note
- Binding semantics are physical-key (`OIS::KeyCode`) based, which is stable for gameplay behavior.
- On non-US keyboard layouts, displayed key labels may not always match expected character output for some punctuation/modifier keys.
- This is an expected limitation of raw key-code binding and not a functional correctness issue.

## Performance Analysis
- Runtime cost remains O(1): single key check + edge detection.
- No per-frame iteration over full keyspace.
- Validation runs only in options interactions and save path.
- No new heap allocations required in update hook.

## Compatibility Notes
- Existing configs remain valid (`hotkey` optional).
- Unsupported key names safely degrade to default.
- Version/platform gating logic remains unchanged.

## Testing Checklist
1. Plugin loads with no crash on supported versions.
2. Default behavior unchanged (`X` works).
3. Rebind to allowed key applies and persists across restart.
4. Rebind to blocked key is rejected.
5. `Esc` cancels capture without changing key. (Known issue: options menu may close first.)
6. While awaiting key capture, gameplay hotkey action does not execute.
7. AwaitKey UI clearly shows capture-active state (`Press key...`).
8. `enabled=false` disables action regardless of key.
9. Corrupt/missing `hotkey` in config falls back to default safely.
10. Unsupported key values (if injected manually) are rejected and fallback to default.
11. Soak test in active settlements: no regressions/crashes.
12. Confirm no per-frame logging in release behavior.

## Build & Install Steps
1. Build plugin in Release (`x64`).
2. Copy updated DLL and `mod-config.json` schema into mod folder.
3. Launch Kenshi with RE_Kenshi enabled.
4. Open plugin options and verify keybind UI and save/apply behavior.

## Disable / Rollback Steps
1. In plugin menu, disable Emkejs Mod Core with existing toggle.
2. Remove `hotkey` entry from `mod-config.json` to force default `X`.
3. Full rollback: replace DLL with previous release.

## Decision Log (Updated)
- Overlap detection: intentionally omitted.
- UI implementation default: native keybind row attempt, fallback to injected buttons.
- Key lists: code-defined in this phase (not user-configurable).
- Config representation: string key names for readability and backward compatibility.
- Input model: physical-key-based (`OIS::KeyCode`), with known display limitations on some layouts.
- UI binding control: native keybind row in plugin options if available; fallback button-row otherwise.
- Status line UI: removed.

## Delivery Phases
1. Phase 1: Config + runtime generalized hotkey (no UI capture yet).
2. Phase 2: Plugin UI capture/reset wiring.
3. Phase 3: Basic validation enforcement (system/reserved only).
4. Phase 4: Stabilization pass + compatibility and soak testing.
