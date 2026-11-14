# Forward terrain preparation - private local candidate, 2026-11-14

No published release or version bump. Retains the existing resource-file RAM cache and all unrelated KEO settings/patches. Not a public announcement or supplied release README.

## Scope and settings

Three live MCM controls under Resource memory: Prepare nearby terrain (default OFF), Terrain VRAM warmup (default OFF), VRAM warmup limit 1-8 GiB in 1 GiB steps (default1). Requires the existing RAM cache, which remains 1-32 GiB. EN/RU/ZH catalogs have98 translated keys. Debug remains last. No new settings require restart. The actual OS/driver memory budget and RAM reserve override configured maxima; limits are not preallocations.

Preparation predicts nearby camera cells and movement direction, plus at most four already-watched moving player characters near the camera. No extra zone/NPC activation. A guarded main-thread metadata collector copies one biome's owned filenames per eligible50ms tick. A bounded queue (32cells) replaces whole-world scans. Background IO is the existing low-priority paced worker. This stage covers six-layer diffuse/normal terrain arrays, not new actors, city meshes, physics or every resource in a zone.

## Actual GPU reuse

The driver must support concurrent ID3D11Device creation and must not be SINGLETHREADED. A separate low-priority worker creates exact BC3/BC1 six-layer arrays from immutable cached DDS bytes. No immediate-context or Ogre/game-pointer worker calls. Legacy DDS dimensions/mips/full payload are validated. GPU standby is bounded by1-8GiB,128entries,32queued jobs and DXGI adapter3 local-memory headroom. RAM pressure also releases standby GPU objects and DDS pins. Missing OS budget capability or unsupported data/device falls back to native loading.

Verified Steam native array entry A10080, exact64byte prologue; render module timestamp5ca5f9e9/imageBF000 and getter38A20 are guarded. The native hidden-return entry, image decode, materials, SRV creation and manager notification remain native. A main-thread TLS scope admits only matching device+full D3D descriptor. Native receives an AddRef of the actual prepared COM texture, and standby charge detaches exactly once. Each native upload is skipped only on equal full-box/pitch/bytes. A single mismatch abandons the remaining shortcut so the unchanged native writer finishes the array correctly. D3D fast-dispatch vtable rewrites are re-admitted through bounded per-target thunks; no map lookup on ordinary UpdateSubresource forwarding.

The native GPU seam is deliberately not admitted on GOG yet. This candidate does not claim VRAM warmup on GOG; normal loading and prior cache remain available. Failure to admit the consumer/device disables the worker, rather than filling unused textures.

## Scheduling and limitations

Loading, transitions or a frame >=20ms pause new preparation with a1second cooldown. IO can pause between256KiB chunks; driver CreateTexture2D is indivisible. At sustained<=50FPS the conservative gate can postpone preparation indefinitely. This is a pending gameplay-tuning issue, not a promise of a hard per-frame GPU budget or zero FPS loss. Native GPU driver work and byte comparisons can still cost time. Do not claim performance benefit until real queue/adoption and frame-time counters are measured.

World/reset/off invalidates queued generations. Inflight work owns all COM/byte references; no stale game pointers. Stop is signal-only in DllMain and bounded outside it. In-use native arrays remain engine-owned on disable. No DLL unload while hook thunks exist; KEO is already pinned.

## Verification before install

- Native DEV and PROD359sources built /O2 /GL /LTCG, required x64 startPlugin export;181KenshiLib imports compatible with0.5.0; CRT MSVC100 only.
- Collector397pure+36nativeSDK fixture checks; forward-cell/cooldown/key and descriptor/byte-match production policies PASS.
- Fresh RAMstore: identity/replacement/cap/pressure/readers plus queued pause/resume, actual mid-read pause/resume, cancellation while paused, allocation returns tozero. Real Ogre archive ABI and retained stream suite PASS.
- Real WARP GPU readback validates every compressed byte of allsix layers and selected native mips for BC1/BC3. Worker, RAM/VRAMpressure, cap/eviction, queue, cancel/reset, device refusal, refcounts/adoption, bounded stop tests PASS inO2/Od. Eight negative controls fail expected assertions.
- Native bridge fixture invokes actual production TLS/COM hooks. Native receives the identical prepared texture; matching uploads suppressed; changed bytes use native uploads and GPU readback is correct. Runtime vtable rewrite regression covered.
- Three MCM suites PASS (32rows/defaultOFF/live controls/1-8range); adapterDEV/PROD and six behavior mutants+two expected native-fault controls PASS.
- EN/RU/ZH MO98keys roundtrip; RU strings checked for unsupported dash characters. No personalINI or debug settings distributed.
- Independent review: cache eviction released too late, key size, RAMpressure coupling and camera scheduling findings fixed. Final consumer-admission/device-release delta reviewed; no remaining Critical/Important.

Pending: actual-game device/metadata admission, GPU created/adopted/skipped counters, first-visit vs revisit comparison, sustained frame-time and memory behavior. Tests do not prove absence of any leak or stutter. No Nexus/Steam publication and no ALL_CHANGELOG_MODS entry.
