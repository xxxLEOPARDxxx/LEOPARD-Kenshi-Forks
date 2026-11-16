# Temporary pose borrowing for private body morphs: investigation plan

Goal: remove deep copies of all sparse pose maps which createMorphedMesh uses
read-only and immediately deletes. Existing installed native code: game71AE1
clones the source mesh, loops cloned poses through name/offsets/normals getters,
then71CDC removeAllPoses before71D07 switchBodyMesh. Ogre Mesh::clone1A43FA calls
Pose::clone229BF0, which allocates a Pose and deep-copies both maps38/60.

Candidate, NOT yet authorized to ship by these notes: during this exact private
owner-thread body-morph call only, return source Pose pointers from the native
clone loop, without copying their maps. Detach these borrowed pointers before
native deletion. Original pose blending, weights, RNG, GPU locks/uploads, naming,
materials, per-character meshes/entities and source pose data stay native.

Requirements before production:
- Independent review of source-pose immutability/lifetime within this scope.
- Pin source MeshPtr from original Entity; pin destination MeshPtr after native
  createManual, never construct a second control block from a raw pointer.
- Exact admitted native outer create caller, Mesh::clone caller71AE7, Pose::clone
  caller1A43FF, private target/source/count validation; no general mesh sharing.
- All-or-fallback installation across outer scope, native clone/create capture,
  Pose::clone and removeAllPoses; preserve every valid original trampoline.
- Fixed bound512poses and bounded nested contexts; no persistent map/queue.
- Remove ONLY pointers positively recorded as borrowed, preserving any native
  owned clones in a mixed list. Compact pointers before calling original remove.
- On normal return, C++ exception and handled SEH, detach any remaining borrowed
  pointers before dropping pins, restore TLS, and release added references.
  No GPU map/unmap or new worker/destruction transfer is introduced.
- Source pose pointers/maps must remain unchanged and native normal mesh clones
  must remain independent outside this exact scope. Native error propagation and
  public results must remain correct. No replay after partial success.

Tasks:
1. Independent design review plus exact native cleanup/clone/reader contracts.
2. Fail-first standalone actual Ogre fixture with native hooks and native blend;
   prove no deep Pose copies while output vertex bytes and source maps match.
3. Implement only if lifetime, nested/mixed cleanup and SEH gates can be proven.
4. Production-path bounds, native errors, allocation failures, nesting, partial
   hook/admission/live settings, normal/C++/SEH teardown and compiled controls.
5. If accepted, owner MCM EN/RU/ZH, full build/import/source review, settings
   preservation, scoped commit/push and local install with Kenshi closed.

No new game runs/captures/profiles, FPS measurement or timing claims. Existing
seven foreign Steel files stay unchanged. Working2, no publication/version bump.
If source immutability or cleanup cannot be established, reject this candidate
and retain the installed DLL; do not substitute another unproven ownership trick.
