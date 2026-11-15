# Benchmark clock ownership, 2026-11-15

Stage-only correction in `src/bench/bench_pin_run.cpp` and its API comments. No hook, game execution, profiler, capture, full DLL build, installation or settings change.

The always-called idle tick previously normalized any admitted nondefault clock rate, even before a benchmark was armed. A later write by another mod was therefore overwritten by an unused benchmark.

A successful actual KEO clock write now records restoration debt outside resettable pin state. Observing an equal rate or a refused write creates no debt. Release/idle retry only that debt; failed restores preserve it, successful restoration clears it, and a later differing foreign write relinquishes KEO ownership. Shutdown drops debt before any clock facade access. Native readiness and the existing clock-write facade stay unchanged. Default idle behavior is a phase/debt/stop check without any clock facade access. No allocation, scan, worker or added timer.

`leopard/test_bench_clock_ownership.cpp` includes the actual production pin-run body, links actual pin parsing/progress/rate policy, and controls only the game facade. It covers one million idle calls leaving a foreign rate untouched; no-write arms; refused pin writes; failed restore/retry; debt across Begin reset; restoration followed by a later foreign write; equal foreign rate; temporary readiness loss; and shutdown with no facade access. Old production compiled, 33 controls reached, 12 runtime assertions failed. Final VC10 x64 /Od and /O2: all 33 controls PASS. Six compiled /O2 mutants fail at runtime: missing debt gate, debt on refused write, debt lost on restore refusal, debt reset by Begin, overwriting a later foreign rate, and shutdown facade access. Runner: `python leopard/test_bench_clock_ownership.py`; logs under `build/bench-clock-ownership`.

No timing or FPS claim. A foreign write of the identical floating-point value is intrinsically indistinguishable from KEO's own last value; the fix does not claim an external-writer notification API.
