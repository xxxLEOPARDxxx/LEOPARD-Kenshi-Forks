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
- showIcons
- showText
- portraitIconDisplaySizePx
- portraitTextFontHeightPx
- portraitOverlayMarginXPx
- portraitOverlayMarginYPx
- portraitIconAnchor
- portraitTextAnchor

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
- starvingIconTexture
- starvingIconSizePx
- starvingIconCoordLeft / Top / Width / Height
- crippledArmIconTexture
- crippledArmIconSizePx
- crippledArmIconCoordLeft / Top / Width / Height
- crippledLegIconTexture
- crippledLegIconSizePx
- crippledLegIconCoordLeft / Top / Width / Height

Custom state icon example:
```json
{
  "showIcons": true,
  "showText": true,
  "portraitIconDisplaySizePx": 0,
  "portraitTextFontHeightPx": 14,
  "portraitOverlayMarginXPx": 0,
  "portraitOverlayMarginYPx": 0,
  "portraitIconAnchor": "bottom_left",
  "portraitTextAnchor": "top_right",
  "unconsciousIconTexture": "Kenshi_UI.png",
  "unconsciousIconSizePx": 64,
  "unconsciousIconCoordLeft": 45,
  "unconsciousIconCoordTop": 122,
  "unconsciousIconCoordWidth": 32,
  "unconsciousIconCoordHeight": 32,
  "recoveryComaIconTexture": "gui/gfx/heart_64px_opt.png",
  "recoveryComaIconSizePx": 64,
  "dyingIconTexture": "gui/gfx/death_64px_opt.png",
  "dyingIconSizePx": 64,
  "playingDeadIconTexture": "Kenshi_UI.png",
  "playingDeadIconSizePx": 64,
  "playingDeadIconCoordLeft": 80,
  "playingDeadIconCoordTop": 126,
  "playingDeadIconCoordWidth": 38,
  "playingDeadIconCoordHeight": 27,
  "starvingIconTexture": "gui/gfx/starving_64px_opt.png",
  "starvingIconSizePx": 64,
  "crippledArmIconTexture": "gui/gfx/broken-arm_64px_opt.png",
  "crippledArmIconSizePx": 64,
  "crippledLegIconTexture": "gui/gfx/broken-leg_64px_opt.png",
  "crippledLegIconSizePx": 64
}
```

`portraitIconDisplaySizePx` controls the on-screen icon widget size. `0` keeps the previous automatic portrait-relative sizing. `portraitOverlayMarginXPx` and `portraitOverlayMarginYPx` are shared anchor-relative edge margins for both icons and text: positive values move inward from the portrait edge, negative values move outward. `portraitIconAnchor` and `portraitTextAnchor` accept `bottom_left`, `bottom_right`, `top_left`, or `top_right`.

Texture lookup accepts plain filenames and relative paths. If no coord crop is provided, the whole image is used. All states share the same lookup behavior, and the runtime will try common locations including:
- `mods/Vital-Read/icons/`
- `mods/Vital-Read/gui/gfx/`
- `mods/Vital-Read/`
- `gui/gfx/`

When Emkejs-Mod-Core is present, Mod Hub also exposes `enabled`, `showIcons`, `showText`, `portraitIconDisplaySizePx`, `portraitTextFontHeightPx`, `portraitOverlayMarginXPx`, `portraitOverlayMarginYPx`, `portraitIconAnchor`, `portraitTextAnchor`, and `debugLogging` so they can be adjusted in-game. The two anchor settings use dropdown controls for the supported portrait corners.

## Current Runtime
When `enabled` is on, Vital Read now shows a small always-on corner marker on each strictly unconscious, `recovery_coma`, `dying`, `playing_dead`, `starving`, `crippled arm`, or `crippled leg` squad portrait it can map with high confidence. `showIcons` controls the icon overlays, `showText` controls the text badges, `portraitIconDisplaySizePx` controls icon widget size, `portraitTextFontHeightPx` controls badge font height, `portraitOverlayMarginXPx` / `portraitOverlayMarginYPx` control shared edge margins for both icons and text, and `portraitIconAnchor` / `portraitTextAnchor` choose which portrait corner each overlay type uses. By default unconscious and playing dead use built-in `Kenshi_UI.png` atlas pips, recovery coma and dying use the supplied `_opt` heart/death textures, and starving and the two crippled states use the supplied `_opt` textures. `unconsciousIconTexture`, `recoveryComaIconTexture`, `dyingIconTexture`, `playingDeadIconTexture`, `starvingIconTexture`, `crippledArmIconTexture`, and `crippledLegIconTexture` can override those per state. Text badges now use `ZZ` for unconscious, `RC` for `recovery_coma`, `DY` for dying, `PD` for playing dead, `ST` for starving, `CrA` for crippled arm, and `CrL` for crippled leg. Probe hotkeys remain available for verification.

