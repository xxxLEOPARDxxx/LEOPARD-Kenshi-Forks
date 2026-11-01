## Container Highlight (RE_Kenshi plugin)

This repository is the starter scaffolding for the Container Highlight native RE_Kenshi plugin mod.

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
Mod data folder name: Container-Highlight

After deploy, expected files:
- [Kenshi install dir]\mods\Container-Highlight\Container-Highlight.mod
- [Kenshi install dir]\mods\Container-Highlight\RE_Kenshi.json
- [Kenshi install dir]\mods\Container-Highlight\Container-Highlight.dll
- [Kenshi install dir]\mods\Container-Highlight\mod-config.json

## Config
mod-config.json starts with the shared logging baseline:
- enabled
- debugLogging
- debugSearchLogging
- debugBindingLogging

## Изменения форка (ветка leopard)
- Настройки - во вкладке MCM окна «Настройки»; Mod Hub не нужен.
- Утечка: клоны материалов подсветки теперь убираются из MaterialManager. Раньше каждое нажатие ALT оставляло там по клону на каждую часть каждого контейнера.
- Производительность: перехваты Direct3D 11 (10 методов, тысячи вызовов за кадр) ставятся только по запросу трассировки, а не всегда.

## Изменения форка (15.10.2026)
- `mod-config.json` при первом запуске создаётся копией `mod-config.defaults.json` (`IniDefaults::PrepareJson`) - больше нет «could not read mod-config.json» в журнале на чистой установке.
- Нагрузка (16.10.2026): значки - текстура (поиск ресурса по нескольким путям), надпись, шрифт и цвет 9 слоёв ставились каждый кадр на каждый значок; теперь только при изменении (положение - по-прежнему каждый кадр). Окраска контейнеров - обход узлов сцены, перекраска клонов и setMaterial каждый кадр; теперь после каждого скана (150-500 мс), при смене цвета и раз в 0,5 с, а неизменный клон не трогается. Утечка: lektor поиска объектов (без деструктора) заводился заново на каждом скане - теперь постоянный, с clear().
- Клавиша (16.10.2026): в MCM задаётся одним сочетанием на кнопке клавиши (CTRL+X). Геттер отдаёт модификаторы из highlight_key_require_*, сеттер их туда пишет (раньше отклонял с use_modifier_toggles); строки Require Ctrl/Shift/Alt убраны со страницы, ключи в mod-config.json остались.


Localization diagnostics / Журнал локализации (2026-11-01)
Locale and catalogue-size summaries are written only while this plugin's Detailed log is enabled. Enabling it after translations have loaded reports the existing catalogue once. Loading/format errors remain visible with detailed logging off.
Сводка языка и числа переводов записывается только при включённом подробном журнале этого плагина. Включение журнала после загрузки переводов выводит сводку существующего каталога один раз. Ошибки загрузки и формата видны и при выключенном подробном журнале.
