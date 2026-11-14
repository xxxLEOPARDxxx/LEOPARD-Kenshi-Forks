# Loaded texture retention module - 2026-11-15

Independent staged implementation; root owns configuration, shared GPU admission,
integration, full builds, review, commit and installation. No publication/version
change or gameplay performance claim.

## Contract and scope

`Resource::load` resolves the actual named Ogre export at RVA `270650`, admits
only stamp `5ca5f929`, image size `9c9000`, exact 32-byte prefix and expected
export address. GOG is refused. It calls native exactly once with the unchanged
background argument. Native exceptions propagate; optional successful capture
cannot replace native success. Both wrapper and observer check owner identity
before any mutable State read; workers forward native without inspecting State.
Only on the owner with retention enabled does the wrapper capture pre-native
isLoaded, allowing already-loaded requests to differ from explicit reloads.

Only TextureManager creator identity is accepted before any Texture cast. Capture
stores at most 128 handle/raw-identity/time triples, no native controlling pointer,
filesystem search, heap allocation, or texture size query. Each250ms Tick resolves
manager handle+pointer+creator again; at most four candidates are processed and
two held entries refreshed. Unloaded pending observations expire after30seconds.

Eligible resources are already loaded, nonmanual `.dds` textures (case insensitive),
actual2D, depth1, static, non-RTT and nonmultisampled. Width/height1..32768,
actual mip count<=15, known format and nonzero native size are required.
Native `TextureManager::createManual` passes manual=true and is excluded.
The normal native nonmanual DDS loader provides the file-backed provenance;
there is deliberately no `resourceExists` archive search across hundreds of roots.
A custom nonmanual DDS-named resource populated via an unusual external loading
path is not distinguishable by this metadata filter. The cache observes existing
loads, never invents filename requests or forces a resource load.

Native Texture::calculateSize uses base-level PixelUtil bytes plus CPU/object
metadata, rather than the full mip chain. Reservation is the conservative maximum
of native Resource::getSize and native Image::calculateSize using actual loaded
dimensions/format/mips. This is logical accounting, not measured WDDM residency.
No getBuffer or unsupported vtable index is invoked. Native Texture has an extra
writeTextureArray slot after the accessors used here; this code does not reach it.

Actual TexturePtr acquisition for a persistent entry follows shared budget admission;
up to1024 held entries. Temporary manager ResourcePtr resolution stays within Tick.
LRU admission evicts one entry if needed for the entry ceiling, then at most two
additional entries for byte admission per candidate. Pressure
releases at most32 own references per heartbeat, leaving native unloading/removal
authoritative. Lowered reservation publication is supported even under pressure.
OFF/world/device/shutdown Clear releases all own refs while Ogre is alive.
State is intentionally process-lifetime so DLL detach cannot destroy Ogre objects.

`reused` is a repeated successful native load request for the same retained texture
that was already loaded before the call. It proves a native consumer reference hit; it does not
prove an avoided upload or elapsed-time improvement. Native explicit reload/unload
causing unloaded->loaded through the captured Resource::load immediately releases
the old pin/reservation, then weakly queues the new identity for bounded Tick
readmission at its new size. Such reloads never increment reused. Other direct
metadata changes, explicit unload and manager removal are detected by bounded
refresh. At1024 entries, a complete refresh cycle can take128seconds for those
paths that do not go through the captured native load transition.
No live CPU/FPS/VRAM/leak conclusion follows from these fixtures.

## Verification evidence

- Fail first: native Texture load/getSize/calculateSize succeeded, then the
  production-inclusion fixture failed specifically at missing retained texture.
- Final VC10 `/Od` and `/O2`: **77 checks each PASS**, real installed Ogre DLL,
  native Resource/Texture/TextureManager subclasses and SharedPtr control blocks.
  Only owner/budget boundary functions are substituted; no replacement Ogre SDK.
- Actual native purge: one persistent TexturePtr increments useCount by exactly1,
  prevents unused resource unload; releasing it restores native purge. Weak pending
  observations do not increment useCount or prevent purge.
- Forwarding once, unchanged bool, same native thrown value, optional capture
  exception isolation, off-owner and nontexture rejection; stale handle/pointer,
  manager removal, timeout, manual/dynamic/RTT/FSAA/shape/zero-size exclusions,
  native mip accounting, size refresh,1024/LRU, queue128, bounded4/2 cadence,
  pressure32, world/device/OFF lifetime controls pass. Review regressions were
  observed RED first: PAGE_NOACCESS State probe catches off-owner reads; actual
  native reload test catches stale reservation before wrapper return. Reload
  readmission uses the new size, excludes overcap textures and preserves no-pin
  pending behavior/refcounts and unchanged reused counter.
- Offline actual images/export/size/vtable/purge: **26 checks PASS**, hashes
  Steam `504b362c...502cd3e`, Ogre `7d3888d2...df5f6`.
- Eleven independently compiled `/O2` broken-production controls all expected RED:
  missing controlling reference, ignored manager identity, manual admission,
  pressure/world/device ignored, duplicate native call, omitted mip accounting,
  observer/wrapper worker reads of owner State and reload counted as reuse.

Commands: `leopard/test_resource_texture_retention.cmd Od`, same with `O2`, and
`python leopard/test_texture_retention_native.py --mutants`.
The renderer-only native createManual entrypoint dereferences RenderSystem
capabilities before creation; it cannot run in a no-renderer fixture. Instead the
fixture uses actual native manager.create(...,true,loader), while offline bytes
verify createManual passes that same true flag. No renderer is fabricated.

Files owned by this module task: `src/resources/resource_texture_retention.{h,cpp}`,
`leopard/test_resource_texture_retention.{cpp,cmd}`,
`leopard/test_texture_retention_native.py`, this report. Test outputs are under
`build/texture_retention_test`; they are disposable own build artifacts.
