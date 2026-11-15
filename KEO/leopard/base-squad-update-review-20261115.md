# Base/squad update boundary: private offline review, 2026-11-15

Baseline source: `03d28129c014c12548531674caac3e29c8b6cbee` (verified HEAD). Native input: on-disk `RE_Kenshi/Kenshi_x64.exe`, SHA256 `504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e`, admitted by the existing offline inspector. Labels come from the installed SDK export/RVA table. No game/process inspection, profiling, runtime capture, save edit or production edit was performed. This is a bounded contract review, not a frame-cost measurement.

## Findings

- `Faction::update` at `6BA9B0` advances `p_TIME`, processes the kill list, runs mission spawning, then active and unloaded updates. Skipping it for an empty visible area would suppress unrelated simulation work.
- `updateActivePlatoons` at `6BA810..6BA995` already returns when the active list count at `+210` is zero. Otherwise it calls each platoon's virtual update, handles persistence/deactivation/destruction and mutates the list. Its null-entry branch compacts one entry and returns. A snapshot count, cached list, offscreen test or reduced update frequency would change lifecycle/timing rather than merely remove redundant computation.
- `updateUnloadedPlatoons` at `6B9620` already processes at most one indexed unloaded entry per call (`periodicUpdateCounter_unloaded`, `+23C`), resetting the cursor at the list end. The selected entry can run periodic work, update, activate, and move between lists. The separate `_activateUnloadedPlatoons` / manager activation routines scan candidates and may activate one per faction. Their full sweep must not be confused with the ordinary one-entry update, nor presumed to run every frame without caller evidence.
- `Platoon::update` at `7EB1A0` runs `Blackboard::update`, copies the active/unloaded position, and delegates to the relevant virtual update. `ActivePlatoon::update` at `4FF930` checks unloading/carried characters, setup/persistence, recalculates position and may refresh trader inventory. These are simulation/lifecycle operations, not rendering work that can be omitted for camera invisibility.
- `hand::getCharacter` at `7974F0` is not a pure flat lookup: it distinguishes handle types and can enter death-parade/handle-manager fallbacks. `getCharacterOrPlatoon` at `4FDF90` performs a container virtual lookup and then active-platoon character lookup. A cached null/pointer would need creation, death, unloading and generation invalidation; the inspected code does not justify that cache.

## Concrete redundant allocation candidate

`Blackboard::update` at `26A2E0..26A4D7` has a narrower opportunity. It always creates a ten-element temporary handle buffer (10 x 32 bytes): size preparation at `26A325..26A349`, initialization at `26A356..26A385`. Only afterwards does it test the request-map size at `26A3A8`. On zero size it skips to `26A48D`, performs no request/timer/removal loop, releases the scratch buffer at `26A4B1`, and returns. The empty path contains no Blackboard state write, world call, RNG, activation or simulation timer update.

The field is already asserted in current production layout code: `Blackboard.requests` is `+158`, `RequestMapTable.size_` is `+20`, giving the existing composed accessor `KlibField_Blackboard_requests_size` at `+178`. `Platoon::update` calls this function before either active/unloaded delegation, so the redundant allocation is source-proven at each empty invocation; its frequency and CPU contribution in the user's session remain unknown.

An exact empty-request fast return is viable within the verified sequential native phase below. It must forward every nonempty or uncertain call unchanged. There is no lock inside the inspected routine: that fact alone does not prove that another request writer cannot run during the interval formerly occupied by allocation. Eliminating allocation also changes allocation-failure/allocator-instrumentation behavior; the equivalence claim is about native gameplay state under the supported owner phase, not preserving an otherwise unnecessary allocation's possible failure. No production change is included in this report.

## Follow-up: caller, writer and phase contract

The pinned executable's code section was searched only for direct `E8`/`E9` references to this update, add/remove writers, faction update and their jump thunks. Candidate instruction boundaries were checked against decoded `.pdata` spans; chained unwind records were followed for split functions. This is a scoped static xref result, not a claim to enumerate arbitrary indirect calls or third-party callers.

- The only native direct call to `Blackboard::update` found is `7EB1B8 -> 16CDE -> 26A2E0`, inside `Platoon::update`. Its return address is `7EB1BD`.
- Native `addRequest` at `269FB0` calls the map insertion helper at `269FEE` and writes the new/updated request directly. Its two direct callers are `334767` (function beginning `334690`, character/medic request setup) and `34F05B` (function beginning `34EEC0`, medic body task). Native `removeRequest` at `26A0B0` is just `this += 158; jmp erase`; its direct caller `33C8A2` is the medic body-task stop path. None of these routines wraps the request map with a lock or atomic protocol. The declaration uses an ordinary Boost unordered map, not a concurrent container.
- The supported update chain is `GameWorld::mainLoop_GPUSensitiveStuff 787E70 -> 788485 FactionManager::updateMT 2E74B0 -> 2E74EB Faction::update 6BA9B0 -> active/unloaded Platoon::update -> Blackboard::update`. The manager call returns to `78848A`.
- Before that chain, `mainLoop_GPUSensitiveStuff` joins the world-owned thread at `+790` through its `lockedWhileRunningMute +68` (`787ECA` acquire, `787ED3` release). It later acquires/releases the singleton background `ThreadClass +68` at `78844D` / `788456`, then synchronously calls `charsUpdate` at `788467` (or paused update), then the faction manager at `788485`. The singleton's `startRunning` call is afterwards, at `7884A9`.
- `ThreadClass::startRunning 3BED70` acquires `+68` before publishing `_running` and releasing the run mutex. `stopRunning 25F8C0` clears `_running` and releases `+68` at `25F8F2`. Thus the main-loop acquire/release is an actual completion barrier, not a misleading method name or a plain Boolean sample.
- Full `charsUpdate 7862F0..7866E2` executes its character traversal/callbacks synchronously and fills later background-work lists; it does not launch those lists before returning. The separate background routine at `786E00` calls `FactionManager::updateThreaded 2E7550`, which calls faction periodic updates rather than this `Blackboard::update`. Its native character work occurs before the background completion barrier; it does not overlap the later main faction phase.

