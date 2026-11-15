# Watched characters must publish their current zone before returning

## Confirmed bug

PollActiveMovers updated currentZoneX/Y only below its baseline/arrival early
return. A baseline character always takes that return, and EnsurePlayerCharsWatched
skips an already-watched entry. Its position therefore stayed indefinitely at the
old cell even while its destination was refreshed from its actual position.
Arrived movers could also be downgraded without publishing the arrival cell.

CollectMoverRetainZones and scheduling consume these scalar fields. With squad
retention enabled they kept renewing leases/stamps for old cells instead of
allowing normal expiry after a character left. This is a concrete retention bug;
it does not establish the sole cause of long-campaign FPS decline.

## Fix and verification

The two current-cell stores now follow the existing live-player/path/grid
validation and precede both early-return branches. No new world/character scan,
native call, queue operation, allocation or cadence is added. Existing retention
radii, movement orders, arrival downgrade and invalid-character removal stay the
same. Baselines still do not enqueue directional preloads.

Fail-first test extracted the actual production EnqueueAheadOfMover,
PollActiveMovers and CollectMoverRetainZones functions and WatchedCharacter
structure. It failed specifically at stale baseline current cells. All 34
production controls pass under /Od and /O2: repeated movement across 20 cells,
arrival, registry on/off, active-mover preloads and lifetime/grid rejections.
Independent review and rerun confirmed the same result. No gameplay capture.
