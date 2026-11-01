## Vital Sense

Vital-Sense is a RE_Kenshi native plugin that highlights downed characters with state markers, optional body tint, and configurable bounty indicators.

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
Mod data folder name: `Vital-Sense`

After deploy, expected files:
- `[Kenshi install dir]\mods\Vital-Sense\Vital-Sense.mod`
- `[Kenshi install dir]\mods\Vital-Sense\RE_Kenshi.json`
- `[Kenshi install dir]\mods\Vital-Sense\Vital-Sense.dll`
- `[Kenshi install dir]\mods\Vital-Sense\mod-config.json`

## Config
`mod-config.json` controls marker rendering and bounty symbol behavior.

Current keys:
- `enabled` (bool): master toggle.
- `update_interval_ms` (number): KO target refresh interval.
- `highlight_key` (string): primary key that gates KO highlights. Use `UNBOUND` for always-on, default `ALT`.
- `highlight_key_require_ctrl` (bool): require `CTRL` with `highlight_key`.
- `highlight_key_require_shift` (bool): require `SHIFT` with `highlight_key`.
- `highlight_key_require_alt` (bool): require `ALT` with `highlight_key`.
- `max_highlight_distance_m` (number): max horizontal distance from camera center for KO highlights (default `3500`).
- `show_icons` (bool): show state icons.
- `show_text` (bool): show state text (`ZZ`, `RC`, `DY`, `PD`, `DE`).
- `show_bounty_glow` (bool): show bounty glow.
- `show_bounty_symbol` (bool): show bounty symbol text using tier colors.
- `debug_log_diagnostics` (bool): enable extra runtime diagnostics.
- `debug_log_texture_info` (bool): log custom icon texture resolution details.
- `enable_character_tint` (bool): enable experimental per-character shader tint highlight (default `true`).
- `character_tint_include_squad` (bool): tint conscious squadmates too when character tint is enabled (default `true`).
- `character_tint_include_bounty_only` (bool): also tint non-downed bounty-only targets (default `false`).
- `character_tint_force_depth_override` (bool): force tint to render through depth when supported by the shader (default `true`).
- `show_bounty_symbol_on_all_characters` (bool): when true, bounty symbol is also shown for non-downed on-screen targets with bounty.
- `bounty_symbol` (string): symbol text (for example `B` or `$`).
- `bounty_symbol_size_px` (number): base bounty symbol font height (`$` gets a small automatic readability bump, and higher tiers scale up slightly).
- `bounty_symbol_position` (string): `before_state_icon` or `before_state_text`.
- `bounty_symbol_live_anchor_y_offset_cm` (number): vertical world-anchor offset for live/non-downed `$` marker (stable across scroll/zoom), default `420`, max `20000`.
- `bounty_tier_trivial_max`, `bounty_tier_low_max`, `bounty_tier_modest_max`, `bounty_tier_notable_max`, `bounty_tier_high_value_max`, `bounty_tier_elite_max` (number): ascending tier boundaries.
- `bounty_color_trivial_hex`, `bounty_color_low_hex`, `bounty_color_modest_hex`, `bounty_color_notable_hex`, `bounty_color_high_value_hex`, `bounty_color_elite_hex`, `bounty_color_legendary_hex` (hex string): per-tier bounty symbol colors (`#RRGGBB` or `#RRGGBBAA`).
- `enable_unconscious`, `enable_recovery_coma`, `enable_dying`, `enable_playing_dead`, `enable_dead` (bool): per-state visibility toggles.
- `unconscious_text`, `recovery_coma_text`, `dying_text`, `playing_dead_text`, `dead_text` (string): per-state text labels.
- `unconscious_text_size_px`, `recovery_coma_text_size_px`, `dying_text_size_px`, `playing_dead_text_size_px`, `dead_text_size_px` (number): per-state text sizes.
- `enemy_color_hex`, `ally_color_hex`, `squad_color_hex` (hex string): relation colors.
- `unconscious_icon_texture`, `recovery_coma_icon_texture`, `dying_icon_texture`, `playing_dead_icon_texture`, `dead_icon_texture` (string): per-state icon textures.
- `unconscious_icon_size_px`, `recovery_coma_icon_size_px`, `dying_icon_size_px`, `playing_dead_icon_size_px`, `dead_icon_size_px` (number): per-state icon sizes.

## Mod Hub Menu Integration
Vital Sense now registers supported settings with the `Emkejs-Mod-Core` Mod Hub using the current public consumer SDK/helper flow tracked in `tools/mod-hub-sdk`.

- Namespace: `emkej.qol`
- Mod ID: `vital_sense`

