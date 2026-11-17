# Full load profile and linear model-weight preparation

2026-11-17; working KEO release 2, locally installed, not published. No new
release package, version increment or personal published changelog entry.

## Evidence and scope

The owner explicitly authorized a new complete save-load-through-NPC-ready
profile, replacing the previous no-new-capture constraint for this run.
The owner prepared the menu, received the load instruction only after WPR
File-mode and begin acknowledgement, then marked NPC readiness. Capture:
`etw_tools/captures/load-20261117-144655-92db61b8` in the private game stage.

Sequential ETL 617420557 bytes, 79.310028 seconds; target samples cover
7.5773..79279.9524 ms and all 80 second bins. There are 145621 CPU samples,
1143 missing sample stacks (~0.785%), no reported lost events. Owner signal
boundaries 18740.5643..78635.9088 ms are not exact engine click/load phases.
Matched waits exclude unpaired edge gaps: 254 gaps, 235 uncertain lifetimes,
186 intersecting the owner window. Missing blocking stacks/owner mismatches
are reported separately. No GPU timeline, dedicated disk IO attribution,
zero-overhead claim or complete frame-by-frame latency proof is made.

Independent CPU review uses main/render TID16080 at35..40s:4840 samples,
ResourceLoader::loadModelEntityDetached2771 inclusive; Ogre mesh import1226,
Entity initialization1233, rationaliseBoneAssignments1043 inclusive/549 leaf.
Inclusive categories overlap. Across the trace, direct samples inside the
rationalise helper show765 normalization/tree-scan,11 count-loop,14 tail,
zero in the >4-pruning branch. The expensive ordinary path repeatedly walks
the multimap from its root for each vertex; RAM file warming cannot remove
that CPU work. Late renderer/AI/PhysX samples are not relabelled NPC assembly.

## Production change

`mesh_weights_linear.h`: an integer-only first traversal validates complete
contiguous vertex groups,1..4 assignments each, matching vertex indices,
ordinary nonnegative finite[0,1] weights, nonzero groups, <=1048576 vertices,
and masked SSE exceptions. No node/out/FP-status mutation occurs on refusal.
The second traversal retains native multimap order, scalar sum, fabs(1-sum),
FLT_EPSILON threshold, one reciprocal, then reciprocal*weight. It changes no
topology, node ownership, bone indices or animation order.

`mesh_weights.cpp`: main owner is checked before live config; admitted and
enabled eligible calls use the linear helper. Other cases call the original
exactly once. New live/defaultON `meshWeights` setting has EN/RU/ZH labels;
Debug remains last/defaultOFF. No new worker, retained world pointer,
allocation/free, per-frame scan or growing cache is introduced. Rejected
large/late-unusual inputs can pay the bounded validation traversal before
native fallback; no universal improvement is claimed for such inputs.

Admission verifies installed Ogre timestamp/size/export identity and2383
bytes: complete helper1A5510..1A5D6B, count helper1AC010..1AC0F6 and actual
FP constants. VC10 List40/VBA12/weight+8 ABI assertions are compiled.
Unsupported/detoured builds retain native handling. Hook installer follows
the existing one-shot KenshiLib pattern and preserves any published
trampoline on failure. Installer fault injection itself was not separately
mocked; Admit/Validate/wrapper are tested directly.

## Verification

- RED stub returned false and failed an ordinary CHECK with native shutdown.
- O2 and Od native oracle: all rounding modes x DAZ/FTZ, epsilon thresholds,
  minimum-normal/underflow, raw exceptional values, mixed counts, shuffled
  insertion, duplicate bones, late bad groups, refusal bytes/out/MXCSR,
  node address/order preservation and native fallback gates.
- Final `/O2 /GL /LTCG` actual-production fixture:2184303 checks, normal
  native shutdown, all5 imported asset lists qualify and match weights
  bitwise. Imported snapshots: male15504/25269 assignments; female16614/28711;
  hive9414/17791; queen25650/32476; robot29727/31498. This does not cover all
  mods or live postprocessing. Skeleton dependencies are intentionally not
  loaded by the offline mesh fixture; no animation/gameplay proof is claimed.
- Six compiled negative controls fail ordinary CHECK/exit1 with native
  shutdown; compilation errors/crashes are not counted as killed mutants.
- Cold independent16000-vertex lists with1..4 bones, normalized and requiring
  normalization: linear/native helper ratios0.074..0.169. Copies excluded,
  execution order alternated. These are isolated helper timings, not a
 6-13x whole-save speedup or measured in-game wall-time improvement.
- Complete385-source DEV and PROD builds pass. Final code has scalar
  ADDSS/SUBSS/DIVSS/MULSS, no reciprocal approximation/FMA/x87 replacement;
  MAP boundaries include LTCG's multiple unwind fragments. Entry/wrapper
  unwind records verified for both variants.
-127 host suites covered:124 pass in the final full sweep; remaining3 pass
  after correcting new-key golden counts and supplying KENSHI_ROOT to the
  pre-existing render fixture. MCM checks defaultON/live/noRestart and actual
  off/on callbacks. Source/installed MO155 keys each, roundtrip passes;
  RU strings contain no forbidden non-ASCII dashes.
-183 KenshiLib,37 Ogre and65 MyGUI imports resolve against installed DLLs.
  Independent production review found no important blocker.

## Installed artifacts and remaining work

PROD SHA256 `6903e3ca3b6c60ca926f96d7c167b6ce9a6e58d30964ac2898b9d80f8a0900fa`,
2126848 bytes. DEV `7de5fced052bddad80389e01e051f29afa7d49225f89f4c0f633eae1ee348038`,
2233856 bytes. Installed DLL/README/3MO verified; all9 protected hashes,
including personal INI/modlist and7 foreign Steel files, unchanged. Previous
68b029 DLL/catalogs remain backed up in `mesh-weights-installed-backup`;
personal settings were preserved without modification.
No game launch/new profiling after installing this change. Full save-load
acceleration, all NPC stalls and developed-save FPS remain unproven/open.

Next profile-backed CPU candidate: ordered SubMesh::addBoneAssignment
insertion(552 inclusive/152 leaf samples in the same5-second window). Native
node allocation precedes an inline root search. Any future optimization must
inspect CURRENT tree after allocation, preserve equal-key order, ownership,
max-size cleanup, dirty flag and exception behavior. No speculative
mid-function patch was installed. General FPS remains outside this pass.

TimeLight map comparison/proposal is separately documented in the ours repo;
no map runtime code or worker was changed during this task.
