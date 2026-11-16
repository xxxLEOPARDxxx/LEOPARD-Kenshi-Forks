# Native animation-state table insertion, 2026-11-16

Local KEO2 candidate. No game launch, fresh gameplay log/profile/capture/process-memory access or publication. This optimizes one part of model/equipment initialization; it does not parallelize NPC creation or eliminate all load stalls.

## Design and native contract

Pinned Ogre SHA7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6, stamp5CA5F929/image9C9000, known Steam host stamp65D604D7/image232C000, before active world. createAnimationState2AF70 already searches under its native lock and then invokes operator[]2BD80 at2B182, repeating a tree search. Only that5-byte E8 call is redirected through an RX leaf thunk to normal C++ Lookup; return2B187/owner/admission/live flag must match. Native creation, mutex, state-name/scalar copy and dirty notification remain intact.

The actual table has unsigned32 keys, not the String keys declared in the older SDK header. Runtime code uses no stale typed SDK map cast. Map head+8/count+10;48-byte node key+18/mappedptr+20/nil+29. Only currentempty or key>currentmaximum qualifies, otherwise native operator[] once before allocation. Existing native Allocate2DF30 receives the same16-byte pair(key/defaultNULL); native Insert2D750 owns the node and returns actual mappedslot from outNode+20. END is reread from CURRENT table after allocation. No fallback, new throwing callback or logging occurs after allocation. Existing native duplicate/full-search/max-size/rotation cleanup remains.

A rejected design reused the first lookup node across AnimationState construction. Name growth may invoke CRTnew_handler under a recursive lock, so that node could become stale. The shipped path checks the current maximum after construction and retains no node across it. Allocation can itself reenter; reloading END plus native hint validation handles that boundary. Native table concurrency and supported code/layout premises remain.

Ten complete native proof regions include construction/operator[]/allocation/hint insertion/general insertion/cleanup/dirty notify/lock. Unwind CHAININFO fragments are merged:2D750..2D95F is527bytes, not only its first52-byte runtime-function entry. Altered/unsupported startup code falls back. Later arbitrary third-party re-patching is not continuously authenticated. No runtimeworker, map cache, retained world pointer, tree scan or extra allocation is introduced by the helper; one4096-byte code island is kept if published.

## Executable evidence

- Baseline forward-only boundary fixture fails the fast-return check. Production helper24 checks eachOd/O2: empty/currentend, duplicates/nonsorted IDs, unsigned0xffffffff, OFF/nonowner before map access, null API guards, freshEND after reentrant allocator callback, allocator/inserter exception forwarding. Controlled insertion callbacks prove only helper boundaries, not native cleanup. Eight compiled wrong production variants fail ordinary CHECKexit1.
- Actual Ogre DLL public createAnimationState with a real0x68-byte AnimationStateSet ctor2A8E0/dtor2ACA0 and patched2B182 CALL:13 checks eachOd/O2. Enabledappend skips instrumented native operator[]; OFF/wrongreturn/nonowner/unadmitted/nonappend call it once. Correct state lands at its actual native mappedslot, uintmax/tablecount and teardown pass. No game process runs.
- Separate actual Ogre ownership fixture:18 checks eachOd/O2. Reentrant allocator inserts a duplicate through native operator[], then native Allocate/Insert free the unused new node exactly once and return the existing mappedslot. A size-overflow injected AFTER eligibility invokes native free exactly once and throws real std::length_error. Count restored before real Set destruction. Precisely captured node frees are observed by temporary verified CALL wrappers at2CC46/2DB0C, forwarding to actual1A1340; both calls restored before DLL unload. This is not a universal leak proof or real CRTnew_handler fault injection.
-22 pinned-file contracts validate exact native code/layout/call addresses and merged insertion range. Native DLL hashing is separate from runtime fixtures.
- Independent material_copy_design_review approved actual helper/patch and reviewed the native ownership evidence; no remaining Critical/Important finding. Full combined build/host/catalog/import/PE-unwind receipts are private in stage.

New animationStateLookup defaultON/live setting has EN/RU/ZH labels/help and no restart flag. Detailed log remains last/defaultOFF. Unsorted/currentexisting IDs pay a small guard before ordinary lookup; benefit is limited to eligible append creation. No overall gameplay loading/FPS improvement is measured or promised.

## Combined integration result

Final DEV/PROD builds contain379CPP sources. All127 host suites pass; EN/RU/ZH149 keys roundtrip, Russian unsupported dash glyphs excluded. Installed exports satisfy183KenshiLib/37Ogre/65MyGUI imports; all four existing/new C++ bridge functions have finalPE exception metadata. PROD74a4dbb45467a0d5d0d1c62f5b8360bfbf43c44c6317b89dab020c2a7196e7c4 (2106368B); DEV1f37a1279f01b346213e5798ca179f65948a807e74f044a18ea3fd2205ab772f (2213376B).

PROD, README and three MO catalogs installed locally after a fresh name-only no-game check. Nine protected hashes (personalINI/mod-list/sevenforeignSteel files) unchanged. Previous1dca49f DLL/settings/catalogs kept in private stage save-animation-installed-backup. Release remains2, unpublished. No gameplay speed/FPS proof or new capture. Exact receipts: save-animation-binaries.json and save-animation-install.json.
