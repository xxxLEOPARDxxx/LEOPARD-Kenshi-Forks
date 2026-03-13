## Job-B-Gone (RE_Kenshi plugin)
Job-B-Gone adds an in-game panel above the squad UI so you can quickly clean up queued jobs for:
- `Me`
- `Selected`
- `Squad`
- `All`

It is a QoL mod only. It does not change game balance, stats, or content.

## Current Features
- Collapsible Job-B-Gone panel above squad UI
- Scoped delete-all actions: `Me`, `Selected`, `Squad`, `All`
- Per-job row actions across the same scopes
- Confirmation dialogs with scope impact counts for destructive scopes
- Two-step destructive flow to reduce accidental clicks
- Multi-select support with merged unique row view
- Adaptive panel height with row scrolling (arrows + mouse wheel)
- Drag-to-reposition panel with persisted coordinates
- Debug/info logs for actions and safety outcomes

## Setup
Clone normally. Shared build scripts are tracked in `tools/build-scripts` via `git subtree`, so no build-scripts submodule init step is required.

Initialize the Mod Hub SDK submodule:
- `git submodule update --init --recursive tools/mod-hub-sdk`

1. Open a PowerShell terminal in this repo.
2. (Optional) Create `.env` from `.env.example` for local paths.
3. Source:
   - `. .\scripts\setup_env.ps1`

This sets:
- `KENSHILIB_DEPS_DIR`
- `KENSHILIB_DIR`
- `BOOST_INCLUDE_PATH`

## Build And Deploy
Build-only (compile verification):
- `.\scripts\build-deploy.ps1`

Build + deploy:
- `.\scripts\build-and-deploy.ps1`

Build + package:
- `.\scripts\build-and-package.ps1`

Package-only:
- `.\scripts\package.ps1`

Sync and validate the bundled Mod Hub SDK:
- `.\scripts\sync-mod-hub-sdk.ps1`
- `./scripts/sync-mod-hub-sdk.sh`

Optional parameters (where supported):
- `-KenshiPath "H:\SteamLibrary\steamapps\common\Kenshi"`
- `-Configuration "Release"`
- `-Platform "x64"`

## Deploy Layout
Mod folder name:
- `Job-B-Gone`

After deploy, expected files:
- `[Kenshi install dir]\mods\Job-B-Gone\Job-B-Gone.mod`
- `[Kenshi install dir]\mods\Job-B-Gone\RE_Kenshi.json`
- `[Kenshi install dir]\mods\Job-B-Gone\Job-B-Gone.dll`
- `[Kenshi install dir]\mods\Job-B-Gone\mod-config.json`

## How To Use
- Open squad UI and use the Job-B-Gone panel above it.
- Top scope actions remove all jobs in the chosen scope.
- Per-row actions remove that job in the chosen scope.
- For destructive scopes (`Squad`, `All`): first click arms, second click opens confirmation.
- Drag the panel header to reposition; position persists in config.

## Config
Runtime config path:
- `[Kenshi install dir]\mods\Job-B-Gone\mod-config.json`

Supported keys:
- `enabled` (bool): master plugin toggle.
- `debug_log_transitions` (bool): transition diagnostics.
- `enable_delete_all_jobs_top_actions` (bool): toggle the top scoped delete-all buttons (`Me`, `Selected`, `Squad`, `All`).
- `log_selected_member_job_snapshot` (bool): debug logging for selected-member job snapshots.
- `hide_panel_during_character_creation` (bool): hide the panel while in character creation/edit mode. Default: `true`.
- `hide_panel_during_inventory_open` (bool): hide the panel while an inventory/trade window is open. Default: `true`.
- `hide_panel_during_character_interaction` (bool): hide the panel while characters are engaged in dialogue/interaction. Default: `true`.
- `job_b_gone_panel_collapsed` (bool): persist the panel collapsed/expanded state. Default: `false`.
- `job_b_gone_panel_has_custom_position` (bool): true after panel position is customized.
- `job_b_gone_panel_pos_x` (number): persisted panel X coordinate.
- `job_b_gone_panel_pos_y` (number): persisted panel Y coordinate.
- `panel_visibility_toggle_hotkey` (string): keyboard shortcut to toggle panel visibility. Default: `"CTRL+B"`. Set to `"NONE"` to disable.

If config is missing or unreadable, defaults are used and written back.

## Emkejs Mod Hub
If `Emkejs-Mod-Core` is present, Job-B-Gone registers its core settings with Mod Hub on startup and keeps `mod-config.json` as the persistence source of truth. The panel toggle hotkey is exposed there as a primary key plus separate Ctrl/Alt/Shift toggles; panel-position fields remain file-backed only. If the hub is unavailable or registration fails, Job-B-Gone falls back to its standalone file-based config without disabling the in-game panel.

Recommended load order:
- `Emkejs-Mod-Core`
- `Job-B-Gone`

### Runtime smoke check
After launching Kenshi with `Emkejs-Mod-Core` and `Job-B-Gone` enabled, run:
- `.\scripts\phase22_mod_hub_runtime_smoke_test.ps1 -ExpectedMode attached`

The script reads the latest `RE_Kenshi_log.txt` session and passes only when the latest Job-B-Gone startup reaches `event=mod_hub_attached` or `event=mod_hub_fallback`, which proves the current run reached a recognizable Mod Hub attach or fallback path.
