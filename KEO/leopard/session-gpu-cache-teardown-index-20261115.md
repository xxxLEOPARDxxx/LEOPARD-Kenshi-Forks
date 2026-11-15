# GPU parameter-cache map retirement index (2026-11-15)

Private stage-only follow-up to `1718df4d9ce682e405f74753ee5a51ba73989f30`.
No game run, capture, profiler, collector, diagnostic DLL, installed-file change,
settings change, release-number change or publication. This corrects proven
recurring CPU work; it does not identify the cause of campaign FPS decline.

## Source-proven cost and lifetime

`gpu_params.cpp` invalidates named-constant cache entries before an admitted
native map's final deleting destructor. It also conservatively invalidates on
same-map rebinding and unsupported control-block replacement. Each owner-thread
`GpuParamCache::DropMap` previously examined all4096 cache slots, including an
empty cache and maps never observed by the cache. NPC/material teardown can
therefore repeatedly scan the whole fixed table with no relevant entries.
These are native map retirements/rebindings, not a newly added world scan.

The new fixed1024-bucket index links the existing occupied4096 slots with encoded
16-bit next/previous indices. Exact map identity still decides removal. Cache
lookup,16-probe insertion/home-slot replacement, string validation, hook
admission, native forwarding, map revision and GPU upload invalidation retain
their earlier behavior. A global occupancy count bypasses empty-cache work;
an empty map bucket also touches no cache slots. Worst-case collisions remain
bounded by4096 occupied slots. No map, key or definition pointer is dereferenced
during index upkeep or retirement.

Store unlinks an overwritten slot in fixed work using its predecessor and
successor, so the optimization does not move a bucket walk into the insertion
hot path. A preliminary singly linked implementation was rejected by an added
fail-first Store work-bound fixture before freezing this result. Same-map
rewrites keep their existing single index membership. Clear resets slots,
heads, both links and occupancy together. The unusual historical return count
of DropMap(NULL) is preserved; production stores use admitted non-NULL maps.

The additional per-cache storage is18440 bytes including final x64 alignment
(1024 heads,4096 next indices,4096 previous indices and one count). The singleton
still has no heap allocation, lock, new native reference or per-frame world
scan. Only owner-thread Store/DropMap/Clear touch this index. Existing worker
deletions still publish the flush flag; owner Clear removes all indexed entries
before a later cache lookup. No AI, movement or physics work is disabled.

## Verification

`python leopard/test_gpu_cache_drop_index.py --mutants` compiles the actual full
production implementation with only its slot/link arrays wrapped to count
accesses. It compares exact slot contents, Find and DropMap results against the
previous16-probe algorithm. A separate index walk checks occupied-only
membership, uniqueness, bucket identity, both link directions, empty-link
cleanup and occupancy. VC10 /Od and /O2 each pass more than2.36 million controls;
counts can vary slightly with actual name addresses and hash distribution.

Coverage includes10000 inserted keys beyond table saturation,50000 mixed
operations,128 deliberate same-bucket maps, rewrites, collisions, head/middle/
tail removals, Clear,30 map/name-address reuse cycles, and constant Store link
work. Nine compiled runtime-negative controls reject the whole-table scan,
lost count updates, deleting a colliding neighbor, missing overwrite unlink,
stale Clear heads, missing successor reverse links in both unlink and deletion,
and duplicate rewrite insertion. The first run against the old source failed
at the intended empty-cache bound:4096 slot touches. The compiled scan mutant
reproduces that failure without changing production files.

With2000 keys across300 maps,1000 never-cached map deletions examine at most19
cache slots per call in the fixture, versus4096 before. This is an offline
operation-count result, not a game timing or FPS measurement. The deliberately
colliding bucket case separately checks exact removals and bounded chains.

The existing actual-native Ogre regressions link the unwrapped production
cache and exercise native/SDK map ownership, same-map rebinds, last-reference
destruction, worker-flush handoff and upload-plan/shadow invalidation:

- `python leopard/test_gpu_map_lifetime.py`:36 controls in /Od and /O2,
  seven compiled mutants expected runtime RED.
- `python leopard/test_gpu_params_rebind.py`:38 controls in /Od and /O2,
  six compiled mutants expected runtime RED; pinned native-image checks pass.
- Host `render_units`:PASS after final production changes, including original
  deterministic5000/2000-key cache replacement/validation expectations.

The native executables are standalone fixtures using the already pinned local
Ogre library; they do not start Kenshi or create a renderer/GPU device. Outputs
are in `build/gpu-cache-drop-index`, `build/gpu-map-lifetime`,
`build/gpu-params-rebind` and `build/tests/render_units.log`.

## Boundaries retained

Resource retention heartbeat/queue bounds and native mesh/texture mutation
checks remain unchanged. Movement roster validation remains unchanged. The
previously documented particle two-publication reader race was not replaced
with an unproven concurrency design. No new unbounded render/resource list was
established in this review; this patch removes a concrete teardown scan rather
than claiming all long-session performance problems or leaks are fixed.

Owned files: `src/render/gpu_param_cache.h`, `src/render/gpu_param_cache.cpp`,
`leopard/test_gpu_cache_drop_index.cpp`, `leopard/test_gpu_cache_drop_index.py`,
this report, and the compact stage hash receipt. Seven foreign Steel files,
source checkouts, player settings and installed binaries are untouched.