## Probe Hotkeys
Probe hotkeys are manual, quiet by default, and only active when `debugLogging=true`.

- `Ctrl+Alt+F3`: dump the hovered state panel text candidates, the selected character's raw backend snapshot, and arm a one-shot live medical GUI capture for the selected character
- `Ctrl+Alt+F4`: run the full current mapping probe chain
- `Ctrl+Alt+F6`: start a new probe session
- `Ctrl+Alt+F5`: place a temporary marker on the first strictly unconscious matched portrait
- `Ctrl+Alt+F7`: dump hovered widget + parent chain
- `Ctrl+Alt+F8`: dump the hovered root widget subtree as the current portrait-bar tree probe
- `Ctrl+Alt+F9`: dump portrait-like widget candidates across visible MyGUI roots
- `Ctrl+Alt+F10`: place a temporary hovered-portrait debug marker when confidence is high enough
- `Ctrl+Alt+F11`: dump the currently scoped squad members from the player/member side
- `Ctrl+Alt+F12`: dump the currently scoped squad member states

## License
This project is licensed under the GNU General Public License v3.0.
It uses KenshiLib, which is released under GPLv3.

## Изменения форка (ветка leopard)
- Значки больше не на слое `Top` (просвечивали сквозь окна). v1 перенёс их на `Back` - но там их закрывала сама панель портретов. v2: слой берётся у панели портретов (`SetOverlayLayerFromWidget`): значки над портретом, окна над значками.
- v2: «умирает» - как статус «Умирает» в окне персонажа самой игры (stats.cpp): `isInBloodlossTrauma()` или `isProbablyDying()` или кровь ниже точки невозврата, без требования кровотечения и потери сознания. Раньше у скелетов и перевязанных значка не было.
- Настройки — во вкладке MCM окна «Настройки»; Mod Hub не нужен.
- v3: значки на слое панели портретов закрывались ею, когда панель поднималась наверх (щелчок по портретам, перестройка другим плагином). KeepOverlaysAboveTarget сверяет порядок узлов слоя и, если панель выше значков, поднимает значки (upLayerItem); окна выше панели остаются выше и значков.
- v3 (2): подъём над панелью не помог - значки появлялись и через время пропадали. Теперь значки и подписи - ДОЧЕРНИЕ виджеты корня панели портретов с setDepth(-1000) (поверх соседей по панели, окна выше панели закрывают и их). Удаление панели ловит IUnlinkWidget (OverlayUnlinker) и обнуляет ссылки в списках значков.
- v3 (3): на корне панели значки пропадали при наведении на портрет - родитель значка теперь сам портрет (portraitimage), место зажато внутри него.
- v3 (4): setInheritsAlpha(false) - значок не меркнет вместе с портретом оглушённого.
- v3 (ревью 08.10): отписчик сначала сверяет множество своих виджетов (g_ownWidgets); удаление закешированного портрета сбрасывает кеш портретов (OnWidgetGone) - раньше он проверялся чтением удалённых виджетов.
- v3: лимит обхода интерфейса в поисках портретов 512 -> 4096, скрытые ветки и свои значки не считаются: при отряде в 30 человек обход кончался на первом ряду портретов.

## Изменения форка (15.10.2026)
- `mod-config.json` при первом запуске создаётся копией `mod-config.defaults.json` (`IniDefaults::PrepareJson`) - больше нет «could not read mod-config.json» в журнале на чистой установке.
- Ремонт скелета (16.10.2026): скелет, который лежит на кровати-ремонтнике (`Character::inSomething == IN_BED`, у постройки `inWhat` назначение `BF_SKELETON_BED`), получает значок ключа (`gui/gfx/repair-wrench_64.png`, настройки `repairingIcon*` в mod-config.json), в текстовом режиме - «RP». По приоритету - сразу после «мёртв», раньше «без сознания».
- В кровати (16.10.2026): любой, кто лежит в обычной кровати (`BF_BED`), получает красный плюс (`gui/gfx/bed-plus_64.png`, настройки `inBedIcon*`), в текстовом режиме - «BD». Сразу после ремонта скелета (ремкровать - `BF_SKELETON_BED`, ключ), раньше «без сознания».
- Плюс у кровати (16.10.2026) - тем же голубым, что и ключ ремонта: один цвет = «лечится».


