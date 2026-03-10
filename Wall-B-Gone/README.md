## Wall-B-Gone (RE_Kenshi plugin)
Safely dismantles selected walls and sleeping bags (only when not occupied) with Hotkey X and returns materials.

## Setup
Clone normally. Shared build scripts are tracked in `tools/build-scripts` via `git subtree`, so no submodule init step is required.

1) Open a PowerShell terminal in this repo.
2) (Optional) Create `.env` from `.env.example` to set local paths.
3) Source the env script:
   - `. .\scripts\setup_env.ps1`

This sets:
- `KENSHILIB_DEPS_DIR`
- `KENSHILIB_DIR`
- `BOOST_INCLUDE_PATH`
The bash shims in `scripts/` also load `.env`.

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
Mod data folder name: `Wall-B-Gone`

After deploy, expected files:
- `[Kenshi install dir]\mods\Wall-B-Gone\Wall-B-Gone.mod`
- `[Kenshi install dir]\mods\Wall-B-Gone\RE_Kenshi.json`
- `[Kenshi install dir]\mods\Wall-B-Gone\Wall-B-Gone.dll`

## Enable in game
Run RE_Kenshi and enable the mod via Kenshi's `Mods` tab.

## Settings
This mod stores its runtime settings in `mod-config.json`:

- `enabled`
- `sleepingBagDismantleEnabled`
- `hotkeyRequireCtrl`
- `hotkeyRequireShift`
- `hotkeyRequireAlt`
- `hotkey`

If `Emkejs-Mod-Core` is installed, these same settings can also appear in Emkejs Mod Hub. If Mod Hub is unavailable, Wall-B-Gone falls back to its native plugin settings tab.

At runtime, the plugin reads:

- `[Kenshi install dir]\mods\Wall-B-Gone\mod-config.json`
- `%USERPROFILE%\AppData\LocalLow\Lo-Fi Games\Kenshi\mods\Wall-B-Gone\mod-config.json`

`LocalLow` is treated as an override if both exist. If neither exists or either is unreadable, the mod defaults to `enabled=true`, `sleepingBagDismantleEnabled=true`, `hotkeyRequireCtrl=false`, `hotkeyRequireShift=false`, `hotkeyRequireAlt=false`, and `hotkey="X"`.
