# Changelog

## MCM fork releases / Выпуски MCM-форка

### v5 (prepared / подготовлено)

v5 - English
- Bookmarks survive imports. Importing a save now preserves that saved world's custom map markers.
- Reliable saving. Manual saves, quicksaves, autosaves and Save As retain the current world's markers without mixing bookmarks from another playthrough.

v5 - Русский
- Метки сохраняются при импорте. Импорт сохранения больше не теряет пользовательские метки этого мира.
- Надёжное сохранение. Обычное сохранение, быстрое сохранение, автосохранение и «Сохранить как» сохраняют метки текущего мира, не смешивая их с другим прохождением.

### v4

v4 - English
- Optional language diagnostics. Language and translation-catalogue summaries follow this plugin's Detailed log setting, which is off by default.
- Useful errors remain visible. Translation loading and format errors are still reported with Detailed log disabled. Enabling the setting after loading reports the saved summary once without reloading translations.
- Clean update package. English, Russian and Simplified Chinese each use one compiled runtime catalogue. Personal settings and gameplay saves are preserved.

v4 - Русский
- Необязательная диагностика языка. Сводки языка и каталога переводов подчиняются настройке подробного журнала этого плагина, выключенной по умолчанию.
- Ошибки остаются видимыми. Ошибки загрузки и формата переводов записываются и при выключенном подробном журнале. Включение настройки после загрузки выводит сохранённую сводку один раз без перезагрузки переводов.
- Чистый пакет обновления. Английский, русский и упрощённый китайский используют по одному скомпилированному рабочему каталогу. Личные настройки и игровые сохранения сохраняются.

### v3

v3: A debug log option in Mod Config Menu (it was only on Ctrl+Alt+F8). The marker window has a close button and a row of label colours; the Markers button and the labels on hover are no longer cut.

v3: «Подробный журнал» в Mod Config Menu (раньше - только Ctrl+Alt+F8). В окне метки - крестик и строка цветов подписи; кнопка «Метки» и подписи при наведении больше не обрезаются.

### v2

v2: Marker editor translated (Russian, Chinese), wider, drawn above the map icons, with a Delete button. Much lower FPS cost with the map open and with many markers.

v2: Окно метки переведено (русский, китайский), стало шире, рисуется поверх значков карты и получило кнопку «Удалить». Открытая карта и множество меток почти не снижают FPS.

### v1

v1: First release of this fork: settings in Mod Config Menu instead of Mod Hub, Russian and Chinese translations.

v1: Первый выпуск форка: настройки в Mod Config Menu вместо Mod Hub, перевод на русский и китайский.

## Original upstream history

All notable changes to Map-markers are documented in this file.

## [0.1.0-alpha.2] - 2026-03-12

- Added Mod Hub int-setting v2 support for `default_marker_type` on compatible Emkejs Mod Core builds.
- Reduced default probe logging noise while keeping targeted diagnostics available.
- Refreshed release and build documentation to match the current config defaults and subtree-tracked shared scripts.

## [0.1.0-alpha.1] - 2026-03-12

- Proved world-map feasibility through runtime MyGUI discovery and diagnostics hooks.
- Added map-space marker rendering on the Kenshi world map.
- Added middle-click marker placement and per-save JSON persistence.
- Expanded the single-marker spike into a multi-marker management MVP.
- Added marker selection, move, delete, and right-click deselect flow.
- Added marker types, labels, and a selected-marker editor panel.
- Added persistent UI settings in `mod-config.json`.
- Added a `Markers: On/Off` footer button on the map UI.
- Improved label editing with single-space input and `Ctrl+Left`, `Ctrl+Right`, and `Ctrl+Backspace`.
- Added overlay diagnostics for map/footer/editor troubleshooting.
- Fixed markers drawing over the editor by masking overlapping marker widgets.
- Added optional Emkejs Mod Hub integration for `enabled`, `close_editor_on_map_close`, `show_hover_labels`, and `default_marker_type`.

## [0.0.0] - 2026-02-24

- Reset repository to a clean Map-markers plugin base.
- Removed legacy feature implementation and related release artifacts.
