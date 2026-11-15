# KEO phase 2: core/render/resources source audit (2026-11-15)

Scope: staged source after the first nine fixes (`a6fcd84` baseline). The source review was read-only; four findings were subsequently authorized for the narrow stage-only corrections recorded below, no game execution, new measurements, profiler, capture, installation, full DLL build or edits to the seven foreign Steel files. This report does not prove an FPS improvement or absence of all leaks.

All implementation bodies and headers in the nine assigned directories were read. Large files were read in bounded chunks; a later pass omitted blank/comment-only lines but retained original line numbers, with relevant ownership/ABI comments read directly. The coverage table below records every file and its read-snapshot digest, not merely a search inventory. Its digests predate the GPU/bench/settings follow-up corrections and deliberately remain the original reviewed snapshot; final correction identities are listed separately. The other agents' zone/movement/planner/navmesh/game/fixes areas are outside this report.

## Concrete findings

1. **Fixed-log full output was unterminated (corrected, frozen).** `src/base/fixed_log_buf.h` previously called the ordinary append primitive for the terminator; the primitive refuses when `n == cap - 1`. A poisoned buffer filled to its final payload byte therefore escaped as a nonterminated string to logging consumers. The explicit full-buffer terminator preserves append/count semantics. Separate report: `leopard/phase2-fixed-log-termination-20261115.md`; corrected old-code fixture failed first, final VC10 /Od and /O2 passed, reverted /O2 mutant compiled and failed at runtime.

2. **GPU definition pointers can outlive their map nodes (corrected, frozen).** `gpu_params.cpp:120-138` caches maps without checking whether their SharedPtr control block uses the watched native deleting destructor. The existing real Ogre fixture proves SDK-owned map destruction bypasses DAC30, so final params destruction can free a cached map with no invalidation. Separately, the setter's conservative same-pointer invalidation (`gpu_params.cpp:174-181`) drops only lookup entries; `gpu_upload.cpp:295-302` keys its raw-definition upload plan on the deletion count, which does not change for in-place erase/rebuild followed by same-map rebinding. `upload_plan.cpp:66` then dereferences a potentially freed definition node. These are source/lifetime defects, not evidence of a particular observed crash. Implemented: lookup caching and upload plans admit only verified native control blocks, and plans use a separate revision advanced before conservative setter invalidations and actual deletion. The actual deletion counter keeps its meaning. Review additionally caught unknown-map uploads overwriting an existing buffer shadow; these now invalidate copies before native fallback. Actual native/SDK map fixtures pass 36 controls at /Od and /O2 with seven compiled runtime RED mutants; the earlier rebind fixture still passes 38 controls and its six mutants. Report: [GPU map lifetime correction](phase2-gpu-map-lifetime-20261115.md). Independent recheck approved the final fallback correction and independently reran both GPU suites and all mutants.

3. **Unused benchmark resets a foreign clock rate (corrected, frozen; independent review clean).** `bench_runner.cpp:761-762` calls `BenchPinIdleTick()` even when no run has ever been armed; `bench_pin_run.cpp:249-252` rewrites any nondefault admitted clock rate. Admission at startup does not grant ownership of another plugin's later write. Implemented restoration debt only after a successful actual KEO clock write. Idle retries only that debt, clears it after success, and yields to a later differing foreign write. No debt is created by refused/equal/no-write paths; shutdown performs no facade access. Production pin-run fixture: 33 controls /Od and /O2 PASS with six compiled runtime RED mutants. Report: [Benchmark clock ownership correction](phase2-bench-clock-ownership-20261115.md).

4. **Fallback settings panel loses current group gating (capacity corrected, frozen; independent review clean).** `gui/settings_panel.cpp:75` stores only eight gated children, while `settings_layout.cpp:15-24` contains a resource group with nine dependent rows, followed by other groups. `settings_panel.cpp:291-310` intentionally leaves overflow rows enabled; the current schema has exceeded that storage assumption. The visible setting still applies via its normal callback, so this is a UI consistency defect rather than a native ownership issue. Implemented capacity-only derivation from the existing bounded staging domain (`CONFIG_MODULE_MAX * CONFIG_STAGE_MAX`); layout and callbacks stay unchanged. Existing actual-table fixture independently enumerates all sixteen memberships, then tests the actual production capacity and staging invariants. /Od and /O2 PASS; old eight-entry capacity compiled and failed first, and the reverted-capacity mutant remains RED. Report: [Settings group capacity correction](phase2-settings-group-capacity-20261115.md).

## Unproven risks / deliberate boundaries

