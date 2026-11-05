# KEO: private squad-frequency diagnostic, 2026-11-05

Status: installed locally for observation, **not a published update**. The public release remains v1. No ZIP, Steam upload or personal published changelog entry is required for this candidate.

The owner intentionally maximizes squad size. The issue under investigation is excessive **frequency** at the owner's normal frequency setting of 1, including squads appearing near the camera. No duplication claim and no spawn restriction are introduced.

`SquadWatch` runs only with KEO's existing Detailed Log / Debug enabled. It incrementally reads active and unloaded platoon lists at the already joined main-thread reporting point after loading refusal. It adds no native hook, native import, gameplay write or worker reading live entities.

Every ten seconds after the previous pass completes it collects a rolling census. Stable full handles are scoped to the existing world epoch. Reports distinguish first observation, becoming active and reappearance; **first observation is not proof of native creation**. Initial baseline events are summarized rather than dumped individually. Last-known positions and planar camera distance help correlate the user's observed near-camera appearances. Current population/squad/raid multipliers and game speed are context, not settings modified by this diagnostic.

Bounds: 32 native entries per frame, 256 visited factions, 8192 list visits per census, 8192 identity slots with at most 64 probes per observation. Up to 24 copied detail events per census, drained two per frame. No game pointer or string survives a call. Queue/identity state resets on world epoch changes and on disabling/re-enabling Debug. Saturation, reader faults and omitted details are reported explicitly; truncated statistics are incomplete. Missing observations do not imply destruction.

Verification: actual policy tests compiled/run under VC10 /Od and /O2, including off/on, world reset, active/unloaded union, reappearance and 20000-observation saturation. Native DEV and PROD builds passed; 210 installed game imports verified with no additional native imports. Existing native-readiness regression passed 6144 actual-hook cases in both modes and refused old-policy negative controls. Independent review accepted the bounded queue, probe limit and lifecycle after corrections. Runtime FPS/CPU cost still requires a real gameplay measurement.

The public DLL before this diagnostic is backed up under game/_codex_work/20261105-keo-squad-watch/before. Personal INI and RE_Kenshi manifest were not changed. Debug was already true. Latest candidate hash and bounds are in squad-watch-verification-20261105.json.
