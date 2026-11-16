# Private equipment constant-field CPU candidate - 2026-11-16

Implemented only src/fixes/world/equipment_field_lookup.h/.cpp. Runtime patch installation, flags/configuration, localization and hooks remain the parent's separate scope. No game/process/log access, DLL build/install, Git changes or publication were performed by this work.

## Actual contract

The pinned game SHA is 504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e. The three exact int/slot E8 callsites are535BAC (setupItem),537C96 and537EDE (buildAttachments), all targeting thunk480FE->6CEF0. Literal construction at535B7D/537C69/537EBB passes exactly `slot`. Other keys/callsites are excluded. A two-argument native bridge must call the five-argument Slot helper; Slot itself is not a replacement entrypoint for a two-argument native call.

The native return is pair<const std::string,int>*, not int*. Hit path6CF6C returns node+10, callers access pair+28. Native64-bit string is40 bytes and SDK typed Ogre unordered map is64 bytes. Actual native bucket_count+18, size+20, buckets+38, node hash+8, node value+10 match the typed Boost1.60 fixture. SDK GeneralAllocPolicy is the same CategorisedAllocPolicy<MEMCATEGORY_GENERAL> alias used by ogre_unordered_map.

Native hash6A6B0 starts seed0 at6A6D2, combines signed characters at6A6E0..6A704 using0x9e3779b9, then applies mix64 from6A706. Exact SDK compiled hash of `slot` is0x28253c9a37862 and stored mixed node hash0xaf7187ef7256aa87. These match native instruction semantics. Public find(key,FixedHash,std::equal_to<string>) applies the same mix64 policy and returns the actual existing pair; no private Boost nodes are accessed by runtime code.

## Behavior and limits

Only enabled, owner-thread, nonnull exact four-byte `slot` queries use the precomputed raw hash. A real hit returns &*iterator. A miss, different field, disabled feature or non-owner thread calls the original exactly once, preserving default insertion, allocation/rehash and native exceptions. No exception is swallowed. No metadata, string, iterator or map pointer is retained. No added runtime allocations, worker, queue, global lookup cache or per-frame sweep.

The parent must establish exact module/profile/callsite identity and main-thread/feature gate before routing native calls here. The helper does not independently authenticate an arbitrary foreign map layout. No GOG optimization is proven by these Steam image descriptors; unsupported profiles must retain stock execution.

## Verification

Forward-only baseline failed the runtime hit-avoids-original assertion (RED). Actual helper218 controls under both /Od and /O2 pass, including mutable fields, heap-backed key, missing defaults, collisions, rehash, pair-address identity, actual non-owner thread fallback, allocation-free hit and original exception propagation. Four compiled wrong-hash/pair-address/thread/fallback mutants fail ordinary CHECK controls. Run leopard/equipment_field_run.py; run leopard/equipment_field_native.py for the pinned offline image checks. Test Ogre allocation functions use malloc/free stubs; no game DLL is loaded or called.

Five latest hot-map offline trials,2,000,000 queries per path/trial: optimized SDK stock7.510..8.088ns; fast4.685..5.283ns; fast with volatile enable plus actual GetCurrentThreadId ownership guard5.442..6.013ns, guarded/stock0.714..0.792. This demonstrates a modest per-query CPU saving even against an aggressively optimized SDK baseline. It does not measure the real native6CEF0 function, whole equipment assembly, gameplay FPS, or the41ms spikes. A few nanoseconds per lookup alone cannot explain/fix those spikes.

The earlier bind-pose-copy candidate was rejected: clone re-normalizes orientation, copies current local transforms rather than binding inverses, and native binding also updates derived/initial state. Evidence is in the private stage build/equipment-binding-review-20261116. No unsafe copy shortcut was shipped.
