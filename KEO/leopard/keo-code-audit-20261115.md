# KEO maintenance audit: NPC streaming, loading, retained work and startup

## Scope and evidence

The owner requested concrete fixes without additional gameplay reproductions or
profiling, then expanded the task to auditing the fork and its slower startup.
The slow pre-menu startup predates our RAM/VRAM work. It must not be blamed on
the new caches merely because they were implemented recently.

`keo-source-inventory-20261115.json` enumerates all 364 production translation
units and their headers/includes across 16 areas. The inventory is a source map,
not a claim that every line is correct. Detailed review in this pass follows the
startup, frame dispatch, retention, NPC body completion, material setup, archive
IO, loading pause and configuration paths. Unrelated seven local Steel jobs
changes were preserved separately. A complete line-by-line review of every
inventory entry remains a larger task; this report does not label it complete.

Evidence is current source, actual installed SDK/binary contracts and previously
saved logs. Standalone tests exercise production code/native Ogre objects. No
new game run, ETW session, injected game profiler or gameplay measurement was
performed. In particular, the existing ETW file retained only its last4.7s, and
cannot be treated as a complete streaming trace despite zero EventsLost.

## Confirmed findings addressed in this patch set

1. **Stale character retention cells.** Baseline and arrived characters returned
   before publishing currentZoneX/Y. Their old cells could keep receiving leases
   and priority. Publish the already-validated live cell before those branches.
   No additional position scan or preload was introduced.
   See `tracking-live-zone-fix-20261115.md`.
2. **Purge fallback starvation.** Six native purge calls shared one timestamp;
   the first due call renewed it and prevented the remaining five from taking
   that timed fallback. Each verified native call site now has a bounded clock;
   unknown/worker/unadmitted/GOG paths preserve native purging.
   See `purge-cycle-fix-20261115.md`.
3. **Orphan geometry pins.** A Mesh removed from MeshManager could stay loaded
   through our reference and remain charged despite being unavailable to native
   lookup. Existing two-entry heartbeat refresh now validates creator and
   manager-handle identity before keeping the pin.
   See `geometry-orphan-fix-20261115.md`.
4. **Final NPC body meshes were not observed.** The native body builder removes
   clone poses and returns a replacement Entity from switchBodyMesh. Its eligible
   final mesh now enters the existing weak queue after native success. The old
   Entity and consumed input MeshPtr are never read afterwards. No morph/RNG,
   unique-player naming, animations, scene creation or sharing rules are changed.
   Animation/manual-LOD exclusions remain. See
   `npc-final-mesh-retention-fix-20261115.md`.
5. **Loading paused demand work.** ResourceForward's speculative pause stopped
   the entire RAM worker. Demand-observed requests now continue at the configured
   rate, can promote queued speculative work and keep priority. Speculative reads
   defer rather than blocking the sole worker; bounded cancellation/pressure/
   identity/budget policies remain. See `save-load-cache-fix-20261115.md`.
6. **Path work for unsupported files.** Every read-only archive open built an
   absolute path before rejecting unsupported formats. Shared pure extension
   admission now forwards those calls directly. See
   `archive-fast-admission-fix-20261115.md`.
7. **Unnecessary shader-map invalidation.** Assigning a different shared native
   parameter map does not mutate the old map, yet KEO scanned4096slots or flushed
   the main-thread cache. Skip that invalidation only for the proven native
   control-block/deleter contract. Same-pointer rebinds and SDK/custom ownership
   keep conservative invalidation. Actual last-reference destruction stays
   authoritative. This needs its dedicated review/native fixture evidence; no
   claim that it caused the previously recorded127ms attachment spike.
8. **Pure timing hooks installed while logging was OFF.** Only texture/final-body
   capture is mandatory for the caches. Other timing hooks are installed once
   when detailed logging is enabled via the existing owner heartbeat. Cache
   capture remains independent of logging. See
   `startup-probe-lazy-fix-20261115.md` and final NPC integration report.
9. **Missing configuration clamp entries.** RAM GiB, GPU GiB and read MB/s had
   range metadata but were absent from the explicit load clamp order. The three
   keys now follow their published bounds. Source template and stale host-test
   expectations/goldens were updated; actual layout code was already correct.

## Startup findings that are not a claimed complete fix

The saved startup log completes its build gate at0.038s and runtime/resource
hook installation at4.048s. Many installation intervals are31-33ms. KenshiLib
AddHook immediately enables each hook. This is existing startup evidence, not a
new factor measurement. Its synchronous overhead is a significant remaining
candidate for the owner's observation, which predates our caches.

A blanket change to QueueHook/ApplyQueuedHooks was rejected. Actual installed
KenshiLib applies hooks individually, can fail after enabling an arbitrary
subset, and exposes no corresponding rollback/query contract. Existing paired
installers publish readiness immediately. Preserving behavior under partial
failure needs a deliberate installation architecture change, not a text replace.
Native source/byte evidence is in `startup-probe-lazy-fix-20261115.md`.

The default planner builder also scans the whole-map base before TITLE. Existing
warm-cache runs of3995tiles took180-210ms; this does not explain a multi-second
or twofold slowdown. Startup bulk RAM work has no explicit first-title gate,
but the examined logs did not show pre-title bulk reads. Neither candidate was
turned into a speculative behavioral patch in this set.

## What KEO already does outside the camera

The engine has frustum culling; not drawing an NPC does not remove its AI,
navigation or physics. KEO's default `onScreenStagger` distributes expensive far
off-screen visibility checks, preserving player/engaged/visible/unsafe cases and
full checks near the camera. `pausedOffscreenSkip` is specifically a paused-world
optimization. `sceneForkSkip` avoids worker forks only when their lists prove
there is no work; `instEmptySkip` and render-state/shader caches target separate
render overhead. They do not globally freeze off-screen squads.

No new rule disables world simulation behind the camera. This pass reduces
confirmed repeated work and stale retention while preserving gameplay.

## Save-load and performance limits

Completed RAM file data already survives world/save resets. Warming bytes does
not by itself deserialize a save, create characters, attach equipment, compute
body variants or rebuild AI/physics state. Startup inventory currently targets
textures; model/skeleton source-byte prewarming would need consistent option/HUD
text, ordering and budget tests. It is not silently claimed as implemented.

The baseline DEV DLL's2142720bytes include1306624bytes of .text and465920bytes
of .rdata. File size is not frame time. Existing bounded tables also have virtual
zero-initialized storage, so neither a1MB nor2MB file establishes efficiency or
leak behavior. PROD already uses /GL,/LTCG,/Gy,/OPT:REF and /OPT:ICF.

The fixes establish specific code behavior and native ownership safety. Actual
startup/save/FPS gains, disappearance of all NPC stutters and the full cause of
long-campaign decay remain unmeasured, as requested. No release version bump,
Nexus/Steam publication or personal published changelog entry accompanies this
private maintenance build.
