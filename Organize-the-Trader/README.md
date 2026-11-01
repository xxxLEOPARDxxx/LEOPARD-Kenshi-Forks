# Organize the Trader - LEOPARD fork

Original mod: [Organize the Trader](https://www.nexusmods.com/kenshi/mods/1886) by **Emkej8**.
Thank you to Emkej8 for the original search and sorting mod.
**Original source code:** https://github.com/Emkej/Organize-the-Trader
License: GPLv3; see LICENSE. KenshiLib is also GPLv3.

Оригинальный мод: [Organize the Trader](https://www.nexusmods.com/kenshi/mods/1886), автор **Emkej8**.
Спасибо Emkej8 за оригинальные поиск и сортировку.
**Исходники оригинала:** https://github.com/Emkej/Organize-the-Trader
Лицензия: GPLv3, файл LICENSE. KenshiLib также распространяется по GPLv3.

## Current player guide / Актуальное руководство

Search the trader's items by typing in the field at the bottom. `/` or `Ctrl+F` focuses search; the clear button resets it. The preset list sorts by name, unit price, stack value, unit weight, stack weight, or price/weight; the adjacent direction button reverses the order. `Ctrl+Shift+F8` toggles the controls. `Ctrl+Shift+F9` writes a diagnostic snapshot.

The upper strip shows the trader's money on the left, the current deal in the centre, and the player's shared money on the right. The deal is the trader's starting money minus the current money for this window: a plus is a gain for the player; a minus is a cost. The background and text follow the active UI skin. It does not change any money or transaction. Disable only the deal in MCM if desired. Native money formatting follows the game's locale.

Widths follow the translated captions, preset names, counter, and active font. A narrow window places search and sorting on separate rows and reserves space by reducing the scroll viewport. Controls share the same height and vertical alignment. Extremely large fonts that cannot fit even a separate full-width row first omit the redundant Sort label and reduce gaps, then use a smaller font for the affected caption (at least75% of its original size and at least12px, without enlarging an originally smaller font). Their original font returns when space is available. In a pathological window still too narrow at that readable floor, the caption is bounded to its control and the full text is available in the preset popup or a native tooltip; full inline visibility is not claimed for that case. Manual widths in MCM are preferences; readable text takes priority. The inventory cell layout and item interactions remain native.

Настройки находятся в MCM. Печатайте название в нижнем поле поиска; `/` или `Ctrl+F` переводит в него курсор, кнопка очистки сбрасывает запрос. Пресеты сортируют по имени, цене за штуку, стоимости стопки, весу за штуку, весу стопки и цене/весу. Кнопка рядом меняет направление. `Ctrl+Shift+F8` включает и выключает панели; `Ctrl+Shift+F9` записывает диагностический снимок.

Наверху слева показаны деньги торговца, по центру - итог текущей сделки, справа - общие деньги игрока. Итог равен деньгам торговца при открытии окна минус его текущие деньги: плюс означает доход игрока, минус - расход. Фон и цвет текста следуют текущему оформлению интерфейса. Суммы и сделки мод не меняет. В MCM можно скрыть только итог сделки. Формат денег берётся у игры.

Ширины зависят от переведённых подписей, пунктов списка, счётчика и текущего шрифта. В узком окне поиск и сортировка занимают отдельные строки; для них уменьшается область прокрутки предметов. У полей и кнопок одинаковая высота и вертикальное выравнивание. Если очень крупный шрифт не помещается даже в отдельную строку на всю ширину, сначала убирается необязательная подпись сортировки и уменьшаются интервалы, затем размер соответствующего текста можно уменьшить не ниже75% исходного и12px (исходно меньший шрифт не увеличивается). При расширении исходный размер возвращается. Если патологически узкое окно тесно и при этом пороге, текст ограничивается своим полем, а полная подпись доступна во всплывающем списке или штатной подсказке; полная видимость в строке для такого случая не заявляется. Ручные ширины в MCM - желаемые значения, читаемый текст важнее. Ячейки инвентаря и взаимодействие с предметами остаются штатными.


## Prepared release 7 / Подготовленный выпуск 7

EN: Restored the player wallet on the right at the user's request (2026-11-01), alongside merchant money on the left and the optional deal in the centre. All three labels use the real native money plate, font and XML text palette; the deal amount retains Good/Bad accents. Long amounts in a narrow window use the existing extra-row layout instead of overlapping. The wallet is resolved through a fresh actual player character, never an inspected NPC. No native Character pointer is cached and no money is modified. Unchanged frames reuse the numeric/caption/layout caches; all three widgets are owned and removed together, including partial creation failure.

RU: По запросу пользователя от 01.11.2026 возвращены деньги игрока справа: деньги торговца остаются слева, отключаемая сделка - по центру. Все три подписи используют настоящую денежную плашку, шрифт и XML-палитру; у суммы сделки сохранены цвета Good/Bad. Длинные суммы в узком окне переходят на дополнительные строки без наложения. Кошелёк читается через заново полученного персонажа игрока, а не через осматриваемого NPC. Указатель персонажа не сохраняется, деньги не изменяются. Неизменные кадры используют кэш чисел, подписей и раскладки; все три виджета удаляются вместе, в том числе при частичной ошибке создания.

Validation: 200 extracted-production header/style/lifecycle checks (vanilla, Dark UI and asymmetric XML roles), 27709 layout assertions, 16 native-text/cache checks, installed layout/packing/rollback tests and the exact compiled virtual wallet dispatch/import audit. Independent review found no production issue; an incorrect test requirement for a virtual getMoney DLL import was replaced with the emitted vtable-call check. Real game layout, wallet changes during a sale and quantitative FPS/long-memory acceptance remain pending for this candidate. Published version 6 is unchanged; this preparation is version 7.

## Prepared changes / Подготовленные изменения - 30.10.2026

- EN: Replaced the half-window split with measured text allocation, equal control heights, separate rows when needed, and safe viewport restoration after resize, disable, close, and world replacement.
- RU: Деление окна пополам заменено распределением по фактической длине текста; выровнены высоты, добавлен перенос строк и восстановление области предметов после изменения размера, отключения и закрытия окна.
- EN: Placed merchant money on the left and centred the optional deal. The native trader caption remains the source for the merchant wallet; the separate player wallet is omitted. Unknown layouts with a money panel directly under Content keep their native header rather than hiding the entire content.
- RU: Деньги торговца размещены слева, итог сделки выровнен по центру. Сумма торговца берётся из штатной подписи; отдельная сумма игрока не выводится. Неизвестная разметка, где денежная панель находится прямо в Content, сохраняет штатный верхний блок: весь Content не скрывается.
- EN: Fixed the GOG setup binding: the old GOG address selected AnimalInventoryLayout::setupSections, so activation depended on first opening an animal inventory. Character and trader setup targets now resolve through the installed KenshiLib exports and the active platform table; no nearest-address guess is used.
- RU: Исправлена привязка GOG: старый адрес указывал на AnimalInventoryLayout::setupSections, поэтому работоспособность зависела от первого открытия инвентаря животного. Цели персонажа и торговца теперь берутся через экспорты установленной KenshiLib и таблицу текущей платформы; подбор ближайшего адреса исключён.
- EN/RU: Updated all three MO catalogs and MCM help: en_GB, ru_RU, zh_CN. / Обновлены три каталога MO и подсказки MCM: en_GB, ru_RU, zh_CN.

## Native correction after first UI trial / Исправление после первой игровой проверки

The first adaptive candidate (SHA4310395...) failed visual acceptance: all three money labels vanished and the longest preset still clipped. The saved read-only runtime snapshot RUNTIME-29460-OTT-READONLY.json showed valid captions, visible widgets and alpha1 through their parents, but font height0 and RGBA(0,0,0,0) on all three new labels. The source money EditBox and ComboBox had no root main text. Their inherited nonvirtual TextBox style getters therefore returned empty/zero fallbacks even though the separate Client text was visible (the combo Client used Kenshi_StandardFont_Medium at21px). The earlier arithmetic checks did not model this composed-widget contract and did not constitute visual acceptance.

The correction follows the public Client chain, bounded to four widgets, and reads the actual ISubWidgetText font/height/colour. It never reads private MyGUI offsets in production. A missing or non-visible source style leaves the native money header visible. The accepted font/footer behaviour is retained; the current header has merchant money and deal only. Presets are measured by the native text renderer in that real font, with the actual closed/popup text-region padding. A hidden owned measuring TextBox is created once; separate closed/popup caches refresh only for a changed font, height or translated preset list. Resizing and unchanged frames reuse those measurements. Native measurement/create/caption counters are available to the matching read-only diagnostic probe. Footer/header widgets and measuring widgets remain unlink-safe and are cleared on teardown.

Первый адаптивный кандидат (SHA4310395...) не прошёл визуальную приёмку: три денежные подписи пропали, длинный пресет оставался обрезан. Сохранённый read-only снимок RUNTIME-29460-OTT-READONLY.json показал правильные суммы, видимые виджеты и alpha1 у них и их родителей, но fontHeight0 и RGBA(0,0,0,0) у трёх новых подписей. У денежного EditBox и ComboBox основной текст корневого виджета отсутствовал. Унаследованные невиртуальные getters TextBox возвращали пустые/нулевые значения вместо стиля отдельного Client (у combo Client был Kenshi_StandardFont_Medium21px). Прежние арифметические проверки не моделировали этот контракт составного виджета и не заменяли игровую проверку.

Исправление следует публичной цепочке Client (не больше четырёх виджетов) и берёт шрифт, размер и цвет настоящего ISubWidgetText. Приватных смещений MyGUI в runtime-коде нет. Если нужный стиль не найден или не виден, штатный денежный блок сохраняется видимым. Принятые механики шрифта и нижней панели сохранены; в актуальном верхнем ряду остаются только деньги торговца и сделка. Ширина пресетов берётся у штатного текстового рендера с настоящим шрифтом и фактическими отступами поля/списка. Скрытый измерительный TextBox создаётся один раз; отдельные кэши поля и списка пересчитываются только при изменении шрифта, его высоты или переведённых пунктов. Изменение ширины окна и неизменные кадры используют готовые размеры. Для соответствующего read-only probe доступны счётчики создания и измерений. Ссылки на виджеты безопасно обнуляются MyGUI, временные элементы удаляются при закрытии.

## Accepted UI and final header adjustment / Приёмка и уточнение верхней строки

The user confirmed the native-font candidate7c629088: money and presets are visible. Its controlled Native capture reported opaque21px styles, full preset/client widths and stable measurement counters. The subsequent user request removes the separate player wallet entirely: only merchant money on the left and the optional deal in the centre remain. The player label, native Character::getMoney/getAnyPlayerCharacter calls and related cache fields are removed. The initial header caption counter now records two writes, one per header label; unchanged frames still add none. Preset measurements, fonts, footer layout, configuration and MO catalogs are unchanged. This is still pending release6, not another version bump.

Пользователь подтвердил кандидата7c629088 с исправленным Native-контрактом: суммы и пресеты видны. Контролируемый Native-снимок показал непрозрачный шрифт21px, достаточную ширину пресетов/клиентов и стабильные счётчики измерений. После этого пользователь попросил полностью убрать отдельные деньги игрока. Остаются только деньги торговца слева и отключаемая сделка по центру. Удалены подпись игрока, вызовы Character::getMoney/getAnyPlayerCharacter и связанные поля кэша. Начальное заполнение верхних подписей теперь даёт две записи, по одной на каждую; неизменные кадры их не добавляют. Принятые шрифты, измерения пресетов, нижняя строка, конфигурация и каталоги MO не менялись. Это всё ещё подготовленный выпуск6, без нового повышения версии.

## Implementation and verification notes

The footer and money text use unlink-aware WidgetRef references. Font measurements, formatting, and geometry updates run only when content/font/size/settings change. The scroll viewport adapter tracks the native baseline and its last written rectangle separately: normal MyGUI alignment projects both on resize; an explicit native geometry reset supplies a new baseline. Zero-height viewports are temporarily hidden and restored when they regain space. The original money plate keeps its real skin and opacity; only its old data-panel contents become transparent while remaining updateable. Teardown restores the latest native plate rectangle and data-panel opacity and removes the LEOPARD_OTT_MoneyHeader marker. UITweaks yields only that marked data panel. Both custom header widgets must exist before this ownership is acquired.

### Native skin inheritance correction / Исправление наследования оформления

EN: The native money plate and inventory viewports can be nested under the window's generated Content container. Header placement and reserved inventory space use each widget's actual parent; close/resize/reparent restore is scoped to that live window. The binding search is bounded and does not descend into item icons. This fixes the native “Cash” fallback and missing deal balance on both vanilla and Dark UI. The user verified header and inventory placement in both vanilla and Dark UI. The subsequent Good/Bad deal-amount colour correction still awaits visual acceptance.

RU: Денежная плашка и область товаров могут находиться внутри созданного игрой контейнера Content. Заголовок и место под него рассчитываются относительно настоящего родителя каждого элемента; восстановление при закрытии, изменении размеров и смене родителя ограничено живым окном. Поиск ограничен и не заходит в значки предметов. Это исправляет возврат штатного «Нал.» и исчезновение сделки в ванильном и Dark UI. Пользователь проверил положение заголовка и товаров в ванильном и Dark UI. Последующее восстановление цвета суммы сделки из Good/Bad ещё ожидает визуальной проверки.

EN: A vanilla screenshot exposed a background regression hidden by Dark UI's empty money container. Fading the entire native plate removed the vanilla white frame, while copying the input field's dark foreground onto independent text left unreadable text on the window. The correction retains the actual live plate and its XML skin, places both labels inside it, and inherits their own loaded standard-text colour/shadow with Good/Bad accents for the deal amount from the active XML palette. Native data-panel padding and ordinary Stretch alignment are respected; the native client font and all preset/search measurement caches remain in use. Only the two existing amounts are displayed. Missing styles or partial creation restore the original header and suppress repeated allocation attempts for that live panel. Close/disable restores native geometry/opacity; an external reparent never receives coordinates from the old parent. This remains prepared release6, with vanilla/Dark visual and sale/reopen acceptance pending.

RU: Ванильный скриншот выявил исчезновение фона, которое скрывал пустой денежный контейнер Dark UI. Прозрачность всей штатной плашки убирала белую рамку, а скопированный тёмный цвет поля ввода делал отдельные надписи нечитаемыми на фоне окна. Исправление сохраняет настоящую живую плашку и её XML-скин, помещает обе подписи внутрь и оставляет их собственные загруженные цвет/тень стандартного текста, с цветом суммы сделки из ключей Good/Bad активной XML-палитры. Учитываются штатные отступы datapanel и обычное растяжение; сохраняются настоящий шрифт клиента и кэши измерения пресетов/поиска. Показаны только две прежние суммы. Недоступный стиль и частичное создание возвращают исходный заголовок без повторных попыток выделения виджетов для этой живой панели. Закрытие/отключение восстанавливает геометрию и прозрачность; при чужом переносе в другого родителя старые локальные координаты не применяются. Это всё ещё подготовленный выпуск6; визуальная приёмка vanilla/Dark и продажа/повторное открытие остаются непроверенными.

Native evidence: installed KenshiLib setup exports map to Character slot2019 (Steam0x14EEA0, GOG0x14EDC0), Animal slot600 (Steam0x14F530, GOG0x14F450), Trader slot6959 (Steam0x150530, GOG0x150450). The old hard-coded GOG0x14F450 was the Animal target. The separate player-wallet label and its native wallet getter have been removed at the user's request. InventoryGUI::updateDatapanel Steam0x70C1F0 updates through its pointers without testing alpha. See tests/NATIVE_UI_EVIDENCE.md and tests/CheckNativeContract.py.

Production build: `D:/DEV/Kenshi/ours/build.ps1 -SrcDir D:/DEV/Kenshi/forks/LEOPARD-Kenshi-Forks/Organize-the-Trader -Name Organize-the-Trader -FromVcxproj` (VC10 x64, /GL + /LTCG). Offline tests cover arithmetic, native-baseline lifecycle, mixed viewports, text bounds, installed layout recognition, packing/rollback, Cyrillic search, and installed DLL imports. No test calls a function inside the running game.

These are offline checks on Steam1.0.65 and official Steam/GOG RVA tables. Native visual acceptance, sales/refreshed captions, human/animal switches, close/reopen and world reload still require an in-game run. GOG runtime acceptance has not been performed. Historical notes below describe earlier versions and are superseded by this guide where they differ. Debug logging stays the last MCM setting and defaults to off; errors are always logged.

## Setup
Clone normally. Shared build scripts are tracked in `tools/build-scripts` via `git subtree`, so no submodule init step is required.

1) Open a PowerShell terminal in this repo.
2) (Optional) Create `.env` from `.env.example` to set local paths.
3) Source the env script:
   - `. .\scripts\setup_env.ps1`

