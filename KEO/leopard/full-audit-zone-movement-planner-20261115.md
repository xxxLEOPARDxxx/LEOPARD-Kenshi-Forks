# Private source audit: zone, movement, planner (2026-11-15)

Status: full assigned CPP/H read complete. Source-only findings; no game/process access, collectors, full DLL builds, installation or source-checkout edits. The separately authorized phase-2 fixes below were changed only in the private stage and checked by focused standalone fixtures/translation-unit compilation. Previously installed tracking/purge fixes are excluded from new findings. This is not evidence of measured FPS improvement or absence of every leak.

## Confirmed actionable findings

1. **Important, fixed in stage: planner replan starvation.** `src/planner/planner_tick.cpp` previously restarted its64-slot traversal at0 with4 total attempts. Replan failure still consumed an attempt and left the plan due. Four permanently failing low slots could indefinitely prevent a later good slot from replanning. The rotated traversal retains all64 validations, four attempts, cadence and reset rules. See `planner-replan-fairness-fix-20261115.md`;216 assertions/Od+/O2 and8 compiled mutants, actual DEV/PROD TU compilation. Awaiting independent review.

2. **Important, fixed in stage: stale formation gather speed after invalid roster.** `src/movement/formation.cpp:821` returned before the pace frame was cleared/published on missing singleton, missing list, empty list or count>200. The AI speed hook has no expiry, so a stale factor could persist. Those refusals now publish only empty pace output, preserving group state and avoiding invalid character reads. See `formation-pace-roster-fix-20261115.md`;23 controls/Od+/O2 and7 compiled mutants, actual DEV/PROD TU compilation. Awaiting independent review.

3. **Confirmed wasted recurring work, fixed in stage: dormant island overlay rebuild.** Old private static `g_isMod` was zeroed on every rebuild and had no producer/address escape. Despite that, each eligibility/signature rebuild read native Set B zones and performed full4096-cell union-find/component passes to produce zero components. `src/movement/island_components.cpp:252` now publishes the identical valid empty snapshot after startup/reset and only advances eligibility on subsequent rebuilds. Native WalkSetB/signature/router/far-span/edge-ring behavior is preserved. See `island-empty-rebuild-fix-20261115.md`.

4. **Confirmed wasted idle frame scan, fixed in stage:** `src/zone/handoff/zone_handoff.cpp:528` called CollectReady over4096 entries on every quiet/idle frame even when no cell was ready. Main owns the ledger and maintains its READY occupancy synchronously in Init/Begin/SetState/Release/ClearAll. An existing zero count now returns before collecting. Nonempty collection/admission/order/cap32 unchanged. See `zone-idle-readiness-fix-20261115.md`.

5. **Confirmed diagnostic work during normal play, fixed in stage:** `src/zone/readiness/readiness_hook.cpp:489` performed diagnostic classification/counting with Debug OFF even though step>=2 always preserves native readiness. A native-false main call could take world/map try-locks, scan up to8192 collection instances, read QPC and update atomics. The step>=2 fast path now returns the original's result immediately after the unchanged queue-priority/original call when Debug OFF. Functional ClassifyZoneReadiness callers in K7/island paths, and the below-step2 legacy arm, remain unchanged. See `zone-idle-readiness-fix-20261115.md`.

## Confirmed retention/dependency risks left unchanged

- `src/planner/coarse_graph.cpp:269` increments the world generation without clearing old `dir.over` blocks. `CgRead` rejects old generations, so those heap blocks remain allocated until replaced or test-only destruction. The insert-only4096 interior UID table also persists across worlds. Bounded by the directory, not an unbounded leak; no observed heap size/FPS claim. `SwapOver` around427 can accept a stale incoming overlay if the current overlay is absent/stale. A bounded epoch-protected overlay-only sweep plus stale incoming publication refusal is a candidate, but base/index/worker lifetime proof is required. Interior UID reuse requires separate probe-chain and stable-base proof. No cleanup or identity change implemented here.
- `src/zone/camera_zone_hook.cpp:397,616,638` places PollFormationGroups after preload/camera and native-idle gates. `formation_pace.cpp` reads its last publication until replaced or ClearFormationGroups. This is a confirmed feature/lifecycle dependency, separate from the now-fixed invalid roster path. Moving the native formation poll requires verifying order/loader reentrancy and the intentional preload A/B coupling in island reissue; no speculative caller move implemented. A safe disabled-feature contract could revoke published factors while preserving group state; native scatter/gather scheduling still needs a separately proven caller contract.

