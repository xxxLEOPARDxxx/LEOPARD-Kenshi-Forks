## Loot-Scoot-Execute (RE_Kenshi plugin)
Adds an Execute action for downed enemies in Kenshi's right-click context flow.

Current status: implemented for Kenshi `1.0.65` using:
- a custom Execute row aligned to the context menu flow,
- queued execute dispatch with range and facing checks,
- a single vanilla downward strike on compatible character rigs, timed to the motion,
- an `Execute All` panel action that chains nearby unconscious enemies around the selected target,
- In-game settings through the MCM tab.
- no save-load pause behavior (that remains exclusive to Auto-Pause on Load).

## Persistent execution jobs
An ordinary click keeps the existing one-shot behavior. **Shift + Execute** adds
`Execute own defeats` to that character's native Jobs list. It executes only
targets that this character actually knocked unconscious with a melee hit,
projectile or successful stealth knockout. Waking clears that permission; hitting
someone who is already unconscious does not claim them. A target knocked out
before this plugin observed the event has no verified owner and is skipped.

**Shift + Execute All** adds `Execute enemies` to the selected characters' Jobs
lists, within the existing maximum-actors setting. It finds eligible unconscious
enemies around each executor using the configured Execute All radius. This
automatic job always requires hostility. Both jobs follow native job priority,
wait until their executor has finished fighting, respect Jobs off and direct
player orders, and recheck eligibility before the strike. Remove their Jobs rows
to stop them. Existing work rows are preserved. Jobs and confirmed knockout
ownership survive native save/load; a recovered target loses its old ownership.

The search radius is **1-200 metres**, default 10, matching Better Looting's
distance scale. One-time Execute All searches around the selected target;
permanent work searches around its executor. This does not increase the strike
distance: the executor still approaches the target before the finisher.

Executing a non-hostile target when the alliance-check setting allows it now
registers murder on the executor. Native witnesses, guards, faction loyalty,
bounties and betrayal determine the response; unseen NPCs are not forced hostile.

**Shift-задания:** Shift + «Добить» добавляет в список работ «Добивать своих
поверженных» - только тех, кого этот исполнитель действительно нокаутировал
в бою, выстрелом или успешным скрытым ударом. Пробуждение снимает эту метку;
удар по уже лежащему её не передаёт. Старый нокаут, который мод не наблюдал,
своим не считается. Shift + «Добить всех» добавляет выделенным персонажам работу
«Добивать врагов» с текущими радиусом и ограничением числа исполнителей.
Автоматическая работа требует враждебности цели. Обе работы выполняются после
боя, учитывают штатную очередность работ и переключатель «Работы», уступают
прямым приказам и сохраняются вместе с подтверждённой меткой нокаута. Чтобы
отключить работу, удалите её строку. Остальные работы сохраняются.

Радиус поиска задаётся в метрах: **1-200 м**, по умолчанию 10 м, в том же
масштабе расстояний, что Better Looting. Разовый приказ ищет вокруг выбранной
цели, постоянная работа - вокруг исполнителя. Радиус не увеличивает дистанцию
удара: перед добиванием персонаж по-прежнему подходит к цели.

Намеренное добивание нейтрала или союзника при разрешающей настройке теперь
заявляет штатное преступление «Убийство» на исполнителя. Реакцию увидевших это
стражников и сторонников жертвы определяет игра, включая предательство и награду
за голову. Агрессия всем персонажам принудительно не назначается.

### Runtime and validation notes
Native task IDs **294/295** are stable save identifiers. Registration refuses an
occupied ID; types 291-293 and other mods' rows are left alone. The two task
metadata records are registered before loading characters. The native default
factory creates `Task_Blank`; its selected `runAction` hands off to the existing
bounded temporary-order/finisher queue on the main thread. The native chooser
owns priority; a cached candidate alone never starts an execution.