- Particle policy's double-buffered lookup (`render/particle_levers.cpp:89-96`) has no reader pin or sequence validation across two later owner publications. A sufficiently paused worker could read a reused table. The source itself documents the lack of reader sequencing. No observed failure: require a deterministic paused-reader/two-publish fixture plus native thread/lifetime contract before changing it. The separate timeout records intentionally survive effect-pool reuse and are bounded; they are not reported as an unbounded leak.
- Diagnostic throw-ring publication advances the writer ticket before filling a slot (`diag/throw_ring.cpp`). A death-path drain can miss an in-flight diagnostic record. This is best-effort diagnostic completeness, not demonstrated gameplay corruption.
- RAM cache hit identity checks intentionally touch filesystem metadata; prewarming is not a guarantee of zero filesystem calls. Existing completed RAM content survives world reset, speculative work is paused independently from demand fills, and worker/chunk budgets remain bounded after the earlier fix. No new filesystem timing claim.
- Startup inventory is texture-only; adding ordered mesh/skeleton seeds is a separate feature proposal, not a partial audit patch.
- SDK `Character::isAnimal()` returns `CharacterAnimal*`; the inventory reader's cast is consistent with that contract. MyGUI callback replacement is a value delegate, not a raw delegate pointer; no inferred callback leak is claimed. Save sidecar lifecycle relies on the native nested new-game clear already documented by the hook; no duplicate clear is proposed without contradicting evidence.
- Potential exceptional-allocation logging lock release, plugin/thunk unload lifetime and generic numeric-prefix acceptance were examined but lack a concrete supported failure fixture. They remain risks rather than confirmed fixes.

## Lifecycle and cadence conclusions

Resource queues/caches have explicit limits, weak observations are resolved through managers on the owner, GPU and geometry pins release their own references only, and cancellation/world/device epochs gate pending work. These mechanisms were inspected; static review does not establish a universal leak-free claim. Production diagnostics generally use cached flags and bounded owner cadence; the benchmark clock exception above is concrete. Hook installation remains guarded and no new hook/batch API is proposed. Steel jobs were reviewed but their foreign changes are preserved.

## Per-file coverage

The digest is SHA256 truncated to 12 characters. These are coverage markers from the read snapshot before follow-up implementation, not release identities or current production hashes.

### base (33 files, 3469 lines)

| File | Lines | Digest | Coverage |
|---|---:|---|---|
| `src/base/clock.h` | 68 | `bd76c5a074a4` | Body/header read; lifecycle and cadence reviewed |
| `src/base/config.cpp` | 265 | `9dee1f104f43` | Body/header read; lifecycle and cadence reviewed |
| `src/base/config.h` | 93 | `6d4e45fb8547` | Body/header read; lifecycle and cadence reviewed |
| `src/base/config_keys.cpp` | 132 | `272881823da1` | Body/header read; lifecycle and cadence reviewed |
| `src/base/config_rows.h` | 45 | `99e5b9e6ca64` | Body/header read; lifecycle and cadence reviewed |
| `src/base/config_table.cpp` | 379 | `6507787056c2` | Body/header read; lifecycle and cadence reviewed |
| `src/base/config_table.h` | 151 | `4348096b3425` | Body/header read; lifecycle and cadence reviewed |
| `src/base/config_values.h` | 10 | `5b0dab978b47` | Body/header read; lifecycle and cadence reviewed |
| `src/base/core.cpp` | 106 | `9c7a33b843b4` | Body/header read; lifecycle and cadence reviewed |
| `src/base/core.h` | 221 | `40cb1be51c8f` | Body/header read; lifecycle and cadence reviewed |
| `src/base/core_internal.h` | 16 | `05dddeab617f` | Body/header read; lifecycle and cadence reviewed |
| `src/base/fixed_log_buf.h` | 114 | `55d6dae07148` | Body/header read; lifecycle and cadence reviewed |
| `src/base/hash.h` | 31 | `5042a265f621` | Body/header read; lifecycle and cadence reviewed |
| `src/base/havok_alloc.cpp` | 46 | `ed3d7c3e2761` | Body/header read; lifecycle and cadence reviewed |
| `src/base/ini_names.h` | 18 | `7221d1aacc96` | Body/header read; lifecycle and cadence reviewed |
| `src/base/ini_text.cpp` | 268 | `18399219fbc7` | Body/header read; lifecycle and cadence reviewed |
| `src/base/ini_text.h` | 79 | `7dd6bd4755fd` | Body/header read; lifecycle and cadence reviewed |
| `src/base/keo_tr.cpp` | 27 | `4b36e8626280` | Body/header read; lifecycle and cadence reviewed |
| `src/base/keo_tr.h` | 12 | `3f63b18fa995` | Body/header read; lifecycle and cadence reviewed |
| `src/base/klib_include.h` | 8 | `54cbfed95272` | Body/header read; lifecycle and cadence reviewed |
| `src/base/klib_include_end.h` | 2 | `23b55f2c4268` | Body/header read; lifecycle and cadence reviewed |
| `src/base/legacy_ini_import.cpp` | 191 | `7b93732dfbff` | Body/header read; lifecycle and cadence reviewed |
| `src/base/legacy_ini_import.h` | 47 | `148229caf1f0` | Body/header read; lifecycle and cadence reviewed |
| `src/base/Localization.h` | 594 | `35d180dd8100` | Body/header read; lifecycle and cadence reviewed |
| `src/base/LocalizationLog.h` | 45 | `f047303cd356` | Body/header read; lifecycle and cadence reviewed |
| `src/base/log.cpp` | 280 | `c90d7c830f40` | Body/header read; lifecycle and cadence reviewed |
| `src/base/log_admission.h` | 12 | `73a1532e4a9c` | Body/header read; lifecycle and cadence reviewed |
| `src/base/log_bounded.cpp` | 27 | `41f13f8bac43` | Body/header read; lifecycle and cadence reviewed |
| `src/base/log_bounded.h` | 15 | `291e618bed59` | Body/header read; lifecycle and cadence reviewed |
| `src/base/log_status.h` | 28 | `dfcf4e9d7eac` | Body/header read; lifecycle and cadence reviewed |
| `src/base/prologue.cpp` | 127 | `56e923f7d62f` | Body/header read; lifecycle and cadence reviewed |
| `src/base/worker_count.cpp` | 9 | `a0810f31bb48` | Body/header read; lifecycle and cadence reviewed |
| `src/base/worker_count.h` | 3 | `b37ef07c454f` | Body/header read; lifecycle and cadence reviewed |

