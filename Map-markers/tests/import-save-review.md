# Map-markers: import/save lifecycle candidate, 2026-11-03

## Actual storage and confirmed loss

The existing plugin stores player settings in mod-config.json and markers separately, in <active-save>/Map-markers.json (schema 3, with a legacy single-marker reader). SaveManager::import(SaveInfo,int) previously only cleared a pending Save As hint and forwarded the request. Later map identity polling cleared the marker vector when the new save directory lacked the sidecar. There was no selected-source import handoff. This is not evidence that personal configuration is being reset.

The local mod is not enabled in data/mods.cfg and its DLL is staged under D:/DEV/Kenshi/stage/Map-markers, not installed in mods or Workshop here. The user's screenshot is a report from another player, not local reproduction evidence.

## Native read-only evidence

ReadProcessMemory only, while the already running game was available. Saved bounded listings: D:/DEV/Kenshi/research/map-markers-import-20261103/import-native.asm and execute-native.asm.

* SaveManager::import, actual Steam RVA 0x47AA80: when signal is nonzero, returns without replacing it. Otherwise queues signal=3, flags and copies SaveInfo.name (+0x28) and location (+0xB8) into manager fields. This is asynchronous request admission, not import completion.
* execute at 0x47BAA0 waits until SFS state allows work, then calls actual importGame with its copied location/name/flags. After that call at 0x47BE5D, result 2 branches to retry, result 0 to success; other results take an error path.
* importGame at 0x3788F0 checks saveExists; missing source returns 1. Native physics-not-ready path at 0x378A35 returns 2. Its successful tail calls SFS::analyse at 0x379DD8 and returns zero at 0x379E18. It calls SFS::writeFile for imported output files.
* SFS::newGame listing obtained at RVA 0x470D00 clears current save/folder/index state. The subsequent 2026-11-04 read-only audit resolved the direct importGame -> GameWorld::resetGame -> SFS::newGame chain; see the native conclusions below.

No source hardcodes these RVAs. New hooks resolve named SDK members through KenshiLib::GetRealAddress, retaining the existing supported-platform check. New private import hook failure reports an error without disabling the working load/save hooks. Current source compiles with VC10 /GL and links /LTCG; 25 KenshiLib imports resolve against 0.5.0.

## Candidate behavior

The old 15-second SaveManager destination-guessing mechanism is removed. Use actual SFS load/new/save hooks; marker writes register through writeFile and reads resolve through readFile, with existing active-directory legacy fallback. SFS save serializes current markers before native copy admission, so first save and Save As include the registered file. Ordinary changes no longer write directly into a previously loaded slot.

Actual importGame captures only the selected location/name sidecar before native execution, into a local owned vector. It restores that vector only after result 0. There is no cross-call pending import state. Native registered destination sidecars win over the staged source, including preservation of invalid destination bytes. Missing/invalid selected data never falls back to another loaded world's markers. Native new/load hooks invalidate old marker state. Source references are not accessed after the original returns.

No new per-frame filesystem work, retained owner pointer, background worker, queue or global cache is added. The existing map identity check remains throttled; new I/O is on explicit save/load/import/edit events. The temporary vector's size follows the existing marker file parser (there is no new independently unbounded retained collection). No FPS/long-run leak claim follows from these host tests.

## Focused production proof

Runner: Map-markers/tests/test_save_import.py. It extracts actual production wrappers/helpers and actual pure JSON/path helpers and links actual MapMarkersPersistence.cpp. Native entry points are owned seams operating only on generated private fixture files.

Before implementation, RED: import selected A retains A bookmarks while active world was B. Final 88 checks each /Od and /O2. Selected A/active B, no active world, success/failure/retry, exceptions before/after reset, nested new/load during original, source argument mutation, existing/invalid destination, missing/empty source, first save, Save As/rename, failed save, new-game isolation, and legacy schema are covered. Three compiled mutants fail for wrong active-save source, accepting failed imports, and bypassing SFS file registration. Exact logs stay in ignored tests/_generated; fixtures are about 6 MB total, not copies of the game/repo.

Native six-TU build: D:/DEV/Kenshi/research/map-markers-import-20261103/native-build.log; candidate D:/DEV/Kenshi/build/Map-markers/Map-markers.dll, 238592 bytes. No game/UI/INI/save mutation or installation/publication.

## Native contract conclusions, verified 2026-11-04

The three outstanding native facts are now resolved for the supported Steam 1.0.65 execution path. `tests/audit_native_import.py` reads an already running process with ReadProcessMemory only. It does not launch/control the game, suspend threads, invoke native functions or write memory. Frozen native evidence PID 9388, bounded read: executable .text 23,528,690 bytes (32 MiB cap), .rdata at most 16 MiB, unwind metadata at most 2 MiB, and selected method listings. No large raw image is saved. The audit retains SDK labels BEFORE following installed detours; the earlier listing lost those labels by following all the way into plugin trampolines.

1. **Imported-world registry reset.** Actual importGame calls GameWorld::resetGame at 0x3789FC. That reset obtains the canonical SFS receiver and calls SFS::newGame at 0x36CB11. The latter clears the old file index, currentSave/currentFolder and paths. Old active world B's registered sidecar cannot legitimately take destination precedence in successful import A. Native imported output uses SFS::writeFile and finally analyse. The source vector is owned before reset and applied only on the actual result 0.
2. **No native nested save during import.** Across the complete executable .text, the non-virtual SFS::saveGame has one direct native caller, SaveManager::saveGame at 0x376505. That manager method has one direct native caller, execute at 0x47C11B (signal=1). ImportGame is execute's separate signal=3 branch at 0x47BE5D. SaveManager::save checks signal at 0x47B0F1/0x47B0F8 and rejects an occupied manager, so an import cannot queue a nested normal/autosave request while owning signal=3. No LEA/literal64 address escapes for either save method were found in executable code, and no corresponding pointer escape was found in .rdata. This is a native controlled-path conclusion, not a promise about arbitrary third-party code calling private methods.
3. **Serialized UI/render caller and singleton receiver.** execute's two native call sites are 0x7880B1 in GameWorld::mainLoop_GPUSensitiveStuff, immediately beside PlayerInterface::updateUT at 0x788047 and after the physics-queue gate, and 0x82AC79 in a MainListener vtable callback. Actual MSVC RTTI confirms MainListener derives from Ogre::FrameListener, WindowEventListener, RenderTargetListener and OIS MouseListener/KeyListener; the same callback calls the GPU-sensitive loop and KingOfRenderThread::mainloop. These are the normal UI/render paths, not the SFS copy worker. Getter, manager save/load/import and world reset load the same canonical SFS storage RVA 0x212DC08. Native SFS save starts its copy worker only inside the original, after the plugin's caller-side registration.

The audit asserts the reset chain, sole direct save/import callers, MainListener RTTI and identical singleton storage. Its exact method hashes, aliases, direct callers, unwind-table ownership and selected code are in tracked `tests/native-evidence/native-contract.json` and adjacent compact .asm files. Final native report SHA256: `efa9564dfb3cd3867042a5716ad8d99909f0eb050274e1c455edb10c3a12826b`. Audit source SHA256: `0d6cbc809eb469949fb871c709f8fa5473d66b1103164fff76fa62fa882b03d0`.

Production source and linked DLL did not change during this verification. The source uses SDK method lookup for the existing supported platforms/versions; no Steam-only RVA was added to production. GOG execution was not performed here. Final independent review is CLEAR for the source/native contracts and host lifecycle tests. An actual UI import of a copied save remains the next gameplay acceptance check; no game installation, settings change, publication was performed during verification. A v5 package is prepared separately; it is not published.
