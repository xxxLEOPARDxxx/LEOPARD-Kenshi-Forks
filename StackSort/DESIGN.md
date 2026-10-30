# StackSort MCM fork, unreleased v1

Preserve Jakematt2004's StackSort first-click consolidation, five-solution
cycling, H/W free-space goal, arrow controls, right-click layout undo and
background preparation when opening an inventory. Keep its C++03 solver and
Win32 worker implementation in the same VC10 ABI as the native adapter.

Investigate native `addQuantity` semantics before changing consolidation.
Native `canStackWith` is the authority for compatible stacks; never equate
items solely by template. Resolve live section membership before dereferencing
snapshot pointers, stop on ambiguous native failure, and invalidate results
after any consolidation. Never reuse a consumed source pointer.

Use a common eligibility guard for UI, snapshots and application: enabled,
unequipped, a meaningful grid with more than one usable cell and no one-item
limit. Preserve the game's original UI for single-slot resource containers.

Keep worker inputs plain solver values. Native objects are read on the game
thread only. Preserve generation checks/abort on close and add explicit world
reset/load cancellation before native world teardown, without dereferencing
cached GUI pointers. Bound cache and tracking as upstream does.

Use the Rotate Items C API for optional orientation changes. Snapshot actual
per-item dimensions and preserve existing rotations without the API. Never
write template GameData dimensions. MCM settings use shared localization in
ru_RU/en_GB/zh_CN, with debug last and disabled by default; errors always log.

Verification: compile isolated StackSortCheck with the existing build stand;
exercise the real policy/consolidation helper against native fault models,
solver rotation, invalid/stale generations and cancellation. Game acceptance
still covers mixed-quality/stolen/charged items, restricted storage, undo,
close/reopen, load/new game and Rotate Items visuals/save persistence.

The upstream fixes already present are retained rather than replaced by a
second sorter. Config tuning stays advanced; MCM exposes player settings.
