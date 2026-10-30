# Stack Sort — MCM fork, v1 (unreleased)

Original **StackSort** by **Jakematt2004**. Thank you for the original packing
solver, inventory controls, background preparation and Rotate Items support.

- Original mod: https://steamcommunity.com/sharedfiles/filedetails/?id=3813810734
- Original source: https://github.com/jacobericson/StackSort
- Fork source: https://github.com/xxxLEOPARDxxx/LEOPARD-Kenshi-Forks
- License: GNU GPL version 3; see LICENSE and ORIGIN.md for the source revision.

The original author's features are retained: close packing, merging split
stacks on the first sort, cycling through the five best arrangements, H/W
free-space goals, arrow controls, right-click position undo, background
calculation on opening an inventory and cached results on reopening. Trader
stock keeps its original sorting controls. Up to 16 visible inventory panels
and 64 cached sections are supported.

## What this fork changes

- MCM settings and Russian, English and Simplified Chinese interface text.
- Stack consolidation uses the game's full native insertion path. Empty
  source stacks are removed correctly; items with incompatible type, quality,
  material, manufacturer or ownership remain separate under native rules.
  Charge transfer follows the game's own compatibility and arithmetic. Partial
  medical/repair items can combine even at their item-count limit: two half-used
  kits may become one full kit while preserving all usable charges.
- Single-cell storage, sections limited to one item and single-resource
  storage whose footprint only permits one stack keep their original controls.
- Backpack controls form one row centered under the inventory grid. With
  KEO, its backpack-priority checkbox joins that row; its selection and click
  behavior stay intact, and its position is restored on close or disable.
  Other neighboring widgets keep their positions. Narrow panels use a compact caption or `[N]`
  with a tooltip. An exceptionally narrow row retains the game's Arrange button.
- Rotate Items integration is enabled in the build. Actual item dimensions
  are used; the optional native C API changes orientation and refreshes icons.
  Either mod-list order is supported: StackSort retries detection when opening
  an inventory or requesting a sort if Rotate Items was not loaded at startup.
  With no compatible API, existing rotated items retain their actual
  dimensions and the sorter introduces no new rotations.
- Background jobs are cancelled and stale cache generations rejected before
  new-game, load-game and world reset. Target memory is cleared between worlds.

## Controls

`[ < ] [ Sort [N] ] [ H/W ] [ > ]`

Left-click Sort to cycle the best arrangements. H reserves a free
rectangle with the selected height; W reserves one with the selected width.
The arrows decrease/increase N. Right-click Sort once to restore item
positions captured **after first-click stack consolidation**. It does not split
merged stacks. Adding/removing items invalidates the undo capture. The H/W
button changes the active direction during play. Target choices last for the
current world and are remembered separately for body, backpack and container.

## Requirements and installation

Requires RE_Kenshi/KenshiLib; optional Mod Configuration Menu for in-game
settings and KenshiRotate (Rotate Items) for new rotations. Replace the original
StackSort installation with this fork; run one StackSort DLL. Place
`StackSort.dll`, default `StackSort.ini`, `StackSort.cfg`, the mod marker and
`locale/<language>/LC_MESSAGES/stack_sort.mo` under `mods/StackSort`.
Distribute MO catalogs only; PO files are translation sources.

## Settings

MCM exposes Enable Stack Sort, Merge split stacks, starting H/W direction,
number of solutions, background worker count and Detailed log. Enable, merge,
solution count and logging apply live. Starting direction and worker count
are initialized at plugin startup and carry MCM's restart marker. Detailed log
is last and defaults off; errors always appear in RE_Kenshi_log.txt.

`StackSort.cfg` retains the original advanced solver tuning and button geometry.
MCM INI values take precedence for settings exposed in MCM. Search budgets and
geometry in CFG are read at startup. Without MCM, edit the same INI/CFG files.

## Developer reference and verification

Build with the VC10 x64 stand and **STACKSORT_ENABLE_ROTATION**. Run
`build_check.ps1` for the isolated StackSortCheck DLL, and
`tests/build_tests.ps1` for the native-free C++ tests. Compile runtime catalogs
with bundled Python `tests/compile_locales.py --output <build-folder>`.
No modern C++ standard-library objects cross the game ABI.

