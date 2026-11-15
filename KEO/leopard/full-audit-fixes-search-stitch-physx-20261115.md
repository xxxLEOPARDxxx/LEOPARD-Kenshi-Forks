# Private KEO audit: fixes/search, fixes/stitch, fixes/physx - 2026-11-15

Scope: all 53 assigned source/header files, initially 8,619 physical lines. After the two owner-authorized narrow corrections and related comments, the current coverage inventory contains 8,633 physical lines. Every handwritten body/declaration was read; full-line comments were skipped only when not defining a native/ownership contract. Truncated stitch_source output was reread over the missing range. Parent covers the other fixes directories. This report makes no claim that these findings explain the reported main-menu delay, NPC stutters or campaign FPS decline.

Audit used existing local source and targeted reference searches. No game, process control, live native pointers, captures, profiler, gameplay tests, new hooks or installed DLL were used. The subsequent phase compiled only isolated production-body/policy host fixtures. Source checkout and installed game module/settings were not changed.

## Corrected findings

### F1 - Crash report could scan total session history rather than the 16-slot ring

Original src/fixes/stitch/stitch_byte_guard.cpp:453 iterated n from s_next down to zero until maxEntries valid seq matches were found. plugin/crash_record.cpp:440 requests four rows. An interrupted/unpublished ring can have fewer than four committed matching rows; old tickets cannot recover overwritten records, but the loop could still examine a million tickets after a million lifetime records. A stale seq generation left in a reused slot could even be emitted outside the newest 16 tickets.

Authorized correction: current stitch_byte_guard.cpp:452-456 counts examined slots independently of shown rows, with at most kRingSize=16 iterations. It preserves newest-first order, requested row limit, startup ticket-one bound, existing output fields, no-allocation formatting and the existing native guard paths. It neither blocks a producer nor retries publication. No machine code/stub/ABI was changed.

The production-body formatter fixture counts actual Read(seq) calls, not elapsed time. With ticket 1,000,000 and all slots unpublished the bound holds; with three fresh rows plus an expired seq generation, only those three fresh rows are emitted. Requested row count above capacity remains bounded to 16. Null/zero capacity, zero requested rows and one-byte termination are covered.

### F2 - DetailedLog OFF still performed all StitchSource forensic work

Original src/fixes/stitch/stitch_source.cpp:370-408 always computed in-flight metadata, called the native stitch, reread both NavInstances, traversed both graphs' matching connection sets via ReadSet, computed index ranges and published up to four bounded forensic tables. Insert observation also read native slots and wrote s_add with logging OFF. Formatting functions alone had a DetailedLog guard.

Proof of safe separation:
- s_write, s_pair, s_add, s_gen, overflowBits, inFlightPairs, repeat state and their storage are translation-unit-local.
- All table/pair readers feed StitchSourceOnSetDrops classification, EmitHeartbeat or emitted forensic fields. No consumer changes native edges, admission, scheduler queues, cache content, budgets or worker ownership.
- StitchSourceOnSetDrops already returns immediately when DetailedLog is OFF.
- NmAdjCheckStitch is a separate adjacency observer and must remain called; this correction preserves it always, before the native stitch in both branches. The stitch still installs because adjacency observes that seam.
- Streaming insertion still executes original addInstance and NavMeshLifeOnAdd in path_guard_hooks.cpp:27-30. Only the independently called forensic StitchSourceOnAdd returns early.
- The old counted ON path retains CallOrigCounted's __finally cleanup and exact return/argument ABI. OFF directly forwards the same native call after adjacency.

Authorized correction: current stitch_source.cpp:370-377 keeps SiteOf and NmAdjCheckStitch, then directly returns the native result with DetailedLog OFF. The ON branch keeps existing counters/forensic reads and writes. OnAdd:427, BeginFired:461 and Tick:693 now avoid their forensic work while OFF. No new thread, allocation, cache, queue, lock, native pointer retention or hook installation was added. Existing fixed tables remain initialized once, so enabling DetailedLog requires no unsafe concurrent reset.

