# Taunt Expansion v1.0 RC1

Taunt Expansion is an RE_Kenshi plugin that strengthens Kenshi's existing Taunt function without replacing target selection.

## What it changes

Kenshi adds a confirmed fixed score bonus of `+2.0` when a valid target candidate has Taunt enabled. Taunt Expansion multiplies only that Taunt-specific bonus.

It also supports a separate optional multiplier when an enemy already has a valid target, allowing a taunting tank to pull enemies away from other squad members more reliably.

The plugin does **not** directly alter:

- target validity;
- pathfinding;
- distance evaluation;
- attack slots;
- combat animations;
- damage, dodge, or guard calculations;
- Kenshi's target-retention logic itself.

## Default design

```ini
[Taunt]
Multiplier = 5.0
RetargetMultiplier = 2.0
```

With the confirmed vanilla bonus of `+2.0`:

- new target evaluation receives up to `+10.0` from Taunt;
- evaluation with a valid old target receives up to `+20.0` from Taunt.

These values are intended to support a clear tank role while preserving Kenshi's position, reachability, and candidate-selection rules.

## Requirements

- Kenshi 1.0.65 x64 (tested Steam build)
- RE_Kenshi 0.3.4
- KenshiLib 0.4.0

## Build

1. Ensure the KenshiLib dependency environment variables are configured.
2. Open `TauntExpansion.sln`.
3. Select `Release | x64`.
4. Rebuild the solution.
5. The DLL is copied to `ModPackage\TauntExpansion\TauntExpansion.dll`.

## Installation

Copy the contents of `ModPackage\TauntExpansion` to:

```text
Kenshi\mods\TauntExpansion\
```

Expected files:

```text
TauntExpansion.dll
TauntExpansion.ini
TauntExpansion.mod
RE_Kenshi.json
```

Enable `Taunt Expansion` in Kenshi's mod launcher.

## Compatibility approach

The patch redirects only the verified Taunt-specific floating-point addition instruction. The shared vanilla constant remains unchanged. This minimizes interference with unrelated AI scoring.