At most 50 opted-in loaded actors are scheduled. One retained local-query buffer
is reused, with at most 128 candidates examined and one query per 250 ms globally.
The game's query density parameters are not hard result limits: native spatial
search can return more characters, and a larger physical radius can cost more
CPU time. The buffer retains its largest local-query allocation for reuse.
One-time Execute All and its hover preview also reuse one protected buffer,
instead of losing an allocation after every query. Nested/concurrent collection
falls back to its validated primary target without touching that shared buffer.
At 50 actors a full scan round can take **12.5 seconds** (16 actors: 4 seconds),
plus native job scheduling. Offers expire after 20 seconds and are revalidated at
handoff and contact. There is no per-frame world traversal. The fixed knockout
ledger has 1024 entries and at most 16 probes per lookup. Overflow discards
permission rather than attributing another character's target. With an empty
ledger, the global medical observer takes only its disabled/empty branch.
World clearing resets transient identities, offers, requests and attribution;
native character records restore only still-unconscious confirmed targets.

`python tests/test_jobs.py` compiles extracted production scheduler, attribution
and RTTI lookup code with fixtures. The native PE check maps the Steam executable
as data, applies relocations and checks exact actions without executing game
code. The GOG stealth-action binary is unavailable for an equivalent offline
check; runtime RTTI/body validation fails closed with one startup error if its
contract differs. This development build still needs actual game checks for
native job ordering, stealth KO, save/load, visible/unseen crime witnesses,
resumed combat, CPU/FPS and memory over a long session. It has not been installed
or published as part of this change.

## Setup
Clone normally. Shared build scripts are tracked in `tools/build-scripts` via `git subtree`, while the Mod Hub SDK still uses the `tools/mod-hub-sdk` submodule.
`tools/build-scripts` is the shared source of truth. Top-level `scripts/` entrypoints are local compatibility wrappers that delegate into `tools/build-scripts`, plus a few repo-specific helper scripts such as Mod Hub SDK sync and runtime smoke checks.

1) Open a PowerShell terminal in this repo.
2) (Optional) Create `.env` from `.env.example` to set local paths.
3) Source the env script:
   - `. .\scripts\setup_env.ps1`

This sets:
- `KENSHILIB_DEPS_DIR`
- `KENSHILIB_DIR`
- `BOOST_INCLUDE_PATH`

## Mod Hub SDK Sync
This repo tracks the Mod Hub SDK through the `tools/mod-hub-sdk` submodule.

Sync and validate the SDK with:

```bash
./scripts/sync-mod-hub-sdk.sh
```

Use `--skip-pull` for validation-only mode.

If you intentionally want to test against a local `Emkejs-Mod-Core` checkout instead of GitHub, override the submodule URL locally:

```bash
git config submodule.tools/mod-hub-sdk.url /mnt/i/Kenshi_modding/Emkejs-Mod-Core
git -C tools/mod-hub-sdk remote set-url origin /mnt/i/Kenshi_modding/Emkejs-Mod-Core
./scripts/sync-mod-hub-sdk.sh
```

Notes:
- Default submodule remote is `git@github.com:Emkej/Emkejs-Mod-Core.git`.
- `scripts/sync-mod-hub-sdk.ps1` allows local `file` transport during local-override workflows.
- Running `git submodule sync -- tools/mod-hub-sdk` will restore URL from `.gitmodules`, so reapply the local override after sync if needed.

## Build
You can build in Visual Studio, or via the script below.

### Scripted build + deploy
Run:
- `.\scripts\build-deploy.ps1`

Optional parameters:
- `-KenshiPath "H:\SteamLibrary\steamapps\common\Kenshi"`
- `-Configuration "Release"`
- `-Platform "x64"`

### Runtime smoke check
After launching Kenshi once with the deployed mod, validate the latest runtime log with:
- `pwsh -NoProfile -File scripts/phase22_mod_hub_consumer_runtime_smoke_test.ps1 -ExpectedMode attached`

For fallback validation without `Emkejs-Mod-Core.dll`, use:
- `pwsh -NoProfile -File scripts/phase22_mod_hub_consumer_runtime_smoke_test.ps1 -ExpectedMode fallback`

## Deploy layout
Mod data folder name: `Loot-Scoot-Execute`

