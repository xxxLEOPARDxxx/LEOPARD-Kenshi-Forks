## Auto Pause on Load (RE_Kenshi plugin)
Pauses Kenshi after a save load lifecycle is detected.

Current status: implemented for Kenshi `1.0.65` using:
- `SaveManager::load(...)` hooks to arm a one-shot pause.
- `GameWorld::isLoadingFromASaveGame()` to detect load phase transitions.
- `GameWorld::userPause(true)` to force paused state after load completes.

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

## Mod Hub Menu Integration
This plugin now registers its settings with the `Emkejs-Mod-Core` Mod Hub using the current public consumer SDK/helper flow tracked in `tools/mod-hub-sdk`.

- Namespace: `emkej.qol`
- Mod ID: `auto_pause_on_load`

Behavior:
- If Mod Hub is available, settings can be changed from the hub menu and are persisted to `mod-config.json`.
- If Mod Hub is unavailable or registration fails, the plugin falls back to file-only config behavior.
