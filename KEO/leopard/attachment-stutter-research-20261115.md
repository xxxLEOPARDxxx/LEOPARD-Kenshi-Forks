# Private attachment stutter audit, 2026-11-15

## Finding

A concrete texture-residency opportunity exists beyond the accepted mesh cache: attachment material assignment has a synchronous Ogre texture-loading path outside the existing TextureLoadData::getTexture timing probe, and native Entity cleanup removes textures only below a reference-count threshold. Bounded ownership of actual completed file-backed TexturePtr objects can therefore preserve eligible textures through normal cleanup without bypassing native load/readiness. This is a supported optimization candidate, not proof that texture reload/upload caused the measured41ms callback or that retaining it will remove that spike.

The saved `runtime-appearance-preparation-first` run is the reason to examine this path: update239.813s42104us / attachment41007us / callback42038us, and237.814s23634us / attachment21756us; morphCreate and poseBlend are zero in these records. The entire run has poseBlend max927us. Moving pose arithmetic to workers would target work absent from these stalls. morphLookup refusal remains untouched; no unknown detour is overwritten.

Offline pinned Steam executable SHA256 `504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e` and installed Ogre were inspected as files only. Names use actual KenshiLib exports plus Steam7630-slot table or actual Ogre exports, not SDK address comments. `../build/attachment-seam-descriptors-20261115.json` records exact32prefixes, module hashes, SDK signatures, and assertions for the relevant Ogre virtual slots. No runtime source, settings, DLL, process, game input or memory were touched.

## Exact callback path

AppearanceBase::createEntityCallback536590 validates live body and attachment identity, clears the one-shot binding, checks mesh tangents through anonymous helper83F0D0 at5366CB, performs a special mesh preparation for native itemType111, then:

* attachEntity52C9F0 at536723. This attaches to a bone or scene node, shares the body's skeleton for the appropriate attachment type, and changes visibility; it is mutable scene/instance work.
* Branches by itemType6: setupHairMaterial531B70 at536776, otherwise setupItemMaterial535780 at53677D.
* Copies the resulting material name into AttachedEntity+88.
* updateWetness(true)52B9C0 at5367B5, plus item/node visibility callbacks.
* affectsPortrait and updatePortrait52B110 at53680C/536818. updatePortrait only checks player ownership and calls GameWorld::addPortraitUpdate at52B14C; it does not render a portrait inline. NPC callbacks fail the player check.

The tangent helper only builds tangents when they are absent on an already-loaded mesh; otherwise it returns quickly. No `missing tangents` message was found in this snapshot's RE_Kenshi_log.txt. This does not measure its duration, but gives no present reason to optimize that helper.

updateWetness has chained unwind fragments: bounded52B9C0..52BD09, not just the first59bytes. It updates waterline/wetness state, writes custom render parameter1 to body subentity0, then iterates existing attachment nodes and writes the same parameter to each subentity0 at52BCA0..52BCE5. Calling force once per completed attachment repeats a small whole-attachment traversal; blindly coalescing it changes when a new attachment receives its wetness and changes fade progression. No evidence places41ms here. Leave it unchanged unless measured.

## Material preparation and the synchronous texture consumer

setupHairMaterial looks up its saved material name at531E55; otherwise it clones `Hair` under a unique `Hair<N>` name at5320B5. It assigns this material to Entity at532176, then gets pass0/diffuseMap and calls ResourceLoader::loadTextureUnit at532376. It sets per-appearance color/texture controls and can update body hair/beard maps through setHairTexture5305B0. The unique material is mutable: sharing one cached material between unrelated characters would mix color/appearance state.

setupItemMaterial uses an anonymous native helper840B80 through1D633/446AC0. That helper already builds a material key and calls MaterialManager::getByName841291; hit skips clone/setup through84293C, miss clones the template8413B9 and configures texture units and GPU programs. Thus a second generic material-construction cache would duplicate existing native reuse. setupItemMaterial subsequently applies per-item/faction tint, color maps and scale; reuse requires all mutable dependencies, not just mesh or base material name.

ResourceLoader::loadTextureUnit449F30 is not an unconditional synchronous decode: it first calls resourceExistsInAnyGroup449FB5, scans pending requests, looks up TextureManager's existing resource44A206, checks isLoaded44A21B, and either binds an already-loaded texture44A2CF/44A330 or enqueues native TextureLoadData44A3D1. It already reuses loaded textures and pending requests. An unbounded duplicate filename cache is unjustified. The existence check precedes loaded-resource lookup, so repeat metadata searches are an identifiable smaller candidate, but caching results across mutable resource groups/files needs explicit invalidation and has no measured time here.

Separately, the material assignment itself can synchronously request textures:

