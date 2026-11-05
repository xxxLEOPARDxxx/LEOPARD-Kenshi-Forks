# KEO Task3: diagnostic logging and three-locale compatibility

Status: source and native candidates frozen; awaiting independent review.
No game/process/UI/install, installed/personal INI write, commit, push or publication.
Base HEAD remains 49b1550dc5b270f6c26737659d792b046467976f, branch leopard.
Original origin jacobericson/KEO was not changed or pushed.

## Scope and source identity

Exact Task3 file list: `task3TouchedFiles` in logging-task3-20261103.json.
The manifest pins the complete 694-file compile/support surface and every staged
DEV/PROD candidate file. It distinguishes the 14 inherited dirty files left
unchanged by this continuation and records overlaps with prior MCM/budget pins.
The MCM bridge differs from its d10ae808 frozen source only by the one
`KeoSetTranslationReady(ready);` call (removing exactly those bytes reproduces
that prior hash). Requested-settings/MCM callbacks and native budget policy are
preserved; pathfind_hooks changes here are diagnostic capture/admission only.

The broad diagnostic scope is intentional: producer admission is explicit at
call sites, rather than muting LogMsg globally. Current channel inventory is in
logging-producer-inventory.json. Error channels are independent of Debug.
Independent review has already prompted corrections to signature/ABI/IO errors,
loading-guard faults, native/off-main refusals, unload anomalies, corruption
alarms, delayed arming failures and explicit benchmark refusals. Generic successful
installation/configuration chatter remains detailed-only. Error/fatal/worker
retirement sinks remain unconditional and obey their existing thread boundaries.

## Behavior

- Debug=false in DEV and PROD; final live MCM row, no restart marker. It is now
  documented in the default INI (97 documented keys/43 advanced undocumented).
- KEO_DETAIL/DEFERRED gates argument evaluation; pure emitter entry gates avoid
  formatting/native diagnostic reads. Detailed-disabled errors still queue/flush.
- ZoneLeak full lifecycle/retention/release/age walk is retained; only optional
  memory read and text generation are gated afterward. Render maintenance and
  periodic counter drains remain active; token formatting is skipped while OFF.
- DEV movement trace stops sampling/copying while OFF, clears owned storage once,
  and resumes empty without reinstalling hooks or rereadingINI on re-enable.
- L2 no-bank error alarm reads three counters at the existing 10/30 s cadence
  before the detailed-report gate. It is one-shot, respects bypass, excludes
  zero-face generations and remains independent of the expensive report.
- KeoTr returns English before native admission and on non-UI threads. The common
  loader loads once; pointers stay stable. A rising Debug edge replays one cached
  256-byte summary without reloading or invalidating strings. Format/load errors
  use ErrorLog independently. MO loading validates all rows before mutation,
  accepts both byte orders, bounds file/decoded text to 16 MiB and rows to 65536.
- All 65 emitted player metadata/Error/Text keys verified against all three source
  catalogs; 68 total catalog keys each. en_GB/ru_RU/zh_CN only. Build stagers
  deterministically compile MO and copy README/default INI; no sourcePO in output.
- README adds EN/RU unpublished maintenance documentation, original author thanks
  and source, temporary-maintenance/withdraw-if-author-returns policy, diagnostic
  behavior and explicit limits on inherited benchmark claims. No release number
  was raised and no personal published changelog entry was created.

## Executed verification

`python tools/tests/run_suites.py` with VC10 x64 / SDK 7.1 INCLUDE/LIB/PATH and
KENSHI_ROOT=D:/SteamLibrary/steamapps/common/Kenshi: 126 suites PASS.
`python tools/tests/check_test_guards.py` in the same toolchain:PASS.
Logs:build/logging-task3-final-{host,guards}.log.

- `python leopard/test_logging_task3.py`:15 actual logger/probe checksPASS.
- `python leopard/test_translation_seam.py`:6 actual native admission checksPASS;
  3 compiled translation mutants rejected.
- `python leopard/test_task3_catalog.py`:27 actual shared loader/admission/summary
  checksPASS, including 11 malformed MO cases and valid big-endian input.
