# Emkejs Mod Core Hub v1.1 Reliability And Consumer Simplification Plan

## Summary
Harden post-v1 Mod Hub integration so third-party mods can attach and fallback predictably with less per-mod glue code.

Primary outcomes:
1. Deploy fails fast with actionable guidance when target DLL is locked/in-use.
2. Consumer options-init retry no longer depends on per-mod hard-coded RVAs.
3. Hub attach/export contract is versioned and resilient to symbol transitions.
4. SDK integration path is simpler and remains single-implementation SSOT.
5. Automated smoke coverage validates attach/fallback/retry/failure paths.
6. Required game hook addresses have one SSOT document per version/platform during transition.

## Background Problems (Observed)
1. Deploy may fail when the consumer DLL is loaded by a running game process.
2. Consumer options-init retry often relies on hard-coded Kenshi 1.0.65 RVAs in each mod.
3. Attach currently depends on one export symbol (`EMC_ModHub_GetApi`) with no alias strategy.
4. Consumer bootstrap/wiring still has repetitive steps across mods.
5. Reliability paths are tested by multiple scripts but not one canonical smoke matrix.
6. Address ownership/versioning is not documented as a strict SSOT.

## Scope
In scope:
1. Deploy preflight lock detection and error UX improvements.
2. Stable observer/export API for options window lifecycle notifications.
3. Export stability policy and compatible alias lookup path.
4. SDK/scaffold simplification with generated sample assets.
5. New/updated validation harnesses for attach/fallback/retry matrix.
6. Address SSOT docs and governance while transitional hook logic exists.

Out of scope:
1. New hub setting row types.
2. Hub API v2 breaking changes.
3. Broad callback abstraction layers that hide mod state logic.
4. Full removal of all game-hook addresses in this phase unless observer rollout is complete.

## Design Constraints
1. Preserve existing v1 behavior for current consumers by default.
2. Keep SDK SSOT to one helper runtime implementation (`mod_hub_client.cpp`).
3. Prefer extending existing scaffold (`init-mod-template --with-hub`) over creating parallel generators.
4. Keep abstractions opt-in and minimal; avoid heavy wrapper frameworks.
5. Every behavior change requires scriptable validation criteria.

## Mandatory Decisions (Freeze Before Implementation)
1. Observer ABI growth model:
append optional observer function pointers to the end of `EMC_HubApiV1`, keep `EMC_HUB_API_V1_MIN_SIZE` unchanged, and require feature detection via `out_api_size` before invocation.
2. Export compatibility policy:
`EMC_ModHub_GetApi` remains canonical; one compatibility alias may be shipped for exactly one minor release and must emit a deprecation warning event when used.
3. Script/phase naming convention:
new reliability harnesses must align with phase numbering to keep plan and tests traceable (`phase12_*`, `phase13_*`, etc.).
4. Deprecation boundary:
consumer-local options-init RVA hooks are marked deprecated only after observer path is validated in at least one real consumer and reliability smoke matrix is green.

## Assumptions
1. Current baseline target remains Kenshi `1.0.65` x64 for hook-related behavior.
2. Existing consumers can adopt updated SDK helper without changing hub-internal code copies.
3. Local automation/runtime environment can execute PowerShell harnesses used in current phase tests.

## Workstream A: Deploy Lock Preflight
### Objective
Make deploy/build scripts fail predictably before copy operations if destination DLL cannot be replaced.

### Implementation
1. Add preflight lock check in deploy paths before copying plugin binaries.
2. Detect active lock/process and emit actionable error text:
`target_path`, suspected process name/PID when available, and next step guidance.
3. Keep exit code non-zero and explicit for automation/local script handling.
4. Do not kill processes automatically; fail fast with guidance only.

### Artifacts
1. `scripts/build-deploy.ps1` (and any wrapper path invoking deploy copy).
2. `scripts/build-and-deploy.ps1` / `scripts/build-and-package.ps1` integration points as needed.
3. Updated README deploy troubleshooting section.

### Validation
1. Manual: run deploy with game closed -> pass.
2. Manual: run deploy with DLL locked -> clear preflight failure message.
3. Automated: add a script test using a temp file lock to verify preflight behavior.

## Workstream B: Stable Options-Init Observer API
### Objective
Remove per-consumer RVA dependence for options-init attach retry.

