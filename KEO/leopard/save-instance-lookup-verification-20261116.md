# Saved-object tree lookup reuse, 2026-11-16

Local KEO2 candidate, no version bump/publication/ZIP or published changelog. No new game run, log/profile capture or process-memory access.

## Native contract

Steam EXE504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e. AddSaved6CAB10 already finds lower_bound6CAB42→515C3→FE310 and stores rawnode atRSP+70. Its call6CAB7D→21643→100630 repeats the same descent at100667. Only that5-byte CALL is retargeted. Lookup verifies exactreturn10066C, outer6CAB82, parser6C21C7, originalkey/map saved by nativepushes, and nonnull/alignednode. Off-thread returns native before mutable config/admission reads. Source result is used only in the active native call; no pointer survives it.

ReturnSlot+160 is operator[] callerreturn, +1D0 parserreturn, +148 savedkey(R12), +150 savedmap(RDI), +1D8 firstrawlowerbound. Importantly this is not the alternate selected RBP/sentinel metadata hint. Between first search and reuse there are only localbranches and pure native stringcompare691A0→MSVCR100.memcmp. No allocation, defaultconstructor, applicationcallback or mapmutation intervenes. Ten complete native/caller/thunk/comparison proof regions1266B and actualmemcmpIAT authenticate this assumption. Unsupported/changed startup chains keep native execution. Arbitrary later rewriting of these functions by other plugins is not reauthenticated at runtime. Native map's no-concurrent-mutation premise remains.

All downstream native comparison, default ObjectInstance construction, node allocation, hinted insertion, metadata writes, cleanup and exceptions remain byte-for-byte active. No typed native map cast, alternate allocator, hash/cache, world scan, runtimeworker or memory budget added. One4096-byte RX leaf thunk is retained if possibly published. Existing synchronized rollback-safe code_patch_batch performs full-call revalidation/protection/cache cleanup. Install occurs before SaveStateListTransfer so that latter's full AddSaved proof still matches. Existing detailed log defaultOFF/last remains; new saveInstanceLookup is defaultON/live, no restart.

## Verification

Faithful VC10 map + actual three MASM native-layout frames fail baseline at2 descents vs expected1. Production wrapper/callsite then pass2102 checks eachOd/O2: empty/new/existing/end/before/between/long identifiers, unchanged stored addresses/values,2048 insertions, different callers/config/thread/provenance, null/misaligned frame candidates, nested construction, defaultconstructor failure, nativefallback C++ and SEH through real unwind frames, balanced value lifetimes. Fixture comparison count2688→1408 is a synthetic tree sample, not an in-game measurement; it does not reproduce untouched outer6CAB59 compare or execute the game's ObjectInstance constructor/hint inserter.

Actual production Admit is executed against a private populated image with controlled metadata stubs; every changed proof region, image version/size, world/thread and IAT refusal is tested. This does not activate native game hooks.27 offline pinned-file contracts pass.16 separately compiled production mutations fail ordinary CHECKexit1; crashes/compile failures are not accepted.

Both full377-source VC10 /O2 /GL /LTCG builds passed,127 host suites pass. EN/RU/ZH each147 MO keys; no unsupported Russian dash glyphs. All183KenshiLib/37Ogre/65MyGUI imports resolve and final PE exception metadata for Lookup is present. Independent morph_upload_design_review found no remaining production blocker after correcting thread-first mutable-state gating. Private stage save-instance-binaries.json has exact hashes/unwind locations.

This proves reduced CPU tree-search work and preserves the inspected native chain; no gameplay loading-duration, FPS or long-run leak absence claim. NPC equipment assembly remains separately in progress.

## Combined integration result

Final DEV/PROD builds contain379CPP sources. All127 host suites pass; EN/RU/ZH149 keys roundtrip, Russian unsupported dash glyphs excluded. Installed exports satisfy183KenshiLib/37Ogre/65MyGUI imports; all four existing/new C++ bridge functions have finalPE exception metadata. PROD74a4dbb45467a0d5d0d1c62f5b8360bfbf43c44c6317b89dab020c2a7196e7c4 (2106368B); DEV1f37a1279f01b346213e5798ca179f65948a807e74f044a18ea3fd2205ab772f (2213376B).

PROD, README and three MO catalogs installed locally after a fresh name-only no-game check. Nine protected hashes (personalINI/mod-list/sevenforeignSteel files) unchanged. Previous1dca49f DLL/settings/catalogs kept in private stage save-animation-installed-backup. Release remains2, unpublished. No gameplay speed/FPS proof or new capture. Exact receipts: save-animation-binaries.json and save-animation-install.json.