Localization diagnostics / Журнал локализации (2026-11-01)
Locale and catalogue-size summaries are written only while this plugin's Detailed log is enabled. Enabling it after translations have loaded reports the existing catalogue once. Loading/format errors remain visible with detailed logging off.
Сводка языка и числа переводов записывается только при включённом подробном журнале этого плагина. Включение журнала после загрузки переводов выводит сводку существующего каталога один раз. Ошибки загрузки и формата видны и при выключенном подробном журнале.

### Icon resource lookup / Поиск картинок значков

Icon paths keep their configured priority, including custom relative, mod-qualified and absolute paths supported by MyGUI. The loader checks aliases through MyGUI's DataManager before loading them. Kenshi registers each mod's `gui/gfx` as a GUI resource root, so a configured `gui/gfx/death_64px_opt.png` may resolve successfully as `death_64px_opt.png`. Absent intermediate aliases no longer create misleading MyGUI texture errors. A requested image that cannot be loaded still reports an error and retains the existing `default_icon.png` fallback; image decoding errors remain visible.

Пути проверяются в прежнем порядке, включая пользовательские относительные, полные и начинающиеся с `mods/` пути, которые поддерживает MyGUI. Игра регистрирует `gui/gfx` мода как корень ресурсов GUI: например, `gui/gfx/death_64px_opt.png` может успешно загрузиться под именем `death_64px_opt.png`. Промежуточные отсутствующие варианты больше не засоряют MyGUI.log. Если нужную картинку загрузить не удалось, ошибка остаётся видимой; сохраняется прежняя запасная картинка `default_icon.png`. Ошибки чтения формата картинки не скрываются.

Existence results and once-only missing-image reports share a bounded 256-name FIFO cache per plugin, reset for a different DataManager. Already registered textures, including manual/runtime textures, remain usable without a file. Steady-state widget/image caches remain unchanged; the new code does not scan the world or poll the filesystem every frame. Resource locations are fixed at game startup; installing/replacing assets requires restarting the game. Regression command from the forks repository: `python Vital-Read/tests/test_texture_preflight.py`; it compiles both actual production loaders and the preflight helper against a narrow MyGUI boundary fixture.

Результаты поиска и отметки уже показанных ошибок хранятся в ограниченном FIFO-кэше на 256 имён для каждого плагина; смена DataManager сбрасывает его. Уже зарегистрированные текстуры, в том числе созданные в памяти, работают и без файла. Прежние кэши виджетов и картинок сохраняются. Новая проверка не обходит мир и не опрашивает файловую систему каждый кадр. Набор ресурсов задаётся при запуске игры; установленные или заменённые картинки применяются после перезапуска.

## Prepared release 6 / Подготовленный выпуск 6 (2026-11-02)

English

- Optional icon compatibility. Optional custom textures are checked before loading, avoiding misleading missing-texture messages when the standard icon is used.
- Optional language diagnostics. Language and translation-catalogue summaries follow this plugin's Detailed log setting, which is off by default.
- Useful errors remain visible. Translation loading and format errors are still reported with Detailed log disabled. Enabling the setting after loading reports the saved summary once without reloading translations.
- Clean update package. English, Russian and Simplified Chinese each use one compiled runtime catalogue. Personal settings and gameplay saves are preserved.

Русский

- Совместимость необязательных значков. Необязательные пользовательские текстуры проверяются до загрузки: использование штатного значка не вызывает ложных сообщений об отсутствующей текстуре.
- Необязательная диагностика языка. Сводки языка и каталога переводов подчиняются настройке подробного журнала этого плагина, выключенной по умолчанию.
- Ошибки остаются видимыми. Ошибки загрузки и формата переводов записываются и при выключенном подробном журнале. Включение настройки после загрузки выводит сохранённую сводку один раз без перезагрузки переводов.
- Чистый пакет обновления. Английский, русский и упрощённый китайский используют по одному скомпилированному рабочему каталогу. Личные настройки и игровые сохранения сохраняются.
