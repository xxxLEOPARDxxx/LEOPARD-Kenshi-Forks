# Better Looting regression checks

These checks use the real source functions or headers with narrow engine boundaries. Run with Python and the configured VC10 / Windows SDK7.1 development stand at D:/DEV/Kenshi. Generated harnesses go into `_generated/`.

Run test_rules.py, test_storage_guard.py, test_ground.py, test_config.py, test_scratch.py, then check_invariants.py. The invariant check compares preserved transfer/deposit paths against Git commit15bdb01. Generate the three gettext MO catalogs before checking translations.

No test changes the installed player INI, loads a save or proves native pickup behavior. A real game smoke test is required for ground orders.

## Ground route regression (30 October 2026, still release v4)

`test_ground.py` extracts the actual production discovery/filter/dispatch functions and checks the order of calls at the `Character::addOrder` boundary. Fixtures cover cheap loot at the character's feet versus expensive distant loot with BestItemsFirst enabled, nearest-neighbor continuation with BestItemsFirst disabled, deterministic equal-distance ties, NaN/infinite coordinates, duplicate query hits, the 256-result / 32-order budget, existing orders and the unchanged shift=false / clear=false flags. A reversed submission order also fails the first fixture.

Before the fix the new nearby-versus-expensive fixture failed at `actor.orders[1]==&route[0]&&actor.orders[2]==&route[1]`: ground candidates used value-per-weight before distance. The old single-origin distance sort also did not build a route from each preceding target. The corrected implementation chooses the nearest eligible candidate at each step and leaves inventory-loot value sorting untouched.

The boundary double records submission order only. It does **not** prove that Kenshi executes orders FIFO or LIFO. Steam table resolution found Character::addOrder RVA 0x5D1640 and OrdersReceiver::addOrder RVA 0x5078F0, but no live game remained when the read-only disassembly was attempted; the protected disk image does not match those live entry points. No reversal was inferred. The existing native addOrder call is preserved, and an actual walk through the route remains an in-game verification step. Debug-only `GROUND_ROUTE` lines record each submitted position for that comparison.
