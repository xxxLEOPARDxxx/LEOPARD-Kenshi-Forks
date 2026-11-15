# Empty squad-request allocation fast path - 2026-11-15

Working release2 code follow-up, not a publication or measured FPS result. User confirms mature-save FPS loss survives restart and existed in vanilla, and may involve base jobs/loaded zones as well as squad growth. No global cause is established. No new gameplay run, process capture, profiler, collector or diagnostic DLL was used. Source baseline03d28129c014c12548531674caac3e29c8b6cbee.

## Concrete native correction

Pinned native Blackboard::update allocates a320-byte ten-handle scratch array, initializes it and frees it even when the request map is empty. In that empty route it writes no Blackboard/game state. The full existing native bodies, writer paths and completion barriers are documented in base-squad-update-review-20261115.md. Ordinary unloaded updates already process at most one entry per faction call; there is no justified blanket squad/zone/job throttling.

Two optional Steam-only native wrappers remove only that empty allocation. FactionManager::updateMT enters a TLS scope only for the known native main-loop return78848A, main owner, both confirmed hooks and live toggleON. Blackboard::update skips only inside the scope, native return7EB1BD, live optionON and readable request count0 at the existing composed/width-asserted field. Null/unreadable/nonempty/worker/unknown/nested/unconfirmed calls forward exact arguments and native exceptions. Separate TLS ownerEntered prevents a third nested call from re-admitting after a middle call masked phase; RAII restores both prior fields on C++ unwind.

Admission verifies pinnedSDK identity through the existing cached check, native PE stamp/image size and seven exact loaded-code regions: both32-byte entries, both caller thunks, Blackboard call site, world-thread join and101-byte main join/character/faction/restart sequence. All contracts pass before installing either hook. Both native AddHook calls must succeed before atomic ready publication. First/second failure remains forward-only and retains originals, no rollback/retry. Existing coupled and independent hook installers otherwise remain unchanged. This optional pair adds two immediate startup hook operations; no further main-menu acceleration is claimed by this patch. GOG and incompatible/mid-patched phase contracts retain ordinary native behavior.

Live defaultON MCM/standalone setting blackboardEmpty is under Performance, with no restart marker and EN/RU/ZH wording. It does not skip jobs, AI or timer-bearing request work, change RNG, reduce update frequency, defer NPC creation, alter physics, register queues or spawn workers. Detailed successful/refusal summaries follow existing logging rules; errors remain visible. TwoTLS booleans per thread, fixed198-byte proof payload, no new per-frame/world scan, dynamic cache, engine reference, allocation, timer, diagnostic counter or retention of stack/native object pointers. Only confirmed phase calls read the request-size field. Existing settings state bounds and row-count/capacity checks pass.

## Verification

- Actual-source wrapper fixture failed first with original forward-only behavior on empty known-phase call, both /Od and /O2. Independent review then found deep nesting; added three-level known/unknown nesting fixture reproduced runtimeRED before separate ownerEntered fix.
- Final actual production fixture:128controls each VC10 /Od and /O2 PASS. Covers exactfloat bits/NaN and self forwarding, native board/faction C++ exceptions, thread-local isolation with a real fixture worker, inaccessible pages, requests arriving after empty update, midphase toggleOFF, all seven admission-region refusals, SDK/PE/GOG gates, both partial hook failures, original retention and no retry.
- Twelve separately compiled production mutations runtimeRED: lost phase/owner/callers/readiness, old nested-phase guard, lost scope cleanup, ignored nonempty/live setting, retry, ignored proof and premature ready. A survived first toggle mutant exposed a test gap; a real inside-phase toggle-off case was added and now rejects it. Compile failures are not accepted as RED.
-39 offline pinned native checks: complete EXE/SDK SHA, both full function hashes, export/RVA-table indices, all loaded proof arrays, allocation/empty branch and completion/join/call/thunk ordering. Files only, no native engine calls.
- Combined fullDEV andPROD builds365TUs pass. All127host suites pass with explicit new schema/golden/live/UI row expectations. ActualMCM off/on callbacks update the live field and save exactly once each. Three compiled catalogs131keys each roundtrip and Russian forbidden-dash check pass.
- Independent reviewer reran128controls/12mutants/39native checks and both actual MCM suites, and reviewed final6productionfiles/3languages. No remaining blocker. Build/import/install hashes are in private blackboard-empty receipts in stage.

## Empty .platoon question and limits

Actual LEO3 contains a referenced100-byte Банда костей_10.platoon with charcount0 but dead=false,persistent=true in quick.save. Empty file size therefore is not a safe deletion rule. Missing files take a failed native load path; equivalent fallback was not proved. Existing files were read only, never changed. See empty-platoon-save-review-20261115.md for typed fields, native save/load references and unchanged hashes. Saved file count is not active squad count or evidence of per-frame disk polling.

This patch removes proven redundant native allocations in supported empty updates. It does NOT establish the cause or full cure of mature-world FPS decline and does not eliminate heavier nonempty NPC appearance/skeleton/texture/attachment callbacks. Native resource completion time slicing remains outside proven readiness/lifetime contracts. The LRU pressure-scan candidate is deferred and is not misrepresented as the cause of vanilla mature-base degradation. No gameplay speed, FPS, leak-free or loading-latency claim.

## Focused production paths

- src/fixes/fixes_config.cpp
- src/fixes/fixes_config.h
- src/fixes/world/blackboard_empty.cpp
- src/fixes/world/blackboard_empty.h
- src/gui/settings_layout.cpp
- src/plugin/hook_manifest.cpp
