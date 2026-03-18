## Organize the Crafting Stations

This repository is the working RE_Kenshi plugin repo for a crafting-window recipe-search mod.

## Current status

On 2026-03-18 this repo was reseeded from the current `Organize-the-Trader` working tree so the crafting mod could reuse the proven search-input behavior, UI flow, and diagnostics while replacing the trader-specific adapter layer.

The authoritative implementation plan is:
- [docs/plans/main_plan.md](/mnt/i/Kenshi_modding/mods/Organize-the-Crafting-Stations/docs/plans/main_plan.md)

The authoritative developer role is:
- [docs/roles/dev.md](/mnt/i/Kenshi_modding/mods/Organize-the-Crafting-Stations/docs/roles/dev.md)

## Cleanup policy for this repo

- Keep reusable search-input pieces.
- Remove stale trader-only docs and planning residue.
- Do not trust copied trader placement or binding assumptions for crafting.
- Defer broad file/project renames until the crafting adapter starts landing, so cleanup stays small and reversible.

## Planned implementation shape

- Reuse: search input behavior, text editing, shortcuts, clear button, counts, persistence, config style, and logging style.
- Replace: crafting window detection, clamp parent discovery, recipe row discovery, recipe-name extraction, filter apply/reset, and selection handling.
- Development rule: probe first, then place, then filter.

## Build note

The codebase still contains legacy `Organize-the-Trader` filenames and build metadata because the repo is in the middle of a reuse-first transition. Treat those names as migration residue, not as the intended final shipping identity for this mod.
