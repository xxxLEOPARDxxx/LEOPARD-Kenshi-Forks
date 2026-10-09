# Changelog

All notable changes to Emkejs-Mod-Core will be documented in this file.

## [0.1.0] - 2026-03-05
- Added Mod Hub v1 core functionality:
  - ABI handshake/export surface (`EMC_ModHub_GetApi`) with version/size guards.
  - Hub registry and UI flows for bool, keybind, int, float, and action rows.
  - Commit/apply pipeline with callback error propagation and summary telemetry.
- Added consumer SDK + scaffold workflow:
  - Public helper runtime (`include/emc/mod_hub_client.h`, `src/mod_hub_client.cpp`).
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

## [0.0.0] - 2026-02-24
- Reset repository to a clean Emkejs Mod Core plugin base.
- Removed legacy feature implementation and related release artifacts.
