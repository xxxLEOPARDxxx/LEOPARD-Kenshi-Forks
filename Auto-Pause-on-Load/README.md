## Auto Pause on Load (RE_Kenshi plugin)
Pauses Kenshi after a save load lifecycle is detected.

Current status: implemented for Kenshi `1.0.65` using:
- `SaveManager::load(...)` hooks to arm a one-shot pause.
- `GameWorld::isLoadingFromASaveGame()` to detect load phase transitions.
- `GameWorld::userPause(true)` to force paused state after load completes.

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
Mod data folder name: `Auto-Pause-on-Load`

After deploy, expected files:
- `[Kenshi install dir]\mods\Auto-Pause-on-Load\Auto-Pause-on-Load.mod`
- `[Kenshi install dir]\mods\Auto-Pause-on-Load\RE_Kenshi.json`
- `[Kenshi install dir]\mods\Auto-Pause-on-Load\Auto-Pause-on-Load.dll`
- `[Kenshi install dir]\mods\Auto-Pause-on-Load\mod-config.json`

## Config
At runtime, the plugin reads:
- `[Kenshi install dir]\mods\Auto-Pause-on-Load\mod-config.json`

Supported keys:
- `enabled` (bool)
- `pause_debounce_ms` (number, 0..600000)
- `debug_log_transitions` (bool)
- `pause_on_trade` (bool)
- `resume_after_trade` (bool, only applies when `pause_on_trade=true`)
- `pause_on_inventory_open` (bool)
- `resume_after_inventory_close` (bool, only applies when `pause_on_inventory_open=true`)

If config is missing or unreadable, defaults are used and written back.
