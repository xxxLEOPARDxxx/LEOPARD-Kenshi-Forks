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
