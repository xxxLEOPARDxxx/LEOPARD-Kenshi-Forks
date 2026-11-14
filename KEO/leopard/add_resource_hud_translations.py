from pathlib import Path
import json,re,sys
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tools/localization'))
from compile_catalogs import read_po
items=[
 ('Show resource warmup panel','Показывать панель прогрева','显示资源预热面板'),
 ('Shows RAM and VRAM cache use, background reading speed and remaining resource requests in the upper left. The queue can grow as you move; this is not progress for the whole world.','Показывает слева вверху объём RAM- и VRAM-кэша, скорость фонового чтения и оставшиеся запросы ресурсов. Очередь пополняется при движении; это не прогресс загрузки всего мира.','在左上角显示内存和显存缓存用量、后台读取速度与剩余资源请求。移动时队列可能增加；这不是整个世界的加载进度。'),
 ('remaining','осталось','剩余'),('Off','Выкл.','关闭'),('ready','готово','已就绪'),
 ('Paused','Пауза','已暂停'),('Warming','Прогрев','预热中'),('Ready','Готово','已就绪'),
 ('created','подготовлено','已准备'),('used','использовано','已使用'),('uploads skipped','загрузок пропущено','已跳过上传'),('files','файлов','文件')]
for lang,col in [('en_GB',0),('ru_RU',1),('zh_CN',2)]:
    path=root/f'locale/{lang}/LC_MESSAGES/keo.po'; entries=read_po(path);text=path.read_text(encoding='utf-8-sig');added=0
    for row in items:
        if row[0] in entries:continue
        if lang=='ru_RU':assert not re.search('[\u2010-\u2015\u2212]',row[col])
        text+='\n\nmsgid '+json.dumps(row[0],ensure_ascii=False)+'\nmsgstr '+json.dumps(row[col],ensure_ascii=False)+'\n';added+=1
    path.write_text(text,encoding='utf-8');print(lang,added,'HUD translations added')