### plugin (12 files, 2946 lines)

| File | Lines | Digest | Coverage |
|---|---:|---|---|
| `src/plugin/banner.cpp` | 116 | `fb9d8e4a840c` | Body/header read; lifecycle and cadence reviewed |
| `src/plugin/crash_record.cpp` | 743 | `608f0fd26256` | Body/header read; lifecycle and cadence reviewed |
| `src/plugin/crash_record.h` | 32 | `0f83f0adbdfd` | Body/header read; lifecycle and cadence reviewed |
| `src/plugin/hook_manifest.cpp` | 745 | `decabdce2185` | Body/header read; lifecycle and cadence reviewed |
| `src/plugin/hook_manifest.h` | 53 | `3917bb75e63f` | Body/header read; lifecycle and cadence reviewed |
| `src/plugin/hook_manifest_policy.cpp` | 92 | `733975e37fda` | Body/header read; lifecycle and cadence reviewed |
| `src/plugin/hook_manifest_policy.h` | 103 | `4b254827806c` | Body/header read; lifecycle and cadence reviewed |
| `src/plugin/hook_manifest_rows.inc` | 518 | `f483579980dd` | Body/header read; lifecycle and cadence reviewed |
| `src/plugin/plugin_entry.cpp` | 496 | `066cd9944234` | Body/header read; lifecycle and cadence reviewed |
| `src/plugin/plugin_entry_internal.h` | 15 | `2b68213c6cf8` | Body/header read; lifecycle and cadence reviewed |
| `src/plugin/profiler_image.cpp` | 23 | `2f98914f426c` | Body/header read; lifecycle and cadence reviewed |
| `src/plugin/profiler_image.h` | 10 | `f067d8800a1d` | Body/header read; lifecycle and cadence reviewed |

### gui (17 files, 2349 lines)

| File | Lines | Digest | Coverage |
|---|---:|---|---|
| `src/gui/bench_buttons.cpp` | 92 | `b8ea3ef4ea8f` | Body/header read; lifecycle and cadence reviewed |
| `src/gui/bench_buttons.h` | 11 | `e5b67e6f9a30` | Body/header read; lifecycle and cadence reviewed |
| `src/gui/gui_config.cpp` | 36 | `a0e80832f9b1` | Body/header read; lifecycle and cadence reviewed |
| `src/gui/gui_config.h` | 21 | `83a934409c4a` | Body/header read; lifecycle and cadence reviewed |
| `src/gui/mcm_bridge.cpp` | 128 | `20038580245e` | Body/header read; lifecycle and cadence reviewed |
| `src/gui/mcm_bridge.h` | 16 | `401c8618e119` | Body/header read; lifecycle and cadence reviewed |
| `src/gui/ModConfigMenu.h` | 181 | `637f820ca6cc` | Body/header read; lifecycle and cadence reviewed |
| `src/gui/requested_settings.cpp` | 94 | `0033ac074dc7` | Body/header read; lifecycle and cadence reviewed |
| `src/gui/requested_settings.h` | 28 | `5e80c0fd7a03` | Body/header read; lifecycle and cadence reviewed |
| `src/gui/settings_factory.cpp` | 569 | `808f6846bce8` | Body/header read; lifecycle and cadence reviewed |
| `src/gui/settings_factory.h` | 100 | `7b56fc7a3fdd` | Body/header read; lifecycle and cadence reviewed |
| `src/gui/settings_layout.cpp` | 149 | `926fd560712d` | Body/header read; lifecycle and cadence reviewed |
| `src/gui/settings_layout.h` | 38 | `5053d92a4b88` | Body/header read; lifecycle and cadence reviewed |
| `src/gui/settings_panel.cpp` | 498 | `c74f860c44c6` | Body/header read; lifecycle and cadence reviewed |
| `src/gui/settings_panel.h` | 13 | `ba8f731f7ca8` | Body/header read; lifecycle and cadence reviewed |
| `src/gui/settings_rows.cpp` | 258 | `a6b447750d62` | Body/header read; lifecycle and cadence reviewed |
| `src/gui/settings_rows.h` | 117 | `43580312d8d4` | Body/header read; lifecycle and cadence reviewed |

