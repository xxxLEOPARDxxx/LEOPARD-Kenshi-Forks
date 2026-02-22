## Loot-Scoot-Execute (RE_Kenshi plugin)
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
Mod data folder name: `Loot-Scoot-Execute`

After deploy, expected files:
- `[Kenshi install dir]\mods\Loot-Scoot-Execute\Loot-Scoot-Execute.mod`
- `[Kenshi install dir]\mods\Loot-Scoot-Execute\RE_Kenshi.json`
- `[Kenshi install dir]\mods\Loot-Scoot-Execute\Loot-Scoot-Execute.dll`
- `[Kenshi install dir]\mods\Loot-Scoot-Execute\mod-config.json`

## Config
At runtime, the plugin reads:
- `[Kenshi install dir]\mods\Loot-Scoot-Execute\mod-config.json`

Supported keys:
- `enabled` (bool)
- `pause_debounce_ms` (number, 0..600000)
- `debug_log_transitions` (bool)
- `enable_context_menu_probe` (bool, default false; probe-only scaffolding for context menu lifecycle logging)
- `enable_context_menu_injection` (bool, default false; guarded native menu mutation switch)
- `enable_execute_action` (bool, default false; guarded execute action switch)
- `debug_context_menu` (bool, default false; enables extra compatibility/probe diagnostics)
- `enable_debug_direct_damage_fallback` (bool, default false; debug-only fallback when task-based execute dispatch fails)
- `enable_execute_kill_sound` (bool, default true; plays a short audio event when execute kill succeeds)

If config is missing or unreadable, defaults are used and written back.

Debug execute hotkey:
- `F8` triggers internal execute dispatch against the current `mouseRightTarget` while `enable_execute_action=true` and `debug_context_menu=true`.
