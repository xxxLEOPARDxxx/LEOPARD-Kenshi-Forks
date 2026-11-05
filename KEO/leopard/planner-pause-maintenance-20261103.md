# Planner pause-clock maintenance, 2026-11-03

Base: clean local `680a2b5` on `leopard`. Original jacobericson/KEO authorship and GPLv3 remain unchanged. This is temporary bug maintenance, not a new routing feature or a public release.

## Confirmed problem and change

The actual planner tick continued validating/searching/replanning slots while paused. Its plan, waiting and completion deadlines used raw elapsed wall time, whereas formation gather holds already used the shared pause-aware OOT clock. An unchanged plan could therefore exhaust its 120-second age, 8-second completion or 10-second section-wait budget during a long pause. Converting only Tick would mix clock domains with order capture and gather-resume rewrites.

The four production-file change uses one main-thread `BehaviorNow(rawNow)` seam, delegating to the existing OOT active clock. Order capture, repeat matching, merge record lookup, gather-resume rewrites and Tick's slot/merge ages use that same domain. The existing gather hold difference no longer converts already active time twice. Tick samples the clock every enabled frame, retains its original wall-time cadence, publishes pre-arrival frame data and processes generation retirement, then skips slot/world searches while paused. Its diagnostic reporting remains on wall time.

Native asynchronous request-flight QPC deadlines, locks, worker concurrency, graph/search algorithms, path costs, limits, water behavior, cancellation, priorities, settings and defaults are unchanged. They are a separate bounded result/ownership handshake, not these simulation-age deadlines. No per-frame heap allocation, cache, thread, retained pointer, timer, INI read or native write was added. The old per-slot OOT conversion is removed; the active-clock conversion is once per tick and per relevant order boundary.

Исправлена подтверждённая ошибка: пауза больше не расходует время ожидания и срок действительности плана маршрута. Планировщик не запускает повторные поиски маршрута на паузе. После снятия паузы отсчёт продолжается; обычные сроки ожидания сохранены. Это не подтверждение исправления всех крюков, кругов и остановок в реальной игре.

## Test evidence

- Actual old extracted `PlannerTick` produced RED: `FAIL paused actual tick does not scan or replan slots`, retained in `build/planner-pause-red.log`.
- `python leopard/test_planner_pause_clock.py`: 18 actual boundary controls each `/Od` and `/O2`; six separately compiled negative controls fail their intended runtime assertion. The fixture uses the actual extracted Tick, actual caller timestamp expressions, actual hold expression, actual OOT implementation and actual `PlanReplanDue`. Native seams only observe work/arguments; no game functions/process are invoked.
- Cases: order on pause, 600-second pause/resume, capture/merge/resume domain agreement, no double-conversion of gather hold, unchanged 8/10/120 active-second deadlines, wall diagnostic cadence, generation retirement during pause, routing OFF.
- Existing `plan_policy_units`, `plan_store_units`, `plan_merge_units` and `order_outcome_table_units`: four suites pass. Log `build/planner-pause-tests/existing-suites.log`.
- Changed native translation units `planner_capture.cpp`, `planner_merge.cpp`, `planner_tick.cpp` compile using the real KEO SDK with VC10 `/O2 /GL /Gy`; logs under `build/planner-pause-tests/native/`.
- `git diff --check`: clean. No full compile matrix was repeated; no final DLL link or installation performed by this task.

Host/static evidence establishes these deadline/caller properties, not in-game route quality, FPS or absence of all long-running leaks. The preserved personal installed `keoRouting=false` was read only. Installed DLL still E1558949...; prior newer priority-publication candidate D396C05A... remains separate and uninstalled. No game/INI/UI/publication/commit actions.

## Scope

Production: `src/planner/planner_tick_internal.h`, `planner_tick.cpp`, `planner_capture.cpp`, `planner_merge.cpp`.

Proof: this report, `leopard/test_planner_pause_clock.py`, `leopard/planner-pause-verification-20261103.json`. The manifest pins direct test inputs and compact logs; source/read-only installed identities are recorded without distributing personal settings.
