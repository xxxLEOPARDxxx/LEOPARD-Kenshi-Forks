# BetterLooting

<!-- fork-publication:start -->
## Our fork: author, sources and release history / Наш форк: автор, исходники и версии

Original author / автор оригинала: **XxAtreuSSxX**. Thank you for the original mod! / Спасибо за оригинальный мод!
Original project / оригинальный проект: https://github.com/XxAtreuSSxX/BetterLooting
Original source / исходники оригинала: https://github.com/XxAtreuSSxX/BetterLooting

### Changes made by this fork / Отличия именно нашего форка

Inherited gameplay features are described in the player guide; the list below records our changes. / Исходные игровые возможности описаны в справочнике; ниже перечислены наши изменения.

**Русский**
- MCM и три языка; исправления переноса с KEO и правил краденого; восстановленная раскладка Alt+Insert.
- Обязательные предметы, защита своих хранилищ и подбор с земли по близости. Подробности и настройки — ниже.

**English**
- MCM and three languages; KEO transfer and theft fixes; restored Alt+Insert deposit.
- Required items, protected player storage and nearest-first ground pickup. Details and settings are below.

### Fork release history / История выпусков нашего форка

**v4**
- Добавлены точный список имён/ID обязательной добычи, защита хранилищ игрока по умолчанию и необязательный подбор с земли, изначально выключенный. Подбор ограничен 256 результатами и 32 приказами; маршрут идёт от ближайшей вещи к следующей ближайшей без приоритета цены за вес, сохраняя прежние приказы и проверки кражи/вместимости.
- Added an exact name/ID required-item list, player-storage protection enabled by default and optional ground pickup, off by default. Ground pickup is bounded to 256 results and 32 orders and follows nearest-neighbor routing instead of value-per-weight priority, preserving existing orders and theft/capacity checks.

**v3**
- Восстановлен доступ к прежней раскладке по хранилищам: Alt+Insert кладёт всё ненадетое в ближайшее принимающее хранилище игрока. Можно ограничить раскладку правилами сбора; непоместившиеся вещи остаются у персонажа.
- Restored access to upstream storage deposit: Alt+Insert puts unequipped items into nearby player storage that accepts them. Deposit can follow loot filters; items with no room stay with the character.

**v2**
- Исправлен повторный перенос уже принятой игрой вещи, вызывавший вылет с рюкзаками KEO и объединением стопок. Краденым считается взятое у живого человека без сознания, но не с мёртвых, животных и животных-роботов.
- Fixed duplicate transfers after the game accepted an item, preventing the backpack/KEO and merged-stack crash. Taking from dead characters, animals or animal robots is no longer treated as stealing from a living unconscious person.

**v1**
- Настройки, профили и выбор клавиши перенесены из отдельного окна в MCM; интерфейс следует английскому, русскому или китайскому языку игры.
- Settings, profiles and key assignment moved from the separate window to MCM; text follows the English, Russian or Chinese game language.

<!-- fork-publication:end -->

BetterLooting is a native RE_Kenshi plugin for Kenshi.

This repository contains the corresponding source code for the distributed plugin build.

## Current release / Текущая версия: v4 (29–30 October / октября 2026)

### English — changes in this version