1. Entity::setMaterial OgreC0FD0 iterates SubEntity::setMaterial368270.
2. SubEntity at36844F invokes material virtual+70 = Resource::load270650(false). Actual Material constructor vtable68A028+70 is asserted to contain270650.
3. Material::loadImpl156660 -> Technique::_load372D70 -> Pass::_load214B10 -> TextureUnitState::_load37DA80.
4. `_load` calls ensureLoaded37DEA0 per frame. If its frame TexturePtr is absent, ensureLoaded37DF98 invokes TextureManager virtual+128 = named TextureManager::load379890. Its actual vtable6A69A8+128 is asserted. TextureManager::load37993B calls returned texture Resource::load(false).
5. If a frame TexturePtr is present, ensureLoaded37E026 still invokes Resource::load(false), allowing an unloaded existing texture to be loaded through the native state machine.

Resource::load has state/locking/wait handling and calls the resource load implementation. Pass::_load also loads GPU programs. Therefore elapsed time here could be image decode/upload, backend waits, material/shader work or synchronization; it is not proven GPU upload time. Existing getTexture timings do not include this independent Ogre path. RAM file-byte hits alone cannot avoid native texture creation, decode, transfer or state waits.

## Native cleanup proves reference-aware retention is possible

ResourceLoader::destroy(Entity)44A4C0 collects a material, then checks material count against RESOURCE_SYSTEM_NUM_REFERENCE_COUNTS+1 at44A67B..44A68E; only the eligible unused-material case continues into texture cleanup. It snapshots texture references from pass0/contentType0 via `_getTexturePtr`44A6EF/44A6FE, removes/unloads that material, then checks each texture's reference count at44A7B3..44A7CC.

If texture count > SYSTEM+2,44A7CC jumps past deletion. Otherwise it calls TextureManager::remove(handle)44A7F1 and unload(handle)44A81C. The slot identities are independently asserted from TextureManager's constructor vtable: +78 = remove2937A0; +48 = unload2932A0. The on-disk initial SYSTEM variable is3, so the image's nominal comparison is>5; live memory was not read and the implementation must use actual SDK/native ownership rather than hard-coding a guessed count.

An additional strong TexturePtr influences this existing native gate. Unlike a bare D3D pin or MeshPtr, it can preserve the named Ogre texture resource for later TextureManager lookup. The expected baseline includes manager/group, material texture-unit and cleanup temporary ownership; confirm the real retained case takes the skip path before claiming coverage. A pin that leaves count<=threshold can still be removed from the manager, creating an orphan retained object that won't help name lookup. Do not patch the threshold or skip native cleanup wholesale.

## Smallest useful optimization direction and limits

Extend retention to actual completed, native-owned file-backed textures already consumed by normal loading. Capture on the owner/main thread after successful native completion or from a completed attachment's already-loaded texture units, with a strict whitelist and bounded enumeration. Never trigger extra loads to fill the cache. Capture actual TexturePtr ownership, not material filenames, raw pointers or only D3D interfaces.

Use the existing shared VRAM ceiling and pressure/disable/world/device cleanup contract; charge each underlying resource once and prevent double charging with existing geometry/array entries. Admit only supported loaded file-backed textures and stable identity/group; exclude manual/generated arrays, render targets, compositor/shadow/portrait outputs and per-character generated maps. Bound pending observations and entries. Do not share materials or Entity instances, retain character pointers, or bypass native callbacks/RNG/readiness. Preserve gameplay IO settings and native scheduling. An incremental retention feature should remain independently reversible until gameplay acceptance.

This can help revisits and repeated file texture use after cleanup. It cannot eliminate first encounters, new generated material creation, skeleton sharing, per-item tint setup or shader compilation. Existing native material/texture reuse means gain depends on actual eviction/reload, so evidence must show retained resources surviving cleanup and actual later native resource reuse; a cache admission counter or reference hit alone is insufficient. Device pressure and bounded memory growth need real gameplay measurements.

## Only if another attribution run is necessary

The attached JSON provides three precise optional measurement boundaries: setupHairMaterial531B70, setupItemMaterial535780, and Ogre TextureUnitState::ensureLoaded37DEA0. First two are `void(self, AttachedEntity*)` (RCX/RDX); ensureLoaded is `void(const self, size_t frame)` (RCX/RDX64), with no hidden return. Each has a verified exact32prefix and symbol; native functions include Klib slots.

Numeric main-thread/debug-gated timings can distinguish hair versus item preparation, with ensureLoaded child deltas measuring the direct synchronous texture leg. Keep callbacks forward-once and exception behavior unchanged, with no native object retention or loading getters inside the timing wrapper. Calling `_getTexturePtr(frame)` from an ensureLoaded hook can re-enter ensureLoaded; it is not a safe casual collection method without explicit nonrecursive design. No broad resource or renderer hook set is proposed. Preserve morphLookup refusal. Do not introduce a behavior-changing optimization solely because a callee is reachable from this path.
