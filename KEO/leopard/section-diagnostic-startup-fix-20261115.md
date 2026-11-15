# Private section diagnostic startup correction, 2026-11-15

Stage only, sources based on `1718df4`. No checkout/install/INI/release changes, game launch, gameplay input, capture or profiling. The owner stopped further game experiments. This correction removes two unnecessary native hook installations from normal detailed-log-OFF startup in both PROD and DEV. No new startup-second measurement is claimed, and two hooks cannot explain or resolve the whole reported twofold startup difference.

## Source-proven unnecessary work

`InstallNavMeshLife` installed hkaiStreamingCollection::removeInstance for its default-ON lifecycle diagnostic even while detailed logging was OFF. The existing retire wrapper immediately forwards while OFF. The separate addInstance wrapper had only three consumers: a diagnostic insertion counter, `NavMeshLifeOnAdd`, and `StitchSourceOnAdd`. The latter two already return immediately while detailed logging is OFF; the insertion count is read only by the detailed-log-gated PathGuard report. Neither hook controls native collection lifetime, path budgets/priority, crash rescue, section validity, adjacency, world/save reset or cache ownership.

The mandatory nmgStitch hook is retained: its `NmAdjCheckStitch` call performs functional scheduling work before the forensic logging gate. Section-key crash rings, extraction rescue, hierarchical guards and path priority/enqueue chains are retained.

## Change and lifetime

`hook_manifest.cpp` puts the independent section observers behind a main-thread, one-shot initializer. OFF startup merely marks the phase reached. ON startup initializes sampler vtables/frequency before native enable, then attempts retire and add independently. The existing camera owner heartbeat admits them on a later detailed-log enable. Reentrant native installation cannot duplicate attempts because attempted state is published first. Later OFF/ON, successful installs and refused installs never cause retries or resets. Lazy calls pass NULL for the startup count pointer, retaining no stack pointer. Both sites freshly verify their existing native prologue immediately before installation. Partial diagnostic refusal logs an error and does not affect unrelated gameplay hooks.

`HookWantInputs` now includes the existing cached detailed-log preference. Wanted totals exclude these two rows while OFF, so the existing runtime-ready comparison does not mistake intentionally omitted diagnostics for failed gameplay installation. The add wrapper now also returns after its unchanged native call while OFF, before even incrementing the diagnostic counter. Native function types, arguments, return values, exceptions, table rows/bytes, offsets and ABI remain unchanged.

No new thread, allocation, retained engine pointer, growing container, queue, hook, setting or per-frame scan was added. The owner heartbeat adds two bool reads and the cached detailed-log check until the attempt, then short-circuits. Native sampler pointers are initialized once before the hook can call them and remain process-lifetime constants. Already installed wrappers forward native work when logging is disabled. A later explicit log enable pays the same native installation work once; moving work is not represented as eliminating its cost when requested.

## Existing native startup cost evidence

The saved `runtime-narrow-texture-startup-first/KEO.log` has build gate completion at 0.038 s and hook/resource-probe installation complete at 4.048 s. The retire hook is reported at 1.461 s and the add hook at 3.202 s, within many roughly 31-33 ms successive install intervals. These are old inclusive timestamps, not isolated MinHook timings or a new result for the present PROD DLL.

Actual on-disk KenshiLib SHA256 `5b6240622d06801dc39896e385efb834b8e1fe2fcdf86067d071ae7a04f6c83b` is pinned by the bounded offline decoder. AddHook RVA2390 calls per-hook Enable14B9C. Enable calls Freeze144A0 before Patch1460C, then Thaw144E4. Normal zero-initialized freeze mode0 uses Toolhelp13F50: CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD), enumerate all threads, OpenThread/SuspendThread for this process, temporary bounded-per-thread handle storage, then resume/close/free. Alternative mode1 uses NtGetNextThread14144 and also suspends threads. Per-hook Patch1460C rebuilds/validates the trampoline via14548 and adjusts suspended instruction pointers via14304. No explicit 31 ms sleep/timer appears in these paths. Repeated enumeration/suspension is a concrete synchronous cost; these sources do not isolate which OS operation accounts for every observed gap.

Queued Apply14D90 freezes once before its patch loop, unlike immediate AddHook. It still patches via1460C per queued row and updates thread contexts per row. It can preserve a failure and continue enabling arbitrary later rows; there is no aggregate rollback. A transparent blanket QueueHook conversion remains rejected. A safe general replacement would need a new fail-closed publication/forwarding design and must account for side effects such as CohesionPatchStep modifying native bytes during InstallHooks. No SDK modification or private native rollback patch was made here.

Other entry/admission work was traced: the prologue table is a bounded 16-byte-per-row read; native bindings resolve once; GetKenshiVersion returns a previously initialized version object; GOG file/SDK SHA admission caches successful verification. No repeated full Steam-image hash/signature scan or equivalent multi-second redundant initializer was found in these paths. Existing whole-map builder observations of 180-210 ms remain distinct from hook installation; its lifecycle is outside this patch.

## Verification

`test_section_diagnostics_lazy.py` first failed on the old production initializer with `FAIL OFF startup installs no pure section hooks`. It extracts real production installer, want evaluator and wrappers; only native installation, game pointers/sampling/report sinks are replaced. VC10 /Od and /O2 cover OFF/ON startup, report before initialization, reentrant installation, independent add/retire refusal, repeated toggles, all16 want combinations, original argument/result preservation and native exception propagation. Seven compiled source mutants fail their intended assertions. Errors on refused native admission remain visible. No mock game execution establishes measured menu time, FPS or leak freedom.

Existing `test_navmesh_life_idle.py` passes nine production-wrapper controls in each optimization mode. Four compiled manifest suites (DEV/PROD/ZONEHAND1/ZONEHAND2) pass with detailed-log defaults and complete field mapping; the truth table explicitly enables detailed logging when exercising the original per-feature key combinations. The hook structural checker passes against an explicit inventory of729 staging CPP/H files; its standard git-ls-files entry cannot run unchanged because staging intentionally has no .git. Root owns final full-build/import verification and independent integration review.

Commands:

```powershell
python leopard/test_section_diagnostics_lazy.py --mutants
python leopard/test_navmesh_life_idle.py
python leopard/inspect_klib_startup_20261115.py 2390 2540 26A0 14B9C 14D90 1460C 144A0 144E4 14548 14304 13F50 14144
```

Generated compiler/run logs, actual-source fixtures, bounded SDK disassembly and source hashes are under `build/section_diagnostics_lazy_test`. No prepared or published update is implied.
