# Full KenshiQOL-style automatic navmesh repair - KEO4

User correction: implement BOTH native building/furniture repair and full zone
navmesh generation, including interiors. The earlier exterior-only scope below
is superseded. User explicitly authorized performance testing on17 November.

1. Verify native repair receiver/player+0x298, Steam/GOG code/layout and reuse
   the installed full-generate detour so force/cache/UI handling stays native.
2. Test the real runtime wrapper with zero repair changes, receiver and call order,
   idle/loading/world gates, cooldown, live switches and no locks at native calls.
3. Keep bounded once-per-world discovery and existing workers. Native synchronous
   building repair is part of requested parity; measure rather than omit it.
4. Benchmark scheduling cost separately from native repair/generation on a real
   loaded scene; do not label a fake-boundary benchmark as game performance.
5. Name the feature after the author's public wording, Pathing repairs itself,
   with EN/RU/ZH labels and accurate tooltips. Prepare KEO4 through canonical
   release toolchain, CRC/content/locales/licenses/privacy/docs review. Preserve
   personal preferences; Nexus first, no Steam publication before user confirms.

## Historical exterior-only plan (superseded)

# Optional automatic exterior navmesh recovery

User approved implementation. No new game runs, captures or profiling.

1. Add a host-tested fixed per-world admission bitmap and rotating grid cursor.
   Inspect at most 64 cells every 250 ms, accept at most one request per 5 s.
   Preserve admission history on live off/on; clear on actual world reset.
2. On the existing main-thread safe point, require idle loader, no transition,
   no adoption, stable physics, supported queue bindings and navmesh caching.
   Resolve each cell afresh. Require positive finite native camera/player lease,
   loaded accessible content, terrain and actual native active-set membership.
   Submit one exterior cell through NmForceRebuildCells, with no GUI call.
   Bitmap means accepted request, not proven path repair. Failed admission stays
   eligible on later passes. Do not retain native pointers across ticks.
3. Add a live default-OFF MCM option, INI reference, EN/RU/ZH catalogs and README.
   Keep native global furniture repair out: it is an unbudgeted active-world
   traversal with building/floor mutations, not a bounded cell operation.
4. Verify policy boundaries, reset, time pacing and nonfinite leases, production
   integration contracts, config suites, full DEV/PROD build and translations.
   Independent review, fix findings, commit/push. Install only with game closed;
   preserve personal settings. No publication or gameplay performance claims.
