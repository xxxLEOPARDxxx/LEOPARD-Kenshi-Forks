# Private retention victim index review - 2026-11-16

Scope: stage-only KEO source correction, under
`D:/SteamLibrary/steamapps/common/Kenshi/_codex_work/keo-resource-cache-20261114`.
No Kenshi game run, profiler, collector, capture, diagnostic DLL, install,
source-checkout edit, package, commit or push was performed by this worker.
The root agent owns integration, independent review, combined DLL builds and
release handling. This is not evidence of measured campaign FPS or NPC loading
improvement; cache bookkeeping does not establish the vanilla mature-save cause.

## Concrete cost and algorithm

The old `Oldest` scans every retained model/texture for each victim. At full
capacities, 32 pressure victims perform 65,040 model touch comparisons and
32,272 texture touch comparisons (`2048+...+2017`, `1024+...+993`).
The fail-first fixture extracts the actual production `Oldest` function bodies,
compiles them, instruments touch comparisons, and requires <=2,048 comparisons
per collector. Before the production edit it compiled and returned runtime RED
exit 1 with those exact old totals; the retained baseline JSON records it.

`VictimIndex<Map,Capacity>` stores fixed arrays of nonowning map iterators.
Each Entry has one slot, enabling arbitrary erase and touch repair without a
search. The comparator is numeric unsigned touch ascending, then the existing
map's `key_comp()` on its pointer key. This reproduces the previous scan's
first-in-map tie choice, including unsigned clock wrap. A refreshed entry may
be promoted with its original old touch; insertion and change repair both
directions. There is no normalization or clock-age reinterpretation.

Selection is O(1); removal, insertion and changed touch are O(log N). The same
production-body fixture now reports 892 and 796 touch comparisons for 32
removals at full capacities, including index repair work. This is a counted
algorithmic workload, not elapsed game time. Native resource destruction and
geometry buffer accounting still have their prior costs.

Tradeoff: an already retained native consumer previously assigned a touch in
O(1) after its existing map lookup. It now repairs the index in O(log N), bounded
by the same capacities (11 model/10 texture levels). A cold Promote also checks
for duplicates with a map lookup. This does not claim a universal speedup for a
hit-only workload without pressure.

## Lifetime, admission and fixed memory

- `Release` removes the iterator BEFORE any model/texture map erase.
- `Clear` resets the index BEFORE map clear destroys native references.
- The heap owns no Ogre pointer/reference, adds no allocation, native call,
  lock, queue, timer, traversal of the game world, or additional resource pin.
- Native SharedPtr ownership, weak pending observations, same pointer/handle/
  creator identity gates, description eligibility, MaxPending, MaxModels,
  MaxTextures, MaxBuffers, shared GPU budget/reservation, two refreshes, four
  admissions, 32 pressure victims and 250 ms heartbeat remain intact.
- Duplicate Promote is refused before reservation/accounting/index mutation.
  The insert result is also checked defensively. A failed unexpected insert or
  heap admission clears own bookkeeping; allocation exceptions remain handled
  by the existing Tick catch/Clear. Account, budget and capacity refusals do not
  insert into the index.
- No cache map node is moved by repair; refreshAfter remains a raw key.

Measured with the shipping VS2010 x64 /MD, `_ITERATOR_DEBUG_LEVEL=0` ABI:

| Component | Bytes |
| --- | ---: |
| Model index object (2048 iterator slots + map pointer/count/padding) | 16,400 |
| Texture index object (1024 iterator slots + map pointer/count/padding) | 8,208 |
| Model Entry old/new | 72 / 80 |
| Texture Entry old/new | 40 / 48 |
| Increment at both full capacities, including Entry padding | 49,184 |

The two fixed index objects total 24,608 bytes even when empty. Entry additions
cost 8 bytes per admitted map node (including alignment), up to 24,576 bytes.
The collector State remains intentionally process lifetime as before; Clear
releases resource pins normally while the engine is alive. No new native leak
claim is made from offline tests.

## Verification

One reproducible command, from the stage root:

```text
python leopard/verify_retention_victim_index.py
```

