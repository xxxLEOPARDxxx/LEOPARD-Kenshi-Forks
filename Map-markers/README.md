## Map-markers (RE_Kenshi plugin base)

This repository is a clean starter base for a `Map-markers` RE_Kenshi native plugin.

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
Mod data folder name: `Map-markers`

After deploy, expected files:
- `[Kenshi install dir]\mods\Map-markers\Map-markers.mod`
- `[Kenshi install dir]\mods\Map-markers\RE_Kenshi.json`
- `[Kenshi install dir]\mods\Map-markers\Map-markers.dll`
- `[Kenshi install dir]\mods\Map-markers\mod-config.json`

## Config
`mod-config.json` stores global plugin UI settings such as marker visibility, editor-close behavior, and remembered editor position.
