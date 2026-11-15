# Save/NPC resource-byte startup coverage - 2026-11-15

Stage-only implementation. No game run, process/memory inspection, capture,
profiler, new runtime collector, settings edit, DLL build/install, checkout edit,
commit, release preparation or publication was performed by this task.

## Source-proven gap and change

The previous save-load report already identified that `ScanStep` admitted only
textures, while the real archive consumer accepts textures, `.mesh` and
`.skeleton` files. A completed startup texture pass therefore did not mean any
model/skeleton bytes had been pre-read. The fail-first actual-production fixture
registered a texture directory before a model directory and expected three
files / 48 bytes; the previous implementation failed that assertion in both VC10
`/Od` and `/O2`. No save-load time or NPC-stall reduction is inferred from it.

The same existing low-priority IO worker now collects model/skeleton paths into
one bucket and texture paths into another. Their **combined** size remains at
most 262144, with the same case-insensitive duplicate set. It completes the scan,
then reads model/skeleton bytes first regardless of discovery order. It does not
call Ogre, load model resources, construct NPCs, parse models, share materials,
change RNG/morph/animation/physics, change native readiness or add a hook. The
only extra persistent worker bookkeeping is a second bounded vector; total
candidate strings/duplicate-set cardinality remain bounded by the previous cap.
There is no new frame/per-NPC traversal, cache, lock, engine-pointer ownership,
timer, log, worker or queue.

The legacy `resourceStartupTextures` key/default is retained. The EN/RU/ZH
option/help and HUD now consistently say resource warmup/scanning resources,
including the explanation that models/skeletons are read first. README includes
the same explanation and limits. Local MO catalogs are under
`build/startup-model-locales`, never installed by this task. No personal INI was
edited. RAM 16 GiB, gameplay read 128 MiB/s and shared GPU 8 GiB values are unchanged.

Startup still ends at its **first** capacity refusal and never evicts existing
entries to scan the library. Same native-demand preemption, speculative pause,
menu lease, 256 KiB chunking, memory reserve, 256 MiB per-file maximum, file identity,
root/reparse exclusions, cancellation/epochs and existing-reader lifetime apply.
Both buckets and their copied paths release on reset, terminal completion,
disable and shutdown through the existing worker lifecycle. This can prepare
model file bytes before a texture-filled startup budget; it cannot remove save
deserialization, native world restoration or first per-instance appearance work.

## Verification

- `python leopard/test_resource_startup_models.py --mutants`: 29 actual-production
  checks each `/Od` and `/O2` PASS; six separately compiled `/O2` mutations produce
  runtime RED, never accepted compile failures. Controls cover missing admission,
  incorrect texture-first order, models escaping the combined inventory cap,
  retained model bucket after reset, early texture-only termination and startup
  evicting previously read model files. The 32-byte cap test retains both 16-byte
  models before refusing the texture, remains terminal and preserves a live
  reader after index clear. Pause/epoch cancellation and extension exclusions
  also execute production branches.
- `leopard/run_resource_startup.cmd [Od]`: actual filesystem worker suite PASS
  `/Od` and `/O2`, updated to include both model extensions; tests registered-root
  aliases/duplicates, invalid/missing roots, recursion, reparse-file/junction
  exclusions, global limits, zero-byte mesh/oversized skeleton rejection, 4 MiB
  model read preemption by native demand, terminal capacity/no eviction, pressure,
  off/re-enable, late roots, existing cached skips and stop. Observed-demand test
  uses a separate file outside the startup roots, so model-first admission does
  not turn its preemption check into an already-cached hit.
- `leopard/test_resource_menu_startup.cmd`: 46 actual production checks each
  `/Od` and `/O2` PASS; `python leopard/test_resource_menu_mutants.py`: all 12
  compiled existing menu/pacing/demand/cancel/worker negative controls runtime RED.
- `leopard/run_resource_cache_test.cmd Od`: actual worker IO, atomic file identity
  replacement and pinned reader lifetime suite PASS.
- `leopard/run_resource_cache_native.cmd`: real installed Ogre FileSystemArchive
  hidden-return ABI and retained CachedStream read/lifetime PASS using a `.mesh`
  path. This tests immutable model **bytes** through the actual archive boundary,
  not valid mesh decoding or creation of a live game Entity.
- `python leopard/test_resource_warm_hud.py`: 43 actual formatting/layout checks,
  38 actual runtime/lifecycle controls, eight compiled expected-RED controls and
  native SDK HUD translation-unit compile PASS. Existing vanilla/Dark UI text
  skin and font contracts inspected; no game visual/performance claim.
- `tools/localization/compile_catalogs.py --output build/startup-model-locales`:
  all three 129-key catalogs compile and roundtrip. New labels resolve correctly
  in the resulting MO catalogs; every Russian translation passes the forbidden
  U+2010..U+2015/U+2212 dash check.

Production source/PO/README and local MO SHA256 are frozen in
`build/startup-models/source-manifest.json`. Existing host golden INI files keep
the same keys/defaults. No hard-coded old UI labels were found in host metadata
fixtures; root owns the final combined host gate and independent review.

## Why no additional native NPC change

Existing pinned native reports prove that nonunique morphed meshes and item
materials already have native name-based reuse. Player mesh uniqueness, mutable
material tint, RNG side effects, skeleton/scene/physics ownership and per-entity
callbacks prevent substituting shared whole-NPC objects. The saved 41 ms attachment
callback can contain synchronous texture/material/state waits; actual loaded
textures and final morph meshes already receive bounded retention. There is no
evidence isolating another safe native rewrite. No new speculative native seam
or worker offload was added to pretend that the remaining stutter is solved.

Eight failed-test fixture directories contain 512 bytes total of owned tiny
inputs. Automatic approval review rejected the PowerShell cleanup command with
the stated reason `blocked by policy`, before its manifest/deletion could run.
Those inputs and all verification evidence remain under stage/build; no cleanup
or reclaimed size is claimed.
