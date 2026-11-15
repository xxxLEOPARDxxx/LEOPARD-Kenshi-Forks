# Private narrow texture capture coverage audit, 2026-11-15

**Confirmed:** the two current capture hooks omit the ordinary asynchronous ResourceLoader 2D image-completion route. Existing native getTexture output is a suitable future observation boundary; it is already hooked for timing, so coverage does not require another competing native detour. This is a coverage finding, not proof that retention caused or prevented the owner's stutters. No behavior/runtime code, settings, DLL, process, UI or live memory were changed in this audit.

## Actual path, resolved from pinned images

The executable SHA256 is504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e. `../build/narrow-texture-coverage-proof-20261115.json` contains all three module hashes, asserted virtual-call sites, actual renderer vtable/import-thunk resolution and the existing output seam's exact32bytes. `verify_narrow_texture_coverage_20261115.py` completed with PASS; this reads files and does not open a process.

1. TextureLoadData::loadImage448360 reads/decodes the requested file into its owned Ogre::Image+58 (openResource44840C, Image::load44850A). This is the ResourceLoader image-preparation route, not TextureManager::load.
2. ResourceLoader::updateMT44BBA0 dispatches the completed request's virtual getTexture at44C58B, using caller-owned TexturePtr output storage in RDX. It subsequently binds that returned pointer to waiting TextureUnitState objects at44C5F1.
3. TextureLoadData::getTexture44ACD0 calls TextureManager virtual+118 at44AD74. Actual TextureManager vtable6A69A8+118 is **createOrRetrieve3795D0**, not the hooked load379890 at+128. It requests nonmanual creation; a .dds resource name does not make this path manual or unsupported.
4. For a new/unloaded result, getTexture calls Texture virtual+238 at44ADB8 with the predecoded Image. Actual D3D11Texture vtable7CE08+238 is **renderer47C70, its loadImage override**. This is not renderer47D20, the currently hooked loadImpl.
5. D3D11Texture::loadImage47C70 constructs a one-image vector and calls virtual+248 at47CDA. The actual renderer vtable entry is import thunk5830E -> **Ogre::Texture::_loadImages377650**. That performs GPU buffer/image work and calculates resource mSize at37810D. This route never needs D3D11Texture::loadImpl47D20.
6. Back in getTexture,44ADC4 invokes virtual+D0. The actual renderer entry is thunk57738 -> **Resource::setToLoaded66740**, whose body sets loading state2. This slot is not _fireLoadingComplete. The output therefore has its LOADED state published before getTexture returns normally; failure cases still require ordinary loaded/identity eligibility checks.

The decisive distinctions above were checked against actual vtable contents and import names. SDK comments use other build addresses; they were not treated as Steam RVAs. SDK ResourceLoader.h identifies the Image/name/group fields and getTexture signature; OgreTexture.h describes external loadImage versus internal _loadImages.

## What the new runtime counts establish

Current resource_texture_retention.cpp observes only successful TextureManager::load outputs and D3D11Texture::loadImpl completions, on the main thread. Its final289 observations equal146 manager requests+143 load completions. The old generic Resource::load hook's enormous repeated render-path observation count is gone. That reduction is desirable evidence of narrowed call frequency, not proof of complete resource coverage.

This saved run has859 timed TextureLoadData::getTexture calls while retaining only79 textures/305056536 logical bytes through the current two paths. Not all859 outputs are necessarily unique or eligible DDS; calls, unique textures and avoided loads are different quantities. The native bypass nonetheless proves a systematic coverage gap independently of those counts. The old run's762 retained resources/~3GB versus79/~305MB is consistent with reduced coverage, but the runs are not a matched per-resource inventory and cannot establish the exact missing set.

Texture arrays through448BC0 are a separate virtual override and are intentionally rejected by the current 2D whitelist. Their absence is expected and should not be fixed by widening this file-backed 2D feature to manual/generated terrain arrays. Main-thread-only observation also intentionally excludes worker notifications; this demonstrated gap already occurs in main-thread updateMT completion, so worker observation is unnecessary to explain it.

## Existing future observation seam, no new detour

resource_load_probe.cpp already admits getTexture44ACD0 with its exact32bytes and raw ABI `void* (void* self, void* hiddenOut)`: RCX=TextureLoadData*, RDX=caller-owned16-byte TexturePtr result storage, RAX=same output address. After successful original return, an optional retention observer can validate result==out, nonnull output, creator/manager identity and normal eligibility. The observer must borrow the caller-owned output; it must not destroy, overwrite or invent another value-return ABI.

Capture must depend on the retention setting and owner-thread gate, independently of Debug: the current timing wrapper's debug-OFF early return would otherwise silently suppress capture. Keep measured native time separate from optional observation work. Preserve all native exceptions/returns, existing weak pending ownership and bounded Tick admission; do not move Ogre pointers to the IO worker. Already-admitted native seam ownership must remain unique.

An output event alone does not distinguish a newly loaded texture from a native cache hit; it must not automatically be reported as an avoided reload. Existing retained-resource reload/accounting behavior needs explicit tests if this observer is added. Useful controls are: actual createOrRetrieve -> D3D11loadImage -> setToLoaded completion is observed with both current capture hooks absent from that path; debugOFF still captures when retentionON; retentionOFF forwards without observer work; failed/unloaded output is not pinned; original ownership and exactly-once forwarding survive exceptions; arrays remain excluded.

## Current decision

Do not implement the coverage addition before the owner's prepared one-key OFF A/B on the current DLL, same save and route. The large initial update/attachment record and subjective worsening do not prove a retention regression; neither startup timing, effects nor this uncovered loading path establish causality. Startup/RAM/geometry acceptance and their settings remain separate. This report provides the concrete future coverage fix boundary while preserving that A/B isolation.
