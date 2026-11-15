# KEO whole-source audit and follow-up corrections (2026-11-15)

Private development report. This follows the first nine corrections committed as `a6fcd84`. The owner explicitly stopped new game reproductions, profiling and data collection. Work uses existing source/binary evidence, actual-production offline fixtures, compiler checks and independent reviews. No game was launched or captured. No Nexus/Steam publication, release-number bump or personal changelog entry is implied.

## Coverage

All 733 source CPP/H/INC files are accounted for by five body/declaration review reports and the final SHA256 coverage manifest. This goes beyond the earlier inventory pass. The generated GOG profile table received structural validation; its bytes were not independently reverse-engineered one by one. Third-party engine implementations and arbitrary modded executable builds are outside this source audit.

- `full-audit-core-render-resources-20261115.md`: 253 files.
- `full-audit-zone-movement-planner-20261115.md`: 199 files.
- `full-audit-navmesh-pathfind-game-20261115.md`: 164 files.
- `full-audit-fixes-world-streaming-20261115.md`: 64 files.
- `full-audit-fixes-search-stitch-physx-20261115.md`: 53 files.

Every substantive correction received independent review. One reviewer found an actual GPU fallback/shadow regression in the proposed correction; it was fixed and a native->SDK->native same-buffer regression was added before approval. Passing fixtures establish the tested source boundaries, not measured game FPS, startup seconds or universal absence of leaks.

## Corrected behavior

- Replan work rotates fairly through the existing 64 plans, so four failing low slots no longer indefinitely starve later plans. Invalid player rosters revoke stale published formation pace without dereferencing old characters or deleting group state.
- The permanently empty island component overlay no longer performs unused grid/union-find work. Quiet handoff frames skip the 4096-cell scan when the ready count is zero. With detailed logging off, the native readiness hook no longer runs classification scans/locks/clocks that cannot alter its answer; real functional classifier callers are preserved.
- Normal living squads stop the corpse-position fix's duplicate member scan at the first living character, matching the original policy. Navmesh lifecycle sampling and stitch forensic tables remain dormant with detailed logging off; mandatory native calls, adjacency checks, bounds guards and crash error paths stay active.
- Selected path-pass/request/A* telemetry now bypasses pure depth scans, timing/histograms, goal/sqrt/output samples and optional slow-sample locking with detailed logging off. Native guard/manager latch, enqueue priority, player request budget ownership/Take/Apply, owner labels and hierarchical search remain active. OFF dequeue clears old diagnostic stamps, and nested pass TLS/timing context restores correctly. Coupled gate-pass/hierarchical sequences were deliberately not half-gated; this is not a claim of removing every path timer. See `path-telemetry-debug-gate-fix-20261115.md`.
- Required TryPush seam work uses atomic bounded reservation and a consumer that retains accepted unpublished entries. Required per-cell force-rebuild invalidations survive reporting-table eviction, backlog and temporary unloading until consumed or world reset. No new world scan or unbounded queue.
- GPU lookup/upload plans accept only map owners whose final deletion is actually watched; conservative rebinds advance a separate revision. Unsupported native uploads invalidate tracked byte shadows before they can be reused. SDK/custom maps keep native behavior.
- An unused benchmark no longer overwrites a foreign game-clock rate every idle frame. Restoration debt is incurred only by successful KEO writes, preserved across failed restore/reset and relinquished after a differing foreign write.
- Full diagnostic buffers are terminated inside their storage. Crash ring formatting examines only stored ring capacity rather than walking the session ticket history. Fallback settings-group storage covers its bounded staged row domain rather than silently losing gating after eight children.

The focused test/report files describe actual-source extraction or inclusion, native boundary replacements, /Od and /O2 runs, fail-first results and compiled negative controls. The existing 127 host suites passed together with KENSHI_ROOT set. Final DEV/PROD link/import verification and installation are recorded separately in `full-audit-build-verification.json` and `full-audit-install.json` in the private staging directory; those receipts are not distributed as player settings or public release notes.

## Performance questions and limits

Loaded off-camera squads still perform world simulation, AI, pathfinding and physics. Existing KEO far visibility staggering, paused update reduction, empty scene-worker forks and empty instance uploads reduce particular work; they do not freeze those squads. Native frustum culling is not equivalent to eliminating CPU simulation. Whole NPC creation was not moved blindly to workers: world/scene/material/physics ownership remains constrained by native contracts.

RAM warmup avoids repeat resource reads and the bounded GPU/geometry caches can reuse admitted resources. They cannot remove save deserialization, world restoration and all NPC creation cost. No claim that warmup should make the entire save load proportional to disk-read speed is made.

The owner's slow-main-menu observation predates the new caches. Existing records show roughly 4 seconds spent across startup admission/hook work and repeated ~31ms per-hook enable intervals. Actual KenshiLib QueueHook/ApplyQueued was examined: apply can leave an arbitrary partial enabled set on failure and offers no safe query/rollback contract. A blanket deferred-hook conversion would risk partially armed paired hooks; it was not performed. The earlier first phase made twelve pure resource timing seams lazy. The reported twofold startup difference is not represented as fully fixed or blamed only on warmup.

No unbounded campaign leak or single verified cause of long-campaign FPS decline was established. Confirmed unnecessary recurring work was removed; bounded stale planner overlays/interior UID retention and formation/preload lifecycle coupling remain documented. Resource queues/pins/generations/reset paths were reviewed but this is not a universal leak-free guarantee.

## Explicit remaining correctness risks

- The older WallSplice lossy input plus 64 pending boxes and missing payload epoch still do not guarantee end-to-end required delivery under saturation/reset. The corrected TryPush SpliceSeam uses a separate required-consumer contract; it does not silently claim to fix WallSplice. A safe complete dirty-region/epoch design needs its own native/coalescence proof.
- Existing signed ring ticket/sentinel wrap remains outside the pre-wrap delivery guarantee. Best-effort diagnostic live payloads can mix under concurrent overwrite; formatter bounds do not establish snapshot consistency.
- Fresh navmesh material-map allocation failure can retain incomplete settings instead of taking the real-buffer fallback. The player re-request set rejects 201-256 members despite its 256 capacity. Both moderate conditional findings are documented and were not folded into this performance-focused patch without their required construction/ownership fixtures.

Seven pre-existing foreign Steel changes were protected byte-for-byte and excluded from these commits. Player INI, translations, resource budgets and enabled gameplay settings are preserved by the final installation transaction. Detailed logging was already restored to OFF in the preceding phase at the owner's request to stop diagnostic collection.