This sets:
- `KENSHILIB_DEPS_DIR`
- `KENSHILIB_DIR`
- `BOOST_INCLUDE_PATH`

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
Mod data folder name: `Organize-the-Trader`

After deploy, expected files:
- `[Kenshi install dir]\mods\Organize-the-Trader\Organize-the-Trader.mod`
- `[Kenshi install dir]\mods\Organize-the-Trader\RE_Kenshi.json`
- `[Kenshi install dir]\mods\Organize-the-Trader\Organize-the-Trader.dll`
- `[Kenshi install dir]\mods\Organize-the-Trader\mod-config.json`

## Config
`mod-config.json` supports:

- `enabled` (bool): master plugin toggle.
- `showSearchEntryCount` (bool): show visible/total item entries in the search bar.
- `showSearchQuantityCount` (bool): show visible stack quantity in the search bar.
- `showSearchClearButton` (bool): show the clear button in the search bar.
- `autoFocusSearchInput` (bool): auto-focus the search input when trader controls are injected.
- `debugLogging` (bool): master runtime debug switch for support diagnostics.
- `debugSearchLogging` (bool): enables search-path diagnostics when `debugLogging` is enabled.
- `debugBindingLogging` (bool): enables inventory-binding diagnostics when `debugLogging` is enabled.
- `searchInputWidth` (int): desired search input width in pixels, clamped at runtime.
- `searchInputHeight` (int): desired search input height in pixels, clamped at runtime.
- `searchInputPositionCustomized` (bool): when `true`, reuses the saved drag position instead of the default placement.
- `searchInputLeft` (int): saved left coordinate for the search bar inside the trader window.
- `searchInputTop` (int): saved top coordinate for the search bar inside the trader window.