### Implementation
1. Add a hub-owned lifecycle observer export surface (optional in `EMC_HubApiV1` via size-gated function pointers):
`register_options_window_init_observer(...)` and `unregister_options_window_init_observer(...)`.
2. Hub core owns the game hook once; consumers register callbacks through ABI/helper rather than using per-mod hard-coded options-init RVAs.
3. Define invocation contract explicitly:
callbacks run on main thread during options-window-init lifecycle point; callback must be non-reentrant.
4. Helper (`mod_hub_client.cpp`) uses observer registration when available; fallback to current explicit consumer `OnOptionsWindowInit()` call path if observer API is absent.
5. Keep one-retry semantics unchanged.

### Artifacts
1. `include/emc/mod_hub_api.h` additions (versioned/size-checked).
2. Hub export table implementation and observer registry.
3. `include/emc/mod_hub_client.h` + `src/mod_hub_client.cpp` integration updates.
4. Migration notes in `docs/mod-hub-sdk.md`.

### Validation
1. Observer path: attach failure on startup, observer-triggered retry on options-init, eventual success.
2. Observer path fallback: observer unavailable -> old behavior remains functional.
3. Confirm no per-consumer hard-coded options-init RVA required in scaffold-generated mods.
4. Confirm unregister path cleanup during plugin shutdown (no stale callbacks).

## Workstream C: Export Contract Stability
### Objective
Avoid attach failures from export naming drift.

### Implementation
1. Declare `EMC_ModHub_GetApi` as stable contract symbol in docs and SDK metadata.
2. Add optional compatibility alias export for one release cycle when symbol transitions are needed.
3. Loader/helper lookup strategy:
try canonical name first, then known alias list.
4. Emit one warning log when alias path is used.
5. Publish alias removal target release at introduction time.

### Artifacts
1. Export docs updates.
2. Loader/helper symbol lookup update.
3. Regression test for canonical + alias attach path.
4. Logged event contract for alias usage/deprecation.

### Validation
1. Canonical symbol path passes existing handshake tests.
2. Alias path succeeds and logs compatibility warning once.
3. Missing symbol still returns deterministic fallback behavior.
4. After alias removal toggle in test configuration, alias path fails with explicit fallback reason.

## Workstream D: SDK And Scaffold Simplification
### Objective
Reduce consumer setup friction without introducing duplicate runtime implementations.

### Implementation
1. Keep SDK runtime SSOT as:
`include/emc/mod_hub_client.h` + `src/mod_hub_client.cpp`.
2. Extend `init-mod-template --with-hub` with optional richer presets:
namespace/mod IDs, bridge wiring, and optional single-TU sample generation.
3. Publish generated minimal single-TU consumer sample and validate it in automated local checks.
4. Add small opt-in helper utilities only for repetitive primitives (for example, error-buffer write helpers), not full bool-setting framework wrappers.
5. Do not add a parallel scaffold command (`init-hub-bridge`); all generation remains under `init-mod-template --with-hub`.

### Artifacts
1. `scripts/init-mod-template.ps1` and `.sh` enhancements.
2. `scripts/templates/*` updates.
3. SDK package sample asset(s) and docs cross-links.
4. Explicit migration notes from old scaffold output to new output.

### Validation
1. Fresh scaffold output compiles without internal hub includes.
2. Single-TU sample compiles with SDK headers/source only.
3. Existing scaffold flags remain backward compatible.

## Workstream E: Reliability Smoke Matrix
### Objective
Create one canonical smoke harness for attach/fallback reliability paths.

### Matrix
1. Attach success at startup.
2. Attach fail at startup + retry success at options-init.
3. Attach fail at startup + retry fail -> local fallback remains active.
4. Attach success + registration failure -> local fallback remains active.
5. Export symbol missing/invalid -> deterministic fallback.

### Artifacts
1. New script:
`scripts/phase16_hub_attach_reliability_smoke_test.ps1`.
2. Local automation integration for smoke matrix in Debug harness build.
3. Plan/docs references for required local environment inputs.

### Validation
1. Script prints explicit pass/fail per matrix case.
2. Failures include direct cause context (`attach`, `retry`, `registration`, `symbol lookup`).
3. Smoke harness remains non-flaky across repeated runs.

## Workstream F: Address SSOT Governance (Transitional)
### Objective
Centralize any remaining required hook RVAs while observer rollout is in progress.

