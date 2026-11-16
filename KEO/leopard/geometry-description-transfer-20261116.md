# Geometry retention description transfer, 2026-11-16

User scope: continue NPC/model/equipment/save loading from existing evidence,
without new gameplay runs, captures, profiles or general FPS measurements.
Working release 2, local correction only, no publication or version bump.

## Root cause and correction

The geometry cache already collected a bounded buffer description before
admission. Promote copied it into a temporary Entry, into a pair and into the
map node. The actual production/native fixture observed three redundant buffer
array allocations and a different final array. The 512-buffer case requested
12288 bytes for each copy, beyond the original already collected array.

Promote now inserts empty metadata with the correct touch, adds the native mesh
reference once, and accounts buffers from the still-owned input description.
Only after every allocating operation succeeds does it swap the vector into the
entry and zero the consumed input byte counters. VC10 std::allocator vectors
use the equal-allocator pointer-swap path, which does not allocate. No throwing
operation follows transfer. All false-return paths retain input for retry;
allocation exceptions retain the existing Tick Clear recovery, including partial
map/refcount bookkeeping and reservation reset. Both callers stop using the
input after success. Pending queue, byte limits, victim ordering, capture rules,
model refresh/unregister detection, native pins, threads and hooks are unchanged.

## Verification

- Fail-first actual-source fixture: copies3, different storage, unconsumed input;
  ordinary failed CHECK exit1 after clean Ogre teardown.
- Actual production/native fixture: 6430 checks, /O2 and independent /Od, copies0,
  same storage, consumed input; one retained MeshPtr and balanced release.
- Nine new allocation fault points from map-node creation through the last
  buffer node preserve the input array and clear partial state. Budget refusal
  preserves input and refs. Existing tests include full2048 models, duplicate
  buffers, refresh, unregister, cap/pressure/world/device and retry behavior.
- Three compiled production-copy controls reintroduce copying, redundant copy
  and stale consumed-byte metadata. Each compiles and fails an ordinary CHECK
  exit1 with clean native teardown. Canonical fixture/source hashes stay fixed.
- Independent reviewer approved the production ownership/recovery change and
  reran /Od. The existing no-native-pin mutation anchor was updated for the new
  assignment; the prior mutation runner still accepts generic nonzero failures,
  so its inherited results are not represented as strict CHECK-only controls.

This proves reduced KEO bookkeeping allocations, not faster native NPC creation,
equipment assembly, save deserialization, frame time or long-running leak absence.
The unchanged player options/EN-RU-ZH catalogs require no translation update.

## Rejected mapped-pose batch investigation

The native pose path repeatedly locks one buffer. It was inspected offline,
including the actual D3D11 forwarding wrapper and inner unlock/copy-back path.
Outer suppressHardwareUpdate alone does not suppress the delegated inner path.
Renderer20500 clears its staging flag and unmaps before copy-back through20600,
which has RenderingAPIException branches. Generic unlock only clears the outer
locked flag on success. Retrying cleanup can therefore use a different path.

Independent review also found unproven handled-SEH cleanup under /EHsc, second
exception policy, renderer/reentrant exclusion and target lifetime. Null-manager
VertexData actually substitutes the global manager and allocates declarations/
bindings, unlike null-manager HardwareVertexBuffer. The mapped-scope candidate
was rejected for production. No new batching, arithmetic, graphics or worker
operation is included in this correction; the existing native path is retained.
