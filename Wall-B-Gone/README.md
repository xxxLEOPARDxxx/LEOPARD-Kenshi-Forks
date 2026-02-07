## Wall-B-Gone (RE_Kenshi plugin)
Safely dismantles selected walls with Hotkey X and returns materials.

## Setup
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
