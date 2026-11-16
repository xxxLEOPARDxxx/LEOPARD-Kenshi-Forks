# Local model clone storage preparation, 2026-11-16

Scope: continue NPC model/equipment and save-load work from existing source and
installed native binaries. No game run, capture, process-memory access, new
profile or general FPS measurement. Working release 2; no publication.

## Native contract and decision

The installed Ogre Mesh::clone (1A3EE0) calls MeshManager::createManual
(1BE970), returning at 1A3FDD before appending SubMeshes and Poses. Its constructor
zeros the pointer vectors at 170/178/180 and 348/350/358. Native SubMesh and Pose
append loops grow the vectors repeatedly and have no upfront reserve. The Entity
SubEntity vector already reserves: duplicating that work was rejected.

Two optional, paired hooks use the x64 member hidden-result ABI directly. The
wrapper never copies or destroys the returned SharedPtr. A scoped TLS source is
masked on every nested ineligible entry and restored across native exceptions.
Only the owner thread, enabled setting, fully admitted pair, exact native return
site, matching result storage and loaded/distinct source and target permit
preparation. Empty destination lists reserve source sizes independently, bounded
to 64 SubMeshes and 512 Poses. Adequate capacity and nonempty lists are untouched.
At most 4608 pointer bytes are reserved, ordinarily needed by native cloning
anyway; malformed counts use the native path. No persistent cache or queue.

No resize, object creation, appearance blending, materials, animation, mesh
decode, GPU calls, readiness/spawn ordering or temporary destruction changes.
Optional reserve and error-logger failures cannot suppress native forwarding.
Native exceptions remain native. A failed hook pair retains any trampoline,
stays forwarding-only and is never retried. Admission pins the native PE identity,
both export addresses, constructor/list/call-site bytes and VC10 vector layout.

Moving Pose destruction to a worker was rejected: the native allocator has
thread-local pools and engine-shutdown lifetime is not established. Pose's copy
constructor also shares a GPU buffer whereas clone does not. Neither operation
is substituted. No new worker is added in this change.

## Verification

- Actual production wrappers installed through the real SDK in a standalone
  native fixture, using actual Ogre meshes, buffers, poses and clone calls.
  This executable does not open or control Kenshi or another process.
- 48061 checks each /Od and /O2. Native clone contents equal for OFF and ON,
  including vertex/index bytes, declarations, bounds, parent/material/name data,
  Pose names, offsets and normals. Includes empty/one/cap/over-cap cases and
  independent/nonempty/adequate destinations.
- An 8 SubMesh / 128 Pose fixture has 6/13 native pointer-vector growth
  allocations OFF; ON has two preparatory reserves and zero append-loop growth.
  These are storage-allocation counts, not timings or native object counts.
- Nested scopes, disabled/off-owner/unadmitted/wrong-caller/result gates,
  partial hooks/null trampolines/no retry, first/second reserve faults, throwing
  logger and a genuine native exception crossing both wrappers are checked.
- Initial Prepare NOP failed an ordinary CHECK with exit 1; normal native
  teardown completed. Pinned offline native contracts: 37 checks.
- Twenty-five compiled production mutations fail an ordinary CHECK with exit 1
  and clean native teardown; compilation failures and crashes are rejected.
  The runner never rewrites the canonical fixture. Aggregate verification pins
  all participating sources before and after the run.
- The fixture's private MinHook initialization is confined to the standalone
  test, pinned by the SDK disk hash, loaded prefix and native call contract.
  Production uses only the normal public SDK hook API.

## Warmup HUD follow-up

The active startup scan/read and pending or in-flight RAM/GPU requests keep the
three text rows visible. Terminal startup and idle queues retire rows and their
unlink observer; retained cache occupancy does not count as work. New real work
restores text. A refused non-waiting cache lock produces an unknown snapshot,
which cannot create, hide or repaint rows. Explicit disable and GUI teardown
still work immediately. Existing snapshot cadence and worker scheduling stay
unchanged. No per-frame allocation or extra world scan is added while idle.

Actual HUD runtime tests: 71 checks each /Od and /O2, policy 45 each, exact RAM
Snapshot 10 each, GPU Snapshot 9 each. Twenty compiled controls fail an ordinary
CHECK with exit 1. Native VC10 compilation and installed vanilla/Dark UI skin
definitions are checked. No in-game appearance or measured CPU/FPS claim.

## Integration and limits

MCM option modelCloneReserve / Prepare model storage is ON by default and live,
after skeleton storage preparation in Performance. EN/RU/ZH labels and help are
maintained; detailed logging remains last and OFF. Existing user INI is retained.

This removes repeated pointer-list growth inside one native loading operation.
All per-character objects and equipment still load through the native engine.
It does not eliminate save deserialization/world restoration or establish an
overall save-load, frame-time or FPS improvement. Those claims require gameplay
measurements, which the user explicitly deferred. No new testing loop requested.

Focused source coverage extends the existing full-source review only for the
changed bodies/declarations. It is not a fresh reread of the entire engine fork.

Independent review approved both production changes after rerunning the actual
native fixture, offline contracts and complete HUD script. Full VC10 /O2 /GL
DEV/PROD builds cover 367 translation units, and all 127 host suites pass. Both
DLLs import only available symbols from the installed Klib/Ogre/MyGUI binaries
and use the VC10 runtime. All 135 EN/RU/ZH messages compile and round-trip.