## Bounds and lifecycle reviewed

Planner: fixed64 plan slots, bounded search/capture/result tables, world generations, epoch/retirement and builder/cache publication, ownership and cancellation. Main replan fairness bug fixed; stale heap overlays documented above. Fixed world-setup scans (biome/base graph build, section locate fallback) are not evidence that campaign FPS decay is caused by them.

Movement: all formation/gather/follow/pace, tracker, K7, cancellation/reissue, island snapshots, edge-ring/span and diagnostic paths. Working tables and diagnostics are bounded. Save-load paths clear raw character/group/order state. Native exceptions in existing cancellation depth bookkeeping are a conditional robustness concern, not an observed failure or performance root-cause claim; no unrelated change made.

Zone: camera/frame ordering, queue budgets45/12, process/register/handoff/native ownership, readiness contracts/locks, unload/expiry fences and reset admission/drain ordering, first-time/town/population handoff, fixed4096-cell ledgers, retention/grace/pressure/deferral backoff, save/load world clear and bounded diagnostics. No new confirmed ownership/freeing defect was established. World reset scans and native zone costs are expected lifecycle work; this audit does not certify arbitrary engine pointers or unknown binary patches.

## Exact per-file full-read coverage

Counts below refer to the files actually read, not a grep/inventory-only claim. All were read end-to-end, including headers; large files were split into nontruncated sections. Parent/other-agent concurrent changes after that read are outside this audit snapshot. The latest staged fairness/pace/island/handoff/readiness changes above were additionally read/verified.

### planner: 45 CPP/H

- `src/planner/coarse_graph.cpp`
- `src/planner/coarse_graph.h`
- `src/planner/coarse_graph_base.cpp`
- `src/planner/coarse_graph_base.h`
- `src/planner/coarse_graph_cache.cpp`
- `src/planner/coarse_graph_cache.h`
- `src/planner/coarse_graph_cross.cpp`
- `src/planner/coarse_graph_live.cpp`
- `src/planner/coarse_graph_live.h`
- `src/planner/coarse_graph_live_site.cpp`
- `src/planner/coarse_search.cpp`
- `src/planner/coarse_search.h`
- `src/planner/plan_build.cpp`
- `src/planner/plan_build.h`
- `src/planner/plan_merge.cpp`
- `src/planner/plan_merge.h`
- `src/planner/plan_policy.cpp`
- `src/planner/plan_policy.h`
- `src/planner/plan_store.cpp`
- `src/planner/plan_store.h`
- `src/planner/planner_acid.cpp`
- `src/planner/planner_acid.h`
- `src/planner/planner_capture.cpp`
- `src/planner/planner_config.cpp`
- `src/planner/planner_config.h`
- `src/planner/planner_hooks.cpp`
- `src/planner/planner_hooks.h`
- `src/planner/planner_merge.cpp`
- `src/planner/planner_merge.h`
- `src/planner/planner_prearrival.cpp`
- `src/planner/planner_prearrival.h`
- `src/planner/planner_report.cpp`
- `src/planner/planner_search_ops.cpp`
- `src/planner/planner_search_ops.h`
- `src/planner/planner_tick.cpp`
- `src/planner/planner_tick.h`
- `src/planner/planner_tick_internal.h`
- `src/planner/planner_water.cpp`
- `src/planner/planner_water.h`
- `src/planner/planner_water_table.cpp`
- `src/planner/planner_water_table.h`
- `src/planner/tagfile_reader.cpp`
- `src/planner/tagfile_reader.h`
- `src/planner/tile_graph_extract.cpp`
- `src/planner/tile_graph_extract.h`

