# Geometry retention refresh without temporary collection (2026-11-16)

Working release2, continued NPC/model/equipment/save work from existing evidence
only. No game run, live log/memory read, profile or new gameplay data. This pass
reduces KEO cache bookkeeping; it is not a native NPC/save-loading speed claim.

## Decision and implementation

Offline Entity builder review confirmed C2B7B->C4FA0 already reserves exact
SubEntity count*158 before construction. Its temporary construction/copy has one
extra56B empty Renderable-map allocation/free, not a second geometry/material/
buffer copy. Replacing that native vector-publication and exception path is not
proportionate to the unmeasured benefit. No Entity constructor rewrite was added.

Confirmed KEO cost: each retained mesh refresh built a fresh Description vector
and compared it with stored buffer records. Even an unchanged resource therefore
grew/copied/destroyed temporary storage twice per250ms heartbeat.

New local Match visitor compares each first buffer encounter directly against
the next retained record. It deduplicates against only the already matched
prefix, preserving the original unique-record order. Pointer, GPU bytes and CPU
shadow bytes must match individually; equal totals cannot hide redistribution.
Final record count and totals must match. Null/system buffers and duplicates
retain their previous skip order. Collector and matcher share ReadBuffer size/
cumulative validation and the entire mesh/vertex/index/LOD walk and limits.

Identity, creator, manager registration and loaded-resource checks remain before
matching. A mismatch still runs ordinary Describe before releasing/re-promoting
the entry; invalid resources and allocation failures retain original recovery.
New candidates still collect normally. No persistent scratch, extra native pin,
new queue, worker, hook, budget, MCM row or locale change. Existing owner/passive
getter/reentrancy assumptions remain; no generic concurrent-unload guarantee.

Stable matching allocates no temporary description storage and avoids the final
record-comparison pass. Dedup remains quadratic, bounded by512 unique buffers.
A changed mesh can require a partial match walk followed by ordinary collection;
the existing two-refresh/250ms budget is unchanged. This does not remove native
mesh decoding, equipment/material setup, NPC creation or world/save restoration.

## Evidence

Actual native Ogre fixture first failed ordinary CHECK(stableWithoutAllocation):
with the next descriptor allocation forced to fail, the old refresh cleared its
cache. Native teardown completed. Corrected actual fixture passes6481 checks in
bothOd/O2. Sixteen ticks refresh the same512-buffer mesh32 times without any
attempted descriptor allocation, preserving native pins, map/vector identity,
touch value and byte accounting. The injected failure remains unused.

Matrix covers pointer replacement, missing tail/internal record, first-encounter
reorder, duplicates across shared/dedicated/shadow/index/LOD data, size and shadow
changes with equal aggregate totals, system-memory exclusion, zero/over8GiB/
cumulative overflow, pose/null resources,64/65 bindings,64/65 submeshes,64/65 LOD
entries per submesh and2048/2049 total visits. Tests isolate these bounds instead
of letting another active limit mask the one under test.513 distinct buffers are
rejected separately from valid512 buffers totaling513 bytes.

On a real mismatch, injected allocation failure in ordinary collection clears
the cache/pins/accounting. Subsequent observation safely readmits the512-buffer,
513-byte inventory. Existing native registration/creator/orphan/device/world/
pressure/purge/lifetime and transfer cases remain in the same fixture.

Eighteen compiled production-copy controls fail ordinary CHECKexit1 with clean
Ogre shutdown and no crash. Three existing description-transfer controls also
pass their strict CHECK-only runner. Fixture early-return cleanup now clears
both cache pins AND manager-owned meshes before the stack hardware manager dies;
an initial deliberately broken matcher exposed this fixture teardown gap and
its crash was explicitly not accepted as a killed control. An initial fixture
local-name compilation error was corrected, not counted as regression evidence.

Pinned existing Steam/Ogre ownership/orphan/manual-LOD contracts pass. Full369-
source DEV/PROD builds restore368 unchanged objects and compile the changed one.
No unchanged UI/config suite repeat is needed; prior127-host result is historical.
Independent design/production/test-boundary/cleanup review found no important
remaining issue. Final6481 positive results and18-control completion were checked
by the parent after the review's last source inspection. Imports/installed hashes
and preserved settings are verified in separate private receipts.

Source coverage inherits previous full audit plus this actually changed resource
file, not a new whole-source reread. Personal INI,3 compiled catalogs, mods.cfg and
seven unrelated Steel paths remain protected. Scoped commit/push and local DLL
install only; no release number increase, archive, Nexus/Steam publication or
personal published-changelog entry. Gameplay loading/FPS/long-run memory impact
remains unmeasured.
