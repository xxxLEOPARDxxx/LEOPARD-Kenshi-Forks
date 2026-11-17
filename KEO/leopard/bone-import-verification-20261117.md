# Ordered bone-assignment import, working KEO 2

2026-11-17: concrete profile-backed change, locally installed, unpublished.
No game launch, route, new profile, gameplay-log read or process-memory capture
was performed. Only static files, the retained profile summary and isolated
native-oracle executables were used. General FPS work remains deferred.

## Why this path

The retained complete load profile `load-20261117-144655-92db61b8` has552
inclusive/152 leaf samples in SubMesh::addBoneAssignment during the35..40s
main-thread window(4840 samples total). These inclusive samples overlap with
mesh import and are not a whole-save latency percentage. Native369445 first
allocates a node through1BB050, then36944A..36948D performs another root search
before1BA270 links/rebalances and3694A5 marks dirty. The earlier linear weight
normalization correction is retained and targets a different operation.

## Final implementation and ownership

The whole admitted `SubMesh::addBoneAssignment` method is hooked. There is no
mid-function game patch. Owner/admitted/existing-live-meshWeights gates and
shared-vertex rejection forward to original once before node allocation.
An inexpensive precheck also forwards ordinarily unordered input to original.

For ordered input, a zero-extended vertex key and all12 record bytes are
copied before native allocation. No saved header, last-node pointer or input
borrow is used after that allocation. Current header/rightmost are read again;
equal or increasing keys append after the current last node. If allocation
reentry changes ordering, the original upper-bound algorithm searches with
the SAME allocated node. Native1BA270 owns linking, balancing, maximum-size
error cleanup and exceptions. Dirty is written only after successful return.
No plugin allocation, logger, catch-and-retry, manual free or callback is
inserted between allocation and native handoff. There is no new worker,
retained model/world pointer, growing cache or per-frame operation.

Admission pins1225 bytes: complete method369330..3694CC, allocator1BB050..1BB0C7
and link/ownership routine1BA270..1BA523; exact export, PE identity and VC10
SubMesh D0/F8, List40 and record12 layouts. Native allocator/free internals
remain called as before. Existing detours/refused contracts retain normal
handling. Installer follows the existing one-shot KenshiLib pattern and keeps
any published trampoline on failure; installer fault injection itself is not
separately mocked.

The existing defaultON/live **Speed up model bone weights** control governs
both import and normalization. Its EN/RU/ZH explanation was updated; no new
menu row or restart flag. Personal INI is preserved; Debug stays last/OFF.

## Independent native checks

- RED ordinary CHECK proved that pure forwarding lacked the optimized path.
- Final O2 and `/O2 /GL /LTCG` asset fixtures:9421 checks with native shutdown;
  Od and LTCG benchmark fixtures:9468 checks. Tests exercise sorted/repeated/
  reverse/random keys, full uint32 vertex indices, raw NaN/Inf/negative/zero
  weight bits, assignment ordering, source padding, dirty and MXCSR invariance.
- Allocator reentry inserts larger/equal keys, clears an aliased source node,
  swaps trees and throws bad_alloc. Post-allocation tree state is used.
- Maximum count is injected INSIDE allocation after obtaining the native node.
  Native length_error and native free exactly once are observed, dirty remains
  unchanged, and artificial count is restored before destruction/assertion.
- Isolated fixture hooks modify only its own mapped Ogre: native reader CALL
 1CABFF tracks input, and failure-free CALL1BA2BC counts release. Both restore
  before native shutdown; on-disk game DLLs are not patched.
- Five actual files are imported through the native reader, with ALL135745
  assignments ordered: male25269, female28711, hive17791, queen32476, robot31498.
  Original/optimized stored lists and weight bits match. Offline skeleton
  dependencies are intentionally absent; this is not a gameplay/animation test
  or exhaustive mod-asset coverage.
- Genuine Admit/Validate proof-byte mutations, modified-native refusal and
  owner/OFF/unadmitted forwarding are checked. Six independently compiled
  semantic controls fail ordinary CHECK/exit1 with native shutdown; crashes,
  compiler failures and loader errors do not count as detection.

## Performance and bounds

Complete production wrapper/native import of32000 assignments,12 fresh lists,
alternated order, excludes list destruction/input preparation: final LTCG
ratios optimized/native: ordered0.432, reverse1.085, random1.027. Native/helper
times ordered1505.87/650.00 microseconds. This is about2.3x for this isolated
ordered import, not2.3x overall save loading. Reverse/random pay small wrapper
guards; there is no promise of zero overhead for unsupported inputs. An early
prototype's reverse1.65 regression was rejected, leading to pre-allocation
unordered forwarding. All five tested native-reader streams take the useful
ordered path; other assets are not assumed to do so.

Full387-source DEV/PROD builds pass. All127 host suites pass; four relevant
MCM/layout suites pass again after final tooltip-only clarification. Three
MO catalogs roundtrip155 keys; RU contains only supported ASCII hyphens.
183 KenshiLib/37 Ogre/65 MyGUI imports resolve. New helper/bridge unwind ranges
and no floating arithmetic in the import path are verified in final binaries;
existing weight normalization's scalar/unwind checks also pass. Native assembly
has no call between native allocation and native link. Independent review
found no P1/P2 issue.

## Local artifacts

PROD `1d1528f35e075703dbd386c2cb51f6c8fc97ebd107127a59275b90a011fbf40e`,
2129920 bytes; DEV `fbe6cc38b65826027d028685497e0cfd446a48986dc7163328bbb3a59e10f160`,
2237440 bytes. Previous6903e3ca DLL/README/catalogs are backed up under private
`bone-import-installed-backup`. DLL/README/3MO are installed and verified,
all9 protected hashes unchanged, including personal settings/modlist and7
foreign Steel files. Raw retained trace and its profiled old MAP are untouched.
No ZIP/publication/version increment/ALL_CHANGELOG entry. Whole-save time,
remaining NPC stalls and developed-save FPS are still not measured as improved.
