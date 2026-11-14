# Private startup seed audit, 2026-11-15

Read-only inspection of the pre-change resource_startup_native.cpp / resource_cache_store.cpp and actual-production-code fixture test_resource_startup_boundary.cpp. No production/test-source edits, runtime interaction or process/memory access. The parent is editing the startup code concurrently; this is a design/verification audit of the inspected old version, not a claim about the final patch or its tests.

## Cause supported by code and snapshot

SeedExisting runs at most once per250ms Heartbeat. Its old code processes exactly one resource group per call, even when that group is empty, removed, or contains only one location. Each such small group therefore costs an entire heartbeat while leaving most of the32-location budget unused. At10.249s the saved runtime-texture-retention-first snapshot has only69 registered roots/no startup inventory; at20.25s it has598roots/7379files and has begun reading. These two snapshots support incomplete root seeding at10s, not a measurement that one native group call itself took10s.

The worker also waits until2000ms after rootsChanged. RegisterTextureRoot updates rootsChanged, increments startupGeneration and resets published startup progress only for a new normalized root or a recursive upgrade. Same-root duplicates and recursive downgrades return immediately without resetting anything. ConfigureStartup and RAM configuration likewise do not restart unchanged values. Thus slow delivery of new roots keeps restarting the two-second quiet period; frequent ResourceCacheTick calls alone do not.

There is NO seed-complete handshake. StartupStep only requires a two-second gap since the last effective root change. It can start before seeding is complete if seeding spends>=2s crossing empty/removed/duplicate-only groups; a subsequent new root cancels that partial epoch and starts a fresh pass. Do not describe the present delay as a guaranteed wait for complete enumeration.

## Smallest defensible fix

Consume the SAME32 newly visited LocationList entries across at most8 group attempts per heartbeat. Keep the whole seed capped at8192 entries/256 group names, keep250ms heartbeat throttling, main-thread checks, native forwarding, path copying and the existing IO worker. Empty lists and removed-group exceptions must each consume one of the8 group attempts; rejected/null/non-FileSystem locations still consume the32 entry budget, as before. The entry budget is outside the per-group loop and must never reset when advancing to another group.

Only the first, previously partial group can have nonzero seedOffset. Once it finishes, subsequent groups in the same heartbeat begin at offset0. Consequently this batching does not multiply the existing rewind/skip cost across8 partially processed groups. It does add up to8 native group lookups per heartbeat, which is why the group-attempt bound is necessary even for empty groups.

A soft QPC deadline checked before starting another group is reasonable if needed. At most a handful of QPC reads per seed call suffices; do not add QPC calls to every location. Clearly distinguish a cooperative budget from a strict1ms guarantee: a single native group lookup, Remember/register operation or a large rewind can exceed it. This change should not claim zero FPS impact without a game measurement.

Keep the two-second quiet period and every worker memory/IO/pause policy unchanged in this minimal patch. Faster seeding moves the same effective last-root timestamp earlier and reduces the chance of a partial inventory restart. It does not change requested gameplay bandwidth or start IO on the main thread.

## Do not add a naive iterator-advance cap

The current code reconstructs `it=list.begin()` and skips seedOffset entries each heartbeat. That has existing quadratic total traversal for one large group. A naive128/64 total-advance limit cannot resume: after seedOffset exceeds that limit, every heartbeat reaches the cap before reaching its saved offset and makes no progress forever. A QPC deadline inside that rewind has the same starvation problem.

Retaining an Ogre/std::list iterator across native frames instead would require a proven invalidation contract for group/location removal; the Add hook does not cover removals. A cached iterator or ResourceLocation pointer must not be introduced merely to make a numeric cap look bounded. No Ogre pointer/iterator should cross to the IO worker.

For this focused change, keep the old large-group rewind behavior and document the remaining bound; do not combine a cursor-lifetime redesign with the initial-delay fix. Full seed traversal remains finite under the existing8192-entry cap (single large group about1.05million list increments across its256 slices, with the last slice skipping8160 then handling32), not O(32) per heartbeat. A strict per-heartbeat traversal redesign is separate work.

## Required focused fixture updates

The current fixture includes the real production file, preserves native forward/failure semantics, tests a removed group, a global8192 ceiling and600 simulated60Hz frames. Retain those checks. Its old one-group expectations must change intentionally; do not weaken the32/global cap assertions just to get a pass.

1. For the existing General40 / Removed / Later40 fixture: heartbeat1 visits32 in General; heartbeat2 visits8 from General, skips Removed and visits24 from Later (64 total, Later offset24); heartbeat3 finishes16 (80 total) and releases seedGroups. No later heartbeat restarts the seed.
2. Add distinct small/empty groups: eight1-entry groups finish8 in one heartbeat; a ninth waits until the next. Eight empty/throwing groups consume8 attempts without an unbounded walk. Later valid groups remain reachable after removed ones.
3. Mix a partially processed large group with small groups: the total new entries remain<=32 across transitions; seedOffset and seedVisits increment once per visited entry, including null archives and Remember-rejected locations. Counters must not count rewound skips as new entries.
4. Preserve8200-location case: exactly8192 visits globally, bounded group snapshot, terminal seedGroups release and no subsequent reseed. This also catches the naive128-advance starvation mutant.
5. If a deadline is introduced, use the fixture QPC clock with deterministic advancement per group; assert progress persists between calls and that budget exhaustion does not skip or duplicate a location. Do not rely on real scheduler timing for a unit test.
6. Preserve main-thread/nav-stop, both Root overload forwarding,250ms throttling, menu responsiveness/one-second recovery and600-frame60Hz checks. No seed change should alter menu lease behavior.
7. Preserve store tests proving duplicate root/unchanged configuration does not reset progress, new roots and recursive upgrades do reset, and no scan starts before the existing2s quiet period. A separate assertion should acknowledge that seed completion is not presently an eligibility condition.

Useful negative controls: reset32 budget per group => fail; ignore8-group limit for empty/throwing groups => fail; naive rewind cap128 => large-group progress fails; restart completed seed each heartbeat => fail. These test behavior, not source spelling.

After final edits, run the existing compiled wrapper/title suite and relevant startup/store suite once. Avoid concurrent runs into the same build/resource-startup directory while the parent is editing/testing. Runtime confirmation should report time to inventory/read start and time to completion separately; leaving the menu before completion ends fast pacing and is not evidence that the seeding change regressed total warm time. This saved run left fast mode at64.337s after about12.63GB and only completed104.989s, so it does not provide an uninterrupted-menu completion benchmark.
