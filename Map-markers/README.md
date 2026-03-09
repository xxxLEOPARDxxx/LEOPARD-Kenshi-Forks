## Map-markers

`Map-markers` is a native RE_Kenshi plugin for Kenshi that adds persistent custom markers to the world map.

Current status: pre-alpha MVP. The core marker flow is working, save-backed, and usable in-game.

## Requirements

- Windows
- Kenshi `1.0.65` or `1.0.68`
- RE_Kenshi installed and enabled

## Current features

- Per-save marker persistence in `Map-markers.json`
- Marker add, select, move, delete, and deselect flow
- Marker types with color and glyph differences
- Optional short labels
- Selected-marker editor panel on the map
- `Markers: On/Off` toggle button on the map footer
- Remembered UI settings in `mod-config.json`
- Optional Emkejs Mod Hub integration for selected marker UI settings
- Editor overlap masking so markers do not draw through the editor

## Controls

- Middle-click empty map: add marker
- Left-click marker: select marker
- Left-click empty map with a selection: move selected marker
- `Delete`: remove selected marker
- Right-click map: deselect marker
- Type button in the editor: cycle marker type
- Label field in the editor: edit marker label
- `Ctrl+Alt+F7`: one-shot diagnostics snapshot
- `Ctrl+Alt+F8`: toggle live hover diagnostics

## Marker types

- Note
- Danger
- Stash
- Ruin
- Mine / Resource
- Base / Outpost
- Trader / Shop
- Safe Spot / Bed / Recovery
- Quest
- Todo

## Files and persistence

- Global config: `[Kenshi install dir]\\mods\\Map-markers\\mod-config.json`
- Per-save marker data: `%LOCALAPPDATA%\\kenshi\\save\\<save name>\\Map-markers.json`

`mod-config.json` currently stores:

- `enabled`
- `markers_visible`
- `close_editor_on_map_close`
- `show_hover_labels`
- `default_marker_type`
- `editor_position_customized`
- `editor_left`
- `editor_top`

`default_marker_type` is stored in `mod-config.json` as a type id string such as `note`, `danger`, or `stash`. The loader also accepts legacy integer values.

If `Emkejs-Mod-Core` Mod Hub is present, these user-facing settings can also be changed there:

- `enabled`
- `close_editor_on_map_close`
- `show_hover_labels`
- `default_marker_type` (`0-9`: `0 Note | 1 Danger | 2 Stash | 3 Ruin | 4 Mine | 5 Base | 6 Trader | 7 Safe Spot | 8 Quest | 9 Todo`)

Marker saves currently use schema version `3`.

## Build setup

Clone with `--recurse-submodules`, or run:

- `git submodule update --init --recursive`

Then load the local environment once per shell:

```powershell
. .\scripts\setup_env.ps1
```

This sets:

- `KENSHILIB_DEPS_DIR`
- `KENSHILIB_DIR`
- `BOOST_INCLUDE_PATH`

## Build and deploy

PowerShell:

```powershell
.\scripts\build-deploy.ps1 -KenshiPath "H:\SteamLibrary\steamapps\common\Kenshi"
```

WSL / bash:

```bash
./scripts/build-and-deploy.sh -KenshiPath /mnt/h/SteamLibrary/steamapps/common/Kenshi
```

Temporary smoke deploy used during development:

```bash
./scripts/build-and-deploy.sh -KenshiPath /mnt/i/Kenshi_modding/Map-markers/.tmp_kenshi
```

## Deploy layout

After deploy, expected files are:

- `[Kenshi install dir]\\mods\\Map-markers\\Map-markers.mod`
- `[Kenshi install dir]\\mods\\Map-markers\\RE_Kenshi.json`
- `[Kenshi install dir]\\mods\\Map-markers\\Map-markers.dll`
- `[Kenshi install dir]\\mods\\Map-markers\\mod-config.json`

## Known limits

- The editor currently stays within the map UI area.
- Markers that overlap the editor are hidden while the editor is open.
- Drag-to-move markers is not implemented yet.
- Marker visuals still use the current square MyGUI button skin.
- Mod Hub integration currently exposes hover label, map-close, default-type, and master enable settings.
