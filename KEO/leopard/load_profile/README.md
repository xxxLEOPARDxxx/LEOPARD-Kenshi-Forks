# Full save-load CPU/wait recording

Private development tooling; not a mod payload or release feature.
No DLL, settings, save or game-state writes are performed by these tools.

`Capture-Load.ps1` uses Windows WPR, an elevated token and a unique named
instance. The parent creates an exclusive run directory and launches the
helper hidden. The owner must already have finished warmup at the main menu.
Never instruct the owner to load before these gates pass:

1. `ready.json` is true for this live helper/instance/PID, with mode File.
2. `profiles.txt` confirms `KeoSaveLoad.Verbose.File`.
3. Parent writes `begin.request` and observes `recording.json` transition to
   `recording_load` for the same instance.

The parent asks the owner to load a save and report when all surrounding NPCs
have appeared. On the reply, write `stop.request` immediately and wait for
`result.json`. A successful save revokes readiness and clears session debt.
If `activeSessionNeedsRecovery` is true, the parent must recover/stop this
exact named instance; never run an unqualified/global WPR cancel.

The recorder waits at most 120 seconds for the begin handshake, then allows
MaxSeconds of load recording (default 360). These are polling-loop limits,
not hard deadlines on WPR's own start/merge operations. File mode is
sequential and prevents circular-buffer overwrite; it is not a fixed-size
file. Temporary-size/free-space checks are approximate polling safeguards.
Both temporary/output volumes are checked with a conservative merge reserve.
Prefer temporary recording on another physical drive than the game where
space permits. Profiling overhead and extra IO are not assumed to be zero.

Errors after begin attempt a recovery save. Failed recovery retains named
session ownership for the parent instead of automatically discarding data.
Metadata writes are best-effort during failure cleanup. An exit code or
`saved=true` alone is not proof of a complete CPU profile.

## Post-capture gate

Decode offline with the retained TraceDecode tool; select the captured game
PID/process lifetime. Check eventsLost, sample/target event endpoints,
per-second coverage, missing/truncated stacks, process/thread lifetimes and
owner boundaries. A trace with zero eventsLost can still be an incomplete
ring-buffer suffix, which invalidated the older 2026-11-15 recording.
Matched off-CPU totals exclude unpaired edge intervals. Missing-stack time
is only a subset of matched waits; it is not unknown edge/lifetime coverage.
Unknown edges and their potential upper bounds are not measured waiting.

`analyse_waits.py RUN --symbols native-symbols.json` streams decoded tables,
maps owner QPC boundaries using recorder frequency plus a same-event QPC/time
pair, and counts clipped wait intervals. Large worker idle times overlap
with work on other threads and are not added to the critical-path duration.
Native names are emitted only for an authenticated export/function range;
otherwise module RVAs remain explicit.

This profile includes CPU samples, context switches and ready-thread events.
It does not include a GPU timeline or dedicated Disk/File IO providers and
must not be described as complete profiling of those subsystems.

## Actual first complete recording, 2026-11-17

Run: `load-20261117-144655-92db61b8` under the private etw_tools/captures stage.
File mode confirmed before begin acknowledgement and owner instruction.
Stopped on `owner_npcs_ready`; saved 617420557-byte ETL, no capture/recovery
error, readiness revoked, no active-session recovery debt.

Decoded session 79310.028 ms; target CPU samples 7.5773..79279.9524 ms,
145621 samples, 1143 without a stack (~0.785%), target samples in all 80
second bins, reported eventsLost=0. Owner instruction/readiness boundaries
18740.5643..78635.9088 ms. The exact click/load phase is narrower and must be
identified from executing stacks/log anchors, not treated as this whole
59.9-second instruction interval. There are 6230 missing blocking stacks,
6209 rejected blocking-stack owner mismatches and one truncated stack;
254 unmatched edge gaps (235 with uncertain lifetime), of which 186 intersect
the owner instruction window, are reported separately from matched waits;
preserve those limitations when interpreting waits. No isolated speedup or
absence of profiling overhead follows from these coverage facts.
