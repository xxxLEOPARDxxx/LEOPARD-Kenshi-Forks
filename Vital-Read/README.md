## Vital Read (RE_Kenshi plugin)

This repository is the starter scaffolding for the Vital Read native RE_Kenshi plugin mod.

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
Mod data folder name: Vital-Read

After deploy, expected files:
- [Kenshi install dir]\mods\Vital-Read\Vital-Read.mod
- [Kenshi install dir]\mods\Vital-Read\RE_Kenshi.json
- [Kenshi install dir]\mods\Vital-Read\Vital-Read.dll
- [Kenshi install dir]\mods\Vital-Read\mod-config.json

## Config
mod-config.json starts with the shared logging baseline:
- enabled
- debugLogging
- debugSearchLogging
- debugBindingLogging

Optional runtime marker settings:
- unconsciousIconTexture
- unconsciousIconSizePx
- unconsciousIconCoordLeft / Top / Width / Height
- recoveryComaIconTexture
- recoveryComaIconSizePx
- recoveryComaIconCoordLeft / Top / Width / Height
- dyingIconTexture
- dyingIconSizePx
- dyingIconCoordLeft / Top / Width / Height
- playingDeadIconTexture
- playingDeadIconSizePx
- playingDeadIconCoordLeft / Top / Width / Height

Custom state icon example:
```json
{
  "unconsciousIconTexture": "Kenshi_UI.png",
  "unconsciousIconSizePx": 64,
  "unconsciousIconCoordLeft": 45,
  "unconsciousIconCoordTop": 122,
  "unconsciousIconCoordWidth": 32,
  "unconsciousIconCoordHeight": 32,
  "recoveryComaIconTexture": "gui/gfx/heart_64px.png",
  "recoveryComaIconSizePx": 64,
  "dyingIconTexture": "gui/gfx/death_64px.png",
  "dyingIconSizePx": 64,
  "playingDeadIconTexture": "",
  "playingDeadIconSizePx": 64
}
```

Texture lookup accepts plain filenames and relative paths. If no coord crop is provided, the whole image is used. All states share the same lookup behavior, and the runtime will try common locations including:
- `mods/Vital-Read/icons/`
- `mods/Vital-Read/gui/gfx/`
- `mods/Vital-Read/`
- `gui/gfx/`

When Emkejs-Mod-Core is present, Mod Hub also exposes the main `enabled` switch plus the three debug logging flags so they can be toggled in-game.

## Current Runtime
When `enabled` is on, Vital Read now shows a small always-on corner marker on each strictly unconscious, `recovery_coma`, `dying`, or `playing_dead` squad portrait it can map with high confidence. By default unconscious uses the built-in atlas pip, recovery coma and dying use the same heart/death textures shipped with Vital Sense, and playing dead uses a distinct tinted pip unless you override it. `unconsciousIconTexture`, `recoveryComaIconTexture`, `dyingIconTexture`, and `playingDeadIconTexture` can override those per state. Probe hotkeys remain available for verification.

## Probe Hotkeys
Probe hotkeys are manual and quiet by default.

- `Ctrl+Alt+F4`: run the full current mapping probe chain
- `Ctrl+Alt+F6`: start a new probe session
- `Ctrl+Alt+F5`: place a temporary marker on the first strictly unconscious matched portrait
- `Ctrl+Alt+F7`: dump hovered widget + parent chain
- `Ctrl+Alt+F8`: dump the hovered root widget subtree as the current portrait-bar tree probe
- `Ctrl+Alt+F9`: dump portrait-like widget candidates across visible MyGUI roots
- `Ctrl+Alt+F10`: place a temporary hovered-portrait debug marker when confidence is high enough
- `Ctrl+Alt+F11`: dump the currently scoped squad members from the player/member side
- `Ctrl+Alt+F12`: dump the currently scoped squad member states
