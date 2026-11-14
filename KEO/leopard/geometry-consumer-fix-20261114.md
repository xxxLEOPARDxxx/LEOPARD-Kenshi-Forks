# Private: actual Entity geometry retention and NPC timing, 2026-11-14

Local development only. No release number, publication or player-facing promise
of eliminated stutters. Existing RAM/VRAM settings and locale catalogs unchanged.

## Recorded combined run

`runtime-combined-first/analysis.json` and captured log preserve the original run.
Startup scanned 7,379 texture files, processed all, loaded 7,294, skipped 85;
startupCapped=0. RAM ended at 15,389,173,264 bytes of a 16 GiB budget. This
inventory fits. Skips include existing/unreadable/empty/oversize files, not a
proven partition of the 85 cases. Other observed resources account for the
remaining RAM entries. Processed files are not all simultaneously GPU resources.

Geometry retained 264 models / 121,303,980 logical GPU bytes, with 14,024 rejected
observations and zero retained-reference hits. Zero does not independently prove
GPU reuploads. Existing resourceFinish maximum was 83,630 us; factoryDrain
maximum 88 us measures only its queued drain, not all direct construction.
Completed inclusive elapsed time includes waits, and nested totals cannot be added.

## Confirmed identity error

Game loadModelEntity creates a temporary input Mesh on the background path.
The native name/group collision callback can refuse its registration. Ogre
SceneManager::createEntity(MeshPtr) resolves the name/group again; the returned
Entity can own a different, registered Mesh. Observing the input weak handle
therefore rejects the disposable duplicate and misses retained-reference hits.

After original success, observe the returned Entity's passive getMesh() result.
Do not retain the input, load unseen models, or replace native return/exception
behavior. Fixed counters separate different-input observations, queue saturation,
missing manager/handle, identity mismatch, timeout, unsupported mesh kinds,
missing GPU buffers and byte admission refusal. These are diagnostic categories,
not an exhaustive partition of every later refresh rejection.

Bounds and lifecycle remain: weak pending128, retained2048, 4 observations/250ms,
deduplicated buffers, shared terrain/geometry budget, pressure/world/device/OFF
cleanup. Logical retention does not guarantee WDDM physical residency. The seam
captures background create requests; synchronous entity creation can bypass it.

## NPC/load boundaries

Three optional detailed-log probes cover direct RootObjectFactory::process,
ResourceLoader::SetMeshData and Ogre Entity::_initialise. Exact native build,
32-byte prefixes and Ogre export are required. See native-finalization reports.
No scheduling, readiness, NPC spawning, population or thread-affinity change.
World registration and per-entity skeleton/material setup remain on native paths.
Native completion already batches 20 meshes; lowering this blindly is unsafe
because isLoading does not include the ready-completion list.

RAM reduces file reads, geometry retention may reduce reacquiring shared buffers.
Neither removes save parsing, world restoration or per-character instantiation.
There is no controlled save-load OFF/ON comparison or measured FPS improvement
for this fix yet. Normal-play runtime evidence is the next validation step.

## Verification

VC10 /Od and /O2 actual-Ogre collector/lifetime/purge fixture: 41 checks each.
The returned-Entity selection fixture uses an adapter and real registered versus
unregistered native MeshPtrs, not a SceneManager-created Entity. Selection is
additionally supported by native binary analysis. Compiled mutation of this
helper back to the temporary input must fail. Existing boundedness/purge/cap
negative controls remain. Three-probe production-wrapper fixture: 42 /Od and
42 /O2 controls, independently rerun; disabled/off-main paths, exactly-once,
return/arguments and C++/SEH propagation included. Independent review found no
Critical/Important issue. Full DEV/PROD link and actual DLL imports are checked
separately by geometry-consumer-build-verification.json in the local stage.
