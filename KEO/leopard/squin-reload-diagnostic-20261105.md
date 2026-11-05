# Squin second-load diagnostic, 05 Nov 2026

User's controlled reproduction supersedes the earlier saved-state comparison:
main-menu import of LEO4, first Squin approach inside, unload via Hub, return
outside with KEO; independent same import/cycle stays inside without KEO.
Running KEO is implicated in that unload/reload scenario. Placement mechanism
is not yet established; this candidate deliberately does not change loading,
readiness, town activation or NPC positioning behavior.

## Trace

With the existing detailed log enabled, the production build emits:

* `ReadinessBypass`: only actual native-false / KEO-true outcomes, with cell,
  raw EXE return RVA, thread ID, main-thread state, existing readiness/ledger
  classification, section count and just-loaded flag. QPC first/last and repeat
  count identify event time independently of delayed print time.
* `ReadinessBypassQueue`: dropped, coalesced and Debug-off-discarded rows.
  Fixed 128 POD rows, one non-waiting try-CAS; Main drains at most 16 per .5s
  and formats only after releasing the gate. No engine pointers are retained.
* `ZoneTownRefuse`: an existing TownGuard refusal with cell, epoch, arguments
  and main phase. The original remains skipped exactly as before.
* `FirstTime`: private processContent finalization flags, town/file predictor
  inputs and epoch. Repeated cell visits remain observable, replacing the
  former DEV-only once-ever latch. Actual handoff events also carry epoch.
* Existing `ZoneLife unload ... save=` and retention/transition summaries remain.

Lifecycle events use two fixed 32-key budgets, each at most 64 per minute and
one identical kind/cell every 5s. Debug OFF gates extra lifecycle queries and
readiness QPC/TID/state reads; the readiness reporter retains a small constant
try-CAS/discard step. Logs are diagnostic samples with explicit queue losses,
not an exhaustive history of every callback.

Caller class 0 is Main with zone-manager state4, not proof of a particular
native caller. Raw 3AC822 on Steam is the shared wrapper return; it cannot
distinguish its upstream caller. Readiness classes: 0=no section, 1=outdoor
missing, 2=outdoor present/buildings pending, 3=not in world, 4=unknown,
-1=off-thread classification not sampled. Readiness rows do not contain world
epoch; QPC timestamps must be used across imports and delayed drains.

## Verification and scope

Actual extracted readiness hook parity: 1536 combinations in each /Od and /O2
against the pre-change body, including identical native/classify invocation
counts and return values. Queue concurrency covers four producers/8000 write
attempts, bounded drain/full/coalescing/Debug lifecycle. Removing the recording
call returns RED. Lifecycle actual handlers: 111 controls each /Od and /O2,
with old event behavior returning RED. Independent reviews of both scopes
found no blocker. This does not establish the root cause or constitute an FPS
benchmark. No new native hook/address/ABI was added.

Native audit in `_codex_work/20261105-squin-second-load` verifies the readiness
to B3 publication and unloaded-platoon admission chain on the actual Steam
image. The native fallback checks generator work as well as section count;
the current unknown-cell KEO bypass does not. Town coverage is another
candidate: listed town cells are protected, not every physical building cell.
Neither candidate is treated as the proven cause before the controlled trace.

Use one fresh game session: import LEO4 from main menu; first Squin; Hub until
Squin unloads; return Squin; pause immediately at the outside crowd and leave
the process open for read-only inspection. Do not run an OFF control before
the ON log is copied. Active LoK files are deliberately unchanged for this run;
its independently prepared update is held to avoid a second changed variable.
No public release/version/ALL_CHANGELOG change. DF and satellite remain paused.
