## Organize the Crafting Stations

RE_Kenshi plugin that adds a reusable search bar to Kenshi crafting and research windows.

## Current behavior

- Auto-attaches a search input to detected crafting and research windows.
- Filters visible recipe and blueprint rows by substring match.
- Compacts visible matches upward so filtering does not leave empty scroll gaps.
- Shows visible and total entry counts.
- Supports a clear button, configurable autofocus, configurable width and height, and persisted custom position.
- Applies Mod Hub setting changes live.
- Coexists with `Organize-the-Trader` without taking over trader windows.

## Configuration

Settings live in [Organize-the-Crafting-Stations/mod-config.json](/mnt/i/Kenshi_modding/mods/Organize-the-Crafting-Stations/Organize-the-Crafting-Stations/mod-config.json).
The same enabled, count, clear button, autofocus, and size settings are exposed in Mod Hub when `Emkejs-Mod-Core` is loaded.

Shipped defaults:
- `enabled`: `true`
- `showSearchEntryCount`: `true`
- `showSearchClearButton`: `true`
- `autoFocusSearchInput`: `true`
- `searchInputWidth`: `220`
- `searchInputHeight`: `26`

The authoritative implementation plan is:
- [docs/plans/main_plan.md](/mnt/i/Kenshi_modding/mods/Organize-the-Crafting-Stations/docs/plans/main_plan.md)

The authoritative developer role is:
- [docs/roles/dev.md](/mnt/i/Kenshi_modding/mods/Organize-the-Crafting-Stations/docs/roles/dev.md)

## Cleanup policy for this repo

- Keep reusable search-input pieces.
- Remove stale trader-only docs and planning residue.
- Do not trust copied trader placement or binding assumptions for crafting.
- Keep external build and deploy identity aligned to `Organize-the-Crafting-Stations`.

## License
This project is licensed under the GNU General Public License v3.0.
It uses KenshiLib, which is released under GPLv3.

## Изменения форка (ветка leopard)
- Настройки - во вкладке MCM окна «Настройки»; Mod Hub не нужен.
- Производительность: своя панель поиска запоминается при создании (shared/WidgetRef.h) и сама забывается, когда MyGUI её удаляет. Раньше её искали по имени через весь интерфейс по нескольку раз за кадр. Окно крафта, пока панели нет, ищется 10 раз в секунду, а не каждый кадр.
- «Подробный журнал» в MCM (debugLogging).
- Подсказка в пустом поле поиска переводится (Tr, «Search items...» в .po); счётчик «7 / 27» отодвинут от правого края на 8 px (kSearchCountRightInset) - на тёмных интерфейсах налезал на кайму рамки.
- Ширина счётчика - по настоящей ширине текста (getTextSize), предел - всё место под счётчик; раньше 5 px на символ и предел = текущая ширина: ужимался до 44 px и не рос, «2 / 32» обрезался.
- Проверка «изменились ли строки» - раз в 150 мс, а не каждый кадр.

## Изменения форка (15.10.2026)
- `mod-config.json` при первом запуске создаётся копией `mod-config.defaults.json` (`IniDefaults::PrepareJson`) - больше нет «could not read mod-config.json» в журнале на чистой установке.


Localization diagnostics / Журнал локализации (2026-11-01)
Locale and catalogue-size summaries are written only while this plugin's Detailed log is enabled. Enabling it after translations have loaded reports the existing catalogue once. Loading/format errors remain visible with detailed logging off.
Сводка языка и числа переводов записывается только при включённом подробном журнале этого плагина. Включение журнала после загрузки переводов выводит сводку существующего каталога один раз. Ошибки загрузки и формата видны и при выключенном подробном журнале.

## Prepared release 2 / Подготовленный выпуск 2 (2026-11-02)

English

- Optional language diagnostics. Language and translation-catalogue summaries follow this plugin's Detailed log setting, which is off by default.
- Useful errors remain visible. Translation loading and format errors are still reported with Detailed log disabled. Enabling the setting after loading reports the saved summary once without reloading translations.
- Clean update package. English, Russian and Simplified Chinese each use one compiled runtime catalogue. Personal settings and gameplay saves are preserved.

Русский

- Необязательная диагностика языка. Сводки языка и каталога переводов подчиняются настройке подробного журнала этого плагина, выключенной по умолчанию.
- Ошибки остаются видимыми. Ошибки загрузки и формата переводов записываются и при выключенном подробном журнале. Включение настройки после загрузки выводит сохранённую сводку один раз без перезагрузки переводов.
- Чистый пакет обновления. Английский, русский и упрощённый китайский используют по одному скомпилированному рабочему каталогу. Личные настройки и игровые сохранения сохраняются.