Default:

```json
{
  "enabled": true,
  "showSearchEntryCount": true,
  "showSearchQuantityCount": true,
  "showSearchClearButton": true,
  "autoFocusSearchInput": true,
  "debugLogging": false,
  "debugSearchLogging": false,
  "debugBindingLogging": false,
  "searchInputWidth": 372,
  "searchInputHeight": 26,
  "searchInputPositionCustomized": false,
  "searchInputLeft": 0,
  "searchInputTop": 0
}
```

## Mod Hub (Optional)
If `Emkejs-Mod-Core` is loaded, Organize-the-Trader registers its user-facing settings in Mod Hub (`enabled`, search counts, clear button, auto-focus search, search width, search height) and writes committed changes back to `mod-config.json`.

Recommended load order:
- `Emkejs-Mod-Core`
- `Organize-the-Trader`

If Mod Core is absent, registration fails, or the helper retry path never attaches, this mod keeps using `mod-config.json` only.

### Runtime smoke check
After launching Kenshi with `Emkejs-Mod-Core` and `Organize-the-Trader` enabled, run:
- `.\scripts\phase22_mod_hub_runtime_smoke_test.ps1 -ExpectedMode attached`

The script reads the latest `RE_Kenshi_log.txt` session and passes only when the latest Organize-the-Trader startup reaches `event=mod_hub_attached`, which proves Mod Hub lookup and setting registration succeeded for that run. If you see `event=mod_hub_attach_retry_pending`, the helper-owned retry path is still pending and that run is not a clean attached pass yet.

