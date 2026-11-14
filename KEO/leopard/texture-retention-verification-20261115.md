# Private KEO loaded texture retention verification, 2026-11-15

Change: optional loaded DDS TexturePtr retention to complement existing Mesh
retention and terrain preparation. New defaultOFF live MCM key
resourceTextureRetention. All share one1..8GiB ceiling with DXGI/RAM pressure
admission. Native scheduling, threads, ready flags, factory/RNG untouched.

Captured runtime92d3b6f5: startup firstreportedcomplete64.665s,7379processed,
7295loaded/84skipped/capped0; RAM15386059768bytes,1657hits; geometry838models,
354966284logicalGPUbytes,5911referencehits, nofinalpressure/evictions. Five new
appearance seams installed; morphLookup guard refused unknown runtime code and
was left native. Singleposeblendmax927us, animationbodymax167us. Late239.813s
update42104us includes41007usattachment, zero morph/pose;235.946s genericcallback
91756us remains unclassified. Added existingfactoryProcess scopedchilddelta
to distinguish construction from appearance in future records; no extra hook.
Do not treat cumulative/window maxima as a wholeNPC/frame or sum nested times.

Native audit attachment-stutter-research proves synchronous TextureManager.load
through Material/SubEntity assignment outside TextureLoadData.getTexture timer.
Destroy44A4C0 uses referencecountgate before textureunload/remove. CurrentMesh
retention does not hold those TexturePtrs. This establishes an optimization
hypothesis for revisits, not evidence that every41ms was texture upload or that
firstencounter/save parsing/AI/object construction become faster.

Implementation: resource_texture_retention module/report describe exact ABI,
eligibility, ownership, weak queue128, entries1024, work4promote+2refresh/250ms,
pressure32 and conservativeactualmipaccounting. No filesystem/archive search,
no unknown resource load, no nativepointer retained on worker, no percallheap.
Explicit ownerreload immediately drops oldpin/account and weakrequeues;
ordinary alreadyloadedhits remaincheap. Worker calls reject beforeownerstate.
OFF/world/device and beforeOgre shutdown release own references. Unusual
nonmanual DDS-named generated external resources are not distinguishable by
metadata; explicit changes outside observedownerload use boundedrefresh.

Three-wayreservation: workerReserve+publish count both nativepin stores;
owneradmissions count eachother+terrainallocated/reserved. Requestedceilings
stay separate from effectiveadmission; Configure propagates common cap toidle
pins. Raising1->8GiB withbothstoresnonempty andterrain-onlygrowth is covered.
Decreases/releases remain allowed duringRAM/DXGI/devicepressure. Logicalcache
size is not WDDM residency. Cache cap cannot restrict textures owned by thegame.

Verification:
* Actual installedOgre nativefixture77controls each/Od and/O2;26actual
  image/export/accounting/vtable/purge contracts;11compiled brokenproduction
  controls all expectedRED. Includes native refcount/purge,weakpending,OFF,
  worlds/devices/pressure,identityorphan,manual/dynamic/RTT/FSAA,ownerPAGE_NOACCESS,
  explicitlargerreload/newcharge/overcaprefusal andsharedptrlifetime.
* ActualWARP sharedstore checksOd/O2 (andregressionafterreview) cover all3caches,
  capdecrease/increase/CPUDXGIpressure/release/device.2compiled productionbudget
  mutants rejected; earlierfullasyncterrainGPU/decoder/readbackfixture passes.
* HUD actualformat/layout+nativewidgetlifecycle,8compilednegativecontrols and
  actualvanilla/DarkUIresource contracts pass. Three text-only rows, no frame.
* MCMactualstagedcallback PROD/DEV plusexistingnegativecontrols pass; new36rows,
  all3previous/newcachecontrols shown, texturedefaultOFF/live, Debuglast.
* Probe111actualwrappercontrols/Od+/O2, nowfactoryProcessUs in16slowsamples.
* ThreeMO129keys/gettextroundtrip/sourcecompiledRUASCIIhyphens pass. Newlabel,
  tooltip,sharedcapdescription andHUDtextures counter localized EN/RU/ZH.
* Full364sourceDEV/PROD/O2GL/LTCG build,183Klib0.5.0/32actualOgre/65MyGUI/CRT100.
  Binaryhashes in texture-retention-build-verification.json.
* Independentreview found3Important beforeinstall:capincrease,stalereloadcharge,
  workerownerstate read. Each reproducedRED, fixed, independentlyrechecked.
  NoCritical/Important remains. No liveFPS/leak/gameplaybenefit claim yet.

First runtime test: menuwarmdone, save,oneNPCareaandreturn, pauseafterstutter.
Check textureAdmitted/count/heldbytes/reused plusactualslowchildren. Existing
terrain/model settings preserved; localtestadds onlynewtruekey. No publicrelease,
versionbump, Nexus/Steam upload orALL_CHANGELOG entry.