### Implementation
1. Create one address table document per game version/platform.
2. Define ownership + update process:
who updates, when, and how changes are validated.
3. Restrict hard-coded address usage to designated files; add grep-based guard in local automation if practical.
4. Mark table entries as `required`, `deprecated`, or `removal-target`.

### Artifacts
1. `docs/addresses/kenshi_1_0_65_x64.md` (initial SSOT table).
2. Contributor guidance in README/docs.
3. Optional guard script for address literal scanning outside allowed paths.

### Validation
1. New addresses only accepted when documented in SSOT table.
2. Observer migration reduces table surface over time.
3. Local guard catches drift when literals are added outside approved files.

## Phase Plan (Execution Order)
1. Phase 12: Deploy lock preflight and troubleshooting UX.
2. Phase 13: Export contract stability (canonical + alias strategy) and tests.
3. Phase 14: Observer API in hub ABI + helper integration (non-breaking).
4. Phase 15: Scaffold/SDK simplification and generated single-TU sample.
5. Phase 16: Unified reliability smoke matrix harness and automation wiring.
6. Phase 17: Address SSOT governance rollout and guardrails.

## Phase Exit Criteria (Quality Gate)
1. Phase 12 exit:
deploy preflight rejects locked target with actionable diagnostics and dedicated test coverage.
2. Phase 13 exit:
canonical + alias lookup behavior is documented, tested, and deprecation event is emitted.
3. Phase 14 exit:
observer callbacks are size-gated, lifecycle-safe, and validated with attach-retry scenarios.
4. Phase 15 exit:
scaffold output and single-TU sample compile using SDK public assets only.
5. Phase 16 exit:
reliability smoke matrix script passes all matrix cases in at least one automated local run.
6. Phase 17 exit:
address SSOT document exists with ownership/update policy and optional literal guard is enforced or explicitly deferred with rationale.

## Acceptance Criteria
1. Deploy scripts fail early with clear in-use/lock diagnosis before copy step.
2. Existing consumer behavior remains functional with no source changes required.
3. At least one stable observer path allows options-init retry without consumer RVA hooks.
4. Canonical symbol attach continues to pass all handshake tests.
5. Alias symbol attach succeeds when enabled and emits compatibility warning.
6. Scaffold output contains working hub wiring and deterministic local fallback by default.
7. SDK includes one authoritative helper runtime source only.
8. Single-TU sample is present, current, and build-validated.
9. Unified smoke test covers all matrix scenarios with deterministic results.
10. Address SSOT docs exist for supported game/version targets with clear ownership.
11. No new dead code/debug spam introduced by reliability changes.

## Risk Register
1. ABI growth risk:
Mitigation: strict size/version handshake guards and fallback behavior.
2. Observer callback lifetime risk:
Mitigation: explicit register/unregister ownership and shutdown cleanup.
3. Alias strategy confusion risk:
Mitigation: canonical-first lookup and one-release deprecation policy.
4. Scaffold expansion drift risk:
Mitigation: generate sample from templates and validate in automated local checks.
5. Transitional address drift risk:
Mitigation: SSOT table + literal guard + explicit version tags.

## Rollout Notes
1. Ship observer API as additive first; do not remove old path immediately.
2. Mark consumer RVA hooks as deprecated after observer path is validated in at least one real consumer.
3. Remove deprecated path only in a later cleanup phase with explicit migration notice.
4. Alias export (if introduced) is temporary and removed only after the published deprecation window closes.

## Traceability Mapping (Feedback -> Plan)
1. Deploy in-use failure -> Workstream A.
2. Per-mod options-init RVAs -> Workstream B + F.
3. Export rename attach failure -> Workstream C.
4. Official runtime source in SDK -> Workstream D (SSOT preserved).
5. New bridge scaffold -> Workstream D (extend existing scaffold, no duplicate script).
6. Bool wrapper boilerplate reduction -> Workstream D (small opt-in utilities only).
7. Attach/fallback/retry failure path coverage -> Workstream E.
8. SSOT address table by version/platform -> Workstream F.
9. Minimal single-TU consumer sample -> Workstream D.

## Explicit Deferrals (YAGNI Guard)
1. No generalized callback framework for bool/int/float ownership semantics in v1.1.
2. No automatic process termination in deploy scripts.
3. No hub API major-version redesign as part of this reliability cycle.