## License
This project is licensed under the GNU General Public License v3.0.
It uses KenshiLib, which is released under GPLv3.

## Изменения форка (ветка leopard)
- Настройки - во вкладке MCM окна «Настройки»; Mod Hub не нужен.
- Производительность: панели поиска и сортировки и пристыкованная панель запоминаются при создании (shared/WidgetRef.h) вместо поиска по имени через весь интерфейс - до восьми полных обходов за кадр. Окно торговли, пока панели нет, ищется 10 раз в секунду.
- Поиск: найденные вещи собираются в начало сетки, скрытые поиском плотно занимают оставшееся место (крупные первыми). Раньше все вещи ставились полками с пустым рядом между ними, у торговца с сотней вещей полки не влезали, и раскладка отменялась целиком - найденное оставалось разбросанным. Если раскладка всё же не удалась, причина пишется в журнал строкой `search packing skipped reason=...`.
- Найденное и отсортированное ложится плотно, без пустого ряда между полками.
- v2: со страницы MCM убраны ширина и высота поиска и сортировки (`search_input_width/height`, `sort_panel_width/height`): панели встроены в окно торговца (`InlineSearchCoord`, `PositionInlineControls`) и берут размер у места в нём, так что эти числа ни на что не влияли - игрок жаловался. Ключи в mod-config.json по-прежнему читаются, но не используются.
- v2: «Подробный журнал» в MCM (debugLogging).
- v2: подсказка в пустом поле поиска - через перевод (Tr, «Search items...» в .po); раньше была по-русски и в английской игре.
- v2: проверка «изменились ли вещи торговца» (подпись всего ассортимента) - раз в 150 мс, а не каждый кадр; при пустом поиске без сортировки проход поиска только пересчитывает счётчик, без перекладки сетки (просадка FPS при каждой продаже).
- v3: подписи сортировки ("Сорт" и семь режимов) - через перевод (Tr, en/ru/zh в .po); их зашила по-русски присланная RU-переделка, и в английской/китайской игре список был пустым (в шрифте нет кириллицы).
- v3: список сортировки на ванильном интерфейсе: строка текста у Kenshi_ComboBox прибита к верху (Client 10 5 130 20), в низком ряду сортировки текст сидел низко и обрезался - теперь растянута на всю высоту и по центру (FitSortComboText). Открытый список был шириной с поле и резал названия - теперь расширяется по самому длинному пункту (WidenOpenSortList: открытый список держит фокус клавиатуры и является дочерним виджетом поля).
- v4: настройка MCM «Ширина списка сортировки» (`sort_combo_width`, `sortComboWidth` в mod-config.json, 0-400, 0 - по месту в окне, как раньше). С заданной шириной ряд сортировки шире, игровая кнопка ArrangeButton сдвигается вправо (не за край своего окна), её место запоминается и возвращается при 0 и при уничтожении панели (RestoreArrangeButton). Минимум не поднимать: MCM шлёт каждый шаг ползунка и читает значение обратно.
- v4: английские подписи от игрока: "Sort" -> "Sort by", "By name" -> "Name". Подпись сортировки шириной по своему тексту (не меньше 52 px), остальной ряд сдвигается.

