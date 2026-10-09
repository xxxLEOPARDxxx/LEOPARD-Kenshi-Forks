# Mod Hub Wrapper Decision (2026-03-27)

## Decision
Do not start the thin declare-style wrapper work yet.

Phase 24 is satisfied by a defer decision, not a prototype. The current
`scaffold + shared helper` path is now the recommended integration path and is
already short enough for new consumers without introducing a second runtime
mental model.

## Evidence
1. The scaffold now covers the common row kinds directly and routes updates
   through shared validation plus apply/persist/rollback helpers.
   Source slice: `scripts/init-mod-template.ps1`,
   `scripts/templates/mod_hub_consumer_adapter.cpp.template`,
   `scripts/templates/mod_hub_consumer_single_tu.cpp.template`,
   `include/emc/mod_hub_consumer_helpers.h`.
2. Quick Start and packaged SDK assets now lead consumers to the adapter-first
   path and make the local `PersistExampleModState(...)` seam explicit instead
   of hiding persistence behind another layer.
   Source slice: `docs/mod-hub-sdk-quickstart.md`, `docs/mod-hub-sdk.md`,
   `scripts/package-sdk.ps1`.
3. A real hand-written consumer can adopt the new helper surface without losing
   clarity. Auto Pause moved its live adapter to a local `PersistHubConfig(...)`
   seam plus shared `ValidateBoolValue`, `ValidateValueInRange`, and
   `ApplyUpdateWithRollback` calls while keeping mod-specific side effects
   local.
   Evidence repo: `/mnt/i/Kenshi_modding/mods/Auto-Pause-on-Load/AutoPauseModHub.inl`.
4. The remaining legacy consumers still own persistence and side effects in
   ways that a thin `DeclareBool/DeclareInt/...` wrapper would not remove.
   `Wall-B-Gone` still carries per-setting side effects around hotkey syncing
   and UI refresh, `Organize-the-Trader` still normalizes and reapplies full
   config snapshots, and `Vital-Read` already reduced its bool-row registration
   churn with a descriptor loop.

## Why Defer
1. The wrapper would create a second authoring story while the current story is
   finally coherent: scaffold the adapter, keep one local persistence seam, and
   use shared helpers for validation and rollback.
2. The remaining friction is concentrated in a few legacy consumers that can
   adopt the helper surface incrementally without a new abstraction layer.
3. A thin wrapper still would not own persistence, normalization, or runtime
   side effects, so the most important boundaries would remain consumer-local
   anyway.
4. The project would take on more docs, samples, and maintenance burden before
   proving that the wrapper removes meaningful real-world code.

## Revisit Conditions
Re-open wrapper work only if all of the following become true:
1. At least two active consumers still have repeated hand-written adapter churn
   after vendored SDK sync plus helper adoption.
2. That remaining churn is primarily descriptor/callback ceremony rather than
   consumer-owned persistence or side effects.
3. A concrete wrapper sketch can stay as transparent as the current adapter
   files when debugging registration and commit failures.

## Current Recommendation
1. Keep the adapter scaffold as the primary path.
2. Continue real-consumer helper adoption where it removes repeated transaction
   code.
3. Prefer vendored SDK sync plus incremental helper rollout in existing mods
   before inventing another public integration layer.
