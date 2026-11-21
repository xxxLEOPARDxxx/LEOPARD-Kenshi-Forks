# KEO (MCM fork) (QoL) - release 8

## English

A temporary maintenance fork of **KEO by jacobericson**. Thank you to the original author for the optimizations and public source release.

- [Original KEO](https://steamcommunity.com/sharedfiles/filedetails/?id=3802456502)
- **Original source:** [jacobericson/KEO](https://github.com/jacobericson/KEO)

This fork is a temporary measure to maintain bug fixes, MCM configuration and supported Steam/GOG compatibility. It is not intended as a separately developed successor. If the original author returns and continues development, we will remove our version to avoid confusing players.

### Features and settings

KEO retains the original area preloading, path preparation, route planning, squad movement, rendering options, gameplay fixes and backpack handling. These features belong to the original author. Use **MCM > KEO** to configure them; without MCM, the original **Options > KEO** tab remains available. The interface supports English, Russian and Simplified Chinese. Other game-interface languages use English.

The settings are grouped as follows:

- **Zone loading:** preloading, path-preparation workers, retained area radii and limits, route look-ahead, and the disk-cache limit.
- **Performance:** particle and scenery processing budgets. No fixed FPS gain is promised.
- **Squad movement:** optional KEO routing, group cohesion, gathering pace, resuming move orders after combat and route-planner mode. Swimming and dangerous water affect route choices. Long trips may still stop short; a repeated move order can continue them.
- **Gameplay fixes:** player-building ownership, repeated throw-out handling and paths around walls. Native city path-readiness checks are retained when areas reload.
- **Backpacks and jobs:** backpack handling, the operator's maximum collection load, optional carried weight and three independent jobs.
- **Logging:** Detailed log is last and OFF by default. Errors remain visible. Enable it to reproduce a report, then disable it after collecting the logs.

MCM marks settings requiring a restart. The carried-weight option requires one; the three job switches take effect during play. Advanced settings are documented in the packaged `KEO.defaults.ini` template. KEO creates your personal `KEO.ini` for configuration. Preserve that personal file when updating.

### Automatic path repair

**Pathing repairs itself** uses the name from KenshiQOL's feature description.
Enable it under **MCM > KEO > Gameplay fixes**. It is live and OFF by default.
After each save load, each actually visited active zone gets one automatic
Ctrl+Shift+F11-style pass: the native building/furniture repair, then full native
navmesh generation for its exterior and eligible building interiors. The repair
covers all active zones, as in the original native command. It may briefly pause
the game; requests are spaced and wait for idle loading, physics and navigation.
Generation uses the existing KEO workers and requires navmesh caching. Native
loading messages are retained. It does not teleport actors or guarantee that
all pathfinding issues disappear. Private speculative preloads do not count as
visits. Off/on preserves visit history; loading a save/new world clears it.

### Resource preparation and loading

The **Resource memory** group contains optional file preparation in RAM,
startup warmup, nearby-terrain preparation and retention of loaded model
geometry/textures in video memory. These memory options are OFF by default.
Enable the RAM resource option first, then startup preparation if wanted.
Startup preparation reads registered local mesh/skeleton/texture resources
within the configured memory limit; reaching that limit does not mean the
entire game is loaded. It does not create NPCs early. RAM/VRAM limits and
background reading speed are configurable; keep room for the rest of the game.
These settings apply live. **Show warmup text** displays work at the upper left
and hides it automatically when finished, returning when new work is queued.

Under **Performance**, the loading improvements include skeleton/model and
material storage preparation, model bone weights/index tables, body-shape
preparation, equipment/animation reads and save-file parsing. Individual options
can be disabled live. New model work uses supported engine contracts and keeps
native handling for unsupported cases. Existing game simulation and NPC spawn
order remain in the game; the whole NPC pipeline is not moved onto workers.

The owner confirmed faster save loading and smoother warmed area transitions
on17 November2026. This is gameplay feedback for the combined update, not a
controlled timing multiplier or a promise of identical results on every save.
Cold areas, physics, NPC spawning and rendering can still cause pauses. General
developed-save FPS tuning is a separate task. Detailed implementation evidence
and the three bone-processing helpers are documented in the corresponding
source's `leopard` verification reports; isolated helper ratios are not overall
loading ratios.

### Model weight and topology preparation

**Speed up model preparation** (`meshWeights`) is ON by default and switches
live. It covers linear native-order weight normalization, ordered assignment
import without a repeated root search, and bone lookup tables collected in a
bounded 8320-byte local bitset hierarchy. Supported memory streams also decode
complete triangle records in one pass. Ogre grows the output arrays and reads
edge groups. Short reads, other stream types and other byte orders use the
native triangle reader.
Weights, equal-index order and sorted bone ranks remain unchanged. Ogre owns
imported nodes and output-array growth; old table gaps and allocation-error
partial state are preserved. Unusual weights, unordered imports and unsupported
cases keep the appropriate native stage. No persistent cache, per-frame scan
or new worker is added. The option affects newly prepared models; isolated
stage timings are not whole-save speedup factors.

### Operator cargo limit

Under **Backpacks and jobs**, set the operator cargo limit from 5% to 100% in 5% increments. The limit counts occupied cells in the main inventory and worn backpack. The weight limit still applies. Right-click the existing backpack checkbox to set a separate limit for its wearer. A green mark on the checkbox indicates an active personal limit. Use **Use global limit** to restore the global value for that character. Personal limits travel with the save.

### Optional carried weight and jobs

This package combines KEO with useful features adapted from **KenshiQOL by Steel**. Thank you to Steel for releasing the original features and source.

- [Original KenshiQOL](https://steamcommunity.com/sharedfiles/filedetails/?id=3772744830)
- **Original source:** the `src` folder distributed with that mod, including `KenshiQOL.cpp` and `TaskTypeFactoryPatch.h`. [Original distribution/source release](https://steamcommunity.com/sharedfiles/filedetails/comments/3772744830). A separate source repository has not been verified.

All four additions are **OFF by default**. Enable the required ones under **Backpacks and jobs**.

**Carried body and container weight** counts the contents of loose carried containers and carried characters. Body weight follows race, age/size and hunger. Worn-backpack weight reductions remain in effect. Restart after changing this option.

**Gather output:** use a production machine's right-click command; SHIFT adds a standing job. Assign several machines to the same collector. This uses ordinary production output rather than creating new items.

**Stock shop counter:** use the right-click command on your own shop counter; SHIFT adds a standing job. It delivers carried items matching products from assigned Gather Output machines. Without those assignments, it uses the game's resource classification. Assign both collection and stocking to supply a counter from production. This does not fetch arbitrary equipment from storage, make a monetary trade, or generate income. Equipped gear is not stock. Full/incompatible counters are skipped; a transfer error stops this stocking operation rather than repeatedly retrying it.

**Waypoint route:** SHIFT + right-click on open ground appends a point for the characters selected at that click. Each point is an ordinary Jobs-panel row with coordinates. Drag rows to reorder them or remove individual rows to change the route. The route loops and is saved with the game. Up to 64 points per character are accepted. Turning the feature off pauses it without deleting the queued rows. Ordinary movement remains available when it is off.

Use a separate save for the first collection-to-counter test. The cyclic route and its restoration through ordinary save/load were confirmed in the local Steam test. Physical collection/counter delivery, every mod combination and long-session performance are not claimed universally verified.

### Installation and compatibility

1. Install **RE_Kenshi 0.3.5 or later** and a supported **Kenshi 1.0.65** installation. [RE_Kenshi](https://www.nexusmods.com/kenshi/mods/847).
2. Disable the original KEO and KenshiQOL. Use one KEO implementation at a time.
3. Extract the `KEO` folder into the game's `mods` directory and enable it in the Kenshi launcher. Use the RE_Kenshi launch method for your installation.
4. Install Mod Config Menu if you want its settings page. MCM is optional; default settings and the fallback KEO tab work without it.

One DLL selects the supported Steam/GOG profile automatically. Matching RE_Kenshi/KenshiLib files are required. The GOG profile currently accepts the verified RE_Kenshi-compatible GOG 1.0.65 executable and **KenshiLib 0.5.0**. Other executables or SDK files are refused rather than treated as a known build. Game executables and SDK DLLs are not bundled.

The current release has been checked in gameplay on the supported GOG build. See Release 7 below for the loading and Stock shop counter changes.

Do not run the original KenshiQOL alongside the weight/jobs integration. Existing routes use ordinary saved job records; a world import is not a promise to preserve those records. Do not overwrite personal settings with a template during an update.

### Reporting a problem

Enable Detailed log, reproduce once, then provide the mod-folder `KEO.log`, the game's `RE_Kenshi_log.txt`, and `crash_dump.txt` if present. Include the store/version, active mod list and exact steps. Do not infer the responsible mod from a nearby log line alone.

### Release 1 changes

- First maintenance fork of KEO with optional MCM configuration, English/Russian/Simplified Chinese interface, and detailed logging off by default.
- Steam and GOG are supported in one package. GOG works, with startup confirmed on an earlier candidate; broad testing of the current gameplay features is not complete.
- Corrected route ownership for selected actors outside the formation limit, stale route-point application, and handling of native path readiness when cities reload.
- Optional carried-body/container weight and three independently switched jobs adapted from Steel's KenshiQOL: Gather Output, Stock Shop Counter and cyclic waypoints. All are off by default.
- Cyclic routes use ordinary job rows, retain distinct points and survive normal saving/loading. Counter stocking physically delivers items and does not create a sale or income.

### License and corresponding source

GPL-3.0; see `LICENSE`. Attribution/provenance files accompany the package. Corresponding source and build scripts are in the shared fork repository: https://github.com/xxxLEOPARDxxx/LEOPARD-Kenshi-Forks/tree/main/KEO . The original KEO and KenshiQOL source links above identify the authors' code; this package contains a modified integration.

---

## Русский

Временный сопровождающий форк **KEO автора jacobericson**. Спасибо автору за оптимизации и открытые исходники.

- [Оригинальный KEO](https://steamcommunity.com/sharedfiles/filedetails/?id=3802456502)
- **Исходники оригинала:** [jacobericson/KEO](https://github.com/jacobericson/KEO)

Это вынужденная временная мера для исправления ошибок, настройки через MCM и совместимости поддерживаемых сборок Steam/GOG. Отдельного преемника мы не развиваем. Если автор вернётся и продолжит разработку, наша версия будет удалена, чтобы не путать игроков.

### Возможности и настройки

Сохранены исходные возможности автора: предварительная загрузка местности, подготовка путей, планирование маршрутов, движение отрядов, графические оптимизации, игровые правки и работа с рюкзаками. Настраиваются в **MCM > KEO**; без MCM остаётся исходная вкладка **Настройки > KEO**. Интерфейс поддерживает русский, английский и упрощённый китайский; для остальных языков используется английский.

Группы настроек:

- **Загрузка местности:** предварительная подгрузка, потоки подготовки путей, радиусы и лимиты удерживаемых областей, подготовка маршрута вперёд и размер дискового кэша.
- **Производительность:** бюджеты обработки частиц и окружения. Фиксированный прирост FPS не обещается.
- **Движение отряда:** маршрутизация KEO, согласованное движение, темп сбора группы, возобновление движения после боя и режим планировщика. При выборе пути учитываются плавание и опасная вода. Дальние переходы ещё могут прерываться; повторный приказ помогает продолжить движение.
- **Игровые исправления:** принадлежность построек игрока, повторяющийся вынос нарушителей и пути возле стен. При повторной загрузке городов сохраняются штатные проверки готовности пути.
- **Рюкзаки и работа:** работа с рюкзаками, предел нагрузки перед доставкой, переносимый вес и три независимых задания.
- **Журнал:** последний переключатель, по умолчанию выключен. Ошибки остаются видимыми. Включайте подробный журнал для воспроизведения проблемы и выключайте после сбора данных.

MCM отмечает параметры, которым нужен перезапуск. Переносимый вес требует перезапуска; три задания включаются и выключаются во время игры. Расширенные настройки описаны в шаблоне `KEO.defaults.ini` из пакета. KEO создаёт личный файл настроек `KEO.ini`; сохраняйте его при обновлении.

### Автоматическое восстановление путей

**Автовосстановление путей** (Pathing repairs itself, название из описания
KenshiQOL) включается в **MCM > KEO > Исправления игры**. Настройка действует
без перезапуска, по умолчанию ВЫКЛЮЧЕНА. После загрузки сохранения каждая реально
посещённая активная зона один раз получает аналог Ctrl+Shift+F11: штатный ремонт
построек/мебели, затем полную генерацию внешней навсетки и подходящих внутренних
помещений. Ремонт, как у исходной команды, охватывает все активные зоны и может
ненадолго задержать кадр. Между заявками есть пауза; при занятой загрузке мира,
физике или навигации новая заявка ждёт. Генерация использует существующие потоки
KEO и требует кэширования навсетки. Штатные сообщения загрузки сохранены.
Персонажей не телепортирует и решение всех проблем поиска пути не гарантирует.
Фоновые предзагрузки не считаются посещением. Выключение/включение сохраняет
историю; загрузка сохранения или нового мира её очищает.

### Подготовка ресурсов и загрузка

Группа **Память ресурсов** содержит необязательные подготовку файлов в ОЗУ,
стартовый прогрев, подготовку соседнего рельефа и удержание загруженной геометрии
моделей/текстур в видеопамяти. Эти настройки памяти по умолчанию ВЫКЛЮЧЕНЫ.
Сначала включите подготовку ресурсов в ОЗУ, затем при необходимости стартовый
прогрев. Он читает зарегистрированные локальные модели, скелеты и текстуры в
пределах выбранного лимита; достижение предела не означает загрузку всей игры.
Прогрев не создаёт NPC заранее. Пределы ОЗУ/видеопамяти и скорость фонового
чтения настраиваются; оставляйте память самой игре. Настройки применяются на
ходу. **Показывать текст прогрева** выводит текущую работу слева сверху;
после её завершения текст скрывается, а при новых запросах появляется снова.

В группе **Производительность** находятся ускорения подготовки памяти скелетов,
моделей и материалов, весов и таблиц костей, форм тела, чтения снаряжения и
анимации, а также разбора сейвов. Отдельные опции отключаются на ходу.
Для неподдерживаемых случаев сохраняется штатная обработка. Симуляцией и
порядком появления NPC по-прежнему управляет игра; весь этот процесс не
переносится в рабочие потоки.

Владелец17 ноября2026 подтвердил более быструю загрузку сейвов и более плавные
переходы между прогретыми областями. Это игровая проверка совокупного обновления,
а не контролируемый коэффициент ускорения или обещание одинакового результата
для любого сейва. Холодные области, физика, появление NPC и отрисовка могут
по-прежнему давать паузы. Общая оптимизация FPS развитой катки остаётся отдельной
задачей. Подробные проверки реализации и трёх этапов подготовки костей сохранены
в отчётах `leopard` соответствующих исходников; результаты отдельных функций
не являются коэффициентами ускорения всей загрузки.

### Подготовка весов и топологии моделей

**Ускорять подготовку моделей** (`meshWeights`) по умолчанию включено и
переключается на ходу. Опция объединяет линейную нормализацию весов в штатном
порядке, импорт упорядоченных привязок без повторного поиска от корня и сбор
таблиц костей локальной битовой структурой размером 8320 байт. При загрузке из
поддерживаемого потока памяти полные записи треугольников читаются за один
проход. Ogre увеличивает выходные массивы и читает группы рёбер. Частичные
чтения, другие типы потоков и другой порядок байтов обрабатываются штатно.
Веса, порядок одинаковых индексов и ранги костей сохраняются.
Узлами импорта и ростом выходных массивов управляет Ogre; старые промежутки
таблиц и частичное состояние при ошибке выделения памяти сохранены. Необычные
веса, неупорядоченный импорт и неподдерживаемые случаи оставляют соответствующий
этап штатным. Постоянного кэша, покадрового обхода и нового потока нет. Опция
действует на вновь подготавливаемые модели; время отдельного этапа не является
коэффициентом ускорения всего сейва.

### Необязательный вес и задания

В одном пакете с KEO объединены полезные функции **KenshiQOL автора Steel**. Спасибо Steel за исходные возможности и предоставленный код.

- [Оригинальный KenshiQOL](https://steamcommunity.com/sharedfiles/filedetails/?id=3772744830)
- **Исходники оригинала:** папка `src` в поставке мода, в том числе `KenshiQOL.cpp` и `TaskTypeFactoryPatch.h`. [Поставка/публикация исходников автора](https://steamcommunity.com/sharedfiles/filedetails/comments/3772744830). Отдельный репозиторий не подтверждён.

Все четыре добавления **по умолчанию выключены**. Включайте нужные в разделе **Рюкзаки и работа**.

**Вес переносимых тел и контейнеров** учитывает содержимое переносимых контейнеров и персонажей. Вес тела зависит от расы, возраста/размера и голода. Снижение веса надетым рюкзаком сохраняется. После изменения нужен перезапуск.

**Забирать продукцию:** команда ПКМ производственной машины; с SHIFT добавляется постоянная задача. Несколько машин можно назначить одному сборщику. Используется настоящая продукция машин, новые предметы не создаются.

**Пополнять прилавок:** команда ПКМ собственного торгового прилавка; с SHIFT добавляется постоянная задача. Доставляются имеющиеся у работника предметы, соответствующие продуктам назначенных задач сбора. Без таких задач используется штатная классификация ресурсов. Для доставки от производства назначьте и сбор, и пополнение. Произвольные вещи из хранилищ не забираются; денежная сделка и доход не создаются. Надетое снаряжение не является товаром. Заполненные и несовместимые прилавки пропускаются; ошибка переноса останавливает эту операцию вместо повторяющихся попыток.

**Маршрут по точкам:** SHIFT + ПКМ по свободной земле добавляет точку выбранным в этот момент персонажам. Каждая точка - обычная строка задач с координатами. Перетаскивайте строки для смены порядка или удаляйте отдельные точки. Маршрут зациклен и сохраняется вместе с игрой. До 64 точек на персонажа. Выключение приостанавливает его, сохраняя строки. При выключенной функции обычное передвижение доступно как прежде.

Первую проверку сбора и доставки выполняйте в отдельном сейве. Циклический маршрут и восстановление при обычном сохранении/загрузке подтверждены локальным тестом Steam. Физический цикл сбора/пополнения, все сочетания модов и производительность долгих сессий не заявляются универсально проверенными.

### Установка и совместимость

1. Установите **RE_Kenshi 0.3.5 или новее** для поддерживаемой **Kenshi 1.0.65**. [RE_Kenshi](https://www.nexusmods.com/kenshi/mods/847).
2. Отключите оригинальный KEO и KenshiQOL. Используйте одну реализацию KEO.
3. Распакуйте папку `KEO` в игровой каталог `mods`, включите мод в лаунчере Kenshi и запускайте игру способом, предусмотренным RE_Kenshi для вашей установки.
4. Для страницы настроек установите Mod Config Menu. Он необязателен: без него действуют значения по умолчанию и исходная вкладка KEO.

Одна DLL автоматически выбирает поддерживаемый профиль Steam/GOG. Нужны соответствующие RE_Kenshi/KenshiLib. Для GOG сейчас допускается проверенный совместимый с RE_Kenshi исполняемый файл GOG 1.0.65 и **KenshiLib 0.5.0**. Другие файлы игры и SDK не принимаются за заведомо совместимые. Исполняемые файлы игры и DLL библиотеки в пакет не входят.

Текущий выпуск проверен в игре на поддерживаемой сборке GOG. Изменения загрузки и работы «Пополнять прилавок» описаны в заметках выпуска 7.

Оригинальный KenshiQOL одновременно с перенесёнными весом/заданиями не запускайте. Маршруты хранятся в обычных записях задач; импорт мира не гарантирует сохранность таких записей. При обновлении не заменяйте личные настройки шаблоном.

### Сообщение о проблеме

Включите подробный журнал, воспроизведите проблему один раз и приложите `KEO.log` из папки мода, игровой `RE_Kenshi_log.txt` и `crash_dump.txt`, если он есть. Укажите магазин/версию, список модов и точные действия. Ближайшая строка журнала сама по себе не доказывает виновника ошибки.

### Изменения выпуска 1

- Первый сопровождающий форк KEO: необязательная настройка через MCM, русский/английский/китайский интерфейс и подробный журнал, выключенный по умолчанию.
- Steam и GOG поддерживаются одним пакетом. GOG работает: запуск более ранней сборки подтверждён, но широкая проверка нынешних игровых возможностей ещё не завершена.
- Исправлены старое управление маршрутом у персонажей сверх лимита группы, применение устаревшей точки пути и обработка штатной готовности пути при повторной загрузке городов.
- Необязательный вес переносимых тел/контейнеров и три отдельных задания из KenshiQOL автора Steel: сбор продукции, пополнение прилавка и циклический маршрут. По умолчанию выключены.
- Циклический маршрут использует обычные строки задач, различает отдельные точки и сохраняется при обычном сохранении/загрузке. Пополнение прилавка физически переносит вещи и не создаёт сделку или доход.

### Лицензия и соответствующие исходники

GPL-3.0, файл `LICENSE`. Файлы авторства и происхождения кода входят в пакет. Соответствующие исходники и скрипты сборки находятся в общей репе наших форков: https://github.com/xxxLEOPARDxxx/LEOPARD-Kenshi-Forks/tree/main/KEO . Ссылки на исходные KEO и KenshiQOL выше указывают код авторов; в этом пакете содержится изменённая интеграция.

## Release 2 / Выпуск 2

### English

- Resource-cache eviction no longer scans all retained models or textures for every removed item. The same eviction order, memory limits and native resource lifetimes are preserved. This reduces cache-maintenance work as the cache fills; it does not eliminate native NPC creation work.
- Avoids repeated temporary allocation when a squad has no timed requests. This optimization preserves active requests, jobs, AI and squad update frequency. Its live switch is under Performance; supported Steam builds only.
- Four independent startup hooks now share one verified SDK queue/apply operation. KEO uses a local matching-module adapter for the SDK wrapper defect; unsupported SDK builds keep the immediate path. Coupled hooks retain their existing order. Partial failures retain original functions and do not announce unconfirmed installation success.
- Startup RAM warmup now reads model and skeleton files before textures, so a full texture cache does not consume the startup budget before model files are considered. It still stops at the RAM limit and gives native game requests priority. This prepares file bytes; NPC creation, model decoding and geometry upload remain native work.
- Set operator cargo limits by weight and occupied inventory and backpack cells in steps of 5%. A supported operator delivers when either limit is reached.
- Right-click the backpack checkmark to set an individual cell limit or restore the global one. A green checkmark indicates an individual limit.
- With MCM installed, KEO settings are available on its MCM page. The standalone tab is available without MCM.

### Русский

- При вытеснении ресурсов больше не перебирается весь кэш моделей или текстур ради каждого удаляемого элемента. Порядок вытеснения, лимиты памяти и время жизни ресурсов сохранены. Это снижает расходы на обслуживание заполненного кэша, но не устраняет собственную работу движка по созданию NPC.
- Убраны повторные временные выделения памяти у отрядов без заявок с таймером. Активные заявки, работы, ИИ и частота обновления отрядов сохраняются. Переключатель работает сразу в разделе производительности; только на поддерживаемых сборках Steam.
- Четыре независимых хука запуска теперь устанавливаются одной проверенной пакетной операцией SDK. Локальный адаптер KEO исправляет несовпадение модулей в обёртках SDK; при неподдерживаемой версии остаётся прежняя установка. Связанные хуки сохраняют прежний порядок. Частичный отказ не теряет оригинальные функции и не выдаётся за успешную установку.
- Стартовый прогрев ОЗУ теперь читает файлы моделей и скелетов раньше текстур: текстуры не занимают весь стартовый бюджет до рассмотрения моделей. При достижении лимита прогрев останавливается, запросы самой игры имеют приоритет. Подготавливаются байты файлов; создание NPC, разбор моделей и загрузку геометрии выполняет сама игра.
- Для операторов поддерживаемых станков можно настроить предел по весу и долю занятых ячеек инвентаря и рюкзака с шагом 5%. Доставка начинается по первому достигнутому пределу.
- ПКМ по галочке рюкзака открывает личный лимит ячеек и позволяет вернуться к общему. Зелёная галочка показывает, что задан личный лимит.
- При установленном MCM настройки доступны на странице KEO в MCM. Отдельная вкладка доступна без MCM.

### Startup RAM warmup reference / Справка по стартовому прогреву ОЗУ

The live option **Warm resources at startup** keeps its existing INI key `resourceStartupTextures` for compatibility. The default remains OFF. It requires the RAM resource cache and scans only copied, registered local filesystem roots. `.mesh` and `.skeleton` files share the existing 262144-file inventory limit with supported textures and are read first after the scan. The byte cache retains the configured RAM limit, memory reserve, per-file limit, native-request priority, cancellation and immutable-file identity checks. Startup never evicts existing cached files to cycle through the library. At the first capacity refusal it ends; that does not mean every resource was loaded. **Show resource warmup text** displays the active startup scan/read or pending RAM/VRAM requests, then automatically hides when work finishes, including when the RAM limit ends startup. Retained cached files, models and textures do not keep it on screen. New resource requests make it reappear; after startup it shows current work rather than the old startup percentage. The option does not create unseen NPCs, parse meshes, share character materials or preload model geometry on a worker. RAM file warmup does not remove save deserialization, world restoration, physics or per-character appearance work. Its actual effect on save-load time needs a separate measurement; source fixtures do not establish a gameplay speedup.

Переключатель **Прогрев ресурсов при запуске** работает без перезапуска и сохраняет прежний ключ INI `resourceStartupTextures`. По умолчанию выключен; требует кэш ресурсов в ОЗУ. Обходятся только скопированные зарегистрированные локальные папки. Файлы `.mesh` и `.skeleton` делят прежний лимит списка 262144 файла с поддерживаемыми текстурами и читаются первыми после сбора списка. Сохраняются заданный лимит ОЗУ, резерв свободной памяти, предел одного файла, приоритет запросов игры, отмена и проверки неизменности файла. Стартовый прогрев не вытесняет готовые записи ради циклического обхода библиотеки: при первом отказе по вместимости он заканчивается. Это не означает, что загружены все ресурсы. **Показывать текст прогрева** отображает активный стартовый обход/чтение или ожидающие запросы RAM/VRAM, затем автоматически скрывается после завершения работы, в том числе при остановке стартового прогрева по лимиту ОЗУ. Сохранённые в кэше файлы, модели и текстуры не удерживают надпись на экране. При новых запросах она появляется снова и после стартового прогрева показывает текущую работу вместо старого процента. Прогрев не создаёт невидимых NPC, не разбирает модели, не объединяет материалы персонажей и не загружает геометрию моделей в рабочем потоке. Он не устраняет разбор сохранения, восстановление мира, физику или индивидуальную подготовку внешности. Влияние на время загрузки сохранения требует отдельного замера; проверки исходников не доказывают ускорение игры.

## Local NPC loading correction / Локальная правка загрузки NPC (2026-11-16)

MCM > KEO > Performance > Prepare NPC skeleton storage (default ON, live):
prepares the empty native bone-pointer array once for a newly loaded skeleton.
The game still creates every bone, hierarchy, binding pose and animation itself;
no NPC is shared, moved to another thread or made ready early. The optimization
uses an admitted native engine contract and falls back to normal loading when
unsupported. No controlled game load-time or FPS gain has been measured.

MCM > KEO > Производительность > Подготовка памяти скелетов NPC (включено по
умолчанию, применяется без перезапуска): заранее подготавливает пустой массив
указателей на кости нового скелета. Все кости, их иерархию, исходную позу и
анимацию по-прежнему создаёт сама игра. NPC не разделяются между персонажами,
не создаются в другом потоке и не объявляются готовыми раньше времени.
Неподдерживаемая сборка движка сохраняет обычную загрузку. Ускорение загрузки
сейва и изменение FPS в игре пока не измерены.

Implementation: only an empty Skeleton bone vector is reserved to the master's
full handle span1..256. No resize, bone creation, root enumeration or persistent
cache. The SDK OldSkeletonInstance tail differs from the installed engine;
its master SharedPtr/member order/size/vtable are deliberately not used. The
borrowed master pointer is copied from the admitted native field+0x290; only
the separately verified Skeleton vector prefix+0x178 uses the SDK accessor.
Optional failures forward native loading exactly once; native exceptions remain
native. Main-owner and live setting gates precede preparation. See private
leopard/skeleton-reserve-20261116.md for verification and limits.

## Local model preparation / Локальная подготовка моделей (2026-11-16)

MCM > KEO > Performance > Prepare model storage (default ON, live) prepares
empty model-part and shape pointer lists once before native cloning. The engine
still copies every part and shape, blends the appearance, applies materials,
creates animation/render objects and destroys temporary shape data itself.
This is separate from retaining geometry in VRAM. It adds no worker or queue,
changes no spawn/readiness policy and does not accelerate save deserialization.
The actual game load-time/FPS benefit remains unmeasured.

MCM > KEO > Производительность > Подготовка памяти моделей (включено по умолчанию,
применяется без перезапуска) заранее подготавливает пустые списки частей модели
и данных формы перед обычным копированием. Копирование частей и форм, изменение
внешности, материалы, анимацию, объекты рендера и удаление временных данных
по-прежнему выполняет сам движок. Это отдельная оптимизация от удержания
геометрии в VRAM. Новых потоков и очередей нет; порядок появления и готовности
NPC сохранён. Ускорение загрузки сейвов и изменение FPS в игре пока не измерены.

Native details: verified Mesh::clone + MeshManager::createManual pair, raw
member hidden-result ABI, owner-only scoped source, exact native clone caller,
loaded/distinct source/target and empty list admission. Reserve only, capped
at64SubMeshes and512Poses (at most4608pointer bytes before native append loops).
Native exceptions and ownership stay native; preparatory failures are optional
and cannot suppress forwarding. Partial installation keeps trampolines and
remains forwarding-only without retry. See leopard/model-clone-reserve-20261116.md.

## Geometry cache admission / Добавление моделей в кэш (2026-11-16)

Loaded-model retention now transfers its collected buffer description directly
into the admitted cache entry, avoiding three redundant list allocations and
copies per model. Refused admission preserves the description for the existing
bounded retry. Cache limits, model ownership, unload/refresh rules and native
loading stay unchanged. This reduces KEO bookkeeping; no measured save-load
or gameplay frame-time gain is claimed.

При добавлении загруженной модели в кэш готовый список её буферов теперь
передаётся напрямую, без трёх лишних выделений памяти и копирований списка.
При отказе список сохраняется для прежней ограниченной повторной попытки.
Лимиты кэша, владение моделями, обновление и освобождение ресурсов, а также
обычная загрузка движка сохранены. Это сокращает служебную работу KEO;
ускорение полной загрузки сейва и кадровое время в игре не измерялись.

## Native save-file input / Чтение файлов сохранения (KEO 3 implementation, 2026-11-16)

**Batch save file reads** is a live, default-ON Performance option. For newly
opened, supported main-thread save streams it requests a 64 KiB native CRT buffer
when the file is at least 128 KiB. Small files keep ordinary buffering. It uses the
already-open handle for one size query without opening, reading or seeking.
The parser, format, values, error/EOF behavior and restoration order stay native.
Nontrivial text conversion, already-used/foreign streams and other engine builds
retain normal behavior. Disabling affects subsequent opens, not a file already
being parsed. The native CRT owns the buffer and frees it at close; there is no
new cache, worker, per-frame scan or retained save-file handle. Local stream tests
show 50 versus 4 refills for 200003 single-byte reads, not measured game-load gains.
One metadata query per eligible file is an added cost; the size threshold is a
conservative policy, not a profiled optimum. Details: `leopard/save-read-verification-20261116.md`.

On verified Steam and GOG builds, the option also groups consecutive fields
already in the native input buffer. One 28-byte read covers position and rotation.
Each of two 8-byte reads covers the record type and numeric ID or the last two
reference arguments. The engine still checks the file version before deciding
whether the reference has the additional arguments. Partial or unbuffered fields
use the original two reads.

**Укрупнять чтение сохранений** - настройка раздела «Производительность», включена
по умолчанию и работает без перезапуска. Для новых поддерживаемых потоков чтения
сейва в основном потоке запрашивается штатный буфер 64 КиБ, если файл не меньше
128 КиБ. Маленькие файлы читаются как прежде. Размер запрашивается через уже
открытый дескриптор без нового открытия, чтения и перемещения позиции. Формат,
значения, правила разбора, ошибки и порядок восстановления остаются штатными.
Потоки с преобразованием символов, уже использованные или неподдерживаемые потоки
не меняются. Выключение применяется к следующим открытиям файлов. Буфер освобождает
сама стандартная библиотека при закрытии файла; нового кэша, рабочего потока или
покадрового обхода нет. Проверки вне игры подтверждают 4 вместо 50 пополнений буфера
для 200003 однобайтовых чтений. Ускорение загрузки игры не измерялось; дополнительный
запрос размера имеет собственную стоимость.

На проверенных сборках Steam и GOG соседние поля из штатного буфера читаются
вместе. Координаты и поворот читаются одним вызовом на 28 байт. Тип записи и её
числовой ID, а также два последних параметра ссылки читаются парами по 8 байт.
Наличие дополнительных параметров движок по-прежнему определяет по версии файла.
При нехватке данных в буфере движок читает поля по отдельности.

## Private body shape preparation / Подготовка форм тела (2026-11-16)

**Reuse temporary body shape data** (Performance, default ON, live) avoids deep
copies of shape maps which native body preparation only reads and then discards.
Every character still receives its own mesh and Entity. Shape weights, shaved
rules, appearance randomness, equipment, materials, animation, native vertex
blending and the rendered result remain unchanged. Unsupported engine paths use ordinary
native copies. This targets body construction during NPC/save loading; a game
load-time or frame-time improvement has not been measured.

**Повторное использование форм тела** (Производительность, включено по умолчанию,
работает без перезапуска) убирает глубокое копирование данных форм, которые
подготовка тела только читает и затем удаляет. У каждого персонажа сохраняются
собственные модель и объект рендера. Веса форм, правила выбритой головы,
случайность внешности, снаряжение, материалы, анимация, расчёт вершин и работа
GPU дают тот же результат. Неподдерживаемые пути движка используют обычные копии.
Правка относится к созданию тела при загрузке NPC и сейва; ускорение в игре
пока не измерялось.

Implementation: only the admitted private createMorphedMesh / Mesh::clone call
sequence may borrow at most512 source Pose pointers within at most8 nested
scopes. Actual source/destination control blocks are pinned before borrowing.
All-context cleanup removes positively recorded borrowed pointers before native
deletion, including unload, mixed lists and handled SEH. Both added pins are
released at the final pose-removal boundary before the old Entity is destroyed,
preserving native use-count-sensitive cleanup. No pose maps are mutated and no
worker or persistent cache is added. Existing capture wrappers
publish functional readiness independently of Debug; partial hook failure retains
every published original but never admits the optimization. See private
leopard/morph-pose-scope-verification-20261116.md and integration review evidence.

The same option now batches shadow-buffer uploads during the exact private body
morph. Ogre still applies every pose with its original arithmetic and locks.
For a distinct, unlocked destination POSITION buffer with an independent RAM
shadow, hardware updates are suppressed until the final pose-removal boundary.
The complete result uploads before mesh switching; live OFF and exceptional
cleanup restore the flag and release the one temporary buffer lease. Other
buffers retain native behavior. This removes intermediate body-buffer uploads;
it does not parse saves, prepare equipment or parallelize NPC spawning. Offline
native checks establish byte equivalence and upload counts, not a measured game
load-time/FPS improvement. See `leopard/morph-upload-verification-20261116.md`.

Та же опция теперь объединяет отправку изменений формы тела из копии буфера в
ОЗУ на GPU. Каждая форма рассчитывается штатной функцией Ogre. Промежуточные
отправки подавляются только у отдельного незаблокированного буфера новой модели;
готовый результат отправляется до смены модели. Исключения и выключение опции
не оставляют флаг подавления и временную ссылку. Другие буферы работают как раньше.
Разбор сейва, подготовка снаряжения и порядок появления NPC не меняются. Проверки
вне игры подтверждают совпадение байтов и сокращение отправок, но не измеренное
ускорение загрузки или FPS.


### Local NPC loading review follow-up (release 3 implementation, 2026-11-16)

Temporary body-shape cleanup now verifies and clears an unchanged borrowed list
in one linear pass, preserving the existing mixed/changed-list ownership fallback.
Geometry retention preserves a published native trampoline on hook-enable failure
and requires a valid original before admission. No new setting or worker is added.
Independent native and resource lifecycle reviews, actual Ogre cleanup checks,
exact install-policy fixtures and full builds are documented in
`leopard/npc-loading-review-20261116.md`. These are offline correctness/work-count
results; save loading, equipment construction and gameplay performance remain
unmeasured by this pass.

### Локальное ревью загрузки NPC (реализация выпуска 3, 16.11.2026)

Очистка временных форм тела проверяет неизменённый список за один линейный проход.
Для смешанных и изменённых списков сохранена прежняя проверка принадлежности.
Кэш моделей сохраняет полученный указатель оригинальной функции при отказе
включения хука и допускает работу только с действительным оригиналом. Новых
настроек и потоков нет. Выполнены независимые проверки владения ресурсами,
проверки на настоящих объектах Ogre и полная сборка. Ускорение загрузки сейва,
сборки снаряжения и изменение FPS в игре в этом проходе не измерялись.


### Copied material storage (release 3 implementation, 2026-11-16)

Performance: **Prepare copied material storage**, enabled by default and live.
Prepares a bounded texture-layer pointer array while a new material pass is
copied. Native material/texture copies, colours, shader setup and loading order
remain in control of Ogre. This removes repeated array-growth allocations for
eligible new hair/item material copies; existing item-material cache hits already
skip cloning. It does not skip NPC/equipment creation or save restoration.
See `leopard/material-reserve-verification-20261116.md` for native ownership,
exception, allocator, ABI and test evidence and its limits. No new gameplay
load-time/FPS measurement is asserted.

### Подготовка копий материалов (реализация выпуска 3, 16.11.2026)

Настройка в разделе производительности включена по умолчанию и применяется сразу.
При создании новой копии прохода материала заранее подготавливается небольшой
массив указателей на текстурные слои. Материалы и текстурные слои копирует движок;
цвета, шейдеры и порядок загрузки сохраняют штатную обработку. Это убирает
повторное расширение массива у подходящих новых материалов волос и предметов.
Для уже найденных в кэше материалов предметов клонирование и так пропускается.
Создание NPC, снаряжения и восстановление сейва не пропускаются. Скорость
загрузки и FPS в игре этим проходом не измерялись.


### Geometry refresh bookkeeping (release 3 implementation, 2026-11-16)

Retained, unchanged models are checked directly against their stored buffer
inventory, avoiding temporary descriptor-vector allocations on each refresh.
Changed resources still use the existing full collection and admission path.
No setting, cache limit, native model/equipment creation or save restoration
policy changes. Native fixtures, fault recovery and ownership review are recorded
in `leopard/geometry-refresh-verification-20261116.md`; no new gameplay loading
or FPS measurements were performed.

### Проверка кэша геометрии (реализация выпуска 3, 16.11.2026)

Для неизменившейся модели кэш проверяет уже сохранённый список буферов без
повторного выделения временного массива. При изменениях описание собирается
прежним способом с повторной проверкой и учётом памяти. Настройки, лимиты,
штатное создание моделей и снаряжения, восстановление сейва не меняются.
Выполнены проверки на настоящих объектах Ogre, проверки отказов и владения
ресурсами. Скорость загрузки и FPS в игре этим проходом не измерялись.

### CPU save-state strings (release 3 implementation, 2026-11-16)

Performance: **Transfer temporary save strings**, enabled by default and live.
Eligible temporary state lists pass directly into fresh saved objects, avoiding
per-string copying and destination-array growth. Smaller or ineligible lists
retain native handling; remaining long by-value strings can transfer without
an extra copy into fresh slots. Source values, save format, metadata assignments
and loading order stay native. No worker, cache or hot-path allocation/free is
added. Retained list capacity never exceeds the native destination growth size.
See `leopard/save-state-list-verification-20261116.md` and the preceding
`leopard/save-string-verification-20261116.md`. These offline ownership/ABI and
copy-count checks do not measure total save loading time or gameplay FPS.

### Строки состояния при разборе сейва (реализация выпуска 3, 16.11.2026)

Настройка **Передавать временные строки сейва** в разделе производительности
включена по умолчанию и применяется сразу. Подходящие временные списки состояний
целиком передаются новым объектам без копирования каждой строки и расширения
второго массива. Маленькие и неподходящие списки сохраняют штатную обработку;
оставшиеся длинные временные строки могут передаваться в свежие слоты без
лишнего копирования. Значения, формат сейва, присваивание прочих полей и порядок
загрузки остаются штатными. Новых потоков, кэшей и выделений памяти при переносе
нет. Запас массива ограничен размером, который оставил бы штатный код.
Проверки вне игры подтверждают владение памятью и сокращение копирования;
общее время загрузки сейва и FPS в игре этим проходом не измерялись.


### Equipment property reads / Чтение свойств снаряжения

The live, default-enabled **Speed up equipment property reads** setting reduces repeated hashing for three supported equipment-assembly queries. Existing values return their original address; absent properties, defaults, other keys and unsupported engine builds retain native handling. It adds no worker, retained map pointers or lookup cache. This is a small CPU improvement, not a measured whole-NPC loading or FPS gain.

Настройка **Ускорить чтение свойств снаряжения** включена по умолчанию и переключается без перезапуска. Для трёх поддерживаемых обращений при сборке снаряжения устранён повторный расчёт хеша. Существующие значения возвращаются по штатному адресу; отсутствующие поля, значения по умолчанию, другие ключи и неподдерживаемые версии движка обрабатываются штатно. Новых потоков, сохраняемых указателей на таблицы и кэша поиска нет. Это небольшое сокращение CPU-работы; ускорение всей загрузки NPC и рост FPS игровыми замерами не подтверждались.

Implementation and isolated validation: `leopard/equipment_field_verification.md`, `leopard/cpu-loading-verification-20261116.md`.


### Saved-object lookup reuse / Повторное использование поиска объектов сейва

**Reuse saved object lookups** is enabled by default and switches live. During supported saved-object restoration, KEO passes the first tree-search result to the following native insertion path. This removes one repeated tree descent per admitted object; construction, default values, insertion, fields and cleanup remain native. Other callers and unsupported builds retain normal lookup. No lookup cache, worker or world pointer is retained.

**Повторно использовать поиск объектов сейва** включено по умолчанию и переключается без перезапуска. При восстановлении поддерживаемых объектов результат первого поиска передаётся следующей штатной операции вставки. Убирается один повторный обход дерева на объект; создание, значения по умолчанию, вставка, поля и очистка остаются штатными. Другие вызовы и неподдерживаемые версии используют обычный поиск. Кэш поиска, новый поток и сохраняемые указатели на мир не добавляются.

Isolated native-frame tests prove fewer tree searches; no gameplay save-load timing or FPS gain is claimed. Проверки подтверждают сокращение поисков, но не измеряют время загрузки в игре. Evidence: `leopard/save-instance-lookup-verification-20261116.md`.


### Animation state preparation / Подготовка состояний анимации

**Avoid repeated animation state searches** is enabled by default and switches live. When preparing a model, a new state whose numeric ID follows the current maximum can use native insertion at the table end. Empty tables also qualify. Other IDs keep the normal search. State creation, values, dirty notifications, tree balancing and existing locks are preserved. This removes a repeated search during eligible model/equipment initialization; it adds no background worker, retained object or per-frame sweep.

**Убрать повторный поиск состояний анимации** включено по умолчанию и переключается без перезапуска. При подготовке модели новое состояние с ID больше текущего максимума добавляется штатной вставкой в конец таблицы; пустые таблицы также подходят. Остальные ID используют обычный поиск. Сохраняются создание состояний, параметры, уведомления об изменениях, балансировка дерева и существующие блокировки. Убирается повторный поиск при подходящих инициализациях моделей и снаряжения. Новый фоновый поток, сохраняемые объекты и покадровый обход не добавляются.

The current table is checked after the state constructor; its earlier lookup result is never cached across possible allocation callbacks. Real Ogre DLL fixtures exercise native insertion, duplicate and size-error cleanup, plus the installed CALL bridge in an isolated process. No game was launched and whole-NPC/save-load/FPS gains remain unmeasured. Private details: `leopard/animation-state-lookup-verification-20261116.md`.


### Save fields and held animation locks / Поля сейва и блокировки анимации

Release 3 implementation, code dated 2026-11-16: **Skip discarded save field keys** avoids the first temporary key in six native field readers only when the permission filter is empty. Final key construction, permissions and insertion remain native. **Transfer temporary save strings** also transfers eligible long string/file field buffers into fresh fields after native map insertion; repeated fields and unsuitable buffers retain normal assignment. These live options default to enabled.

**Reuse held animation lock** reduces recursive mutex bookkeeping during model animation-state creation only when the same thread already owns that exact native parent lock. The outer lock, dirty counter semantics, creation and cleanup remain native; standalone or other-thread calls keep normal notification. It works together with the existing animation lookup option. No new workers, retained world pointers, per-frame work or growing caches are introduced. Pinned Steam engine/Ogre contracts fail closed to native handling. Isolated VC10/Ogre boundary, exception, ownership and concurrency tests are not measurements of save-load time, NPC stutters, gameplay FPS or long-term memory usage.

Реализация выпуска 3, код от 16.11.2026: **Пропускать ненужные ключи полей сейва** устраняет первый временный ключ в шести штатных обработчиках полей только при пустом фильтре разрешений. Итоговый ключ, проверки разрешений и вставка остаются штатными. **Передавать временные строки сейва** также передаёт подходящие буферы длинных строковых полей и имён файлов свежим полям после штатной вставки в таблицу. Повторные поля и неподходящие буферы используют обычное присваивание. Настройки включены по умолчанию и переключаются без перезапуска.

**Использовать удерживаемую блокировку анимации** сокращает повторный учёт рекурсивной блокировки при создании состояний анимации модели, только если текущий поток уже удерживает именно эту блокировку родителя. Внешняя блокировка, счётчик изменений, создание и очистка остаются штатными; отдельные вызовы и другие потоки используют обычное уведомление. Совместимо с существующей оптимизацией поиска состояний. Новых потоков, сохраняемых указателей на мир, покадровой работы и растущих кэшей нет. Проверки поддерживаемой Steam/Ogre сборки при несовпадении оставляют штатную обработку. Изолированные проверки VC10/Ogre не доказывают ускорение полной загрузки сейва, устранение игровых рывков, рост FPS или отсутствие любых утечек долгой сессии.


## Release 3 / Выпуск 3

English

- Added optional resource preparation in RAM, startup warmup and advance preparation of nearby terrain. Reusing warmed resources reduces loading interruptions when revisiting areas.
- Added bounded retention of loaded model geometry and textures in video memory, with configurable limits.
- Accelerated startup before the main menu and startup resource preparation.
- Reduced CPU work when loading saves and preparing NPC skeletons, models, body shapes, equipment and bone data.
- Warmup text shows current progress and disappears when preparation finishes; it reappears for new queued work.
- Reduced redundant resource bookkeeping and corrected resource lifetime, cancellation and compatibility fallback handling.
- Updated Rotate Items compatibility for counter stocking. Unavailable stocking no longer disables production collection or waypoint routes.

Русский

- Добавлены необязательная подготовка ресурсов в ОЗУ, стартовый прогрев и предварительная подготовка соседнего рельефа. Повторное использование прогретых ресурсов уменьшает задержки при возвращении в уже посещённые области.
- Добавлено удержание загруженной геометрии моделей и текстур в видеопамяти с настраиваемыми пределами.
- Ускорены запуск до главного меню и стартовая подготовка ресурсов.
- Уменьшена работа CPU при загрузке сейвов и подготовке скелетов, моделей, форм тела, снаряжения и данных костей NPC.
- Текст прогрева показывает текущий прогресс и исчезает после подготовки; при новых запросах появляется снова.
- Убрана лишняя обработка ресурсов, исправлены их освобождение, отмена загрузки и переход к штатной обработке при несовместимости.
- Обновлена совместимость пополнения прилавка с Rotate Items. Недоступность пополнения больше не отключает сбор продукции и маршруты по точкам.

## Release 4 / Выпуск 4

### English

- Added optional automatic Ctrl+Shift+F11-style building repair and full exterior/interior navmesh generation once per visited zone after each save load.
- Reuses KEO background generation, waits for idle loading/physics/navigation, excludes speculative preloads and spaces requests.
- The live setting is named Pathing repairs itself, following KenshiQOL's public feature name, and is off by default.
- Added English, Russian and Simplified Chinese help and optional detailed per-operation timings. Personal settings are preserved.
- Corrected the package loader manifest so it no longer requests a compatibility DLL absent from the package.

### Русский

- Добавлены необязательный автоматический ремонт построек по Ctrl+Shift+F11 и полная генерация внешней/внутренней навсетки один раз для каждой посещённой зоны после загрузки сохранения.
- Используются фоновые потоки KEO; при занятой загрузке мира, физике или навигации новая заявка ждёт. Фоновые предзагрузки исключены, между заявками есть пауза.
- Настройка «Автовосстановление путей» (Pathing repairs itself, название из описания KenshiQOL) переключается без перезапуска и по умолчанию выключена.
- Добавлены подсказки на русском, английском и китайском и необязательные подробные замеры отдельных операций. Личные настройки сохраняются.
- Исправлен манифест загрузки: убрана ссылка на DLL совместимости, отсутствующую в пакете.

### Automatic repair technical reference / Техническая справка

The fixed4096-byte requested bitmap contains cell IDs only, no borrowed pointers.
Discovery examines at most64 grid slots per250ms, one active-set walk and one
section readiness query per pass. Accepted requests are at least5s apart, timed
from native-call completion. Native camera/player countdowns, loaded content,
terrain and actual SetB ownership establish current demand. Town retention and
private preloads do not. World reset clears the bitmap; live OFF preserves it.

The same main-thread LevelEditor::fixInvalidInteriorFurnitureStates() used by
the native key runs first. Its return0 means no changes and does not skip full
generation. The existing NavMesh::generate(ZoneMap*) detour then submits exterior
and eligible interior jobs, retaining native messages, exterior force/cache
invalidation and worker scheduling. Interior jobs use the usual KEO cache rules,
as through the existing manual-key detour. Queue/PJ/physics checks are advisory
idle snapshots; no mod lock spans either native operation. Synchronous building
repair is intentionally retained for KenshiQOL-style action parity and cannot be
split into a per-cell budget without changing that operation.

Native bindings and the installed full-generate hook must pass admission. An
unsupported build or loaded KenshiQOL.dll refuses the optional automation with
an error. Detailed logs distinguish accepted requests from proven path repair,
and show repairUs, generateUs and furnitureChanged. Skipped scheduling stays
eligible; accepted work is not automatically repeated after downstream failure.

Таблица заявок хранит4096 байт по номерам ячеек, без указателей. Проверки за
проход ограничены64 ячейками каждые250мс; между принятыми заявками не менее5с
после завершения штатных вызовов. Сохраняется синхронный штатный ремонт всех
активных зон: он нужен для поведения KenshiQOL и не подменяется внешней
пересборкой. Генерация проходит через действующий обработчик KEO; внутренние
задачи используют обычные правила кэша, как при ручной команде. Блокировки мода
не удерживаются во время ремонта и полной генерации. Подробный журнал показывает
время ремонта, постановки задач и число исправленных объектов; принятие заявки
само по себе ещё не доказывает исправление пути. Игра, сохранения и остальные
плагины остаются источниками ограничений; универсальный прирост FPS не заявляется.

## Release 5 / Выпуск 5

### English

- Fixed custom-job initialization when KenshiLua scripts, including Animal Husbandry, already use the shared context menu. This could disable Waypoint route, Gather output and Stock shop counter.
- Existing job rows and personal settings are preserved. No save import or relocation between Workshop and local mods is required.

### Русский

- Исправлена инициализация работ KEO, когда скрипты KenshiLua, в том числе Animal Husbandry, уже используют общее контекстное меню. Из-за этого могли отключаться маршрут по точкам, сбор продукции и пополнение прилавка.
- Назначенные работы и личные настройки сохраняются. Импорт сохранения и перенос модов между Workshop и локальной папкой mods не требуются.

## Release 6 / Выпуск 6

### English

- Fixed a second compatibility issue that could prevent KEO jobs from registering when another plugin had already connected to the game’s job creation system. This could leave Shift + right-click unable to add Waypoint route.
- If registration still fails, the standard log now identifies which stage failed.

### Русский

- Исправлена ещё одна причина, по которой работы KEO могли не регистрироваться, если другой плагин уже подключился к системе создания заданий игры. Из-за этого Shift + ПКМ мог не добавлять работу «Маршрут по точкам».
- Если регистрация всё же не удаётся, в обычном журнале теперь указан этап сбоя.

## Release 7 / Выпуск 7

### English

- Enabled verified save-loading, NPC model and equipment, and resource reuse optimizations on the supported GOG 1.0.65 build.
- Restored Stock shop counter with compatible StackSort releases on Steam and GOG. It works in either plugin load order.

### Русский

- На поддерживаемой сборке GOG 1.0.65 включены оптимизации загрузки сохранений, подготовки моделей и экипировки NPC, а также повторного использования ресурсов.
- Восстановлена работа «Пополнять прилавок» с совместимыми выпусками StackSort на Steam и GOG. Порядок загрузки плагинов не важен.

## Release 8 / Выпуск 8

English

- Faster initialization before the main menu with compatible KenshiLib.
- Short movement orders stay closer to the selected point. Squads keep the current destination and gathering point when an order is replaced or the leader has no usable route.
- Long trips request the next required area sooner and resume only when a usable passage is found.
- Supported models prepare triangle connectivity faster. Buffered save loading reads related fields together on Steam and GOG.
- Shop counter stocking remains compatible with Rotate Items 4.

Русский

- Ускорена инициализация до главного меню с совместимой версией KenshiLib.
- Короткие приказы ведут ближе к выбранной точке. Отряд сохраняет актуальную цель и точку сбора при повторном приказе или отсутствии пути у ведущего.
- На длинном маршруте следующий нужный участок запрашивается заранее. Отряд продолжает движение, когда найдена пригодная точка прохода.
- Быстрее подготавливаются связи между треугольниками поддерживаемых моделей. На Steam и GOG связанные поля сейва читаются вместе, когда они уже в буфере.
- Пополнение прилавка совместимо с Rotate Items 4.
