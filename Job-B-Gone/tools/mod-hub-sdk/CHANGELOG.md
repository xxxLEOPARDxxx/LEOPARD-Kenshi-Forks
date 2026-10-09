# Changelog

All notable changes to Emkejs-Mod-Core will be documented in this file.

## [0.1.0-alpha.2] - 2026-03-12
### NexusMods summary
- Gives supported mods one shared in-game settings menu with better search, keyboard shortcuts, smoother scrolling, configurable debug logging defaults, quieter release diagnostics, clearer numeric helper text, and build/release workflow cleanup.

### Steam Workshop summary
- Gives supported mods one shared in-game settings menu with better search, configurable debug logging defaults, quieter diagnostics, clearer numeric helper text, and smoother browsing.

- Added logging policy scaffolding:
  - Added shared logging helpers in `src/logging.cpp` / `src/logging.h`.
  - Added `debugLogging`, `debugSearchLogging`, and `debugBindingLogging` to `mod-config.json`, all defaulting to `false`.
  - Gated low-value debug/search/binding logs behind the new logging policy so release behavior stays quieter by default.
- Added verification coverage:
  - Added `scripts/phase21_logging_policy_smoke_test.ps1` to validate the logging policy surface and defaults.
- Changed release/build workflow docs:
  - Clarified the shared build-scripts subtree workflow in `README.md`, including the local wrapper-script boundary and the per-clone `build-scripts` remote expectation for subtree pulls.
  - Cleaned up release wording around the current Mod Hub feature set.
- Fixed build stability:
  - Avoided `std::string`-backed logging inside the startup SEH handler so the current tree builds cleanly under MSVC.
- Includes the alpha.2 core runtime, SDK, reliability, and UX work finalized on 2026-03-09:
- Added Mod Hub v1 core functionality:
  - ABI handshake/export surface (`EMC_ModHub_GetApi`) with version/size guards.
  - Hub registry and UI flows for bool, keybind, int, float, and action rows.
  - Commit/apply pipeline with callback error propagation and summary telemetry.
- Added consumer SDK + scaffold workflow:
  - Shared helper runtime (`include/emc/mod_hub_client.h`, `src/mod_hub_client.cpp`).
  - `init-mod-template --with-hub` generation for adapter wiring and optional bool-setting scaffolds.
  - SDK packaging pipeline and quick-start integration docs.
- Added reliability v1.1 hardening:
  - Deploy lock preflight with actionable failure diagnostics.
  - Export contract stability with canonical-first lookup and temporary alias support.
  - Options-init observer API with legacy fallback compatibility path.
  - Unified attach/fallback smoke matrix plus address SSOT guardrails and local hook automation.
- Added verification harness coverage through phases 1-18, including phase16 reliability smoke and phase18 dummy-consumer menu smoke.
- Changed core ownership boundary:
  - Removed embedded Wall-B-Gone bridge logic from Mod Core runtime.
  - Kept phase6 as a delegating wrapper to the consumer-owned Wall-B-Gone harness.
- Fixed release-safety gaps:
  - Isolated dummy-consumer test-only code behind `EMC_ENABLE_TEST_EXPORTS`.
  - Tightened phase6 wrapper skip signaling (skip no longer reports a green pass).
  - Lowered alias deprecation event logging from error channel to debug channel.
  - Ignored local `RE_Kenshi_log.txt` runtime artifact in git.
- Added post-plan reliability/docs alignment (2026-03-06):
  - Added shared consumer callback helper header (`include/emc/mod_hub_consumer_helpers.h`) and switched scaffold templates to use it for bool/int/float/keybind/action row callback patterns.
  - Added SDK stamp drift warning support in `ModHubClient` (`expected_sdk_api_version`, `expected_sdk_min_api_size`) with runtime warning emission when contract drift is detected.
  - Added `scripts/sync-mod-hub-sdk.ps1` scoped to sync + validate only; command does not modify changelog/release-note files.
  - Added `scripts/phase19_sdk_sync_command_test.ps1` and extended docs/test coverage to enforce sync-command behavior and updated SDK guidance.
- Added Mod Hub UX and build workflow polish (2026-03-07):
  - Mod Hub search now supports scoped `mod:term` queries, shows a clear button, and preserves cursor position across rebuilds.
  - Added search-box keyboard editing shortcuts: `Ctrl+Left`, `Ctrl+Right`, and `Ctrl+Backspace`.
  - Migrated shared `tools/build-scripts` consumption from submodule/vendored-copy hybrid handling to the shared subtree workflow.
  - Shared build/package/deploy scripts now print one final timestamp footer on both success and failure paths.
- Added Mod Hub stability and layout polish (2026-03-09):
  - Hardened startup/runtime behavior around version detection and Options-window Mod Hub attachment to avoid current crash paths.
  - Fixed a Mod Hub issue where the tab could fail to appear in Options on some runs by making panel wheel delegate attachment best-effort.
  - Reworked Mod Hub scrolling around the persistent viewport path so list scrolling is stable and no longer rebuilds on every wheel step.
  - Fixed scroll-state rendering so rows stay visible while scrolling and the menu remains attachable after the recent clamp experiments.
  - Polished the Mod Hub list UI with larger setting labels, right-aligned muted numeric range hints, a `Collapse all` / `Expand all` toggle, extra bottom padding for the last row, and a narrower scrollbar gutter.
  - Numeric int/float rows now use row descriptions as helper footers when provided, while generated range hints stay aligned to the control group.
  - Mod Hub search now stays in place when you close and reopen Options during the same Kenshi session, and Emkejs Mod Core exposes toggles for search persistence, collapse-state persistence, and optional search auto-focus in the hub itself.
  - Search focus is now more keyboard-friendly: Mod Hub can focus the search box automatically when the tab is opened, and `Ctrl+F` or `/` now jump focus into search while Mod Hub is active.

## [0.0.0] - 2026-02-24
- Reset repository to a clean Emkejs Mod Core plugin base.
- Removed legacy feature implementation and related release artifacts.
