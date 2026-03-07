## Loot-Scoot-Execute (RE_Kenshi plugin)
Adds an Execute action for downed enemies in Kenshi's right-click context flow.

Current status: implemented for Kenshi `1.0.65` using:
- a custom Execute row aligned to the context menu flow,
- queued execute dispatch with range and facing checks,
- Mod Hub integration for settings and Execute button layout when `Emkejs-Mod-Core.dll` is installed.
- no save-load pause behavior (that remains exclusive to Auto-Pause on Load).

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

## Mod Hub SDK Sync
This repo tracks the Mod Hub SDK through the `tools/mod-hub-sdk` submodule.

Sync and validate the SDK with:

```bash
./scripts/sync-mod-hub-sdk.sh
```

Use `--skip-pull` for validation-only mode.

If you intentionally want to test against a local `Emkejs-Mod-Core` checkout instead of GitHub, override the submodule URL locally:

```bash
git config submodule.tools/mod-hub-sdk.url /mnt/i/Kenshi_modding/Emkejs-Mod-Core
git -C tools/mod-hub-sdk remote set-url origin /mnt/i/Kenshi_modding/Emkejs-Mod-Core
./scripts/sync-mod-hub-sdk.sh
```

Notes:
- Default submodule remote is `git@github.com:Emkej/Emkejs-Mod-Core.git`.
- `scripts/sync-mod-hub-sdk.ps1` allows local `file` transport during local-override workflows.
- Running `git submodule sync -- tools/mod-hub-sdk` will restore URL from `.gitmodules`, so reapply the local override after sync if needed.

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

If `Emkejs-Mod-Core.dll` is present, these same settings are also exposed in the Mod Hub menu under:
- Namespace: `Emkej QoL`
- Mod: `Loot-Scoot-Execute`

Supported keys:
- `enabled` (bool)
- `enable_execute_kill_sound` (bool, default true; plays a short audio event when execute kill succeeds)
- `execute_button_width` (int, default `310`; pixel width for the Execute button panel)
- `execute_button_height` (int, default `56`; pixel height for the Execute button panel)
- `execute_button_x` (int, default `0`; horizontal offset in pixels from the default anchored position)
- `execute_button_y` (int, default `0`; vertical offset in pixels from the default anchored position)

If config is missing or unreadable, defaults are used and written back.
