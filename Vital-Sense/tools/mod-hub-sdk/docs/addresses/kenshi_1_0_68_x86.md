# Kenshi 1.0.68 x86 Hook Address SSOT

This document is the transitional source of truth for hard-coded Kenshi hook
addresses that still exist in the core hub implementation for the `1.0.68` x86
runtime path.

## Scope

- Game version: `1.0.68`
- Platform selector in `InitPluginMenuFunctions(...)`: `0` (`x86`)
- Runtime owner file: `src/hub_menu_bridge.cpp`
- Consumer mods must not copy these addresses into their own code.

## Status Meanings

- `required`: still used by the current hub runtime and must remain accurate.
- `deprecated`: still present temporarily, but new usage is not allowed and the
  path is scheduled to be removed after the migration target is validated.
- `removal-target`: intentionally retained for a planned cleanup window; remove
  the literal when the replacement path is shipped and validated.

## Address Table

| Symbol | Status | RVA | Notes |
| --- | --- | --- | --- |
| `FnOptionsInit` | `required` | `0x003EFC00` | Core options-window init hook used by the hub-owned observer path. |
| `FnOptionsSave` | `required` | `0x003EC430` | Core options-window save hook for commit/apply behavior. |
| `FnOpenOptionsWindow` | `required` | `0x003FAF10` | Native options-window opener used by the gameplay `Ctrl+M` Mod Hub path. |
| `FnGetOptionsWindow` | `required` | `0x004068D0` | Returns the native `OptionsWindow` singleton before opening it from gameplay. |
| `FnCreateDatapanel` | `required` | `0x0073F980` | Creates the hub datapanel UI inside Kenshi's options flow. |
| `ForgottenGUI* g_ptrKenshiGUI` | `required` | `0x021326E0` | GUI singleton pointer used by the hub menu bridge. |

## Ownership

- The maintainer changing `src/hub_menu_bridge.cpp` owns updating this table in
  the same change.
- Do not add new hard-coded hook RVAs outside the designated core file while
  the transition is in progress.
- Consumer-facing scaffold output and SDK samples must continue to avoid
  consumer-local hook RVAs.

## Update Process

1. Change the address literal only in `src/hub_menu_bridge.cpp`.
2. Update this table in the same commit before merging.
3. Run `./scripts/phase17_address_ssot_guard_test.ps1` to verify:
   - every approved core address literal is documented here
   - no new address literals were introduced outside approved files
4. Run `./scripts/phase16_hub_attach_reliability_smoke_test.ps1` (or the Debug
   build wrapper with `-RunReliabilitySmoke`) after any hook-address change.
5. If an address is being phased out, change its status to `deprecated` or
   `removal-target` before the literal is removed.

## Related Tables

- `docs/addresses/kenshi_1_0_65_x64.md`
- `docs/addresses/kenshi_1_0_68_x64.md`
- `docs/addresses/kenshi_1_0_65_x86.md`
