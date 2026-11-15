# Loaded texture retention module - 2026-11-15

Independent staged implementation; root owns configuration, shared GPU admission,
integration, full builds, review, commit and installation. No publication/version
change or gameplay performance claim. The loader-output coverage fix is prepared
in sources only. This module task does not install a DLL or edit personal settings;
the owner's texture-retention setting remains OFF for the existing trace baseline.

## Contract and scope

The first live run disproved the suitability of generic `Resource::load` capture:
225seconds contained162,891,634 observations and129,371,281 claimed reference
hits. That boundary is called by normal render passes, not just texture requests.
The current implementation completely removes that hook. Immutable evidence is
`runtime-texture-retention-first/analysis.json`; no isolated CPU cost or FPS delta
is inferred from those counts alone.

Two narrow boundaries now forward original exactly once, preserving native
exceptions and result ownership:

- Ogre `TextureManager::load` named export RVA379890, stamp5ca5f929/size9c9000,
  full32bytes and native vtable6A69A8+128 are verified. Its raw x64 ABI is
  `TexturePtr*(self, TexturePtr*out, const String&name, const String&group,
  TextureType, int mips, float gamma, bool alpha, PixelFormat, bool hwGamma)`.
  RDX is caller-owned hidden result storage; RAX returns that exact address.
  The wrapper observes the existing result without a controlling pointer copy.
- Ogre `Texture::loadImpl` is pure and has no exported implementation. The actual
  D3D11Texture method is renderer RVA47D20, `void(Texture*)`, vtable7CE08+58.
  Renderer stamp5ca5f9e9/sizeBF000, full32bytes, vtable target and RTTI COL88248 /
  type descriptorB22C8 / exact class name are verified. Its successful completion
  releases any held old pin/reservation and queues a weak identity. Outer native
  Resource::load has not yet published LOADED/new size; bounded Tick waits for it.

The subsequent native audit proved ordinary TextureLoadData.getTexture44ACD0
uses TextureManager.createOrRetrieve3795D0 -> D3D11Texture.loadImage47C70 ->
Texture._loadImages377650 -> Resource.setToLoaded66740, bypassing both hooks.
The new public `ObserveLoadedResult(void*)` borrows that already-existing loader
wrapper's successful caller-owned16-byte TexturePtr output. Root integrates the
notification independently of Debug; the module adds no third native detour.
The helper gates OFF/owner before reading either State or output, rejects null
outputs, validates creator ownership, and queues only scalar weak identity.
For a held result it synchronously checks passive Describe/bytes; changed size or
eligibility releases the old pin/accounting before weak requeue. Metadata exceptions
drop the old own pin conservatively and never escape into native loading. Unchanged
held output remains a reference hit. No SharedPtr copy, native load, filesystem
lookup or allocation is performed by this helper; changed-held removal frees its
existing bookkeeping node.

Both complete contracts are required before retention can enable; a partial hook
failure leaves capture OFF. Missing renderer at startup is a normal wait: bounded
250ms main Tick retries module discovery. A present incompatible module is refused
once with an error. GOG remains refused. An Interlocked-published capture gate
comes before owner checks; OFF wrappers forward without querying owner/State.
Workers never read mutable owner State. Optional observation exceptions cannot
replace successful native return. No per-render-pass generic resource hook remains.

Only TextureManager creator identity is accepted before any Texture cast. Capture
stores at most 128 handle/raw-identity/time triples, no native controlling pointer,
filesystem search or heap allocation. The two hook wrappers do not query texture
size; only the rare loader-result helper revalidates held metadata. Each250ms Tick resolves
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

