# Planner destination TLS unwind maintenance, 2026-11-03

Separate bounded patch on local base `680a2b5`, alongside the independently frozen planner pause-clock patch. Original jacobericson/KEO authorship and GPLv3 unchanged; no publication, version bump, game/INI/UI/installation/commit action.

## Confirmed failure and repair

Actual `hook_setDestinationVec3` saved `t_plan` at entry and restored it only at normal return. An exception escaping the original destination routine left the interrupted character/slot in thread-local routing context. Owned host originals reproduce this with C++ exceptions; the full regression additionally covers SEH. This proves the stale-context defect, not that it caused a particular player's reported circles.

Only this wrapper changes. A POD `__try/__finally` restores the exact saved context after both ordinary and exceptional exits, including nested calls. The original function still receives identical arguments exactly once. Existing post-success diagnostics execute in their original position; exceptions are not caught/swallowed or retried. No routing algorithm, native address/profile, queue, budget, worker concurrency, gameplay setting or default changes.

No allocation, lock, retained pointer, logging, world walk, polling or own worker is added. Normal entry does the same bounded TLS copy as before; finally extends that restoration to unwind. Runtime CPU/FPS and long-session leak freedom are not established by this host test.

Исправлен подтверждённый случай, когда после ошибки при назначении пути в контексте потока оставался предыдущий персонаж. Контекст теперь восстанавливается и при исключениях; новая логика маршрутов или принудительные повторные запросы не добавлены.

## Verification

- Before edit: `leopard/test_planner_tls_unwind.py` produced actual RED `FAIL normal/C++/SEH path restores exact previous TLS` (log `build/planner-tls-red.log`). Earlier isolated actual-body reproduction is under `build/planner-pause-tests/tls-audit/result.log`.
- Final `/Od` and `/O2`: 51 actual production-wrapper controls each, two compiled negative controls fail runtime assertions. Normal, nested, NULL destination, unarmed routing, C++ unwind, SEH unwind, original exactly once and an independent thread's TLS isolation are exercised. Test uses owned stack movement buffers and originals, never a game process or native engine function.
- Actual `src/planner/planner_hooks.cpp` compiles against current KEO SDK with VC10 `/O2 /GL /Gy`: `build/planner-tls-tests/native-hooks.log`.
- `git diff --check`: clean. No final DLL link performed at this stage.

Scope: `src/planner/planner_hooks.cpp`, `leopard/test_planner_tls_unwind.py`, this report and its small `planner-tls-verification-20261103.json` input/log manifest. The separate pause-clock files were not modified by this second change. Installed DLL and personal routing state remain unchanged.