- Added **Always take these items**: exact full item names or FCS IDs, up to 128 entries / 8191 UTF-8 bytes. It bypasses category, minimum value and value-per-kg filters only. Theft rules, storage protection, distance, native capacity and transfer checks still apply. Profiles and All/No categories preserve this list.
- Added **Protect player storage**, enabled by default. Nearby and directly open player-owned building inventories are excluded from looting, including storage and furniture inside a player-owned building. The existing **Alt+Insert** deposit action is preserved.
- Added optional **Pick up ground items**, disabled by default. A loot-key press searches within the configured radius, examines at most 256 results and appends up to 32 temporary native pickup orders. The character walks to each item, and existing orders/jobs remain queued. Money and nonempty containers are excluded; duplicate hits in that press are removed. The full native pickup gameplay scenario still needs a separate in-game check.
- Corrected the ground-pickup route: orders are submitted for the nearest eligible item first, then the nearest remaining item from the previous target. Ground routing no longer uses value-per-weight priority, which could send the character far away before collecting items at their feet. Equal distances use item identity for a stable tie; non-finite coordinates are rejected. **Best items first** still applies to inventory looting. Existing native order flags and the 256-result / 32-order limits are unchanged; actual execution of the submitted route still needs an in-game check.
- Reused separate scratch buffers for nearby inventory, storage and ground searches; early-return cleanup and nested-scan protection prevent one scan from overwriting another. Ground searches run on the loot action, with no idle world scan.
- Preserved the earlier **KEO / merged-stack transfer fix**: once the game accepts an item, BetterLooting does not insert it again or roll it back merely because the selected inventory count did not rise. This covers the game's backpack destination choice and stack merging. Deposit remains all unequipped items by default, with an optional “Deposit only by looting rules” filter.
- Updated MCM labels and help in **English, Russian and Simplified Chinese** (`en_GB`, `ru_RU`, `zh_CN`). The full whitelist editor needs MCM v10 or newer; current MCM v11 includes this support. Changes apply without restarting. Detailed logging remains the last setting, off by default; failures are always logged.

### Русский — изменения этой версии

- Добавлен пункт **«Всегда брать эти предметы»**: точные полные названия или FCS ID, до 128 записей / 8191 байта UTF-8. Список обходит только категорию, минимальную цену и цену за килограмм. Проверки краденого, защиты хранилищ, расстояния, вместимости и безопасного переноса сохраняются. Профили и «Все/Ни одной категории» список не стирают.
- Добавлена **«Защита хранилищ игрока»**, включённая по умолчанию. Сбор пропускает принадлежащие игроку инвентари построек, в том числе открытый сундук и мебель внутри здания игрока. Прежняя раскладка по **Alt+Insert** сохранена.
- Добавлен необязательный **«Подбор вещей с земли»**, выключенный по умолчанию. Нажатие клавиши сбора проверяет до 256 результатов в заданном радиусе и добавляет до 32 временных штатных приказов подойти и подобрать предмет. Старые приказы и задания остаются в очереди. Деньги и непустые контейнеры исключены, повторы одного результата в рамках нажатия убираются. Пользователь подтвердил подбор по близости в игре30.10.2026.
- Исправлен маршрут наземного подбора: сначала отправляется приказ для ближайшего подходящего предмета, затем для ближайшего оставшегося от предыдущей цели. Приоритет цены за вес больше не отправляет персонажа за дальней вещью раньше вещей у ног. При равном расстоянии используется идентификатор предмета, некорректные координаты исключаются. **«Сначала ценное»** по-прежнему действует на сбор из инвентарей. Прежние флаги штатного приказа и лимиты 256 результатов / 32 приказа сохранены; подбор по близости подтверждён пользователем в игре30.10.2026.
- Поиски ближайших инвентарей, хранилищ и наземных вещей используют раздельные повторно используемые буферы; очистка при раннем выходе и защита вложенных поисков исключают перезапись активного обхода. Поиск на земле запускается только по действию сбора.
- Сохранено прежнее исправление переноса с **KEO и объединением стопок**: принятый игрой предмет больше не вставляется повторно и не возвращается назад только потому, что счётчик выбранного инвентаря не вырос. Игра может направить его в рюкзак или объединить со стопкой. Раскладка по умолчанию переносит всё ненадетое; при желании включаются правила сбора.
- Подписи и подсказки MCM обновлены на **русском, английском и упрощённом китайском** (`ru_RU`, `en_GB`, `zh_CN`). Для полного редактора списка нужен MCM v10 или новее; текущий v10 это поддерживает. Настройки действуют без перезапуска. Подробный журнал — последняя настройка, по умолчанию выключен; ошибки пишутся всегда.

