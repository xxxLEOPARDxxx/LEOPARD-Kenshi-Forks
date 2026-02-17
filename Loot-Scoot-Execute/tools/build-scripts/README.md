# Kenshi Shared Build Scripts

Shared build/deploy/package scripts for Kenshi plugin mods.

## Scope
This repository stores the canonical script set extracted from `Loot-Scoot-Execute/scripts`.

## Consumption (Submodule)
Add this repository as a submodule in each mod repo at `tools/build-scripts`.

## Notes
- Scripts currently expect to run from the consuming mod repository layout.
- Mod-specific values should be supplied through `.env` or script parameters.
