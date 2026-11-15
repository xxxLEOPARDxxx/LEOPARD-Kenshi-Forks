# Private KEO audit: navmesh, pathfind, game - 2026-11-15

Scope: source snapshot identified by owner as a6fcd84. All 164 assigned files are byte-identical between D:/DEV/Kenshi/forks/LEOPARD-Kenshi-Forks/KEO and the game staging tree at the end of reading. 29,848 physical lines: navmesh 86 files / 17,666; pathfind 50 / 7,735; game 28 / 4,447. No game, live memory, process control, captures, profiling, builds or production edits were used for this audit. Existing game stutters and the reported main-menu slowdown are not attributed to any finding below.

Reading method: every non-generated file's complete code body/declarations was read, with comments retained where they define ownership/native/lifecycle contracts. Large files were split into bounded output ranges; a truncated klib-layout output was reread. The generated GOG byte table was mechanically read and structurally checked, not independently disassembled byte-by-byte. Per-file coverage follows below.

## Actionable correctness findings

### A1 - Required wall updates are discarded when pending slots are full (Important)

`src/navmesh/construction/wall_splice_drain.cpp:103-136`: DrainIntoPending removes up to 64 ring records before checking whether each can coalesce into, or occupy, one of 64 pending slots. If all pending slots are occupied and the next record cannot coalesce, line 135 increments pendingFull and loses the box. Pending slots can legitimately remain occupied while physics queues, loading/readiness or a transition prevent issuance (EvaluatePending, lines 251-297). This is required work: `wall_splice.cpp:151-189` NOPs the original progress call, and `wall_splice.cpp:72-94` substitutes the ring record. A counter does not recover that suppressed update.

Safe seam: a record must remain owned by the input until a pending destination or an explicitly bounded equivalent dirty-region representation is secured. Do not synchronously fall back to generating while hull changes are still pending. A free-slot-only drain prevents pendingFull loss but is insufficient by itself: upstream lossy Push can still overwrite a full ring. The complete fix must preserve the required update through both bounds.

Needed production tests: hold all 64 pending slots behind a real gate, supply a 65th non-coalescible region, unblock, verify eventual delivery of all regions or equivalent covering rebuild; verify coalescence, world reset/drop, no early native call, per-tick issue limit and bounded storage. Existing splice-ring accounting tests accept drained+lost equality, which proves accounting, not required delivery.

### A2 - TryPush capacity check and reservation race; accepted entries can also be abandoned (Important)

Subsequent correction status (not part of the original reading snapshot): fixed in staging by the authorized precise reservation/required-consumer change. Production fixtures reproduce the original race and accepted-publication loss; 11 ring controls at Od/O2 and 10 combined delivery mutants are recorded in `leopard/navmesh-delivery-fixes-20261115.md`. Ordinary lossy Push and A1 wall end-to-end delivery remain explicitly unresolved. The signed LONG counter/sentinel wrap limitation also remains unresolved.

`src/navmesh/construction/splice_ring.cpp:40-45` reads written/readPub and checks fullness, then ClaimAndFill independently increments written at line 19. With written=63, readPub=0, two producers can both see one free slot, then reserve 63 and 64. The second can replace unread slot 0, and both can return TAKEN. `splice_seam.cpp:185-189` treats TAKEN as consuming the native request and returns without calling the original. Header explicitly permits any thread; no single-producer precondition exists.

Separate issue: `splice_ring.cpp:91-103` abandons an unpublished index after two drain passes. A producer preempted between reserving its index and publishing may later return TAKEN after the drain has skipped that index. Capacity reservation alone cannot fix this case.

Safe seam: atomic bounded reservation for TryPush, publication state that distinguishes an active reservation from an explicit failed/cancelled one, and no timeout abandonment of required accepted work. Preserve the intentionally overwrite-oldest Push API's existing semantics separately until its functional wall consumer is migrated safely. Do not let a stalled producer cause CPU spinning; drain may return and retry next heartbeat.

Needed tests: deterministic barrier after fullness read with two producers and one free slot (one TAKEN, one FULL, no overwritten unread record); pause producer before publication over >2 drains, then resume and deliver once; claim failure/cancel hole; wrap/reuse, reset, and conservation including native fallback only for records not accepted.

### A3 - Required force-rebuild marks are cancelled at the 16-entry tracking bound / 60s TTL (Important, conditional stale-cache consequence)

