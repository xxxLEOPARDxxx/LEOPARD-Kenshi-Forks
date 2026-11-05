# KEO pre-publication priority fix — source freeze, 2026-11-03

Base: `6e15b3a6926040de19f0418df10e5f1c47c0dc2a` on `leopard`, uncommitted local patch.
No game calls, memory inspection, UI, installation, INI writes, commits or pushes
were performed by this task. The original jacobericson/KEO origin is untouched.

## Result and exact boundary

The old submit wrapper wrote tier45 only after native submit returned. The native
producer publishes into NavMesh.characterMessages before then, and the consumer's
mutex is different. The consumer can read10/20 while the producer still holds its
submit mutex. This is distinct from the request-local 4x budget repair already in
6e15b3a: its current-native-player predicate and original budget values are kept.

The new bridge runs before the original enqueue call, before every optional
pool-diagnostic branch. It accepts only the current synchronous submit's request,
its manager-derived SDK input queue, native enqueue return3AAF60, kind0 and the
expected native10/20 supplied by that submit's high-priority argument. It consumes
its POD TLS context once and writes45 before native publication. Cancellation at
3AAFD5, result queues, foreign requests/returns, NPC tiers and reused contexts
refuse without borrowing an outer tier. No request read/write occurs after the
original enqueue/submit transfers ownership; finally only restores context.

The submit wrapper and requestPath wrapper save and restore exact previous TLS
through POD __try/__finally, including nested NPC calls, C++ unwind and SEH. The
requestPath wrapper also restores the real planner water requester; the existing
water write still runs before native submit. No native call is repeated. Existing
formation notification, group/water decisions, OFF behavior, planner clocks,
original worker concurrency and request-local search-budget predicate are kept.

A fixed28-window/edge Steam504b contract validates the actual caller, native
initialization, input publication, cancellation, consumer priority reads and
relevant rel32 thunks BEFORE hooks modify entries. Capability is finalized only
AFTER all install steps: routing wanted, profile, requestPath, submit, enqueue and
all three originals must be present. Partial admission leaves native priorities;
there is no post-submit fallback. Native profile/refusal errors remain visible.
The SDK-derived input queue uses NavMesh_characterMessages with its compile-time
offset check; no private address or inferred diagnostic manager owns the write.
A foreign chained hook can conservatively lose admission through exact return
matching; no speculative adjustment is attempted.

## Read-only native proof / TDD

The pre-edit research snapshot remains frozen at
`D:/DEV/Kenshi/research/keo-priority-publication-20261103/report.md`, SHA
`a37bc35b51ff947c16a059decb5f9a0ad45f19a9e2fbe2fe0c01690383f1de6a`.
That report deliberately describes the old defect as still present at its stage.
Its evidence.json is `2adcb76d5a65dadfcb6f9973ada4e8b40679e0d51fc0cbf2f1b7172fd6681a62`.
It pins675 original production inputs at clean6e15 before this patch. Offline EXE
SHA504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e;
no executable code from the game is called. SDK ABI and13 original native critical
windows/13mutants pass in that independently reproduced research stage.

The actual old6e15 submit body and owned queue fixture are copied unchanged into
leopard/fixtures/priority_publication_6e15. The final actual-body test retains an
expected RED: old code observes20/10; current code observes45 before publication.
A separate consumer thread executes while the producer mutex flag remains held.
The consumer can protect/retire its owned request page before submit returns:
old code faults C0000005, current code returns safely. This is a host lifetime
stress, not a claim that real Kenshi necessarily frees the object at that point.
The final fixture extracts the actual production wrappers/bridge and links the
actual policy, player-tier decision and real planner water table. It checks all
28profile windows, missing reader/originals, partial hook masks, OFF/NPC, exact
queue/request/return,10/20/kind, one-shot, nested/TLS/thread isolation and both
exception paths. Independent review must validate this frozen source rather than
accepting only the earlier prototype.

## Separate optional module census hunk

Only SnapshotModuleBases' bounded curated-truncation notice changes LogError to
LogDetail, with DetailedLogEnabled before its formatting. Census collection,
curated cap48, raw cap256, self/RE priority pinning and crash attribution remain
unchanged. The actual function is extracted into a fake152-entry enumeration:
OFF collects152raw/48curated with zero formatting/logs; ON does the same and logs
one detail, zero errors. Attribution through a raw entry outside the curated list
still works, and each handle closes. Compiled severity/producer mutants fail.
No actual process enumeration or game process API is used in this fixture.

## Verification commands and results

