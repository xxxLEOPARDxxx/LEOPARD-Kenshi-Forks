## Vital Sense (RE_Kenshi plugin base)

This repository is a clean starter base for a `Vital-Sense` RE_Kenshi native plugin.

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
Mod data folder name: `Vital-Sense`

After deploy, expected files:
- `[Kenshi install dir]\mods\Vital-Sense\Vital-Sense.mod`
- `[Kenshi install dir]\mods\Vital-Sense\RE_Kenshi.json`
- `[Kenshi install dir]\mods\Vital-Sense\Vital-Sense.dll`
- `[Kenshi install dir]\mods\Vital-Sense\mod-config.json`

## Config
`mod-config.json` is currently a minimal placeholder for future plugin settings.