Late enabling necessarily gives partial forensic history. Tables are never cleared under worker writers; retained old diagnostic rows are not an authoritative complete timeline. This is documented in stitch_source.h and is not used by gameplay correctness. No retrospective record or performance measurement is promised.

## Evidence and integration state

Files staged by this last correction:
- src/fixes/stitch/stitch_byte_guard.cpp
- src/fixes/stitch/stitch_source.cpp
- src/fixes/stitch/stitch_source.h
- tools/tests/test_stitch_diagnostic_boundaries.py
- tools/tests/py_tests.txt row 102 (registered scoped production-boundary test)

The test compiles the exact formatter, hook_nmgStitch, CallOrigCounted, ClaimInFlight, ReadUidOr, insert observer, repeat observer and Tick bodies. External native stitch/guarded reads are controlled boundaries; the add observer uses the real LfTable implementation and production layout. This is stronger than policy-only testing but does not replace the parent’s eventual complete source compilation/native review.

Fail first: build/stitch-diagnostic-boundaries-red.txt records four formatter failures and six OFF-path failures at BOTH Od and O2, while native forwarding/cleanup controls passed. After correction: eleven formatter controls plus twenty-one source controls pass at BOTH Od/O2. Native arguments/result, adjacency order/site flag, read failure, no-write stitch, SEH propagation/finally release and a DetailedLog flip during a counted native call are checked. Insert observation publishes a real lock-free add record when ON and performs zero native reads/table writes when OFF.

Nine compiled production-body mutants are refused by behavioral assertions: unbounded crash history, one-short bound, wrong starting order, OFF forensic path, missing adjacency, missing native finally count cleanup, OFF insertion reads, OFF repeat-state writes and OFF heartbeat work. Mutant sources exist only under build/tests/stitch_diagnostic_boundaries; production files are never mutated by the test.

Commands:
- python tools/tests/test_stitch_diagnostic_boundaries.py --mutants
- python tools/tests/run_suites.py --suites leopard/stitch-search-physx-suites.txt
- python tools/tests/run_py_tests.py --list leopard/stitch-diagnostic-py-tests.txt

Logs: build/stitch-diagnostic-boundaries-green.txt, build/stitch-diagnostic-boundaries-mutants.txt, build/stitch-search-physx-controls.txt and build/stitch-diagnostic-host-integration.txt. The thirteen scoped existing policy/layout suites all passed; the registered production Python module passed. Whole DLL build/installation belongs to parent and was not performed here. Independent reviewer /root/texture_retention_impl verified the frozen hashes/diff and complete consumer separation, independently reran all controls and nine compiled RED mutants, and reported no Critical/Important finding before integration.

Frozen production SHA256:
- stitch_byte_guard.cpp: 85cb1ac2a2fafbd561dd515294ebafdb2e5b2b7cfdd390e833a1e804130b3b4c
- stitch_source.cpp: 2af3634a0ba869e91b2f7412470d941901e42496db9aaf91bc48b8f79c1278ef
- stitch_source.h: 7e234a86191b537f81def59a81e6056ef5f6cb5ea1bad0e14978abe43b80ef0b

## Remaining risks and deliberately unmodified behavior

No additional Critical/Important defect was established by this audit. These items distinguish coverage limitations from proved regressions:

