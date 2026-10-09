## Loot-Scoot-Execute (RE_Kenshi plugin)
Adds an Execute action for downed enemies in Kenshi's right-click context flow.

Current status: implemented for Kenshi `1.0.65` using:
- a custom Execute row aligned to the context menu flow,
- queued execute dispatch with range and facing checks,
- an `Execute All` panel action that chains nearby unconscious enemies around the selected target,
- Mod Hub integration for settings and Execute button layout when `Emkejs-Mod-Core.dll` is installed.
- no save-load pause behavior (that remains exclusive to Auto-Pause on Load).

## Setup
Clone normally. Shared build scripts are tracked in `tools/build-scripts` via `git subtree`, while the Mod Hub SDK still uses the `tools/mod-hub-sdk` submodule.
`tools/build-scripts` is the shared source of truth. Top-level `scripts/` entrypoints are local compatibility wrappers that delegate into `tools/build-scripts`, plus a few repo-specific helper scripts such as Mod Hub SDK sync and runtime smoke checks.

1) Open a PowerShell terminal in this repo.
2) (Optional) Create `.env` from `.env.example` to set local paths.
3) Source the env script:
   - `. .\scripts\setup_env.ps1`

This sets:
- `KENSHILIB_DEPS_DIR`
- `KENSHILIB_DIR`
- `BOOST_INCLUDE_PATH`

## Mod Hub SDK Sync
This repo tracks the Mod Hub SDK through the `tools/mod-hub-sdk` submodule.

Sync and validate the SDK with:

```bash
./scripts/sync-mod-hub-sdk.sh
```

Use `--skip-pull` for validation-only mode.

If you intentionally want to test against a local `Emkejs-Mod-Core` checkout instead of GitHub, override the submodule URL locally:

```bash
git config submodule.tools/mod-hub-sdk.url /mnt/i/Kenshi_modding/Emkejs-Mod-Core
git -C tools/mod-hub-sdk remote set-url origin /mnt/i/Kenshi_modding/Emkejs-Mod-Core
./scripts/sync-mod-hub-sdk.sh
```

Notes:
- Default submodule remote is `git@github.com:Emkej/Emkejs-Mod-Core.git`.
- `scripts/sync-mod-hub-sdk.ps1` allows local `file` transport during local-override workflows.
- Running `git submodule sync -- tools/mod-hub-sdk` will restore URL from `.gitmodules`, so reapply the local override after sync if needed.

## Build
You can build in Visual Studio, or via the script below.

### Scripted build + deploy
Run:
- `.\scripts\build-deploy.ps1`

Optional parameters:
- `-KenshiPath "H:\SteamLibrary\steamapps\common\Kenshi"`
- `-Configuration "Release"`
- `-Platform "x64"`

### Runtime smoke check
After launching Kenshi once with the deployed mod, validate the latest runtime log with:
- `pwsh -NoProfile -File scripts/phase22_mod_hub_consumer_runtime_smoke_test.ps1 -ExpectedMode attached`

For fallback validation without `Emkejs-Mod-Core.dll`, use:
- `pwsh -NoProfile -File scripts/phase22_mod_hub_consumer_runtime_smoke_test.ps1 -ExpectedMode fallback`

## Deploy layout
Mod data folder name: `Loot-Scoot-Execute`

After deploy, expected files:
- `[Kenshi install dir]\mods\Loot-Scoot-Execute\Loot-Scoot-Execute.mod`
- `[Kenshi install dir]\mods\Loot-Scoot-Execute\RE_Kenshi.json`
- `[Kenshi install dir]\mods\Loot-Scoot-Execute\Loot-Scoot-Execute.dll`
- `[Kenshi install dir]\mods\Loot-Scoot-Execute\mod-config.json`

## Config
At runtime, the plugin reads:
- `[Kenshi install dir]\mods\Loot-Scoot-Execute\mod-config.json`

If `Emkejs-Mod-Core.dll` is present, these same settings are also exposed in the Mod Hub menu under:
- Namespace: `Emkej QoL`
- Mod: `Loot-Scoot-Execute`

Supported keys:
- `enabled` (bool)
- `enable_execute_kill_sound` (bool, default true; plays a short audio event when execute kill succeeds)
- `debug_execute_logging` (bool, default `false`; enables focused execute investigation logs in `RE_Kenshi_log.txt` for target eligibility and dispatch failures)
- `enable_execute_all` (bool, default `true`; shows the `Execute All` button and allows batch execute behavior)
- `ignore_execute_alliance_check` (bool, default `false`; allows execute on incapacitated non-hostiles and allies, but still blocks player characters)
- `execute_distance_units` (int, default `2`; required distance before queued execute triggers, clamped to `1..200`; legacy `execute_distance_meters` is still accepted)
- `execute_all_radius_units` (int, default `10`; search radius used by `Execute All` around the selected target, clamped to `1..200`)
- `execute_button_width` (int, default `310`; pixel width for the Execute button panel)
- `execute_button_height` (int, default `56`; pixel height for the Execute button panel)
- `execute_button_gap` (int, default `-8`; vertical gap in pixels between `Execute` and `Execute All`; use negative values to overlap the rows, clamped to `-64..64`)
- `execute_button_x` (int, default `0`; horizontal offset in pixels from the default anchored position)
- `execute_button_y` (int, default `0`; vertical offset in pixels from the default anchored position)

If config is missing or unreadable, defaults are used and written back.

For faction-specific execute failures, set `debug_execute_logging` to `true`, reproduce the issue once, then attach the updated `RE_Kenshi_log.txt`. Look for lines starting with `[investigate][execute]`.

## Изменения форка (ветка leopard)
- Настройки - во вкладке MCM окна «Настройки»; Mod Hub не нужен.
- v2: подписи кнопок Execute / Execute All идут через перевод (Tr) - en/ru/zh в locale.

## License
This project is licensed under the GNU General Public License v3.0.
It uses KenshiLib, which is released under GPLv3.
- v3: задания добивания - массив на исполнителя (до 16, ExecuteJob; старые имена g_queuedExecute*/g_executeAllBatch* - макросы на текущее задание). «Добить всех» берёт всех выделенных своих; цели - общий пул, каждый берёт ближайшую свободную к себе (раньше - по порядку сбора, один исполнитель).
- v3: приказ игрока выделенным (newPlayerTaskSelectedCharacters, кроме прокси-приказа добивания, и updateLastMoveWaypointSelectedCharacters - ПКМ по земле) снимает их добивание; задание моложе 500 мс не трогается.
- v3: подход к цели - раз в 1 с вместо 200 мс, clearAllAIGoals только первым приказом (раньше задача персонажа мелькала).
- v3: убрана строка «Кнопка по умолчанию» (MCM сбрасывает всю страницу).
- v4: звук добивания - только события из банков игры (проверено по хешам в data/audio/*.bnk): у оригинала первыми шли несуществующие "Heavy_Hit"/"Light_Hit", и звука не было. Теперь на жертве Impact + голос: человек - VO_Get_Hit, животное - VO_Creature_Die, скелет - Deflection (металл).
