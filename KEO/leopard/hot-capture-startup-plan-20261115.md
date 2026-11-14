# KEO narrow texture capture and menu startup follow-up

Owner completed51307c5b test, asks about10seconds idle after menu appears.
Source baseline07b0204. No new gameplay settings, translations or release bump.

Evidence snapshot runtime-texture-retention-first:762retainedtextures,
2979100031logicalbytes,admitted1,pressure0,evicted0. GenericResource::load
observation162891634times/reused129371281/rejected1516729 in~225seconds reveals
wrong hot capture seam. Those counts are render-pass requests, not savedloads.
This follow-up must remove that interception rather than add another sample
counter to the same hotcall. Retention/lifecycle/sharedcap stay intact.

Task1: independent module implementer replaces genericloadhook by exact guarded
OgreTextureManager::load379890 hiddenTexturePtrout and actualD3D11Texture.loadImpl
renderer47D20. Manager observes completed real request output withoutcopy; actual
loadImpl handles reloadinvalidations/weakqueuebeforeouterResource publishesLOADED.
No Resource::load hook. AtomicOFFgatebeforeownerstate; absentRenderer waits and
retries250ms; fullbothcontracts beforeenable. No GPU work offowner. Header adds
managerRequests/loadCompletions; root logs separate bounded counters.

Task2 root: startupSeedExisting previously burns250ms on EACHgroup, including
empty/removedgroups. Share32totalnewentries across8groupattempts, soft1ms with
numericprogresscommittedbeforeyield. Neverbudgetthe rewind-to-offset loop, which
wouldstarvelargegroups. Existing8192totalvisits/256groups remain. In responsive
TITLEonly, slices mayrun>=16ms QPC; gameplay/slowmenu/cacheTick stays250ms. Same
20msframegap/1srecovery, nodoublenestedFrame/Timed slice. Completedseed earlyreturn
avoidsnativemanagerlookup. Roots2squietsettlement unchanged, no newhandshake that
couldblocklate roots. Debugbegin/end and firstOBSERVEDread1scadence prove phases.

Fail-first fixtures: old32/40/40sample now32/64/80, emptygroups8/16/complete,
smallgroupsshare32,8192DISTINCTlargeentries, optionaldeadline progress afteritem
advance, title32/64/65across16mswhilecacheTickunchanged andduplicateheartbeatnoextra.
TitlecoarseGetTickCountvsQPC600frames60Hz remainsgreen. Mutants coverall7faults.

Task3 independentfinalreview/nativecontracts/fullDEV+PROD/imports, preserve
foreign7Steelfiles, syncownsourcecommitpush, installonlyclosedorrequestclosure.
NoINI/MO changes. One nextboot checks initialreadlatency andnarrowcapturecounts
alongwithresidualNPCstutters; no unsupported guarantee of instantstart/zeroFPS.

Ledger: task1sourcefrozen92nativechecksOd/O2,40nativecontracts,16mutants.
Task2green1255checksOd/O2+7compiledmutants. Review/fullbuild/installpending.
