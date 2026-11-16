# KEO NPC/model/save loading review follow-up (2026-11-16)

Private development report. Current working release2. User requested quality
review plus continued loading optimization from existing evidence, without new
game runs, profiles, captures or data collection. No publication or timing claim.

## Reviewed scopes

Independent native review: temporary morph-pose leases, model/skeleton reserve,
probe integration; exact caller/admission, shared control blocks, partial-hook
forwarding, nested scopes, C++/handled-SEH teardown and added-reference removal
before native reference-sensitive body replacement. Existing saved native body
assembly reads source pose maps/names only. No new important ownership defect
was reproduced. Independently compiled canonical native fixtures: morph9582,
model48064, skeleton13199 checks, clean native teardown.

Independent resources/save review: RAM store, resource geometry/texture/GPU
retention, victim indexes, initialization, world/device/reset and native pins.
Transfer has no throwing operation after ownership handoff; indexes are removed
before referenced nodes disappear; save reset releases GPU pins while retaining
file bytes. One concrete hook failure defect was found and corrected below.
This focused review does not replace the earlier full-source review or prove
absence of all leaks, races or gameplay regressions.

## Corrected findings

1. Morph cleanup used a quadratic membership loop even for the exact unchanged
   recorded sequence. It now checks size and every ordered pointer first, then
   clears the pointer vector without destroying Pose objects. Normal512-pose
   cleanup needs512 comparisons instead of131328. Any size/order/owned-element
   mismatch falls through to the unchanged ownership-preserving compaction.
   No arithmetic, source poses, GPU work or destruction thread is changed.
2. Geometry capture Init erased an original function pointer after SDK hook
   installation failure. SDK creation can publish a trampoline before enable
   fails. The pointer is now preserved; readiness requires success AND a valid
   original. Allocated State retains one-shot behavior after failure. Optional
   retention remains OFF; a partially armed wrapper can still forward natively.

No new persistent storage, queue, world scan, background worker or setting was
added. The fast path allocates nothing; its fallback is still bounded by the
existing512 recorded poses/8 contexts. Existing native owned-list assumptions
and source/destination lifetime admission remain essential.

## Evidence and test limitations

- New exact Detach-function extraction uses actual native Mesh/Pose objects and
  instruments the two pointer-comparison expressions only. BothOd/O2:34 checks,
  normal512 comparisons. Reordered/mixed/replaced/short/empty/OFF/skipped and last
  index511 replacement are covered. Four production-copy controls compile and
  fail ordinary CHECKexit1 with clean Ogre teardown. The equality-ignored
  control preserves counting and fails OWNED element preservation, avoiding a
  misleading early failure in the operation-count assertion.
- Existing canonical morph aggregate after change:9582 eachOd/O2,53 offline
  pinned contracts and25 compiled strict CHECKexit1 controls, clean teardown.
- Geometry hook fixture extracts exact Init/Create bodies but uses controlled
  SDK/module/minimal types:34 eachOd/O2 plus2 ordinary CHECKexit1 controls.
  Verifies published-original preservation, readiness, direct forwarding/native
  C++ exception propagation, one-shot init, owner/GOG/prefix refusal. This is NOT
  native detour activation, ABI, handled-SEH or Ogre ownership evidence.
- Existing actual native geometry fixture:6430 checks, description-transfer
  copies0/sameStorage1/consumed1, accounting/purge/lifetime cases unchanged.
- Fail-first results were ordinary measured==512 and original==provided CHECKs.
  Initial fixture-only include/module-stub compile errors were corrected and
  are not counted as regression evidence.
- Both full368-source DEV/PROD builds passed using the existing object cache;
 366 unchanged objects restored and2 changed sources compiled per variant.
  Imports and frozen source hashes are verified separately before installation.

Native final NPC/equipment assembly, model decoding, save deserialization and
world restoration still perform their existing work. These corrections remove
confirmed extra KEO work and a failure hazard; no measured save-load, FPS,
stutter or long-running memory improvement is asserted. Current fixture body
substitutes Kenshi body/scene replacement; it does not exercise live game saves.

## Deferred findings

Fresh Description allocation during geometry refresh is small bounded cache
housekeeping (two entries/250ms), not established native loading cost. It was
not combined into this change. VC10 empty vector assignment retains capacity;
earlier speculation that Describe always discards storage was disproved.

A complete RAM OFF/ON cycle before the worker observes OFF may retain completed
entries; the usual file-identity validation still applies. This low-priority
asset-edit/invalidation corner needs its own contract and was not treated as an
evidenced save/gameplay failure. No unsafe global material/path-answer cache or
new NPC worker scheduling was introduced without an invalidation/ownership proof.

Personal INI/catalogs/mods.cfg and seven unrelated Steel files remain protected.
Commit/push/local DLL replacement and backups are recorded separately; no ZIP,
version bump, Nexus/Steam upload or personal published-changelog entry.
