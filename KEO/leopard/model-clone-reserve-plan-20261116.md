# Bounded Mesh clone storage preparation

Goal: remove repeated pointer-vector allocations before native model clone loops
without changing decoding, morph arithmetic, GPU uploads, native destruction,
object identity, spawn order or readiness. No game run or new runtime capture.

- Mesh clone + MeshManager createManual exact admitted native pair; both ready
  before preparation. Native member hidden-result ABI forwarded exactly once.
- Main-owner live option. TLS source scope on EVERY Clone entry, masking disabled
  or off-owner nested calls; restore even on native exceptions.
- createManual captures caller at entry, requires Ogre+1A3FDD, calls native first,
  and borrows caller-owned output only on success result==output.
- Loaded distinct source/target, empty per-target lists, positive source spans
  <=64SubMeshes and<=512Poses: reserve only. Typed base member-pointer list170,
  public getPoseList348 mutable actualtarget; compiled vector ABI32.
- Optional reserve/logging exceptions cannot prevent native continuation. Keep
  any published trampoline on partial install failure; no retries.
- No worker destruction: earlier CPU-only Pose disposal idea rejected because
  native allocator lifetime/teardown and benefit were not established.
- Fail-first exact production/native Ogre fixture, real hooks in isolated unit
  executable only; SDK private MinHook init is TEST ONLY and pinned. No game init.
- Full native output/lifetime/refcount/count/ABI/lifecycle/error/partial/nested
  controls, compiled runtime-negative controls, three locale/default/live MCM,
  full DEV/PROD/host/import gates and independent review before source/install.
- Preserve seven foreign Steel files and personal INI. Workingrelease2; no new
  version/package/publication. HUD auto-hide is an independent companion fix.
