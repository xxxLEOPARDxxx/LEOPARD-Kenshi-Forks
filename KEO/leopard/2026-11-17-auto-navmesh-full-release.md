# KEO4: full automatic path repair release verification,17 November2026

Supersedes exterior-only5582933 scope after the user explicitly requested native
KenshiQOL-style action parity and performance testing. Prepared, installed locally,
not published. Published baseline remainsKEO3.

## Actual behavior

Main-thread native LevelEditor building/furniture repair across active zones, then
full NavMesh::generate(zone) through the installed KEO detour (native exterior and
eligible interior jobs/UI retained). Return0 from repair still proceeds to generation.
The original KEO cache/worker rules remain; exterior force marks are preserved.
Bounded discovery and once-per-loaded-world cell history, spaced accepted requests,
idle/loading/reset/adoption guards, proper F11 hold accessor and no native pointer
retained between ticks. Native repair is synchronous and not internally budgeted.

## Verification

- Compiled RED test rejected old exterior-only implementation with a runtime CHECK failure.
- Actual scheduling runtime: /Od and /O2 pass with fake native boundaries.
- Extracted production full-generation helper and manual-hold accessor: /Od and /O2 pass.
- Six compiled production mutants fail runtime CHECKs, including stale-clock spacing.
- Full391-source DEV/PROD builds pass.128 host suites pass.
- Both real native repair heads match32 authenticated bytes on pinned Steam/GOG EXEs.
-3MO catalogs,157 keys each: gettext roundtrip and Russian ASCII-hyphen validation pass.
- x64/startPlugin/MCM_Describe and183KenshiLib0.5.0 imports pass.
- Independent final review /root/auto_nav_native_design: no P1/P2 blocker.
- Canonical prepare_release.py KEO: descriptions/BBCode/privacy/credits/history, package CRC and contents pass.
- Public EN/RU descriptions fit Steam limits; histories1..4 preserved separately.
- Factory loader corrected to KEO.dll only; old package requested absentKEOCompat.dll.

## Performance limits

The user stopped Computer Use with physical Escape before any save was loaded.
The latest log contains no AutoNavmesh repair event. The user subsequently
confirmed the setting exists and requested continuation without game control.
No completed native-call/FPS/gameplay-success measurement is claimed.
Five scheduling-only runs of3 modes x2million ticks =30million fake-boundary calls:
- off: median0.001731 us/call, range0.001724..0.001785; native operations0.
- unloaded-grid: median0.008770 us/call, range0.008131..0.009148; native operations0.
- all-requested: median0.008198 us/call, range0.007777..0.008554; native operations0.
These timings describe only the scheduling logic under fake native boundaries;
they do not measure actual native locks, building traversal, generation or FPS.
Detailed repairUs/generateUs/furnitureChanged logging is available for real calls.

## Package and local state

- Archive: `D:\DEV\Kenshi\workshop\nexus\KEO-4.zip` SHA256`11c0249c080979afa49667f086ba16e2f1b8d945d0f5946bb998d17707dea358`.
- DLL: SHA256`137bd0a9398f1eadfea261d84ea58ba1deeac3f1fde6f63abe28259fe94375d0`.
- Descriptions: EN5469, RU7828 bytes.
- Personal INI restored exactly:6387c7e46e4fd253409da14f479312726ba7929d3c43a8dca108da15564a1d66.
- mods.cfg, published registry, ALL_CHANGELOG_MODS and other mod archives unchanged.
- Isolated save copy `_codex_autonav_perf_20261117` (~97.7MB) remains in local save directory.
  Automatic approval review rejected its removal with blocked by policy; no retry/circumvention.
- No Nexus/Steam upload or publication-ledger update.