### movement: 62 CPP/H

- `src/movement/char_movement_fields.h`
- `src/movement/formation.cpp`
- `src/movement/formation.h`
- `src/movement/formation_follow.cpp`
- `src/movement/formation_follow.h`
- `src/movement/formation_follow_policy.cpp`
- `src/movement/formation_follow_policy.h`
- `src/movement/formation_gather_policy.cpp`
- `src/movement/formation_gather_policy.h`
- `src/movement/formation_internal.h`
- `src/movement/formation_members.cpp`
- `src/movement/formation_members.h`
- `src/movement/formation_pace.cpp`
- `src/movement/formation_pace.h`
- `src/movement/formation_pace_policy.cpp`
- `src/movement/formation_pace_policy.h`
- `src/movement/formation_query.cpp`
- `src/movement/island_cancel_hooks.cpp`
- `src/movement/island_components.cpp`
- `src/movement/island_edge_legs.cpp`
- `src/movement/island_edge_legs.h`
- `src/movement/island_edge_ring.cpp`
- `src/movement/island_edge_ring.h`
- `src/movement/island_edge_ring_policy.cpp`
- `src/movement/island_edge_ring_policy.h`
- `src/movement/island_hooks.cpp`
- `src/movement/island_orders.cpp`
- `src/movement/island_overlay_internal.h`
- `src/movement/island_reissue.cpp`
- `src/movement/island_span_policy.cpp`
- `src/movement/island_span_policy.h`
- `src/movement/island_stuck.cpp`
- `src/movement/islands.cpp`
- `src/movement/islands.h`
- `src/movement/islands_inline.h`
- `src/movement/islands_internal.h`
- `src/movement/islands_reissue.cpp`
- `src/movement/islands_reissue_counters.h`
- `src/movement/islands_reissue_internal.h`
- `src/movement/k7_arrival_policy.cpp`
- `src/movement/k7_arrival_policy.h`
- `src/movement/k7_observe.cpp`
- `src/movement/k7_reissue.cpp`
- `src/movement/k7_swap_policy.cpp`
- `src/movement/k7_swap_policy.h`
- `src/movement/movement_config.cpp`
- `src/movement/movement_config.h`
- `src/movement/movement_trace.cpp`
- `src/movement/movement_trace.h`
- `src/movement/movement_trace_policy.cpp`
- `src/movement/movement_trace_policy.h`
- `src/movement/mover_policy.cpp`
- `src/movement/mover_policy.h`
- `src/movement/order_outcome.cpp`
- `src/movement/order_outcome.h`
- `src/movement/order_outcome_policy.cpp`
- `src/movement/order_outcome_policy.h`
- `src/movement/order_outcome_table.cpp`
- `src/movement/order_outcome_table.h`
- `src/movement/scatter_patch.cpp`
- `src/movement/tracking.cpp`
- `src/movement/tracking.h`

### zone: 92 CPP/H

