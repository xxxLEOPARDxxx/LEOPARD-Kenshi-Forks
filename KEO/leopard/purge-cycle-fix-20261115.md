# Private: prevent purge fallback starvation, 2026-11-15

Scope: staged source and offline/native-wrapper fixtures only. No game run,
profiling, installed DLL, personal settings, source checkout or publication.

The single fallback clock in `render/ogre_purge_skip.cpp` is advanced by the
first due purge. Subsequent group/manager purges in that same adoption cycle
then see a fresh clock and skip. This can indefinitely starve later targets.

Plan:

1. Include the actual production wrapper implementation in a native fixture;
   demonstrate that a six-target due batch incorrectly forwards only once.
2. Give the six verified native call sites independent fixed-size clocks.
   Verify their exact pinned Steam instruction windows offline and at
   startup. Unknown callers, incomplete admission and non-owner calls forward.
   Preserve the feature flag, transition eligibility and strict timeout rule.
3. Test due/cold/repeated/interleaved batches, original argument forwarding,
   unknown callers, OFF/transition, exceptions and call-site refusal. Run
   Od/O2 fixture and unchanged pure policy controls, plus old-clock mutant.
4. Hand the staged files and verification evidence to the root for independent
   review and its existing full-build/integration workflow.

This fixes a resource-cleanup policy bug. It does not establish the cause or
size of the owner's campaign FPS difference or claim any measured speedup.

## Implemented boundary

The six native call sites belong to actual Steam `ZoneManager::processLoading`
at RVA A0E950, resolved through the actual installed KenshiLib export/table.
The pinned decoded EXE SHA256 is
`504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e`.
Native order is TextureManager at A0EB67 (`reloadableOnly=false`), MeshManager
at A0EB79 (`true`), then group calls A0EBB4 / A0EC02 / A0EC50 / A0EC9E for
General / foliage / Landscape / Overlaymaps (`true`). Their six distinct return
addresses are the immutable keys. No manager pointer or group string is retained.

At installation the EXE timestamp 65D604D7 and image size 232C000, plus all six
32-byte caller windows ending at those return addresses, must match. Both existing
Ogre hook admissions must also succeed before skipping is active. Unknown callers,
the wrong hook kind, workers and partial hook installation forward native. There
are only six timestamps and six keys; no allocation, world walk, new native hook,
IO, logging or profiling timer was added to purge decisions.

Each key starts its timeout at installation, preserving cold skip behavior and
the existing strict `elapsed > maxSkipSeconds` boundary. OFF and real transitions
always forward. A due fallback updates only that target; it cannot refresh the
other five targets' deadlines. The original argument tuple, call count and exception
remain unchanged. The existing pure single-target policy is unchanged.

An independently verified GOG caller image is unavailable in this task. Its
different EXE identity therefore leaves ordinary native purging intact; this
change does not claim optimized GOG purge skipping or guess interior RVAs from
the function-entry translation. Other KEO features are unaffected.

## Verification

- Fail-first fixture compiled the unmodified old production wrapper bodies and
  failed at the due six-call batch: only the first manager call forwarded.
  Preserved output: `build/purge-cycle/red-original-shared-clock.txt`.
- Final production-wrapper inclusion controls: **30 checks each PASS /Od and /O2**.
  Source extraction removes only includes/intrinsic declarations; it does not
  rewrite the decision or hook functions. Tests substitute native calls, owner,
  elapsed clock, module identity and a synthetic caller image populated from the
  pinned file. This is not a live hook-installation test.
- Controls cover cold/due/repeated/interleaved batches, all six targets, original
  arguments, transitions/OFF, strict boundary, disabled fallback, native throw,
  unknown/wrong-kind/non-owner callers, mismatched bytes, GOG/unknown identity and
  partial admission. Unchanged `ogre_purge_units` pass in both modes.
- Offline caller checks independently resolve actual imported manager getters,
  virtual +68 calls, group import targets and all four native string literals.
  Compact evidence: `build/purge-cycle/native-callers.json`.
- Four independently compiled production mutants fail behavior as expected:
  collapsed shared clock, unknown-call skipping, partial-hook skipping and ignored
  hook kind.
- Actual affected production translation unit compiles with real headers in
  **DEV and PROD /O2**, logs `build/purge-cycle/production-{DEV,PROD}.txt`.
  Full plugin build, independent change review and integration belong to root.

Reproduce scoped controls: bundled Python
`leopard/test_ogre_purge_wrappers.py`. No game/process, profiling or runtime data
collection is performed by this command. Source checkout and installed DLL remain
untouched by this task.