From `D:/DEV/Kenshi/forks/KEO` with VS2010x64 compiler/SDK environment:

- `python leopard/test_path_priority_publication.py`:195 actual checks PASS,
  frozen old expected RED confirmed,9 compiled negative controls rejected.
- `python leopard/test_path_budget.py`:3151 actual budget/caller checks PASS,
 10 compiled negative controls rejected. Submit setup now uses the real
 pre-publication policy; no obsolete post-submit write assumption remains.
- `python leopard/test_module_census_notice.py`: actual fake census PASS,
 2 compiled negative controls rejected.
- `python tools/tests/run_suites.py`:127 suites all PASS, including the new pure
 policy suite and existing planner/water/budget/lifecycle suites.
- `python tools/tests/check_test_guards.py`: all checks PASS. An initial invocation
 without the VS PATH failed compiler-dependent guard self-tests; rerunning with
 the same compiler environment as the127host run passed. No false green claimed.
- `STEP4_SUFFIX=_prioritypub STEP4_VARIANTS="dev prod" BUILD_CACHE=off` then
 `cmd /c build_leopard.bat`: clean native DEV341/341 compiled26.6s and
 PROD341/341 compiled32.4s, both build OK. No cache objects restored.
- `git diff --check`: PASS.

Scripts set their own VS compiler paths except the standard full host/guard
runner. For those, INCLUDE is VC/include;WindowsSDK/Include, LIB is VC/lib/amd64;
WindowsSDK/Lib/x64, PATH begins VC/bin/amd64;WindowsSDK/Bin. VC root is
C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC; WindowsSDK root is
C:/Program Files/Microsoft SDKs/Windows/v7.1. KENSHI_ROOT is the game folder for
existing offline SDK/fixtures, never process access. Full compiler output and
per-case outputs are under build/path-priority-tests, build/leopard-budget-tests,
build/module-census-tests and build/tests; compact logs are hashed in manifest.

## Performance / lifetime / limits

Steady path adds bounded POD stack/TLS state, scalar metadata checks and at most
two4-byte reads/one4-byte priority write before ownership transfer. No heap, cache,
world scan, queued request retention, timer, INI polling or worker serialization
is introduced. The fixed28-window check runs once before install, only if wanted.
OFF exits before native profile reads and request payload access. Existing pool
instrumentation is unchanged. The water getter reads only current-thread cached
ownership; its existing arm/live contract is once at startup. Exact prior-state
restoration replaces destructive clears and is tested for both exception modes.

This corrects the confirmed priority-publication interleaving only. Static/host
proof does not establish live route quality, route acceptance, FPS or absence of
all leaks. No game ON movement QA was performed here. No release/version bump,
new language, packaging, current installed candidate alteration or publication.
The existing three MO catalogs and default INI are byte-identical to base.

## Frozen source and native candidate

Manifest SHA256: `1ad61306dc04114b807068d290198f5416e03ec839c3fff3071e2b335d591144`

`leopard/priority-publication-20261103.json` pins 704 source/test/doc inputs plus exact DEV/PROD compositions and evidence.

- `dev`: `build/KEO_step4_prioritypub_dev/KEO.dll`, SHA256 `4369038da9944e1b042b72967c69a39d305e52907286d338446ab6308c936f7e`.
- `prod`: `build/KEO_step4_prioritypub_prod/KEO.dll`, SHA256 `d396c05a64dfa45eb8ea515782cb7c0327d00e7ceb86108881a4e2e21f767ca2`.

Exact changed files relative to base (all other frozen production sources preserved):

- `README.md`
- `leopard/fixtures/priority_publication_6e15/current-submit.extract.cpp`
- `leopard/fixtures/priority_publication_6e15/publication_fixture.cpp`
- `leopard/test_module_census_notice.py`
- `leopard/test_path_budget.py`
- `leopard/test_path_priority_publication.py`
- `src/pathfind/path_pool_bindings.h`
- `src/pathfind/path_pool_hooks.cpp`
- `src/pathfind/path_priority_hooks.h`
- `src/pathfind/path_priority_publication.cpp`
- `src/pathfind/path_priority_publication.h`
- `src/pathfind/pathfind_cache.h`
- `src/pathfind/pathfind_hooks.cpp`
- `src/planner/planner_water_table.cpp`
- `src/planner/planner_water_table.h`
- `src/plugin/crash_record.cpp`
- `src/plugin/hook_manifest.cpp`
- `src/plugin/hook_manifest_rows.inc`
- `tools/build/coresrc.txt`
- `tools/tests/path_priority_units.cpp`
- `tools/tests/suites.txt`
