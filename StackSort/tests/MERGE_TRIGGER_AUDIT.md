# Explicit-click consolidation retry (2026-10-30)

## Reproduced cause

FirstClickInit formerly set stacksConsolidated for the entire open inventory.
No external mutation invalidated it. A production-function extraction using the
unchanged NativeMergeAdapter/driver reproduced sort -> consume charges/split kit
-> sort in the same GUI:4 assertions,2 failures. The compatible kit remained split
with70 rather than100 charges. The correction passes41 assertions in that same
production boundary plus the existing121 Native adapter fault-model checks.
The screenshot's remaining actual compatibility is unverified; this trigger bug
does not prove every photographed item can stack.

## Native/SDK contract

Official KenshiLib0.5.0 declares RootObjectBase::getGameData at virtual slot0x18,
InventoryItemBase::isSameAs at0x2A0 and getProperOwner at0x2A8. The loaded-function
map resolves getGameData to Steam RVA0xD1D30 (slot6100); names/API were checked
against the SDK and the full candidate is compiled with VC10. Disk-only proof
uses the matching unpacked RE_Kenshi executable; no process or native call was
used for investigation.

Steam Native isSameAs0x75C9D0 compares the other's virtual getGameData result
against self+0x40, then manufacturerData+0xC0 and materialData+0xC8. It does not
read quality+0x11C or charge remainder. Native canStackWith0x75C930 calls virtual
isSameAs first, then isStolen(true); when both are clean their proper-owner
identities are ignored. If EITHER is stolen, Native compares BOTH proper owners;
it does not require equality of their stolen states. Stolen-owner compatibility and Rotate's extra rotated
membership predicate remain enforced by the original Native adapter. No synthetic
weapon-quality or extra owner-equality predicate is added for ammunition.

## Change detector and ownership

Only explicit FirstClickInit calls inspect real borrowed getAllSections/getItems
contents. Each item contributes full handle identity, prototype/manufacturer/
material, the Native stolen predicate and relevant proper owner, quantity, charged
units, equipment, actual isStackable capacity and orientation. Charge remainder
is ignored when full-charge capacity is nonpositive, matching adapter Units.
XY and native vector order are excluded. Rotation groups use ONLY real section and prototype/manufacturer/material, a
superset of every potentially Native-compatible pair. Stolen state and full
proper owner of EVERY item (including clean items) contribute to that item's
semantic words. Ownership compatibility is conditional and non-transitive;
partitioning rotation groups by stolen state or owner would be incorrect. The two orientation partitions of each
group are canonicalized: flipping an entire group preserves every pair's relative
compatibility, whereas moving one member between partitions changes the stamp.
Native canStackWith still authorizes each actual transfer. A detector hash
collision could defer a retry, never permit an incompatible transfer.

The table uses bounded open addressing with expected linear cost, maximum128
sections/4096 entries and transient capacity8192. Only24 bytes of stamp plus the
existing success flag persist per UI; no cached Item pointer is dereferenced.
Successful merging is followed by a fresh live stamp, so consumed pointers are
not reused. Faults leave the gate uncached and stop before layout capture/apply.
No native hooks, worker paths, gameplay ownership, quantities or rotation writes
were changed. Existing arrow/dimension/undo handlers are retained; right-click's
existing reset invalidates the gate. Undo continues restoring positions rather
than unmerging. Missing mutation hooks do not affect content/charge detection;
existing safe undo disablement without mutation tracking remains intact.

Detailed log (default off) distinguishes unchanged -> skipped and
uncached/changed -> attempted, then changed/no transfer/fault. It logs only on
explicit clicks. No per-frame item scan or merge call was added. The tests cover
1000 unchanged/reordered requests without another merge pass; this is not an
in-game FPS or memory benchmark.

## Verification and remaining acceptance

The41 new production-gate cases include real charged repair-kit and ammunition
adapter transfers, quiet charge consumption, raw ammunition quality ignored,
strict Native stolen rejection and later compatibility change, relative rotation,
per-group global rotation swap, reorder/XY changes, MCM off/on, existing undo
reset, invalid-float and inventory-count bounds, partial-credit Native fault
recovery, full handle serial and material changes. Existing suites cover solver,
UI click/init/lifecycle, rotation binding and native transfer reconciliation.
Native mutation-test variants remain required to fail their assertions.

Three tooltip PO catalogs and isolated MO outputs are round-trip checked. The
version and personal INI are unchanged. The isolated candidate needs independent
review and Native visual acceptance; neither installing nor inspecting the live
game was authorized or performed for this task. Unsupported third-party native
isSameAs overrides or rotation APIs can have additional invisible compatibility
inputs; the final Native predicate remains the transfer authority. GOG runtime
acceptance is outstanding.


## Independent P2 correction: mixed stolen states

Candidate beb53380 is rejected. Its Native test fake incorrectly required equal
stolen states, and its rotation groups separated clean/stolen items and owners.
Native disk branches75C964 and75C972 BOTH jump to75C981 owner comparison when
either predicate is true. A clean and stolen item with the same proper owner can
therefore be compatible. Two singleton orientation groups could hide a relative
rotation change across their boundary, and the clean owner's quiet change was
missing entirely. With the fake corrected, new actual stamp + FirstClickInit
cases reproduced41 checks /4 failures before the fix. The corrected grouping
and per-item ALL-owner words pass41/0. The original Native transfer adapter
remains unchanged; no theft/owner state is rewritten. A deliberate omission of
the clean owner's index must fail the regression suite.
