## Hidden Faction Relations (RE_Kenshi plugin)
Adds an in-game panel to the Options menu that shows the player's relation toward hidden factions.

Current status: implemented for Kenshi `1.0.65` using:
- `PlayerInterface::updateUT` as the recurring gameplay host.
- `FactionManager::getAllFactions()` enumeration to iterate all factions.
- `Faction::isNotARealFaction()` to identify hidden factions.
- `FactionRelations::getRelationData()` to read player-to-faction relation values.
- Kenshi Options window hook to inject a `Hidden Factions` tab with a searchable, sortable panel.

## Setup
Clone normally. Shared build scripts are tracked in `tools/build-scripts` via `git subtree`, so no build-script submodule init step is required.

This repo tracks the current Emkejs Mod Core consumer SDK through the `tools/mod-hub-sdk` submodule. Initialize it with `git submodule update --init --recursive -- tools/mod-hub-sdk`.

1. Open a PowerShell terminal in this repo.
2. (Optional) Create `.env` from `.env.example` to set local paths.
3. Source the env script:
   - `. .\scripts\setup_env.ps1`

This sets:
- `KENSHILIB_DEPS_DIR`
- `KENSHILIB_DIR`
- `BOOST_INCLUDE_PATH`

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
Mod data folder name: `Hidden-Faction-Relations`

After deploy, expected files:
- `[Kenshi install dir]\mods\Hidden-Faction-Relations\Hidden-Faction-Relations.mod`
- `[Kenshi install dir]\mods\Hidden-Faction-Relations\RE_Kenshi.json`
- `[Kenshi install dir]\mods\Hidden-Faction-Relations\Hidden-Faction-Relations.dll`
- `[Kenshi install dir]\mods\Hidden-Faction-Relations\mod-config.json`

## Config
At runtime, the plugin reads:
- `[Kenshi install dir]\mods\Hidden-Faction-Relations\mod-config.json`

Supported keys:
- `enabled` (bool)
- `debugLogging` (bool)
- `debugSearchLogging` (bool)
- `debugBindingLogging` (bool)

- `autoFocusSearchOnOpen` (bool)
- `openMenuRequireCtrl` (bool)
- `openMenuRequireShift` (bool)
- `openMenuRequireAlt` (bool)
- `searchInputWidth` (number)
- `searchInputHeight` (number)
- `openMenuKeycode` (number)

If config is missing or unreadable, defaults are used and written back.

## Mod Hub Menu Integration
This plugin registers its settings with the `Emkejs-Mod-Core` Mod Hub using the current public consumer SDK/helper flow tracked in `tools/mod-hub-sdk`.

- Namespace: `emkej.qol`
- Mod ID: `hidden_faction_relations`

Behavior:
- If Mod Hub is available, settings can be changed from the hub menu and are persisted to `mod-config.json`.
- If Mod Hub is unavailable or registration fails, the plugin falls back to file-only config behavior.

## Usage
Open Kenshi's Options window and switch to the `Hidden Factions` tab to inspect the current hidden-faction list and the player's relation to each hidden faction. The list refreshes when that tab is selected.

Controls:
- **Search bar**: filter factions by name; supports Ctrl+Left/Right to jump by word, Ctrl+Delete to delete word.
- **Sort dropdown**: choose relation order or alphabetical order.
- **Scope toggle**: show all factions or hidden-only.
- **Non-zero filter**: toggle between showing all and only factions with non-zero relation.

Keyboard shortcut to open the panel: removed in the leopard fork (07.10.2026) - the list lives on its MCM page, the shortcut did not work for players.

## License
This project is licensed under the GNU General Public License v3.0.
It uses KenshiLib, which is released under GPLv3.

## Изменения форка (ветка leopard)
- Настройки - во вкладке MCM окна «Настройки»; список фракций - на странице мода там же.
- v2: убрана клавиша открытия (Ctrl+Alt+G) и её три флажка модификаторов: у игроков не срабатывала, а список и так открывается на странице MCM.
- v2: «Подробный журнал» в MCM (debugLogging).