Worker code sees copied packer values and opaque cache keys. Native inventory,
item, MyGUI and Ogre reads happen on the game thread. Publishing takes the
cache entry lock and checks abort, generation and slot bounds. Close/destroy
cancels a panel's jobs; world lifecycle hooks cancel every job and invalidate
entries without deleting nodes active workers may hold. Custom sorting stays
disabled if the required world lifecycle hooks fail.

Native Steam 1.0.65 audit: `InventoryItemBase::addQuantity` at 0x75D6E0 only
changes quantity/charge fields. `InventorySection::addItem` at 0x74BD70 performs
native compatibility, callback and source cleanup. Its incoming count is
`item.quantity + amount - 1`: reinsert an entire existing stack with **amount
1**. Accepted calls may consume the source; never dereference it afterwards.
False/throw recovery resolves the captured handle and derives uncredited
quantity/charges from a ledger allocated before detachment, because the native
call temporarily sets source.quantity to 1. A fault stops the current sort;
it does not run a second vanilla sort over a partial operation. Unexpected
third-party mutation cannot be safely guessed and is logged as a fault.

Rotate API v1: ApiVersion, IsRotated, CanRotate, SetRotated and optional
RefreshVisuals exports from KenshiRotate.dll. SetRotated receives a NULL GUI
only after removal and before placement. No shared GameData is mutated.
Without the API, orientation-exact identities prevent stale layouts from
overwriting a player's manual rotation. With it, canonical dimension identities
allow cached layouts to reconcile orientation through the native API.

Automated checks cover the actual native adapter against refusal, partial
credit, source consumption and throws during native source normalization;
item compatibility models; actual solver rotations and existing rotations
without the API; cancellation, cache generation invalidation and target reset.
These are model/solver checks, not a claim of in-game acceptance. Remaining
game checks: mixed quality/material/stolen/manufacturer/charge stacks; cactus
and iron storage; original and Dark UI layouts; close/reopen/load/new-game;
undo, rotated icon visuals and rotation persistence after saving/loading.

## v1 changes (unreleased)

- Retain the original sorter and controls with MCM and RU/EN/ZH localization.
- Fix split-stack cleanup and restricted-storage controls.
- Enable optional Rotate Items support and cancel stale work across worlds.

---

# Stack Sort — форк с MCM, v1 (не опубликован)

Оригинальный **StackSort**, автор **Jakematt2004**. Спасибо автору за алгоритм
упаковки, кнопки инвентаря, фоновую подготовку и поддержку Rotate Items.

- Оригинальный мод: https://steamcommunity.com/sharedfiles/filedetails/?id=3813810734
- Исходники оригинала: https://github.com/jacobericson/StackSort
- Исходники форка: https://github.com/xxxLEOPARDxxx/LEOPARD-Kenshi-Forks
- Лицензия: GNU GPL версии 3. Ревизия исходников указана в ORIGIN.md.

Сохранены возможности автора: плотная раскладка, объединение разделённых стопок
при первой сортировке, перебор пяти лучших вариантов, свободный прямоугольник
заданной высоты/ширины, стрелки, отмена позиций по ПКМ, фоновый расчёт при
открытии и повторное использование результатов. Склады торговцев сохраняют
штатные кнопки. Поддерживаются 16 открытых панелей и 64 раздела в кэше.

## Что изменено в нашем форке

- Настройки MCM и интерфейс на русском, английском и упрощённом китайском.
- Объединение через штатное добавление предмета правильно удаляет опустевшую
  исходную стопку. Несовместимые типы, качество, материалы, изготовители и
  владельцы не смешиваются по игровым правилам. Заряды объединяет сама игра:
  два наполовину использованных набора могут стать одним полным даже при
  ограничении стопки одним предметом; все оставшиеся заряды сохраняются.
- Одноячеечные разделы, ограничение одним предметом и ресурсные хранилища,
  допускающие только одну стопку, сохраняют штатное управление.
