## Organize the Inventory (RE_Kenshi plugin)

This repository is the starter scaffolding for the Organize the Inventory native RE_Kenshi plugin mod.

## Setup
1. Review .env and adjust local paths as needed (.env.example is kept as a reference copy).
2. Open a PowerShell terminal in this repo.
3. Source the environment script:
   - . .\scripts\setup_env.ps1

This sets:
- KENSHILIB_DEPS_DIR
- KENSHILIB_DIR
- BOOST_INCLUDE_PATH

## Build
You can build in Visual Studio, or via the scripted wrapper:

- .\scripts\build-deploy.ps1

Optional parameters:
- -KenshiPath "H:\SteamLibrary\steamapps\common\Kenshi"
- -Configuration "Release"
- -Platform "x64"

## Mod Hub SDK
This repo includes the optional Mod Hub SDK checkout in tools/mod-hub-sdk.
The generated checkout keeps consumer-facing SDK files only; reference docs stay in the template repo/upstream SDK repo.

Generate the standard Mod Hub adapter scaffold with:

- ./scripts/init-mod-template.sh --with-hub
- ./scripts/init-mod-template.ps1 -WithHub

That scaffold creates src/mod_hub_consumer_adapter.h and src/mod_hub_consumer_adapter.cpp.

Sync and validate it with:

- ./scripts/sync-mod-hub-sdk.sh

Use --skip-pull for validation-only mode.
## Deploy layout
Mod data folder name: Organize-the-Inventory

After deploy, expected files:
- [Kenshi install dir]\mods\Organize-the-Inventory\Organize-the-Inventory.mod
- [Kenshi install dir]\mods\Organize-the-Inventory\RE_Kenshi.json
- [Kenshi install dir]\mods\Organize-the-Inventory\Organize-the-Inventory.dll
- [Kenshi install dir]\mods\Organize-the-Inventory\mod-config.json

## Config
mod-config.json starts with the shared logging baseline:
- enabled
- debugLogging
- debugSearchLogging
- debugBindingLogging

## License
This project is licensed under the GNU General Public License v3.0.
It uses KenshiLib, which is released under GPLv3.