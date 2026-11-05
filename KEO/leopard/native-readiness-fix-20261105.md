# Preserve native readiness on city reload, 05 Nov 2026

The user's independent main-menu IMPORT LEO4 reproduction is decisive:
KEO ON first enters Squin normally, but after Hub unload/re-entry residents
appear outside; fresh identical import with KEO OFF stays inside. The earlier
comparison on an already persisted save did not exclude KEO.

Diagnostic D2EC3531 captured native-owned Squin cell(18,35). First approach:
outdoor navmesh ADD49.554 before KEO readiness override49.572. Reload:
outdoor RET101.741, Main state4 override while outdoor missing120.123,
transition end120.136, outdoor ADD120.928. No sampled bypass loss:1994calls,
557rows+1437coalesced,0dropped. Actual read-only snapshot557actors included
46 common-home actors from multiple platoons near the cell's southeast edge.
TownGuard refusals affected another cell(20,31); custom unload/save counters0.
The actual per-platoon placement call was not instrumented, so the trace alone
does not establish its exact timing. The subsequent single-variable control
does: same D2EC DLL/assembly and fresh import/out-back cycle with only
readinessOverrides=false retained NPC inside, confirmed by the user. Control
log shows bypass0/overridesoff. Original INI is backed up byte-exactly.

## Fix

ZoneReadinessContractDecide always returns ORIGINAL for every cell class and
just-loaded state. Current shipping builds use ZONEHAND_STEP=3 (contract is
consulted for >=2); original true remains true, original false remains false.
This applies to unclassified/game-owned and invalid classes as well. A drained
add list does not mean outdoor and building navmesh has finished generation.
There is intentionally no outdoor-only exception: interiors matter too.

The original readiness function is still called exactly once. Preloading,
worker generation, caches and queue prioritization remain unchanged. No new
allocation, lock, scan, native call or hook is introduced. State4 may wait
longer because the invalid readiness shortcut is removed. Existing hidden
INI flags are accepted but cannot re-enable early readiness at >=2; help and
comments mark that behavior retired. The old enum value is retained for
compatibility. Pre-step2 development code remains legacy, outside this fix.

## Verification and boundaries

The actual extracted hook links the real changed contract and tests6144 cases
per /Od and /O2: main/off-main, main phases, all flags, native answers, section
counts, loading states and valid/invalid ownership classes. Every result equals
native, the original and classification counts are preserved and queue
priority still runs once when eligible. Reintroducing the old policy returns
RED. Legacy diagnostic capture is retained as a separately linked old-policy
fixture with6144 cases per optimization and its missing-record negative test.
Pure contract units and cross-TU ledger/bucket tests also pass. Independent
review found no blocker.

The controlled native-wait INI is temporary and must be restored byte-exactly
after closing the game and installing this DLL. New-build gameplay acceptance
is still pending; do not treat the control as having executed the newly built
binary. No global path-quality, FPS/leak or GOG-new-jobs claim follows from it.
The user reports no errors/crashes with the last separately supplied GOG test;
the new readiness fix is not that accepted binary.

LoK update was prepared independently and its active files were held unchanged
through both captured spawn tests. It can be installed after these logs are
preserved. No public release/version bump/ALL_CHANGELOG entry. DF and satellite
remain paused.
