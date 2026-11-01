# Rotate Items - MCM fork

[Nexus — our release / наша версия](https://www.nexusmods.com/kenshi/mods/2260)

<!-- fork-publication:start -->
## Our fork: author, sources and release history / Наш форк: автор, исходники и версии

Original author / автор оригинала: **jacobericson**. Thank you for the original mod! / Спасибо за оригинальный мод!
Original project / оригинальный проект: https://github.com/jacobericson/KenshiRotate
Original source / исходники оригинала: https://github.com/jacobericson/KenshiRotate

### Changes made by this fork / Отличия именно нашего форка

Inherited gameplay features are described in the player guide; the list below records our changes. / Исходные игровые возможности описаны в справочнике; ниже перечислены наши изменения.

**Русский**
- MCM заменяет прежнюю вкладку MODS: одно назначение клавиатуры или мыши с модификаторами, MMB по умолчанию; NONE отключает ввод. Для назначения мыши нужен MCM 10 или новее.
- Первый активный прежний способ переносится автоматически; существующее новое сочетание и личные настройки сохраняются.
- Исправлены повторные повороты после фокуса инвентаря; редактируемые поля и окно настроек блокируют вращение.
- Добавлены русский, английский и китайский, сокращены лишние обновления значков, ссылки очищаются при удалении предметов и значков.

**English**
- MCM replaces the old MODS controls: one keyboard or mouse binding with modifiers, MMB by default; NONE disables input. Mouse assignment requires MCM 10 or newer.
- The first active legacy binding migrates automatically; an existing new binding and personal settings are preserved.
- Repeated rotation after inventory focus was fixed; editable fields and the Options window block rotation.
- Added Russian, English and Chinese text, fewer redundant icon updates, and reference cleanup when items or icons are destroyed.

### Fork release history / История выпусков нашего форка

First fork release: v1; pre-publication fixes stay in this version. / Первый выпуск форка: v1; исправления до публикации остаются в нём.

**v1**
- Настройки перенесены из MODS в MCM с одним назначением клавиатуры/мыши и модификаторов: MMB по умолчанию, NONE отключает ввод, первый активный старый способ переносится автоматически. Исправлены повторные повороты при фокусе инвентаря с защитой ввода текста; добавлены три языка, сокращены лишние обновления значков, состояние очищается при удалении предметов и значков.
- Settings moved from MODS to MCM with one keyboard/mouse binding and modifiers: MMB by default, NONE disables input, and the first active old binding migrates automatically. Repeated rotation with inventory focus was fixed while protecting text entry; three languages, fewer redundant icon updates and item/icon lifetime cleanup were added.

<!-- fork-publication:end -->

Локальный форк jacobericson/KenshiRotate. Рабочая логика поворота, переносов,
разделения стопок и записи поворота в сохранение оставлена из оригинала.

По умолчанию наведите мышь на предмет в инвентаре и нажмите среднюю кнопку
мыши. Можно также повернуть предмет, уже взятый курсором. Для поворота
должно хватать свободного места.

Повторные нажатия поворачивают предмет обратно без закрытия инвентаря или
рюкзака; удержание кнопки не повторяет поворот. Ввод в редактируемое текстовое
поле временно блокирует сочетание, обычный фокус инвентаря ему не мешает.

Настройки: MCM -> Rotate Items. Одна настройка «Кнопка поворота»:
по умолчанию MMB (средняя кнопка мыши). Можно назначить одну клавишу или
LMB/RMB/MMB/X1/X2 с CTRL, SHIFT или ALT. Нажмите строку настройки, затем
нужную клавишу или кнопку; Esc отменяет захват, Backspace задаёт NONE.
NONE отключает ввод поворота. Назначение применяется сразу. Выключатель
запрещает новые повороты, но сохраняет правильное отображение и сохранение
уже повёрнутых вещей. Во время открытых настроек и ввода в редактируемое
текстовое поле предметы не поворачиваются.

При первом запуске с прежним INI первый активный способ переносится в
RotationKey: сначала основной, иначе второй. Сохраняются клавиша и
модификаторы; отключённые способы дают NONE. Старые пользовательские ключи
остаются в файле для восстановления, но новая версия больше ими не управляет.
Существующий RotationKey всегда имеет приоритет.

Отдельные кнопки в старой вкладке MODS убраны. Настройки сохраняются в
KenshiRotate.ini; при обновлении распространяется только defaults.ini.
Не включать одновременно с оригинальным Rotate Items.

Настройки и игровые уведомления переведены на русский, английский и
упрощённый китайский. Рабочие переводы выпуска находятся в locale в формате .mo; .po остаются в исходниках;
старые файлы lang сохранены в исходниках только для справки.

Требуется RE_Kenshi/KenshiLib. Сборка: штатный build.ps1 -SrcDir <этот каталог>\src -Name KenshiRotate.
Лицензия GPL-3.0, автор и исходный коммит в ORIGIN.md. Подробности исходной
логики в README.upstream.md. Пользователь подтвердил повторный поворот и одно назначение 30.10.2026; все варианты предметов и стопок отдельно не проверены.

Исправления по проверке 29.10.2026:
Debug=1 в MCM включает журнал настроек, попыток и результатов вращения в RE_Kenshi_log.txt. По умолчанию выключен, применяется сразу. Ошибки записываются всегда.
## Current release / Текущая версия: v1 (29–30 October / октября 2026)

### English — changes in this version

- Integrated **MCM → Rotate Items** and moved settings out of the old MODS tab. Removed the old options injection and active-mode controls. The current page has Enable rotation input, **one Rotation binding**, and Detailed log.
- Replaced primary/secondary controls with the single `RotationKey`, default **MMB**. It accepts one keyboard key or **LMB/RMB/MMB/X1/X2**, with optional **Ctrl, Shift and Alt** modifiers. The MCM hotkey row opts into mouse capture; use the updated MCM v10 for mouse assignment. The first click arms capture, Escape cancels, and Backspace clears to **NONE**. NONE explicitly disables rotation input.
- Preserved existing assignments during migration: if a personal INI has no `RotationKey`, the **first active legacy binding** is converted before defaults are merged (primary first, secondary otherwise). Keyboard modifiers are retained; inactive bindings result in NONE. An existing RotationKey wins. Legacy keys remain in the personal file for recovery but are no longer used as separate controls.
- Fixed repeat presses after an inventory/backpack gains keyboard focus: ordinary inventory focus no longer blocks a second rotation. Editable text boxes and their child widgets still suppress rotation, as does the open Options window. One press rotates once; holding the binding does not auto-repeat.
- Added lifecycle cleanup for destroyed items and inventory icons: the rotated-item handle is removed and any matching cursor reference is cleared before destruction. Kept upstream save/load, copying, placement validation, split-stack inheritance and right-click transfer/stack behavior unchanged.
- Localized MCM labels/help and notifications in **English, Russian and Simplified Chinese** (`en_GB`, `ru_RU`, `zh_CN`), using the game's language. Runtime `.mo` catalogs are packaged; editable `.po` and historical `lang` files remain in source.
- Added **Detailed log** last in MCM, off by default, for input settings, attempts and results in `RE_Kenshi_log.txt`. Errors are always logged. All settings apply without a restart; personal settings are preserved and only `KenshiRotate.defaults.ini` is supplied as release defaults.

### Русский — изменения этой версии

- Настройки перенесены в **MCM → Rotate Items**. Старое внедрение настроек во вкладку MODS и управление отдельными активными режимами удалены. Текущая страница содержит включение ввода, **одно назначение поворота** и подробный журнал.
- Вместо основного и дополнительного способов используется один `RotationKey`, по умолчанию **MMB**. Можно назначить одну клавишу или **LMB/RMB/MMB/X1/X2** с модификаторами **Ctrl, Shift и Alt**. Строка MCM поддерживает захват мыши; для назначения кнопок мыши нужен обновлённый MCM v10. Первый щелчок начинает захват, Escape отменяет, Backspace задаёт **NONE**. NONE явно отключает ввод поворота.
- Прежние назначения сохраняются при миграции: если в личном INI нет `RotationKey`, **первый активный старый способ** переносится до добавления новых defaults (сначала основной, иначе дополнительный). Модификаторы клавиатуры сохраняются; отключённые способы дают NONE. Уже существующий RotationKey имеет приоритет. Старые ключи остаются в личном файле для восстановления, но больше не образуют отдельные способы управления.
- Исправлены повторные нажатия после получения фокуса инвентарём/рюкзаком: обычный фокус окна больше не блокирует следующий поворот. Редактируемое поле и его дочерние виджеты по-прежнему блокируют вращение, как и открытое окно настроек. Одно нажатие даёт один поворот, удержание не повторяет его.
- Добавлена очистка при удалении предмета и его значка: удаляется отметка поворота, сбрасывается соответствующая ссылка предмета/значка на курсоре. Сохранены исходные пути записи/загрузки, копирования, проверки места, наследования ориентации разделённой стопкой и объединения при быстром переносе правой кнопкой.
- Подписи/подсказки MCM и уведомления поддерживаются на **русском, английском и упрощённом китайском** (`ru_RU`, `en_GB`, `zh_CN`) по языку игры. В пакет идут рабочие `.mo`; `.po` и исторические `lang` остаются в исходниках.
- **«Подробный журнал»** находится последним в MCM и по умолчанию выключен. Он пишет настройки ввода, попытки и результаты в `RE_Kenshi_log.txt`; ошибки пишутся всегда. Все настройки действуют без перезапуска. Личный INI сохраняется, в выпуске распространяется `KenshiRotate.defaults.ini`.

## English player reference

Hover over a non-square inventory item and press **middle mouse** to rotate it 90 degrees; press again to restore its orientation. You may also rotate a cursor-held item, which takes priority. Character, backpack and container grids use their normal placement restrictions: rotation needs room and an inventory that accepts the item. Equipped items and square shapes are not rotated. Icons rotate with the item and charge bars retain the original handling.

Open Options → MCM → **Rotate Items** to change the single binding. Disabling input stops new rotations while existing rotated items continue to display and save correctly. Without MCM, edit the file next to the DLL:

```ini
[KenshiRotate]
Enabled=1
RotationKey=MMB
Debug=0
```

Rotation is saved with the item in the native save, without a separate rotation-state file. Split stacks inherit orientation. The upstream drag/drop rules prevent stacking opposite orientations; right-click quick transfer may merge into the destination stack's orientation. Removing the plugin restores default item shapes on load, so items that no longer fit may be dropped or equipped by the game. Do not run the original Rotate Items DLL alongside this fork. Requires RE_Kenshi/KenshiLib.

## Справочник: сохранения и стопки

Поворот записывается вместе с предметом в обычное сохранение, отдельного файла состояния нет. Разделённая стопка наследует ориентацию. Исходная логика перетаскивания не объединяет противоположные ориентации; быстрый перенос правой кнопкой допускает объединение с ориентацией целевой стопки. Квадратные и надетые предметы не вращаются. Значок поворачивается вместе с предметом, полоски заряда обрабатываются по прежней логике. Если удалить плагин, при загрузке вернутся исходные размеры; вещи, переставшие помещаться, игра может выбросить или экипировать.

## Source, credits and history / Исходники, авторство и история

Original author / автор оригинала: **jacobericson** — [KenshiRotate](https://github.com/jacobericson/KenshiRotate), imported commit `599cb45b8561b0d249450563a4ec281e67d4b73a`. MCM fork and fixes / MCM-форк и исправления: **LEOPARD**. See [ORIGIN.md](ORIGIN.md) and [LICENSE](LICENSE), GNU GPL v3. Native foundation / основа: RE_Kenshi/KenshiLib; localization / локализация: Asur's shared `Localization.h`.

[README.upstream.md](README.upstream.md) preserves the original feature list, build information and API explanation. Its MODS-tab and dual-binding instructions describe upstream history; use the current single-binding MCM instructions above. / В README.upstream.md сохранены исходный список функций, сведения о сборке и API. Указания о вкладке MODS и двух назначениях относятся к оригиналу; текущее управление — одно назначение MCM, описанное выше.

Developer note / Для разработчика: rotation textures are cached per unique source icon; the cache has no numeric byte cap. Lifecycle cleanup is not a guarantee against long-session leaks, and no isolated FPS gain is claimed. / Текстуры поворота кэшируются по уникальной исходной иконке, числового ограничения в байтах нет. Очистка состояния не доказывает отсутствие длительных утечек; изолированный прирост FPS не заявляется.

## Local diagnostic candidate / Локальная диагностическая сборка

**English.** The existing **Detailed log** switch remains last in MCM, off by default and effective immediately. Alongside rotation events, it records manual stack checks involving the cursor-held item in `RE_Kenshi_log.txt`. Enable it before playing; if an ordinary drag refuses to stack apparently identical items, keep the log and send it with a screenshot and a short account of where the items came from. This candidate does not change ownership, stolen status, quantities, charges, rotation or the original compatibility result. It records the pair at the check; it cannot guarantee a complete history of how an ownership tag arose. No additional transfer/drop/pickup hooks are installed.

Each `STACK_PAIR` line distinguishes `OriginalReject` (the original game/mod chain returned false), `RotateReject` (Rotate rejected opposite orientations), `TargetFull` (the pair is compatible but a freshly verified target stack has no quantity/charge capacity), or `Accepted`. A positive predicate result is still reported as `result=1` for `TargetFull`: capacity is context, not a replacement predicate. The record includes the actual rotation flags, exact handles, prototype/SID, maker/material, quality, quantity/charges, dimensions, stored/current/proper owner and native stolen result. Capacity is `unknown` unless an existing placement call provides a live GUI and the target is verified in its named section. Caller module/RVA identifies the immediate caller only.

Capture and logging run only on the observed inventory UI thread, with no extra item reads or formatting when Debug is off. Fixed storage retains at most 64 pair keys; logging is limited to four entries per second globally, one entry per pair per 250 ms, and an unchanged pair state per two seconds. Other pairs and suppressed intermediate events can be absent. Temporary placement context restores across nested calls and C++ exceptions; no foreign widget/item references are retained for later capture. Errors are logged once without changing the predicate result. The public release remains v1; this section describes a local diagnostic candidate pending game validation.

**Русский.** Существующий **«Подробный журнал»** остаётся последним в MCM, по умолчанию выключен и применяется сразу. Помимо поворотов он записывает в `RE_Kenshi_log.txt` ручные проверки стопки, если один из предметов находится на курсоре. Включите его перед игрой; если обычное перетаскивание не объединит внешне одинаковые вещи, сохраните журнал и передайте его со скриншотом и кратким описанием происхождения вещей. Эта сборка не меняет владельцев, метки кражи, количество, заряды, поворот или исходный результат совместимости. Она фиксирует состояние пары в момент проверки, а не гарантированную полную историю появления метки владельца. Новые перехваты переноса, выбрасывания и подбора не добавлены.

Строка `STACK_PAIR` различает `OriginalReject` (отказ исходной цепочки игры/модов), `RotateReject` (Rotate запретил разные ориентации), `TargetFull` (пара совместима, но подтверждённая целевая стопка заполнена по количеству и зарядам) и `Accepted`. При `TargetFull` положительный исходный результат всё равно указан как `result=1`: вместимость — пояснение, а не подмена проверки. Записываются настоящие флаги поворота, полные handles, прототип/SID, изготовитель/материал, качество, количество/заряды, размеры, сохранённый/текущий/фактический владелец и вычисленная игрой метка кражи. Вместимость остаётся `unknown`, если текущий вызов размещения не подтверждает живой GUI и наличие цели в указанном разделе. Модуль/RVA указывает только непосредственного вызывающего.

Снятие состояния и запись выполняются только в установленном потоке интерфейса инвентаря; при Debug=0 дополнительных чтений предметов и форматирования нет. Фиксированная память хранит до 64 ключей пар. Ограничения: четыре записи в секунду на весь мод, одна запись пары за 250 мс и повтор неизменившегося состояния не чаще двух секунд. Другие пары и подавленные промежуточные события могут отсутствовать. Временный контекст размещения восстанавливается при вложенных вызовах и исключениях C++; чужие ссылки не сохраняются для последующего чтения. Ошибки пишутся однократно без изменения результата проверки. Публичный выпуск остаётся v1; этот раздел описывает локальную диагностическую сборку до проверки в игре.


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
