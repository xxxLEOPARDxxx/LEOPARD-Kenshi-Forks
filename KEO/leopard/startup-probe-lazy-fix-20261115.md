# Private KEO optional loading-probe startup fix, 2026-11-15

Stage only. No checkout edit, whole-module build, installation, INI/UI change, gameplay run, capture, profiler, or game-process access. The installed DLL remains unchanged. This reduces unnecessary installation work when detailed logging is OFF; an actual new main-menu/FPS improvement was not measured or claimed.

## Concrete change

`src/resources/resource_load_probe.cpp` previously installed every loading timing seam from `ResourceLoadProbeInit`, even with detailed logging OFF. Normal Steam initialization has 14 attempted seams: one Texture2D consumer hook plus 13 optional timing hooks; a refused common mesh callback can add the existing appearanceBody fallback. The array bridge owns its existing hook separately and adds no hook here.

Texture2D at native Steam RVA `44ACD0` remains mandatory during initialization. Its existing post-success call to `ObserveLoadedResult` remains independent of detailed logging, preserving the recently installed ordinary DDS retention coverage fix. No wrapper body, ABI, native arguments/result, exception behavior, ownership, or exact byte contract was changed.

Optional timing hooks now have a separate one-shot initializer. It runs immediately if detailed logging is already ON, otherwise from the existing owner-thread `ResourceLoadProbeReport` heartbeat on first enable, before the ten-second report throttle. It cannot run until the mandatory capture initialization attempt finishes. Marking the diagnostic attempt before installing its hooks prevents reentrant double installation. A refusal is still final for that process; repeated OFF/ON does not retry or overwrite an unknown patch. GOG admission remains closed. Debug OFF reporting still returns before timing clocks/report work.

This adds three owner-thread booleans and one short owner-heartbeat check. There are no new hooks, threads, queues, retained native pointers, hot-wrapper work, or configuration options. Existing optional hooks remain installed after later disabling the log and retain their original immediate-forward behavior. Enabling detailed logging later can incur the existing installation work once; it is not represented as free.

`ResourceCacheInit` calls `ResourceCacheTick` before `ResourceLoadProbeInit`. The explicit capture-complete gate therefore also prevents that first report from installing diagnostics prematurely. Mandatory capture refusal does not prevent independent optional diagnostic admission when requested.

## Verification

The new integration fixture first ran against the unmodified production initializer and failed with `FAIL Debug OFF installs only mandatory capture`. After the change, 96 production initialization/report checks pass under both VC10 `/Od` and `/O2` across ten isolated process scenarios: OFF startup; ON startup; report before initialization; worker calls; mandatory contract refusal; mandatory AddHook refusal; optional refusal; existing body fallback; GOG refusal; and reentrant mandatory/optional installation callbacks.

The fixture compiles the production initialization/report implementation. Only native installation/module-resolution dependencies are rebound to a fake backend; it does not execute game code. Separate actual-file tests retain native admission coverage. Eight compiled production-init mutants each fail their expected assertion, including missing capture, Debug-gated capture, unconditional timing hooks, duplicate admission, missing lazy heartbeat, missing initialization gate, premature capture completion, and bypassed GOG guard.

Existing 114 production wrapper controls pass in both `/Od` and `/O2`; DEV and PROD production translation units compile. Actual-file preparation contracts pass (12), completion attribution contracts pass (10), and texture image/export/vtable/load-path contracts pass (53).

Commands from the game directory:

```powershell
python '_codex_work/keo-resource-cache-20261114/leopard/test_startup_probe_lazy.py' --mutants
& '_codex_work/keo-resource-cache-20261114/leopard/test_resource_load_probe.cmd'
python '_codex_work/keo-resource-cache-20261114/leopard/test_appearance_probe_native.py'
python '_codex_work/keo-resource-cache-20261114/leopard/test_completion_attribution_native.py'
python '_codex_work/keo-resource-cache-20261114/leopard/test_texture_retention_native.py'
```

Integration logs and generated fixture translation units are under `build/startup_probe_lazy_test`; native wrapper build outputs remain under `build/resource_load_probe_test`.

## Central batching explicitly rejected

The existing saved `runtime-narrow-texture-startup-first/KEO.log` records build-gate completion at 0.038 s and completion of runtime/resource-probe installation at 4.048 s, with many successive installation gaps of 31-33 ms. These are existing inclusive startup observations, not a new measurement or proof that every gap is solely MinHook execution. The optional probe portion ends 3.689 -> 4.048 s in that run. This motivated removing unnecessary diagnostic installations while OFF.

Batching the central manifest would require safe delayed publication and failure handling. Actual installed KenshiLib.dll SHA256 `5b6240622d06801dc39896e385efb834b8e1fe2fcdf86067d071ae7a04f6c83b` was decoded as a file. AddHook export `2390` immediately enables at `2497 -> 14B9C`; QueueHookInternal `2540` queues enable at `2605 -> 14CA0` and returns success zero on its success path; ApplyQueuedHooksInternal `26A0` calls `14D90` and translates nonzero aggregate status to FAIL.

Critically, the apply loop at `14E6C` invokes per-hook patching `1460C`. At `14E77..14E79` it preserves a nonzero error in ESI, then continues through `14E7C..14E84` to later hooks. There is no rollback of successful hooks. Therefore aggregate failure can leave an arbitrary partially enabled set. Existing central installers publish paired flags/counters immediately after each successful HookInstall return. Clearing originals on aggregate failure could crash enabled hooks; retaining originals while claiming dependent pairs installed could activate incomplete behavior. The public API does not supply rollback/query functionality needed for a small transparent replacement.

The actual linked library archive hash is `0b0e11eb80e2f7172ed1f305ab83696d3ee3866cadcae083001aefc43af50fa1`. Its static Functions wrapper member exists; source has a historical queue/apply module-ID correction, but executable wrapper-ID verification was not completed because the independently proven partial-apply semantics already reject the proposed conservative transformation. No central hook batch, generated forwarding thunk, new publication protocol, native rollback patch or fallback duplicate AddHook was implemented.

## Other startup findings and limits

The default whole-map planner builder also starts before TITLE. However, existing warm-cache builds of all 3995 tiles took only 180-210 ms in the saved runs. This does not explain a multi-second or twofold startup slowdown. Deferring the builder would require preserving first-world fallback, request/event delivery and generation handling; it was left unchanged.

Bulk RAM startup scanning lacks an explicit first-title gate in source, but the examined saved logs had zero startup inventory files at the first visible menu summary. It is a possible early-work admission gap, not observed pre-title bulk IO in those runs. Unsupported archive-extension early bypass is handled separately by the parent; this change does not touch resource_cache.cpp.
