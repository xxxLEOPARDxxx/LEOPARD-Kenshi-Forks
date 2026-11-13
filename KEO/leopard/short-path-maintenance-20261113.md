# Short-distance routing investigation, 13 November 2026

Scope: direct local MOVE first, then native FOLLOW and squad movement; long
cross-world routes follow only after local results are understood. Preserve
native geometry, gate barriers, water costs, task priority and zone readiness.

The installed DEV DLL is 5463CDF65B9276D90AE00C712E21E710715711919DC6D0A6B690AF26DC984ED9.
Both its last log and personal INI show routing OFF. Do not describe existing
detours as a proven failure of the currently inactive hierarchical search.

## Evidence and decisions

- The OFF master leaves the older park/growth/retry recovery poll reachable.
  Its actual extracted code can resend an obsolete MOVE despite master OFF.
  Guard the existing adapter after pause rebasing; independent zone management
  and tracking remain intact. 85 controls in each optimization and an omitted
  guard negative control cover the repair.
- Existing DEV traces sample every 7.5 metres. Small circles can remain inside
  that radius and be absent from the trace. Add an explicit local-only trace
  mode at 1 metre, retaining default/long trace spacing and bounded buffers.
- No new path algorithm is justified yet. Hierarchical ON accepts successful
  paths without native cost comparison, but that alone does not prove this
  user's detour. Compare actual travel and requests in a short reproducible
  scene before changing search costs or adding path shortcuts.

## Work and verification

1. Reproduce OFF steering with actual production extraction, fix, review.
2. Add local trace sampling boundary tests, observe RED, implement, run GREEN.
3. Build the test DLL, audit changed sources and imports. Preserve personal
   config and prior binary. Enable existing routing and local trace only for
   the identified diagnostic session; keep native readiness unchanged.
4. Capture one actor on short open-ground and obstacle routes, then a follower.
   Measure travelled path and pauses separately from portal-midpoint geometry.
5. Fix only an evidenced search/follow defect, verify boundaries, repeat the
   exact episode. Offline tests are not gameplay or FPS acceptance.

No release, version bump, public announcement or personal changelog entry.
The unrelated existing steel_jobs working changes are preserved.

## First test candidate

Installed locally while Kenshi was closed:
`3392e6489a329352eaa08fd40d55a08a6d2ac53e47400d077853bb41ed1564f1`
(DEV). Original DLL/INI/log are backed up under
`D:/DEV/Kenshi/build/keo-short-path-20261113-075022`. Personal INI changes
for this test are only `keoRouting=true` and `movementTrace=local`;
Detailed log was already enabled. Other plugin configuration and mods.cfg
were preserved. This is a diagnostic baseline, not acceptance of route quality.

Verification: 22 relevant host suites passed, master configuration 436 checks
in each DEV/PROD fixture, reissue guard 85 per optimization, job cancellation
142 per optimization. Compiled negative controls reject the omitted OFF guard
and existing cancellation failures. Local trace RED lacked the new spacing
argument; GREEN retains legacy sampling/ring tests and adds one-metre controls.
Both native variants built all 352 sources; DEV restored 344 cached objects
and compiled 8, PROD compiled 352. Each DLL's 180 KenshiLib, 49 MyGUI and one
Ogre imports resolve in installed libraries. Native profiles were not changed.
Independent read-only review found no blocker. Runtime and FPS remain pending.
