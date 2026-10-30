# StackSort

Inventory auto-sort mod for Kenshi that optimizes for maximum contiguous free space.

Replaces the vanilla sort button with a smarter packer. Click "StackSort [N]" to cycle through the top results — the mod pre-reserves an N-cell-tall (or N-cell-wide) empty strip so incoming items of a known size always have room. Switch between **H**eight-mode and **W**idth-mode with the H/W toggle button to reserve space for whichever orientation fits your use case. Results are precomputed on a background thread for instant response.

![StackSort demo](stacksort.gif)

## Features

- MAXRECTS/Skyline-based 2D bin packing, far superior to vanilla's greedy 3-pass placer
- Player-controlled target size: click StackSort to cycle, back button to decrement
- **Dual-mode targeting**: H-mode tries to reserve a stack `N` high; W-mode reserves a strip on the right `N` wide. Toggle with the H/W button.
- Precomputed results on a worker thread — zero main-thread stutter
- Guaranteed empty rectangle via pre-reservation (not emergent)
- Automatic stack consolidation on first click (merges split stacks)
- Works on character backpacks, body inventories, and containers
- Right-click the sort button to revert to original item positions
- Multiple simultaneous inventories supported (up to 16)
- Results cached across inventory close/reopen for the same character
- Remembers the target you last used, per character and per container, for the rest of the play session
- Configurable through `StackSort.cfg` — logging, worker threads, button geometry, search budget
- UI available in ten languages, following Kenshi's own language setting
- Trader inventories excluded (vanilla sort preserved)
- [KenshiRotate](https://github.com/jacobericson/KenshiRotate) integration available behind the `STACKSORT_ENABLE_ROTATION` compile flag (disabled in the initial release)

## Requirements

- [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) ([Nexus](https://www.nexusmods.com/kenshi/mods/847)) — includes KenshiLib
- Optional: [KenshiRotate](https://github.com/jacobericson/KenshiRotate) for rotation-aware sorting

## Installation

1. Download the latest release
2. Copy the `StackSort` folder into Kenshi's `mods\` directory
3. Launch Kenshi via the RE_Kenshi launcher

## Usage

Open any inventory. The vanilla "Arrange" button is replaced with four controls:

```
[ < ]  [ StackSort [N] ]  [ H/W ]  [ > ]
```

- **StackSort [N]** — Left click to see the top 5 scoring inventory arrangements in descending order. Right click to revert your inventory to its initial layout.
- **H/W** — toggles between Height-mode and Width-mode. The button caption shows the active dim. Toggling invalidates results and precomputes under the new dim; the first click after toggling uses a quick sync fallback while the worker catches up.
- **<** — back button, decrements N with symmetric skip logic.
- **>** — forward button, increments N with symmetric skip logic.

Target N=1 is the general-purpose sort — it packs items tightly and lets the largest empty rectangle emerge naturally. N=2+ pre-reserves a strip of that size along the active axis, useful when you know what size items you'll be picking up (e.g., W-mode with N=7 before buying katanas you plan to stack flat, or H-mode with N=4 if you want a tall column to stuff your wheat).

Right-click the StackSort button to revert items to the layout they had before the first sort.

- Revert is one-shot: it consumes the saved layout, so a second right-click does nothing until you sort again.
- The saved layout is dropped when you add or remove items or rearrange by hand, so right-click then does nothing.
- Items added since the sort go into free cells around the restored ones. If they cannot all be placed, the vanilla Arrange runs instead so nothing leaves the inventory.
- Revert needs the inventory add/remove hooks. If they failed to resolve, revert is disabled (see Known Limitations).

**Remembered targets.** When you close an inventory, StackSort remembers the `N` you
left it on and restores it the next time you open that same inventory. The memory is
per character and per container; a character's body and backpack strips are remembered
separately, and H-mode and W-mode are remembered separately too. Right-clicking to
revert the layout also forgets the target for that inventory. The H/W mode itself is
not per inventory — it is one global setting shared by all of them, whose starting
value is the `default_dim` config key. None of this is written to disk: quitting the
game clears every remembered target, and the next launch starts fresh at `N=0`.

KenshiRotate support is compiled out of the initial release to narrow confounding variables during troubleshooting. Rebuilding with `/DSTACKSORT_ENABLE_ROTATION` enables the full integration (both orientations evaluated per item, visuals refreshed after sort, rotation penalized to avoid visual clutter).

## Configuration

`StackSort.cfg` lives next to `StackSort.dll` in `mods\StackSort\`. It is a flat
`key=value` file — one setting per line, no sections, `#` starts a comment and also
comments out the rest of a line it appears on. The copy that ships documents every
key at its default value, so an untouched file changes nothing.

The file is read once, at startup. Editing it takes effect on the next launch.

| Key | Default | Range | Meaning |
|---|---|---|---|
| `log_level` | `1` | `0`..`2` | 0 = errors only, 1 = normal, 2 = verbose (adds per-job packer lines). |
| `default_dim` | `H` | `H`, `W`, `height`, `width` | Dimension the sort targets when the game starts. |
| `consolidate_stacks` | `true` | `true`, `false` | Merge stackable items into full stacks before the first sort. |
| `num_workers` | `4` | `1`..`8` | Background packing threads. |
| `mutation_debounce_ms` | `150` | `0`..`5000` | Quiet period after an inventory changes before it is re-packed. |
| `resnapshots_per_frame` | `3` | `1`..`16` | Inventories re-snapshotted per frame after a burst of changes. |
| `top_targets` | `5` | `1`..`10` | Entries in the sort button's click cycle. |
| `arrow_button_width` | `30` | `10`..`200` | Width of the `<` and `>` buttons, in pixels. |
| `dim_button_width` | `40` | `10`..`200` | Width of the H/W button, in pixels. |
| `sort_button_width` | `180` | `40`..`600` | Width of the StackSort button, in pixels. Raise it if a translated caption is clipped. |
| `button_gap` | `2` | `0`..`50` | Gap between the four buttons, in pixels. |
| `body_x_offset` | `-40` | `-500`..`500` | Horizontal shift of the button strip on body inventories, in pixels. |
| `first_pass_restarts` | `-1` | `1`..`256`, or `-1` | Restarts in the first search pass. |
| `first_pass_iters` | `-1` | `100`..`1000000`, or `-1` | LAHC iterations per first-pass restart. |
| `refine_restarts` | `-1` | `1`..`256`, or `-1` | Restarts in the refinement pass. |
| `refine_iters` | `-1` | `100`..`1000000`, or `-1` | LAHC iterations per refinement restart. |
| `refine_lahc_history` | `-1` | `1`..`100000`, or `-1` | Length of refinement's late-acceptance history. |
| `refine_plateau` | `-1` | `1`..`1000000`, or `-1` | Iterations without improvement before a refinement restart gives up. |
| `refine_item_threshold` | `-1` | `1`..`1000`, or `-1` | Item count above which refinement always runs. |
| `sync_restarts` | `4` | `1`..`8` | Restarts for the main-thread fallback pack, used only when no precomputed result is ready. |
| `sync_iters` | `1000` | `100`..`2000` | LAHC iterations for that fallback. The fallback blocks the game thread, so the two ranges cap its cost at about 4x the default (tens of ms at defaults). |

The seven search-budget keys take `-1` as a sentinel meaning "leave the packer's
compiled default alone", and they ship commented out for that reason. Those compiled
defaults are the settings every published quality measurement in this repository used.
Uncomment them only to experiment: larger values cost CPU, smaller ones cost layout
quality.

`num_workers` is not purely a speed dial. The range of target values is split into one
chunk per worker, and each chunk warm-starts its own search chain, so changing the
worker count can change which layouts come out — all valid, but not identical. The
value actually in force is logged at startup as `[StackSort] 4 worker threads started`,
so a bug report that includes the log stays reproducible.

### When a setting is wrong

Unknown keys, values that will not parse and values outside the listed range are
skipped; the rest of the file is still applied. Every problem gets one error line in
`RE_Kenshi_log.txt`, which appears even at `log_level=0`:

```
[StackSort] cfg: StackSort.cfg:6: unknown key 'bogus'
[StackSort] cfg: StackSort.cfg:7: button_gap=999 out of range [0..50], using 2
```

Settings that were accepted and differ from the default are logged too, one line each
(`[StackSort] cfg: log_level=2`), so the log shows exactly what the run used.

On top of that, the first time you open an inventory in a session with a bad config,
the game shows a single on-screen message:

```
StackSort: 2 problem(s) in StackSort.cfg were ignored (see RE_Kenshi log)
```

It appears once per session, not once per inventory.

## Translation

UI text lives in `mods\StackSort\lang\<code>.txt`, one file per language, in the same
flat `key=value` format as the config file (whitespace around keys and values is
trimmed; `#` only starts a comment at the beginning of a line, so values may contain
`#`; an empty value falls back to English). StackSort reads Kenshi's own `language=`
setting and loads the matching file at startup — there is no language key in
`StackSort.cfg`.

Ten locales ship: `en`, `ja`, `zh_CN`, `zh_TW`, `ko`, `ru`, `de`, `fr`, `es`, `pt`.

The nine non-English files are machine-drafted and marked at the top of each file as
awaiting native review. Corrections are very welcome — see below.

Some strings contain a `{n}` placeholder that the mod substitutes at runtime (the
target number in the button caption, the problem count in the config warning). Keep the
token exactly as written; a translation that drops it loses the number.

Missing text falls back to the built-in English, per key rather than per file: if the
file for the detected language is absent, or one line inside it is missing or
malformed, only that string reverts to English. The startup log says which case
applied:

```
[StackSort] Language: de (lang\de.txt: 7/7 keys)
[StackSort] Language: de (lang\de.txt not found, using built-in English)
[StackSort] Language: en (built-in)
```

`lang\en.txt` is a template for translators, not a runtime input. English always comes
from strings compiled into the DLL, so editing `en.txt` changes nothing in game; it
exists so a translator has the canonical key list to copy.

Files must be UTF-8; a byte-order mark is tolerated, and UTF-16 is rejected with an
error in the log. If a translated caption is too long for its button, raise
`sort_button_width` in `StackSort.cfg` rather than shortening the text.

**Contributing a locale.** To fix one of the nine drafts, edit
`lang\<code>.txt` and open a pull request or attach the file to an issue — no code
change is involved. A language that does not ship yet also needs its code added to the
mod's language table, so open an issue with the file and it will be wired up.

## Building from Source

Requires:
- Visual Studio 2010 v100 x64 toolset
- RE_Kenshi / KenshiLib headers and libraries
- Boost 1.60 headers
- [Git LFS](https://git-lfs.com/) — the demo gif is stored via LFS, so a clone without it leaves a text pointer in place of the image

Link dependencies: `KenshiLib.lib`, `MyGUIEngine_x64.lib`

Run `build.bat` from the repository root. It is the canonical build: it produces
both `build\StackSort\` (reduced logging) and `build\StackSort_dev\` (verbose
logging), and it is the only build the project is tested against. `StackSort.vcxproj`
and `StackSort.sln` are a maintained mirror for IDE editing and IntelliSense — they
list the same sources and use the same flags, but `build.bat` is what release
binaries come from.

### Benchmark harness builds

The standalone harness at `tests/harness/build.bat` produces four binary variants controlled by env vars:

| Env | Output | Purpose |
|---|---|---|
| (none) | `stacksort_bench.exe` | Production build — use for bitmatch/quality runs. |
| `STACKSORT_PROFILE=1` | `stacksort_bench_profile.exe` | Coarse per-phase rdtsc counters (MoveGen / SkylinePack / LER / etc.) emitted as `fp_cycles_*` CSV columns. Use for apples-to-apples perf comparisons. |
| `STACKSORT_PROFILE_SUBPHASE=1` | `stacksort_bench_subphase.exe` | Adds per-candidate sub-phase counters (`skyline_waste_map`, `skyline_candidate`, `skyline_adjacency`, `skyline_commit`, `ler_histogram`, `ler_stack`). Adds ~30% overhead to SkylinePack — use for bottleneck drill-down, not for cycle comparisons. |
| `STACKSORT_VALIDATE_GROUPING=1` | `stacksort_bench_validate.exe` | Cross-checks every incremental grouping score against the O(n^2) reference scorer and aborts on a mismatch. A correctness build, not a perf build. |

The four objdirs (`obj`, `obj_profile`, `obj_subphase`, `obj_validate`) are separate so switching variants doesn't force a rebuild.

Harness runs take a config from `tests/configs/`. `prod_defaults.ini` mirrors the
shipping build and is what the current perf and quality numbers are quoted against;
`baseline.ini` is kept frozen as the historical reference and no longer describes the
shipping configuration. `tests/bitmatch.py --strict` is the parity gate: it compares the
scoring columns *and* the search-trajectory columns, so a change that alters the search
path fails even when the final scores are unchanged.

Two profile-guided optimization pipelines sit alongside them, each driven by
`PGO_PHASE={INSTRUMENT,OPTIMIZE}`:

| Script | Output | Purpose |
|---|---|---|
| `tests/harness/pgo_train.bat` | `stacksort_bench_pgo.exe` (and `stacksort_bench_pgo_profile.exe` with `STACKSORT_PROFILE=1`) | Instruments the harness exe, trains it on the corpus in parallel shards, merges the counters, and relinks optimized. |
| `tests/harness/pgo_dll_train.bat` | `StackSort_pgo.dll` | Same pipeline targeted at the plugin DLL. MSVC 2010's `/LTCG:PGOPTIMIZE` rejects `/DLL` if it wasn't in the instrumented link, so the DLL has to be instrumented directly rather than reusing an exe-trained profile. |

Both pipelines train on `tests/configs/prod_defaults.ini`, and `pgo_train.bat` trains on
rotation-disabled runs by default because that is what the release ships.
`build_pgo_dll.bat` with `PGO_HOSTABLE=1` emits `StackSort_pgo_hostable.dll` — the same
optimized code with the game imports delay-loaded, so it can be exercised outside Kenshi.

## How It Works

The plugin hooks five game functions via KenshiLib:

1. **`InventoryGUI::autoArrangeButton`** — Intercepts the sort button click. Routes to StackSort's H-cycling logic or falls through to vanilla for trader GUIs.
2. **`InventoryGUI::_NV_show`** — Detects inventory close. Aborts running worker jobs and cleans up UI state.
3. **`InventoryGUI::_NV_update`** — Detects inventory open (visible transition), snapshots items into the result cache, enqueues worker jobs, polls for completion, and initializes the StackSort UI.
4. **`Inventory::_sectionAddItemCallback`** — Detects item additions. Invalidates cached results and arms a debounced re-snapshot.
5. **`Inventory::_sectionRemoveItemCallback`** — Same as above for item removals.

### Packing Algorithm

The packer runs a 6-layer pipeline, then a query-time selection over its cached results:

1. **Greedy packer** (MAXRECTS or Skyline) places items in a single pass
2. **LAHC search** (Late Acceptance Hill Climbing) explores item orderings to find better layouts
3. **Diversified moves** (swap, insert, rotate-flip, repair) perturb the ordering
4. **Multi-restart** (16 restarts first pass, 24 refinement) escapes local optima
5. **Path relinking** walks transposition paths between the elite orderings collected during the restarts, intensifying the search in the space between good solutions rather than around a single one
6. **Post-packing grouping moves** rearrange items to improve clustering without changing which cells are occupied — `StripShift` permutes items within an aligned strip, `TileSwap` swaps a single item with a rectangular group of smaller items elsewhere on the grid, and `OptimizeGrouping` exchanges same-footprint items across types

At click time, **best-across-target** selects the highest-scoring cached result whose LER side covers the requested target — so later clicks are served from a single lookup rather than rerunning the pipeline.

For target>1, a pre-reservation scan carves out a strip at the grid edge (bottom in H-mode, right in W-mode) and packs items into the L-shaped complement. The empty strip is guaranteed by construction. Scoring uses a unified function with strict tier separation: items placed >> LER area >> target-met bonus >> fragmentation >> grouping.

W-mode is implemented via transposition: the inner packer only knows how to reserve at the bottom, so W-mode swaps the grid and item dimensions on the way in and swaps them back on the way out. All the scoring, LER, skyline, and search code stays dim-agnostic.

### Performance

All packing computation runs on `num_workers` background worker threads (4 by default). Opening an inventory triggers precomputation of every target value; clicking the sort button reads the cached result with zero main-thread work. If the worker hasn't finished yet (first click within ~1 second of opening), a quick synchronous fallback runs on the main thread, costing tens of milliseconds at the default settings.

If the inventory add/remove hooks cannot be resolved (degraded mode), nothing can tell the mod when items change, so precomputed results and revert are disabled. Every click then runs the reduced-budget synchronous pack on the main thread, with soft grouping turned off. The startup log line `Mutation hooks missing` confirms this mode.

On a fully loaded 20x20 backpack (43-45 items, the largest inventories in the game), the worker sweep completes in **1.3-1.6 seconds** for the first pass and **2-3 seconds** including refinement, measured under real game load. Smaller inventories (8x10 body sections, 10x14 backpacks) finish proportionally faster.

Across a test corpus of 17 real Kenshi inventory configurations (200 seeds, both with and without KenshiRotate), the packer achieves 100% item placement, median free-space concentration of 1.0 (all empty space in one contiguous region), and zero stranded cells (no isolated interior waste).

### Tuning

Search parameters were tuned over 10 rounds using a standalone benchmark harness (`tests/harness/`) with automated ablation analysis. Each round evaluated multiple configurations across a 17-instance corpus at 100 seeds per configuration, with per-component score decomposition (LER, grouping, stranded cells, concentration) and bimodal basin-rate analysis for diversity-sensitive signals. The corpus covers sparse to 95%-full inventories across body, backpack, and container grid sizes.

#### First pass (16 restarts x 4000 iters)

| Parameter | Value | Alternatives tested | Result |
|---|---|---|---|
| LAHC history | 200 | 100, 500, 1000 | 200 is the sweet spot; 100 gains grouping but loses -4.6pp basin rate from weakened refinement; 500+ wastes budget on lateral moves |
| Plateau threshold | 1500 | 500, 750, 1000, 2000 | Monotonic: higher = better basin rate. 1500 gets +4.3pp over 1000 at 1.5x wall time; 2000 adds only +0.9pp more |
| Restarts | 16 | 2, 4, 8 | +17.9pp basin rate over 4 restarts; ~96ms first-pass cost hidden by worker thread |
| Grouping power | b^1.5 | b^1.25, b^1.75, b^2 | Local quality optimum; tested across 3 separate rounds |
| Repair move | enabled | disabled | Live feature: ~1.4k attempts/run, 33% LAHC acceptance rate |
| Pre-reservation | enabled | disabled | Mandatory for target>1; disabling drops quality to 0% on dense inventories |

#### Refinement (24 restarts x 8000 iters, conditional)

Refinement runs a second LAHC pass warm-started from the first-pass best ordering. It triggers conditionally to avoid wasting budget on inventories that are already well-packed.

| Parameter | Value | Alternatives tested | Result |
|---|---|---|---|
| LAHC history | 200 | 100, 400, 800, 1000 | Monotonic: shorter = better. 200 won on basin rate (+2.4pp over 1000), replace rate, score lift, and grouping lift |
| Restarts | 24 | 8, 20, 32, 64 | Monotonic but diminishing: 24 gets +6.4pp basin rate over 8; 64 adds only +1.8pp more past 32. 24 wins +1.5pp over 20 on bimodals for +20% refine wall, still amortized on the worker thread |
| Plateau threshold | 2000 | 1000, 1500 | Higher plateau gives refinement more budget per restart to escape local optima on warm-started seeds |

#### Refinement trigger heuristic

Correlation analysis on `refine_always` data (every target refined regardless of quality) identified `num_items` as the strongest predictor of refinement value (r=0.67).

| Trigger condition | Purpose |
|---|---|
| `!allPlaced` | Always refine if items couldn't be placed |
| `perpendicularLerSide < gridExtent` | LER doesn't span full width (when structurally possible) |
| `concentration < 0.95` | Free space isn't well consolidated |
| `strandedCells > 0` | Any isolated interior waste cells |
| `numItems > 20` | High item count — first pass is search-starved |

The item-count gate was tuned at thresholds 20, 25, and 30. Threshold 20 matches baseline basin rate (-0.4pp, noise) while cutting trigger rate from 54% to 42% and saving ~100ms wall time. Threshold 30 loses -5.3pp basin rate from skipping 25-30 item inventories that genuinely benefit.

#### Intensification and post-pack stages

These run once per pack, after the restart loop has finished, and are bounded by
hard caps rather than by search budget.

| Stage | Bound | Notes |
|---|---|---|
| Path relinking | Elite pool of 8 orderings; path length up to the item count | Walks transposition paths between the elite orderings captured at each global-best improvement. Enabled by default, no diversity filter. |
| `StripShift` | Strips of at most 5 items | Enumerates all `n!` orderings within a contiguous same-column or same-row strip and keeps the best-grouped one. |
| `TileSwap` | One item against a group of 2 or more | Swaps an item with the rectangular union of several smaller ones. Rotated cases are re-tiled by a corner-first, most-constrained-cell backtracking tiler, which beats a DLX exact-cover solver at these piece counts. |
| `OptimizeGrouping` | Same-footprint pairs only | Exchanges item identities between equally sized placements, so it cannot change the layout at all. Runs last, over the geometry the two moves above produced. |

Path relinking re-packs and so can change the layout; the three post-pack moves
cannot — each preserves the set of occupied cells exactly, so the largest-empty-
rectangle, concentration and stranded-cell scores they inherit from the search are
invariant and only grouping can move.

### Compatibility

The plugin hooks game functions at runtime and doesn't modify any game data, save files, or item templates. Compatible with saves made before installation and with other mods. The initial release ships without KenshiRotate integration compiled in — builds with `/DSTACKSORT_ENABLE_ROTATION` detect KenshiRotate at startup and use it opportunistically. The example gif is with rotation disabled.

### Removing the mod

Uninstall by removing the `StackSort` folder from Kenshi's `mods\` directory. No persistent state is stored outside the mod folder.

## Known Limitations

- **Degraded mode without the add/remove hooks.** If the game build does not expose them, StackSort still sorts but with no precompute, no revert and no grouping, and every click blocks the main thread briefly. Check the log for `Mutation hooks missing`.
- **Revert is one-shot and best-effort.** It is skipped once the saved layout no longer applies, and falls back to the vanilla Arrange when items added since the sort cannot all be placed.
- **Trader inventories are not sorted.** The sort button falls through to vanilla behavior in trade GUIs.
- **Rotation-aware sorting is compiled out of release builds.** Items are packed in their natural orientation unless you rebuild with `/DSTACKSORT_ENABLE_ROTATION`.
- **Remembered targets last only for the session.** A character or container reopens at the target you last left it on, but quitting the game clears every one of them.
- **H/W is a single global setting.** Toggling it changes the mode for every inventory, not just the one you have open. Its starting value comes from `default_dim` in `StackSort.cfg`.
- **`StackSort.cfg` is read once at startup.** Editing it while the game is running has no effect until the next launch.
- **The nine non-English translations are machine-drafted** and have not been reviewed by native speakers yet.
- **Very large inventories are left alone.** The packer accepts at most 255 items and at most 126 cells on a side; anything past that is returned unsorted rather than partially packed. No vanilla Kenshi inventory comes close to either limit.

## Bug Reports

If you run into a problem, [open an issue](../../issues/new) and include:

- **Other mods installed** — list of all mods you have enabled
- **RE_Kenshi version** — shown in the launcher or in `RE_Kenshi_log.txt`
- **RE_Kenshi_log.txt** — found in your Kenshi game directory
- **The inventory that misbehaved** — its grid size and roughly what was in it, or a screenshot

## License

GPL-3.0 - see [LICENSE](LICENSE) for details.
