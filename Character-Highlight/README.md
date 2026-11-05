# Character Highlight (LEOPARD fork)

While the highlight key is held, characters are tinted by how they stand to
you: your squad, enemies, allies, neutrals (red to yellow to green by their
faction's relation).

## Local candidate 4 / Локальная тестовая сборка 4 (2026-11-06)

Not published. Contour appearance and equipment coverage await in-game validation.
Не опубликовано. Внешний вид контуров и охват снаряжения проверяются в игре.

- Optional highlighting under the cursor: a beige contour, or this mod's relationship colours.
- Optional contour style for the highlight key (ALT by default) and relationship-coloured hover. Original model tint remains the default.
- Adjustable tint intensity (0-100%), contour thickness (1-6 pixels) and brightness (0-200%). Contours respect visible surfaces; the existing through-walls option applies to model tint.
- New settings apply immediately in MCM. Hover is off by default; personal colours and key bindings are retained.

- Необязательная подсветка под курсором: бежевый контур либо цвета отношений этого мода.
- Контурный стиль для клавиши подсветки (по умолчанию ALT) и наведения с цветами отношений. Привычная окраска модели остаётся стилем по умолчанию.
- Настраиваемая сила окраски (0-100%), толщина контуров (1-6 пикселей) и яркость (0-200%). Контуры учитывают видимые поверхности; прежний показ сквозь стены относится к окраске модели.
- Настройки применяются сразу через MCM. Наведение изначально выключено; личные цвета и назначения клавиш сохраняются.

### Renderer implementation notes

At most 512 actors, 4096 material clones and 24 attachment attempts per frame.
Scene enumeration is bounded to 128 nodes and 64 entities per actor; unsupported
materials are skipped until replacement. Hair alpha cards are excluded until an
alpha-aware contour variant exists. Saved SubEntity addresses are comparison
keys, never restoration targets: restoration enumerates current live objects.
Native scene access and cleanup are guarded; a renderer failure disables contours
and reports an error rather than repeatedly attempting a broken pass. Portrait
renders suspend only the corresponding actor. Shader compilation and native
import checks do not substitute for visual, FPS and memory validation in-game.

Original: **BFrizzleFoShizzle**, [KenshiLib_Examples](https://github.com/BFrizzleFoShizzle/KenshiLib_Examples)
(`CharacterHighlight`), GPLv3. This folder keeps the original commits.

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
