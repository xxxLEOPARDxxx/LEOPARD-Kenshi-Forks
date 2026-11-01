## Auto Pause on Load (RE_Kenshi plugin)
Pauses Kenshi after a save load lifecycle is detected.

Current status: implemented for Kenshi `1.0.65` using:
- `PlayerInterface::updateUT` as the recurring gameplay host.
- `GameWorld::isLoadingFromASaveGame()` polling to detect load phase transitions and arm a one-shot load pause.
- `GameWorld::userPause(true)` to force paused state after load completes.

## Setup
Clone normally. Shared build scripts are tracked in `tools/build-scripts` via `git subtree`, so no build-script submodule init step is required.

This repo tracks the current Emkejs Mod Core consumer SDK through the `tools/mod-hub-sdk` submodule. Initialize it with `git submodule update --init --recursive -- tools/mod-hub-sdk`.

1) Open a PowerShell terminal in this repo.
2) (Optional) Create `.env` from `.env.example` to set local paths.
3) Source the env script:
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
Mod data folder name: `Auto-Pause-on-Load`

After deploy, expected files:
- `[Kenshi install dir]\mods\Auto-Pause-on-Load\Auto-Pause-on-Load.mod`
- `[Kenshi install dir]\mods\Auto-Pause-on-Load\RE_Kenshi.json`
- `[Kenshi install dir]\mods\Auto-Pause-on-Load\Auto-Pause-on-Load.dll`
- `[Kenshi install dir]\mods\Auto-Pause-on-Load\mod-config.json`

## Config
At runtime, the plugin reads:
- `[Kenshi install dir]\mods\Auto-Pause-on-Load\mod-config.json`

Supported keys:
- `enabled` (bool)
- `pause_debounce_ms` (number, 0..600000)
- `debug_logging` (bool; `debug_log_transitions` is still accepted as a legacy alias)
- `pause_on_trade` (bool)
- `resume_after_trade` (bool, only applies when `pause_on_trade=true`)
- `pause_on_inventory_open` (bool)
- `resume_after_inventory_close` (bool, only applies when `pause_on_inventory_open=true`)

If config is missing or unreadable, defaults are used and written back.

## Mod Hub Menu Integration
This plugin now registers its settings with the `Emkejs-Mod-Core` Mod Hub using the current public consumer SDK/helper flow tracked in `tools/mod-hub-sdk`.

- Namespace: `emkej.qol`
- Mod ID: `auto_pause_on_load`

Behavior:
- If Mod Hub is available, settings can be changed from the hub menu and are persisted to `mod-config.json`.
- If Mod Hub is unavailable or registration fails, the plugin falls back to file-only config behavior.

## License
This project is licensed under the GNU General Public License v3.0.
It uses KenshiLib, which is released under GPLv3.

## Изменения форка: v2 (13.10.2026)
- Пауза после торговли: при смене персонажа во время торговли окно торговли на миг "закрывалось", открытым оставался инвентарь (пауза при открытии инвентаря включена по умолчанию), торговля "открывалась" снова уже на паузе и считала её паузой игрока - после торговли игра оставалась на паузе. Теперь TryPauseOnUiOpen(..., inheritPluginPause): если паузу держит другое окно мода (торговля/инвентарь), новое окно её перенимает.

## Изменения форка (15.10.2026)
- Пауза после переноса (импорта) сохранения: хук `SaveManager::importGame` (по имени экспорта KenshiLib) и сигнал импорта; пауза через 1.5 с после того, как импорт затих.
- Настройка `pauseOnSquadAttack`: пауза, когда бьют персонажа из невыбранного отряда (хук `MedicalSystem::addWound`, не чаще раза в 30 с на отряд).
- `mod-config.json` при первом запуске создаётся копией `mod-config.defaults.json` (`IniDefaults::PrepareJson`) - больше нет «could not read mod-config.json» в журнале на чистой установке.
- Интервал паузы при нападении на отряд настраивается: `squad_attack_cooldown_s` (0-300 с, по умолчанию 30), ползунок в MCM; 0 - на каждый удар.
- Пауза после переноса сохранения стабильнее (16.10.2026): ждём, пока кадры идут ровно (без провала больше 0,25 с) 2 с подряд; пауза ставится, даже если уже стоит, и 3 с удерживается - если игра сняла её сама, ставим снова (до трёх раз); если за это время игрок нажал клавишу или кнопку мыши - паузу снял он, удержание прекращается (движение мыши не в счёт). Строка `paused_after_load=true source=save_import` пишется в журнал всегда.


Localization diagnostics / Журнал локализации (2026-11-01)
Locale and catalogue-size summaries are written only while this plugin's Detailed log is enabled. Enabling it after translations have loaded reports the existing catalogue once. Loading/format errors remain visible with detailed logging off.
Сводка языка и числа переводов записывается только при включённом подробном журнале этого плагина. Включение журнала после загрузки переводов выводит сводку существующего каталога один раз. Ошибки загрузки и формата видны и при выключенном подробном журнале.

## Prepared release 6 / Подготовленный выпуск 6 (2026-11-02)

English

- Optional language diagnostics. Language and translation-catalogue summaries follow this plugin's Detailed log setting, which is off by default.
- Useful errors remain visible. Translation loading and format errors are still reported with Detailed log disabled. Enabling the setting after loading reports the saved summary once without reloading translations.
- Clean update package. English, Russian and Simplified Chinese each use one compiled runtime catalogue. Personal settings and gameplay saves are preserved.

Русский

- Необязательная диагностика языка. Сводки языка и каталога переводов подчиняются настройке подробного журнала этого плагина, выключенной по умолчанию.
- Ошибки остаются видимыми. Ошибки загрузки и формата переводов записываются и при выключенном подробном журнале. Включение настройки после загрузки выводит сохранённую сводку один раз без перезагрузки переводов.
- Чистый пакет обновления. Английский, русский и упрощённый китайский используют по одному скомпилированному рабочему каталогу. Личные настройки и игровые сохранения сохраняются.