### render (64 files, 7828 lines)

| File | Lines | Digest | Coverage |
|---|---:|---|---|
| `src/render/compositor_layout.cpp` | 207 | `b4b8f77f50ed` | Body/header read; lifecycle and cadence reviewed |
| `src/render/compositor_layout.h` | 48 | `6127ad599fbf` | Body/header read; lifecycle and cadence reviewed |
| `src/render/compositor_lever.cpp` | 390 | `3f5c5bde9b7a` | Body/header read; lifecycle and cadence reviewed |
| `src/render/d3d_state_policy.cpp` | 110 | `67fd6417c77d` | Body/header read; lifecycle and cadence reviewed |
| `src/render/d3d_state_policy.h` | 95 | `8c9dfbea5256` | Body/header read; lifecycle and cadence reviewed |
| `src/render/d3d_state_skip.cpp` | 273 | `bbd0569f5520` | Body/header read; lifecycle and cadence reviewed |
| `src/render/d3d_state_skip.h` | 7 | `0ebd70e097a5` | Body/header read; lifecycle and cadence reviewed |
| `src/render/empty_pass_policy.h` | 44 | `c78e6341ab4a` | Body/header read; lifecycle and cadence reviewed |
| `src/render/empty_pass_visibility.cpp` | 171 | `d71798c1d2d3` | Body/header read; lifecycle and cadence reviewed |
| `src/render/empty_pass_visibility.h` | 26 | `553eff20cdda` | Body/header read; lifecycle and cadence reviewed |
| `src/render/fog_fade.cpp` | 72 | `04bc8f2faadf` | Body/header read; lifecycle and cadence reviewed |
| `src/render/fog_fade.h` | 17 | `e8b2ae5eef55` | Body/header read; lifecycle and cadence reviewed |
| `src/render/foliage_budget.cpp` | 48 | `6a81adb0c142` | Body/header read; lifecycle and cadence reviewed |
| `src/render/foliage_budget.h` | 37 | `4daa30821060` | Body/header read; lifecycle and cadence reviewed |
| `src/render/foliage_lever.cpp` | 98 | `16700e4a4d88` | Body/header read; lifecycle and cadence reviewed |
| `src/render/gpu_param_cache.cpp` | 85 | `81dd209d1ae4` | Body/header read; lifecycle and cadence reviewed |
| `src/render/gpu_param_cache.h` | 53 | `4127a368c9bd` | Body/header read; lifecycle and cadence reviewed |
| `src/render/gpu_params.cpp` | 303 | `6005d1037008` | Body/header read; lifecycle and cadence reviewed |
| `src/render/gpu_upload.cpp` | 482 | `36e18d3ca46d` | Body/header read; lifecycle and cadence reviewed |
| `src/render/id_string.cpp` | 58 | `88eb452162fe` | Body/header read; lifecycle and cadence reviewed |
| `src/render/id_string.h` | 9 | `a80c45c6418d` | Body/header read; lifecycle and cadence reviewed |
| `src/render/inst_empty_skip.cpp` | 135 | `14590d61e41f` | Body/header read; lifecycle and cadence reviewed |
| `src/render/module_hooks.cpp` | 88 | `7fec0c1c2940` | Body/header read; lifecycle and cadence reviewed |
| `src/render/module_hooks.h` | 32 | `7c42b2ba88f8` | Body/header read; lifecycle and cadence reviewed |
| `src/render/name_list.h` | 42 | `2ba38ea3c458` | Body/header read; lifecycle and cadence reviewed |
| `src/render/ogre_join_spin.cpp` | 194 | `68affb5325a5` | Body/header read; lifecycle and cadence reviewed |
| `src/render/ogre_join_spin.h` | 15 | `298e748decf6` | Body/header read; lifecycle and cadence reviewed |
| `src/render/ogre_purge_policy.cpp` | 27 | `3ea01fc7da6f` | Body/header read; lifecycle and cadence reviewed |
| `src/render/ogre_purge_policy.h` | 33 | `aedbf918e100` | Body/header read; lifecycle and cadence reviewed |
| `src/render/ogre_purge_skip.cpp` | 177 | `5530f1549bbd` | Body/header read; lifecycle and cadence reviewed |
| `src/render/ogre_worker_policy.cpp` | 20 | `ec0cd47fc6d7` | Body/header read; lifecycle and cadence reviewed |
| `src/render/ogre_worker_policy.h` | 33 | `3a33020f5306` | Body/header read; lifecycle and cadence reviewed |
| `src/render/old_anim_lever.cpp` | 313 | `a24ba164bc2d` | Body/header read; lifecycle and cadence reviewed |
| `src/render/particle_levers.cpp` | 356 | `86fa4d17c687` | Body/header read; lifecycle and cadence reviewed |
| `src/render/particle_policy.h` | 28 | `cf7f4b4f02f6` | Body/header read; lifecycle and cadence reviewed |
| `src/render/pass_visibility.cpp` | 87 | `3a7d04ab7b17` | Body/header read; lifecycle and cadence reviewed |
| `src/render/pass_visibility.h` | 70 | `81e56a8ba0e6` | Body/header read; lifecycle and cadence reviewed |
| `src/render/pe_imports.cpp` | 57 | `c36a85baaa59` | Body/header read; lifecycle and cadence reviewed |
| `src/render/pe_imports.h` | 7 | `8cb38753acc3` | Body/header read; lifecycle and cadence reviewed |
| `src/render/ptr_set.h` | 80 | `63f24ae9f284` | Body/header read; lifecycle and cadence reviewed |
| `src/render/pu_layout.cpp` | 93 | `39910abccee1` | Body/header read; lifecycle and cadence reviewed |
| `src/render/pu_layout.h` | 39 | `7d42b331acc7` | Body/header read; lifecycle and cadence reviewed |
| `src/render/reflection_lever.cpp` | 96 | `7718dd07f7b5` | Body/header read; lifecycle and cadence reviewed |
| `src/render/render_config.cpp` | 18 | `1642b9f8587d` | Body/header read; lifecycle and cadence reviewed |
| `src/render/render_config.h` | 63 | `57654a6701ce` | Body/header read; lifecycle and cadence reviewed |
| `src/render/render_defaults.cpp` | 43 | `20ced78dc85d` | Body/header read; lifecycle and cadence reviewed |
| `src/render/render_keys.cpp` | 259 | `d268370dbf78` | Body/header read; lifecycle and cadence reviewed |
| `src/render/render_keys.h` | 58 | `43874623f8a5` | Body/header read; lifecycle and cadence reviewed |
| `src/render/render_levers.cpp` | 190 | `338e666e0cec` | Body/header read; lifecycle and cadence reviewed |
| `src/render/render_levers.h` | 129 | `d986915c172a` | Body/header read; lifecycle and cadence reviewed |
| `src/render/render_live.cpp` | 208 | `4359c603292f` | Body/header read; lifecycle and cadence reviewed |
| `src/render/scene_fork_skip.cpp` | 340 | `46c7df4d394b` | Body/header read; lifecycle and cadence reviewed |
| `src/render/scene_lever_policy.cpp` | 236 | `dc023a49bd33` | Body/header read; lifecycle and cadence reviewed |
| `src/render/scene_lever_policy.h` | 110 | `95c769317d9c` | Body/header read; lifecycle and cadence reviewed |
| `src/render/scene_levers.h` | 15 | `2f2d6eb6d31f` | Body/header read; lifecycle and cadence reviewed |
| `src/render/shadow_reach.cpp` | 540 | `71b2b988a3f6` | Body/header read; lifecycle and cadence reviewed |
| `src/render/shadow_reach_math.cpp` | 225 | `4b9411b42685` | Body/header read; lifecycle and cadence reviewed |
| `src/render/shadow_reach_math.h` | 115 | `0e3d73edf88a` | Body/header read; lifecycle and cadence reviewed |
| `src/render/shadow_receiver_depth.cpp` | 218 | `1e781200833b` | Body/header read; lifecycle and cadence reviewed |
| `src/render/shadow_receiver_depth.h` | 27 | `2dd17731d0df` | Body/header read; lifecycle and cadence reviewed |
| `src/render/upload_plan.cpp` | 81 | `ec834215c58f` | Body/header read; lifecycle and cadence reviewed |
| `src/render/upload_plan.h` | 45 | `c9cc6b28cf3a` | Body/header read; lifecycle and cadence reviewed |
| `src/render/upload_shadow.cpp` | 165 | `1fcca4c9831d` | Body/header read; lifecycle and cadence reviewed |
| `src/render/upload_shadow.h` | 88 | `ef5d3cca8c65` | Body/header read; lifecycle and cadence reviewed |

