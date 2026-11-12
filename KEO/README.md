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

- Set operator cargo limits by weight and occupied inventory and backpack cells in steps of 5%. A supported operator delivers when either limit is reached.
- Right-click the backpack checkmark to set an individual cell limit or restore the global one. A green checkmark indicates an individual limit.
- With MCM installed, KEO settings are available on its MCM page. The standalone tab is available without MCM.

### Русский

- Для операторов поддерживаемых станков можно настроить предел по весу и долю занятых ячеек инвентаря и рюкзака с шагом 5%. Доставка начинается по первому достигнутому пределу.
- ПКМ по галочке рюкзака открывает личный лимит ячеек и позволяет вернуться к общему. Зелёная галочка показывает, что задан личный лимит.
- При установленном MCM настройки доступны на странице KEO в MCM. Отдельная вкладка доступна без MCM.
