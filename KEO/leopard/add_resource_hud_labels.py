from pathlib import Path
import json,sys
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools/localization'))
from compile_catalogs import read_po
rows=[('RAM warm','RAM-кэш','内存预热'),('VRAM standby','VRAM-запас','显存预备')]
for lang,col in [('en_GB',0),('ru_RU',1),('zh_CN',2)]:
    p=root/f'locale/{lang}/LC_MESSAGES/keo.po';text=p.read_text(encoding='utf-8');old=read_po(p)
    for row in rows:
        assert row[0] not in old
        text+='\n\nmsgid '+json.dumps(row[0],ensure_ascii=False)+'\nmsgstr '+json.dumps(row[col],ensure_ascii=False)+'\n'
    p.write_text(text,encoding='utf-8')