After deploy, expected files:
- `[Kenshi install dir]\mods\Loot-Scoot-Execute\Loot-Scoot-Execute.mod`
- `[Kenshi install dir]\mods\Loot-Scoot-Execute\RE_Kenshi.json`
- `[Kenshi install dir]\mods\Loot-Scoot-Execute\Loot-Scoot-Execute.dll`
- `[Kenshi install dir]\mods\Loot-Scoot-Execute\mod-config.json`

## Config
At runtime, the plugin reads:
- `[Kenshi install dir]\mods\Loot-Scoot-Execute\mod-config.json`

These settings are available in **Options > MCM > Loot-Scoot-Execute**.

Supported keys:
- `enabled` (bool)
- `enable_execute_kill_sound` (bool, default true; plays a short audio event when execute kill succeeds)
- `debug_execute_logging` (bool, default `false`; enables focused execute investigation logs in `RE_Kenshi_log.txt` for target eligibility and dispatch failures)
- `enable_execute_all` (bool, default `true`; shows the `Execute All` button and allows batch execute behavior)
- `ignore_execute_alliance_check` (bool, default `false`; allows execute on incapacitated non-hostiles and allies, but still blocks player characters)
- `drop_carried_corpse_before_execute` (bool, default `true`). Put down a dead body before approaching an execution target. The setting takes effect immediately in MCM.
- `execute_distance_units` (int, default `2`; contact distance before striking, limited to `1..20` in game; legacy `execute_distance_meters` is still accepted)
- `execute_all_radius_units` (legacy key retained; integer physical metres, default `10`, clamped to `1..200`; one-time search is centered on the selected target, permanent work on its executor). Before the 13 November correction this value was incorrectly used as native world units, giving a radius ten times smaller. Existing numeric preferences are retained and now mean metres; a value of 200 now searches 200 m rather than 20 m. Strike/contact settings are independent and unchanged.
- `execute_all_max_actors` (int, default `16`; up to `50` selected characters per `Execute All` order)
- `execute_button_width` (int, default `310`; pixel width for the Execute button panel)
- `execute_button_height` (int, default `56`; pixel height for the Execute button panel)
- `execute_button_gap` (int, default `-8`; vertical gap in pixels between `Execute` and `Execute All`; use negative values to overlap the rows, clamped to `-64..64`)
- `execute_button_x` (int, default `0`; horizontal offset in pixels from the default anchored position)
- `execute_button_y` (int, default `0`; vertical offset in pixels from the default anchored position)

If config is missing or unreadable, defaults are used and written back.

For faction-specific execute failures, set `debug_execute_logging` to `true`, reproduce the issue once, then attach the updated `RE_Kenshi_log.txt`. Look for lines starting with `[investigate][execute]`.

## Изменения форка (ветка leopard)
- Настройки - во вкладке MCM окна «Настройки»; Mod Hub не нужен.
- Добивание сопровождается одним штатным рубящим взмахом, если у персонажа есть эта анимация.
- v2: подписи кнопок Execute / Execute All идут через перевод (Tr) - en/ru/zh в locale.

