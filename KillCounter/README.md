# Kill Counter - локальный MCM fork

<!-- fork-publication:start -->
## Our fork: author, sources and release history / Наш форк: автор, исходники и версии

Original author / автор оригинала: **Rokeshi**. Thank you for the original mod! / Спасибо за оригинальный мод!
Original project / оригинальный проект: https://www.nexusmods.com/kenshi/mods/2103
Original source / исходники оригинала: Source code shipped with the installed original; a public repository and author permission to redistribute a modified version have not been verified. No explicit license was present locally. / Исходники поставлялись с установленным оригиналом; публичный репозиторий и разрешение автора на распространение изменённой версии не подтверждены. Явной лицензии локально не было.

### Changes made by this fork / Отличия именно нашего форка

Inherited gameplay features are described in the player guide; the list below records our changes. / Исходные игровые возможности описаны в справочнике; ниже перечислены наши изменения.

**Русский**
- В MCM добавлены общее включение учёта и независимый показ каждой строки; скрытие строки не останавливает её учёт, отключение сохраняет накопленное.
- Подписи и настройки переведены на русский, английский и китайский; прежние исправления сообщества и совместимость со StatColours сохранены.
- Числа обновляются только при изменении; очищаются записи удалённых персонажей и просроченных скрытных оглушений, не накапливаются пустые записи статистики.

**English**
- MCM adds a tracking enable switch and independent row visibility; hiding a row does not stop counting, and disabling retains totals.
- Labels and settings support Russian, English and Chinese, retaining community fixes and StatColours compatibility.
- Numbers refresh only when changed; destroyed-character and expired stealth-attempt records are removed, and empty statistics records do not accumulate.

### Fork release history / История выпусков нашего форка

First fork release: v1; pre-publication fixes stay in this version. / Первый выпуск форка: v1; исправления до публикации остаются в нём.

**v1**
- Добавлены MCM-переключатели нового учёта и независимого показа каждой строки, английские/русские/китайские подписи. Обновляются только изменившиеся числа, удаляются записи об уничтоженных персонажах и просроченных попытках оглушения; отключение сохраняет накопленное, совместимость со StatColours сохранена.
- Added MCM controls for new-event tracking and independent visibility of each row, with English, Russian and Chinese labels. Only changed numbers are refreshed, destroyed-character and expired stealth-attempt records are removed, disabling retains totals, and StatColours compatibility is preserved.

<!-- fork-publication:end -->

Оригинал Rokeshi, существующие исправления перевода сохранены.
Убийства, нокауты и оторванные конечности видны в окне навыков.

MCM -> Kill Counter: включение нового учёта и показ каждой из трёх строк.
Настройки применяются сразу, без MCM - KillCounter.ini.
Отключение сохраняет накопленные значения. Поля статистики в сохранениях
совместимы с установленной прежней версией.
Цвет счётчиков по-прежнему может задавать StatColours. Неизменные числа
не заставляют интерфейс перерисовывать текст каждый кадр.

Собирается штатным build.ps1 -SrcDir <этот каталог> -Name KillCounter.
Локальный тестовый форк; публичная публикация в этой задаче не выполняется.
Происхождение: ORIGIN.md. Нужна проверка накопления и загрузки в игре.

Исправления по проверке 29.10.2026:
Debug=1 в MCM включает журнал в RE_Kenshi_log.txt: убийства, боевые и скрытные нокауты, отсечённые конечности и загруженные счётчики. По умолчанию выключен, применяется сразу. Ошибки пишутся всегда.
## Current local release / Текущая локальная версия: v1 (29–30 October / октября 2026)

### English — changes in this version

- Integrated **MCM → Kill Counter** with an Enabled toggle and independent **Show kills**, **Show knockouts** and **Show severed limbs** toggles, all on by default. Changes apply immediately. Hiding a row does not stop its counter; disabling the plugin stops new event accounting and hides the rows while keeping accumulated saved totals.
- Maintained **English, Russian and Simplified Chinese** (`en_GB`, `ru_RU`, `zh_CN`) for counter labels, MCM settings and help. The existing community translation fixes are preserved.
- Added character-destruction cleanup for totals and pending hit/stealth attribution. Tracked attackers are cleared from other pending records before their addresses can be reused. Ordinary unrelated NPC destruction avoids walking pending attribution lists. Empty statistics no longer create persistent map entries just because a stats window or save reads them; zero load records remove an existing entry.
- Pending death and stealth records are cleared while tracking is disabled. Expired stealth-attempt records are pruned; the existing short validity window prevents a stale chance query from attributing a later knockout to the wrong character.
- Preserved the native save fields **kills / kos / limbs** and the existing melee, ranged, stealth knockout and delayed-death accounting. UI rows refer to the character displayed in each stats window. Values update only when their caption changes; the plugin retains neighboring native row skins and does not overwrite colors applied by **StatColours**.
- Added **Detailed log** as the last MCM setting, off by default, recording settings, kills, combat/stealth knockouts, severed limbs and loaded totals in `RE_Kenshi_log.txt`. Errors are always logged. Personal INI settings are preserved; release defaults use `KillCounter.defaults.ini`.

### Русский — изменения этой версии

