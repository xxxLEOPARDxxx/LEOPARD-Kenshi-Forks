# RAM resource cache - local test candidate, 2026-11-14

Not a published release. Existing pathfinding diagnostic controls are unchanged.

## Player controls

MCM / KEO / Resource memory: opt-in RAM resource-file cache; cap 1-32 GiB in 1 GiB steps (default 1); background read ceiling 1-128 MiB/s (default 32). All three settings apply live. Detailed log remains last and is off in release defaults. Russian/English/Chinese catalogs updated.

This first stage reuses successfully requested DDS/mesh/skeleton files. It does not predict dependencies of unvisited zones and does not allocate VRAM, activate extra NPCs, change simulation or call graphics APIs on a worker. Reuse removes file reading/allocation work from subsequent archive requests; decoding and GPU uploads still belong to the engine. Whether it measurably reduces zone stutters is pending gameplay A/B.

## Ownership and limits

One low-priority IO worker; 128 queued paths, 4096 cached files, maximum 256 MiB per file, 256 KiB cancellable chunks. In-use stream bytes remain charged even after eviction or disable. Cache readers have independent Ogre stream positions and shared read-only byte ownership. Main-thread archive lookup does not wait for the cache lock; a miss/failure uses the original archive. No entity pointers or world scans.

RAM reserve: stop filling and trim unused files when available physical memory drops below max(2 GiB, 20% total), also check commit availability. Not a guarantee against other allocations. File identity checks volume/file ID, size and last-write timestamp. Deliberate in-place asset edits preserving timestamp during runtime require toggling the cache off/on; editing installed assets during gameplay is not a supported hot-reload workflow.

Shutdown: signal and CancelSynchronousIo, wait at most 250 ms. The existing pinned KEO module lets a misbehaving storage filter drain later without stale code pointers. One wake event deliberately lives to process exit so a late callback cannot signal a recycled handle.

## Verification

- RED: store test initially failed to compile because cache API did not exist.
- Actual production store tests: byte identity/reuse, changed size, same-size/timestamp atomic replacement, pressure trim with injectable memory-status input, active-reader budget, oversize refusal, off during 8 MiB slow read.
- Actual local Ogre DLL tests: normal native archive open and explicit hidden-return bridge produce identical data; retained CachedStream has independent cursors, read-only mode, safe lifetime after disable and final release to zero payload.
- MCM adapter tests DEV/PROD plus negative controls; requested-settings/MCM DEV/MCM PROD suites passed. New three rows live; default off; RAM 1-32; debug last.
- DEV/PROD native build: 354 sources; x64/startPlugin; 180 KenshiLib imports accepted against SDK 0.5.0. MO catalogs roundtrip 92 keys each.
- Independent review: four important findings fixed (bounded shutdown, pressure trim, control-block exception RAII, file identity); second pass no Critical/Important. Minor stale-lookup eviction race fixed by comparing FilePtr before erase.

No gameplay FPS, time-to-zone or long-run memory improvement claim yet. No Nexus/Steam publication and no personal published changelog entry.
