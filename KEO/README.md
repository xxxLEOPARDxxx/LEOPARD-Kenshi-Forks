# KEO (MCM fork) (QoL) - release 2

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

GOG works on the supported build, but has not been comprehensively tested. Startup of an earlier GOG candidate was confirmed by a player. The complete current release feature set has not been retested on GOG, so broader GOG gameplay feedback is still needed. This is not a claim that every GOG revision works.

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

GOG работает на поддерживаемой сборке, но ещё не прошёл всестороннюю проверку. Запуск более раннего кандидата GOG подтверждён игроком. Полный нынешний набор возможностей повторно на GOG ещё не проверен; нужны отзывы по игровому поведению. Совместимость со всеми редакциями GOG этим не заявляется.

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

## Private body shape preparation / Подготовка форм тела (2026-11-16)

**Reuse temporary body shape data** (Performance, default ON, live) avoids deep
copies of shape maps which native body preparation only reads and then discards.
Every character still receives its own mesh and Entity. Shape weights, shaved
rules, appearance randomness, equipment, materials, animation, native vertex
blending and GPU work remain unchanged. Unsupported engine paths use ordinary
native copies. This targets body construction during NPC/save loading; a game
load-time or frame-time improvement has not been measured.

**Повторное использование форм тела** (Производительность, включено по умолчанию,
работает без перезапуска) убирает глубокое копирование данных форм, которые
подготовка тела только читает и затем удаляет. У каждого персонажа сохраняются
собственные модель и объект рендера. Веса форм, правила выбритой головы,
случайность внешности, снаряжение, материалы, анимация, расчёт вершин и работа
GPU остаются прежними. Неподдерживаемые пути движка используют обычные копии.
Правка относится к созданию тела при загрузке NPC и сейва; ускорение в игре
пока не измерялось.

Implementation: only the admitted private createMorphedMesh / Mesh::clone call
sequence may borrow at most512 source Pose pointers within at most8 nested
scopes. Actual source/destination control blocks are pinned before borrowing.
All-context cleanup removes positively recorded borrowed pointers before native
deletion, including unload, mixed lists and handled SEH. Both added pins are
released at the final pose-removal boundary before the old Entity is destroyed,
preserving native use-count-sensitive cleanup. No pose maps are mutated and no
worker, persistent cache or GPU-lock batching is added. Existing capture wrappers
publish functional readiness independently of Debug; partial hook failure retains
every published original but never admits the optimization. See private
leopard/morph-pose-scope-verification-20261116.md and integration review evidence.


### Local NPC loading review follow-up (working release 2, 2026-11-16)

Temporary body-shape cleanup now verifies and clears an unchanged borrowed list
in one linear pass, preserving the existing mixed/changed-list ownership fallback.
Geometry retention preserves a published native trampoline on hook-enable failure
and requires a valid original before admission. No new setting or worker is added.
Independent native and resource lifecycle reviews, actual Ogre cleanup checks,
exact install-policy fixtures and full builds are documented in
`leopard/npc-loading-review-20261116.md`. These are offline correctness/work-count
results; save loading, equipment construction and gameplay performance remain
unmeasured by this pass.

### Локальное ревью загрузки NPC (рабочий выпуск 2, 16.11.2026)

Очистка временных форм тела проверяет неизменённый список за один линейный проход.
Для смешанных и изменённых списков сохранена прежняя проверка принадлежности.
Кэш моделей сохраняет полученный указатель оригинальной функции при отказе
включения хука и допускает работу только с действительным оригиналом. Новых
настроек и потоков нет. Выполнены независимые проверки владения ресурсами,
проверки на настоящих объектах Ogre и полная сборка. Ускорение загрузки сейва,
сборки снаряжения и изменение FPS в игре в этом проходе не измерялись.


### Copied material storage (working release 2, 2026-11-16)

Performance: **Prepare copied material storage**, enabled by default and live.
Prepares a bounded texture-layer pointer array while a new material pass is
copied. Native material/texture copies, colours, shader setup and loading order
remain in control of Ogre. This removes repeated array-growth allocations for
eligible new hair/item material copies; existing item-material cache hits already
skip cloning. It does not skip NPC/equipment creation or save restoration.
See `leopard/material-reserve-verification-20261116.md` for native ownership,
exception, allocator, ABI and test evidence and its limits. No new gameplay
load-time/FPS measurement is asserted.

### Подготовка копий материалов (рабочий выпуск 2, 16.11.2026)

Настройка в разделе производительности включена по умолчанию и применяется сразу.
При создании новой копии прохода материала заранее подготавливается небольшой
массив указателей на текстурные слои. Материалы и текстурные слои копирует движок;
цвета, шейдеры и порядок загрузки сохраняют штатную обработку. Это убирает
повторное расширение массива у подходящих новых материалов волос и предметов.
Для уже найденных в кэше материалов предметов клонирование и так пропускается.
Создание NPC, снаряжения и восстановление сейва не пропускаются. Скорость
загрузки и FPS в игре этим проходом не измерялись.
