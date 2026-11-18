# Late task-factory compatibility, 18 November 2026

Status: ordinary functional source fix, built and verified offline. Not installed,
packaged or published. Published release remains 5. No player diagnostic build,
game launch, profile or new capture was requested. Private player logs remain in
ignored build storage, not in source or a distribution archive.

## New supplied evidence

`KEO Logs 3.zip` contains KEO.log, RE_Kenshi_log.txt and two exception records.
The initial native-task-contract refusal in the previous archive is absent.
KEO.log line113 instead reports at14.951s:
`Steel jobs: registration refused; custom assignments disabled`.
The corresponding RE log event is at41.166s, after the remaining plugins load.

HookTitle emits this generic refusal for failed metadata registration OR failed
InstallFactory. There is no custom-ID ownership or factory AddHook failure line.
InstallFactory can silently reject a changed native contract after startup.
The archive contains neither the relevant live bytes nor the player's DLL, so
it does not identify the exact rejected site or prove which plugin caused it.

The MyGUI access-violation record is separate evidence. The supplied session
continues well beyond that event and eventually retires its navigation workers.
Do not describe the record alone as proof of a terminating KEO crash.

## Reproduced defect and change

InitializeFactory may run before another plugin hooks the task factory.
InstallFactory runs later at the title screen and revalidates that entry.
The previous check rejects an ordinary five-byte E9 entry chain even when the
remaining native window, dispatch and all class tables are unchanged.

Admit that normal chain ONLY for FACTORY, alongside the existing CONTEXT_SHOW
exception. All27 remaining bytes must match. Native factory dispatch, metadata
helpers, score thunk and every vtable entry remain strict. The factory is hooked
through the SDK's existing chain; Create calls its original once, checks the
returned native class and preserves foreign custom-ID refusal. It does not
overwrite another plugin's custom task or vtable.

Normal error messages now distinguish metadata registration failure from a
post-startup factory/data/vtable contract change. This is not a diagnostic mode.
No new gameplay scan, allocation, retained pointer, timer or hot-path I/O.

## Verification

- Expanded production admission regression first FAILS on the old code at the
  late factory-hook scenario; then212 checks PASS in Od and O2. Includes both
  shared hooks together, every factory tail byte, arbitrary patched opcode,
  unreadable windows, strict direct dependencies/vtables and ASLR.
-206 production native-factory controls PASS.
-321 extracted input/caption controls PASS in Od/O2; eight intentional negative
  controls fail as expected. Steam file windows match the SHA-pinned executable;
  GOG uses the existing reviewed frozen windows, not a fresh GOG runtime test.
- Complete391-source DEV and PROD builds PASS in separate `_factorychain`
  outputs. Production SHA256:
  d0f442610ce92d362178ff924d006bc1672e10b62211e244653fbdddb986a033.
  Development SHA256:
  7700f50ee18157907979d20356b339ccba149d6368f7dc3bb58bc5f25ae4b334.
- Independent review found no blocking code issue. The comment at InstallFactory
  was corrected to describe supported entry chains accurately.

This fixes a reproduced compatibility defect matching the new failure phase.
It is not proof that the particular player's failure has this exact cause.
Personal settings, installed DLLs, saves and release/publication metadata have
not been changed by this work.
