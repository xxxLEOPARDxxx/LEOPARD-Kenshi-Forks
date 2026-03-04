## Vital Sense (RE_Kenshi plugin base)

This repository is a clean starter base for a `Vital-Sense` RE_Kenshi native plugin.

## Setup
Clone with `--recurse-submodules` or run `git submodule update --init --recursive`.

1) Open a PowerShell terminal in this repo.
2) (Optional) Create `.env` from `.env.example` to set local paths.
3) Source the env script:
   - `. .\scripts\setup_env.ps1`

This sets:
- `KENSHILIB_DEPS_DIR`
- `KENSHILIB_DIR`
- `BOOST_INCLUDE_PATH`

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
- `only_when_alt_held` (bool): when true, KO highlights are visible while Kenshi's `highlight` keybind is held (with `ALT` as fallback).
- `max_highlight_distance_m` (number): max horizontal distance from camera center for KO highlights (default `3500`).
- `show_icons` (bool): show state icons.
- `show_text` (bool): show state text (`ZZ`, `RC`, `DY`, `PD`, `DE`).
- `show_bounty_glow` (bool): show bounty glow.
- `show_bounty_symbol` (bool): show bounty symbol text using tier colors.
- `enable_character_tint` (bool): enable experimental per-character shader tint highlight (default `true`).
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
