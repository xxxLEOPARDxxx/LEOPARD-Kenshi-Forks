# Native body-shape upload batching

Scope: actual NPC appearance creation (including restoration from saves), not
save parsing or generic file-cache housekeeping. No game execution, captures,
new runtime collectors or performance claims. Existing morphPoseReuse switch
controls admission. Native arithmetic, independent destination buffers and
creation order remain unchanged.

The pinned createMorphedMesh loop applies poses individually using Ogre's
softwareVertexPoseBlend, locking/unlocking the entire POSITION buffer per pose.
For shadowed GPU buffers Ogre documents suppressHardwareUpdate across repeated
blends. Begin only after the exact private clone has completed, on its distinct,
unlocked, unsuppressed shadowed POSITION buffer. Keep one SharedPtr lease, batch
hardware uploads, flush before pose removal/switchBodyMesh. Restore and release
unconditionally, including OFF/nesting/native exceptions. No frame scan, worker,
persistent cache or shared source writes.

- [x] Add an offline native-Ogre fixture proving repeated uploads with old code.
- [x] Implement bounded scope state and exact buffer/native contracts.
- [x] Verify identical geometry, native math, isolation, forwarding and cleanup;
      normal flush failures propagate, unwind cleanup preserves native faults.
- [x] Independent review; address findings before installing.
- [x] Full VC10 build, imports/exports, protected-file checks, local installation
      only if process is absent; scoped commit/push. No release/version bump.

Investigated and rejected: pre-sharing a skeleton before Entity::_initialise
(native unconditionally overwrites it); globally sharing clone index buffers
(violates independence without complete mutation proof). Save parsing is separate
and is not claimed optimized by the body-model change.