Behavior:
- If Mod Hub is available, supported bool/int/keybind/select/text/color settings can be changed in the hub and are persisted back to `mod-config.json`.
- Vital Sense now exposes the full plugin config surface in Mod Hub, including `highlight_key`, modifier toggles, bounty tier maxima/colors, tint controls, update interval, debug flags, per-state label text/size, and per-state icon texture/size settings.
- `bounty_symbol`, the per-state status labels (`unconscious_text`, `recovery_coma_text`, `dying_text`, `playing_dead_text`, `dead_text`), and the per-state icon textures are exposed as bounded text rows, and `bounty_symbol_position` is exposed as a select row.
- Relation colors and bounty-symbol tier colors are exposed as color rows. Bounty tier rows edit RGB only; any existing alpha stays unchanged.
- On section-aware Mod Hub builds, the settings render as five collapsible sections under `Vital Sense`: `Core`, `States`, `Bounty`, `Tint & relation colors`, and `Advanced`.
- On Mod Hub builds with hover-hint V2 row support, supported bool/keybind/select/text rows also show Vital Sense-specific hover tooltips. Older builds keep the same settings but fall back to the older row types without custom hover hints.
- On older Mod Hub builds that do not understand nested sections yet, Vital Sense still registers one flat settings page in the same order.
- If Mod Hub is unavailable or registration fails, the plugin falls back to file-only config behavior.

## License
This project is licensed under the GNU General Public License v3.0.
It uses KenshiLib, which is released under GPLv3.

## Изменения форка (ветка leopard)
- Значки рисуются на слое `Back` (как имена над головами у самой игры), а не на `Top`: больше не просвечивают сквозь окна и панели интерфейса.
- Настройки — во вкладке MCM окна «Настройки»; Mod Hub не нужен.
- Утечка: клоны материалов подсветки животных убираются из MaterialManager (в обычном кадре, не во время загрузки).