- Кнопки рюкзака центрируются одним рядом под сеткой предметов.
  При наличии KEO его галочка приоритета рюкзака входит в этот общий ряд;
  выбор и нажатие сохраняются, исходная позиция возвращается при закрытии
  или отключении. Остальные соседние виджеты остаются на месте.
  В узких панелях подпись сокращается до «Сорт.» или `[N]` с
  подсказкой. Если даже минимальные четыре кнопки не помещаются, остаётся
  штатная «Разложить».
- Поддержка Rotate Items включена при сборке: учитывается фактический размер
  каждого предмета. Порядок этих двух модов в списке не важен: если Rotate Items
  ещё не загрузился при старте StackSort, проверка повторяется при открытии
  инвентаря или запросе сортировки. Без подходящего API существующие повороты сохраняются,
  новые повороты сортировщик не вводит.
- Новая игра, загрузка и сброс мира отменяют фоновые задания и сбрасывают
  старые результаты и запомненные цели.

## Управление

`[ < ] [ Сортировать [N] ] [ В/Ш ] [ > ]`

ЛКМ перебирает лучшие варианты. В оставляет свободный прямоугольник выбранной
высоты, Ш — ширины. Стрелки уменьшают/увеличивают N. ПКМ однократно возвращает
позиции **после объединения стопок первым нажатием**; стопки обратно не
разделяются. Добавление/удаление предметов отменяет снимок для возврата.
Кнопка В/Ш меняет направление в игре. Цели запоминаются до смены мира отдельно
для основного инвентаря, рюкзака и контейнера.

## Установка и совместимость

Нужен RE_Kenshi/KenshiLib. MCM необязателен, но даёт настройки в игре.
KenshiRotate (Rotate Items) необязателен и разрешает новые повороты. Этот форк
заменяет оригинальный StackSort: должна работать одна DLL сортировщика.
В `mods/StackSort` нужны StackSort.dll, стандартный INI, CFG, маркер мода и
MO-каталоги трёх языков. PO остаются в исходниках.

## Настройки и справочник

MCM: включение, объединение стопок, начальное В/Ш, число вариантов, фоновые
потоки, подробный журнал. Включение, объединение, число вариантов и журнал
применяются сразу. Начальный режим и число потоков читаются при запуске и
помечены MCM как требующие перезапуска. Журнал последний и по умолчанию
выключен; ошибки записываются всегда. В StackSort.cfg остались дополнительные
настройки поиска и расположения кнопок. INI имеет приоритет для настроек MCM.
Без MCM можно редактировать те же INI/CFG.

Фон работает только с копиями чисел и данных решателя. Предметы, инвентари и
интерфейс читаются в игровом потоке. Поколения кэша и отмена не дают применить
старое решение. При отказе обязательных хуков смены мира сортировка отключена.
Штатное addQuantity меняет только количество/заряды, но не удаляет исходную
стопку. Форк отделяет её и возвращает через addItem с параметром 1, чтобы не
удвоить количество. После поглощения исходный указатель не используется.
При отказе остаток восстанавливается по handle и заранее снятым количествам
и зарядам целевых стопок; текущая сортировка останавливается. Неожиданные
изменения стороннего плагина не угадываются и записываются как ошибка.
Подробности адресов, C API Rotate и сборки находятся в английском справочнике
выше и исходниках. Общие GameData поворотами не изменяются.

Проверки: `build_check.ps1`, `tests/build_tests.ps1`, генератор каталогов
`tests/compile_locales.py --output <папка-сборки>`. Это модельные проверки
нативных отказов и настоящего решателя, а не игровая приёмка. В игре ещё нужны
смешанные стопки, кактус/железо, оригинальный и Dark UI, ПКМ, повёрнутые
значки, закрытие/открытие, новая игра и сохранение/загрузка с поворотами.

## Изменения v1 (не опубликован)

- Исходная сортировка и управление дополнены MCM и переводами RU/EN/ZH.
- Исправлены очистка объединённых стопок и кнопки ограниченных хранилищ.
- Включена поддержка Rotate Items и отмена старых заданий при смене мира.