Subsequent correction status (not part of the original reading snapshot): fixed in staging by retaining the existing bounded per-cell required mark until claim/completion/reset, independent of tracker eviction and diagnostic TTL. The 199-control exact-production lifecycle fixture at Od/O2 covers overflow, time, unloaded content, sequence-safe completion and world reset; evidence is in `leopard/navmesh-delivery-fixes-20261115.md`. UI hold timeout remains bounded and unchanged. Original source observations below are preserved for provenance.

`src/navmesh/cache/nm_force_rebuild.cpp:44,173-198`: AddLive drops its oldest tracker entry at LIVE_CAP=16 and changes a still-MARKED word to NONE (lines 184-190). `SweepLiveMarks:558-565` also expires an unclaimed mark after 60s. SpliceSeam may mark four cells per box and process four boxes per tick, so more than 16 distinct unclaimed cells is a reachable source-level workload; neither queue blockage nor a 60s backlog is an ownership cancellation.

`NmForceRebuildClaimLocked:77-96` consumes the mark only when the job is finally claimed. `ClaimedJobBeginLocked` carries forceCell into the pipeline; `NmJobMayReadCache` bypasses old cache entries only while forced. Thus a cancelled pending mark loses its mandatory regeneration property. If the native/cache key is unchanged by the edit, the job can use a pre-edit cached mesh. The audit does not claim every wall edit keeps hashZone unchanged; the defect is cancellation of a required request under a fixed tracker bound rather than explicit world reset/completion. BuildingHashWalk itself hashes position, rotation and stringID, not construction progress/physics state.

Safe seam: separate persistent per-cell invalidation from the small live reporting/hold tracker. The world is a fixed 64x64 grid, so required invalidation can remain bounded to 4096 cells and survive until consumed/completed or explicit world reset. A 60s panel/report timeout may still release UI; it must not restore permission to read stale data. Retain sequence checks so completion of an older generation cannot clear a newer invalidation. Do not widen an unbounded job queue.

Needed tests: >16 distinct marks before any claim, same-key old L1/L2 data, advance clock >60s, claim all and prove forced bypass; new mark during old claim and old completion; explicit world reset; gone cell; existing panel hold cap/cleanup; no repeated queueing merely because a persistent invalidation remains.

### A4 - Material-map allocation failure silently publishes incomplete fresh settings (Moderate, allocation-failure path)

`src/navmesh/generation/nm_fresh_wb.cpp:405-418`: when the source material-map hkArray is non-empty, failure of HavokTlsAlloc(mmCap*8) merely skips its copy and the function continues returning a fresh work buffer. This differs from the override-buffer failure at 470-491, which destroys the incomplete fresh buffer and returns NULL. Source material settings can therefore be omitted rather than using the existing real-buffer fallback. The key does not encode allocation failure.

Safe proposal: on required material-map copy allocation failure, FreeFreshSettings and return NULL, preserving CloneNMG/real-buffer fallback. Test by extracting the production construction path with the actual layout and failing only this allocation, then verify no generation/store uses an incomplete fresh buffer and ownership is balanced. No patch made in audit phase.

### A5 - Player re-request priority drops the entire set at 201-256 members (Moderate, modded-count condition)

`src/pathfind/player_repath_tier.cpp:14,67` sizes the published set to 256 but rejects the whole source list when count >200, publishing zero entries. Adjacent arrival/poll consumers explicitly accept up to 256 (`player_task_snap.cpp:414,818`), and tracked players are also 256. Within that supported local capacity, increasing 200 to 201 players disables player-owned re-request detection for everyone.

Safe proposal: use the declared PRT_MAX_CHARS for the admission bound, retain empty/null/oversize rejection, and fixture 0/200/201/256/257 with distinct pointers. This does not request expansion beyond the existing capacity and makes no FPS claim.

## Avoidable work / performance audit findings

1. Debug OFF gates printing, but much path timing remains active. `path_pool.cpp:79` returns only in the main-thread reporter; `path_pool_hooks.cpp:112-305` still performs QPC, queue histograms, atomics, request stamps and top-slow publication. `pathfind_hooks.cpp:637-678` unconditionally reads diagnostic face/goal fields, computes sqrtf, reads clocks, collects two sample consumers and writes outcome diagnostics. `astar_cost.cpp:189-190,409-410` uses an unbounded spin lock solely for optional slow samples. A safe split must keep NavMeshUpdateGuard, PathPriorityBeforeEnqueue, request-budget ownership/Take/Apply, the CGB_PLAYER owner label and AstarHierSearch functionality active independently. Use one admission snapshot per synchronous call, preserve nested TLS restoration, and handle an OFF->ON toggle without interpreting old request stamps. Production fixtures should prove same native call/args/result/budgets/priority with OFF and zero pure timing callbacks; this is not evidence of a specific measured FPS gain.

