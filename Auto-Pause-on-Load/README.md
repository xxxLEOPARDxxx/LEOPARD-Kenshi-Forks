## Auto Pause on Load (RE_Kenshi plugin)
Pauses Kenshi after a save load lifecycle is detected.

Current status: scaffold wired and safe by default. The state machine and config are implemented, but game-specific `is_loading_save` and `set_paused(true)` offsets are still TODO in code.

## Setup
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
- `pause_on_save_load` (bool)
- `pause_debounce_ms` (number, 0..600000)
- `debug_log_transitions` (bool)

If config is missing or unreadable, defaults are used and written back.

## Scaffold TODO
Hook these in `Auto-Pause-on-Load.cpp`:
- `g_fnIsLoadingSave`
- `g_fnSetPaused`

Until those are wired for each supported Kenshi binary version, the plugin logs that auto-pause is idle.
