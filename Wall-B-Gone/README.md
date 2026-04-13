## Wall-B-Gone (RE_Kenshi plugin)
Safely dismantles selected walls and sleeping bags (only when not occupied) with Hotkey X and returns materials.

## Setup
Clone normally. Shared build scripts are tracked in `tools/build-scripts` via `git subtree`, so no submodule init step is required.
This repo tracks the current Emkejs Mod Core consumer SDK through the `tools/mod-hub-sdk` submodule. Initialize it with `git submodule update --init --recursive -- tools/mod-hub-sdk`.

1) Open a PowerShell terminal in this repo.
2) (Optional) Create `.env` from `.env.example` to set local paths.
3) Source the env script:
   - `. .\scripts\setup_env.ps1`

This sets:
- `KENSHILIB_DEPS_DIR`
- `KENSHILIB_DIR`
- `BOOST_INCLUDE_PATH`
The bash shims in `scripts/` also load `.env`.

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
Both the Mod Hub row and the native plugin hotkey row now preserve Ctrl/Shift/Alt modifier bits directly, so combo bindings stay in sync across both UIs.
When `enabled` is off, the dependent Mod Hub rows for sleeping bags, the dismantle hotkey, and the hotkey reset action are hidden so the panel stays focused on the master toggle.

Wall-B-Gone consumes the current public Mod Hub helper from `tools/mod-hub-sdk`, matching the documented `Emkejs-Mod-Core` consumer SDK flow rather than a vendored local copy.

At runtime, the plugin reads:

- `[Kenshi install dir]\mods\Wall-B-Gone\mod-config.json`
- `%USERPROFILE%\AppData\LocalLow\Lo-Fi Games\Kenshi\mods\Wall-B-Gone\mod-config.json`

`LocalLow` is treated as an override if both exist. If neither exists or either is unreadable, the mod defaults to `enabled=true`, `sleepingBagDismantleEnabled=true`, `hotkeyRequireCtrl=false`, `hotkeyRequireShift=false`, `hotkeyRequireAlt=false`, and `hotkey="X"`.

## License
This project is licensed under the GNU General Public License v3.0.
It uses KenshiLib, which is released under GPLv3.
