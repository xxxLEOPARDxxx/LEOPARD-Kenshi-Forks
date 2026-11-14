# Resource warmup HUD integration (private, 2026-11-14)

Add `src/resources/resource_warm_hud.cpp` to the build list. Caller supplies a
zero-initialized `resource_warm_hud::Snapshot` by scalar copy, never native pointers.
`ramReadBytes` is a cumulative **actual read** count, not cache hits or target
capacity. `ramPending`/`gpuPending` include in-flight work. `gpuAdopted` and
`gpuSkipUploads` are cumulative native consumption/avoided-upload counters.

Call `ResourceWarmHudTick(enabled, snapshot)` only on the admitted UI thread,
after native GUI readiness, at the existing resource main tick. Disabled state
destroys the HUD immediately. First enabled frame creates one root and three
cached text rows; no GUI tree lookup. Painting, measurement and string formatting
are bounded to four times per second; captions and coordinates change only when
their values change. The actual read rate handles counter reset and DWORD wrap.

Call `ResourceWarmHudCleanup()` during the main-thread world reset/teardown before
the GUI vanishes (do not put native cleanup in the asynchronous
`ResourceForwardReset()` signal). The main-thread branch consuming `resetPending`
in `ResourceForwardTick()` is an appropriate reset point. Call
`ResourceWarmHudGuiGone()` when the cached GUI is cleared; it also safely destroys
still-live owned widgets if MyGUI remains available. Native unlink notifications
clear widget pointers on external deletion. An already destroyed manager is never
dereferenced. Caller must keep ticking disabled/cleanup while the feature is off;
do not put the only cleanup behind the cache enable gate.

All mutable HUD state is one fixed global object; exactly four owned widgets;
no entities, world scans, queues, histories or uncapped containers. Widget creation
failure destroys the partially created root, retires the observer, logs an error,
and retries at most once in five seconds. DLL lifetime must include its registered
unlink observer; invoke Cleanup before any supported plugin unload.

Translation keys (RAM/VRAM and IEC units remain conventional):
`RAM warm`, `VRAM standby`, `remaining`, `Off`, `ready`, `Paused`, `Warming`,
`Ready`, `created`, `used`, `uploads skipped`, `files`.
No percent or ETA is derived from cache occupancy: cap is only a memory ceiling,
and this snapshot does not know total bytes remaining. Remaining means the
current queue, not all assets in the world. Created/used/skipped counts are
observability, not an FPS claim.

Native frame default position is (16,96), below the owner's MSI overlay; three
18px text rows with native font, white text and black shadow. The background is
the native `Kenshi_FrameSkin`. Bounds are clamped on screen resize. Vanilla and
installed Dark UI Advanced XML share the same 16px side/12px top/14px bottom frame;
18px horizontal and 14px vertical padding keeps text outside the frame borders.
Very narrow viewports may clip text; no font shrink below 18px is applied.

Verification: `leopard/test_resource_warm_hud.py` builds/runs actual portable
format/rate/layout code and actual production HUD source against a bounded SDK
boundary model, rejects five compiled regressions, compiles production with the
real VC10/MyGUI headers, and inspects actual vanilla/Dark UI XML. Logs under
`build/resource-warm-hud`. Runtime boundary tests model renderer/skin dimensions
and ownership; they are not screenshots, real-game font rendering, FPS or long-run
leak measurements. In-game acceptance remains outstanding.