## Изменения форка (15.10.2026)
- Отношение «нейтрал» (`RELATION_NEUTRAL`) и цвет `neutral_color_hex` (#FFA64D): не враг ни одному своему персонажу и не союзник всем - как в CharacterHighlight (KenshiLib_Examples). Мирные дикие животные - тоже нейтралы, пауки - всегда враги.
- В MCM четыре цвета отношений в разделе окраски (число строк - `kRelationColorSettingCount`).
- `mod-config.json` при первом запуске создаётся копией `mod-config.defaults.json` (`IniDefaults::PrepareJson`) - больше нет «could not read mod-config.json» в журнале на чистой установке.
- Число значков настраивается (16.10.2026): `max_markers` (8-256, по умолчанию 96), строка «Значков на экране» в MCM. Раньше было зашито 48, и брались первые 48 целей списка - в том числе за краем экрана, видимым значков не хватало. Теперь место получают только видимые цели, при переполнении - ближайшие к центру камеры.
- «Окрашивать цели с наградой» (`character_tint_include_bounty_only`, 16.10.2026): цель с наградой - стоящая (при включённом символе награды у всех на экране) или лежащая - красится цветом тира награды, как значок $. Раньше стоящие красились цветом отношения и не отличались от обычных врагов, поэтому казалось, что опция не работает.
- Совместимость с Character Highlight (пример KenshiLib, 16.10.2026): он каждый кадр переписывает общий материал тела всех персонажей - окраска Vital-Sense у чужих держалась один кадр (цель с наградой была жёлтой). Теперь окраска накладывается после кадра мира и у чужих (не свой отряд, не животные - те на своих копиях материала) повторяется каждый кадр.
- Клавиша (16.10.2026): в MCM задаётся одним сочетанием на кнопке клавиши (CTRL+X); строки «Нужен Ctrl/Shift/Alt» убраны. Модификаторы уходят и приходят вместе со значением клавиши (Get/SetKeybindSettingValue) и хранятся в тех же highlight_key_require_* в mod-config.json.

## Изменения форка (22.10.2026)
- Портреты больше не окрашиваются: оглушённый под зажатой клавишей попадал в портрет окрашенным. Портрет игра рисует сразу внутри RenderToTextureotron::renderCharacter, копируя материалы модели, а своих и животных Vital-Sense красит клонами материалов. Теперь на время отрисовки портрета окраска с персонажа снимается и сразу возвращается (как у Character Highlight).


Localization diagnostics / Журнал локализации (2026-11-01)
Locale and catalogue-size summaries are written only while this plugin's Detailed log is enabled. Enabling it after translations have loaded reports the existing catalogue once. Loading/format errors remain visible with detailed logging off.
Сводка языка и числа переводов записывается только при включённом подробном журнале этого плагина. Включение журнала после загрузки переводов выводит сводку существующего каталога один раз. Ошибки загрузки и формата видны и при выключенном подробном журнале.

### Icon resource lookup / Поиск картинок значков

An empty per-state icon path selects the built-in `Kenshi_UI.png` atlas image; it does not disable icons or force text-only mode. The defaults intentionally leave `unconscious_icon_texture` and `playing_dead_icon_texture` empty. Icons and labels are controlled separately by `show_icons` and `show_text`.

Пустой путь картинки состояния означает штатный значок из атласа `Kenshi_UI.png`, а не выключение значка или переход только к тексту. Значения `unconscious_icon_texture` и `playing_dead_icon_texture` по умолчанию намеренно пусты. Значки и подписи включаются отдельно настройками `show_icons` и `show_text`.

The unconscious/recovery fallback is the game's red Knockout pointer glyph (`pic_PointerKnockout`). It keeps its original colours: multiplying red artwork by a green squad colour made it nearly black. Neutral atlas masks retain relationship tint, as do the status labels; custom pictures keep their own colours. This also applies when a missing custom unconscious/recovery picture falls back to the atlas.

Запасной значок потери сознания/восстановительной комы — красный игровой указатель оглушения (`pic_PointerKnockout`). Он сохраняет исходные цвета: наложение зелёного цвета отряда делало красную картинку почти чёрной. Нейтральные маски атласа и текст состояния сохраняют цвет отношений, пользовательские картинки — собственные цвета. Правило действует и при возврате к атласу после отсутствующей пользовательской картинки этих состояний.

Custom paths retain their existing priority, including relative, mod-qualified and absolute paths supported by MyGUI. Before trying an alias, the loader checks it through MyGUI's DataManager. Kenshi registers the mod's `gui/gfx` as a GUI resource root, so `gui/gfx/heart_64px.png` can resolve as `heart_64px.png`. Missing intermediate aliases no longer produce misleading texture errors. Complete lookup failure and decoding errors remain visible, with the existing atlas fallback preserved.

Пользовательские пути проверяются в прежнем порядке, включая относительные, полные и начинающиеся с `mods/` пути, поддерживаемые MyGUI. Каждый вариант сначала проверяется через DataManager. Игра регистрирует `gui/gfx` мода как корень ресурсов GUI: `gui/gfx/heart_64px.png` может успешно загрузиться как `heart_64px.png`. Промежуточные отсутствующие варианты больше не создают ложное впечатление пропавших значков. Полное отсутствие картинки и ошибки её формата остаются видимыми; сохраняется прежний запасной значок из атласа.

The preflight uses a bounded 256-name FIFO cache and resets for a different DataManager. Already registered textures, including manual/runtime textures, remain usable without a file. Existing visual-state caches remain unchanged; no new per-frame filesystem polling or world scans are introduced. Installing/replacing assets requires restarting the game. Regression command from the forks repository: `python Vital-Read/tests/test_texture_preflight.py` (both plugins' actual loaders and preflight helper).

Проверки существования используют ограниченный FIFO-кэш на 256 имён со сбросом при смене DataManager. Уже зарегистрированные текстуры, в том числе созданные в памяти, работают и без файла. Прежний кэш состояния значков сохраняется; новых покадровых опросов файловой системы или обходов мира нет. Установка или замена картинок применяется после перезапуска игры.

## Prepared release 7 / Подготовленный выпуск 7 (2026-11-02)

English

- Readable unconscious icons. The game's coloured Knockout glyph keeps its original colours instead of turning almost black beside green squad labels. Custom pictures and neutral icon masks retain their existing behaviour.
- Optional icon compatibility. Optional custom textures are checked before loading, avoiding misleading missing-texture messages. Settings explain the standard-icon fallback.
- Optional language diagnostics. Language and translation-catalogue summaries follow this plugin's Detailed log setting, which is off by default.
- Useful errors remain visible. Translation loading and format errors are still reported with Detailed log disabled. Enabling the setting after loading reports the saved summary once without reloading translations.
- Clean update package. English, Russian and Simplified Chinese each use one compiled runtime catalogue. Personal settings and gameplay saves are preserved.

Русский

- Читаемые значки потери сознания. Цветной игровой значок оглушения сохраняет исходные цвета и больше не становится почти чёрным рядом с зелёной подписью отряда. Пользовательские картинки и нейтральные маски значков работают по-прежнему.
- Совместимость необязательных значков. Пользовательские текстуры проверяются до загрузки, без ложных сообщений об отсутствующих файлах. Подсказки настроек поясняют использование штатных значков.
- Необязательная диагностика языка. Сводки языка и каталога переводов подчиняются настройке подробного журнала этого плагина, выключенной по умолчанию.
- Ошибки остаются видимыми. Ошибки загрузки и формата переводов записываются и при выключенном подробном журнале. Включение настройки после загрузки выводит сохранённую сводку один раз без перезагрузки переводов.
- Чистый пакет обновления. Английский, русский и упрощённый китайский используют по одному скомпилированному рабочему каталогу. Личные настройки и игровые сохранения сохраняются.