### resources (29 files, 2907 lines)

| File | Lines | Digest | Coverage |
|---|---:|---|---|
| `src/resources/resource_adoption_policy.h` | 15 | `36b790770f5a` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_array_job.h` | 15 | `ea5971fa6860` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_biome_manifest.cpp` | 182 | `fb1a09eadc77` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_biome_manifest.h` | 42 | `5b8796a2f2da` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_cache.cpp` | 133 | `239f68b3a99d` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_cache.h` | 3 | `e45a877fd67c` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_cache_admission.h` | 18 | `a14623c0907e` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_cache_store.cpp` | 442 | `68f5eee07056` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_cache_store.h` | 44 | `e35d91384435` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_cache_stream.h` | 13 | `f97089163b7c` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_dds.cpp` | 29 | `f2a6aceea08b` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_dds.h` | 10 | `0a2c918bd985` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_forward.cpp` | 153 | `a43f11b3bca6` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_forward.h` | 15 | `25c77f607958` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_forward_policy.h` | 30 | `119008ec487b` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_geometry.cpp` | 261 | `d09caa934026` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_geometry.h` | 22 | `55c5763fbd8c` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_gpu_bridge.cpp` | 161 | `934dab15eb00` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_gpu_store.cpp` | 275 | `4692ceadc843` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_gpu_store.h` | 65 | `6085ca10d8e4` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_load_probe.cpp` | 366 | `bea068a0e6d6` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_load_probe.h` | 9 | `6c28a986fc60` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_startup_native.cpp` | 139 | `0919cff9f385` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_startup_native.h` | 3 | `44bac29adc10` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_texture_retention.cpp` | 242 | `a3fd8381484f` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_texture_retention.h` | 17 | `9c1c59e1551d` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_warm_hud.cpp` | 108 | `ba73ac015080` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_warm_hud.h` | 26 | `994de83634f2` | Body/header read; lifecycle and cadence reviewed |
| `src/resources/resource_warm_hud_policy.h` | 69 | `3882860759f6` | Body/header read; lifecycle and cadence reviewed |

### inventory (34 files, 3522 lines)

| File | Lines | Digest | Coverage |
|---|---:|---|---|
| `src/inventory/backpack_first.cpp` | 122 | `2a485a051227` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/backpack_first.h` | 21 | `81eead291eb7` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/backpack_food.cpp` | 105 | `783deaa98803` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/backpack_food.h` | 17 | `1c31c8c340c3` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/backpack_policy.cpp` | 157 | `4d92cb487c81` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/backpack_policy.h` | 49 | `97ef515aeb35` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/backpack_reader.cpp` | 301 | `29d5b89a89a0` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/backpack_reader.h` | 46 | `522a45da84d7` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/backpack_sidecar.cpp` | 348 | `9fce8eb5d308` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/backpack_sidecar.h` | 31 | `12d7f4ce84ee` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/backpack_sidecar_policy.cpp` | 142 | `aefdadacc1b5` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/backpack_sidecar_policy.h` | 39 | `851b798942f0` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/backpack_table.cpp` | 229 | `3ce3aa84b7f4` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/backpack_table.h` | 44 | `2bf2d6773be7` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/backpack_window.cpp` | 416 | `6b3570d8ea0a` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/backpack_window.h` | 17 | `bffb6ddf547f` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/byte_check_policy.cpp` | 108 | `305c7ae5fb0b` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/byte_check_policy.h` | 41 | `2df1cf65a0ea` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/carried_weight.cpp` | 121 | `2e16a88e1226` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/carried_weight.h` | 7 | `96a8ee2c68c7` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/carried_weight_policy.cpp` | 57 | `2f13f430a2ba` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/carried_weight_policy.h` | 15 | `f73f4cbfafa5` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/dialogue_item_function.cpp` | 184 | `9bfcf3692d52` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/dialogue_item_function.h` | 16 | `35418c24af6d` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/dialogue_thunk_policy.cpp` | 75 | `c3d33ced34cd` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/dialogue_thunk_policy.h` | 44 | `058f28801246` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/inventory_config.cpp` | 122 | `e04a2a0226af` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/inventory_config.h` | 62 | `b21ebeb71f8a` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/job_counters.cpp` | 147 | `c71ff54d6936` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/job_counters.h` | 15 | `a2c8c6dd3df4` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/operator_policy.cpp` | 91 | `1315b93697f4` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/operator_policy.h` | 75 | `54a53f38c865` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/operator_trips.cpp` | 242 | `61db9f3bd8c6` | Body/header read; lifecycle and cadence reviewed |
| `src/inventory/operator_trips.h` | 16 | `ac94278a555d` | Body/header read; lifecycle and cadence reviewed |

### jobs (10 files, 1131 lines)

| File | Lines | Digest | Coverage |
|---|---:|---|---|
| `src/jobs/steel_jobs.cpp` | 292 | `10f1ede867dd` | Body/header read; lifecycle and cadence reviewed |
| `src/jobs/steel_jobs.h` | 14 | `c60f114b45c5` | Body/header read; lifecycle and cadence reviewed |
| `src/jobs/steel_jobs_factory.cpp` | 139 | `bd692f792f63` | Body/header read; lifecycle and cadence reviewed |
| `src/jobs/steel_jobs_factory.h` | 21 | `65237fe3b8a0` | Body/header read; lifecycle and cadence reviewed |
| `src/jobs/steel_jobs_policy.cpp` | 82 | `50f397a4cef4` | Body/header read; lifecycle and cadence reviewed |
| `src/jobs/steel_jobs_policy.h` | 51 | `c88d5def559b` | Body/header read; lifecycle and cadence reviewed |
| `src/jobs/steel_jobs_stock.cpp` | 182 | `2fbb06bbda40` | Body/header read; lifecycle and cadence reviewed |
| `src/jobs/steel_jobs_stock.h` | 9 | `c75967a68d09` | Body/header read; lifecycle and cadence reviewed |
| `src/jobs/steel_jobs_transfer.cpp` | 320 | `14c1664c113c` | Body/header read; lifecycle and cadence reviewed |
| `src/jobs/steel_jobs_transfer.h` | 21 | `8d41f0fec836` | Body/header read; lifecycle and cadence reviewed |

### bench (31 files, 5311 lines)

| File | Lines | Digest | Coverage |
|---|---:|---|---|
| `src/bench/bench_game.cpp` | 547 | `75d1265b50a3` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_game.h` | 94 | `7b21ca016533` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_game_internal.h` | 26 | `313f7469bb25` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_game_math.cpp` | 85 | `7cc19a6a095e` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_game_math.h` | 42 | `8a74eac6fa5f` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_group.cpp` | 706 | `51b64ffc0719` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_group.h` | 43 | `bf0dd3d7e0ed` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_lever_ab.cpp` | 239 | `530b1b484612` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_lever_ab.h` | 45 | `9ff6ac17747d` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_pin.cpp` | 270 | `78852a70ef49` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_pin.h` | 53 | `cf5289d53cf7` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_pin_run.cpp` | 258 | `3b151bc94dc2` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_recorders.cpp` | 175 | `8ccd417b166e` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_recorders.h` | 132 | `7c39e27ce6d6` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_report.cpp` | 48 | `c6526525ace9` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_report.h` | 22 | `14cd49264f9f` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_restore.cpp` | 92 | `47f6b4ca4bba` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_restore.h` | 36 | `a90200dad86e` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_run_report.cpp` | 102 | `16ad453bb409` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_run_report.h` | 33 | `076f841fb754` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_runner.cpp` | 810 | `e09388b56b1b` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_runner.h` | 40 | `e08434aee2fc` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_scenario.cpp` | 79 | `205afac30cb0` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_scenario.h` | 77 | `2bfb5c611e6b` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_slots.cpp` | 155 | `0b3954490242` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_slots.h` | 56 | `49a99910cb0c` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_stats.cpp` | 37 | `3a8c30c89578` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_stats.h` | 11 | `b0780fea4a79` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_sweep.cpp` | 659 | `620c78812cb8` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_sweep.h` | 96 | `b62a77cf5ddc` | Body/header read; lifecycle and cadence reviewed |
| `src/bench/bench_weather.cpp` | 243 | `695f064a62f9` | Body/header read; lifecycle and cadence reviewed |

