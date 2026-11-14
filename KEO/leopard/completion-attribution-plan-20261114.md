# Private: complete resource-finish attribution

Goal: split the remaining measured main-thread finish spikes without changing
accepted RAM/geometry caching, spawning, native ordering or readiness.
Scope: diagnostics only until a measured costly child is known. Do not present
this build as another performance fix. No release/number/public changelog.

Evidence/spec: runtime-geometry-consumer-accepted/analysis.json and acceptance.md;
npc-finalization-seams-20261114.md; current Steam/Ogre build byte guards.

1. Confirm actual Steam getTexture2D 44ACD0 (RCX=this/RDX=output, RAX=output),
   array448BC0 (already hooked by bridge), appearance body538590 and attachment
   536590. By-value ResourcePtr is indirect RDX with native callee ownership;
   no extra C++ SharedPtr copy/destructor in a raw forwarding wrapper.
2. Expand production-including fixture first: original once/args/results,
   off-main/debug-off no clocks, completed-only stats, C++/SEH propagation,
   VC10 indirect16-byte owned-value lifetime, bounded slow-update transcript.
3. Add passive timing wrappers for 2D/body/attachment, and time existing array
   bridge wrapper without a conflicting second native hook. Fixed POD counters;
   public Begin/End(-1=disabled) API does not construct/retain native objects.
   Existing resource update captures child-time deltas only if native returns;
   retain last16 slow updates>=10ms until10sec summary. Window maxima alongside
   cumulative maxima avoid attributing startup max to a later route.
4. Run Od/O2 native fixtures, negative control, full DEV/PROD link/import guard,
   independent ABI/perf/lifetime review. Preserve seven foreign Steel changes.
5. Save exact sources/commit/push. Check actual process before installation,
   keep accepted662bda62 backup and exact personal INI/MO/mods list. One route
   after startup is the only next gameplay request. Do not repeat accepted test.
6. Inspect new attribution before any completion budgeting/worker change. Native
   batch20 and incomplete isLoading-ready-list coverage remain untouched.

Acceptance: same native effects/results/exceptions, no hidden resource ownership,
bounded storage/logging and no clocks when Debug is disabled. Not acceptance
of improved FPS; runtime evidence must demonstrate a specific expensive child.
