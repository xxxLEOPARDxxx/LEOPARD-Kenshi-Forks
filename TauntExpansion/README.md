# Taunt Expansion - MCM fork v1

Original mod: [Taunt Expansion](https://steamcommunity.com/workshop/filedetails/?id=3771499164), by **tata / Tata-Gari**. Thank you for the original Taunt mechanic and the public source code.

**Original source code:** [Tata-Gari/RE_Kenshi_Plugins_Source](https://github.com/Tata-Gari/RE_Kenshi_Plugins_Source/tree/b7cc05a3a1b9d6c065bd7c96272abebcd0de8be8/03_Taunt_Expansion_Source). Pinned upstream commit: `b7cc05a3a1b9d6c065bd7c96272abebcd0de8be8` (HEAD verified 2026-10-31). GPL-3.0; original notices and source are retained under `upstream/`, with the complete license in `LICENSE`.

## English

### Features and controls

Taunt Expansion strengthens Kenshi's existing Taunt button so a tank can draw more enemy attention. Turn on **Taunt** for the character using the normal game control. There is no additional hotkey or window.

- The original mod multiplies only the existing Taunt score bonus, normally **+2**. The default multiplier **5** gives **+10**.
- If an enemy already has a valid target, the original mod applies the additional default multiplier **2**, giving **+20** in total.
- Kenshi still decides which candidates are valid, reachable and worth attacking. Taunt is stronger, but is not a guaranteed forced target switch. Several taunting characters compete through the normal game rules.
- Pathfinding, attack slots, combat animations, damage, dodge, guard and the underlying target-retention rules are preserved.

### What our fork changes

- An optional **Taunt Expansion** page in **Mod Config Menu**. Every exposed setting applies during play, including enabling/disabling the mechanic; no setting requires a restart.
- English, Russian and Simplified Chinese settings and tooltips (`en_GB`, `ru_RU`, `zh_CN`).
- Detailed diagnostics are off by default and have a separate master switch. Errors are always reported.
- Concurrent and nested target evaluations preserve the appropriate Taunt strength. Repeated diagnostic data has a fixed capacity rather than accumulating character references across long sessions and save changes.
- Shipped gameplay defaults remain **5 / 2**. Upstream's package already uses these values; this fork makes missing-key defaults agree with them.

The Taunt gameplay, retarget mechanic and defaults are the original author's work. MCM, translations and the safeguards above are fork changes.

### Installation and compatibility

Requires **Kenshi 1.0.65 x64**, **RE_Kenshi** and **KenshiLib**. This build uses the installed KenshiLib 0.5.0 export set and the matching Steam 1.0.65 target addresses. MCM is optional: without it the plugin reads `TauntExpansion.ini` beside its DLL. For manual edits, close the game first. With MCM, changes apply immediately.

Install the packaged `TauntExpansion` directory into `Kenshi/mods/` and enable `TauntExpansion.mod`. The DLL is loaded through `RE_Kenshi.json`. Use this fork **in place of the original**, with one active copy. Keep the real `.mod` and loader JSON from the package. There is no save-specific plugin data and no import requirement.

Other plugins rewriting the same Taunt instruction or target-selection function may conflict. The fork checks the instruction and the original **+2** bonus before changing it; on unsupported/conflicting code it logs an error and leaves the Taunt mechanic at vanilla strength. Steam 1.0.65 is checked offline; other executable builds require their own validation.

The original author notes that ranged enemies, turret users, animals and law-enforcement AI have received less balance testing than ordinary melee enemies. High multipliers, especially **10** and above, remain a player balance choice. This fork adds no custom widgets or skin changes; its settings use the installed MCM interface.

### Full settings reference

All settings are available in MCM and use these INI sections/keys. **Detailed log is the last setting.** Every row is live; no `MCM_RESTART` flags are needed.

| Section / key | Default | Range / effect |
| --- | --- | --- |
| `General / Enabled` | `true` | Enable the multiplier; false uses the vanilla +2 bonus. |
| `Taunt / Multiplier` | `5.0` | 0-100. 1 = vanilla; 0 removes the Taunt-specific score bonus. |
| `Taunt / RetargetMultiplier` | `2.0` | 0-10. Additional factor only with an existing valid target; 1 adds no extra factor. |
| `Logging / LogCandidates` | `false` | Include candidate names and Taunt states in detailed entries. |
| `Logging / LogOnlyWhenTauntPresent` | `true` | Skip observations without a taunting candidate. |
| `Logging / LogOnlyChangedSelection` | `true` | Skip unchanged selections; identities expire after 30 seconds. |
| `Logging / MinimumLogIntervalMs` | `500` | 0-60000 ms per diagnostic identity. 0 disables this rate limit. |
| `Logging / MaximumCandidatesPerEntry` | `32` | 1-512 candidate details per entry. |
| `Logging / Debug` | `false` | Master detailed log in `RE_Kenshi_log.txt`; errors remain unconditional. |

The logging filters produce no output while `Debug=false`. Existing INI keys remain compatible; migration does not enable debug logging. Invalid/non-finite numeric values fall back to defaults, finite out-of-range values are clamped.

### Performance and lifetime review

The hot hook runs only when Kenshi evaluates attack targets. With detailed logging off, the fork adds no world scans, candidate scans, diagnostic strings, diagnostic cache lookups or heap allocations. It performs one recursive critical-section entry per target evaluation and temporarily writes one float used by the private code cave. The original game function and its return value/output parameters are preserved.

Serializing evaluations is required by the original shared code-cave bonus. The fork locks **all** calls, including non-retarget and disabled calls, and saves/restores the previous bonus. This fixes cross-thread observation of another call's bonus and restores the enclosing value during recursion. A `__finally` restores bonus and lock ownership during native structured exceptions or C++ unwinding. No game exception is swallowed.

Optional debug observations use **256** fixed entries, never dereference stored identity values, and expire after **30 seconds**. Collisions can cause extra diagnostic entries but do not affect targeting. There are no persistent game-object owners, per-save collections, widgets, queues or worker threads. Settings reload clears diagnostic entries; no save/load reset is needed for gameplay because no world state is retained.

The code cave is a single bounded allocation of **64 bytes**, retained for the plugin/process lifetime after a successful hook. `stopPlugin` restores vanilla strength and disables diagnostics; it retains its cave, trampoline and critical section because KenshiLib exposes no hook-removal API. Runtime unloading of an active hook is unsupported. If hook installation fails, the original instruction is restored before freeing the cave; a failed restoration retains the allocation instead of leaving a branch into freed memory.

A static review and short concurrency test cannot prove game CPU/FPS or long-session memory behavior. Runtime measurements and the game checks listed below remain required before claiming those outcomes.

### Build and verification

Build with **VC10/v100 x64, Windows SDK 7.1, `/GL` and `/LTCG`**, using the project's matching KenshiLib import library. Example from the local development stand:

```powershell
& D:/DEV/Kenshi/ours/build.ps1 -SrcDir D:/DEV/Kenshi/forks/LEOPARD-Kenshi-Forks/TauntExpansion -Name TauntExpansion
& ./tests/build_tests.ps1
& <bundled-python> ./tests/compile_locales.py --output D:/DEV/Kenshi/build/TauntExpansion
& <bundled-python> ./tests/audit_binary.py --output D:/DEV/Kenshi/build/TauntExpansion/binary_audit.json
```

`build_tests.ps1` compiles and executes real production headers. It covers shipped defaults and strength modes, malformed/NaN/Inf numeric input, bounded debug storage, recursive restoration, a two-thread 10000-call stress test, SEH unwinding, unique signature selection, truncated instructions and ambiguous signatures. `compile_locales.py` verifies complete source-string coverage and compiled MO round-trips for all three languages. Source PO files stay in this repository; release locale folders contain **only MO**, never adjacent PO/MO files.

`audit_binary.py` reads the complete PE export table manually. It verifies every KenshiLib import against the installed actual DLL, required plugin exports, the unique Taunt instruction and original +2 constant in the actual RE_Kenshi runtime executable image, `Kenshi/RE_Kenshi/Kenshi_x64.exe`. Its SHA256 is `504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e`. Offline verification found target slot **154**, function RVA **0x59D870**, Taunt addition **0x59DBF3**, bonus **0x1681AD0**; the plugin resolves the function dynamically and does not hardcode these RVAs. The game-root executable has different internal code and is not the runtime proof target. The generated `release/binary_audit.json` records the exact checked image and DLL hashes.

Game checks still needed: ordinary and already-targeted enemies with defaults/1/0/disabled; repeated live MCM changes; multiple Taunt users; save/load/new game; detailed log on/off; MCM pages in EN/RU/ZH with vanilla and modified skins; runtime CPU/FPS and sustained memory measurements. Build/test success does not replace these game checks.

### Version 1 changes

- First MCM fork based on pinned upstream source; preserve the original gameplay and package defaults.
- Add optional live MCM settings and EN/RU/ZH interface catalogs.
- Add the default-off detailed-log master; bound diagnostic history and always retain error reporting.
- Fix nested/concurrent bonus restoration, neutralize disabled behavior, validate executable memory and complete unique instruction before patching, and preserve live code-cave lifetime on failure/shutdown.
- Add production-core and offline import/signature/locale checks. Runtime game verification is tracked separately.

## Русский

### Возможности и управление

Оригинальный мод: [Taunt Expansion](https://steamcommunity.com/workshop/filedetails/?id=3771499164), автор **tata / Tata-Gari**. Спасибо автору за механику усиленной провокации и открытые исходники.

**Исходники оригинала:** [Tata-Gari/RE_Kenshi_Plugins_Source](https://github.com/Tata-Gari/RE_Kenshi_Plugins_Source/tree/b7cc05a3a1b9d6c065bd7c96272abebcd0de8be8/03_Taunt_Expansion_Source). Форк основан на коммите `b7cc05a3a1b9d6c065bd7c96272abebcd0de8be8`, проверенном 31.10.2026. Лицензия GPL-3.0; оригинальные исходники и уведомления сохранены в `upstream/`, полный текст лицензии находится в `LICENSE`.

Мод усиливает штатную кнопку **Провокация**, помогая танку отвлекать врагов на себя. Включите провокацию у нужного персонажа обычной кнопкой игры. Дополнительной клавиши или отдельного окна нет.

- Оригинал умножает только обычный бонус провокации **+2**. Стандартный множитель **5** даёт **+10**.
- Если у врага уже есть допустимая цель, оригинал применяет дополнительный множитель **2**, что даёт суммарный бонус **+20**.
- Допустимость и достижимость цели, расстояние и прочие факторы по-прежнему определяет Kenshi. Усиленная провокация не гарантирует принудительную смену цели. Несколько провокаторов конкурируют по обычным правилам игры.
- Поиск пути, слоты атак, анимации, урон, уклонение, блокирование и сами правила удержания цели сохранены.

### Что изменено в нашем форке

- Необязательная страница **Taunt Expansion** в **Mod Config Menu**. Все настройки применяются во время игры, включая включение и отключение механики; перезапуск не нужен.
- Русские, английские и китайские подписи и подсказки (`ru_RU`, `en_GB`, `zh_CN`).
- Подробный журнал по умолчанию выключен и управляется отдельным общим переключателем. Ошибки записываются всегда.
- Одновременные и вложенные вычисления цели сохраняют правильную силу провокации. Диагностика больше не накапливает записи персонажей без ограничения при долгой игре и смене сохранений.
- Сохранены настройки поставки оригинала **5 / 2**; резервные значения при отсутствии ключей теперь им соответствуют.

Механика провокации, дополнительное усиление смены цели и исходные значения принадлежат автору оригинала. Наши изменения - MCM, переводы и перечисленные защитные исправления.

### Установка и совместимость

Нужны **Kenshi 1.0.65 x64**, **RE_Kenshi** и **KenshiLib**. Сборка использует набор экспортов установленной KenshiLib 0.5.0 и соответствующие адреса Steam 1.0.65. MCM необязателен: без него настройки читаются из `TauntExpansion.ini` рядом с DLL. Ручные изменения INI делайте при закрытой игре; через MCM они применяются сразу.

Поместите каталог `TauntExpansion` из пакета в `Kenshi/mods/` и включите `TauntExpansion.mod`. Загрузка DLL выполняется через `RE_Kenshi.json`. Используйте форк **вместо оригинала**, оставляя одну активную копию. Сохраните настоящие `.mod` и JSON из пакета. Собственных данных в сохранениях нет, импорт не требуется.

Плагины, заменяющие ту же инструкцию провокации или функцию выбора цели, могут конфликтовать. Перед изменением кода форк проверяет инструкцию и исходный бонус **+2**; при неподдерживаемом или конфликтующем коде сообщает об ошибке и оставляет обычную силу провокации. Steam 1.0.65 проверен офлайн; другим сборкам EXE нужна отдельная проверка.

По замечанию автора оригинала, стрелки, турели, животные и правоохранительный ИИ проверены по балансу менее подробно, чем обычные бойцы ближнего боя. Высокие множители, особенно **10** и выше, остаются выбором игрока. Собственных виджетов и правок скина нет: настройки используют интерфейс установленного MCM.

### Полный справочник настроек

Все строки доступны в MCM и соответствуют следующим секциям и ключам INI. **Подробный журнал расположен последним.** Все настройки применяются сразу; `MCM_RESTART` не требуется.

| Секция / ключ | По умолчанию | Диапазон / действие |
| --- | --- | --- |
| `General / Enabled` | `true` | Включить множители; false возвращает обычный бонус +2. |
| `Taunt / Multiplier` | `5.0` | 0-100. 1 - обычная сила; 0 убирает бонус оценки от провокации. |
| `Taunt / RetargetMultiplier` | `2.0` | 0-10. Дополнительный множитель только при существующей допустимой цели; 1 не даёт дополнительного усиления. |
| `Logging / LogCandidates` | `false` | Добавлять имена кандидатов и состояние провокации. |
| `Logging / LogOnlyWhenTauntPresent` | `true` | Пропускать наблюдения без провокатора среди кандидатов. |
| `Logging / LogOnlyChangedSelection` | `true` | Пропускать повторы цели; записи устаревают через 30 секунд. |
| `Logging / MinimumLogIntervalMs` | `500` | 0-60000 мс на диагностическую запись персонажа. 0 выключает ограничение частоты. |
| `Logging / MaximumCandidatesPerEntry` | `32` | 1-512 кандидатов в одной записи. |
| `Logging / Debug` | `false` | Общий подробный журнал в `RE_Kenshi_log.txt`; ошибки от переключателя не зависят. |

Фильтры журнала ничего не записывают при `Debug=false`. Старые ключи INI поддерживаются; обновление не включает подробный журнал. Некорректные и нечисловые значения заменяются стандартными; конечные числа вне диапазона ограничиваются его границами.

### Производительность и срок жизни

Хук вызывается только при выборе цели атаки самой игрой. При выключенном подробном журнале форк не добавляет обходов мира и кандидатов, построения диагностических строк, обращений к диагностическому кэшу или выделений памяти. На один выбор цели приходится один вход в рекурсивную критическую секцию и временная запись одного float в отдельном блоке кода. Исходная игровая функция, результат и выходные параметры сохраняются.

Блокировка нужна из-за общей константы в механике оригинала. Форк блокирует **все** вызовы, включая выбор без смены цели и отключённый мод, и восстанавливает предыдущее значение бонуса. Это устраняет использование чужого множителя между потоками и сохраняет бонус внешнего вызова при рекурсии. `__finally` восстанавливает значение и освобождает блокировку при структурированных и C++ исключениях, не подавляя игровые исключения.

В диагностике используется **256** фиксированных записей. Сохранённые числовые идентификаторы не разыменовываются и устаревают через **30 секунд**. Совпадение ячеек может добавить повторную строку журнала, но не меняет выбор цели. Собственных владельцев игровых объектов, коллекций сейва, виджетов, очередей и рабочих потоков нет. Перечитывание настроек очищает диагностику; игровое состояние мира не хранится, поэтому отдельный сброс механики при save/load не нужен.

Единственный блок кода размером **64 байта** сохраняется на весь срок жизни плагина и процесса после успешного подключения. `stopPlugin` возвращает обычную силу провокации и выключает диагностику; блок, переходник и критическая секция остаются, поскольку KenshiLib не предоставляет снятие хука. Выгрузка активного DLL во время игры не поддерживается. При ошибке подключения исходная инструкция восстанавливается до освобождения блока; если восстановить её не удалось, память сохраняется, чтобы игра не перешла в освобождённую область.

Статический review и короткий тест не подтверждают FPS или отсутствие любого роста памяти в долгой игре. Для этих выводов нужны отдельные игровые замеры.

### Сборка и проверки

Нужны **VC10/v100 x64, Windows SDK 7.1, `/GL` и `/LTCG`** и соответствующая установленной DLL библиотека импорта KenshiLib. Команды сборки приведены выше в английской части и одинаковы для всех языков.

`tests/build_tests.ps1` собирает реальные production-заголовки и проверяет настройки по умолчанию, режимы бонуса, NaN/Inf и некорректные числа, ограниченность диагностики, вложенные вызовы, 10000 вызовов в двух потоках, восстановление при SEH, выбор уникальной сигнатуры, неполную инструкцию и неоднозначность совпадений. `compile_locales.py` проверяет все строки исходников и чтение скомпилированных MO для трёх языков. PO остаются в исходниках; рабочие языковые каталоги содержат **только MO**, без соседних PO.

`audit_binary.py` вручную читает полную таблицу экспортов PE, проверяет все импорты KenshiLib по установленной DLL, экспорты плагина, единственную инструкцию провокации и бонус +2 в настоящем исполняемом образе RE_Kenshi: `Kenshi/RE_Kenshi/Kenshi_x64.exe`. Его SHA256: `504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e`. Офлайн найдены слот **154**, функция **0x59D870**, сложение **0x59DBF3** и бонус **0x1681AD0**. Эти RVA не зашиты в DLL: функция разрешается через KenshiLib. EXE в корне игры имеет другой внутренний код и не используется как подтверждение рабочего образа. Создаваемый `release/binary_audit.json` содержит точные пути и хеши проверенных EXE и DLL.

Остаются игровые проверки: обычный выбор и смена существующей цели со стандартными значениями/1/0/выключением; многократные изменения MCM; несколько провокаторов; save/load/new game; журнал; страницы EN/RU/ZH на ванильном и модифицированном скине; CPU/FPS и длительный рост памяти. Успешная сборка и локальные тесты их не заменяют.

### Изменения версии 1

- Первый MCM-форк по зафиксированным исходникам; сохранены механика и значения поставки оригинала.
- Добавлены необязательные настройки MCM с применением во время игры и интерфейс EN/RU/ZH.
- Добавлен выключенный по умолчанию общий подробный журнал, ограничена история диагностики; ошибки записываются всегда.
- Исправлены вложенные и одновременные вычисления бонуса, отключение возвращает обычную силу. Добавлены проверки исполняемой памяти и полной уникальной инструкции, безопасный срок жизни блока при ошибках и завершении работы.
- Добавлены проверки production-кода, импортов, сигнатуры и переводов. Игровая проверка учитывается отдельно.
