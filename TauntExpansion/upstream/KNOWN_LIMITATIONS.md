# Known limitations

- Taunt Expansion affects only candidates already considered valid by Kenshi.
- It does not force enemies to select unreachable, invalid, or excluded targets.
- Distance, pathfinding, attack availability, and other vanilla scoring factors still apply.
- Multiple characters using Taunt compete through Kenshi's normal target evaluation.
- Ranged enemies, turret users, animals, and law-enforcement AI have not yet received the same depth of balance testing as ordinary melee enemies.
- `Multiplier = 10.0` and higher are supported by configuration but have not yet been balance-tested.
- The current patch is verified for Kenshi 1.0.65 x64. A game update that changes the target-selection machine code may require a compatibility update; the plugin verifies the instruction before patching and should avoid applying an unverified patch.
