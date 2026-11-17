# KEO3 release review and preparation - 2026-11-17

Published registry is authoritative: KEO2 was published13 November2026. Earlier
working2 labels during the loading work were stale. The accumulated update is
KEO3 (one increment), not a new release per intermediate fix.

The owner now confirms faster save loads and warmed zone-to-zone transitions.
This is gameplay feedback for the combined code already installed. No controlled
overall timing factor is claimed from individual helper benchmarks. No new game
run, profiling, capture, live gameplay log or process-memory read was performed.
This phase reviews/packages earlier code; it adds no new gameplay implementation.

## Independent review

Resource/startup review through4b292300 relative tod46176c: no P1/P2 blocker.
Checked RAM and shared GPU budget accounting, active-stream lifetime afterOFF,
weak handle identity, pending queues, pressure eviction, startup seeding,
world/device/Ogre teardown and exact-byte consumer fallback. New memory features
remain live/defaultOFF; Debug remains last/defaultOFF. Existing bounded refresh
work is not described as zero cost or a proof of all long-session memory behavior.

Fresh isolated extracted/native resource checks passed: startup1255 plus native
title helper and ordinary negative control; HookBatch287, adapter511, SDK
admission10 eachOd/O2; geometry6481, texture3315, RAM identity/lifetime/OFF/pressure,
WARP GPU store/DDS readback and TLS/COM upload-adoption mismatch fallback. First
RAM fixture invocation lacked its fixture directory; corrected isolated setup
and rerun passed. These are not gameplay or profiling runs.

NPC/save review: no P1/P2 blocker in bone weights/import/index, save stream and
field/list/tree transfers, equipment/animation reads, mutex reuse, material/model/
skeleton reservations and morph leases/uploads. Native ownership, input snapshot
before callbacks, reload after allocations, partial exception state, OFF/fallback
and bounded scratch were checked. Existing final native/mutant evidence remains
in the corresponding verification reports. The full NPC pipeline was not moved
to worker threads, and not every remaining load stall is claimed removed.

## Previously dirty job compatibility changes

Seven existing Steel integration files were preserved byte-for-byte during the
loading tasks. Final release review explicitly covered them before committing
d907374: Stock transfer admission failure disables stocking only; other jobs stay
available. Restored stock jobs remain representable. The new Rotate Items build
is pinned by exact PE/SHA/target/relay contracts; no generic hook bypass was added.
Startup refusals log errors, and successful admission adds no diagnostic noise.
Fresh runtime321, stock69, shared-shape83, startup/hot admission123 checks each
Od/O2 passed, including runtime negative controls. Both historical and current
Rotate DLL identities were checked. This preserves the already installed/tested
binary instead of silently rebuilding without the seven changes.

## Candidate and packaging

Production DLL is the previously installed2134016B binary,
SHA2565a869adcbf04e15f8c4952108f8268fc764540fc11735501b72a56c5c208b574.
It includes the reviewed job changes. Candidate DLL equals both installed DLL
and workshop/Nexus payload; source commits now account for that binary's code.
389-source paired builds/127 host suites and prior binary checks are retained.
No binary rebuild or new runtime optimization was needed for documentation-only
release preparation. README has a player memory guide and bilingual v3 notes;
the old v1/v2 histories remain. English/Russian public guides are complete and
under Steam's limit, with original credits/source links and no version-history
duplication or private project announcement.

Canonical prepare_release.py used an explicit clean input root for KEO3,
SkillLevelUpInfo5 and BestMinimap5. CRC/member hashes/defaults/threeMO catalogs,
RU glyph restrictions, x64/startPlugin/MCM exports and installed import resolution
were verified. Old binary ZIPs were retired only after replacement validation;
37 unrelated/source archives were protected. Personal INI/defaults/modlist hashes
remain unchanged. No Nexus/Steam upload or personal published changelog entry.
Detailed receipts and cleanup manifests remain in private release-loading-20261117.
