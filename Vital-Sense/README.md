## Vital Sense

Vital-Sense is a RE_Kenshi native plugin that highlights downed characters with state markers, optional body tint, and configurable bounty indicators.

## Setup
Clone normally. Shared build scripts are tracked in `tools/build-scripts` via `git subtree`, so no build-script submodule init step is required.

This repo tracks the current Emkejs Mod Core consumer SDK through the `tools/mod-hub-sdk` submodule. Initialize it with `git submodule update --init --recursive -- tools/mod-hub-sdk`.

1) Open a PowerShell terminal in this repo.
2) (Optional) Create `.env` from `.env.example` to set local paths.
3) Source the env script:
   - `. .\scripts\setup_env.ps1`

This sets:
- `KENSHILIB_DEPS_DIR`
- `KENSHILIB_DIR`
- `BOOST_INCLUDE_PATH`

## Mod Hub SDK Sync
Sync and validate the pinned Mod Hub SDK with:

```bash
./scripts/sync-mod-hub-sdk.sh
```

Use `--skip-pull` for validation-only mode when you only want to check the currently checked out SDK revision.

## Build
You can build in Visual Studio, or via the script below.

### Scripted build + deploy
Run:
- `.\scripts\build-deploy.ps1`

Optional parameters:
- `-KenshiPath "H:\SteamLibrary\steamapps\common\Kenshi"`
- `-Configuration "Release"`
- `-Platform "x64"`

## Deploy layout
Mod data folder name: `Vital-Sense`

After deploy, expected files:
- `[Kenshi install dir]\mods\Vital-Sense\Vital-Sense.mod`
- `[Kenshi install dir]\mods\Vital-Sense\RE_Kenshi.json`
- `[Kenshi install dir]\mods\Vital-Sense\Vital-Sense.dll`
- `[Kenshi install dir]\mods\Vital-Sense\mod-config.json`

## Config
`mod-config.json` controls marker rendering and bounty symbol behavior.

Current keys:
- `enabled` (bool): master toggle.
- `update_interval_ms` (number): KO target refresh interval.
- `highlight_key` (string): primary key that gates KO highlights. Use `UNBOUND` for always-on, default `ALT`.
- `highlight_key_require_ctrl` (bool): require `CTRL` with `highlight_key`.
- `highlight_key_require_shift` (bool): require `SHIFT` with `highlight_key`.
- `highlight_key_require_alt` (bool): require `ALT` with `highlight_key`.
- `max_highlight_distance_m` (number): max horizontal distance from camera center for KO highlights (default `3500`).
- `show_icons` (bool): show state icons.
- `show_text` (bool): show state text (`ZZ`, `RC`, `DY`, `PD`, `DE`).
- `show_bounty_glow` (bool): show bounty glow.
- `show_bounty_symbol` (bool): show bounty symbol text using tier colors.
- `debug_log_diagnostics` (bool): enable extra runtime diagnostics.
- `debug_log_texture_info` (bool): log custom icon texture resolution details.
- `enable_character_tint` (bool): enable experimental per-character shader tint highlight (default `true`).
- `character_tint_include_squad` (bool): tint conscious squadmates too when character tint is enabled (default `true`).
- `character_tint_include_bounty_only` (bool): also tint non-downed bounty-only targets (default `false`).
- `character_tint_force_depth_override` (bool): force tint to render through depth when supported by the shader (default `true`).
- `show_bounty_symbol_on_all_characters` (bool): when true, bounty symbol is also shown for non-downed on-screen targets with bounty.
- `bounty_symbol` (string): symbol text (for example `B` or `$`).
- `bounty_symbol_size_px` (number): base bounty symbol font height (`$` gets a small automatic readability bump, and higher tiers scale up slightly).
- `bounty_symbol_position` (string): `before_state_icon` or `before_state_text`.
- `bounty_symbol_live_anchor_y_offset_cm` (number): vertical world-anchor offset for live/non-downed `$` marker (stable across scroll/zoom), default `420`, max `20000`.
- `bounty_tier_trivial_max`, `bounty_tier_low_max`, `bounty_tier_modest_max`, `bounty_tier_notable_max`, `bounty_tier_high_value_max`, `bounty_tier_elite_max` (number): ascending tier boundaries.
- `bounty_color_trivial_hex`, `bounty_color_low_hex`, `bounty_color_modest_hex`, `bounty_color_notable_hex`, `bounty_color_high_value_hex`, `bounty_color_elite_hex`, `bounty_color_legendary_hex` (hex string): per-tier bounty symbol colors (`#RRGGBB` or `#RRGGBBAA`).
- `enable_unconscious`, `enable_recovery_coma`, `enable_dying`, `enable_playing_dead`, `enable_dead` (bool): per-state visibility toggles.
- `unconscious_text`, `recovery_coma_text`, `dying_text`, `playing_dead_text`, `dead_text` (string): per-state text labels.
- `unconscious_text_size_px`, `recovery_coma_text_size_px`, `dying_text_size_px`, `playing_dead_text_size_px`, `dead_text_size_px` (number): per-state text sizes.
- `enemy_color_hex`, `ally_color_hex`, `squad_color_hex` (hex string): relation colors.
- `unconscious_icon_texture`, `recovery_coma_icon_texture`, `dying_icon_texture`, `playing_dead_icon_texture`, `dead_icon_texture` (string): per-state icon textures.
- `unconscious_icon_size_px`, `recovery_coma_icon_size_px`, `dying_icon_size_px`, `playing_dead_icon_size_px`, `dead_icon_size_px` (number): per-state icon sizes.

