# Scoped native pose buffer batching: design and gates

Goal: remove repeated locks/transfers while a single newly cloned NPC body is
being morphed, preserving the installed engine's pose arithmetic and order.
Existing saved logs contain morphCreate/poseBlend costs; no new game run,
profile, telemetry or overall FPS test is authorized. This is an investigation
plan, not approval to ship an unverified optimization.

Evidence: game createMorphedMesh71A80 clones a private Mesh, blends each active
Pose into its submesh0 VertexData, removes poses and calls switchBodyMesh before
publishing the replacement Entity. Native Ogre softwareVertexPoseBlend1A9BE0
locks the same entire vertex buffer HBL_NORMAL for each nonzero Pose, performs
the weighted additions and unlocks. D3D11 vertex lock28830 forwards to the inner
buffer+68; a no-shadow normal lock uses staging copy/read/write per invocation.
Calling suppressHardwareUpdate on the outer vertex wrapper alone is ineffective.

Candidate: keep one native lock within the owner-thread createMorphedMesh scope.
Run the original native pose arithmetic against a temporary VertexData clone
whose position binding is a private CPU-view HardwareVertexBuffer over that
mapped region. The live target VertexData/bindings never change. Flush before
native switchBodyMesh and on unwind. No worker, native Pose/Entity replacement,
float rewrite, persistent cache or new graphics resources.

Before implementation, establish:
- SDK/native VertexData and HardwareVertexBuffer prefixes, ctor/dtor ownership
  with null manager, clone(false) behavior and buffer lifetime.
- Exact original/ON vertex bytes against real native Ogre software pose blending
  for varied weights, normals, layouts, sparse maps, repeated/reordered poses.
- One real destination lock/unlock vs one per active pose; no locked buffer at
  publication, nested exit, ordinary exception or handled SEH.
- Bounded target/vertex count/bytes; unsupported types/layouts/targets, nonowner,
  partial native seam admission or setting OFF remain ordinary native calls.
- Native/optional preparation failures never cause replay of already applied
  poses; source weights/RNG/model naming/equipment/materials remain unchanged.
- Independent review must reject the candidate if ownership, scope, concurrency,
  device/unlock failure or compatibility cannot be established from existing
  evidence. No mandatory game acceptance loop is added.

Tasks:
1. Inspect exact native/backend contracts and offline fixture feasibility.
2. Write a fail-first actual native arithmetic/lock-count fixture.
3. Implement only if the ownership/unwind gates can be satisfied; otherwise keep
   the installed DLL and document the rejected avenue honestly.
4. If admitted: integrate live MCM EN/RU/ZH, existing Debug-last behavior and
   existing probe hooks without duplicate detours; run bounds/native/host/build/
   imports/review; preserve personal settings and foreign Steel changes.
5. Commit/push ready source; install only if Kenshi is closed. No release bump,
   package, Steam/Nexus publication or gameplay performance claim.
