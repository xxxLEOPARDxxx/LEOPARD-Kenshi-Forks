# Private geometry orphan-reference fix, 2026-11-15

Stage-only change. No source-checkout edit, game/DLL build, installation, settings change, live process access, gameplay run, capture or profiler. The installed module is unchanged. The observed 127 ms attachment callback is not attributed to this defect; campaign FPS or save-load improvement is not claimed.

## Proven defect and narrow fix

`src/resources/resource_geometry.cpp` refresh previously accepted an owned MeshPtr solely because `Describe` still found loaded buffers. It did not verify that MeshManager still registered the same object. Native `ResourceManager::remove(handle)` can unregister a mesh while external SharedPtr ownership keeps the object and its buffers loaded. The orphan can no longer serve normal name/handle lookup, but KEO continued retaining and charging it until unrelated LRU pressure, explicit unload, or a world/device clear.

The existing two-entry/250 ms refresh now verifies current MeshManager presence, the held resource's creator identity, and `manager->getByHandle(mesh->getHandle()).get()==mesh.get()` before `Describe`. Missing/replaced identities release only KEO's existing pin and accounting through the existing `Release`. No resource is explicitly unloaded/removed/loaded. Entry limits, admission rate, thread ownership, native callbacks, RNG, readiness and shared-budget arithmetic are unchanged. Full-cache revisit latency remains the existing bounded round-robin interval (up to about 256 seconds for 2048 entries); this is eventual cleanup, not immediate interception of removal.

At most two passive manager lookups are added per heartbeat. The temporary ResourcePtr is local to that iteration. No new queued/long-lived reference, cache, hook or per-frame allocation is introduced. Actual native getByHandle has ordinary ResourceManager synchronization; a strict no-wait claim is not made.

## Native evidence

Pinned Ogre SHA256: `7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6`.

Actual MeshManager vtable at RVA `68E748` has remove(handle) at slot `78` -> `2937A0`, getByHandle at `A8` -> `293CE0`, addImpl at `100` -> `292460`, and removeImpl at `108` -> `292FC0`. remove(handle) resolves its local ResourcePtr then calls removeImpl at `2937D9`; retained ownership prevents final destruction. The real-Ogre fixture proves manager removal leaves an externally owned mesh loaded. This actual-build behavior takes precedence over the SDK ResourceManager.h prose saying removal also unloads.

## Verification

The new real-Ogre orphan/replacement case failed against unmodified stage production at its expected `models==1 && reserved==67` assertion: orphan 61-byte buffers remained charged alongside the valid replacement's 67 bytes. The native `isLoaded` and alias assertions before that failure passed. After the refresh fix, all 70 collector/lifetime/purge checks pass with VC10 `/Od` and `/O2`.

Coverage includes native manager removal; same-name replacement with a different pointer/handle; external alias lifetime; exactly one released KEO controlling reference; intact loaded replacement; no re-admission of the old orphan; native unload; creator mismatch; a different registered object returned for the held handle; and five removed resources released strictly two/two/one per heartbeat with exact budget reduction 55 -> 33 -> 11 -> 0. Existing world/device/off/pressure/shadow-buffer/global-dedup tests continue passing.

The creator/handle branch isolation uses a genuine native Mesh subclass registered by actual protected addImpl (verified vtable slot), with two controlled passive getters. Five unused String-animation virtuals absent from this Ogre build are abort traps in that fixture; the production code and native mesh vtable are not replaced. Actual-manager removal/replacement cases use ordinary native createManual resources without those overrides.

Read-only native image contracts pass, including the added manager slots/remove-call bytes. Existing actual WARP shared-cap reservation fixture passes. All 14 compiled mutation controls produce the expected RED result, including deletion of orphan/identity/creator rejection; runner exit is 0. Logs are under `build/resource_geometry_test/mutants`. Its old terrain-cap mutation anchor was updated from the pre-texture-reservation spelling to the equivalent current `cap-retained` expression; production shared-budget code is unchanged.

Commands from the game directory:

```powershell
& '_codex_work/keo-resource-cache-20261114/leopard/test_resource_geometry.cmd' Od
& '_codex_work/keo-resource-cache-20261114/leopard/test_resource_geometry.cmd'
python '_codex_work/keo-resource-cache-20261114/leopard/test_resource_geometry_native.py'
& '_codex_work/keo-resource-cache-20261114/leopard/test_resource_geometry_gpu.cmd'
python '_codex_work/keo-resource-cache-20261114/leopard/test_resource_geometry_mutants.py'
```

## NPC/body investigation retained as read-only findings

Steam buildBody `538C39..5392DE` reconstructs/sorts race and character animation-file references and checks `resourceExistsInAnyGroup` at `539200` for each body. However, actual Ogre Skeleton::addLinkedSkeletonAnimationSource `3454A0` already compares linked names at `345510..345562` and returns on an existing name before load/setup. OldSkeletonInstance `1E6A60..1E6AFD` delegates nonnegative scales to its master skeleton. A second generic linked-animation cache would duplicate existing reuse. The remaining existence searches are an identifiable repeated-work seam, but their cost and safe invalidation across mutable roots/files are not established; no change was made.

Stock morph lookup `7C030` already caches derived meshes and preserves deliberate player uniqueness and native RNG behavior. createMorphedMesh/switchBodyMesh mutate scene/resource/buffer state. Existing attachment slow records do not establish pose arithmetic as their cause. No whole-NPC worker proposal is supported by this audit.
