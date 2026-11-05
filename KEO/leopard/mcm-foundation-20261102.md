# KEO MCM foundation: local candidate, 2026-11-02

Source baseline: leopard13953b03cb2f8df1638a9194d58467372f922262. This patch is uncommitted and uninstalled. It implements the planned Task1 host seam/golden repair plus the parent's explicitly requested minimal requested-preference capture slice from Task2. It does not implement the MCM page, callbacks, save/apply adapter, Debug switch, third catalog or Steam/GOG compatibility work.

## Changes and behavior

`base/keo_tr.h` is now a game-free declaration; `base/keo_tr.cpp` retains the exact existing native Localization.h/Tr behavior and null/empty passthrough. Only eight host suites link an explicit English identity stub. Their fixtures no longer drag LocaleInfo into game-free layout/config tests. The extracted actual native wrapper has separate tests; identity translation is not presented as catalog verification. Existing catalogs/loader and their current limitations are unchanged.

The actual table has20 player rows,107 DEV core rows,139 active keys,23 retired keys,115 module keys,43 NDOC keys,137 golden scalar values and108 writer entries. Existing golden assertions now explicitly include keoRouting=true and its module/section. Layout tests now check all four existing routing dependents alongside throwout/backpack groups. The restart-tooltip assertion checks the exact restart suffix followed by the already-existing dependency sentence for grouped rows. No assertions were removed and no UI layout/label/feature behavior changed.

`gui/requested_settings.{h,cpp}` owns one fixed SettingsStaging plus bounded module/key counts. It preflights table count, state size, row termination, kind and field bounds before StageModule reads. It publishes availability only after capture. Read/metadata calls before capture or after invalid capture refuse; copies are independent. Metadata refers only to the static ConfigModule tables, never native/world/UI objects. There is no INI polling, translation, file write, live apply or hook/worker operation in this helper.

FinalizeConfig captures requested preferences before ApplyRoutingSwitch and later hook refusals can clear effective fields. Both existing LoadConfig exits use FinalizeConfig, including missing INI. A capture-bound failure logs an error and keeps editor data unavailable while preserving original runtime finalization. No front end consumes this snapshot yet; Task2 must wire it into getter/persistence and publish post-save edits without serializing effective refusal flags. The existing Options tab and flat atomic INI writer are untouched.

## RED -> GREEN and verification

- Original settings_floor compiled RED at Localization.h: missing kenshi/LocaleInfo.h. After the native/header split, all8 affected suites compile. Separate stale golden/group/restart suffix failures were observed, then corrected against the actual existing table/layout.
- New requested fixture compiled RED with missing requested_settings.h, then GREEN using actual ConfigModule/StageModule and routing master override. It covers pre-capture refusal, requested masterOFF/childON preservation, independent copies, metadata indices, overflow/state bounds/no terminator and global startup capture.
- Actual extracted FinalizeConfig runs in both DEV/PROD with actual requested helper and routing policy. Five compiled mutants are rejected: capture too late, live-read copy, state-size overflow, lost readiness and off-by-one metadata bound. The last mutant reaches an invalid metadata read and fails the child process; compilation failure is never counted as a passing mutant.
- Actual native KeoTr boundary4 checks and3 compiled mutants PASS. Tr itself is a counted catalog boundary double, not a native LocaleManager test.
- Fresh all123 host C++ suites PASS with KENSHI_ROOT pointing to the installed game's static DLL files. The previous render_units failure was its nonexistent default G: path; no game DLL initialization/native functions are invoked by its DONT_RESOLVE reads.
- Test guards PASS. Selected Python checks retain one known baseline failure: test_objcache.StaleInputTest.test_a_shadowing_header_created_after_the_restore_refuses_the_link expects1 problem but observes0. This unrelated test/build behavior was not changed; the full gate is not claimed green.
- Routing regression remains436 actual checks each DEV/PROD plus5 compiled mutants PASS.
- Fresh native pair:338 source objects per variant;336 cached objects were revalidated/restored and2 compiled. Both link/import gates PASS with startPlugin and143 KenshiLib imports compatible with0.5.0. MCM_Describe is intentionally absent in this foundation stage.

Candidate paths/hashes:

- build/KEO_step4_mcmseam_prod/KEO.dll:1418240B, SHA25619c415510c4d949b91081dafca41bde5f34ea42b1648cdf5079361d41528b897.
- build/KEO_step4_mcmseam_dev/KEO.dll:1531392B, SHA25689c18cdfb09587af0589c80b51ac212886e2dad78a817c51026473db8b63ee13.

Logs: build/task1/baseline.log, seam-before-goldens.log, requested-red.log, final-full-cpp.log, final-native-pair.log, python-selected.log, test-guards.log, routing-regression.log; actual-body fixture logs under build/task1/{translation,requested}. Source and candidate identities: mcm-foundation-verification-20261102.json.

## Performance/lifetime limits

The added storage is one fixed staging object (under32KiB), captured during startup finalization only. No per-frame work, cache growth, scene/world scan, dynamic allocation, owned widgets or native references were added. Read is one bounded scalar-data copy; metadata lookup is constant bounded indexing. KeoTr retains the original native catalog lifetime and UI-thread contract while concentrating native includes in one translation unit. This source/offline review does not prove live UI compatibility, actual CPU/FPS or leak freedom; those remain future MCM integration acceptance work.

Installed/personal INI remains SHA25670e4d9fab5f3a4584be17d05c931d25c07abe3d12d0e534ba454b7056a545afc. Repository KEO.ini, gameplay/spawn/path hooks, GOG gates and source profiles remain unchanged from13953b0. No game/UI/process/settings/install/publish/commit/push actions were performed. Original jacobericson code/license/feature ownership remain intact; this is maintenance infrastructure for the planned optional MCM migration.
