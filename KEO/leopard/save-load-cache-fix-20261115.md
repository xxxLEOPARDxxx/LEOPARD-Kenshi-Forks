# Save-load RAM cache demand/prefetch separation - 2026-11-15

Stage-only source fix. No game run, new capture/profile, DLL build/install,
personal INI change, native readiness change, commit or publication by this task.
Root owns review and integration. No save-load speed/FPS improvement is claimed
without a controlled measurement; the source policy error itself is reproducible.

## Confirmed policy error

`resource_cache.cpp:56` forwards a RAM miss to native archive opening and only
then observes the successfully opened path. That observation is a demand hint
from an existing native consumer, not speculative discovery. Previously Prefetch
was an alias of Observe and all requests shared an unclassified string queue.
`resource_forward.cpp:120` correctly signals loading/transition/frame pressure,
but the RAM worker applied that pause to the entire queue. An in-progress read
also slept inside the only worker while paused. Consequently observed native
opens could not cache-fill during the pause, and repeated opens in the same load
continued to fall back to native IO. Native loading never waited for this cache;
this fix does not change that contract or make initial decoding/scene work free.

## Implemented behavior

- `resource_cache_store.cpp:32`: bounded request records distinguish observed
  native opens from speculative metadata prefetch. Total queue remains128.
- `resource_cache_store.cpp:169`: observed demand ignores only the speculative
  pause. It still obeys configured1..128MiB/s pacing, RAM cap/physical reserve,
  immutable file identity, cancellation/epoch checks and low worker priority.
- `resource_cache_store.cpp:198`: paused/preempted speculative reads defer between
 256KiB chunks or <=10ms pacing slices, releasing partial allocations so demand
  can use the one IO worker. Startup keeps its cursor; ordinary prefetch is
  requeued if still useful and queue room remains. A demand for the same path
  supersedes that speculative retry. Deferred partial IO may reread its prefix
  after resumption; this is a bounded ownership tradeoff, not a promise of zero
  repeated speculative work.
- `resource_cache_store.cpp:309`: worker always selects demand first, even with
  the background gate false. A newly published startup entry prevents a queued
  same-path request from redundantly reading it again. Paused speculative queues
  do not signal themselves into a busy loop.
- `resource_cache_store.cpp:391`: observing an already queued speculative path
  upgrades it without duplicate ownership or growing the queue. A full queue can
  replace one speculative tail with demand; an all-demand full queue drops new
  hints and native IO remains authoritative.
- `resource_cache_store.h`: API comments now describe the speculative pause.
  Existing forward/loading gates and menu startup lease remain unchanged.

The pause itself never clears completed cache entries. Existing byte ceilings,
LRU admission, pressure retirement and ON/OFF semantics remain authoritative.
World generation is not a RAM-cache generation: the save-load reset clears only
geometry/texture pins and forward state. RAM epochs change for enable/cap changes;
startup epochs additionally change for registered roots/startup setting changes.

## Verification

Fail-first actual worker fixture: with background paused, a prefetch remained
queued; a subsequent successful native-open observation of that same path could
not produce a cached result. The old implementation failed at `replacement`.

After the fix:

- Real worker/file IO, edits/atomic identity replacement, existing-reader lifetime,
  byte budget, pressure, paused speculative deferral, demand upgrade and separate
  demand while a speculative file is paused: PASS `/Od` and `/O2`.
- Actual production ReadOne + deterministic Sleep/memory inputs: **46 checks each
  PASS `/Od` and `/O2`**. Tests include16-chunk demand continuing across mid-file
  pause while still paced, paused startup/prefetch, cancellation and memory pressure
  after the first chunk, unchanged completed bytes, queue upgrade/displacement and
  all-demand saturation. Configured1MiB/s4MiB read retains4000ms requested pacing;
  the existing responsive-title startup case retains zero artificial pacing.
- **12 separately compiled production mutants expected RED**: existing menu
  rate/lease controls; demand misclassified/left paused, prefetch misclassified,
  queue overflow, ignored cancellation/pressure, worker pausing everything,
  paused-queue self-wake and lost deferred prefetch. Worker wait counting is a
  local fixture assertion, not a new game capture or performance measurement.
- Existing full startup actual-filesystem suite PASS: junction/reparse exclusion,
  aliases, recursion, missing roots, size limits, no startup eviction, demand
  priority, cap/pressure retirement, late roots and shutdown. Its junction setup
  needed the same sandbox escalation as prior runs after Win32 error5; successful
  rerun touched only its own build fixtures.
- Existing forward-resource policy fixture PASS. No native load/readiness policy
  or resource-group ownership was modified.

Commands: `leopard/run_resource_cache_test.cmd [Od]`,
`leopard/test_resource_menu_startup.cmd`,
`python leopard/test_resource_menu_mutants.py`,
`leopard/run_resource_startup.cmd` and the existing forward fixture.

## Read-only findings and separate model inventory proposal

The previously saved runtime analyses show no large RAM-cache clear on save load:
in the narrow run cached bytes grow14,560,509,661 at42.082s to15,064,025,505
at53.294s, across reset51.943s. Five existing analyses show no >20% cache drop.
They do not identify how much save-load wall time this pause policy consumed.

Startup inventory remains intentionally texture-only at
`resource_cache_store.cpp:256`; runtime eligibility also supports mesh/skeleton.
Thus a completed texture warmup does not establish preloaded model/skeleton bytes.
No partial model-prewarm change was included. A separate consistent change can:

1. Enumerate the same safe registered roots into bounded mesh/skeleton and texture
   buckets, then read model/skeleton bytes first using the existing IO worker.
2. Preserve exact path/reparse/identity, epoch/cancellation, memory reserve and
   no-eviction startup budget rules. Never load Ogre meshes/skeletons on a worker.
3. Rename the existing texture-only option/help/HUD in RU/EN/ZH and documentation
   together; keep old INI key compatibility. No extra option is required.
4. Update the intentionally texture-only startup fixture and verify under a small
   cap that model priority is real and terminal budget behavior does not spin.

That proposal is a coverage/policy extension, not evidence that model IO alone
explains the owner's slow save load.
