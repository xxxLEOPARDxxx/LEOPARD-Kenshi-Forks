# Kenshi Job Management

Kenshi Job Management adds an ingame UI for visualizing and editing job queues.

**Version: 0.5.0-beta** (original). This repository holds the LEOPARD fork -
see "Изменения форка" at the end.

## Disclaimer

This is a beta release. It has been tested for hours on my machine with ~240 other mods, but I can't promise it's safe for every save/mod list. Back up your save just incase. There isn't currently an undo button.

I use Radiant Dark True UI, so the colors may look different with other UI mods. Default settings should be vanilla friendly, there's a dark UI toggle in the options menu.

## Requirements

- Kenshi Steam 1.0.65+
- RE_Kenshi 0.3.4 or newer;

## Languages

The manager follows Kenshi's selected language. It includes German, Japanese,
Russian, and Simplified Chinese (`zh_CN`). English remains the built-in
fallback if a translation file is missing or invalid. Restart Kenshi after you
change the game language.

## Features

### Squad Jobs

- Open the manager with **Ctrl+J** or the Job Manager button on the HUD.
- View every active player squad in Kenshi `TAB`.
- TAB cycles between active squdas, expanding squad displays automatically and returning them to closed if applicable.
- Supports copy and paste, dragging and dropping. Multiple jobs dragged onto one character will move any non duplicate jobs, leaving duplicates in place.
- Selecting jobs also selects their owners as recipients. Select recipients directly by clicking portraits or the bottom squad selector. (Hold ctrl to select more than one character or squad.)
- Add missing Rescue, Put in Bed, Medic, Robotics, and Splinting jobs with the compact Add Medic/Robotics button. `Prioritize Healing` reorders existing healing jobs. (Is this the ideal order??)
- Use `Remove Invalid Jobs` to clean up unresolved fixed-target jobs. (Should only remove jobs that can't otherwise be performed. Such as mining natural resource deposits after an import).

### Stations

- Browse all player-owned workstations and assigned natural resource nodes in a grouped grid. Cards use Kenshi's native building icons when available.
- See exact station names, including renamed buildings, area, assignment count, and issues that may prevent its use. (ex: Not enough power)
- Click on a station to open a new UI to assign/remove workers.
- Workers sort by relevant skill. (Highest to lowest, training benches are lowest to highest.)

The game pauses while the manager is open and restores the prior pause/speed
state when it closes.

## Installation

1. Subscribe on steam https://steamcommunity.com/sharedfiles/filedetails/?id=3785306982

Alternatively:

1. Download the source and compile yourself!


## Credits and license

Codex generated the code and GUI icons. I wrote the prompts, tested the mod in
game, took the screenshots, and made the preview art. Codex created the initial version of this readme, I edited and removed about 80% of it because it was dumb LLM fluff.

Kenshi and its assets belong to Lo-Fi Games. RE_Kenshi and KenshiLib are
separate projects with their own licenses and release notes.

This project is licensed under **GPL-3.0-only**.

## Изменения форка (LEOPARD, 12-13.10.2026)

Оригинал - [SirixKC/KenshiJobManagement](https://github.com/SirixKC/KenshiJobManagement), 0.5.0-beta, GPL-3.0-only. Выкладка форка - «Kenshi Job Management (MCM fork) (QoL)», версия 1.

- Сборка на нашей KenshiLib (`build.ps1 -FromVcxproj`), одна единица компиляции.
- Настройки - во вкладке MCM окна «Настройки» (мод ModConfigMenu): вкл/выкл (после перезапуска), клавиша окна (Ctrl+J, можно NONE), кнопка JM на панели, размер окна 70-200 %. Файл `KenshiJobManagement.ini` рядом с DLL; в раздаче - `KenshiJobManagement.defaults.ini`, личный ini создаётся из него и при обновлении не затирается. Фильтры станций и свёрнутые группы - как в оригинале, `%LOCALAPPDATA%\KenshiJobManagement\settings.ini`.
- Перевод - .po (`mod/locale/<язык>/LC_MESSAGES/kenshi_job_management.po`) через общий `Localization.h`: английский, русский, китайский; немецкий и японский убраны. В бинарнике только английский.
- Интерфейс - родные скины игры (рамки `Kenshi_GenericTextBoxFlatSkin`, окна, кнопки, галочки `Kenshi_TickBoxSkin`, крупные полосы прокрутки `Kenshi_SliderVBig/HBig`) и родные шрифты в их размерах (без растягивания). Цвета - из темы интерфейса игры (ванила или мод на интерфейс); переключатель Dark UI убран.
- Один масштаб на всё окно: высота экрана / 1080 × размер из MCM. Содержимое карточек и строк - внутри светлой линии рамки.
- Заголовок окна без номера версии.
- Ревью на утечки и нагрузку: при закрытом окне - только клавиша и проверка кнопки на панели раз в 400 мс; все виджеты, модальные окна и кэши удаляются при закрытии; иконки станций грузятся в Ogre один раз.
- Полосы прокрутки - крупные игровые (`Kenshi_SliderVBig/HBig`); их ширина считается в масштабе самой игры (`NativeGui`: высота экрана / 1080) - игра увеличивает стрелки и бегунок этих шаблонов, и при ширине 32 px на 1440 строках стрелки срезались.
- Окно станции: одна рамка игры у окна и у каждой строки работника (без кнопки вокруг рамки), размер окна растёт с масштабом, длинные строки могут уйти до самого мелкого шрифта. Портреты выровнены сверху: у дочернего виджета с VCenter MyGUI при каждом изменении размера строки сбрасывает положение в центр.
- Строка персонажа: имя светло-жёлтое (на светлом интерфейсе - цвет заголовков темы), три навыка кончаются вровень с низом портрета (высоты строк - по шрифтам).
- Колесо мыши во вкладке станций и в окне станции: смещение умножалось на `relative` (120 за щелчок), список улетал в конец - теперь через `NormalizeMouseWheelNotches`, шаг - строка. Ошибка оригинала.
- v2: новые значки `mod/gui/*.png` (пользователь, 13.10.2026) - 256 x 256 RGBA вместо 96 x 96; рисуются целиком через setImageTexture, код не менялся.
- v3: v1-v2 были собраны без флагов `KJM_GENERAL_JOB_TRANSFER_VERIFIED` и `KJM_JOB_BATCH_ACTIONS_VERIFIED` из vcxproj (наш build.ps1 -FromVcxproj не брал PreprocessorDefinitions) - перенос задач и групповые действия, включая добавление лечения, писали "отключено в текущей сборке". build.ps1 теперь берёт определения Release-конфигурации.

- v4 (21.10.2026): пересобран с общим GameTheme от 15.10 - палитра мода на интерфейс ищется и в Мастерской Steam (раньше только Kenshi\mods, и с тёмным интерфейсом из Мастерской окно было ванильным: светлые карточки, тёмный текст).


Localization diagnostics / Журнал локализации (2026-11-01)
Locale and catalogue-size summaries are written only while this plugin's Detailed log is enabled. Enabling it after translations have loaded reports the existing catalogue once. Loading/format errors remain visible with detailed logging off.
Сводка языка и числа переводов записывается только при включённом подробном журнале этого плагина. Включение журнала после загрузки переводов выводит сводку существующего каталога один раз. Ошибки загрузки и формата видны и при выключенном подробном журнале.

## Prepared release 5 / Подготовленный выпуск 5 (2026-11-02)

English

- Optional language diagnostics. Language and translation-catalogue summaries follow this plugin's Detailed log setting, which is off by default.
- Useful errors remain visible. Translation loading and format errors are still reported with Detailed log disabled. Enabling the setting after loading reports the saved summary once without reloading translations.
- Clean update package. English, Russian and Simplified Chinese each use one compiled runtime catalogue. Personal settings and gameplay saves are preserved.

Русский

- Необязательная диагностика языка. Сводки языка и каталога переводов подчиняются настройке подробного журнала этого плагина, выключенной по умолчанию.
- Ошибки остаются видимыми. Ошибки загрузки и формата переводов записываются и при выключенном подробном журнале. Включение настройки после загрузки выводит сохранённую сводку один раз без перезагрузки переводов.
- Чистый пакет обновления. Английский, русский и упрощённый китайский используют по одному скомпилированному рабочему каталогу. Личные настройки и игровые сохранения сохраняются.
