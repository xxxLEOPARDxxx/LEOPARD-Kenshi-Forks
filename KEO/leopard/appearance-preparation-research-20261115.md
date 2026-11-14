# Private KEO appearance preparation investigation, 2026-11-15

Offline/read-only investigation of pinned files. No game input, process/memory access, runtime source edits, settings changes, installation or publication. Generated scripts, disassembly excerpts and this report are private investigation artifacts in the existing stage. These findings establish call behavior and possible measurement boundaries, not elapsed time of an unmeasured callee.

## Evidence and scope

RE_Kenshi/Kenshi_x64.exe SHA256 `504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e` is asserted by the inspection script. Names are resolved from actual KenshiLib named exports, six-byte stubs beginning RVA15B46 and the 7630-entry Steam_1.0.65.br table, rather than treating SDK address comments as Steam addresses. Ogre named exports come directly from installed OgreMain_x64.dll; hash and exact first32bytes are recorded in `../build/appearance-seam-descriptors-20261115.json`.

Bounded inspection covers getMorphedBodyMesh7C030, createMorphedMesh71A80, switchBodyMesh71920, buildBody538590, setBodyEntity5B8910, buildAttachments537BA0, and Ogre Mesh::clone1A3EE0/softwareVertexPoseBlend1A9BE0. The saved excerpts are `appearance-{native,ogre}-<RVA>-20261115.txt`. setBodyEntity has chained .pdata fragments: its initial unwind fragment ends5B8938 but the normal function return is5B8B97; the bounded excerpt deliberately extends through5B8C10 and should not be mistaken for a single function size.

Current saved runtime has meshCompletionCallback installed and appearanceBody uninstalled. The callback aggregate covers body and attachment consumers; an expensive generic callback does not establish slow morphing. In particular the parent identified route sample168.046 where callback21410us includes attachment20228us; no body-only explanation should be assigned to that sample. Keep initial save86-99s separate from later route samples. Nested inclusive timings must not be summed.

## What the native morph path already does

buildBody calls getMorphedBodyMesh at5387B7. Its alwaysUnique argument is the actual return of Character::isPlayerCharacter (5386CD -> EDI -> R9D5387A5); ugliness comes from `faces weirdness`; shaved comes from AppearanceBase+168. Thus player uniqueness is deliberate, not an omitted cache opportunity.

getMorphedBodyMesh has existing reuse rather than always rebuilding:

* alwaysUnique skips normal name generation at7C09D ->7C61D. If morph work is required, a global increment generates `_morph_` names at7C6BB..7C725. Do not remove this for player characters.
* Nonunique `morph index` branch builds a derived name from race identity, gender, ugliness conversion and morph index at7C1C5..7C35B. It asks MeshManager through virtual+B8 at7C3C2 before seeding UtilityT RNG and calling randomPoses at7C416/7C426 on the missing-resource path. Caching here must preserve randomPoses side effects and global RNG behavior; it is not a pure lookup rewrite.
* The other nonunique branch iterates the base Mesh's pose list and looks up current appearance pose values, deriving a hash/name at7C430..7C53E. Shaved horn handling may append `SLAVE` at7C601..7C613.
* A subsequent pose scan checks whether all requested pose weights are zero at7C624..7C69F. If no morph is needed and uniqueness is not required, it returns the original entity at7C7F6..7C808.
* At7C73B..7C755 it performs the existing derived-resource lookup. Existing resource -> MeshManager::getByName at7C770 -> switchBodyMesh at7C77C. Missing resource -> createMorphedMesh at7C7EC. Adding an unmeasured second cache risks duplicating this native cache and retaining additional derived resources.

createMorphedMesh does real renderer work: Mesh::clone at71AE1; submesh0 selection71AEF; pose iteration71B40..71CC9; current mutable GameData weight lookups; special shaved horn rules; per-nonzero-pose softwareVertexPoseBlend71CAD; removeAllPoses71CDC; switchBodyMesh71D07. The clone's pose/offset/normal maps, buffers and reference counts are native objects; no borrowed pointer may outlive its owner.

switchBodyMesh obtains the scene manager and invokes its virtual createEntity overload at71957 using the by-value MeshPtr. It detaches the old entity and attaches the replacement at71972/7197E, transfers each subentity's material71994..719D3, and asks ResourceLoader to destroy the old entity71A1F. It destroys/zeros its incoming native-owned16-byte MeshPtr argument71A25..71A5C. The returned Entity is a replacement; caching/reusing one Entity across characters would corrupt ownership/scene/skeleton state.

## Why the native morph entrypoints cannot simply run on workers

Ogre Mesh::clone1A3EE0 registers a new manual resource through MeshManager::createManual1A3FD8 and clones submeshes, VertexData, animation and pose state (1A402F,1A4068,1A435F,1A43FA). It is not just a byte-array copy.