`managerRequests` counts successful captured owner TextureManager.load calls;
`loadCompletions` counts successful captured owner D3D11Texture.loadImpl calls.
`loaderCompletions` counts nonempty captured owner loader outputs. `observed` is
the aggregate accepted-creator observation count from these boundaries; several
can occur for one native load. `reused` means a manager or loader request returned
the same already-held loaded resource. Actual loadImpl completion removes its old
pin first; loader-result completion removes it if size/eligibility changed.
A same-size external image update can still have uploaded pixels; reference hits
never claim avoided uploads or elapsed-time gains. Bounded Tick admits new/rebuilt
resources at their actual current sizes. Other direct
metadata changes, explicit unload and manager removal are detected by bounded
refresh. At1024 entries, a complete refresh cycle can take128seconds for those
paths that go through neither concrete native loadImpl nor observed loader result.
No live CPU/FPS/VRAM/leak conclusion follows from these fixtures.

## Verification evidence

- Fail first: native Texture load/getSize/calculateSize succeeded, then the
  production-inclusion fixture failed specifically at missing retained texture.
- Final VC10 `/Od` and `/O2`: **120 checks each PASS**, real installed Ogre DLL,
  native Resource/Texture/TextureManager subclasses and SharedPtr control blocks.
  Only owner/budget boundary functions are substituted; no replacement Ogre SDK.
- Actual native purge: one persistent TexturePtr increments useCount by exactly1,
  prevents unused resource unload; releasing it restores native purge. Weak pending
  observations do not increment useCount or prevent purge.
- The generic Resource hook prohibition first failed RED against the old source.
  Final fixture calls real native Resource.load one million times on an already
  loaded texture: no observation, manager/completion/hit counter or native loadImpl
  count changes. Concrete loadImpl is wired through the real Resource state machine
  using a GPU-free SDK Texture subclass; no loaded Texture is admitted before the
  outer native loader publishes LOADED. This verifies capture scope, not game FPS.
- Loader helper fail-first: its empty stub failed specifically at first weak
  admission, with successful native metadata calculation/setToLoaded established.
  Actual native TexturePtr output maintains exact useCount, including a live check
  inside Describe that catches transient controlling copies. First capture stays
  weak; repeat held output, resize/overcap, dynamic/array exclusions, immediate stale
  decharge, null outputs, isolated getter exceptions and PAGE_NOACCESS owner/OFF
  gates pass. This GPU-free fixture calls actual native calculateSize/setToLoaded;
  it does not fabricate a renderer or claim to exercise GPU image upload.
- Real native TextureManager.load verifies all parameters including two independent
  float/bool/format/type combinations and signed mips=-1, hidden output address,
  untouched adjacent sentinels, one transferred native TexturePtr reference and
  unchanged ownership on native thrown exception. No actual GPU renderer is mocked.
- Forwarding once, same native thrown value, optional capture
  exception isolation, off-owner and nontexture rejection; stale handle/pointer,
  manager removal, timeout, manual/dynamic/RTT/FSAA/shape/zero-size exclusions,
  native mip accounting, size refresh,1024/LRU, queue128, bounded4/2 cadence,
  pressure32, world/device/OFF lifetime controls pass. Review regressions were
  observed RED first: PAGE_NOACCESS State probe catches off-owner reads; actual
  native reload test catches stale reservation before wrapper return. Reload
  readmission uses the new size, excludes overcap textures and preserves no-pin
  pending behavior/refcounts and unchanged reused counter.
- Offline actual images/export/size/vtable/purge/loader-bypass: **53 checks PASS**, hashes
  Steam `504b362c...502cd3e`, Ogre `7d3888d2...df5f6`, renderer
  `5f224882ded5b6d3f68f14d6bb4e1762267bbb602b7b9733bd153663d1541bb9`.
- Twenty-four independently compiled `/O2` broken-production controls all expected RED:
  missing controlling reference, ignored manager identity, manual admission,
  pressure/world/device ignored, duplicate native call, omitted mip accounting,
  observer/wrapper worker reads of owner State, reload counted as reuse, corrupted
  manager float/bool/return address, disabled owner query and duplicate manager call.
  The eight loader controls independently catch omitted capture/counter, transient
  output copy, owner/OFF ordering, ignored size/eligibility and escaping observation
  exceptions.

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
