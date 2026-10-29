# Better Looting regression checks

These checks use the real source functions or headers with narrow engine boundaries. Run with Python and the configured VC10 / Windows SDK7.1 development stand at D:/DEV/Kenshi. Generated harnesses go into `_generated/`.

Run test_rules.py, test_storage_guard.py, test_ground.py, test_config.py, test_scratch.py, then check_invariants.py. The invariant check compares preserved transfer/deposit paths against Git commit15bdb01. Generate the three gettext MO catalogs before checking translations.

No test changes the installed player INI, loads a save or proves native pickup behavior. A real game smoke test is required for ground orders.
