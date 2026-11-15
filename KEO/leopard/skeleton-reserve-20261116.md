# NPC skeleton storage preparation - private correction, 2026-11-16

Scope: allocator-only preparation before native OldSkeletonInstance::loadImpl.
No game launch, process/memory inspection, new diagnostic collector, capture,
profiler, save edit, FPS test, version bump, package or publication. The owner
postponed general FPS comparisons until the LOK author fixes that mod. No
vanilla/no-LOK run was requested or performed.

## Existing evidence and concrete cost

Existing appearance/texture-retention route records show per-instance Entity
initialisation, body/morphing and attachment work after assets become available.
RAM bytes and retained GPU resources do not preconstruct skeleton/bone instances.
This patch does not claim to explain every remaining stall or save-load delay.

Pinned Ogre SHA2567d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6.
Skeleton default constructor342030 zeroes vector begin/end/capacity at178/180/188;
OldSkeletonInstance constructor1E6800 stores its native master at290. Neither
reserves the vector. loadImpl1E6DB0 iterates roots; cloneBoneAndChildren1E6C00
creates each bone using the master's handle. Native createBone unnamed342BC0
and named342E40 resize to handle+1 (calls342DF4/343170 ->3469E0), allocating
and copying pointer storage repeatedly. Handles are limited to0..255.
Entity's subentity list already reserves before constructing entries; no second
subentity optimization was added.

Initial fail-first chain128 fixture observed13 native vector growths, unchanged
by the original no-op preparation stub, then failed the zero-growth assertion.
Final balanced128 fixtures observe8 native vector growth allocations without
preparation versus1 preparatory allocation +0 native growths, across all named,
unnamed and mixed bones. These are allocator/capacity events, NOT bone-object
allocations, elapsed-time measurements or a claimed gameplay speedup.

## Implementation and ownership

Only main owner + admitted native contract + live skeletonReserve setting admit
preparation. The master must be nonnull, distinct and loaded; this rejects
incomplete resources but is NOT a lock against explicit unload or proof of
arbitrary concurrent mutation safety. Native ownership/order stays authoritative.
An empty destination with source handle span1..256 and insufficient capacity
receives reserve(span), never resize. Sparse holes are included in span. All
bone objects, names, hierarchy, transforms, binding pose, animations, RNG,
callbacks and readiness remain native. No new worker, per-frame scan, persistent
cache/reference, queue, timeout, QPC or diagnostic data collection is introduced.
The native instance owns/frees its array as before; an empty array may acquire
up to2048bytes before native construction, typically avoiding capacity rounding.
One optional native hook is installed once; OFF and unsupported paths forward.

IMPORTANT ABI carve-out: SDK OldSkeletonInstance tail order differs from the
installed engine. Do not instantiate an SDK-derived instance, use its size or
vtable, or read/copy its declared master SharedPtr. Production copies only the
borrowed pointer from admitted native field290 via memcpy. Its Skeleton base
starts at0; the separately verified bone-vector prefix178 uses a legal exposed
base pointer-to-member, never a fabricated derived-object cast. Compile checks
assert vector offset178 and VC10 vector size32. Runtime admission checks actual
Ogre PE identity, exported entry equality and four exact native byte regions.
Offline22-contract verification pins the whole engine file, constructor calls,
resize calls, native instance allocation2F8 and fixture's two bone virtual slots.

Preparatory allocation/diagnostic exceptions cannot suppress native loading:
reserve catch has a best-effort nested logger catch; original executes once
outside it. Native exceptions propagate unchanged. Partial hook failure retains
any trampoline, leaves admission OFF, logs error and never retries.

## Offline verification and limits

Actual installed Ogre constructors/loadImpl/bone creation/destruction execute in
isolated unit executables. Instance placement storage uses the native2F8 size
plus64canary bytes, and only test-local vtable copies count the two bone creation
routes. SDK instances/vtables are never used. Named/unnamed/mixed balanced trees
1/8/128/256 and reverse handle order compare every bone handle/name/parent,
children, position/scale/orientation, exact offset matrix and root iterator;
canaries stay intact. Empty, sparse255, oversized metadata, nonempty destination,
adequate capacity, null master, unloaded master, OFF/nonowner/unadmitted gates,
reserve failure plus logger failure, native exception, fake-image refusal and
one-shot/partial-hook behavior are covered. Invalid/partial master cases use a
spy fallback instead of asking native Ogre to load an unsupported malformed
skeleton. This distinction avoids mistaking a native empty-skeleton assertion
for a production regression. No native byte allocation or bone call is shared
between different instances.

Final fixture13199checks per/Od and/O2;14 separately compiled mutations must
exit1 at CHECK (compile failures/crashes rejected);22 offline pinned contracts.
All127 host suites pass, including config golden records, live MCM persistence,
correct restart flags and Debug last. Three133-key catalogs compile/roundtrip;
Russian catalog rejects unsupported dashes. Full366-translation-unit DEV/PROD
builds,183 actual KenshiLib0.5 imports and complete native Ogre/MyGUI exports
plus CRT100-only gates are recorded in local build receipts. Independent review
found and prompted correction of throwing diagnostic fallback, then approved it.

Run: bundled Python leopard/verify_skeleton_reserve.py. It invokes native
Od/O2 fixture, pinned native contracts and14 compiled negative controls. Per-step
logs and hashes live under build/skeleton-reserve-final. Full source inventory
inherits the earlier audit and records focused review only for six changed/new
source files; it does not assert a fresh whole-source reread.

New MCM Performance setting Prepare NPC skeleton storage defaultsON and is live;
EN/RU/ZH help is maintained. Personal INI/save settings are not edited. Native
admission and appearance/performance in the running game remain unmeasured.
