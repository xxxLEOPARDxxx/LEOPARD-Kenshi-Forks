from pathlib import Path
import ast, json, re
root=Path(__file__).resolve().parents[1]
source=(root/'src/zone/zone_config.cpp').read_text(encoding='utf-8-sig')
rows=[]
for key in ('resourceRamCache','resourceRamGiB','resourceReadMBps'):
    block=re.search(r'CFG_O(?:BOOL|INT)_LIVE\("'+key+r'".*?\),',source,re.S).group()
    strings=[ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"',block)]
    rows.append((strings[1],''.join(strings[2:])))
translations=[
 ('Resource memory','Память ресурсов','资源内存'),
 (rows[0][0],'Кэш файлов ресурсов в ОЗУ','在内存中缓存资源文件'),
 (rows[0][1],'Сохраняет в ОЗУ файлы текстур и моделей, уже запрошенные игрой, для быстрого повторного чтения. Не активирует дополнительные зоны и NPC. Помогает при возвращении в посещённые области; первый вход использует обычную загрузку. Можно выключить сразу; текущие чтения безопасно завершатся.','在内存中保留游戏已请求的纹理和模型文件，供再次读取。不会激活额外区域或 NPC。主要帮助重访区域；首次进入仍使用正常加载。可以随时关闭，正在使用的数据会保留到读取完成。'),
 (rows[1][0],'Лимит RAM-кэша (ГиБ)','资源内存缓存上限 (GiB)'),
 (rows[1][1],'Максимальный объём ОЗУ для кэша файлов, а не заранее выделенная память. При остатке физической памяти менее 2 ГиБ или 20% (берётся большее значение) заполнение прекращается и неиспользуемые файлы освобождаются. Остальные данные игры также требуют памяти. После снижения лимита текущие чтения могут временно удерживать часть кэша.','资源文件缓存的内存上限，不会提前分配全部容量。当可用物理内存低于 2 GiB 或总内存的 20%（取较大值）时，停止填充并释放未使用的文件。游戏的其他数据也需要内存。降低上限后，正在读取的文件可能暂时保留。'),
 (rows[2][0],'Фоновое чтение (МиБ/с)','后台资源读取速度 (MiB/s)'),
 (rows[2][1],'Ограничивает скорость фонового потока с низким приоритетом, который подготавливает запрошенные файлы для повторного чтения. Уменьшите значение, если фоновое чтение диска мешает игре. Это не лимит видеопамяти.','限制低优先级后台读取线程的速度，为再次使用准备已请求的文件。如果后台磁盘读取影响游戏，请降低此值。这不是显存上限。')]
for lang,col in [('en_GB',0),('ru_RU',1),('zh_CN',2)]:
    path=root/f'locale/{lang}/LC_MESSAGES/keo.po'
    text=path.read_text(encoding='utf-8-sig')
    for entry in translations:
        marker='msgid '+json.dumps(entry[0],ensure_ascii=False)
        assert marker not in text,entry[0]
        if lang=='ru_RU':assert not re.search('[\u2010-\u2015\u2212]',entry[col])
        text+='\n\n'+marker+'\nmsgstr '+json.dumps(entry[col],ensure_ascii=False)+'\n'
    path.write_text(text,encoding='utf-8')
print('Added seven resource-cache strings to each of three catalogs')
