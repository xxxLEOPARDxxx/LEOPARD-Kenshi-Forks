## Organize the Trader (RE_Kenshi plugin base)

This repository is a clean starter base for a `Organize-the-Trader` RE_Kenshi native plugin.

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
Mod data folder name: `Organize-the-Trader`

After deploy, expected files:
- `[Kenshi install dir]\mods\Organize-the-Trader\Organize-the-Trader.mod`
- `[Kenshi install dir]\mods\Organize-the-Trader\RE_Kenshi.json`
- `[Kenshi install dir]\mods\Organize-the-Trader\Organize-the-Trader.dll`
- `[Kenshi install dir]\mods\Organize-the-Trader\mod-config.json`

## Config
`mod-config.json` supports:

- `enabled` (bool): master plugin toggle.
- `showSearchEntryCount` (bool): show visible/total item entries in the search bar.
- `showSearchQuantityCount` (bool): show visible stack quantity in the search bar.
- `debugLogging` (bool): master runtime debug switch for support diagnostics.
- `debugSearchLogging` (bool): enables search-path diagnostics when `debugLogging` is enabled.
- `debugBindingLogging` (bool): enables inventory-binding diagnostics when `debugLogging` is enabled.
- `searchInputWidth` (int): desired search input width in pixels, clamped at runtime.
- `searchInputHeight` (int): desired search input height in pixels, clamped at runtime.
- `searchInputPositionCustomized` (bool): when `true`, reuses the saved drag position instead of the default placement.
- `searchInputLeft` (int): saved left coordinate for the search bar inside the trader window.
- `searchInputTop` (int): saved top coordinate for the search bar inside the trader window.

Default:

```json
{
  "enabled": true,
  "showSearchEntryCount": true,
  "showSearchQuantityCount": true,
  "debugLogging": false,
  "debugSearchLogging": false,
  "debugBindingLogging": false,
  "searchInputWidth": 372,
  "searchInputHeight": 26,
  "searchInputPositionCustomized": false,
  "searchInputLeft": 0,
  "searchInputTop": 0
}
```

## Mod Hub (Optional)
If `Emkejs-Mod-Core` is loaded, Organize-the-Trader registers its user-facing settings in Mod Hub (`enabled`, search counts, search width, search height) and writes committed changes back to `mod-config.json`.

Recommended load order:
- `Emkejs-Mod-Core`
- `Organize-the-Trader`

If Mod Core is absent or attach fails, this mod keeps using `mod-config.json` only.