Fresh completed run: all eight subprocesses exit 0. Full logs are in
`build/retention-victim-verification/`; the aggregate JSON contains commands,
summaries and SHA-256/size snapshots checked unchanged over the whole run.

| Command, for both O2 and Od | Result per optimization |
| --- | --- |
| `cmd /c leopard\test_retention_victim_index.cmd O2` (or `Od`) | 2,058,864 checks PASS |
| `cmd /c leopard\test_resource_geometry.cmd O2` (or `Od`) | 6,395 native Ogre checks PASS |
| `cmd /c leopard\test_resource_texture_retention.cmd O2` (or `Od`) | 3,315 native Ogre checks PASS |
| `python leopard/test_retention_victim_cost.py` | 892 / 796 comparisons, exit 0 |
| `python leopard/test_retention_victim_mutants.py` | 16 compiled runtime RED controls PASS |

The actual header differential fixture uses 2048 and 1024 entries, both normal
and reverse stateful map comparators, full ties, 120,000 randomized operations,
unsigned wrap, increasing/decreasing touches, old-touch erase/reinsert,
arbitrary erasure, draining, reset/reinsertion, duplicate/cap refusal and zero
index allocations. It compares each victim with the legacy map scan and checks
slot uniqueness. Reviewer found the initial LCG low-bit action bias; the final
fixture uses high bits and asserts each action occurs >5,000 times per run.
Final action counts per 30,000 operations:
`7617/7816/6879/7688`, `7614/7814/6892/7680`,
`7614/7815/6888/7683`, `7617/7813/6893/7677`.

Both actual production collectors run with real native Ogre resources,
managers, SharedPtrs and purge/unload behavior; only the existing owner/GPU
reservation boundary is substituted. Extended cases exercise full capacities,
oracle eviction, a guaranteed current-root native hit, wrap, real Tick refresh
with unchanged old touch, arbitrary release/re-promotion, pressure, Clear,
duplicate refusal BELOW and AT capacity, cap and reservation refusal. Geometry
also tests Account mismatch and the unchanged 65,536-buffer limit. Allocation
fault injection exercises six model allocation points (including a fault AFTER
heap insertion) and one texture map allocation; each verifies empty consistent
bookkeeping/reservation and the original native resource reference count.

All 16 controls compile, then produce CHECK diagnostics with exit 1. Build
failures/crashes/surviving controls are rejected. Only generated mutant copies
change CHECK's return to immediate `std::exit(1)` to avoid destruction of
deliberately inconsistent fixture state after the assertion has already fired.
Controls cover tie comparator reversal, signed/modular wrap ordering, missing
touch repair, missing insertion repair, arbitrary removal repair, moved slot,
reset slots, both collectors' native hot-touch wiring, old-touch refresh,
duplicate precheck, allocation catch/Clear, and the geometry buffer-count cap.
The final expanded native refresh test forces pixel accounting ABOVE mSize, so
it cannot silently pass without refreshing; the old-touch mutant is detected.

## Exact production SHA-256

```text
ad081307aabe4ca95d615c22275a571721e6c5f5df8c25ec641451127e1a3ef2  src/resources/resource_geometry.cpp
36c4bff39e073e266e72192c7075f7106c39b9533edf983e9a61f9ad9efd3333  src/resources/resource_texture_retention.cpp
deb867abcf8804700414b6355d16e7645d642e1323f450272ff2bade8d489b3d  src/resources/retention_victim_index.h
```

## Owned files

Production: the three files above, no other production edits.
Tests: `leopard/test_resource_geometry.cpp`,
`leopard/test_resource_texture_retention.cpp`,
`leopard/test_retention_alloc.h`, `leopard/test_retention_victim_index.cpp`,
`leopard/test_retention_victim_index.cmd`,
`leopard/test_retention_victim_cost.py`,
`leopard/test_retention_victim_mutants.py`,
`leopard/verify_retention_victim_index.py`.
Reports/receipts: this Markdown report,
`leopard/retention-victim-cost-baseline.json`,
`leopard/retention-victim-mutants-20261116.json`,
`leopard/retention-victim-index-20261116.json`.
The existing geometry/texture `.cmd` files were read and executed, not edited.
