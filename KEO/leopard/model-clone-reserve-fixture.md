# Private offline model-clone fixture, 2026-11-16

This standalone executable loads the installed, pinned Ogre and KenshiLib DLLs
into its own test process. It never launches Kenshi, opens a game process or
collects runtime gameplay data. Its probes observe pointer-vector growth only.

Run `python leopard/verify_model_clone_reserve.py` from the source root with the
bundled Python. The aggregate runs the offline DLL contracts, VC10 `/Od` and
`/O2` fixtures, and 25 compiled production-copy negative controls. It snapshots
production and canonical fixture hashes before and after the four commands.

## What is exercised

The actual production `Clone` and `CreateManual` wrappers are installed directly
on the native Ogre exports using the SDK's actual AddHook implementation. There
is no adapter between native Mesh::clone's call instruction and the production
CreateManual wrapper, preserving the exact `1A3FDD` return-address gate. Additional
test-only hooks observe native unnamed createSubMesh and Pose::clone. The latter
observes the previous append and explicitly completes the last append after the
native clone returns.

Native manager-created Mesh resources contain shared/dedicated vertex buffers,
index buffers, named SubMeshes, materials, bounds, long Pose names, sparse vertex
offsets and normals. Source, OFF clone and ON clone are compared. Caps, independent
lists, partially populated destinations and adequate capacity are covered.
SubMeshNameMap comparisons use logical key/value lookup because native cloning
can change stdext hash-map iteration order while preserving its contents.

The fixture also tests nested eligible/ineligible clone contexts, live settings,
owner/admission/caller/result gates, partial hook failures and no retry, reserve
and logger exceptions, and a genuine native Ogre bounds exception injected at
the manager boundary through the native clone stack and both wrappers. Manager
resources are removed before destroying the default hardware-buffer manager.

The initial Prepare-NOP run failed an ordinary CHECK with exit 1 and clean Ogre
shutdown: both OFF and ON had six SubMesh-vector and thirteen Pose-vector growth
allocations for 8 SubMeshes / 128 Poses. The prepared path has two preparatory
reserves and zero native growth allocations for those same lists. This does not
count native object, map-node, buffer or string allocations and proves no timing,
save-load, FPS or long-running memory improvement.

## Private SDK initialization restriction

The fixture invokes MinHook initialization at SDK RVA `14884`, independently of
the game's RVA-table initialization. Before that private test-only call, it
checks the SDK file SHA256
`5b6240622d06801dc39896e385efb834b8e1fe2fcdf86067d071ae7a04f6c83b`,
the loaded 32-byte prefix and native SDK call `227F -> 14884`. The offline contract
script independently checks these facts. It never invokes InitRVAs. This entry
must remain restricted to the offline fixture and must never be copied into a
production plugin initialization path.

## Negative-control acceptance

The mutant runner changes only private production copies selected through the
`KEO_MODEL_CLONE_SOURCE` compiler definition. It never rewrites the canonical
fixture. Every control must compile, exit exactly 1, print an ordinary CHECK and
complete normal Ogre shutdown without a fixture crash. Compiler failures,
unhandled exceptions, crashes and surviving controls are rejected.

The existing direct Prepare checks precede native clone loops so resize mutants
fail on changed logical sizes before native append consumes them. Test-boundary
catches convert escaped reserve/logger exceptions into explicit failed CHECKs;
they do not change the production exception handling.

Receipts are in `build/model-clone-reserve-final`,
`build/model-clone-reserve-mutants/verification.json`,
`build/model-clone-reserve-native-contract.json` and
`build/model-clone-reserve-fixture-report.json`.
