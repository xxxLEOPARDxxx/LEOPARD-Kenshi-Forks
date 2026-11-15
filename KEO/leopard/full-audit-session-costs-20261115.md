# Startup and session costs: source follow-up, 2026-11-15

Baseline `1718df4`, previous installed PROD `1911b8dc894a10efa4c3d64c9a263a6d414434fe573f645d2226cb9d5660e65b`. This is a focused follow-up, not a second claim that all733 files were reread. Existing evidence and actual-production offline fixtures only; no game launch, new profile/capture, user reproduction request or collection. Personal settings and release2 are preserved; no publication.

## Implemented

### Startup: pure observers admitted only when requested

`src/plugin/hook_manifest.cpp`, `src/plugin/hook_manifest.h`, `src/plugin/hook_manifest_policy.cpp`, `src/plugin/hook_manifest_policy.h`, `src/pathfind/path_guard_hooks.cpp`, `src/zone/camera_zone_hook.cpp`.

The section add/remove observers are diagnostic only. With detailed logging off, startup now omits both native hook installations. Later enabling logging admits them once from the existing owner heartbeat; each has fresh native verification, no retained startup stack pointer, initialized native sampler inputs and independent failure handling. Native extraction rescue and functional adjacency remain installed. The add wrapper forwards immediately while logging is off. Wanted totals reflect the intentional omission. No new setting or world scan. Report: `section-diagnostic-startup-fix-20261115.md`.

Root independently reviewed the exact installer/owner/want/wrapper changes and reran the actual-source fixture at Od/O2 with all seven compiled negative controls. Cases include startup OFF/ON, premature/reentrant admission, partial refusal, repeated toggles, original argument/result/exception preservation. Four existing manifest variants and all127 host suites pass.

### Material teardown: indexed invalidation

`src/render/gpu_param_cache.cpp`, `src/render/gpu_param_cache.h`.

Deleting/rebinding an admitted material map previously scanned all4096 slots, including maps never cached. A fixed1024-bucket intrusive index now visits only the relevant bucket; empty cache removal reads zero slots. The original16-probe Find/Store placement and exact removal behavior remain. Fixed doubly linked slot indices keep Store replacement O(1); no heap, lock, retained native reference or per-frame scan. Additional fixed storage18440 bytes. Root independently reviewed both link directions, collision removal, replacement, Clear/count invariants and owner-thread use, then reran more than2.36 million actual-production/oracle checks at Od/O2 plus nine compiled negative controls. Native36+7 and38+6 regressions passed after final code; host render_units passed. Report: `session-gpu-cache-teardown-index-20261115.md`.

### Planner idle scans and old-world overlay retirement

`src/planner/coarse_graph.cpp`, `src/planner/coarse_graph.h`.

PlannerOnFrame calls promotion every frame. With no live work, it formerly still read all1024 record slots. The pending count is now reserved BEFORE publishing a record, then decremented only after a take or replacement. It includes in-flight reservations, so it may conservatively overcount but cannot hide a posted record. Deferred publication retries still happen before the empty fast return. The existing128-buffer pool bounds outstanding ownership; no new growing queue or native pointer is introduced.

A new world formerly only changed the generation, leaving old heap overlays allocated until their exact slots were replaced. It now arms a one-time owner sweep, at most32 directory entries per ordinary retirement call. Every visited entry, including an empty one, is taken through the existing epoch. A busy entry stops this slice and is retried next frame; the owner never waits/yields there. Stale overlays are linked to the existing retirement stack and freed at its existing outside-search free point. Bases/interior UID indices/current-world overlays are preserved. Old-generation incoming overlays are refused under the entry epoch even if no current overlay exists.

Independent source review approved pending-count arithmetic/lifetime and found a race in the initial outer empty-entry check: a writer could hold an empty entry after reading the old generation and publish behind the sweep. The recommended unconditional entry take was implemented. A deterministic production-source fixture now advances the world and attempts retirement AFTER the writer reads its generation; the reverted empty-entry mutant fails that assertion. The independent agent's final rerun was unavailable; root verified the correction and all compiled controls. No blanket final independent-approval claim is made for this last follow-up.

`test_coarse_session.cpp/.py` compiles the entire real store with array-access counters and two explicit interleaving probes replacing observation boundaries only. Fail-first old source scanned1024 empty slots. Corrected432 controls each at Od/O2 pass; eight compiled mutants fail at runtime. Coverage includes empty/repeated promotion,200 replacement cycles, producer reservation before publication, a take during replacement, old-world incoming refusal, current/base preservation,32-entry slices, busy-empty writer race and destroy/recreate. Existing coarse store/live suites pass; the real concurrent injection passed71716 reads with0 torn views and65885 promoted blocks in the saved run. These are offline correctness/operation counts, not game FPS measurements.

## Startup issue still requiring a separate design

Existing SDK native/source evidence confirms each immediate AddHook repeats thread enumeration/suspend/patch/resume. Queued Apply freezes once, but partial failures have no rollback. Additionally, the supplied static wrappers use the caller module for QueueHook and the SDK module for ApplyQueuedHooks. Source `Functions_static.cpp` and the linked archive relocations agree; naïvely using this public pair can apply an empty queue and falsely report success. A matching-module adapter plus carefully isolated completion publication is needed before batching dependent hooks. No half-finished adapter/batch code, private SDK memory patch or blanket queuing was included here.

The two omitted observer installs reduce specific startup work; this does not establish that the reported twofold main-menu delay is gone. Likewise, the proven recurring scans and old-world retention corrections are not a verified single cause of campaign FPS decline. Off-camera native AI/pathfinding/physics still run. No new gameplay measurement or universal leak-free claim.
