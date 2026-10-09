# Mod Hub SDK Feedback Findings (2026-03-06)

## Summary
This document captures the feedback-analysis assessment of eight proposed SDK/workflow changes and the prioritized implementation path.

## Point-by-point Assessment

| # | Point | Verdict | Principles | Risk | Reason |
|---|---|---|---|---|---|
| 1 | Submodule-based SDK (`tools/mod-hub-sdk`) instead of copying headers/helper code per mod | partial | DRY, SSOT, KISS | medium | Prevents drift and duplication, but submodule workflows need strict pinning and a simple operator flow. |
| 2 | Per-mod `sync-mod-hub-sdk.ps1` (pull + validate + changelog note) | partial | SRP, KISS, DRY | medium | Sync and validation are good; direct changelog mutation mixes concerns and can create noisy diffs. |
| 3 | Generate bridge code from `hub-settings.json` for bool/int/action + rollback/save boilerplate | fail | DRY, YAGNI, overengineering, SRP | high | Full generator adds significant maintenance/debug complexity before clear scale pressure justifies it. |
| 4 | Shared helper header for common callback patterns (bool/int get/set + save/rollback) | pass | DRY, SRP, KISS | low | Low-risk extraction that removes repetitive callback boilerplate and keeps consumer adapters clearer. |
| 5 | One smoke test script per consumer for attach/observer retry/fallback | partial | DRY, KISS, SRP | medium | Coverage is good, but one script per consumer can drift; shared harness with parameters is safer. |
| 6 | SDK version stamp check at startup with clear warning on drift | pass | SSOT, KISS, YAGNI | low | Cheap runtime guardrail with immediate operator value when contract drift occurs. |
| 7 | One short SSOT doc in Mod Core + auto-generated consumer snippets | partial | SSOT, KISS, overengineering | medium | SSOT doc is strong; full snippet generation should be added only if docs drift is recurring. |
| 8 | `init-mod-template --with-hub --from-manifest` as default for new mods | partial | KISS, YAGNI | medium | Helps standardization, but forcing manifest flow by default is premature for simple consumers. |

## Rewrites (Only Points Needing Changes)

### Point 1
Original: Make SDK consumption submodule-based per mod.

Rewrite: Use one pinned SDK submodule (`tools/mod-hub-sdk`) plus one shared sync+validate flow reused by consumers.

Why this is better: Keeps DRY/SSOT gains while minimizing per-mod workflow divergence.

### Point 2
Original: Per-mod sync command should pull, validate, and update changelog note.

Rewrite: Sync command should only pull+validate and print a release-note suggestion; changelog edits remain manual.

Why this is better: Preserves SRP and avoids accidental documentation churn in routine sync operations.

### Point 3
Original: Generate bool/int/action bridge code from manifest.

Rewrite: Start with shared callback helper extraction first; gate full codegen behind adoption thresholds (multiple consumers + repeated pain).

Why this is better: Delivers immediate simplification with lower tooling and maintenance burden.

### Point 5
Original: One smoke script per consumer.

Rewrite: One parameterized smoke harness (`phase_mod_hub_consumer_smoke.ps1`) with optional thin per-consumer wrappers.

Why this is better: Prevents test drift while preserving consumer-specific entry points.

### Point 7
Original: SSOT doc plus auto-generated snippets.

Rewrite: Publish SSOT doc now; add snippet generation only for high-churn sections after drift is observed.

Why this is better: Prioritizes clarity immediately without upfront doc-generation complexity.

### Point 8
Original: Make `--from-manifest` the default scaffold path.

Rewrite: Keep `--with-hub` as default; keep manifest scaffold as opt-in until pattern stability is proven.

Why this is better: Reduces onboarding friction for simple mods.

## Priority Action Plan

1. Immediate fixes
   - Implement shared callback helper extraction (point 4).
   - Add startup SDK stamp drift warning (point 6).
   - Keep sync command scope to pull+validate only; no automatic changelog edits (point 2 rewrite).
2. Next iteration
   - Submodule + pinned revision adoption path (point 1).
   - Parameterized consumer smoke harness model (point 5 rewrite).
3. Optional refinements
   - Manifest-first codegen only after threshold signals (point 3 rewrite).
   - Template-driven snippet generation for selected high-churn docs only (point 7 rewrite).
