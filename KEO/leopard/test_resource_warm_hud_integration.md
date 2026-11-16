# Resource warmup HUD integration (private, 2026-11-14)

Add `src/resources/resource_warm_hud.cpp` to the build list. Caller supplies a
zero-initialized `resource_warm_hud::Snapshot` by scalar copy, never native pointers.
`ramReadBytes` is a cumulative **actual read** count, not cache hits or target
capacity. `ramPending`/`gpuPending` include in-flight work. `gpuAdopted` and
`gpuSkipUploads` are cumulative native consumption/avoided-upload counters.

Call `ResourceWarmHudTick(enabled, snapshot)` only on the admitted UI thread,
after native GUI readiness, at the existing resource main tick. Disabled state
destroys the HUD immediately. With the option on, an unfinished RAM startup
(including waiting for roots, scanning and gaps between reads), active/pending RAM
reads, or pending GPU preparation admit display. Startup completion or a terminal
capacity refusal hides all three rows once no requests remain. Cache occupancy,
ready standby entries and held model/texture references do not admit display.
New work recreates the rows and resets the throughput sampler; a completed
startup no longer overrides current work with an old percentage. An idle tick
performs only a bounded scalar policy check and does not format/measure text or
allocate widgets. First admitted active frame creates three independent
cached root text rows without a frame/panel; no GUI tree lookup. Painting, measurement and string formatting
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

All mutable HUD state is one fixed global object; exactly three owned widgets;
no entities, world scans, queues, histories or uncapped containers. Widget creation
failure destroys all partially created rows, retires the observer, logs an error,
and retries at most once in five seconds. DLL lifetime must include its registered
unlink observer; invoke Cleanup before any supported plugin unload.

Translation keys (RAM/VRAM and IEC units remain conventional):
`RAM warm`, `VRAM standby`, `remaining`, `Off`, `ready`, `Paused`, `Warming`,
`Ready`, `created`, `used`, `uploads skipped`, `files`.
No percent or ETA is derived from cache occupancy: cap is only a memory ceiling,
and this snapshot does not know total bytes remaining. Remaining means the
current queue, not all assets in the world. Created/used/skipped counts are
observability, not an FPS claim.

Default text position is (16,96), below the owner's MSI overlay; three
18px text rows with native font, white text and black shadow. There is no panel
or frame. Bounds are clamped on screen resize. `Kenshi_TextBoxEmptySkin` is the
actual ResourceSkin, with `Kenshi_StandardFont_Medium` assigned explicitly before
font-height or measurement calls. `Kenshi_TextboxStandardText` is a ResourceLayout
template, and must not be passed as a skin. The real native font, its resource,
and the actual skin must exist before any widgets are created; singleton pointers
alone do not prove readiness. Parent also defers HUD work until a scene tick.
Very narrow viewports may clip text; no font shrink below 18px is applied.

Verification: `leopard/test_resource_warm_hud.py` builds/runs actual portable
format/rate/layout code and actual production HUD source at /Od and /O2 against a bounded SDK
boundary model, rejects eighteen compiled HUD regressions with assertion exit 1 (crashes and compile
failures are rejected as invalid controls), compiles production with the
real VC10/MyGUI headers, and inspects actual vanilla/Dark UI skin XML plus the
template-to-skin/font contract. Logs under
`build/resource-warm-hud`. Runtime boundary tests model renderer/skin dimensions
and ownership; they are not screenshots, real-game font rendering, FPS or long-run
leak measurements. In-game acceptance remains outstanding.

RAM/startup and GPU lock snapshot validity is explicit: `resource_cache::Stats::lockedSnapshotValid`
is set only under a successfully acquired non-waiting lock and copied to the HUD
as `ramSnapshotValid`, with the matching GPU store bit copied as
`gpuSnapshotValid`. When either enabled source snapshot is unknown, the HUD
keeps its existing rows and captions without formatting, measuring, creating, or
retiring them. A missing sample cannot resurrect a finished startup from default
zero flags or hide active work from default zero counters. Master-off cleanup and
GUI/manager disappearance still run before this early return. Disabled sources do
not depend on their snapshot validity. No prior resource snapshot is retained and no
worker/cache scheduling changes are made. The exact production `Snapshot()` body
is separately extracted and exercised at /Od and /O2 with deterministic lock
refusal, success, terminal startup, and subsequent refusal. The exact GPU `Snapshot()` body is also exercised with successful and refused
locks. Two compiled missing-validity-publication controls are rejected with CHECK
exit 1.
