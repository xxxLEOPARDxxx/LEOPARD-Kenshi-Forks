# Character Inspector - MCM fork

[Nexus — our release / наша версия](https://www.nexusmods.com/kenshi/mods/2259)

## Version 2 / Версия 2

Environmental protection returned by CharStats is a fraction even above 1.0.
Removed the magnitude-based /100 conversion that turned 155% into a displayed
2%. The existing effective-protection display cap remains: full protection
shows 100%. Applies to acid, dust, burning, gas and rain. No per-frame work,
allocations, game-state changes or new settings are added.

Исправлена защита от среды: значения выше 100% больше не превращаются в 2%.
Полная защита отображается как 100% с прежним пределом. Исправление относится
к кислоте, пыли, опалению, газу и дождю. Настройки и состояние игры не меняются.

Validation: 22 production conversion/UI checks cover boundaries around 100%
and 150%. The old header reproduces Acid 2%; the correction displays Acid 100%.
VC10 build passed; independent review passed. Reporter live confirmation is pending.

<!-- fork-publication:start -->
## Our fork: author, sources and release history / Наш форк: автор, исходники и версии

Original author / автор оригинала: **shutori**. Thank you for the original mod! / Спасибо за оригинальный мод!
Original project / оригинальный проект: https://github.com/shutori/Shutori_Re_Kenshi_Source
Original source / исходники оригинала: https://github.com/shutori/Shutori_Re_Kenshi_Source

### Changes made by this fork / Отличия именно нашего форка

Inherited gameplay features are described in the player guide; the list below records our changes. / Исходные игровые возможности описаны в справочнике; ниже перечислены наши изменения.

**Русский**
- В MCM можно включить осмотр и кнопку инвентаря; отдельное назначение открытия убрано.
- Интерфейс и настройки поддерживают русский, английский и китайский; прежние переводы сообщества и исправления LEOPARD сохранены.
- Убраны повторные поиски и лишние перестановки кнопок; привязки очищаются при закрытии окон, осмотр закрывается при удалении персонажа.

**English**
- MCM controls enable inspection and the inventory button; the separate open binding is removed.
- Interface and settings support Russian, English and Chinese; earlier community translations and LEOPARD fixes are retained.
- Repeated button searches and unnecessary layout changes were removed; bindings are cleared on window close, and inspection closes when its character is destroyed.

### Fork release history / История выпусков нашего форка

First fork release: v1; pre-publication fixes stay in this version. / Первый выпуск форка: v1; исправления до публикации остаются в нём.

**v1**
- Добавлены MCM-переключатели осмотра и кнопки инвентаря, английский/русский/китайский интерфейс; старое назначение открытия больше не используется. Убраны повторные поиски и лишние перестановки кнопок, кэш очищается при закрытии окон, осмотр закрывается при удалении персонажа.
- Added MCM controls for the inspector and inventory button with English, Russian and Chinese text; the obsolete open binding is no longer used. Repeated button searches and unnecessary layout changes were removed, cached controls are cleared on close, and inspection closes when its character is destroyed.

<!-- fork-publication:end -->

Кнопка STATS (СТАТЫ) в инвентаре открывает осмотр частей тела, покрытия
и слоёв брони выбранного в этом инвентаре персонажа. Выбор части тела
показывает подробности её защиты. Отдельного хоткея открытия нет.

MCM -> Character Inspector: включение, кнопка в инвентаре, подробный
журнал. Настройки применяются сразу. Debug=0 по умолчанию; при Debug=1
в RE_Kenshi_log.txt видны создание кнопки, открытие и закрытие осмотра,
персонаж и выбранная часть тела. Ошибки записываются независимо от Debug.
Старый OpenKey из личного INI игнорируется.

Локальный форк shutori с прежними исправлениями LEOPARD и переводом
сообщества. Языки: русский, английский, упрощённый китайский.
Личный CharacterInspector.ini сохраняется. Раздаётся defaults.ini.
Вторую DLL оригинала одновременно не включать. Исходник и лицензия:
ORIGIN.md, LICENSE. Пользователь подтвердил работу осмотра 29.10.2026; это не проверка всех сочетаний брони и модов.
## Current release / Текущая версия: v1 (29–30 October / октября 2026)

### English — changes in this version

- Brought the existing shutori Character Inspector and community localization into the maintained MCM fork, preserving prior LEOPARD fixes. Inspection opens through the inventory **STATS** button; no separate opening hotkey was added, and the obsolete personal `OpenKey` setting is ignored.
- Added optional **MCM → Character Inspector** settings for Enabled and Inventory button, both on by default. Changes apply immediately; disabling the inspector closes its window. Hiding the button restores the original LIMBS position.
- Cached inventory LIMBS/STATS widgets and the original button layout instead of finding and rebuilding the same layout every frame. Existing controls are repositioned or shown only when their state changes. Inventories without LIMBS are remembered until their widgets are removed.
- Added widget unlink and close-window cleanup for the cached controls. Destroying the inspected character closes the inspector before its pointer can be reused. Closing inventories/windows clears the tracking state. Retained the earlier stable source-inventory behavior, so opening two character inventories does not make the inspected target flicker between them.
- Maintained **English, Russian and Simplified Chinese** (`en_GB`, `ru_RU`, `zh_CN`) for the inspector and MCM labels/help. Retained existing readable theme colors, body-part silhouette and armour-layer view.
- Added **Detailed log** as the last MCM setting, off by default. It records settings, button creation, opening/closing, inspected character and selected body part in `RE_Kenshi_log.txt`; errors are logged independently. Personal INI values are preserved; release settings use `CharacterInspector.defaults.ini`.

### Русский — изменения этой версии

- Прежний Character Inspector автора shutori и перевод сообщества перенесены в поддерживаемый MCM-форк с сохранением прежних исправлений LEOPARD. Осмотр открывается кнопкой **STATS / СТАТЫ** в инвентаре; отдельного хоткея не добавлено. Старый `OpenKey` в личном INI игнорируется.
- Добавлены **MCM → Character Inspector**: включение осмотра и кнопка в инвентаре, оба пункта по умолчанию включены. Изменения действуют сразу; отключение осмотра закрывает его окно. Скрытие кнопки возвращает LIMBS на исходное место.
- Кнопки LIMBS/STATS и исходная раскладка инвентаря кэшируются, вместо повторного поиска и расчёта в каждом кадре. Положение и видимость существующих элементов меняются только при отличии. Отсутствие LIMBS запоминается до удаления виджетов этого инвентаря.
- Добавлена очистка кэша при удалении виджетов и закрытии окон. При удалении осматриваемого персонажа окно закрывается до освобождения его объекта. Закрытие инвентарей/окон очищает привязки. Сохранено прежнее закрепление осмотра за исходным инвентарём: при двух открытых инвентарях персонаж осмотра не переключается между ними каждый кадр.
- Интерфейс и подсказки MCM поддерживаются на **русском, английском и упрощённом китайском** (`ru_RU`, `en_GB`, `zh_CN`). Сохранены читаемые цвета темы, силуэт частей тела и просмотр слоёв брони.
- **«Подробный журнал»** — последняя настройка MCM, по умолчанию выключена. В `RE_Kenshi_log.txt` записываются настройки, создание кнопки, открытие/закрытие, персонаж осмотра и выбранная часть тела; ошибки пишутся независимо от этой настройки. Личные значения INI сохраняются; для выпуска используется `CharacterInspector.defaults.ini`.

## Player guide / Справочник игрока

**English:** Open a character's inventory, then press **STATS**. The window follows the character in that inventory. The overview shows body-part protection, a clickable silhouette and environmental resistance. Choose a body part to see the armour pieces covering it, coverage and cut/blunt/pierce protection; **Back** returns to the overview. Escape or the close button closes inspection. Inventory and equipment remain interactive.

Displayed combined protection is an **estimate**: the current model sums resistance × coverage for each layer and averages the body-part totals for the overview. It is not a prediction of the exact damage from the next hit. Environmental protection is read for the character as a whole, rather than invented for individual limbs. Reopening the view or selecting a part refreshes the equipment data.

**Русский:** Откройте инвентарь персонажа и нажмите **СТАТЫ / STATS**. Осмотр относится к персонажу этого инвентаря. Общий вид показывает защиту частей тела, нажимаемый силуэт и защиту от среды. Выберите часть тела для просмотра покрывающей её брони, покрытия и защиты от режущего, дробящего и пробивающего урона. **«Назад»** возвращает общий вид; Escape или крестик закрывает осмотр. Инвентарь и снаряжение остаются доступными.

Общая защита — **приближённая оценка**: текущая модель складывает сопротивление × покрытие каждого слоя, затем усредняет результаты частей тела для общего вида. Это не точный прогноз урона следующего удара. Защита от среды берётся для персонажа целиком, а не рассчитывается отдельно для каждой конечности. Повторное открытие или выбор части тела обновляет сведения о снаряжении.

## Settings / Настройки

Options / Настройки → MCM → **Character Inspector**. MCM is optional / необязателен. Without MCM, edit / без MCM редактируйте `CharacterInspector.ini` beside the DLL / рядом с DLL:

```ini
[CharacterInspector]
Enabled=1
InventoryButton=1
Debug=0
```

No option requires a restart. / Ни одна из этих настроек не требует перезапуска. Requires RE_Kenshi/KenshiLib; do not enable the original inspector DLL together with the fork. / Требуется RE_Kenshi/KenshiLib; не включайте DLL оригинала одновременно с форком. The release folder is **Character Inspector**, while the DLL/INI base name is **CharacterInspector**. / Папка пакета — **Character Inspector**, основное имя DLL/INI — **CharacterInspector**. Runtime catalogs are `.mo`; editable `.po` catalogs remain in source. / В пакете рабочие `.mo`, редактируемые `.po` остаются в исходниках.

## Source and credits / Исходники и авторство

Original author / автор оригинала: **shutori** — [Shutori_Re_Kenshi_Source](https://github.com/shutori/Shutori_Re_Kenshi_Source), imported upstream commit `fa558095a92366484c4775e3579fefa750e37cf8`. Fork integration and fixes / интеграция и исправления форка: **LEOPARD**. Existing community translations are retained / прежний перевод сообщества сохранён. See [ORIGIN.md](ORIGIN.md) and [LICENSE](LICENSE), GNU GPL v3. Native foundation / основа: RE_Kenshi/KenshiLib; localization / локализация: Asur's shared `Localization.h`.

Source modules: `src/InventoryStatsButton.cpp` (inventory controls), `StatsOverviewUI.cpp` / `BodySilhouette.cpp` (inspection UI), `GameProtectionAdapter.cpp` / `ProtectionModel.cpp` (protection data), `InspectorSettings.cpp` (MCM). Build with the development stand's `build.ps1 -SrcDir <this directory>\src -Name CharacterInspector` using its compatible VC10 x64 / KenshiLib environment.


Localization diagnostics / Журнал локализации (2026-11-01)
Locale and catalogue-size summaries are written only while this plugin's Detailed log is enabled. Enabling it after translations have loaded reports the existing catalogue once. Loading/format errors remain visible with detailed logging off.
Сводка языка и числа переводов записывается только при включённом подробном журнале этого плагина. Включение журнала после загрузки переводов выводит сводку существующего каталога один раз. Ошибки загрузки и формата видны и при выключенном подробном журнале.
