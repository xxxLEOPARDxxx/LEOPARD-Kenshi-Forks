# Bone index table preparation, 2026-11-17

Working KEO release2, locally installed only. No ZIP, version increment,
publication or personal published changelog. No new game run, profiling,
game-log read or game process-memory access. Only retained CPU data, static
native images and isolated native fixtures were used.

## Profile and choice

The complete79.310028s recording remains
`load-20261117-144655-92db61b8`; original capture coverage/limitations and
profiled old68bDLL/MAP remain unchanged. `analyse_bone_index_profile.py`
reaggregates its retained decoded CPU tables, without starting a collector.
Main TID16080: buildIndexMap151 inclusive/43 leaf;35..40s110/32 out of4840
samples. Its set insertion105/98 total and76/71 in that window confirms
repeated work for every assignment, separate from the prior import and
normalization changes. Inclusive counts overlap. The selected-window ceiling
is2.27%, not an expected overall save-load percentage.

Edge topology import has457/168 total,231/90 in the window, but batching its
chunk/endian/stream/partial-state handling is a broader future task. General
allocator replacement and borrowed-node caching were rejected. Late rendering,
AI and barrier spinning have not been relabelled as NPC construction.

## Production

Whole exported Mesh::buildIndexMap hook, no mid-function runtime patch.
Existing live/defaultON meshWeights control; owner is checked before mutable
configuration. Tiny(<128), very large(>1048576), aliased/overlapping output
storage and invalid metadata refuse before output writes, then original once.
Unsupported ABI/export/native code keeps original handling.

A local8192-byte bitset plus128-byte occupied-word hierarchy collects unsigned
bone indices before any output growth. This replaces the temporary native set,
its duplicate searches and its allocation/free work. It deliberately removes
those temporary allocation failure/callback points, rather than claiming an
identical allocation schedule. Output growth is still native1B0570, inverse
map first then forward map. Size/data are reloaded after growth callbacks;
new elements zeroed, existing holes retained, shrink changes size only. Errors
propagate with native output partial state; no catch-and-forward after mutation.
Ascending occupied words/bits preserve bone ranks including65535/length65536.
No input-node borrow is used after growth, persistent allocation, cache,
worker, world pointer or per-frame work. Scratch8320B is automatic.

Admission pins1135 exact bytes: complete1A5E30..1A621E and grow1B0570..1B05F1,
plus exact export/PE identity and SDK List40/Map24/VBA12 sizes. Installer uses
the existing one-shot KenshiLib pattern and preserves a published trampoline
on SDK failure. Installer failure injection itself is not separately mocked.

## Checks and limits

- RED forwarding-only helper compiled and failed ordinary CHECK with native
  shutdown. A prior fixture link setup failed first (duplicate SDK Boost symbol);
  that was fixed with a raw ABI helper boundary and not counted as RED.
- Final O2 asset1314 checks; Od and/O2/GL/LTCG fixture1546 checks. Native oracle
  covers repeated/random/reverse vertices, all65536 bones, raw exceptional
  weight bits, unchanged MXCSR, output capacities/sizes/values, preserved old
  holes and deliberately poisoned unused capacity for zeroing checks.
- Growth-site interception is only inside the fixture's own mapped Ogre.
  Shrink/increase/swap/clear-source callbacks and throws on first/second growth
  compare against the real native method. Error injection models the native
  capacity-update/allocation boundary; it is not a real global CRT OOM test.
  Patches are restored before native shutdown and not written to game DLLs.
- Every admission proof byte mutation refuses. Owner/OFF/unadmitted forwarding,
  tiny/oversized input, output-object/storage aliases and late metadata
  refusal are covered. Six compiled semantic mutants all fail ordinary CHECK
  with native shutdown; compiler failure/crash is not counted as detection.
- Five real native mesh imports(male/female/hive/queen/robot),135745 assignments,
  produce exactly matching maps, including initialized previous output storage.
  Offline skeleton dependencies are intentionally absent; not an animation or
  exhaustive mod-asset test.
- Full389-source DEV/PROD builds and127 host suites pass. Existing normalization
  and ordered-import binary checks still pass.1135-byte contract, new helper/
  bridge unwind entries, stack probe for8320B scratch, integer-only operations
  and BSF enumeration are verified in final production binaries.
-183 KenshiLib/37 Ogre/65 MyGUI imports resolve;155 translations per locale
  unchanged, RU game strings retain supported hyphens. No new MCM row/field,
  restart requirement or translation key; Debug unchanged last/OFF.
- Independent native/code/perf/lifetime reviews report no P1/P2. Review found
  sparse-high enumeration was missing from the first benchmark; added coverage
  exposed50x regression with retained output capacity. The linear domain scan
  was rejected and replaced with occupied-word enumeration before installation.

Full production bridge/native microbenchmark, alternating order,24 independent
fresh output pairs, preparation/destruction outside timing:32000 assignments
over73 bone IDs943.43/73.51us, ratio0.078 (~12.8x isolated).128 low-domain0.250;
32000 full-domain0.023;65536 all-bones0.039;128 sparse-high fresh0.066 and
retained0.814. Sub-microsecond comparisons are noisy; no universal speedup is
claimed. Unoptimized Od is a correctness configuration (its retained sparse
benchmark is slower), not the shipped performance result. None of these ratios
establishes whole-save latency, remaining NPC stutters or developed-save FPS.

## Local artifacts

PROD5a869adcbf04e15f8c4952108f8268fc764540fc11735501b72a56c5c208b574,
2134016B; DEVe01679216cf107d5aed2611dfe221bcc540c27595f79c3289ed718b639ca3697,
2242560B. DLL+README only; catalogs/settings unchanged. Previous1d1528DLL and
README are retained in private bone-index-installed-backup. Install/source
receipts and test/evidence/cleanup manifests are private development artifacts.
Foreign7 Steel files and personal INI/modlist stay protected by all9 hashes.