2. `nm_quality.cpp:154-249` ProbeWorkBufferSize probes up to 64KiB from a 544-byte known settings object and scans apparent hkArrays, under processJobCS on first dispatch even Debug OFF (`nm_dispatch.cpp:134-153`). `g_workBufAllocSize`, writable-array and probe results are read only by the reporter; they do not size allocation/cleanup. This is removable optional diagnostic work and unnecessary out-of-object memory probing. Keep CheckGenerationSettingsKey and keycode validation mandatory. Do not infer that this once-only probe explains the owner's 2x menu delay.

3. `nm_cache_core.cpp:132` invokes L2StartupScan synchronously during initialization. `nm_disk_cache.cpp:236-342` enumerates all temp/bin files, deletes obsolete/temp files, allocates records and sorts up to 8192 of them. Work is proportional to directory contents and occurs before ordinary cache operation. Safe later seam: do bounded deferred maintenance off the title-critical path with immutable path/config and filesystem-only ownership, retaining hard cap/accounting/failure guarantees. No source-level timing proves seconds of delay here; never add another game capture to justify this audit.

4. `navmesh_sched.cpp:295-325,359-392` classifies queued jobs twice while holding the native +152 mutex, each calling mover/preload walks; queue length is not capped. Spill growth occurs after releasing the mutex and countOut guards preserve the original queue on refusal, which is good. Consider bounded/reused classification scratch only after preserving pinned-head ordering, stable tier ordering, force-front requests and original-queue fallback. This is a complexity observation, not a measured stutter cause.

## Ownership, lifecycle and limits reviewed without a new proven defect

- Claims raise busy count and claimed-zone slots under native queue +152 before unlink visibility escapes; releases occur after L2 write/clone completion. Mod unload publishes under the same queue lock. Reset admission and post-processJobCS zone checks are explicit. Lock order is queue leaf; processJobCS before cache/build; observer's queue->done is bounded to 512 records and conservatively frees nothing on overflow.
- Workers require stop/build hooks, use stop-aware processJobCS waits, recheck after claim/clone/lock, and retirement waits for thread handles as well as the live counter before Havok teardown. Stop-aware clone cleanup and immutable seed-table teardown order were read. Windows worker handles/events/semaphore remain process-lifetime objects; this is a small bounded handle retention, not campaign-proportional growth.
- ClaimedJob explicitly has no unwind cleanup. WbSwapRestore and lock RAII protect their own resources, but native C++ exception recovery could leave claim/busy/clone/blob state retained if an outer native thread catches and continues. No such catch-and-continue contract was established here, so this remains a risk, not a claimed observed leak/crash.
- L1 is count-bounded to 256, not byte-budgeted. The permitted array maxima sum to 18,400,000 bytes per entry, or about 4.39GiB for 256 at all maxima, before transient reconstructed meshes/blob copies. Actual entries are not established to approach that limit. Seed records are fixed to at most one immutable allocation per grid cell but have no explicit aggregate byte cap; spill scratch retains a queue high-water mark. All deserve explicit budgets in a later design, not a claim of unlimited memory growth.
- L2 validates magic/version/settings/counts/length/CRC before deep-copy publication and unwinds partial arrays. Atomic temp->rename publication was read. Size accounting adds whole writes even when replacing existing files, causing conservative overestimation/rescans, not permission to exceed the byte cap; concurrent scans and >8192 files make the cap eventually enforced rather than a strict byte reservation. fclose failure after successful fflush is not checked; low-priority hardening, no claim of corrupt cache accepted past CRC.
- Background split deliberately allows one background generation beyond clone semaphore cap (`nm_misspar.cpp:405-409`); settings tooltip says total parallel generations. Treat as a documented internal concurrency policy / UI wording mismatch, not a new race or reason to silently reschedule generation.
- Missing neighbour seeds are explicitly allowed in per-session L1 but excluded from L2 when late; this is intentional code policy, not an unnoticed persistence defect. Shipped neighbour records remain immutable until stop. Native loading, mesh buffer ownership and build locks stay owner/native dependent; no whole-NPC worker proposal follows from this audit.
- Native addresses are bound to SDK symbols, SDK-derived layout assertions and Steam/GOG guards. GOG preparation hashes the exact EXE and SDK and translation is exact. No new ABI mismatch was established. This pass did not repeat native disassembly/SDK compilation for every existing address.
- Splice ring indices are signed LONG and use -1/-2 publication sentinels. Existing comparisons/loops do not establish safe behavior at signed-counter wrap; eventual collisions with those sentinels are also unresolved. Phase-2 atomic reservation is sound before that pre-existing counter boundary. A separate counter/sequence representation change and wrap fixtures would be needed; it is not folded into the bounded delivery fix.
- Generated GOG table: 393 sorted unique forward pairs, exact sorted inverse, 344 sorted byte-window keys, every window key mapped, all windows 32 bytes. File SHA256 fd7b58019c1b0b104152b3e1013ea9f7ea05c02b4356e9d4e02013f419e56e1c. Structural correctness is not independent proof that every byte matches its binary.