1. Search guards are targeted null/absent-section defenses, not general validators of arbitrary corrupt graph arrays. GraphPosition/Heuristic/Expand/Visitor pass unjudged cases to the original rather than invent ownership or mutate cached instance slots. Known absent instances use conservative fallback/skip behavior. Raw array lifetimes still depend on the pinned native caller contract; adding arbitrary SEH, locking or broad worker movement was not justified.
2. cluster_cross_cost reads/writes graph instance owned maps/edges under its native registration seam; original registration precedes rewrite and live-graph publication follows it. Counts/neighbor references rely on the native model. The cost format is the Havok half/bfloat-style upper float bits, not IEEE binary16; no format bug was found. Missing reciprocal links leave a partial rewrite by design; changing that requires native semantic proof.
3. Unstitch ScanForOutOfBounds:198-268 answers only whether a recorded opposite node index misses the map. The replacement additionally rejects invalid map values:421-427, but a standalone bad map value cannot itself select that replacement. Comments:416-420 explicitly acknowledge this. This is a known narrower defense/coverage gap, not a newly proven corruption source. Scan alone must not be gated by DetailedLog. Its repeated slot/connection traversal and replacement pass have functional purpose. Early exit after a known offender with logging OFF could reduce denominator-only scanning, but was not needed for the two authorized corrections and was not changed.
4. StitchByte diagnostic payload is still read after a single seq check, and multiple wrapping writers can overwrite a slot while it is formatted. The ring has no exclusive per-slot reservation or stable reader copy; output can therefore be mixed/lost under extreme overlap. Guarded payload reads limit direct faulting, but diagnostics are not a reliable ownership protocol. This correction bounds iterations only. Existing signed LONG ticket wrap and modulo/sentinel assumptions remain unresolved; no broad counter refactor was attempted.
5. StitchSource lock-free tables have fixed capacity and never evict/free keys during a session: 8192 exact +4096 pair +2048 add +2048 generation slots, each at most 64 probes. Overflow/contended results lose diagnostic history but cannot grow memory or affect native work. Pointer-address reuse and non-epoch forensic keys can misattribute old history. A world-safe epoch design would be diagnostic hardening, not a gameplay cache fix.
6. HullQueueGuard's judging/nulling occurs in the native main-thread window with the physics thread idle; it never dereferences a queued hull. It distinguishes duplicates in-list/pending/freed and retains remade addresses registered through hullsToMake. Capacity refusal falls back to untouched native lists. PushRec caller provenance can race/collide but is output-only. The five pusher hooks could potentially be lazy/off with DetailedLog OFF; deduplication itself must remain active. No additional hook changes were made.
7. HullSameSkip table has a single physics-thread writer, bounded 32-probe lookup, clearing at half full, bit-exact target comparisons and actor identity. Actor-less create always forwards and invalidates the old actor match; teleport always forwards. Main-thread toggles publish a clear request before enabling and loading requests a clear. No native pointer is dereferenced by table lifetime. It still relies on native create/apply lifecycle and the verified slot/thunk; no campaign memory growth established.
8. PhysQuery gate uses guarded qword reads and module-range/purecall classification, then the original virtual call. There remains an inherent check/use interval if an external owner frees an object concurrently; the guard cannot provide object ownership. Injected stubs preserve only the volatile registers proved live at their pinned sites; StitchByte's header explicitly states xmm0-5 are dead until next call. No mismatch in the read source contracts was established. The source audit is not independent disassembly of every current binary instruction.
9. Both near-page guards intentionally keep their small code/data allocation when armed and atomically redirect the data slot to a keep-all stub at neutralization. A site VirtualProtect failure can leave one unused near allocation; this is a small failed-init/process-lifetime leak, not an unbounded campaign cache. Returning/freeing armed stubs would require proving no thread can still execute them.
10. Purecall recorder is fatal-path diagnostics, with bounded stack output and existing-handler chaining. Pointer slot install/uninstall uses a best-effort check/write rather than compare-exchange; a competing handler installation could race it. DLL detach does not itself prove prior/in-flight callbacks are quiescent. No runtime plugin unload or observed concurrent handler writer was established, so this remains a native lifetime risk rather than a newly reproduced defect.
11. Search/PhysQ/Unstitch counters and some QPC heartbeat admission still run with DetailedLog OFF. Broad telemetry cleanup must follow consumers individually; no blanket gating of guards, duplicate judging, adaptive scheduling or native fallback is safe. F2 was singled out because its complete consumers were traced.

This audit establishes source work and boundedness, not measured FPS benefit, absence of all leaks, or the cause of the owner’s native appearance callback stalls. Earlier material-map allocation and 201-player cap findings remain in the separate navmesh/pathfind/game report and were not silently patched.

## Per-file coverage

Current physical lines include the authorized comments/fixes. Every row below was fully read as a body/declaration, with native contract comments examined as needed.

