# SDK hook batching and first-load resource coverage - 2026-11-15

Status: offline-verified working release 2 follow-up; no version bump, ZIP, publication or game run. This report supplements the previous 733-file body review. Current snapshot has 734 files; unchanged bodies inherit that review, changed files below have focused review, not a second full reread.

## Proven defect and bounded startup change

Actual SDK Functions_static.cpp selects QueueHook module ID from its static wrapper in KEO but ApplyQueuedHooks module ID from QueueHookInternal in KenshiLib.dll. An extracted actual-SDK wrapper fixture reproduces mismatched 0x11110000/0x22220000 IDs and expected runtime RED. Empty wrong-ID application can report success without applying the intended queue.

KEO now resolves admitted internal exports and explicitly supplies one KEO module ID to both calls. It does NOT modify KenshiLib.dll, its source, global SDK memory, another plugin's ID or another module's queue. Admission requires the already pinned SDK SHA256 plus loaded PE timestamp/image size, exact export RVAs and entry prefixes. An unsupported admission falls back to the existing immediate installation before touching any queue.

Only four mutually independent rows opt in: Character::updateOnScreenCheck, Character::pausedUpdate, ActivePlatoon::calculateCurrentPos and FactionRelations::update. Their helper addresses are prepared first, each has its own static original, and their runtime bodies are unchanged. Queues are filled only at the end of the existing installer sequence, then applied once. Up to four separate SDK freeze/enumerate/suspend/resume cycles become one. Coupled native patches, worker capability/readiness publication and all other hooks keep their existing ordering/path. This is NOT proof that overall startup is twice as fast or that most freezes were removed.

Coordinator: fixed 16 entries, startup owner only, one shot, no heap, frame work, new thread, engine object or persistent stack pointer. Queueing/creation does not publish success. Missing successful original prevents application. Queue/apply failures and exceptions conservatively publish unconfirmed results, preserve every produced original, never retry AddHook and never clear a queue or another plugin's state. A partial apply may have active hooks, so originals remain callable. Independent review found early adapter reentry could replace a callback/clear an original before admission rejection; guards now reject duplicate/noncollecting rows before any such mutation. RAII clears the bounded stack counter even if completion logging throws.

## Save/NPC cache coverage

The existing startup inventory filtered only textures although the archive byte cache already accepts .mesh and .skeleton. It now admits both into the same capped inventory and reads admitted models/skeletons first. No budget increase or startup eviction; native demand remains first. It prepares immutable file bytes, not NPCs, parsed meshes, bones, attachments, physics or GPU geometry. Full details, original fail-first result, ownership, negative controls and limits: save-npc-model-byte-warmup-20261115.md. Model-first priority holds within the bounded admitted inventory; arbitrary files beyond its cap are not promised. Existing INI keys/defaults and personal settings stay unchanged. EN/RU/ZH help and HUD now say resource warmup.

## Fresh verification

- Actual SDK wrapper ID regression: expected runtime RED.
- Actual coordinator: 287 controls each VC10 /Od and /O2; six separately compiled mutations runtime RED. Covers queue/apply throws, absent successful original, same ID, bounds, duplicate/reentrant admission, retained originals and no retry/reapply.
- Actual manifest adapter: 511 controls each /Od and /O2; five compiled mutations runtime RED. All 16 partial-enable subsets, completion-after-apply, original lifetime, unsupported fallback, bad prologue, whitelist and exception cleanup.
- Actual admission function: 10 fault controls each /Od and /O2. Pinned installed SDK hash/PE/export/RVA/prefix checked offline. No native SDK execution.
- Actual model warmup: 29 controls each /Od and /O2; six compiled mutations runtime RED. Existing real filesystem, demand/menu/cancel and real Ogre .mesh CachedStream ABI/lifetime suites passed. Three MO catalogs roundtrip and Russian dash check passed.
- Independent reviewer reran coordinator, adapter, admission and model fixtures including all their mutants, accepted final production fixes and cache ownership/cap/priority/lifecycle.
- Full combined DEV and PROD build: 364 translation units each, VC10 x64 /O2 /GL /LTCG. All 127 host suites pass. Both DLLs pass 183 KenshiLib0.5 imports, actual Ogre/MyGUI export and CRT100 gates. Detailed DLL hashes: batch-models-build-verification.json in stage.

## Limits

No new launch, repro, capture, profiler, collector or diagnostic DLL cycle. Resource file reuse cannot remove save/world deserialization or per-NPC CPU preparation. Native NPC appearance remains a plausible residual stutter source, not proof of every residual hitch. No new safe whole-NPC worker/GPU offload was established. No measured startup/save/FPS gain and no claim the long-campaign FPS decline is resolved.

The cache task's automatic approval review rejected deleting eight tiny owned fixture-input directories (512 bytes total), reason `blocked by policy`. Those inputs remain; no permission was needed or requested for source/build work.

## Focused production paths

- src/fixes/world/corpse_pin.cpp
- src/fixes/world/faction_relations.cpp
- src/fixes/world/onscreen_stagger.cpp
- src/fixes/world/paused_skip.cpp
- src/game/native_profile.cpp
- src/game/native_profile.h
- src/plugin/hook_manifest.cpp
- src/plugin/hook_manifest.h
- src/plugin/independent_hook_batch.h
- src/resources/resource_cache_store.cpp
- src/resources/resource_cache_store.h
- src/resources/resource_warm_hud_policy.h
- src/zone/zone_config.cpp