## Player guide / Краткий справочник

**English:** Open the intended player's main inventory and press **Insert** to collect nearby loot according to the rules. Press **Alt+Insert** once to deposit unequipped items in nearby player storage that accepts them. Both actions use the loot radius (default 20 m); a held deposit binding suppresses collection. The collector is the character whose main inventory handles the press, not every selected squad member. Options → MCM → **Better Looting** provides bindings, radius, value filters, categories, backpack preference and the Balanced/Money/Resources profiles. MCM is optional; file settings use `BetterLooting.ini` beside the DLL. On a clean installation the existing runtime defaults are Insert, 20 m, minimum value 500 and 300 per kg. Existing personal settings are preserved. Requires RE_Kenshi; do not enable another BetterLooting DLL at the same time.

**Русский:** Откройте основной инвентарь нужного персонажа игрока и нажмите **Insert** для сбора ближайшей добычи по правилам. **Alt+Insert** однократно раскладывает ненадетые вещи по подходящим хранилищам игрока. Для обоих действий используется радиус сбора (по умолчанию 20 м); удержание раскладки блокирует сбор. Действует персонаж, чей основной инвентарь обработал нажатие, а не весь выделенный отряд. Настройки → MCM → **Better Looting**: клавиши, радиус, цена, категории, предпочтение рюкзака и профили «Баланс/Деньги/Ресурсы». MCM необязателен; настройки хранятся в `BetterLooting.ini` рядом с DLL. На чистой установке прежние значения по умолчанию: Insert, 20 м, цена от 500 и от 300 за кг. Личные настройки сохраняются. Требуется RE_Kenshi; вторую DLL BetterLooting одновременно не включать.

## Credits and origin / Авторство и происхождение

