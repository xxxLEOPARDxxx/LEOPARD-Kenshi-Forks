# Temporary KEO maintenance: routing master OFF

## Scope / область работ

This is a narrow local maintenance patch for the original KEO by jacobericson / Jakematt2004. Thank you to the original author. Original sources: https://github.com/jacobericson/KEO (local upstream base e712fa6134b4d2f16bb67122638db5a1f19a69aa). GPLv3 license and original history are preserved. This work follows the user's temporary-maintenance policy: fix defects without expanding the author's feature design, and withdraw the maintenance fork if the author returns. It is not a published release or a completed MCM/GOG port.

Это локальная точечная правка KEO автора jacobericson / Jakematt2004. Спасибо автору оригинала. Исходники оригинала указаны выше, лицензия GPLv3 и история сохранены. По указанию пользователя это временное сопровождение: исправлять ошибки без расширения возможностей автора и снять свой форк с сопровождения, если автор вернётся. Публикации нет; перенос в MCM и поддержка GOG этой правкой не завершены.

## Confirmed defect / подтверждённая ошибка

`keoRouting=false` already disabled the planner, hierarchical search, island routing/reissue and squad movement overrides. It left `pathfindDiag=true`, `clusterGraphBypass=player` and `playerRepathTier=true` behind. Despite its name, `pathfindDiag` installs hooks that modify queue tiers and player A* budgets; it is not merely a log toggle. The master OFF switch therefore still allowed routing changes.

The currently installed DLL matches the old local production build exactly: SHA256 dda7c280cf1074f68dd38774f07d3f9691a0a51ea9a7a39b24cca3dd64f3f0d3. Its current personal INI contains `keoRouting=false`, while the latest log reports `conn=.../player` and `playerRepath ... set13`. This confirms the configuration defect is present in the actually used build. It does not prove that it caused every reported loop, NPC spawn outside a town or excess squad.

`keoRouting=false` выключал часть маршрутизации, но оставлял группу изменяющих пути хуков, обход связности графа и повышение приоритета повторных запросов игрока. Это подтверждено исходниками и журналом реально установленной локальной DLL. Причины отдельных петель, появления NPC снаружи города и избытка отрядов пока не установлены.

## Change / исправление

Startup `ApplyRoutingSwitch` now additionally sets `pathfindDiagEnabled=false`, `clusterGraphBypassMode=CGB_ORIGINAL`, `playerRepathTierEnabled=false`. No pathfinding detours or submit-tier hooks are requested through this group when the master is OFF. Even an already retained connectivity handler obeys the original verdict exactly once, and the explicit matched-player repath policy cannot request a tier write. Master ON leaves all individual settings unchanged. Existing load/navmesh/inventory/GUI settings and independent crash guards remain unchanged.

This is a startup-only setting, as the existing configuration row declares. The original Options/KEO tab writes the value for the next launch; it does not hot-uninstall hooks. The fix adds no runtime writer, queue, thread, retained pointer, resource or per-frame INI read. With no logger the actual clamp allocates no C++ heap memory; its optional single startup summary is unchanged in frequency. Full CPU/FPS/memory behavior still needs game measurement after approval to install.

При запуске главный выключатель теперь отключает и оставшиеся три настройки. Включённый главный переключатель не меняет выбранные частные параметры. Нужен перезапуск, как и раньше: живого удаления хуков не добавлено. Новых потоков, кэшей, ссылок на игровые объекты и покадрового чтения INI нет.

This is not a complete vanilla-mode promise: zone preload/readiness/navmesh scheduling still use their own settings. Upstream's path-extraction hook is itself tied to the disabled pathfindDiag group, so only independent crash guards are claimed to remain. The old tooltip's broad vanilla wording should be replaced during the requested MCM/three-language migration; this code patch introduces no new UI message or partial locale.

## Verification / проверка

Run bundled Python `leopard/test_routing_switch.py`. It compiles the actual eight configuration modules, hook-desire policy and repath policy, plus the extracted unchanged production connectivity hook. Old code first failed the original-connectivity assertion. Fixed code passes436 checks in PROD and436 in DEV, across every bypass/hierarchical/diagnostic combination. Tests cover ON preservation, OFF/idempotence, routing hook exclusion, unchanged loading/navmesh/inventory/GUI/guard state, startup-only row, original handler fallback, matched-player tier suppression and no-logger allocation. Five separately compiled mutants fail the intended assertions. Manifest wiring of all six affected hook rows is checked.

Fresh native DEV and PROD builds compiled all336 source files each, object cache disabled, isolated `_routingfix` output. Both x64/startPlugin/143 KenshiLib imports pass the0.5.0 export baseline. No installation, personal INI change, game input, commit or publication was performed.

The full host gate was also run and is **not green**:111/122 C++ suites pass; eight existing GUI suites fail because the local localization bridge now needs game headers in a formerly host-only suite; three existing config suites fail old counts/goldens which omit the earlier local keoRouting row. Recompiling those three with HEAD's original movement_config.cpp reproduces the same failures. The relevant GUI/Localization source files are HEAD-identical after normalizing checkout line endings. The Python object-cache shadowing-header test fails in the complete module run, but passes alone; its sources are also HEAD-identical. Candidate builds use `BUILD_CACHE=off`, so every object was actually compiled. These inherited harness/cache issues are recorded, not hidden or weakened to claim a passing full gate.

Полная старая проверка стенда не зелёная: есть унаследованные проблемы GUI-зависимостей, устаревшие ожидаемые количества настроек и нестабильная проверка кэша сборки. Они не выдаются за успех; для кандидата кэш объектов отключён. Новые точечные проверки и обе сборки проходят.

## Remaining requests / что остаётся

MCM integration, complete ru_RU/en_GB/zh_CN catalogs, an optional detailed-log setting defaulting OFF, and coherent fork/release metadata remain separate work. This checkout still exports the author's own Options tab and has the earlier two-language local catalogs. No new release number is assigned by this patch.

GOG remains intentionally blocked before hooks. The located GOG evidence is the official address table; no verified matching GOG executable/module byte profile is available in the checked research. A safe port requires the real matching GOG executable/modules and audited executable/mid-function/module/vtable/member sites. A Steam-to-GOG blanket substitution or removing the existing gate is not an acceptable compatibility fix.

Spawn/excess-squad reports remain open regression cases: preserve an unchanged save, compare the same world time and squad/population settings, distinguish extra loaded zones from newly created platoons, and record platoon/home-town/position and readiness before changing spawning logic. No save import or speculative spawn patch was made.
