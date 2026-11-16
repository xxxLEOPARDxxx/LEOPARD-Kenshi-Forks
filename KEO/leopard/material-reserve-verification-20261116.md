# Material copy preparation (working release2, 2026-11-16)

Owner requested continued NPC/model/equipment/save loading optimization and
quality review, using existing evidence only. No game executable was launched,
no game process memory/log/profile was collected and no FPS/timing claim follows.

## Confirmed native work

Pinned Ogre Pass copy constructor211C90 initializes its pointer vector at118,
then calls assignment212020 at211E4B (return211E50), before publishing parent,
index and final hash. Assignment clears the old vector without freeing capacity
and copies native TextureUnitState objects with incremental pointer pushes.
Native source8/16/64 lists cause6/8/12 growths with capacities9/19/94. The new
capacity-only preparation allocates once to8/16/64 and leaves no native growth.
Count1 is deliberately excluded because it saves no allocation.

Pinned game code relevance: new hair material5320B5 calls Material::clone. Item
material helper840B80 looks up a cached material at841291; cache hits branch away
from cloning. Misses clone at8413B9, then configure native layers/programs. The
feature affects those eligible clone operations, not existing cache hits or all
equipping calls. Native TUS/material/shader/texture object counts are unchanged.

## Ownership/admission

- One Pass assignment hook; main owner, valid admission, live setting and exact
  native copy-constructor return only. No preparation for ordinary assignment
  to existing live materials. Both operands must be distinct/nonnull; target
  empty, source2..64, target capacity insufficient.
- Exact typed Ogre vector, proven VC10 offset118/vector32/Pass210. Reserve does
  not change size, copy pointers, share texture units, mutate source or load a
  texture. At most512 bytes reserved for this vector. No persistent objects,
  queues, references, new workers, world scans or per-frame preparation.
- Native image identity, exported assignment/copy constructor and five byte
  contracts protect constructor initialization/caller/clearing assumptions.
  Unknown engine builds retain the native path. Original trampoline survives a
  failed enable, readiness requires success+nonnull and installation is one-shot.
- Optional reserve/logger exceptions are contained; original call stays outside
  the catch, exactly once, preserving its return and native exception propagation.
- Header allocator and native vector destructor reach the same Ned allocator.
  Actual native constructor unwind releases the newly reserved pointer array if
  assignment throws. Existing native cleanup of partially constructed texture
  objects is not rewritten or claimed universally leak-free.

## Verification

Native baseline fixture failed ordinary CHECK(grows==0), recording6 growths for
source8 and clean Ogre shutdown. New actual-native fixture passes1073 checks in
bothOd/O2: source0/1/2/3/8/16/64/65, exact growth expectations, independent TUS and
matching selected texture/material properties (scroll/scale/colours/shininess/
depth), native parent bindings, source stability, OFF/nonowner/unadmitted/wrong
caller forwarding, null/self/nonempty/already-capacious cases, reserve and logger
fault fallback, SDK failure/null readiness and forwarded sentinel exactly once.
Nested native copies pass without escaping state. A native assignment exception
inside the actual native copy constructor frees the added buffer exactly once,
observed through a pinned deallocator spy; the spy never changes allocation.

Sixteen independently compiled production-copy controls fail ordinary CHECKexit1
with clean Ogre shutdown. Canonical source/fixture hashes remain unchanged by the
runner. Thirty pinned on-disk contracts verify exports/caller/vector-init/native
free/EH path and both actual hair/item Material::clone imports. No InitRVAs or
game initialization is performed by these fixtures; only test-local SDK MinHook
initialization and real Ogre object constructors.

Important SDK trap found during independent review: inline TUS getParent/getName
use a different layout from the pinned engine. Production never reads TUS fields;
fixture parent checks call the native export. Texture names/scroll/scale are
native methods. This is not a claim of general SDK TUS layout compatibility.

All127 host suites pass, including independent live/defaultON/noRestart MCM row,
configuration roundtrip/golden records, page placement and Debug-last checks.
New labels/help are available in EN/RU/ZH;139-key MO catalogs roundtrip and RU
dash restrictions pass. An initial fixture context typo and old row-count
expectations were corrected before GREEN; those failures were not engine bugs.
The build source manifest was explicitly extended for the new implementation.
Full369-source DEV/PROD builds pass; final imports/hashes checked separately.

Independent review covered design, allocator/unwind proof, implementation,
stronger fault/content/admission tests and MCM/default/struct-order integration.
No remaining important finding. Review is focused; the source audit manifest
inherits earlier whole-source coverage plus this pass's actually changed paths.

## Player setting and limits

MCM Performance: "Prepare copied material storage" / "Подготовка копий материалов",
materialReserve=true by default, live toggle, no restart flag. Debug remains last
and OFF by default. Personal INI is preserved; source INI only documents the key.

This removes small pointer-array allocations during admitted material copies.
It does not eliminate native material/TUS creation, tint/program setup, texture
loads, model decoding, final NPC/equipment assembly or save/world restoration.
Actual loading/FPS/stutter gains and long-running leak behavior were not measured.
Wrapper overhead and material distribution in a real save remain unmeasured.

Scoped source commit/push, local install/backup, preserved settings/foreign files
and cleanup are recorded privately. Working release2, no new version/ZIP,
publication or personal published-changelog entry.
