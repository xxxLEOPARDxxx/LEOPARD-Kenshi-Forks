# Private: menu-only fast warmup and callback coverage, 2026-11-15

Owner explicitly clarified faster warmup after each GAME restart, without PC
reboot. Kept the current residual-stutter task; future long-campaign FPS work
and satellite/DF are not started. Map reference for later: github.com/pizzy00/
timelight-map, supplied by owner; not reviewed as part of this change.

## Actual Corsairs reference and confirmed KEO pacing defect

D:/Games/Corsairs Ship Pack/CSP_Thander/engine/ThunderCSP/src/apps/engine/src/
main.cpp42-136 reads all selected assets into one reused4MiB buffer using
ifstream, after3s delay, no per-read rate sleep. It explicitly logs OS cache
warming. Restarted processes can benefit from surviving Windows file-cache
pages; no process-owned file cache survives exit, no guaranteed physical
residency. s_device.cpp2150-2171 HUD counts read traffic, not retained bytes.
Source was inspected, source-to-installed-Corsairs-binary identity not verified.

KEO always paced256KiB reads, even hot startup texture reads. At configured
128MiB/s this imposes about2ms perchunk plus sleep scheduling; that prevents
cached repeated startup from using available bandwidth. This change removes
intentional pacing ONLY for startup reads under a fresh title-menu lease.

## Change and guards

Root owner heartbeat refreshes a numeric250ms lease before250ms metadata
cadence, only with enabled RAM/startup and visible verified title widget.
QPC checks20ms frame gaps and1s recovery; hidden/disabled resets history and
avoids QPC. Worker consumes only atomics/tick lease, never engine/UI pointers.
Expired/stopped rendering, background pause, startup OFF or cache OFF refuses
fast mode. Requested gameplay reads and startup after menu retain configured
1..128MiB/s pacing. Short uncancellable ReadFile remains one256KiB chunk.
Worker stays low priority, cancellation/requestpreemption/memory/reserve/cap/
reparse safeguards unchanged. It yields after cumulative4MiB, across small
files. Debug counters startupMenuFast (eligible mode) and startupFastReadBytes
prove whether bytes actually used it. No promise of10x or zero IO contention.

No new setting/INI change. Existing startup opt-in remains off by release
default. EN/RU/ZH read-speed tooltip now explicitly distinguishes gameplay
limit from responsive-menu startup;126keys each, MO-only runtime catalogs.

## Residual-stutter evidence and coverage fix

runtime-completion-attribution-first/analysis.json snapshot: active fa3da20d,
696 retained models,2892referencehits, no pressure/evictions. Startup209.878s,
7379processed/7295loaded/84skipped/capped0. Several small slow passes dominated
by2Dtexture10-12ms; others50-70ms had only small measured children. Body-specific
guard refused atstartup. Unknown cause: disk prefix matches, a runtime prior
patch is only an inference, not proven or attributed to a mod.

Measure internal mesh completion dispatcher458860 instead. Pinned Steam prefix:
48 89 54 24 10 53 48 83 ec 50 48 c7 44 24 20 fe ff ff ff 48 8b da 48 8b d1 48 83 39 00 75 33 48.
Thunk04DDBA jumps there; two direct native callers4495FB and44C1BD cover both
finish routes. RCXcallback,RDXindirectowned16ResourcePtr,R8entity. Native clones
and dispatches then destroys incoming temporary; EH cleanup12E94D0 calls
SharedPtr dtor0184BC->0DC520. Raw wrapper retains exact ownership, no C++ copy/
destructor or caught exception. Strict PE/32bytes guards. Preferred common
callback makes body-specific hook unnecessary; body is guarded fallback only.
Stats/ring add inclusive meshCallbackUs. Nested times overlap, not additive.
No native scheduling, batch20, readiness, world/NPC spawn or thread change.
This coverage fix is diagnostic, not a claim to have removed residual stutters.

## Verification

* Actual production store24checks/Od and/O2: normal4MiB at1MiB/s requested
  exactly4000ms pacing vs menu0, no real sleep in spy; expiry/wrap/pause/OFF,
  observed requests always paced. Independent reviewer reran.
*3compiled production mutants fail: observedunpaced,leaseforever,startupstillpaced.
* Main wrapper1242checks includes600steady60Hz frames with15.625ms coarse
  lease clock, stall/recovery/hide/stop/cadence. Reviewer caught and fixed
  original GetTickCount responsiveness false suspensions BEFORE installation;
  QPC rereview has no Critical/Important remaining.
* Full actual filesystem/junction/GUID-reparse/oversize/cap/noeviction/pressure/
  observed priority/late roots/cancel/stop suite passed. Sandbox refused junction
  creation, rerun in authorized elevated context passed (not a product failure).
* Probe69controls/Od and/O2 include dispatchernormal/unwind,C++/SEH/exactargs/
  disabled/offmain, owned16 VC10 surrogate and nested ring child deltas.
  Native PE/byte/RVA/thunk/onearrayhook10checks;4earlier negative controls passed.
* Full363source DEV/PROD/O2GL/LTCG and actual imports checked;183Klib0.5.0,
  31Ogre,65MyGUI,CRT100only. Three MO126keys, complete RU ASCII-dash scan passed.

Runtime validation still required: allow menu startup tofinish, enter saved
game and one route with a residual stutter; then restart GAME and compare
startup progress while Windows cache may remain warm. Do not reboot PC or
flush Windows cache for this request. Preserve accepted662bda62 and lastfa3da20d
backups/settings. No release/number/Nexus/Steam/personal changelog entry.
