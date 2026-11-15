# NPC skeleton storage preparation

Goal: eliminate repeated bone-pointer-vector allocations during native per-instance
skeleton loading, using existing evidence only. No game runs/data collection.

- Native OldSkeletonInstance::loadImpl only: main owner and live option; exact
  installed Ogre image/export/entry plus relevant constructor/resize contracts.
- Before native call, for empty destination and nonnull distinct master with
  handle span1..256, reserve(master bone-vector size), never resize/create/link.
- Legal exposed pointer-to-member access; compile SDK bone field178; native master field290 is read with memcpy because
  the SDK OldSkeletonInstance tail has a different member order. No SDK instance
  construction/vtable/size/master SharedPtr access is permitted.
- Catch reserve failure only; original called once outside catch, exceptions
  preserved. No retained native refs/scratch/shared cache/worker/scheduling change.
- Fail-first actual native Ogre fixture: ascending named/unnamed/mixed trees,
  sparse/out-of-order handles255, zero/nonnull/preexisting/adequate/OFF/foreign,
  forced reserve fault and native exception; compare hierarchy/transforms/roots
  and capacity changes. Native export/ABI proof and refusal controls.
- New live MCM option default ON, EN/RU/ZH, README, Debug stays last; no version
  bump/INI edits/publication. Full DEV/PROD/host/import gates + independent review.
- Stage edits only; source-copy transaction protects seven foreign Steel hashes;
  install local if process closed, preserve INI/catalog settings and backups.
- This removes allocator work. No elapsed NPC/save-load or FPS gain is claimed.
