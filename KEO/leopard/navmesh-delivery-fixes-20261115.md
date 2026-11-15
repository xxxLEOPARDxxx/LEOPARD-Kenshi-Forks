# Private phase-2 navmesh delivery corrections - 2026-11-15

Status: stage-only source and host fixtures. No source-checkout copy, DLL build/link, installation, game run, profiler, settings or publication. Baseline audit: full-audit-navmesh-pathfind-game-20261115.md. Parent independently reviewed ring reservation/required-consumer logic and force lifetime before integration; requested stale comments and pre-existing counter-wrap limitation were addressed in documentation.

## Changes

1. TryPush reserves the bounded ring window atomically. It performs at most 8 compare-exchange attempts and returns conservative FULL without taking an index when capacity/contention prevents reservation. An accepted reservation is retained by a separate DrainRequired consumer until publication. A true failed slot claim publishes an explicit cancelled index, so a refused request with native fallback cannot wedge that consumer. No wait/sleep/spin until another producer completes, heap allocation or native pointer retention is introduced. Existing lossy Push/Drain semantics remain available and unchanged. Required and lossy producer APIs must not mix in one ring. SpliceSeam's two consumers now use DrainRequired; its producers already use only TryPush.

2. Required force rebuild invalidation remains in the existing fixed 64x64 per-cell state until claim/completion or explicit world reset. Rolling the 16-entry reporting tracker no longer clears required marks. Sweeping does not cancel a pending mark because its age exceeds 60s or because its content temporarily unloads. Claim/front policy admits an aged pending mark. Existing sequence CAS prevents an old completion clearing a newer invalidation. No new world scan, queue or storage budget is added. The 60s loading-panel hold cap and a manual key press's ability to replace an old CLAIMED job are preserved.

Memory/work bounds: one extra LONG cancellation tag per ring slot; each slot changes 28->32 bytes under the target layout, +256 bytes per 64-slot ring. The two production rings therefore add 512 static bytes. No per-frame heap allocation. Strict drain still copies at most caller max and returns immediately on an unpublished reservation. Per-cell marks already had 4096 slots, live reporting remains capped at16. No native hook/ABI/address/prologue or worker scheduling was changed.

## Validation

- Force fixture extracts exact production bodies for CellIndex, AgeSec, AddLive, MarkCell, SweepLiveMarks, claim/finish/front and world reset; compiles production policy alongside them. Fake native grid and clock replace external boundaries only. Baseline Od: 146 failed assertions /199, build/nm-force-delivery-red.txt. Corrected Od and O2:199/199 each, build/nm-force-delivery-green.txt. Cases cover32 distinct marks, >121s backlog, same-key cache admission, once-only claim, old completion vs newer mark, temporary unload/reload, type1 refusal, explicit reset and unchanged panel cap.
- Ring fixture includes the exact production .cpp with interceptors around real Win32 atomics to impose deterministic interleavings. Baseline Od:5/11 failed (one free slot/two producers and delayed publication across8 drain passes), build/splice-ring-delivery-red.txt. Corrected Od/O2:11/11 each, build/splice-ring-delivery-green.txt. Failed-claim cancellation has its own control.
- Existing nm_force_rebuild_units, splice_queue_policy_units, splice_seam_policy_units pass. Existing three-producer stress runs600,000 attempts per phase. Required TryPush phase delivered every accepted record, zero overwritten/lost accepted records; ordinary lossy phases still exercise overwrite/accounting. Exact output: build/navmesh-delivery-controls.txt.
- Mutation checks operate on build-directory copies only. All10 were refused by executed assertions:16-tracker cancellation; aged claim cancellation; sweep TTL cancellation; unload cancellation; old completion clearing newer mark; reset leaving old world; age-limited front priority; non-atomic capacity; accepted reservation abandonment; failed-claim hole. build/navmesh-delivery-mutants.txt and per-mutant logs in build/tests/navmesh_delivery_mutants.
- Both fixture drivers are unittest modules registered in tools/tests/py_tests.txt rows100/101 with scoped when= selectors. Actual host Python runner passes both modules, each executing Od/O2. build/navmesh-delivery-host-integration.txt. The runner requires the existing VS2010 compiler environment; vcvars64.bat is missing locally, so verified explicit local VC/SDK paths were used. Sandbox TEMP/timing outputs were redirected to stage/build/tests/navmesh_delivery_tmp.

Reproduce after setting the existing compiler environment:

```powershell
$taskVc='C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC'
$taskSdk='C:/Program Files/Microsoft SDKs/Windows/v7.1'
$env:INCLUDE=$taskVc+'/include;'+$taskSdk+'/Include'
$env:LIB=$taskVc+'/lib/amd64;'+$taskSdk+'/Lib/x64'
$env:PATH=$taskVc+'/bin/amd64;'+$taskSdk+'/Bin;'+$env:PATH
python tools/tests/test_nm_force_rebuild_delivery.py
python tools/tests/test_splice_ring_delivery.py
python leopard/verify_navmesh_delivery_mutants.py
```

## Explicit remaining limits

- WallSplice's NOPed native call, overwrite-oldest input,64 pending boxes and no payload epoch still do not guarantee end-to-end required delivery under saturation or an old producer spanning save load. Wall pending-full drop is not represented as fixed. A free-slot-only drain would just move loss upstream; no such partial patch was made. A complete bounded dirty-region/epoch design requires separate native/coalescence proof.
- Pre-existing signed LONG ring counters, comparisons and -1/-2 sentinels are not safe at eventual wrap. Atomic capacity and required delivery claims are limited to the pre-wrap index domain. No broad counter representation change was added.
- No FPS, loading-time or in-game behavior improvement was measured or claimed. This work repairs source-proven delivery failures and validates bounded host behavior.

## Owned files for parent integration

Production: src/navmesh/construction/splice_ring.cpp, splice_ring.h, splice_seam.cpp; src/navmesh/cache/nm_force_rebuild.cpp, nm_force_rebuild_policy.cpp, nm_force_rebuild_policy.h.

Tests: tools/tests/nm_force_rebuild_units.cpp, splice_ring_injection.cpp, splice_ring_delivery.cpp, test_nm_force_rebuild_delivery.py, test_splice_ring_delivery.py, py_tests.txt. Private report/runner/selection files stay in leopard; no public documentation or settings changed.
