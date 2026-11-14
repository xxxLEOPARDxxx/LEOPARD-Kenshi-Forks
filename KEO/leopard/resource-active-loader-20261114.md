# Correct active terrain consumer and measure native load work (private)

2026-11-14, local test only; no published version change.

## Route evidence and diagnosed error

User traversal tech scribes / Flat Lagoon / back: RAM1363files,2830312070bytes,1517hits/2518misses, no RAMrejections. GPUprepared2arrays,50328576bytes, but adopted/skipUploads/descRefused/mismatch/verifyUs allzero. Therefore the prior GPUcandidate did not accelerate this run. User reports strongly reduced revisiting stutters with RAM, but no visible extra VRAMbenefit; no controlledOFF/ON timing claim.

The prior candidate's hook was the legacy ZoneMap::createTextureArray atA10080. Current RE_Kenshi EXE504b362c...3e has no active references to that function beyond its linker thunk. The material path atA13DC0 usesA0C420 to build names/files, ResourceLoader::loadTextureUnitArray(448DA0) to queue, and TextureArrayLoadData::getTexture(448BC0, KenshiLibslot6023) to actually finish. Previous WARPfixture proved texture reuse under the wrong enclosing native boundary, not that gameplay reached it.

Second error: A0C420 calls UtilityT::removePathFromString. Deferred request filenames are basenames, unlike producer metadata full paths. Matching now uses exact native terrain name and ordered lowercase basenames. Full producer paths remain unchanged for IO/identity; descriptor and every byte of each upload remain mandatory before skipping native writes. Equal basenames or group overrides with different contents cannot bypass those checks. Native member hidden return is RCX=request,RDX=TexturePtr output. SDK fields name8/group30/filesNamesA0 are compiled offset-asserted, copied on main under bounded SEH; no game-owned strings/vectors sent to worker. Full request name supports SID255.

Native arrays are not immortal: ResourceLoader::updateMT unloads/removes manual textures after about10 unused game-delta seconds. Do not explain zero adoption by assumed permanent residency. Producer nativeExists/accepted/declined and consumer calls/off/nonMain/requestRefused/readyHit/readyMiss counters now distinguish resident, unprepared and unused-boundary cases.

## Native NPC/resource investigation

Images already have native background read/decode and ready queues. Mesh resource preparation uses Ogre background loading; entity initialization, binding and some uploads complete on main. RootObjectFactory::mainThreadUpdate drains at mostone queued creation request percall; direct creation bypasses it. Moving live world/physics/AI creation to arbitrary threads or GPU has no verified contract.

Two optional, guarded timing wrappers preserve originals exactly once: factoryDrain582880 and resourceFinish44BBA0. Cached DetailedLogEnabled gates QPC; disabled and off-main calls simply forward. No entity scans or worker/game-pointer changes. Cumulative completed inclusive elapsed/calls/max/over2ms/over20ms are reported only every10seconds with debug. Inclusive elapsed is not CPU threadtime or total NPCspawning. Native failures propagate unchanged, incomplete calls are not reported as completed. Timers observe initial save loading too; no spawn or population policy changes.

## Fresh verification

- Name/key regression initiallyRED(fullpath vs native basename), nowPASS. Long native name regression initiallyRED, nowPASS. Reader45nativeSDKchecksPASS.
- Actual ResourceLoader WARPfixture now uses real SDK request fields, production CopyArrayFiles/CopyArrayJob and fullpath producer vs uppercase basename consumer. Same precreated COMtexture is adopted; changed bytes fall back with correct GPUreadback. No stubbed key or substituted job reader.
- Actual diagnostic wrappers17controls inOd andO2, including C++ and Win32exceptions and disabled/off-main paths, PASS.
- DEV/PROD360sources nativeGL/LTCGbuildPASS,182KenshiLibimports compatible0.5.0,CRT100only. New64byte consumer fingerprint matches currentREEXE448BC0 independently.
- Independent review: noCritical/Important; longnameedge corrected and final helper/test delta reviewed.

Pending gameplay: consumerCalls>0, readyHit/adopted/skipUploads, descriptor/byte fallbacks and native loading timing. None of these counters have been measured in game with this correction yet. RAM16GiB/VRAM8GiB/read128MiB/s are user's current settings; preserve them. No release ZIP, publication or personal changelog entry.
