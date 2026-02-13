## Job-B-Gone (RE_Kenshi plugin)
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
Mod data folder name: `Job-B-Gone`

After deploy, expected files:
- `[Kenshi install dir]\mods\Job-B-Gone\Job-B-Gone.mod`
- `[Kenshi install dir]\mods\Job-B-Gone\RE_Kenshi.json`
- `[Kenshi install dir]\mods\Job-B-Gone\Job-B-Gone.dll`
- `[Kenshi install dir]\mods\Job-B-Gone\mod-config.json`

## Config
At runtime, the plugin reads:
- `[Kenshi install dir]\mods\Job-B-Gone\mod-config.json`

Supported keys:
- `enabled` (bool)
- `pause_debounce_ms` (number, 0..600000)
- `debug_log_transitions` (bool)
- `enable_delete_all_jobs_selected_member_action` (bool)
- `enable_experimental_single_job_delete` (bool, currently reserved for future row-delete work)
- `log_selected_member_job_snapshot` (bool, logs selected-member job rows before/after delete-all)

If config is missing or unreadable, defaults are used and written back.
