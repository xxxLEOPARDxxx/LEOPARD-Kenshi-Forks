## Organize the Crafting Stations

RE_Kenshi plugin that adds a reusable search bar to Kenshi crafting and research windows.

## Current behavior

- Auto-attaches a search input to detected crafting and research windows.
- Filters visible recipe and blueprint rows by substring match.
- Compacts visible matches upward so filtering does not leave empty scroll gaps.
- Shows visible and total entry counts.
- Supports a clear button, configurable autofocus, configurable width and height, and persisted custom position.
- Applies Mod Hub setting changes live.
- Coexists with `Organize-the-Trader` without taking over trader windows.

## Configuration

Settings live in [Organize-the-Crafting-Stations/mod-config.json](/mnt/i/Kenshi_modding/mods/Organize-the-Crafting-Stations/Organize-the-Crafting-Stations/mod-config.json).
The same enabled, count, clear button, autofocus, and size settings are exposed in Mod Hub when `Emkejs-Mod-Core` is loaded.

Shipped defaults:
- `enabled`: `true`
- `showSearchEntryCount`: `true`
- `showSearchClearButton`: `true`
- `autoFocusSearchInput`: `true`
- `searchInputWidth`: `220`
- `searchInputHeight`: `26`

The authoritative implementation plan is:
- [docs/plans/main_plan.md](/mnt/i/Kenshi_modding/mods/Organize-the-Crafting-Stations/docs/plans/main_plan.md)

The authoritative developer role is:
- [docs/roles/dev.md](/mnt/i/Kenshi_modding/mods/Organize-the-Crafting-Stations/docs/roles/dev.md)

## Cleanup policy for this repo

- Keep reusable search-input pieces.
- Remove stale trader-only docs and planning residue.
- Do not trust copied trader placement or binding assumptions for crafting.
- Keep external build and deploy identity aligned to `Organize-the-Crafting-Stations`.

## License
This project is licensed under the GNU General Public License v3.0.
It uses KenshiLib, which is released under GPLv3.
