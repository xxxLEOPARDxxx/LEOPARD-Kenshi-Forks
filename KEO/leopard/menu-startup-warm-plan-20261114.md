# Private: faster startup texture warmup in the title menu

Owner asked why Corsairs repeats warmup much faster after game restart without
PC reboot. Actual reference engine: D:/Games/Corsairs Ship Pack/CSP_Thander/
engine/ThunderCSP/src/apps/engine/src/main.cpp:42-136. It reads into one4MiB
buffer without rate sleeps, warming Windows file cache; process RAM does not
persist. Its HUD measures read traffic, not guaranteed resident OS pages.

Confirmed KEO bottleneck: ReadOne always sleeps per256KiB at read128 even when
Windows serves already cached files instantly. Remove intentional pacing only
for startup texture reads while a fresh main-thread title-menu lease is valid.
Observed gameplay requests ALWAYS keep configured rate; no user INI change.
The read-speed tooltip will explicitly describe the gameplay/background limit.

Architecture: owner render heartbeat refreshes numeric menu lease before its
250ms metadata cadence. Worker consumes bool+GetTickCount lease<=250ms, existing
backgroundAllowed,enabled/startup gates. Main frame gaps>=20ms disable fast mode
for1s; leaving menu revokes it. If frame callbacks stop, fast mode expires.
Keep256KiB cancellable chunks, request preemption, memory/cap/reparse checks,
low-priority IO worker and native consumer unchanged. Yield after4MiB in fast
mode, no engine calls or engine pointers in IO worker. Actual fast byte counter
in debug summary confirms usage; not a promised10x or permanent file residency.

Tests first: exact production lease policy including expiry/wrap/off/pause,
actual4MiB paced1MiB/s versus fast read, requested work still rate limited,
startup OFF/cancel/pressure and whole existing actualFS suite. Boundary fixture
proves menu refresh is independent of250ms seed cadence and rejected off-main/
shutdown; integrate3locale tooltip edits, native64probe/WARP/import/full builds.
Independent review before source commit/local replacement, preserve settings
and accepted build. No release/publication or general campaign-FPS work.

Remaining stutter attribution is independent: appearanceBody exact guard refused
in the new run, cause unknown. Preserve fail-closed behavior and instead measure
the verified generic mesh completion callback458860 (callers4495FB/44C1BD).
Same raw owned16 ABI already covered; keep body seam passive/unavailable, never
overwrite an unknown existing patch. Native order/batch/readiness unchanged.
