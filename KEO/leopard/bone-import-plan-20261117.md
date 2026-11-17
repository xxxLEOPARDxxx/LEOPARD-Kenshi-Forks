# Ordered bone-assignment import implementation plan

Goal: reduce the profile-confirmed SubMesh::addBoneAssignment tree-search cost
without a new game run, capture, log collection or change to assignment data.
Baseline: 3765581; installed working KEO2/6903e3ca. Preserve seven foreign
Steel edits, personal settings, saves, existing model-weight normalization.

Design: hook the entire admitted native method, not its middle. Unsupported,
disabled, non-owner or shared-vertex calls forward once before any allocation.
For supported ordered calls copy the native key/record, call native node allocation,
then inspect CURRENT tree state. An empty tree or key >= current rightmost
can append, preserving equal-key order. All other keys use the native upper-
bound search with the SAME allocated node. Native link/rebalance owns size-
failure cleanup. Set the native dirty byte only after successful linking.
No plugin allocation, logging or throwing callback may occur between node
allocation and native ownership handoff. No retained pointers or cache.

Reuse the existing live/defaultON meshWeights control for the cohesive model
weight preparation feature; update its EN/RU/ZH explanation. No new menu row.

- [x] Authenticate exact method, allocator/link/cleanup helpers, ABI and layout.
- [x] Write failing native-oracle fixture; implement minimal owned-node path.
- [x] Verify ordered/equal/unsorted inputs, bitwise records and dirty flags,
      real asset lists, aliasing, allocator reentry/failure, max-size cleanup,
      owner/off/shared fallback and native exceptions. Use meaningful negative
      controls and compare complete import timings outside the game.
- [x] Independent review, full DEV/PROD builds, MCM/locales/import/unwind checks.
- [x] Verify personal/foreign hashes, install only if game is already closed,
      commit/push ready changes and update memory. No publication/version bump.

Ruling: reject ordinarily out-of-order keys before allocation. A full native
benchmark found reverse-order regression in the generic C++ search. The cheap
precheck restores its native path; no precheck pointer is retained, and rare
post-allocation reentry still uses generic search on the same owned node.

Performance evidence remains scoped to the saved profile and isolated native
fixtures. A whole-save speedup is not claimed without a new game measurement.