This establishes a supported **phase** in which moving the empty request-map read before scratch allocation does not race those native writer callbacks. It does not justify a global `IsMainThread()`-only shortcut: a foreign or reentrant call may occur elsewhere on the same thread. A conservative implementation can scope a TLS marker around `FactionManager::updateMT` only for the known main owner and native return address `78848A`; the Blackboard detour skips only inside that admitted phase, at known native return `7EB1BD`, with readable zero request size. Unknown call sites, other threads, nested/unconfirmed phase, failed layout/admission or nonempty maps forward to the original. Phase restoration must survive exceptions and nested calls. Both hooks must be confirmed before allowing skips; a partial installation remains forwarding-only and retains its originals.

The native original's zero-size branch was read through its final return at `26A4D7`. The 320-byte scratch construction initializes local handle representations only; the zero branch performs no per-handle destructor callback before the single deallocation. Nonempty request timer updates and erase calls remain native. No new hook, runtime field read or skip counter was installed during this review.

## ABI and pinned bytes

| Item | Verified value |
| --- | --- |
| Steam executable identity | AMD64 `8664`, preferred base `140000000`, PE timestamp `65D604D7`, image size `232C000`; full SHA256 given above |
| Blackboard SDK symbol | `?update@Blackboard@@QEAAXM@Z` |
| SDK stub / RVA-table index / Steam target | `17610` / `1143` / `26A2E0` |
| Native ABI | `void (Blackboard* this, float frameTime)`, Microsoft x64 `RCX` / `XMM1`, no hidden return |
| First 32 bytes | `4055565741544155415641574883ec6048c7442420feffffff48899c24a80000` |
| Complete `[26A2E0,26A4D8)` SHA256 | `7e3385e963170a255b0a26a249a292f02849c05ec8e955d54c657ddab3033530` |
| Phase-owner SDK symbol | `?updateMT@FactionManager@@QEAAXM@Z` |
| SDK stub / RVA-table index / Steam target | `1A172` / `2994` / `2E74B0` |
| Phase-owner ABI | `void (FactionManager* this, float frameTime)`, Microsoft x64 `RCX` / `XMM1` |
| First 32 bytes | `48895c241048896c2418564883ec3033ed0f29742420488bd90f28f18bf53969` |
| Complete `[2E74B0,2E7542)` SHA256 | `30ebf545389b5a29dd4cadc836e7bb882c37f1be578dc7344f849b17c01030d1` |
| Blackboard call bytes at `7EB1B8` | `e821bb82ff` |
| Manager call bytes at `788485` | `e81db089ff` |

These are Steam facts. The existing GOG image was located but not analyzed for this candidate; GOG must retain native forwarding until its separate body/caller/phase contracts are verified. Before production, actual-source fixtures should verify empty/nonempty and unreadable cases, thread/caller/phase refusals, native argument and exception forwarding, nested restoration and partial hook installation. Native ownership fixtures should check the exact barriers/call ordering rather than just search for allocation bytes.

## Limits and disposition

No safe blanket squad/zone/job throttle is justified. The saved 651 platoon files are serialized records, not a count of live active squads or per-frame updates. Restart-persistent FPS loss and the owner's report that it existed in vanilla are compatible with restored simulation workload, but do not identify a cause or establish a leak. Existing `SquadWatch` source explicitly refuses to equate first observation with creation and has a Debug-off path before native enumeration; its historical diagnostic description is not a current-session census.

The only newly identified concrete redundant operation is the empty-request scratch allocation above. Actual game FPS, request-map emptiness rates and long-session memory behavior were not measured. Root's separate retention-index work must be assessed on its own source/equivalence evidence and cannot be presented as fixing the vanilla mature-base cause.

Evidence: compact `appearance-native-*20261115.txt` disassemblies for `6BA9B0`, `6BA810`, `6B9620`, `6B9860`, `6BA050`, `7EB1A0`, `7EB160`, `4FF930`, `7974F0`, `4FDF90`, `26A2E0`, `7EB390`, and the allocator import stubs at `ED562E`; SDK `Faction.h`, `Platoon.h`, `AI/Blackboard.h`; current `src/game/klib_member_fields.inc`, `klib_layout.cpp`, and `src/diag/squad_watch.cpp`. Explicit small ranges were used for split unwind chunks; their branch targets and epilogues were read, rather than treating the first `.pdata` fragment as the entire function.