| File | Lines | Coverage |
| --- | ---: | --- |
| src/fixes/physx/hull_queue_guard_policy.cpp | 208 | body |
| src/fixes/physx/hull_queue_guard_policy.h | 143 | body |
| src/fixes/physx/hull_queue_guard.cpp | 469 | body |
| src/fixes/physx/hull_queue_guard.h | 24 | body |
| src/fixes/physx/hull_same_skip_policy.cpp | 117 | body |
| src/fixes/physx/hull_same_skip_policy.h | 55 | body |
| src/fixes/physx/hull_same_skip.cpp | 197 | body |
| src/fixes/physx/hull_same_skip.h | 20 | body |
| src/fixes/physx/physx_query_guard_policy.cpp | 167 | body |
| src/fixes/physx/physx_query_guard_policy.h | 143 | body |
| src/fixes/physx/physx_query_guard.cpp | 516 | body |
| src/fixes/physx/physx_query_guard.h | 41 | body |
| src/fixes/physx/purecall_layout.cpp | 70 | body |
| src/fixes/physx/purecall_layout.h | 61 | body |
| src/fixes/physx/purecall_record.cpp | 417 | body |
| src/fixes/physx/purecall_record.h | 41 | body |
| src/fixes/search/cluster_cross_cost_policy.cpp | 196 | body |
| src/fixes/search/cluster_cross_cost_policy.h | 41 | body |
| src/fixes/search/cluster_cross_cost.cpp | 146 | body |
| src/fixes/search/cluster_cross_cost.h | 10 | body |
| src/fixes/search/graph_expand_guard_policy.cpp | 114 | body |
| src/fixes/search/graph_expand_guard_policy.h | 90 | body |
| src/fixes/search/graph_expand_guard.cpp | 290 | body |
| src/fixes/search/graph_expand_guard.h | 8 | body |
| src/fixes/search/graph_heuristic_guard_policy.cpp | 186 | body |
| src/fixes/search/graph_heuristic_guard_policy.h | 64 | body |
| src/fixes/search/graph_heuristic_guard.cpp | 331 | body |
| src/fixes/search/graph_heuristic_guard.h | 12 | body |
| src/fixes/search/graph_position_guard_policy.cpp | 95 | body |
| src/fixes/search/graph_position_guard_policy.h | 96 | body |
| src/fixes/search/graph_position_guard.cpp | 271 | body |
| src/fixes/search/graph_position_guard.h | 11 | body |
| src/fixes/search/graph_visitor_guard_policy.cpp | 89 | body |
| src/fixes/search/graph_visitor_guard_policy.h | 94 | body |
| src/fixes/search/graph_visitor_guard.cpp | 198 | body |
| src/fixes/search/graph_visitor_guard.h | 9 | body |
| src/fixes/stitch/stitch_byte_guard_policy.cpp | 126 | body |
| src/fixes/stitch/stitch_byte_guard_policy.h | 107 | body |
| src/fixes/stitch/stitch_byte_guard.cpp | 493 | body |
| src/fixes/stitch/stitch_byte_guard.h | 38 | body |
| src/fixes/stitch/stitch_source_policy.cpp | 194 | body |
| src/fixes/stitch/stitch_source_policy.h | 297 | body |
| src/fixes/stitch/stitch_source.cpp | 741 | body |
| src/fixes/stitch/stitch_source.h | 47 | body |
| src/fixes/stitch/unstitch_guard_policy.cpp | 71 | body |
| src/fixes/stitch/unstitch_guard_policy.h | 65 | body |
| src/fixes/stitch/unstitch_guard.cpp | 578 | body |
| src/fixes/stitch/unstitch_guard.h | 27 | body |
| src/fixes/stitch/unstitch_layout.h | 72 | body |
| src/fixes/stitch/unstitch_probe_policy.cpp | 87 | body |
| src/fixes/stitch/unstitch_probe_policy.h | 90 | body |
| src/fixes/stitch/unstitch_probe.cpp | 534 | body |
| src/fixes/stitch/unstitch_probe.h | 26 | body |

Total: 53 files / 8,633 current lines; initial read snapshot 8,619 lines.