Original / оригинал: **XxAtreuSSxX**, [BetterLooting source](https://github.com/XxAtreuSSxX/BetterLooting). MCM fork, integration and fixes / MCM-форк, интеграция и исправления: **LEOPARD**. GNU GPL v3; see [LICENSE.txt](LICENSE.txt). RE_Kenshi/KenshiLib provide the native plugin foundation; localization uses Asur's shared `Localization.h`. Original author and contributors retain credit. Historical release notes below describe earlier fork versions.

## New options reference (29 October 2026)

The MCM page now provides three independent options, applied to the next loot action without restarting:

- **Always take these items**: a whitelist of exact item names or FCS record IDs. Separate entries with semicolons (a vertical bar or newline also works). Use `name:Animal Skin; name:Basic First Aid Kit`, or `id:your-record-id`. Without a prefix, an entry can match either a whole name or an ID. Names match the current displayed name or the item's base name, ignoring ASCII letter case; IDs are case-sensitive. Nothing matches by substring. The list is empty by default and limited to 128 entries / 8191 UTF-8 bytes. Editing the full list of up to **8191 UTF-8 bytes** requires the updated **MCM v10**. The INI supports the same limit. An oversized value is disabled for matching and preserved when saving a profile.
- **Protect player storage**: ON by default. Looting skips all player-owned building inventories, including storage, machines, and furniture with a player-owned parent building. This applies to nearby targets and a directly evaluated open inventory. It does not change depositing with **Alt+Insert**. Turn it off to restore the previous player-storage looting behavior.
- **Pick up ground items**: OFF by default. One press of the loot key searches only the configured radius (1–100 m), examines at most 256 results, and appends up to 32 temporary native pickup orders. The character walks to the items. Existing orders/jobs are preserved; the game handles pathfinding, capacity, ownership and the actual pickup. This is not an instant inventory transfer. Repeated query hits are deduplicated for the current press. Ground items already ordered are still governed by the game's queue; avoid repeatedly ordering the same items before the character reaches them.

The whitelist bypasses category, minimum value and value-per-kg filters only. It never bypasses the stolen-item setting, player-storage protection, range, native capacity or transfer safety. Nonempty ground containers and money are skipped. Untyped ground trade/material items use the Materials category. Ground pickup does not search while idle or while typing in a focused UI field. Radius scans reuse separate bounded scratch buffers; early returns clear them and a nested scan cannot overwrite an active outer scan. Its native orders use the game's/KEO's inventory destination policy; the existing inventory-loot path keeps its backpack preference.

The collector is the player character whose main inventory window processes the loot-key press. With several inventory windows open, the first eligible player inventory to process the press consumes it; this does not distribute work across every selected squad member. Open the intended character's main inventory before collecting. Whitelist item IDs remain stable across language changes; name entries must match the current/base item name.

```ini
[Rules]
RequiredItems=name:Animal Skin; name:Basic First Aid Kit
[Behavior]
IgnorePlayerStorage=1
PickupGroundItems=0
```

Profiles and All/No categories do not erase the whitelist or change these two options. Debug logging remains last in MCM and off by default; errors are always logged. Personal INI files are never distributed or replaced.

### По-русски

В MCM добавлены «Всегда брать эти предметы», «Защищать хранилища игрока» и «Подбирать вещи с земли». Белый список обходит только категории и ограничения цены; краденое, защита базы и вместимость остаются в силе. Защита хранилищ включена по умолчанию и действует даже для открытого сундука. Alt+Insert по-прежнему раскладывает вещи без изменений. Подбор с земли выключен по умолчанию: по клавише сбора персонажу добавляются до 32 разовых приказов подойти и подобрать вещи, не отменяя прежнюю очередь. Все три настройки действуют без перезапуска. Для редактирования полного списка до 8191 байта UTF-8 нужен обновлённый MCM v10. Точные ID не зависят от языка; названия указываются целиком, через `;`.

### 中文

MCM 新增物品白名单、玩家储存保护和地面拾取选项。白名单只绕过类别、最低价值和单位重量价值筛选，不会绕过赃物限制、基地保护、容量或安全检查。玩家储存保护默认开启，已打开的箱子也受保护；Alt+Insert 存放功能不变。地面拾取默认关闭，按拾取键后最多追加 32 个临时拾取命令，角色会走到物品旁拾取，原有命令保留。设置无需重启。编辑最多 8191 字节 UTF-8 的完整列表需要更新后的 MCM v10。完整名称或 FCS ID 用分号分隔；ID 不受游戏语言影响。

## Source layout

- Source/BetterLooting.cpp
- Source/BetterLootingRules.h
- Source/BetterLootingRequiredItems.h
- Source/BetterLooting.vcxproj

## Build

Build the project in Release x64 with the compatible MSVC toolchain and the RE_Kenshi/KenshiLib development dependencies required by the project configuration.

## License

This project is distributed under the GNU General Public License version 3. See LICENSE.txt.

## Изменения форка: MCM и журнал (04.10.2026)
- Настройки — ещё и во вкладке MCM окна «Настройки» (мод ModConfigMenu): клавиша сбора, радиус, правила, категории. Пишутся в тот же BetterLooting.ini и применяются сразу. Своего окна (SHIFT+L) больше нет - см. ниже.
- Журнал: сбои (FAIL, ERROR, fault, failed) пишутся всегда, остальные подробности — только при `[Debug] Log=1` в BetterLooting.ini или «Подробный журнал» в MCM. Раньше на каждое открытие инвентаря в журнал шла строка «Error … CLASS source=…».
- Ini читается тем же путём, что и пишется: рядом с DLL.
- Перевод: en/ru/zh (locale в папке мода).
- На странице MCM - кнопки профилей (баланс, деньги, ресурсы) и «Все категории» / «Ни одной», как в своей панели; применяются и сохраняются сразу.
- Окно настроек на SHIFT+L и сама клавиша убраны (04.10.2026): все настройки, профили и «Все/Ни одной категории» - на странице MCM. Вместе с окном ушла кнопка «Разложить всё».

## Выкладка форка (05.10.2026)
- Версия 1 этого форка - для Steam и Nexus («Better Looting (MCM fork) (QoL)»). Оригинал - XxAtreuSSxX (страница в Steam снята автором), GPLv3.
- В проект `BetterLooting.vcxproj` добавлены `BetterLootingPanel.cpp` и заголовки - раньше в нём был только `BetterLooting.cpp`, и сборка по проекту не находила пресеты.
- Клавиша сбора в MCM назначается нажатием: ключ `[Behavior] ActivationKey` (имя клавиши, `shared/HoldKey.h`); если он есть - главнее старого числового `ActivationVirtualKey`.
- В раздачу не идёт `BetterLooting.ini` (личные настройки): без него действуют значения по умолчанию (Insert, 20 м, от 500 и от 300 за кг), файл появится при первом сохранении настроек.
- Исходники поддерживаются в репозитории форков; общие заголовки Localization.h и ModConfigMenu.h берутся со стенда разработки.

## Изменения форка (15.10.2026)
- Краденым считается только взятое у живого человека без сознания: с мёртвых, животных и животных-роботов вещь уходит без владельца (`setProperOwner(hand())`), `notifyTheftFrom` не зовётся, AllowStolen=0 их не запрещает.
- Флаг «источник - тело» (`forceStolenFromCharacter`) остаётся как у автора: на нём держатся проверки точного переноса. Первая попытка (14.10) меняла сам флаг - проверка не сходилась, перенос клал одну вещь в два инвентаря, и следующее окно инвентаря падало в `InventorySection::recalculateTotalWeight`. Деньги BetterLooting не берёт, как и раньше.
- Падения после сбора (один предмет в двух инвентарях, `InventorySection::recalculateTotalWeight` на следующем окне инвентаря): игра принимала предмет (`addItem` = true), но счётчик в выбранном инвентаре не рос - KEO кладёт подобранное в надетый рюкзак, или стопка слилась. Авторский код считал перенос неудачным и клал тот же предмет ещё раз: во второй инвентарь игрока, обратно в источник или в хранилище. Теперь принятый игрой предмет больше никуда не кладётся (`TRANSFER_ENGINE_TRUSTED`, `STORE_ENGINE_TRUSTED`).

## Изменения форка (22.10.2026, v3)
- Раскладка по хранилищам снова доступна: у автора её запускала кнопка в окне SHIFT+L, окно убрано 04.10 (настройки - в MCM), и кнопка ушла с ним; функция (`RunBetterLootingStoreAll`) оставалась без вызова. Теперь - клавиша `[Behavior] DepositKey` (по умолчанию `ALT+INSERT`, сочетание через `shared/HoldKey.h`), одно срабатывание на нажатие; пока она зажата, клавиша сбора не срабатывает.
- По умолчанию раскладывается всё, что не надето (`DepositByRules=0`): у автора раскладка шла через правила сбора, и при выключенных категориях оружие и броня не раскладывались. `DepositByRules=1` - как у автора.
- Итог раскладки пишется в журнал всегда: сколько вещей и в какое хранилище (имя, расстояние), сколько не нашли места, сколько сбоев переноса. Строка «STORE_ALL END fault=0» больше не идёт в журнал как ошибка.
- Запрос раскладки живёт секунду: если за это время не обновилось окно инвентаря персонажа игрока (никого не выбрано, идёт сбор), он снимается с записью в журнал, а не срабатывает позже неожиданно.


Localization diagnostics / Журнал локализации (2026-11-01)
Locale and catalogue-size summaries are written only while this plugin's Detailed log is enabled. Enabling it after translations have loaded reports the existing catalogue once. Loading/format errors remain visible with detailed logging off.
Сводка языка и числа переводов записывается только при включённом подробном журнале этого плагина. Включение журнала после загрузки переводов выводит сводку существующего каталога один раз. Ошибки загрузки и формата видны и при выключенном подробном журнале.
