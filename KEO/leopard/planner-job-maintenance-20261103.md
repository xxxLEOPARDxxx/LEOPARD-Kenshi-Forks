# KEO planner: accepted job cancellation, 2026-11-03

Scope: one production file, `src/movement/island_cancel_hooks.cpp`, based on KEO `03a2581`. Routing configuration was only read: `keoRouting=false`. Installed DLL SHA256 was `52f7c723a497b22b2e5b19b3632a2699adb6d813dc07169770a44563b4402fde`. No game, UI, INI, installation, publication or original-author remote changes were performed.

## Confirmed lifecycle defect

The actual job hook calls the native original and K7/formation cancellation, but previously never retired planner plans. The native contract already documented in `game/bindings.h` says `Character::addJob` clears orders for a non-queued job (`addDontClear=false`). The planner's actual `PlanDropDue` deliberately retains halted and unchanged cached destinations. Therefore an accepted native job that changes its order state can leave the previous move plan alive; existing tick/replan policy can subsequently consult that stale goal. This is a demonstrated source/owned-buffer lifetime gap, not a live reproduction of the player's precise detour/circle or a claim of complete route-quality repair.

## Repair and boundaries

Around this synchronous non-queued job call, capture only existing planner owners from the existing capped live-player list. At most 64 snapshots; no new world enumeration, heap allocation, lock, thread or logging. After the native original, first revalidate live-player membership, the plan's exact slot/epoch/movement identity, then compare native order state. Drop only the same old plan whose order queue/head/type or current MOVE-to-other transition changed. A nested replacement, removed actor, unreadable order state or native no-op is not guessed to be cancellation.

Queued jobs bypass both list scans. Planner OFF returns before list access. Work occurs only on the explicit job event, never every frame. The existing player-list cap is 200; the worst-case bounded work includes plan lookup and live membership checks over those fixed limits. Snapshots are automatic POD storage and do not survive the hook. The original is invoked once with unchanged arguments; native exceptions propagate without post-success bookkeeping and no new global/TLS state requires restoration.

Nearest-character task cancellation remains outside this patch. A native job that makes no observable order-state transition also remains outside this repair: it is safer to keep its plan than infer acceptance from selection or clear all actors blindly. GOG support, duplicate/spawn placement, native hierarchy route quality and long-session FPS/leak acceptance are still unproven.

## Evidence

`leopard/test_planner_job_cancel.py` extracts the actual production hook/helpers and links actual `plan_store.cpp`, `plan_policy.cpp`, `plan_build.cpp`. Only the native original and native actor reads use owned buffers. RED before production changes: `FAIL accepted native job must retire its old route` in `build/planner-job-red.log`.

Final: 142 controls in each /Od and /O2. Covers accepted clear, unchanged cached/held destinations, unaffected actors, queued and refused/native-filtered no-ops, removed actor, unreadable pre/post, movement replacement, world reset, nested fresh-plan epoch, nested queued job, observe mode, and OFF zero-scan behavior. Two compiled negative controls fail on lost epoch authority and blanket cancellation, respectively. `build/planner-job-final.log` preserves the output.

The actual changed native translation unit compiles with the production VC10 /O2 /GL /Gy /MD /DNDEBUG /DZONEHAND_STEP=3 flags and current KenshiLib include set; `build/planner-job-tests/native-cancel.log`. No full 341-source rebuild was repeated. The linked local test candidate is `build/KEO_step4_jobcancel_prod/KEO.dll`, SHA256 `44096772665f9d3d6bd6bc106157f258634ac91b9b1d77217934c0f5fb99d940` (1,441,280 bytes). The incremental link reuses 340 objects only after validating all 341 old object identities and the previous 712-file source closure (only the changed translation unit differs); that one unit is rebuilt with the exact previous PROD compile-command hash. The 1,284 external dependency inputs are pinned, predate the prior full build, and were rehashed after link. Both required exports and all 144 KenshiLib imports pass against 0.5.0; full import sets equal the previous candidate. It is a test candidate, not a released or live-routing-accepted build. See `planner-job-candidate-20261103.json` for complete object/source identities.