- `python leopard/test_task3_maintenance.py`:17 actual producer/lifecycle/render/
  trace checksPASS;3 compiled mutants rejected (wrong zone/render early returns,
  repeated trace cleanup while OFF). 4095 empty-zone records released and one live
  retained record refreshed while OFF; all 7 render upkeep paths called.
- `python leopard/test_task3_error_alarm.py`:6 actual L2banking checksPASS and
  exact owner error-before-report/cadence wiringPASS.
- `python leopard/test_task3_player_keys.py`:65 actual MCM player metadata + all
  Error/Text literal keys x 3 locales PASS (numeric choices are invariant digits).
- `python leopard/test_path_budget.py`:3149 actual caller/policy checksPASS;
  10 compiled mutants rejected. Original diagnostics-only token distinction stays.
- `python leopard/test_mcm_adapter.py`:PROD/DEV callbacks PASS;6 behavior mutants
  rejected and 2 exact prior finite-clamp AV regressions reproduced as expected
  negative controls. Actual bridge stays unchanged except admission forwarding.
- `python leopard/test_mcm_options.py`:9 actual lifecycle checksPASS,2 mutants
  rejected; platform/partial/runtime admission wiringPASS.
- `python leopard/test_mcm_writer.py`:24 actual atomic-file checksPASS, including
  all 3 read/temp-write/replace errors while Debug OFF and success-log exception.
- `python leopard/verify_logging_task3.py`:frozen 694 source/support files and both
  candidate compositionsPASS. Native exports startPlugin/MCM_Describe checked.
- `git diff --check`:PASS (preexisting GitLF/CRLF advisory only).

Native command:STEP4_SUFFIX=_loggingtask3,STEP4_VARIANTS="dev prod",
BUILD_CACHE=off,cmd /c build_leopard.bat. FinalDEV/PROD 340 sources eachPASS,
VC10 LTCG; 144 KenshiLib imports verified. Log:build/logging-task3-final-native.log
and build/obj_step4_loggingtask3_{dev,prod}/build.log. Candidate identities are
in the JSON manifest (no claim that the installed game uses these binaries).

Additional review followup: `LogStatus` is the shared mixed-status callback for
native config parsing, benchmark group resolution and sweep progress/refusals.
Unknown/invalid/refused/failed/clamped inputs go to LogError even while Debug is
OFF; ordinary retired/DEV-only notices and progress still use LogDetail.
`python leopard/test_task3_status_callbacks.py` links actual game-free config,
group and sweep code plus the actual status callback: 23 checks PASS, including
all three explicit sweep refusal paths, malformed sweep/group input, invalid
lever values, leg/stage/group capacity limits and successful progress staying
silent. Legitimate text values named unknown, stage precedence and DEV-only
notices remain detailed.
Native owner wiring is also verified. No benchmark module API was changed.

## Performance/lifetime review and limits

The flag read is one cached Interlocked operation, with no per Tr/per-frame INI
poll. No new worker threads, world-pointer caches, widget owners or routing
policy were added. Native player/path budgets, priorities and original
multithreading remain unchanged. Pure diagnostic scans are admitted before
work; unavoidable gameplay maintenance remains above the gate. Error detectors
retain their operational cadence. Original bounded counters/queues continue
where needed; detailOFF is not a claim of zero instrumentation overhead.

Localization storage is session-owned and never rebuilt by Debug. Error queue
capacity remains 32 x 384 characters with original overflow accounting; error
admission is independent of Debug, not a guarantee that an unlimited error flood
can be stored. No new unbounded lifetime owner was introduced. Optional trace
storage is released once on disable; reset/save-load paths remain active.

No game was launched or observed. Therefore no claim about FPS/CPU timing,
long-session memory growth, duplicate/displaced NPC reproduction, complete
routing correction, vanilla/modded skin layout or actual late-enable UI behavior
is made. Full Python baseline/tool-cache suite was not reclassified as green;
only the named commands above were run. Independent broad-diff review remains
required before installation/commit. KEO remains Steam 1.0.65-only; no GOG port.
