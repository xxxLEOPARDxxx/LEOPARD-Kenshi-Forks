# CPU field temporaries and animation lock reuse, 2026-11-16

Goal: reduce repeated long-key allocation during save-field parsing and redundant recursive lock work during NPC/model state construction. No new game runs/logs/profiles/captures/process-memory access/world scans.

Animation: exact ctor CALL2A357->Notify2BA20; current thread owns parent mutex (ownerDWORD+4=currentTID, recursionDWORD+0 nonzero). Increment unsigned dirtyDWORD+18 under the existing lock, otherwise original once. Keep outer lock, native lifetime and wrap. Install Append before Dirty; authenticate outer-create lock/caller windows excluding the independently patched Append site.

Save: the first key in six field readers is unused when the native permission filter is empty. A stack-free Win64 leaf checks the live byte, bounded count and filter size, then returns the native fresh local unchanged; final key construction remains native. Full helper proofs establish nonnull/disjoint stack arguments for production mode; strict mode additionally checks fresh/alias/overflow boundaries. Long string/file values at 6C9BF8 can transfer an owned mutable parser local after native map insertion via one typed swap; fresh destination, bounded native capacity, disjoint objects and buffer, owner/exact caller/off0/npos are required. One batch publishes all seven CALLs. A retained island after OS rollback failure contains independently safe key leaves; the value bridge remains pass-through until admitted.


- [x] Fail-first boundary and actual Ogre constructor/notification tests, including combined Append/Dirty and contention.
- [x] Authenticate all six field-reader paths and original string ABI; emitted RSI/Win64/allocation/exception tests.
- [x] Implement only proven paths, independent review and negative controls.
- [x] Full host/build/catalog/import/unwind checks; local closed-game installation preserving personal files.
- [x] Cleanup manifest and scoped commit/push; release2 only, unpublished.
