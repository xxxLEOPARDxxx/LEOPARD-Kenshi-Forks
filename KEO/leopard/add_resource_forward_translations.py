from pathlib import Path
import ast, json, re
root=Path(__file__).resolve().parents[1]
path=root/'src/zone/zone_config.cpp'
source=path.read_text(encoding='utf-8-sig')
for before,after in [('Prepare nearby terrain resources','Prepare nearby terrain'),('Prepare terrain textures in VRAM','Terrain VRAM warmup'),('VRAM preparation limit (GiB)','VRAM warmup limit (GiB)')]:
    source=source.replace('"'+before+'"','"'+after+'"')
path.write_text(source,encoding='utf-8')
rows=[]
for key in ('resourcePrefetch','resourceGpuWarm','resourceGpuGiB'):
    block=re.search(r'CFG_O(?:BOOL|INT)_LIVE\("'+key+r'".*?\),',source,re.S).group()
    strings=[ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"',block)]
    rows.append((strings[1],''.join(strings[2:])))
translations=[
 (rows[0][0],'Подготовка соседнего рельефа','预加载附近地形'),
 (rows[0][1],'Заранее читает текстуры рельефа соседних областей и по направлению движения, не активируя зоны и NPC. Требуется RAM-кэш ресурсов. Остальные данные зон загружаются обычным способом.','提前读取附近区域及行进方向上的地形纹理，不激活区域或 NPC。需要启用资源内存缓存。其他区域数据仍正常加载。'),
 (rows[1][0],'Прогрев рельефа в VRAM','地形显存预热'),
 (rows[1][1],'Подготавливает поддерживаемые текстуры рельефа фоновым графическим потоком. Игра использует совпавшие подготовленные данные. Новая работа приостанавливается при долгих кадрах и активации зон. Требуется подготовка соседнего рельефа. Неподдерживаемые форматы и устройства используют обычную загрузку.','在后台图形线程中准备受支持的地形纹理，游戏复用匹配的数据。帧耗时过高或区域激活时暂停新任务。需要启用附近地形预加载。不支持的格式和设备使用正常加载。'),
 (rows[2][0],'Лимит прогрева VRAM (ГиБ)','显存预热上限 (GiB)'),
 (rows[2][1],'Максимум дополнительной видеопамяти для подготовленного рельефа, а не предварительное выделение всего объёма. Фактический бюджет видеопамяти имеет приоритет. Уменьшите лимит, если на видеокарте мало свободной памяти.','用于预备地形的额外显存上限，不会预先分配全部容量。实际显存预算优先。如果显卡空闲显存较少，请降低上限。')]
for lang,col in [('en_GB',0),('ru_RU',1),('zh_CN',2)]:
    path=root/f'locale/{lang}/LC_MESSAGES/keo.po'
    text=path.read_text(encoding='utf-8-sig')
    for entry in translations:
        marker='msgid '+json.dumps(entry[0],ensure_ascii=False)
        assert marker not in text,entry[0]
        if lang=='ru_RU':assert not re.search('[\u2010-\u2015\u2212]',entry[col])
        text+='\n\n'+marker+'\nmsgstr '+json.dumps(entry[col],ensure_ascii=False)+'\n'
    path.write_text(text,encoding='utf-8')
print('Added six forward-resource strings to three catalogs')
