# Private KEO path ownership repairs, 05 Nov 2026

The user authorized continued KEO fixes and autonomous testing. Dynamic Factions
and the satellite map remain paused. These are private test changes, not a public
release, version increment, or entry in ALL_CHANGELOG_MODS.txt.

## Changes and evidence

* A MOVE selection larger than the formation/planner capture cap of 30 could
  leave an excess selected actor attached to an older formation and route.
  Native receives the entire selection; KEO previously updated only the first
  30 owners. Now the existing selection walk retires excess actors' old planner
  and formation ownership and refreshes their click destination. The first-30
  planning/merge behavior and all capacities remain unchanged. Custom route
  input still returns before this bookkeeping. Native receives unchanged
  arguments exactly once. This does not add formation planning for actors over
  the cap.
* getZoneEdge retained a plan snapshot across native getClosestPoint. A cancel,
  replacement, or leg change during that call could publish retired steering.
  Immediately after the native call, validate mode, TLS owner, slot key, epoch
  and expected current leg before writing either snapped or fallback point.
  Otherwise retain the previously computed native pointer/output. The hook's
  own successful advance remains valid. There are no native retries or locks.

The first defect was RED with the actual extracted hook plus actual formation
eligibility/detach and store helpers: selection 30 passes, selection 31 leaves
the excess actor eligible for the previous formation's send. The repaired
production extraction passes 345 controls in both /Od and /O2, including all
Shift/add combinations, grouping off, full click table, unaffected unselected
owners and consumed custom-route input. Removing only overflow cleanup returns
RED in both builds.

The second defect was RED for cancel and replace inside the owned native snap
boundary. The actual repaired hook linked with unmodified PlanStore/PlanPolicy
passes 186 controls per /Od and /O2, including snap success/failure, slot reuse,
leg change and own advance. --old-guard-control returns RED in both builds.
This check does not make publication atomic with cancellation after the final
read; native game-thread overlap and gameplay impact require runtime evidence.

Existing job cancellation (142), TLS unwind (51) and paused-clock (18) controls
pass in each /Od and /O2; their negative controls reject the previous failure
patterns. Both changes received independent read-only review.

DEV and PROD build all 351 translation units, reusing 349 validated cached
objects and recompiling the two changed units. PROD SHA256:
f8074f907b3527d4d1848ae68ffd6824d9b9ad8d2d7f0be2944c2748316d65b6.

## Performance and outstanding runtime work

No per-frame scan, heap allocation, retained pointer, thread or cache was added.
Overflow work runs only during the existing explicit MOVE selection walk and
uses bounded planner/formation/click stores. Post-snap checks are constant-time
reads. This is code/lifecycle review, not measured FPS or a proof of no leaks.

The previous real-game log shows NPC native A* STATE-FULL (status3/cause3), not
proof of unreachable paths: 57 new capped searches consumed 5.4785 of 5.6271
path-thread busy seconds over 297..327s. NPC budget was not expanded and each
KEO NPC branch called native once. Repeated face keys do not identify one actor;
43.3 is Havok-scaled distance, not 43 meters. No retry suppression or geometry
rewrite is justified by that log alone. Existing DEV NpcCapWho can identify
requesters in a future 30..45s unpaused capture; DEV overhead must not be used
as a PROD FPS comparison.

Window automation failed before initialization with kernel-assets OS error3,
including after a kernel reset. No UI input or game launch occurred during this
audit. Runtime route quality, production-job delivery, FPS/memory and GOG tester
acceptance remain pending. The prior accepted two-point save/load route remains
historical acceptance of df07c25, not acceptance of this candidate.
