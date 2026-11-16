# Private morph-pose scope verification, 2026-11-16

This is staging-only implementation and verification evidence. No game executable,
process inspection, capture, profiling, package, install or publication was used.
The outer game body function is represented by a fixture body; actual native
Ogre Mesh::clone, Pose::clone, Mesh::removeAllPoses, Entity::getMesh and
softwareVertexPoseBlend execute from the pinned installed DLL. The source Entity
has zero SubEntities so the rendererless fixture can use DefaultHardwareBufferManager;
its real Mesh owns shared vertex data and 0/8/128/512/513 real poses. This is an
ownership and byte-equivalence test, not full game body/scene execution.

## Fail-first result

Before `morph_pose_scope.cpp` existed, the initial VC10 /O2 actual-Ogre fixture
cloned an 8-pose native mesh and failed ordinary `CHECK line70
source->getPose(0)==target->getPose(0)` with exit 1 and clean Ogre shutdown.
This demonstrates the stock independent Pose clone behavior which the scoped
optimization is intended to avoid. The parent was informed before core implementation.
The full final fixture retains stock behavior checks outside the admitted scope.

## Production contract

`morph_pose_scope.h` supplies the agreed opaque Create/Clone bridge API.
Installation requires exact Ogre/game image identities, exported native targets,
and six pinned code regions. Pose and cleanup hooks must both succeed, plus the
separate clone/create bridge pair and outer body hook must publish readiness.
Valid original trampolines survive partial installation and installation is once-only.

The source pin copies the actual Entity.getMesh const MeshPtr reference. The
manager capture copies the actual successful hidden-out MeshPtr before first
borrow. Both copies preserve the original control-block identity. No raw Mesh
pointer gains a second control block. The scope is owner-thread only, live setting
controlled, and accepts at most 512 poses. Up to eight retained scope contexts are
allowed; deeper calls forward while masking borrow admission. A nested source
which equals an ancestor borrowed destination falls back to native deep clones.

Only native Mesh::clone's exact Pose::clone return address can borrow, and only
while source pointer sequence, destination insertion index and source count match.
Borrowed IDs are positively recorded before return to native insertion. A mismatch
turns further borrowing off, allowing mixed lists of borrowed pointers and genuine
owned native clones. Every nested Clone masks and finally restores active borrow
TLS, including ineligible calls. The native clone, arithmetic and GPU paths remain
original. No worker, buffer lock change or pose-map write was introduced.

All Mesh::removeAllPoses calls first sanitize every retained context, independent
of setting/admission/current nested state. They compact only positively recorded
borrowed pointers, then call native remove once to destroy owned clones. Real
unload's remove call at 1A3E1F is included. The final game morph remove boundary
71CE2 drops both added MeshPtr references after native removal, before native
switchBodyMesh's use-count-sensitive Entity destruction. Other removals retain
pins until scope exit. Normal return, C++ exceptions and handled structured
exceptions use explicit __finally under /EHsc. Borrow detach precedes pin drop;
logical TLS/depth restore precedes native reference destruction; source release is
itself in finally if target release throws.

## Native evidence

Final canonical fixture passes **9,582 CHECKs each /Od and /O2** using VC10 /EHsc.
Pinned-file verification passes **53 offline contracts**. All **25 compiled
production-copy controls** compile successfully and fail with ordinary CHECK exit1
and clean Ogre shutdown; crashes, compile failures and survivors are rejected.
Canonical source/fixture hashes remain unchanged during control runs.

Covered behavior includes:

- 8/128/512 poses: stock deep clone count equals pose count; admitted count is zero.
  Source pose pointer/maps, MeshPtr control blocks/refcounts and complete vertex
  buffer bytes are preserved/equivalent. Position plus normal share stride24;
  a separate-normal-binding case compares both buffers. Positive, zero, negative
  and NaN native blend weights are exercised without changing the kernel.
- Final remove's source/target refcounts are checked before Create returns.
  The fixture calibrates one stable indirect native-boundary callsite's real
  return address; production's exact71CE2 comparison remains unchanged. Clone
  uses the explicit caller argument as a narrowly scoped harness seam; the actual
  Ogre create and Pose caller checks execute their real native return addresses.
- Normal removal, omitted removal, native unload, C++ and handled SEH exit, live
  setting OFF, mixed appended owned clone, and a 3-borrow/5-owned prefix all pass.
- A genuine native Ogre bounds exception after three borrowed poses tests partial
  clone cleanup and an inner catch tests immediate Clone TLS restoration. Source
  refs/maps remain intact; the registered partial target has no borrowed poses.
- Owner/admission/clone-ready/create-ready/config/caller/capture-result gates;
  empty/513-pose fallback; nested OFF removal sanitizing a parent; ancestor target
  source fallback; an ineligible nested clone during manager capture; depth12
  bounded at8; stock independent clones outside Create.
- Native image/prefix refusals, first/second hook failure or missing trampoline,
  retained valid trampolines, external readiness preservation, and no retry.

A test-only remove spy checks for still-borrowed pointers immediately before
native destruction. Correct production cleanup leaves that guard idle. For
negative controls it records an assertion failure and removes those pointers to
prevent the control from corrupting source maps before reporting an ordinary
CHECK. Thus a crashing mutant is never counted as a successful negative control.
Private SDK MinHook initialization is pinned and test-only; production never calls
that private address, InitRVAs, or game executable code.

## Reproduction and receipts

Run `python leopard/test_morph_pose_scope_all.py` for both compiler modes, offline
contracts and the compiled controls. `--reuse-controls` reuses only a receipt whose
canonical production/fixture hashes match exactly. Final verification used that
option after the complete 25-control pass, avoiding an unchanged recompilation.

- `build/morph-pose-scope-verification/verification.json` and numbered logs
- `build/morph-pose-scope-native-contract.json`
- `build/morph-pose-scope-mutants/verification.json` plus per-control compile/run logs

## Performance and limits

Production retains only stack contexts, a fixed pointer array and two copied
MeshPtrs per admitted scope. No persistent cache/queue, per-frame allocation,
world scan or INI polling was added. Normal cleanup scans the native destination list, checking at most 512 recorded
IDs per pointer; the admitted stock clone has at most512 poses and mixed lists
may additionally contain owned entries. Context traversal is bounded at8. Native owned Pose allocation/deletion persists on all fallback paths.
This removes admitted native Pose/map deep-clone work; no game load-time, overall
FPS, long-running memory or GPU speed claim is made. The source lease guarantees
object lifetime, not a general concurrent-unload lock; exact owner-thread/native
readonly body admission is essential. Full application build, integration review,
source commit and installation belong to the parent task.
