# Body-model upload batching - local KEO 2 candidate, 2026-11-16

This change targets actual native body-model construction during NPC/save
restoration. It is not another archive-cache policy change. Equipment creation,
save deserialization and spawning order are unchanged; these remain open work.
No game was started, no live log/process memory was read, no gameplay route,
capture, profiler or runtime collector was requested or added. No measured game
loading/FPS/long-run memory improvement is claimed.

## Pinned native mechanism

Game SHA256 `504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e`:
createMorphedMesh71A80 clones at71AE1, selects SubMesh0->vertexData at71AE8..71AF9,
applies each nonzero pose at71CAD, removes poses at71CDC, then switches model71D07.
Ogre SHA256 `7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6`:
softwareVertexPoseBlend1A9BE0 selects POSITION and locks the full buffer1A9CB2,
applies native arithmetic, unlocks1A9E2E. The native loop has no upload batching.
Mesh constructor1A2B1E initializes vertex/index shadow policies to true (0x101).
OgreMesh.h explicitly advises suppressing hardware updates across repeated
pose blends. Native suppressHardwareUpdateCBFF0 sets byte39 and false calls
virtual48. Additional production byte proofs pin these seams/layout operations.

## Admission, ownership and failures

The existing live/default-ON `morphPoseReuse` switch admits the same exact private
clone scope. After a complete successful native clone, one distinct POSITION
buffer is eligible only if it has an independent shadow, is not system memory,
is not locked (including its shadow), and was not already suppressed. The one
native SharedPtr lease lives in explicitly initialized POD storage. No extra
hook, worker, persistent cache, queue, per-frame/per-pose wrapper or world scan.
At most one extra buffer reference per existing scope, at most8 nested scopes.

Normal flush precedes pose detachment/removal and mesh switching. End marks the
lease inactive before flushing, so failures are not retried by finally. Normal
backend errors propagate. Exceptional cleanup catches its own C++/SEH errors,
preserving the incoming native exception and releasing leases. An abandoned
native lock is not forcibly unlocked: its suppression clears, cleanup reports
the error and the caller retains responsibility for its lock. A failed upload
cannot be claimed coherent until the native backend succeeds on a later retry.
Unsupported/no-shadow/already-suppressed/shared/malformed paths stay native.

## Verification

- Fail-first compiled native fixture with previous production code: eight poses
  caused eight sink uploads; CHECK `uploads==1` failed with clean Ogre shutdown.
  An earlier fixture setup hit material validation without a render system; it
  was corrected and is not counted as a behavioral RED.
- Current native fixture: **117 checks each /Od and /O2 PASS**. Actual pinned
  Ogre clone, pose arithmetic and native lock/unlock; deterministic shadowed
  hardware-buffer sink, not a real D3D device or GPU timing. Eight uploads become
  one, final bytes match. Guards, parent/child nesting, live OFF, flush-before-
  removal, reference counts, C++/SEH failures, no failed-flush retry and abandoned
  shadow locks are exercised. Fixture Entity is deliberately zero-SubEntity;
  SubMesh is added afterward to exercise cloning/blending without rendering.
- **7 production-copy controls**, each compiles and returns ordinary CHECK exit1
  with Ogre shutdown; crashes/compile failures are rejected as control results.
- Existing scope: **9583 native checks each /Od and /O2**, **58 offline pinned
  contracts**, **25 compiled negative controls**, aggregate PASS.
- Full VC10 /O2 /GL /LTCG build: all369 source units covered; DEV and PROD pass.
  Full installed-engine export/import validation passes:183 KenshiLib,37 Ogre,
 65 MyGUI imports; startPlugin export present; CRT100. KenshiLib0.5.0 gate passes.
- Independent read review `/root/morph_upload_design_review`: no remaining
  blocker. Reviewer inspected source/results without independently executing
  binaries. The requested final117-check /Od run subsequently passed.

Stage evidence: `_codex_work/keo-resource-cache-20261114/build/morph-upload-*`,
`morph-upload-binaries.json`, protected-file and local-install receipts. No MCM
schema/UI/localization change; installed catalogs/settings remain untouched.
No ZIP, version bump, publication or published-update journal entry.