### diag (23 files, 3118 lines)

| File | Lines | Digest | Coverage |
|---|---:|---|---|
| `src/diag/cpp_exception.cpp` | 236 | `778528f0f1f9` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/cpp_exception.h` | 43 | `4e637ce3b5c1` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/exit_capture.cpp` | 411 | `aaa5ceadf886` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/exit_capture.h` | 37 | `4c8aae046e0b` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/exit_capture_policy.cpp` | 127 | `31c532f12b90` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/exit_capture_policy.h` | 110 | `e8af61c920ae` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/fatal_class.cpp` | 220 | `847be1f0e901` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/fatal_class.h` | 73 | `6014e7526b7f` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/mem_format.cpp` | 136 | `77840b286a56` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/mem_format.h` | 43 | `599316feee66` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/mem_probe.cpp` | 233 | `573805d8c613` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/mem_probe.h` | 47 | `cba15d481eb6` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/module_bases.cpp` | 204 | `528e2eb9b9d6` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/module_bases.h` | 88 | `7ff7d94437d5` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/physx_pool_probe.cpp` | 345 | `285f7b633a1d` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/physx_pool_probe.h` | 38 | `078e4ea32d93` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/physx_pool_sets.cpp` | 201 | `ca3c7aad444b` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/physx_pool_sets.h` | 101 | `a9062e97bb97` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/squad_watch.cpp` | 159 | `4f1374028170` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/squad_watch.h` | 3 | `e2a27e716966` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/squad_watch_policy.h` | 45 | `ea7e7635c42f` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/throw_ring.cpp` | 153 | `14d189ad4872` | Body/header read; lifecycle and cadence reviewed |
| `src/diag/throw_ring.h` | 65 | `a65c8164213c` | Body/header read; lifecycle and cadence reviewed |

Total: **253 files / 32581 lines**. No claim that inventory alone constitutes review: bodies/headers were read in this pass. Authorized follow-up implementation details and regression results are in the linked correction reports.

## Independent reviews performed after the coverage pass

- Navmesh required delivery: reviewed exact ring, seam-consumer, force-mark lifecycle/policy changes and fixture/report boundaries. No Critical/Important finding within the documented pre-wrap, TryPush-only domain. Independently reran force 199 /Od and /O2 and ring 11 /Od and /O2 controls, all PASS. Required marks retain the existing fixed per-cell storage; reporting rollover and pending age no longer cancel them. Strict ring consumer returns immediately on an unpublished accepted reservation and skips explicit failed claims only. Counter-wrap and broader old-world wall-producer epoch issues remain explicitly outside this patch. Report: `leopard/navmesh-delivery-fixes-20261115.md`.
- Island empty rebuild / handoff idle guard / readiness diagnostic gate: reviewed exact diffs, former zero-only private scratch and saved c737f binary references, manager/reset paths, retained SetB/signature, ledger occupancy writers and unconditional native readiness contract. No Critical/Important finding. Independently reran island 20,768 /Od and /O2 + seven mutants, cohort 191 + three mutants, readiness 4,610 + six mutants: all PASS / compiled expected runtime RED. No behavior/FPS inference from removed work.
- Root NavMeshLife gate: exact diff only gates private diagnostic counters/samples/heartbeat. No external counter consumer; native removal forwards once before return while Debug OFF, and original behavior resumes when ON. Config default remains true DEVROW, hook admission unchanged. Independent fixture rerun: nine /Od and /O2 controls PASS. No Critical/Important finding. Diagnostic totals naturally describe the enabled collection periods.

- Final stitch diagnostic boundaries: reviewed exact `stitch_byte_guard.cpp` / `stitch_source.cpp` / `.h` diff and actual call sites/consumers. No Critical/Important finding. The crash formatter now counts examined tickets, including unpublished/overwritten skips, and stops at sixteen; the new bound does not alter newest-first or requested-row behavior. Forensic bypass leaves `NmAdjCheckStitch` unconditional before exact original forwarding; counted ON calls retain their termination-handler cleanup if Debug flips OFF or native unwinds. No native behavior depends on the forensic tables. Independently reran `tools/tests/test_stitch_diagnostic_boundaries.py --mutants`: formatter eleven and detour/observer twenty-one controls /Od and /O2 PASS; all nine compiled mutants runtime RED. Existing signed ticket-wrap and mixed diagnostic payload limits remain unchanged and documented. Related report: `leopard/full-audit-fixes-search-stitch-physx-20261115.md`. No production edits during this review.

## Owned phase-2 files

Production: `src/base/fixed_log_buf.h`; `src/render/gpu_params.cpp`; `src/render/gpu_upload.cpp`; `src/render/render_levers.h`; `src/bench/bench_pin_run.cpp`; `src/bench/bench_pin.h`; `src/gui/settings_panel.cpp`.

Tests: existing `tools/tests/fixed_log_buf_units.cpp`; new `leopard/test_gpu_map_lifetime.cpp`, `test_gpu_map_lifetime.py`, `test_bench_clock_ownership.cpp`, `test_bench_clock_ownership.py`, `test_settings_gated_capacity.py`. The prior rebind test was executed, not edited.

Private reports: this file; `leopard/phase2-fixed-log-termination-20261115.md`; `phase2-gpu-map-lifetime-20261115.md`; `phase2-bench-clock-ownership-20261115.md`; `phase2-settings-group-capacity-20261115.md`. Build-directory artifacts are temporary verification evidence, not production or release files. No foreign Steel edits.