## Verification status and next phase

Audit phase used source reading, targeted reference searches, source/stage byte comparison and generated-table structural checks. No tests were executed or new gameplay measurements taken during this read-only phase. A1-A3 have concrete production control-flow/interleaving evidence and are the first correction candidates. Owner subsequently authorized stage-only delivery fixes and fail-first production fixtures; those results must be recorded separately and must not retroactively be described as audit-time validation. No public release or performance claim is implied.

## Per-file coverage

`body` means complete executable/declaration body reviewed; generated literal byte table is `structural` as qualified above. All paths below are relative to the source root stated at the top.

| File | Lines | Coverage |
|---|---:|---|
| src/navmesh/cache/nm_building_hash.cpp | 160 | body |
| src/navmesh/cache/nm_cache_core.cpp | 453 | body |
| src/navmesh/cache/nm_cache_core.h | 623 | body |
| src/navmesh/cache/nm_cache_key.h | 37 | body |
| src/navmesh/cache/nm_cache_report.cpp | 812 | body |
| src/navmesh/cache/nm_cache_slot_policy.cpp | 47 | body |
| src/navmesh/cache/nm_cache_slot_policy.h | 30 | body |
| src/navmesh/cache/nm_cache_type_policy.cpp | 11 | body |
| src/navmesh/cache/nm_cache_type_policy.h | 16 | body |
| src/navmesh/cache/nm_cache_types.h | 86 | body |
| src/navmesh/cache/nm_disk_cache.cpp | 691 | body |
| src/navmesh/cache/nm_disk_cache.h | 77 | body |
| src/navmesh/cache/nm_force_rebuild.cpp | 703 | body |
| src/navmesh/cache/nm_force_rebuild.h | 70 | body |
| src/navmesh/cache/nm_force_rebuild_policy.cpp | 153 | body |
| src/navmesh/cache/nm_force_rebuild_policy.h | 96 | body |
| src/navmesh/cache/nm_key_hash.h | 75 | body |
| src/navmesh/cache/nm_l2_writer.cpp | 192 | body |
| src/navmesh/cache/nm_l2_writer.h | 74 | body |
| src/navmesh/construction/splice_queue_policy.cpp | 94 | body |
| src/navmesh/construction/splice_queue_policy.h | 55 | body |
| src/navmesh/construction/splice_ring.cpp | 109 | body |
| src/navmesh/construction/splice_ring.h | 52 | body |
| src/navmesh/construction/splice_seam.cpp | 389 | body |
| src/navmesh/construction/splice_seam.h | 15 | body |
| src/navmesh/construction/splice_seam_policy.cpp | 83 | body |
| src/navmesh/construction/splice_seam_policy.h | 54 | body |
| src/navmesh/construction/wall_splice.cpp | 190 | body |
| src/navmesh/construction/wall_splice.h | 17 | body |
| src/navmesh/construction/wall_splice_drain.cpp | 368 | body |
| src/navmesh/construction/wall_splice_internal.h | 52 | body |
| src/navmesh/generation/misspar_math.cpp | 129 | body |
| src/navmesh/generation/misspar_math.h | 71 | body |
| src/navmesh/generation/nbr_seed_bindings.h | 121 | body |
| src/navmesh/generation/nm_clone.cpp | 192 | body |
| src/navmesh/generation/nm_fresh_wb.cpp | 636 | body |
| src/navmesh/generation/nm_misspar.cpp | 644 | body |
| src/navmesh/generation/nm_misspar.h | 94 | body |
| src/navmesh/generation/nm_nbr_seeds.cpp | 784 | body |
| src/navmesh/generation/nm_quality.cpp | 251 | body |
| src/navmesh/generation/nm_quality.h | 239 | body |
| src/navmesh/jobs/buildlock_audit.cpp | 35 | body |
| src/navmesh/jobs/buildlock_audit.h | 13 | body |
| src/navmesh/jobs/nm_buildlock.cpp | 543 | body |
| src/navmesh/jobs/nm_buildlock.h | 34 | body |
| src/navmesh/jobs/nm_buildlock_stall.cpp | 40 | body |
| src/navmesh/jobs/nm_buildlock_stall.h | 89 | body |
| src/navmesh/jobs/nm_busy_bridge_policy.cpp | 37 | body |
| src/navmesh/jobs/nm_busy_bridge_policy.h | 40 | body |
| src/navmesh/jobs/nm_claimed_job.cpp | 95 | body |
| src/navmesh/jobs/nm_claimed_job.h | 53 | body |
| src/navmesh/jobs/nm_claimed_job_policy.cpp | 90 | body |
| src/navmesh/jobs/nm_claimed_job_policy.h | 66 | body |
| src/navmesh/jobs/nm_dispatch.cpp | 497 | body |
| src/navmesh/jobs/nm_job_claim.cpp | 442 | body |
| src/navmesh/jobs/nm_job_pipeline.cpp | 1117 | body |
| src/navmesh/jobs/nm_process_job_lock.cpp | 315 | body |
| src/navmesh/jobs/nm_queue_lock.h | 58 | body |
| src/navmesh/jobs/nm_unload.cpp | 313 | body |
| src/navmesh/jobs/nm_unload_fence_policy.cpp | 58 | body |
| src/navmesh/jobs/nm_unload_fence_policy.h | 58 | body |
| src/navmesh/navmesh_config.cpp | 133 | body |
| src/navmesh/navmesh_config.h | 135 | body |
| src/navmesh/nm_lazy_hooks.cpp | 237 | body |
| src/navmesh/nm_workers.h | 256 | body |
| src/navmesh/nm_workers_internal.h | 255 | body |
| src/navmesh/scheduling/navmesh_sched.cpp | 491 | body |
| src/navmesh/scheduling/navmesh_sched.h | 55 | body |
| src/navmesh/scheduling/nm_adjacency.cpp | 242 | body |
| src/navmesh/scheduling/nm_adjacency.h | 87 | body |
| src/navmesh/scheduling/nm_adjacency_check.cpp | 142 | body |
| src/navmesh/scheduling/nm_adjacency_counters.h | 39 | body |
| src/navmesh/scheduling/nm_adjacency_internal.h | 56 | body |
| src/navmesh/scheduling/nm_adjacency_observer.cpp | 105 | body |
| src/navmesh/scheduling/nm_adjacency_policy.cpp | 640 | body |
| src/navmesh/scheduling/nm_adjacency_policy.h | 292 | body |
| src/navmesh/scheduling/nm_adjacency_protocol.cpp | 236 | body |
| src/navmesh/scheduling/nm_adjacency_report.cpp | 92 | body |
| src/navmesh/scheduling/nm_inflight.cpp | 83 | body |
| src/navmesh/scheduling/nm_inflight.h | 30 | body |
| src/navmesh/workers/nm_retire_ops.cpp | 206 | body |
| src/navmesh/workers/nm_retire_policy.cpp | 162 | body |
| src/navmesh/workers/nm_retire_policy.h | 95 | body |
| src/navmesh/workers/nm_worker_gate_policy.cpp | 23 | body |
| src/navmesh/workers/nm_worker_gate_policy.h | 28 | body |
| src/navmesh/workers/nm_worker_pool.cpp | 508 | body |
| src/pathfind/astar_cost.cpp | 492 | body |
| src/pathfind/astar_cost.h | 77 | body |
| src/pathfind/astar_cost_policy.cpp | 42 | body |
| src/pathfind/astar_cost_policy.h | 59 | body |
| src/pathfind/astar_hier.cpp | 386 | body |
| src/pathfind/astar_hier.h | 17 | body |
| src/pathfind/astar_hier_policy.cpp | 273 | body |
| src/pathfind/astar_hier_policy.h | 99 | body |
| src/pathfind/gate_loop.cpp | 12 | body |
| src/pathfind/gate_loop.h | 9 | body |
| src/pathfind/gate_pass.cpp | 257 | body |
| src/pathfind/gate_pass.h | 24 | body |
| src/pathfind/npc_cap_requester.cpp | 252 | body |
| src/pathfind/npc_cap_requester.h | 22 | body |
| src/pathfind/npc_search_probe_policy.cpp | 86 | body |
| src/pathfind/npc_search_probe_policy.h | 29 | body |
| src/pathfind/npc_wait_diag.cpp | 368 | body |
| src/pathfind/order_hook.cpp | 309 | body |
| src/pathfind/order_hook.h | 10 | body |
| src/pathfind/path_budget_policy.cpp | 40 | body |
| src/pathfind/path_budget_policy.h | 34 | body |
| src/pathfind/path_guard_hooks.cpp | 98 | body |
| src/pathfind/path_pool.cpp | 117 | body |
| src/pathfind/path_pool.h | 84 | body |
| src/pathfind/path_pool_bindings.h | 67 | body |
| src/pathfind/path_pool_hist.cpp | 261 | body |
| src/pathfind/path_pool_hooks.cpp | 305 | body |
| src/pathfind/path_pool_internal.h | 177 | body |
| src/pathfind/path_pool_report.cpp | 340 | body |
| src/pathfind/path_priority_hooks.h | 8 | body |
| src/pathfind/path_priority_publication.cpp | 78 | body |
| src/pathfind/path_priority_publication.h | 28 | body |
| src/pathfind/path_probe.cpp | 83 | body |
| src/pathfind/path_result_trace.cpp | 55 | body |
| src/pathfind/path_result_trace.h | 29 | body |
| src/pathfind/pathfind_cache.cpp | 52 | body |
| src/pathfind/pathfind_cache.h | 35 | body |
| src/pathfind/pathfind_config.cpp | 127 | body |
| src/pathfind/pathfind_config.h | 99 | body |
| src/pathfind/pathfind_diag.cpp | 18 | body |
| src/pathfind/pathfind_diag.h | 269 | body |
| src/pathfind/pathfind_diag_report.cpp | 455 | body |
| src/pathfind/pathfind_hooks.cpp | 804 | body |
| src/pathfind/pathfinding.h | 48 | body |
| src/pathfind/player_repath_tier.cpp | 145 | body |
| src/pathfind/player_repath_tier.h | 48 | body |
| src/pathfind/player_repath_tier_policy.cpp | 24 | body |
| src/pathfind/player_repath_tier_policy.h | 30 | body |
| src/pathfind/player_task_policy.h | 117 | body |
| src/pathfind/player_task_snap.cpp | 837 | body |
| src/game/bindings.h | 321 | body |
| src/game/game.cpp | 268 | body |
| src/game/game.h | 51 | body |
| src/game/gog_jobs_profile.cpp | 37 | body |
| src/game/gog_jobs_profile.h | 15 | body |
| src/game/gog_profile.cpp | 44 | body |
| src/game/gog_profile.h | 25 | body |
| src/game/gog_profile_data.inc | 1137 | structural |
| src/game/hand_key.cpp | 46 | body |
| src/game/hand_key.h | 23 | body |
| src/game/klib_address_policy.h | 34 | body |
| src/game/klib_bindings.cpp | 377 | body |
| src/game/klib_bindings.h | 46 | body |
| src/game/klib_dethunk_policy.h | 74 | body |
| src/game/klib_globals.cpp | 44 | body |
| src/game/klib_layout.cpp | 413 | body |
| src/game/klib_layout_combat.cpp | 15 | body |
| src/game/klib_layout_ogre.cpp | 60 | body |
| src/game/klib_member_combat.inc | 3 | body |
| src/game/klib_member_contract.h | 38 | body |
| src/game/klib_member_fields.inc | 353 | body |
| src/game/klib_members.h | 34 | body |
| src/game/klib_platform_policy.h | 14 | body |
| src/game/native_profile.cpp | 66 | body |
| src/game/native_profile.h | 12 | body |
| src/game/offsets.h | 296 | body |
| src/game/prologue_policy.h | 55 | body |
| src/game/rva.h | 546 | body |