softwareVertexPoseBlend1A9BE0 is a named Ogre export, but is not a pure arithmetic API. It resolves vertex declaration/binding state, locks the destination HardwareVertexBuffer at1A9CB2, adds weighted offsets in1A9CD0..1A9D10 and normals in1A9D91..1A9DCC, and unlocks at1A9E2E. That work occurs once per active pose. SDK OgreHardwareBuffer.h201/255 shows shadow updates or backend lock/unlock; unlock can upload. SDK OgreMesh.h describes the blend as read/write and explicitly suggests suppressing hardware uploads across repeated calls. SharedPtr atomics provide reference counting, not thread-safe scene/resource/buffer mutation.

There IS separable arithmetic in principle: weighted additions to owned position/normal arrays in native pose order. A future implementation would need an owned immutable snapshot of base vertex data and sparse pose offsets/normals, copied current weights and shaved rules; worker-only computation into owned arrays; then native/main-thread resource creation and buffer commit, with generation/identity/lifetime validation and stock fallback. Capturing or committing buffers can itself cost time, and preparing future character weights before native RNG/appearance selection is not currently justified. No worker-ready native entrypoint was established.

Remaining body work also mutates live objects: skeleton bone state and transforms538A94..538BF4; animation-file collection from race and character data538C39..5390FB; qsort539136; resourceExistsInAnyGroup539200 and skeleton virtual+22053921B per animation file; setBodyEntity539314; ragdoll update539358; virtual material/proportion setup539363/53936C; buildAttachments539372; blood/visibility updates. AnimationClass::setBodyEntity clears animation layers, tears down previous ragdoll, changes body/skeleton, calculates feet height and initial bone world positions. buildAttachments manages physics attachments, hair materials, entity creation and proportions. None can be wholesale moved to an arbitrary thread.

## Bounded next measurement and decision rules

Exact module identities, prefixes, names, table slots and raw ABI are in `../build/appearance-seam-descriptors-20261115.json`. Forward once; retain stock exceptions and ownership; no native pointer retention, allocation, names, logs or INI access in wrappers. Debug OFF must remain immediate forwarding. Use existing main-thread/debug gates and fixed counters; report correlated child deltas in the existing slow-update record, not only cumulative maxima.

The requested six seams split CPU/renderer preparation from per-entity finalisation in one normal run: getMorphedBodyMesh (whole path), createMorphedMesh (miss), softwareVertexPoseBlend (per-pose buffer work), switchBodyMesh (replacement Entity), setBodyEntity (animation/ragdoll/bone instance), buildAttachments (attachment preparation). Nested timings are inclusive: create includes pose blend and switch; morph includes create/switch; callback includes all. Do not subtract unrelated independent-window maxima.

Classify native paths with fixed per-thread/per-nesting flags inside a getMorphed invocation: nested create => miss; switch without create => native cache reuse; neither => unchanged entity/no morph. Count alwaysUnique separately from its raw argument. Do not infer every call lacking create is a cache hit. Do not inspect keys or add branch detours solely for these counts. Ensure nested exception cleanup cannot leave scope state armed.

Optimization candidates, only after measurements:

1. If createMorphed and pose blend dominate measured route callbacks, investigate batching/suppressing repeated uploads on supported shadow-backed destination buffers, with restoration even on exceptions, and actual backend/device evidence. This may save more than parallelizing a small arithmetic loop. No implementation is yet established safe.
2. If pure weighted arithmetic dominates after separately measuring buffer lock/unlock, consider an owned CPU preparation job. Existing complete native calls cannot be its worker body. Preserve floating-point order/precision, normals, shaved rules, per-character uniqueness and RNG.
3. If switch dominates both cache hits and misses, morph-result caching alone cannot eliminate per-entity creation/skeleton/material work. Target Entity preparation only after its contract and timing are known; do not share Entity instances.
4. If setBodyEntity, skeleton animation linking, or buildAttachments dominate, do not spend effort on morph arithmetic. The repeated race/gender animation-file resolution and pose metadata could be immutable-preparation candidates, but keys/invalidation must also cover character animation overrides, resource groups/mods, worlds, save/load and appearance edits. Actual mutation/attachment remains on its owner thread.
5. An attachment callback already explains one provided route spike almost entirely. If the new morph counters are absent from similar spikes, extend only the attachment-specific measurement subsequently; do not announce morphing as the cause.

No arbitrary threading, batch/readiness modification, world/NPC activation, cache budget change or FPS/no-leak claim follows from this investigation. Hardware-buffer upload costs, worker snapshot/commit costs and actual FPS/memory effects remain unmeasured.
