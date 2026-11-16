# Morph pose scope integration, 2026-11-16

Implements the independently reviewed plan in morph-pose-scope-plan-20261116.md.
The core native fixture and its explicit limitations are documented separately
in morph-pose-scope-verification-20261116.md. This correction is working release2,
local only; no game runs, captures, new profiles or general FPS measurements.

## Integration

- Existing model clone/create hooks bridge into the bounded scope even when
  pointer-vector reservation is OFF. Their own reservation behavior remains
  independent; explicit finally now restores their TLS source for handled SEH.
- The existing CreateMorphedMesh wrapper is installed independently of Debug,
  calls the functional scope and preserves its existing optional timing behavior.
  No second detour is added to the same game function.
- Core installation follows the clone/create pair in hook_manifest. Game scope
  installation occurs later in ResourceCacheInit; exact native prefixes are
  checked before their corresponding target is patched. Optimization readiness
  requires all relevant valid original pointers and complete hook sets.
- Both probe installation helpers preserve any trampoline published before SDK
  enable failure, and reject SUCCESS with a null original. Actual SDK create/
  enable ordering was inspected independently. No previous wrapper loses its
  fallback on partial installation.
- morphPoseReuse is defaultON/live. EN/RU/ZH labels/help maintained, Debug stays
  last and OFF by default. Existing personal INI is not replaced.

## Verification

Core:9582 actual native checks eachOd/O2,53 offline contracts,25 compiled strict
CHECKexit1 controls with clean teardown. Existing model wrapper:48064 checks
eachOd/O2,37 native contracts,26 strict controls, including a new fail-first
handled-SEH context regression. Probe wrappers:124 checks eachOd/O2 and actual
VC10 DEV/PROD compilation. Their install helpers are extracted from exact source:
36 checks eachOd/O2 plus4 strict controls. All127 host suites pass, including new
MCM default/live/Debug-last, metadata and golden configuration checks.

The helper regression failed an ordinary out==provided CHECK before correction.
The old model wrapper likewise failed its source-restoration CHECK under /EHsc
before explicit finally. Those were real runtime failures, not compile errors.

Independent review inspected core, native load/postload/unload/caller evidence,
reference-count-sensitive ResourceLoader::destroy, actual before-switch pin
release, installation order, bridge/timing behavior and all failure corrections.
It independently reran core9582 and both helper36 binaries and approved the final
changes. Old appearance mutation controls also pass their existing nonzero-exit
runner; they are not counted as strict CHECK-only evidence.

These tests establish native ownership, byte equivalence and removal of transient
deep pose copies, not a measured save-load/FPS gain or long-running leak absence.
NPC/equipment finalization, model decoding and world/save restoration still use
their native work. Overall FPS remains deferred until the LOK fix is evaluated.
