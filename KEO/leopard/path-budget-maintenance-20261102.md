# Request-scoped path budget maintenance, 2026-11-02

Base: `49b1550dc5b270f6c26737659d792b046467976f` on `leopard`.
This patch is separate from the concurrent MCM adapter. The native candidates
include that adapter's frozen sources; their hashes are recorded separately.
No game, personal INI, installed DLL, commit, or publication was changed.

## Confirmed defect and bounded change

The previous budget branch read `currentRequestIsPlayer`, a TLS boolean assigned
by consuming a global count of new player orders. An older repath can consume
the new order's token before that order reaches the native search. The real
submit/tag/resolve fixture reproduces that interleaving: both requests carry
player priority, but the fresh order loses the original four-times allocation.
The old tag can also survive until an unrelated native search on that thread.

The existing fallback hook now captures its own synchronous request from
`resultBuf - 128`, plus its priority and the native NPC/unresolved flag. A POD
frame publishes that bounded stack scope in TLS and restores the previous frame
and native owner stamp in `__finally`, including C++ and native unwinds. Only
the existing audited character A* return site can consume it, once. Gate,
generation, Havok and unknown calls neither consume nor borrow the context.
The old counter and tag remain diagnostic inputs only.

Eligibility requires the current request, the audited character caller and the
native NPC flag false. This includes native-positive low-priority player
queries and repaths. The priority-based `playerByReq` label remains unchanged
because the author also uses it for hierarchical and NPC-cap policy. An actual
low-priority/native-positive regression observed RED with the old conjunction,
then GREEN when only the budget decision followed the authoritative flag.

The two original KEO allocations remain exactly 524,288 and 2,361,344 bytes.
No hierarchy, traversal, water, queue, worker-count or request-priority behavior
was changed. Priority publication after native submission and planner pause
timing are separate confirmed concerns and remain unchanged.

## Native evidence

`leopard/audit_path_budget_native.py` reads the actual offline
`RE_Kenshi/Kenshi_x64.exe`, SHA-256
`504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e`.
It asserts 11 exact byte windows and seven call/thunk edges without loading or
executing game code:

- `3AEDE3` computes `request+0x80`; `3AEE9F` passes that same pointer as the
  fallback's ninth argument. `3AEED6 -> 3BDC2 -> 3AABF0` is the fallback.
- `3AEE7A` resolves the same request's character; `3AEE8B -> 1EF97 -> 790B30`
  invokes native `Character::isPlayerCharacter`. `sete` constructs the NPC flag:
  false requires a resolved native player; unresolved and NPC paths are true.
- The character's detailed search returns to `3AADAD`; the existing classifier
  recognizes this exact caller before any request-scope dereference.
- The native A* input reads the two original budget dwords at offsets `9C/A0`.

The longer disassembly and original failing interleaving are retained under
`D:/DEV/Kenshi/research/keo-routing-on-20261102`. The old audit's source-pinned
reproduction intentionally describes the old source; the new production-body
fixture is the green regression.

## Verification

All commands were run from the canonical KEO checkout using bundled Python and
the VC10/SDK7.1 toolchain.

- Initial policy RED: `build/path-budget-red.log` fails on the missing helper.
  The same registered suite is green after implementation.
- `leopard/test_path_budget.py`: 3,149 checks from actual production
  submit/fallback/capture/resolve/budget bodies; ten compiled functional
  mutants rejected. Includes old-repath/new-order interleaving, exact argument
  forwarding/original-once, nested gates and NPC fallback, single consumption,
  no current context, cancellation, same-address reuse, C++/SEH unwind,
  inaccessible owned fixture memory, and four concurrent host threads.
- Nineteen existing routing suites plus the new policy suite: 20 PASS.
- `leopard/test_routing_switch.py`: 436 checks each in DEV and PROD; five
  compiled routing-master mutants rejected. Existing OFF behavior preserved.
- Native DEV and PROD: all 340 sources covered, successful builds. Each exports
  `startPlugin` and `MCM_Describe`; 143 KenshiLib imports match 0.5.0. All 13
  concurrent MCM source hashes match `build/mcm-task2/frozen-source.json`.
- `git diff --check`: PASS (line-ending advisory messages only).

Logs are in `build/leopard-budget-tests`, with native build output in
`build/path-budget-native-pair.log`. Exact source and candidate identities are
recorded in `path-budget-verification-20261102.json`.

## Performance and lifetime review

Each existing fallback adds one guarded scalar header read, a POD stack frame,
and constant-time TLS publication/restoration. Each A* caller adds one existing
caller-classification check; only the owned character path consumes a context.
There are no new allocations, locks, world traversals, native calls, retained
request pointers, queues, hooks, or per-frame work. The maximum A* allocations
per eligible search remain the original KEO values; extending correct
attribution to repaths can increase aggregate memory/work relative to a broken
missed boost. Actual queue pressure, CPU/FPS and memory behavior need a separate
game measurement. Static review and host tests do not prove absence of every
leak or fix every reported detour, circle or stall.

If only the detailed-search hook is active, there is no owned context and the
budget remains native. If only the fallback hook is active, its context still
retires at return/unwind without being consumed. No stale context survives
cancel, exception or nested fallback return. Independent review is pending.