- Добавлены **MCM → Kill Counter**: общее включение и независимый показ **убийств**, **нокаутов** и **оторванных конечностей**. Все четыре пункта по умолчанию включены, изменения действуют сразу. Скрытие строки не прекращает её учёт; общее отключение прекращает новый учёт и скрывает строки, сохраняя накопленные значения.
- Подписи счётчиков, настройки и подсказки MCM поддерживаются на **русском, английском и упрощённом китайском** (`ru_RU`, `en_GB`, `zh_CN`). Существующие исправления перевода сообщества сохранены.
- Добавлена очистка счётчиков и ожидающих записей удара/скрытного оглушения при удалении персонажа. Ссылки на удаляемого атакующего убираются из чужих ожидающих записей до повторного использования его адреса. Удаление постороннего NPC не требует обхода списков атрибуции. Просмотр статистики и сохранение больше не создают постоянные пустые записи; загрузка нулевого счётчика удаляет прежнюю запись.
- При отключении учёта очищаются ожидающие смерти и скрытные оглушения. Просроченные попытки скрытного оглушения удаляются; прежнее короткое окно их действия не даёт старому запросу шанса приписать последующий нокаут чужому персонажу.
- Сохранены поля **kills / kos / limbs** в обычном сохранении и прежний учёт ближних/дальних ударов, скрытных нокаутов и отложенной смерти. Строки относятся к персонажу соответствующего окна навыков. Текст обновляется только при изменении числа, используются скины соседних штатных строк, цвета **StatColours** не перезаписываются.
- **«Подробный журнал»** — последняя настройка MCM, по умолчанию выключена. В `RE_Kenshi_log.txt` пишутся настройки, убийства, боевые/скрытные нокауты, отсечённые конечности и загруженные значения. Ошибки пишутся всегда. Личный INI сохраняется, для выпуска используется `KillCounter.defaults.ini`.

## Player reference / Справочник игрока

**English:** Open a character's stats/skills window to see **Kills**, **Knockouts** and **Limbs removed** beside the native attributes. Each window displays its own character's totals. There is no extra gameplay hotkey. Counters follow new events while the plugin is active; it cannot reconstruct events that happened before it was installed or while tracking was disabled. Totals are stored with the character in normal saves; no separate counter save file is used.

The inherited accounting credits kills to the last recorded attacker when the loaded victim actually dies, including delayed blood-loss deaths. A kill may therefore appear after the hit. A knockout occurs on the transition into unconsciousness, including a successful stealth knockout. If the same attacker knocks down and kills the still-unconscious victim, that knockout is replaced by a kill; a different finisher does not subtract the original attacker's knockout. Limbs count actual lost/crushed arm/leg states. These are the plugin's existing attribution rules, not a retrospective combat history.

**Русский:** Откройте окно навыков/характеристик персонажа: рядом со штатными качествами появятся **«Убийства»**, **«Нокауты»** и **«Отрублено»**. Каждое окно показывает своего персонажа. Отдельного игрового хоткея нет. Счётчики учитывают новые события при работающем плагине; восстановить события до его установки или при отключённом учёте нельзя. Значения сохраняются вместе с персонажем в обычном сохранении, отдельного файла нет.

По сохранённой логике убийство получает последний записанный атакующий, когда загруженная жертва действительно умирает, в том числе позднее от кровопотери. Поэтому число может вырасти после самого удара. Нокаут засчитывается при переходе в бессознательное состояние, включая успешное скрытное оглушение. Если тот же персонаж оглушил и убил ещё бессознательную жертву, нокаут заменяется убийством; другой добивающий не отнимает нокаут первого. Конечности считаются по фактическому состоянию потери/разрушения руки или ноги. Это прежние правила учёта плагина, а не восстановление всей истории боя.

## Settings / Настройки

Options / Настройки → MCM → **Kill Counter**. MCM is optional / необязателен. Without MCM edit / без MCM редактируйте `KillCounter.ini` beside the DLL / рядом с DLL:

```ini
[KillCounter]
Enabled=1
ShowKills=1
ShowKnockouts=1
ShowLimbs=1
Debug=0
```

These settings require no restart. / Эти настройки не требуют перезапуска. Requires RE_Kenshi/KenshiLib. Do not enable the original KillCounter DLL together with this local fork. / Требуется RE_Kenshi/KenshiLib. Не включайте DLL оригинала одновременно с локальным форком. Runtime catalogs are `.mo`; editable `.po` files remain in source. / В пакете используются `.mo`, редактируемые `.po` остаются в исходниках. Continued in-game verification of event accumulation and save/load remains necessary. / Проверка накопления событий и сохранения/загрузки в игре остаётся необходимой.

## Credits and redistribution status / Авторство и статус распространения

Original / оригинал: **Rokeshi**, according to installed mod metadata / по метаданным установленного мода. [Original Kill Counter on Nexus Mods / оригинальная страница](https://www.nexusmods.com/kenshi/mods/2103). Source was included with the installed original mod; imported from the locally patched `KillCounter-src` on 29 October 2026. / Исходник поставлялся с установленным оригиналом; импортирован из локального исправленного `KillCounter-src` 29 октября 2026 года. MCM integration and local fixes / интеграция MCM и локальные исправления: **LEOPARD**; existing community translation work is retained / прежние исправления перевода сообщества сохранены.

[ORIGIN.md](ORIGIN.md) records that **no explicit license was present locally**. Permission from the original author for a public modified redistribution is **not confirmed**. This is a local test fork; neither source availability nor a locally built archive establishes permission to publish it. No GPL license is asserted for this fork. / В ORIGIN.md зафиксировано, что **явной лицензии локально не было**. Разрешение оригинального автора на публичное распространение изменённой версии **не подтверждено**. Это локальный тестовый форк; наличие исходника или локального архива не означает разрешения на публикацию. Лицензия GPL этому форку не приписывается.

Native foundation / основа: RE_Kenshi/KenshiLib. Localization / локализация: Asur's shared `Localization.h`. Build on the development stand / сборка на стенде: `build.ps1 -SrcDir <this directory> -Name KillCounter`. No isolated performance improvement or absence of long-session leaks is claimed. / Изолированный прирост производительности и отсутствие длительных утечек не заявляются.