## Mod Hub Menu Integration
Vital Sense now registers supported settings with the `Emkejs-Mod-Core` Mod Hub using the current public consumer SDK/helper flow tracked in `tools/mod-hub-sdk`.

- Namespace: `emkej.qol`
- Mod ID: `vital_sense`

Behavior:
- If Mod Hub is available, supported bool/int/keybind/select/text/color settings can be changed in the hub and are persisted back to `mod-config.json`.
- Vital Sense now exposes the full plugin config surface in Mod Hub, including `highlight_key`, modifier toggles, bounty tier maxima/colors, tint controls, update interval, debug flags, per-state label text/size, and per-state icon texture/size settings.
- `bounty_symbol`, the per-state status labels (`unconscious_text`, `recovery_coma_text`, `dying_text`, `playing_dead_text`, `dead_text`), and the per-state icon textures are exposed as bounded text rows, and `bounty_symbol_position` is exposed as a select row.
- Relation colors and bounty-symbol tier colors are exposed as color rows. Bounty tier rows edit RGB only; any existing alpha stays unchanged.
- On section-aware Mod Hub builds, the settings render as five collapsible sections under `Vital Sense`: `Core`, `States`, `Bounty`, `Tint & relation colors`, and `Advanced`.
- On Mod Hub builds with hover-hint V2 row support, supported bool/keybind/select/text rows also show Vital Sense-specific hover tooltips. Older builds keep the same settings but fall back to the older row types without custom hover hints.
- On older Mod Hub builds that do not understand nested sections yet, Vital Sense still registers one flat settings page in the same order.
- If Mod Hub is unavailable or registration fails, the plugin falls back to file-only config behavior.

## License
This project is licensed under the GNU General Public License v3.0.
It uses KenshiLib, which is released under GPLv3.

## Изменения форка (ветка leopard)
- Значки рисуются на слое `Back` (как имена над головами у самой игры), а не на `Top`: больше не просвечивают сквозь окна и панели интерфейса.
- Настройки — во вкладке MCM окна «Настройки»; Mod Hub не нужен.
- Утечка: клоны материалов подсветки животных убираются из MaterialManager (в обычном кадре, не во время загрузки).
