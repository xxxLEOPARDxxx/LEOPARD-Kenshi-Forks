# BetterLooting

BetterLooting is a native RE_Kenshi plugin for Kenshi.

This repository contains the corresponding source code for the distributed plugin build.

## New options (29 October 2026)

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
