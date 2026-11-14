# KEO loaded texture retention: implementation plan and ledger

Goal: reduce repeat synchronous texture loading during NPC attachment completion
without moving mutable renderer/world operations onto arbitrary workers.

Evidence: runtime-appearance-preparation-first. Late update42.104ms contains
41.007ms attachment callback, no morph/pose work. Native attachment materials
call Entity/SubEntity::setMaterial ->Resource::load(false)->Material.loadImpl
->TextureUnitState.ensureLoaded->TextureManager.load->Resource.load(false).
ResourceLoader::destroy unloads/removes textures only below its reference-count
threshold; a retained actual loaded TexturePtr can protect eligible resources.
This is a testable optimization hypothesis, not a proven FPS/reload gain yet.
Other remaining callbacks/texture uploads also exist; do not promise zero stutter.

Architecture: separate resource_texture_retention module owns loaded immutable
file-backed2D TexturePtr pins, capped1024 entries/128 weak pending candidates,
bounded4 promotions/2refreshes per250ms. Observe after exact verified Ogre
Resource::load succeeds, main owner only, manager identity before type cast.
No forced load, unload, entity construction, worker mutation or native purge
override. Hold only after sharedVRAM admission; pending observes handle+identity,
not native resources. World/device/OFF/shutdown clear while renderer alive.
Exceptions in optional observation preserve successful native loading.

A new defaultOFF live MCM option controls this cache independently from geometry
and terrain preparation; shared existing1..8GiB cap includes all three caches.
RU/EN/ZH descriptions/HUD stats updated, RU uses ASCII hyphen. Detailed log last.
Owner's current test INI gets only the new explicit ON key; other keys preserved.

Tasks:
1. Independent module implementer: production module/header + actualnative
   resource fixture and byte/export admission verification. Fail-first tests,
   refcounts/no-pendingpins/limits/OFF/world/device/reload/pressure/zero-byte/
   render-target/manual exclusion; no sourcecheckout/install/commit.
2. Root: sharedVRAM reservation arithmetic tests first, implement symmetric
   texture pin reservation alongside geometry/terrain. Integrate config,
   lifecycle, HUD,3locales and bounded diagnostic output. No blind nativebatch
   change. Include already measured factoryProcess child delta in slowrecords
   to distinguish the unclassified91ms callback from appearance work.
3. Independent full review, native/production boundary tests, fullDEV+PROD,
   imports/catalogs, commit/push only own files, preserve7foreignSteelchanges.
4. Install only if closed; otherwise ask for closure after concrete build.
   One subsequent route observes texture pins/reuse and scoped slow calls.
   No release/version/publication/ALL_CHANGELOG update.

Rulings: reuse existing isolated game/_codex_work stage to preserve accepted
cache builds and tests; do not create another checkout. Native texture size is
logical accounted bytes, not physical WDDM residency. Exclude generated/manual,
dynamic, multisampled and render-target textures. Source-of-truth identity is
actual loaded resource manager handle+pointer, not filename alone.

Ledger: preparation/evidence captured. Task1 and task2 in progress independently;
interfaces resource_gpu::TryTextureRetentionReservation(U64 total,U64 cap),
TextureRetentionStandbyAllowed(U64 cap), TextureRetentionReservation().
Root must include textures in every terrain admission/publish and geometry
admission/pressure path; releasing/lowering pins remains valid under pressure.
