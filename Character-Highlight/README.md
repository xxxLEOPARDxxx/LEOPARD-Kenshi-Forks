# Character Highlight (LEOPARD fork)

While the highlight key is held, characters are tinted by how they stand to
you: your squad, enemies, allies, neutrals (red to yellow to green by their
faction's relation).

## Hover highlight / Подсветка при наведении

Hover observes native characterSelected only inside mouseScan, after the game
resolves the visible character and chooses its cursor. It keeps a copied full
handle with short expiry and adds no extra ray/world scan. Raw physics hit
selection was rejected after the game test; native floor/occlusion rules win.
`cursorActionTarget` was rejected by an actual read-only game sample: it stays
empty over ordinary characters. Detailed logging reports hover hits only.

Original: **BFrizzleFoShizzle**, [KenshiLib_Examples](https://github.com/BFrizzleFoShizzle/KenshiLib_Examples)
(`CharacterHighlight`), GPLv3. This folder keeps the original commits.

## NPC names

Enable **Show NPC names** in MCM and hold the separate names key (Ctrl+N by
default). NPC names in the camera frame use the relationship colours. Your
squad keeps its normal labels. Horizontal and vertical offsets are in pixels.
The view reuses up to 96 labels and discovers candidates every 120 ms while active.
In the maintainer's in-game comparison, the name display had no noticeable FPS
drop. The mod that inspired the feature cost about 60 FPS on the same PC.

Включите **Показывать имена NPC** в MCM и удерживайте отдельную клавишу
(по умолчанию Ctrl+N). Имена NPC в кадре используют цвета отношений. У вашего
отряда остаются штатные подписи. Смещение по обеим осям задаётся в пикселях.
Плагин переиспользует до 96 подписей и ищет NPC раз в 120 мс, пока показ включён.
В игровом сравнении автора форка показ имён не дал заметной просадки FPS.
Мод, который подсказал идею, снизил FPS примерно на 60 кадров на том же ПК.

## Changes in this fork

- Settings in Mod Config Menu (Options window, MCM tab): on/off, the show key
  (ALT by default, any key or always on), see through walls, distance
  limit, colours of the squad, enemies, allies and neutrals (by relation or
  one colour). Without MCM - `CharacterHighlight.ini` (made from
  `CharacterHighlight.defaults.ini` on first start, new keys added on update).
- Text in English, Russian and Chinese (`locale/*/LC_MESSAGES/character_highlight.mo` at runtime; `.po` translation sources).
- Much less work per frame. The original, every frame, walked every
  character of the update list (hundreds in a town), asked isEnemy /
  areYouGonnaGetMe / isAlly against every player character, wrote two shader
  constants on each of them - and with the key not held wrote the "off"
  constants on everyone too. Now:
  - the relation colour is cached per character and re-evaluated once a
    second, at most 40 characters a frame;
  - the constants are written when the colour changes and every 0.5 s (in
    case something wrote over them);
  - with the key not held, the tinted characters are reset once and then
    nothing is done.
- A character with no faction no longer crashes the relation check.
- Each kind (squad, enemies, allies, neutrals) can be turned off on its own.
- Diplomats: faction diplomats get a colour of their own (purple by default),
  before any other kind - the game's `Character::isDiplomaticStatus`, the
  character's NPC class Diplomat (`StateBroadcastData::NPCType == OT_DIPLOMAT`,
  "NPC class" 5 in the character template - how mods mark their envoys,
  17.10.2026), or a squad from a "... diplomat" template.
- The portrait and dead-body fixes of the original are kept.

Vital Sense (our fork) tints after the main loop and re-applies its tint
every frame, so its downed and bounty colours stay on top of these.

## Build

VC10 + Windows SDK 7.1, the local KenshiLib SDK header snapshot and imports
matching installed KenshiLib 0.5.0, the shared headers of
[LEOPARD-Kenshi-Plugins](https://github.com/xxxLEOPARDxxx/LEOPARD-Kenshi-Plugins) (`shared/`):

    .\build.ps1 -SrcDir forks\LEOPARD-Kenshi-Forks\Character-Highlight -Name CharacterHighlight


Localization diagnostics / Журнал локализации (2026-11-01)
Locale and catalogue-size summaries are written only while this plugin's Detailed log is enabled. Enabling it after translations have loaded reports the existing catalogue once. Loading/format errors remain visible with detailed logging off.
Сводка языка и числа переводов записывается только при включённом подробном журнале этого плагина. Включение журнала после загрузки переводов выводит сводку существующего каталога один раз. Ошибки загрузки и формата видны и при выключенном подробном журнале.

## Prepared release 3 / Подготовленный выпуск 3 (2026-11-02)

English

- Optional language diagnostics. Language and translation-catalogue summaries follow this plugin's Detailed log setting, which is off by default.
- Useful errors remain visible. Translation loading and format errors are still reported with Detailed log disabled. Enabling the setting after loading reports the saved summary once without reloading translations.
- Clean update package. English, Russian and Simplified Chinese each use one compiled runtime catalogue. Personal settings and gameplay saves are preserved.

Русский

- Необязательная диагностика языка. Сводки языка и каталога переводов подчиняются настройке подробного журнала этого плагина, выключенной по умолчанию.
- Ошибки остаются видимыми. Ошибки загрузки и формата переводов записываются и при выключенном подробном журнале. Включение настройки после загрузки выводит сохранённую сводку один раз без перезагрузки переводов.
- Чистый пакет обновления. Английский, русский и упрощённый китайский используют по одному скомпилированному рабочему каталогу. Личные настройки и игровые сохранения сохраняются.

## Release 4 / Выпуск 4

### English

- Optional NPC names in relationship colours, with a separate Ctrl+N hotkey and adjustable horizontal and vertical offsets. In the maintainer's test scene, enabling names caused no noticeable FPS drop.
- Optional relationship tint when hovering over a character, with adjustable tint intensity so clothing and equipment remain visible.
- Fixed name colours when a highlight palette component exceeds the normal range.

### Русский

- Имена NPC окрашиваются по отношениям. Есть отдельное сочетание Ctrl+N и смещение по обеим осям. В тестовой сцене автора форка показ имён не вызвал заметной просадки FPS.
- Можно включить окраску по отношениям при наведении курсора. Интенсивность регулируется, чтобы одежда и экипировка оставались видны.
- Исправлены неверные цвета имён при выходе значения цвета за допустимый диапазон.
