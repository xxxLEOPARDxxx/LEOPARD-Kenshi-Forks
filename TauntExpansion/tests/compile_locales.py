"""Compile complete RU/EN/ZH source PO catalogs into separate MO runtime output."""
import argparse, ast, gettext, json, pathlib, re, struct
ROOT=pathlib.Path(__file__).resolve().parents[1]
ROWS=[
('Strengthen the vanilla Taunt bonus and help tanks draw enemies away from allies.', 'Усиливает обычную провокацию и помогает танкам отвлекать врагов от союзников.', '增强原版嘲讽效果，帮助坦克角色吸引攻击盟友的敌人。'),
('Enabled','Включено','启用'),
('Apply the Taunt multipliers. Disabling restores vanilla targeting strength immediately.', 'Применять множители провокации. Отключение сразу возвращает обычную силу провокации.', '应用嘲讽倍率。关闭后立即恢复原版嘲讽强度。'),
('Taunt strength','Сила провокации','嘲讽强度'),
('Taunt multiplier','Множитель провокации','嘲讽倍率'),
('Multiply the vanilla +2 Taunt score bonus. 1 is vanilla; default 5 gives +10.', 'Множитель обычного бонуса провокации +2. Значение 1 соответствует оригиналу; по умолчанию 5 даёт +10.', '将原版嘲讽的 +2 评分加成乘以此倍率。1 为原版强度；默认 5 提供 +10。'),
('Retarget multiplier','Множитель смены цели','转移目标倍率'),
('Additional Taunt multiplier when the enemy already has a valid target. Default 2 gives +20 with the default Taunt multiplier.', 'Дополнительный множитель провокации, если у врага уже есть допустимая цель. По умолчанию 2 даёт +20 при стандартном множителе провокации.', '当敌人已有有效目标时额外应用的嘲讽倍率。使用默认嘲讽倍率时，默认值 2 提供 +20。'),
('Diagnostics','Диагностика','诊断'),
('Include candidates','Записывать кандидатов','记录候选目标'),
('Include candidate names and Taunt state in detailed diagnostics.', 'Добавлять имена возможных целей и состояние провокации в подробный журнал.', '在详细诊断中记录候选目标的名称及嘲讽状态。'),
('Only when Taunt is present','Только при наличии провокации','仅在存在嘲讽时'),
('Write target diagnostics only when at least one candidate has Taunt enabled.', 'Записывать диагностику выбора цели, только если хотя бы у одного кандидата включена провокация.', '仅当至少一个候选目标开启嘲讽时记录目标诊断。'),
('Only changed selections','Только изменения цели','仅记录目标变化'),
('Suppress repeated target selections. Diagnostic identities expire after 30 seconds.', 'Пропускать повторные записи одной цели. Диагностические записи устаревают через 30 секунд.', '跳过相同目标的重复记录。诊断记录在 30 秒后过期。'),
('Minimum log interval, ms','Интервал журнала, мс','最小日志间隔，毫秒'),
('Minimum time between diagnostic entries for the same actor. Applies only while detailed logging is enabled.', 'Минимальное время между записями для одного персонажа. Действует только при включённом подробном журнале.', '同一角色两条诊断记录之间的最短时间。仅在启用详细日志时生效。'),
('Candidates per entry','Кандидатов в записи','每条记录的候选目标数'),
('Maximum number of candidates included in one diagnostic entry.', 'Максимальное число возможных целей в одной диагностической записи.', '一条诊断记录中最多包含的候选目标数量。'),
('Detailed log','Подробный журнал','详细日志'),
('Log target choices and settings in RE_Kenshi_log.txt. Disabled by default. Errors are always logged.', 'Записывать выбор целей и настройки в RE_Kenshi_log.txt. По умолчанию выключено. Ошибки записываются всегда.', '将目标选择及设置写入 RE_Kenshi_log.txt。默认关闭。错误始终记录。'),
]
def mo_bytes(catalog):
    keys=sorted(catalog);n=len(keys);offset=28+16*n;tables=[];data=b''
    for strings in ([x.encode('utf-8') for x in keys],[catalog[x].encode('utf-8') for x in keys]):
        entries=[]
        for value in strings:entries.append((len(value),offset+len(data)));data+=value+b'\0'
        tables.append(b''.join(struct.pack('<II',*entry) for entry in entries))
    return struct.pack('<7I',0x950412de,0,n,28,28+8*n,0,0)+b''.join(tables)+data
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=pathlib.Path,required=True);args=ap.parse_args()
    required={ast.literal_eval(x) for x in re.findall(r'\bTr\(("(?:[^"\\]|\\.)*")\)',(ROOT/'TauntExpansion.cpp').read_text())}
    assert required=={r[0] for r in ROWS},(required-{r[0] for r in ROWS},{r[0] for r in ROWS}-required)
    for i,lang in enumerate(('en_GB','ru_RU','zh_CN')):
        header=f'Language: {lang}\nContent-Type: text/plain; charset=UTF-8\nMIME-Version: 1.0\n'
        catalog={'':header};po=['# Taunt Expansion MCM fork v1. Original by tata / Tata-Gari. GPL-3.0.', 'msgid ""','msgstr '+json.dumps(header,ensure_ascii=False),'']
        for row in ROWS:
            assert row[i];catalog[row[0]]=row[i]
            po+=['msgid '+json.dumps(row[0],ensure_ascii=False),'msgstr '+json.dumps(row[i],ensure_ascii=False),'']
        src=ROOT/'locale'/lang/'LC_MESSAGES'/'taunt_expansion.po';src.parent.mkdir(parents=True,exist_ok=True);src.write_text('\n'.join(po),encoding='utf-8')
        dst=args.output/'locale'/lang/'LC_MESSAGES'/'taunt_expansion.mo';dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(mo_bytes(catalog))
        with dst.open('rb') as stream:loaded=gettext.GNUTranslations(stream)
        assert all(loaded.gettext(row[0])==row[i] for row in ROWS)
        print(f'{lang}: {len(ROWS)} entries, all source Tr strings covered, compiled MO roundtrip PASS')
if __name__=='__main__':main()
