# BetterLooting

BetterLooting is a native RE_Kenshi plugin for Kenshi.

This repository contains the corresponding source code for the distributed plugin build.

## Source layout

- Source/BetterLooting.cpp
- Source/BetterLootingRules.h
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
- Исходники - архив `BetterLooting-source.zip` в папке мода (без файлов-бэкапов `*.pre*` наших патчей) и папка `shared` с общими заголовками (Localization.h, ModConfigMenu.h).
