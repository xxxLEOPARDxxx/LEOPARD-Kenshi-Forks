# -*- coding: utf-8 -*-
# LEOPARD fork of KEO (local, not released): the KEO tab's text in Russian.
# Writes locale/en_GB and locale/ru_RU keo.po next to this folder's parent
# (KEO/locale), from the strings the tab shows in a PROD build. English is the
# key (Tr() falls back to it). Only ASCII and Cyrillic: the Russian build's
# fonts have no guillemets, em dash or ellipsis.
import io
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), "locale")

RU = [
    # --- section titles and fixed notes
    ("Zone loading", u"Загрузка зон"),
    ("Performance", u"Производительность"),
    ("Squad movement", u"Передвижение отрядов"),
    ("Gameplay fixes", u"Исправления игры"),
    ("Backpacks and jobs", u"Рюкзаки и работа"),
    ("Other settings", u"Прочие настройки"),
    ("Takes effect after restarting the game.", u"Вступит в силу после перезапуска игры."),
    ("Works only while \"%s\" is on.", u"Работает, только когда включено \"%s\"."),
    ("Fast zone hopping is on (Options, General): the game itself keeps a 3x3 around each of your"
     " characters, so this can't go below 1. Turn Fast zone hopping off to set 0.",
     u"Включён быстрый переход между зонами (Настройки, Основные): игра сама держит 3x3 вокруг каждого"
     u" вашего персонажа, поэтому меньше 1 не поставить. Чтобы выставить 0, выключите быстрый переход."),

    # --- choices
    ("Off", u"Выкл."),
    ("Observe", u"Наблюдение"),
    ("On", u"Вкл."),
    ("Auto", u"Авто"),
    ("Weightless", u"Невесомо"),
    ("Lightweight", u"Легко"),
    ("Moderate", u"Средне"),
    ("Heavy", u"Тяжело"),
    ("Overloaded", u"Перегруз"),

    # --- rows: label, tooltip
    ("Prevent NPC from claiming player buildings", u"Города NPC не забирают ваши постройки"),
    ("A building placed near an NPC town joins the player's own town or none, never the NPC town."
     " Inside an NPC town's radius, where one of your towns also covers the spot, a building that does"
     " not found a town and arrives with no town, or with a town too small to hold the spot, keeps the"
     " game's choice: in an unsaved zone that NPC town, whose power it then draws.",
     u"Постройка рядом с городом NPC отходит вашему городу или никому, но не городу NPC. Если место"
     u" внутри радиуса города NPC накрывает и ваш город, а постройка сама город не основывает и пришла"
     u" без города или с городом, которому место не по размеру, остаётся выбор игры: в ещё не"
     u" сохранённой зоне - тот город NPC, и питание она берёт от него."),

    ("Fix guard throwout loop", u"Стража не таскает тело туда-сюда"),
    ("A knocked-out intruder a town throws out is not carried back and forth until it wakes.",
     u"Нарушителя без сознания, которого выносит стража, больше не носят туда и обратно, пока он не очнётся."),

    ("Thrown-out hold in game minutes", u"Сколько не трогать вынесенного (игровые минуты)"),
    ("How long a thrown-out body stays off the town's throw-out search if it does not wake first.",
     u"Сколько вынесенное тело не попадает в поиск стражи на вынос, если не очнётся раньше."),

    ("Backpack fixes", u"Исправления рюкзаков"),
    ("Picked-up items go into the worn backpack first (each backpack's window can turn that off for its"
     " wearer), food and dialogue checks look in the backpack, and machine operators fill up before"
     " delivering.",
     u"Подобранные вещи сначала кладутся в надетый рюкзак (в окне рюкзака это можно отключить для его"
     u" владельца), еда и проверки в диалогах учитывают рюкзак, а операторы машин набирают полный груз,"
     u" прежде чем нести."),

    ("Operators fill up to", u"Операторы набирают до"),
    ("A machine operator keeps collecting until its load passes this tier, the word on its character"
     " panel, then delivers. Overloaded: weight never sends it.",
     u"Оператор машины собирает, пока груз не превысит эту ступень (слово на панели персонажа), и только"
     u" потом несёт. Перегруз: по весу не уходит никогда."),

    ("Keep squads together", u"Держать отряд вместе"),
    ("Keeps a squad given one move order walking together instead of scattering.",
     u"Отряд, получивший один приказ идти, идёт вместе, а не разбредается."),

    ("Resume move orders after combat", u"Продолжать путь после боя"),
    ("Holds a player move order that ended as combat took over, and re-issues it once the fight ends,"
     " up to 60 seconds. Observe (DEV) only logs what would be held.",
     u"Приказ идти, прерванный боем, не теряется: после боя (до 60 секунд) он отдаётся заново."
     u" Наблюдение (DEV) только пишет в журнал, что было бы удержано."),

    ("Arrive together when gathering", u"Сходиться одновременно"),
    ("When a squad set to run together gathers before it sets off, the members closer to the gathering"
     " point walk slower so everyone arrives at about the same time.",
     u"Когда отряд с режимом \"бежать вместе\" собирается перед выходом, те, кто ближе к точке сбора,"
     u" идут медленнее, чтобы все пришли примерно одновременно."),

    ("KEO pathfinding", u"Поиск пути KEO"),
    ("KEO's changes to how characters find and follow routes: the route planner for long orders,"
     " squad cohesion, re-issued move orders and the rest. Off: characters path as in the vanilla game;"
     " zone loading, navmesh and the stability fixes stay on.",
     u"Все изменения KEO в том, как персонажи ищут путь и идут по нему: планировщик дальних маршрутов,"
     u" отряд вместе, повтор сорвавшихся приказов и прочее. Выключено: путь ищется как в обычной игре;"
     u" загрузка зон, навсетка и исправления падений остаются."),

    ("Background navmesh threads", u"Фоновые потоки навсетки"),
    ("Threads that generate and load navmesh tiles in the background. Auto uses half the logical CPUs.",
     u"Потоки, которые строят и загружают участки навсетки в фоне. Авто - половина логических ядер."),

    ("Navmesh cache size (MB)", u"Размер кэша навсетки (МБ)"),
    ("Size cap of the navmesh_cache folder; past it the oldest files are deleted down to 75% of the cap.",
     u"Предел папки navmesh_cache; сверх него удаляются самые старые файлы, пока не останется 75% предела."),

    ("Player buildings properly stitch into navmesh", u"Постройки игрока сразу входят в навсетку"),
    ("Walls and buildings you place join the navmesh right away, including ones across a map-cell edge",
     u"Стены и постройки, которые вы ставите, сразу входят в навсетку, в том числе на границе клеток карты."),

    ("Route planner", u"Планировщик маршрутов"),
    ("Plans a long squad move over the whole map and walks it leg by leg, so the squad does not stop"
     " short where the game cannot see a path. Observe (DEV) only logs the plans.",
     u"Дальний переход отряда прокладывается по всей карте и проходится по участкам, чтобы отряд не"
     u" вставал там, где игра не видит пути. Наблюдение (DEV) только пишет планы в журнал."),

    ("Route tiles to preload", u"Участков маршрута загружать заранее"),
    ("Tiles along a planned route queued for preloading ahead of the squad. 0 queues none; default 3.",
     u"Сколько участков проложенного маршрута загружать впереди отряда. 0 - ни одного; по умолчанию 3."),

    ("Preload nearby zones", u"Загружать соседние зоны заранее"),
    ("Loads the zones around the camera and the player's characters before they are needed.",
     u"Зоны вокруг камеры и ваших персонажей загружаются до того, как понадобятся."),

    ("Zones kept around the camera (vanilla 1)", u"Зон держать вокруг камеры (в игре 1)"),
    ("Radius, in zones around the camera, inside which the zones the mod loaded stay loaded: 1 keeps a"
     " 3x3, 2 a 5x5. The game itself keeps a 3x3 around the camera.",
     u"Радиус в зонах вокруг камеры, внутри которого загруженные модом зоны не выгружаются: 1 - 3x3,"
     u" 2 - 5x5. Сама игра держит 3x3 вокруг камеры."),

    ("Zones kept around other squads (vanilla 0)", u"Зон держать вокруг других отрядов (в игре 0)"),
    ("Radius, in zones around each of your characters away from the camera. 0 keeps only the zone each"
     " one stands in, as the game does by itself (a 3x3 with its Fast zone hopping option): the mod then"
     " loads and keeps nothing extra around them, which saves CPU with many squads spread over the map,"
     " and switching the camera to one of them loads its surroundings at that moment. 1 keeps and"
     " preloads a 3x3 around each.",
     u"Радиус в зонах вокруг каждого вашего персонажа вдали от камеры. 0 - только зона, где он стоит,"
     u" как в самой игре (3x3 при быстром переходе между зонами): мод ничего лишнего вокруг не грузит и"
     u" не держит, это бережёт процессор, когда отрядов по карте много, а окрестности грузятся, когда"
     u" на отряд переводят камеру. 1 - держать и заранее грузить 3x3 вокруг каждого."),

    ("Max extra zones kept loaded", u"Предел лишних загруженных зон"),
    ("How many zones the mod may keep loaded beyond the ones the game keeps by itself (the game keeps"
     " none extra). Past it the mod keeps only the zones right around the camera and your characters,"
     " and stops preloading around squads away from the camera, until the count falls back.",
     u"Сколько зон мод может держать загруженными сверх тех, что держит сама игра (сама игра лишних не"
     u" держит). Сверх предела мод оставляет только зоны прямо у камеры и ваших персонажей и перестаёт"
     u" грузить заранее вокруг дальних отрядов, пока число не опустится."),

    ("Cap particle updates at high game speed", u"Реже обновлять частицы на большой скорости"),
    ("Above a game speed set by particleStepCapSpeed in KEO.ini (3x by default), particle effects"
     " advance in capped steps, so smoke and fire animate slower.",
     u"Выше скорости игры particleStepCapSpeed из KEO.ini (по умолчанию 3x) частицы обновляются"
     u" ограниченными шагами, поэтому дым и огонь двигаются медленнее."),

    ("Foliage time limit at high speed (ms)", u"Лимит времени на растительность на скорости (мс)"),
    ("Above a game speed set by foliageBudgetSpeed in KEO.ini (3x by default), foliage stops building"
     " pages for the rest of a frame once this many milliseconds are spent; trees and grass fill in a"
     " few frames later. 0 turns it off.",
     u"Выше скорости игры foliageBudgetSpeed из KEO.ini (по умолчанию 3x), потратив столько"
     u" миллисекунд, растительность до конца кадра больше не строится; деревья и трава дорисуются через"
     u" несколько кадров. 0 - выключено."),
]


def esc(s):
    return s.replace('\\', '\\\\').replace('"', '\\"')


def write(lang, pairs, translate):
    d = os.path.join(OUT, lang, "LC_MESSAGES")
    if not os.path.isdir(d):
        os.makedirs(d)
    lines = [u'msgid ""', u'msgstr ""', u'"Content-Type: text/plain; charset=UTF-8\\n"',
             u'"Language: %s\\n"' % lang, u'']
    seen = set()
    for en, ru in pairs:
        if en in seen:
            continue
        seen.add(en)
        lines.append(u'msgid "%s"' % esc(en))
        lines.append(u'msgstr "%s"' % esc(ru if translate else en))
        lines.append(u'')
    with io.open(os.path.join(d, "keo.po"), "w", encoding="utf-8", newline="\n") as f:
        f.write(u"\n".join(lines))


bad = [ru for _, ru in RU if any(c in ru for c in u"«»—–…№")]
assert not bad, bad
write("en_GB", RU, False)
write("ru_RU", RU, True)
print("ok: %d strings" % len(RU))
