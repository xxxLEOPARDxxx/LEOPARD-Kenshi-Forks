# KEO narrow capture and earlier startup: verification, 2026-11-15

Runtime snapshot51307c5b is runtime-texture-retention-first. Texturecache admitted,
762pins/2979100031logicalbytes/evicted0/pressure0. Its general Resource::load hook
generated162891634observations/129371281referencehits in~225s. These were repeated
render-pass requests, not avoided texture reloads; the frequency itself warrants
removing the overly broad seam. No controlled FPS cost was measured.

Captured startup0..20s:10.249s69roots/noinventory/noRead;20.25s598roots/7379files/
585546787readbytes. Firstreportedcomplete104.989s; fastreadstoppedafter12629987010
bytes at64.337s, so do not compare105s with previous64s as menu-onlywarmduration.
Late scopedupdate171.272s42674us/callback42438us/morph8147us/factoryProcess0;
residualnotfullyattributed. Largestrecordfactorydelta nowdistinguishes construction.

Changes:
1. Generic Resource::load is no longer hooked. Exact native Manager.load379890
   observes its successful actual TexturePtr hiddenoutput, forwarding all6stack
   args(float/bools/enums) andresult/ownershipunchanged. ActualD3D11Texture.loadImpl
   renderer47D20 handles actual successfulloads/reloads weakly beforeouterResource
   publishesLOADED/size. Noengineworkmovedthreads. Full32/PE/export plusrenderer
   vtable/RTTI guards, bothrequired. AbsentRendererwaitretry250ms; OFFatomicgate
   beforeownerstate; partial/unknownhookadmission staysdisabled. Separate
   managerRequests/loadCompletions expose boundaryfrequency. Existing1024/128
   queues/sharedbudget/eligibility/world/device/lifecycle remainbounded.
2. Startupseed originally processedONEgroup/250ms evenifempty. It nowshares32
   totalnewentries across8groupattempts, inclmissinggroups, soft1ms aftercommitted
   progress. Neverdeadlineinsideoffsetrewind (avoidsstarvation). ResponsiveTITLE
   allowsone>=16msQPCslice; duplicateFrame/Timedheartbeatsdonotdoublework. Scene/
   slowmenu/cachetick250ms remain. Whole8192visits/256groups,20msframes/1srecovery,
   IOworkerownership, two-secondrootquietsettlement unchanged. No hard10swait.
   Debugseedbegin/complete andfirstOBSERVEDread1scadence givephaseevidence.

Verification:
* Textureactualnativefixture92controls/Od+/O2,40actualimage/export/rendererRTTI/
  vtable/accounting/purgechecks,16compilednegativecontrols all expectedRED.
  MillionactualalreadyloadedResource.load callsleavecapturestatsunchanged.
  Realmanager.load explicitoutputABI exercisesnativecreateOrRetrieve,preserve
  allargs/floatstack,returnslot/refcounts/unwind; backendpreLOADED isweakonly.
* Startup1255actualproductioninclusioncontrols/Od+/O2. Covers32/64/80 across
  groups,emptygroups8/16/complete,32TOTALsmallgroupentries,8192DISTINCTroots,
  no128offsetrepeatstarvation,deadlineprogress,menu32/64/65at16mswithcachetick
  unchanged,duplicateheartbeat and60060Hzcoarsetickframes. Actualtitlehelper
  positiveandwrongfieldexpectedRED.7compiledseedmutantsallRED.
* Independentreviewreran92/40/1255; noCritical/Important. Cooperative1msbudget
  cannotinterruptnativecallsorrewind; no zero-overhead/FPS promise.
* Full364sourcesDEV/PROD/O2GL/LTCG,183Klib0.5.0/32actualOgre65MyGUI/CRT100only.
  Newbinaryhashes in narrow-texture-startup-build-verification.json.

NoMCM/INI/MO/setting changes. Keep129-key catalogs andowner16GiBRAM/8GiBVRAM/
read128 settings. Testbootmustverifyearlierfirstread andnarrowruntimeadmission/
counts, thenoneNPCroute toinspectremainingstutters. Cachedresidency/purge survival
andgameplaytimingsneedliveevidence. No release/version/Nexus/Steam/ALL_CL change.
