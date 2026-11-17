# Retained loading-profile review, 2026-11-17

**Outcome: no new production optimization installed.** The dense parallel-pose
candidate was rejected after checking real ordinary mesh data. Production code,
MCM rows, catalogs and default INI are restored to commit226af2d. Installed KEO2
remains SHA25668b029bd72c665e858feacdd91e14852e3d933cf3a98576601312d6eb3dda511.
No game launch, save traversal, current game-log read, WPR recording, capture or
process-memory collection was performed. All work used previously retained
files, static engine/model files and separate fixture processes.

## What the retained evidence establishes

`leopard/review_retained_loading.py` reaggregates the existing analyses and
decoded ETW CSVs; it records input SHA256 values in its output. It never starts
or attaches to Kenshi. Default output: `build/pose_cpu/retained-review.json`.

| Retained slow-record set | Calls | Selected elapsed | Mesh callback child | Morph-create child | Pose-blend child | Animation-body child |
|---|---:|---:|---:|---:|---:|---:|
| appearance preparation |57|1262.2ms|924.7ms|441.0ms|122.6ms|5.2ms|
| narrow texture startup |55|1339.2ms|968.9ms|379.8ms|117.2ms|4.0ms|
| Lagoon-Mourn, texture retention off |52|1160.5ms|894.9ms|431.8ms|130.6ms|4.3ms|

These are sums over SELECTED slow completion records, not all frames or whole
save loading. Child measurements are inclusive and nested; do NOT sum them.
They include waits/native buffer locking/uploads, not just CPU arithmetic.
They predate later morph-pose borrowing, upload batching and related fixes.
Routes differ, so these sets are not a controlled comparison. They establish a
historically expensive resource-completion/body-creation chain; they do not
establish its current cost or identify the dominant CPU save-parser function.

The ETW session reports80583.6412ms, but retained target CPU samples cover only
75785.3303..80458.997ms: **4673.6667ms**, with9670CPU samples and448missing stacks.
Rendering and steady work dominate this suffix. Range tags over all retained
stacks: save-parser6C1330..6CB000=0; morph-create71A80..71D74=0;
pose-blendOgre1A9BE0..1A9EA1=0; broad resource-finish44BBA0..44CC00=1.
Zero samples do NOT prove those functions were absent in the session; the
retained target samples do not cover the earlier span. They cannot rank whole
save loading. No missing profile data was invented or inferred from wall-time
zone dwell counters.

## Candidate actually investigated

The earlier field-key/animation-lock paths are not established dominant costs.
This investigation instead tested native softwareVertexPoseBlend, an explicitly
measured child of body creation. The proposed immediate accelerator preserved
each native call boundary and used two helper threads for disjoint CPU-owned
scratch ranges, with main-thread native locks/publication. No NPC spawning,
scene objects, RNG or GPU API was moved to a worker.

Across-pose deferral was rejected before shipping: GameData lookup can allocate,
and reentrant callbacks may observe intermediate registered geometry. Pinned
mesh lifetime alone does not prove this observation impossible.

The isolated native arithmetic oracle passed193checks each /Od and /O2 after
isolation; the empty baseline fails ordinary CHECKexit1. It exercises native
Ogre arithmetic/lock/unlock, four rounding modes, FTZ/DAZ, sticky flags, target
NaN/signed-zero/subnormal values, exact bytes, worker joins, buffer references,
C++ late-upload failure and shutdown. This remains an experimental fixture,
not complete production verification. A first metadata fixture used the wrong
stream-owner allocator and was corrected with caller-owned/freeOnClose=false;
the corrected five-asset import exits0 with normal Ogre shutdown.

Initial synthetic dense-map native-time ratios were0.80/0.75/0.64 for4096,
16384and65536entries, including two full-buffer copies and dispatch/join. Sparse
or clustered cases instead ranged1.09..3.35: slower. Full production-scope and
guard overhead was not included in those figures. No game speedup follows.

## Actual ordinary mesh coverage

Static native MeshSerializer imports in an isolated CPU-buffer Ogre process:

| Mesh |Vertices|Stride|Poses|Largest position map|Largest normal map|Proposed eligible poses|
|---|---:|---:|---:|---:|---:|---:|
|human/human_male.mesh|15504|24|23|2040|0|0|
|human/human_female.mesh|16614|24|29|1511|0|0|
|stick_person/Stick_person.mesh|9414|24|9|1735|0|0|
|stick_person/Hive_Queen.mesh|25650|24|0|0|0|0|
|robot/robot.mesh|29727|24|0|0|0|0|

The threshold/density/nonempty-normal conditions that made synthetic parallel
work worthwhile do not cover these imported ordinary bodies. Removing the
guards would admit the slower sparse workloads. Modded assets and later live
postprocessing are not exhaustively covered; no claim is made about every race.
This is sufficient to REJECT presenting the candidate as an ordinary NPC or
save-loading optimization. It remains outside `src` in `leopard/pose_cpu_spike`,
absent from `coresrc.txt`, packages, runtime hooks and MCM. Its remaining
production-quality gaps are listed in the experiment README.

## What remains unresolved

The actual current heavy CPU chain of complete save restoration is unknown
from these retained files. Old resource-completion wall time cannot distinguish
present CPU decoding, native resource construction and driver/synchronization
waits after later fixes. The user's lack of a noticeable save-load improvement
is not contradicted by microbenchmarks or successful correctness tests.

There is no evidence here for promising a measurable new whole-save speedup,
offloading live NPC construction or choosing another speculative DLL change.
No fresh gameplay measurement is requested under the current constraint. The
performance objective remains open; this report does not label it complete.

Independent reviews by material_copy_design_review and morph_upload_design_review
agreed that the experimental dense-path evidence did not justify production.
Seven foreign Steel files and personal INI/modlist hashes are unchanged.
No release ZIP/version bump/publication/personal published-changelog entry.
