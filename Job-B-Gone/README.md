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
Clone with `--recurse-submodules` or run:
- `git submodule update --init --recursive`

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
- `pause_debounce_ms` (number, 0..600000): legacy load-pause debounce setting.
- `debug_log_transitions` (bool): transition diagnostics.
- `enable_delete_all_jobs_selected_member_action` (bool): top scoped delete-all toggle.
- `enable_experimental_single_job_delete` (bool): reserved flag (single-row delete is implemented in current UI flow).
- `log_selected_member_job_snapshot` (bool): debug logging for selected-member job snapshots.
- `job_b_gone_panel_has_custom_position` (bool): true after panel position is customized.
- `job_b_gone_panel_pos_x` (number): persisted panel X coordinate.
- `job_b_gone_panel_pos_y` (number): persisted panel Y coordinate.

If config is missing or unreadable, defaults are used and written back.