## License
This project is licensed under the GNU General Public License v3.0.
It uses KenshiLib, which is released under GPLv3.
- v3: задания добивания - массив на исполнителя (до 16, ExecuteJob; старые имена g_queuedExecute*/g_executeAllBatch* - макросы на текущее задание). «Добить всех» берёт всех выделенных своих; цели - общий пул, каждый берёт ближайшую свободную к себе (раньше - по порядку сбора, один исполнитель).
- v3: приказ игрока выделенным (newPlayerTaskSelectedCharacters, кроме прокси-приказа добивания, и updateLastMoveWaypointSelectedCharacters - ПКМ по земле) снимает их добивание; задание моложе 500 мс не трогается.
- v3: подход к цели - раз в 1 с вместо 200 мс, clearAllAIGoals только первым приказом (раньше задача персонажа мелькала).
- v3: убрана строка «Кнопка по умолчанию» (MCM сбрасывает всю страницу).
- v4: звук добивания - только события из банков игры (проверено по хешам в data/audio/*.bnk): у оригинала первыми шли несуществующие "Heavy_Hit"/"Light_Hit", и звука не было. Теперь на жертве Impact + голос: человек - VO_Get_Hit, животное - VO_Creature_Die, скелет - Deflection (металл).

## Изменения форка (15.10.2026)
- «Добить» / «Добить всех» - пункты самого контекстного меню игры, по рецепту HarvestOrgans: сразу после `showContextMenu` слот PanelEmpty в optionsList и в нём Kenshi_Button1 с размерами пунктов игры (ContextMenuGUI +0x100 / +0x110), растёт только окно меню. Плашка под меню - только запасной вариант.
- `mod-config.json` при первом запуске создаётся копией `mod-config.defaults.json` (`IniDefaults::PrepareJson`) - больше нет «could not read mod-config.json» в журнале на чистой установке.


Localization diagnostics / Журнал локализации (2026-11-01)
Locale and catalogue-size summaries are written only while this plugin's Detailed log is enabled. Enabling it after translations have loaded reports the existing catalogue once. Loading/format errors remain visible with detailed logging off.
Сводка языка и числа переводов записывается только при включённом подробном журнале этого плагина. Включение журнала после загрузки переводов выводит сводку существующего каталога один раз. Ошибки загрузки и формата видны и при выключенном подробном журнале.

## Local changes / Локальные изменения (2026-11-11)

- A character puts down a dead body before carrying out an Execute order. This is enabled by default and can be turned off in MCM. A living captive stays on the character's shoulder.
- The Execute actions appear immediately below the last visible context menu option, even when the game has reserved hidden rows.
- A batch ends cleanly if its executor is no longer available.

- Перед добиванием персонаж опускает мёртвое тело с плеч. Опция включена по умолчанию и отключается в MCM. Живой пленник остаётся на плечах.
- Пункты добивания появляются сразу под последней видимой строкой контекстного меню, даже если игра оставила запас скрытых строк.
- Серия добиваний прекращается, если исполнитель больше недоступен.

## Prepared release 6 / Подготовленный выпуск 6 (2026-11-02)

English

- Optional language diagnostics. Language and translation-catalogue summaries follow this plugin's Detailed log setting, which is off by default.
- Useful errors remain visible. Translation loading and format errors are still reported with Detailed log disabled. Enabling the setting after loading reports the saved summary once without reloading translations.
- Clean update package. English, Russian and Simplified Chinese each use one compiled runtime catalogue. Personal settings and gameplay saves are preserved.

Русский

- Необязательная диагностика языка. Сводки языка и каталога переводов подчиняются настройке подробного журнала этого плагина, выключенной по умолчанию.
- Ошибки остаются видимыми. Ошибки загрузки и формата переводов записываются и при выключенном подробном журнале. Включение настройки после загрузки выводит сохранённую сводку один раз без перезагрузки переводов.
- Чистый пакет обновления. Английский, русский и упрощённый китайский используют по одному скомпилированному рабочему каталогу. Личные настройки и игровые сохранения сохраняются.

## Release 7 / Выпуск 7

### English

- Finishers start faster and approach the target before striking. Characters finish the assigned batch before returning to standing jobs.
- Supported characters use a native strike animation and keep their equipped melee weapon in hand through the swing. A melee weapon in the second slot can be used if a crossbow is in the first.
- Set the maximum finishing distance up to 20 and the number of selected executioners up to 50. Optionally put down a carried corpse before approaching. Carried targets are excluded by default.
- Execution commands align with the game's visible context-menu rows.

### Русский

- Команда добивания выполняется быстрее. Персонаж подходит вплотную, добивает все назначенные цели и только потом возвращается к постоянным задачам.
- Для подходящих персонажей используется штатная анимация удара. Оружие ближнего боя видно весь взмах. Если в первом слоте арбалет, можно достать оружие из второго.
- Дистанция добивания настраивается до 20, число исполнителей до 50. Можно включить сброс переносимого тела перед подходом. Добивание целей, которых несут, по умолчанию выключено.
- Команды добивания выровнены по строкам штатного контекстного меню.