## Изменения форка (15.10.2026)
- Поиск - в нижней строке рядом с сортировкой, на месте кнопки ARRANGE; кнопка спрятана насовсем (удалять нельзя - на неё ссылается окно игры), от неё берётся только место ряда. Настройка `searchFieldWidth` (120-600, 260).
- Итог сделки рядом с деньгами торговца (`showDealBalance`): деньги торговца при открытии окна минус деньги сейчас; формат суммы - как в подписи игры. Строку денег игры не трогаем: узкая плашка (ваниль) - итог за плашкой, плашка на всё окно - за суммой. «Сделка:» - серым ванили, цветная только сумма.
- Плашка денег на всё окно: справа от неё места нет - панель создаётся в правой половине строки и сразу уходит вниз (раньше окно оставалось без поиска).
- Производительность: блок денег и ARRANGE ищутся один раз и запоминаются (WidgetRef), а не обходом всего окна каждый кадр.
- `mod-config.json` при первом запуске создаётся копией `mod-config.defaults.json` (`IniDefaults::PrepareJson`) - больше нет «could not read mod-config.json» в журнале на чистой установке.


Localization diagnostics / Журнал локализации (2026-11-01)
Locale and catalogue-size summaries are written only while this plugin's Detailed log is enabled. Enabling it after translations have loaded reports the existing catalogue once. Loading/format errors remain visible with detailed logging off.
Сводка языка и числа переводов записывается только при включённом подробном журнале этого плагина. Включение журнала после загрузки переводов выводит сводку существующего каталога один раз. Ошибки загрузки и формата видны и при выключенном подробном журнале.

## Prepared release 8 / Подготовленный выпуск 8 (2026-11-02)

English

- Optional language diagnostics. Language and translation-catalogue summaries follow this plugin's Detailed log setting, which is off by default.
- Useful errors remain visible. Translation loading and format errors are still reported with Detailed log disabled. Enabling the setting after loading reports the saved summary once without reloading translations.
- Clean update package. English, Russian and Simplified Chinese each use one compiled runtime catalogue. Personal settings and gameplay saves are preserved.

Русский

- Необязательная диагностика языка. Сводки языка и каталога переводов подчиняются настройке подробного журнала этого плагина, выключенной по умолчанию.
- Ошибки остаются видимыми. Ошибки загрузки и формата переводов записываются и при выключенном подробном журнале. Включение настройки после загрузки выводит сохранённую сводку один раз без перезагрузки переводов.
- Чистый пакет обновления. Английский, русский и упрощённый китайский используют по одному скомпилированному рабочему каталогу. Личные настройки и игровые сохранения сохраняются.