- `src/zone/camera_zone_hook.cpp`
- `src/zone/camera_zone_hook.h`
- `src/zone/geometry/zone_geometry_cert.cpp`
- `src/zone/geometry/zone_geometry_cert.h`
- `src/zone/geometry/zone_geometry_epoch.cpp`
- `src/zone/geometry/zone_geometry_epoch.h`
- `src/zone/geometry/zone_nav_ledger.cpp`
- `src/zone/geometry/zone_nav_ledger.h`
- `src/zone/geometry/zone_nav_recovery.cpp`
- `src/zone/geometry/zone_nav_recovery.h`
- `src/zone/grid.cpp`
- `src/zone/grid.h`
- `src/zone/handoff/first_time_bindings.h`
- `src/zone/handoff/zone_adoption_latency.cpp`
- `src/zone/handoff/zone_adoption_latency.h`
- `src/zone/handoff/zone_adoption_seam.cpp`
- `src/zone/handoff/zone_adoption_seam.h`
- `src/zone/handoff/zone_first_time.cpp`
- `src/zone/handoff/zone_handoff.cpp`
- `src/zone/handoff/zone_handoff.h`
- `src/zone/handoff/zone_lifecycle_diag.h`
- `src/zone/handoff/zone_lifecycle_hooks.cpp`
- `src/zone/handoff/zone_lifecycle_hooks.h`
- `src/zone/handoff/zone_prep_ledger.cpp`
- `src/zone/handoff/zone_prep_ledger.h`
- `src/zone/handoff/zone_registry.cpp`
- `src/zone/hooks_internal.h`
- `src/zone/preload/camera_focus.cpp`
- `src/zone/preload/camera_focus.h`
- `src/zone/preload/coverage_stats.cpp`
- `src/zone/preload/coverage_stats.h`
- `src/zone/preload/preload.cpp`
- `src/zone/preload/preload.h`
- `src/zone/preload/preload_internal.h`
- `src/zone/preload/preload_pipe_stats.cpp`
- `src/zone/preload/preload_pipe_stats.h`
- `src/zone/preload/preload_prepare.cpp`
- `src/zone/preload/preload_prepare.h`
- `src/zone/preload/preload_queue.cpp`
- `src/zone/preload/preload_queue.h`
- `src/zone/preload/zone_cycle_math.cpp`
- `src/zone/preload/zone_cycle_math.h`
- `src/zone/preload/zone_cycle_stats.cpp`
- `src/zone/preload/zone_cycle_stats.h`
- `src/zone/preload_bindings.h`
- `src/zone/readiness/readiness_bindings.h`
- `src/zone/readiness/readiness_bypass_queue.h`
- `src/zone/readiness/readiness_hook.cpp`
- `src/zone/readiness/readiness_hook.h`
- `src/zone/readiness/zone_readiness_classify.h`
- `src/zone/readiness/zone_readiness_contract.cpp`
- `src/zone/readiness/zone_readiness_contract.h`
- `src/zone/readiness/zone_readiness_ledger_bridge.cpp`
- `src/zone/readiness/zone_readiness_ledger_bridge.h`
- `src/zone/reset/preload_saveload.cpp`
- `src/zone/reset/preload_saveload.h`
- `src/zone/reset/save_load_bindings.h`
- `src/zone/reset/zone_reset_fence.cpp`
- `src/zone/reset/zone_reset_fence.h`
- `src/zone/reset/zone_reset_gate.cpp`
- `src/zone/reset/zone_reset_gate.h`
- `src/zone/reset/zone_reset_sequence.cpp`
- `src/zone/reset/zone_reset_sequence.h`
- `src/zone/retention/zone_evict.cpp`
- `src/zone/retention/zone_expiry_guard.cpp`
- `src/zone/retention/zone_expiry_guard.h`
- `src/zone/retention/zone_expiry_guard_policy.cpp`
- `src/zone/retention/zone_expiry_guard_policy.h`
- `src/zone/retention/zone_leak_report.cpp`
- `src/zone/retention/zone_retention.cpp`
- `src/zone/retention/zone_retention.h`
- `src/zone/retention/zone_retention_ledger.cpp`
- `src/zone/retention/zone_retention_ledger.h`
- `src/zone/retention/zone_retention_policy.cpp`
- `src/zone/retention/zone_retention_policy.h`
- `src/zone/retention/zone_save_rule.cpp`
- `src/zone/retention/zone_save_rule.h`
- `src/zone/retention/zone_unload.cpp`
- `src/zone/transition.h`
- `src/zone/transition_hook.cpp`
- `src/zone/transition_hook.h`
- `src/zone/zone_config.cpp`
- `src/zone/zone_config.h`
- `src/zone/zone_helpers.h`
- `src/zone/zone_ledger.h`
- `src/zone/zone_ledger_core.cpp`
- `src/zone/zone_ledger_core.h`
- `src/zone/zone_life.cpp`
- `src/zone/zone_life.h`
- `src/zone/zone_life_internal.h`
- `src/zone/zone_pause.cpp`
- `src/zone/zone_pause.h`
