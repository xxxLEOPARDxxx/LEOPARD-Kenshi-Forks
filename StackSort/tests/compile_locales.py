"""Maintain source PO catalogs and compile MO-only runtime locale folders."""
import argparse, ast, json, pathlib, re, struct
ROOT = pathlib.Path(__file__).resolve().parents[1]
ROWS = [
("Sort [{n}]", "Сортировать [{n}]", "整理 [{n}]"),
("Sort. [{n}]", "Сорт. [{n}]", "整理 [{n}]"),
("Sort items. Left-click cycles arrangements; right-click restores item positions.", "Сортировать. ЛКМ перебирает раскладки; ПКМ возвращает позиции предметов.", "整理物品。左键切换排列方案；右键恢复物品位置。"),
("H", "В", "高"), ("W", "Ш", "宽"), ("<", "<", "<"), (">", ">", ">"),
("StackSort: some hooks failed to install (see RE_Kenshi log); sorting may not work correctly", "StackSort: не удалось подключить некоторые функции (см. журнал RE_Kenshi); сортировка может работать неправильно", "StackSort：部分功能挂接失败（请查看 RE_Kenshi 日志），整理可能无法正常工作"),
("StackSort: {n} problem(s) in StackSort.cfg were ignored (see RE_Kenshi log)", "StackSort: проигнорировано ошибок в StackSort.cfg: {n} (см. журнал RE_Kenshi)", "StackSort：已忽略 StackSort.cfg 中的 {n} 个问题（请查看 RE_Kenshi 日志）"),
("Pack inventory items, merge compatible split stacks and reserve free space.", "Уплотняет предметы в инвентаре, объединяет совместимые стопки и оставляет свободное место.", "紧凑排列物品，合并兼容的分散堆叠，并预留空间。"),
("Enable Stack Sort", "Включить Stack Sort", "启用 Stack Sort"),
("Replace Arrange in inventories with room to sort. Single-slot storage keeps its original controls.", "Заменяет кнопку сортировки там, где есть место для перестановки. Однослотовые хранилища сохраняют обычное управление.", "在有整理空间的物品栏中替换整理按钮。单格储存容器保留原有控件。"),
("Merge split stacks", "Объединять разделённые стопки", "合并分散堆叠"),
("On sort clicks, merge compatible stacks after contents or remaining charges change. Uses the game's item and ownership rules. Undo restores positions, without splitting stacks.", "При нажатии сортировки повторно объединяет совместимые стопки после изменения состава или расхода зарядов. Использует игровые правила предметов и владельцев. Отмена возвращает позиции, не разделяя стопки.", "点击整理时，在物品或剩余使用次数变化后重新合并兼容堆叠。遵循游戏的物品及所有权规则。撤销仅恢复位置，不会拆分堆叠。"),
("Height", "Высота", "高度"), ("Width", "Ширина", "宽度"),
("Starting free-space direction", "Начальное направление свободного места", "初始空余空间方向"),
("Starting H/W mode when the plugin starts. The inventory H/W button changes it during play.", "Режим В/Ш при запуске плагина. Кнопка В/Ш в инвентаре меняет его во время игры.", "插件启动时使用的高/宽模式。游戏中可通过物品栏的高/宽按钮切换。"),
("Solutions to cycle", "Количество вариантов", "循环方案数"),
("Number of best arrangements shown by repeated clicks on StackSort.", "Сколько лучших раскладок показывать при повторных нажатиях StackSort.", "反复点击 StackSort 时显示的最佳排列方案数量。"),
("Background workers", "Потоки фонового расчёта", "后台线程数"),
("Threads used to prepare inventory arrangements. More workers can increase CPU use.", "Потоки для подготовки раскладок инвентаря. Большее число может повысить нагрузку на процессор.", "用于预先计算物品排列的线程数。更多线程可能增加处理器负载。"),
("Detailed log", "Подробный журнал", "详细日志"),
("Log sorting, stack merging, rotation detection and background jobs in RE_Kenshi_log.txt. Errors are always logged.", "Записывает сортировку, объединение стопок, обнаружение поворота и фоновые задания в RE_Kenshi_log.txt. Ошибки записываются всегда.", "将整理、堆叠合并、旋转检测及后台任务写入 RE_Kenshi_log.txt。错误始终记录。"),
]
def mo_bytes(catalog):
    keys = sorted(catalog)
    originals = [s.encode('utf-8') for s in keys]
    translations = [catalog[s].encode('utf-8') for s in keys]
    n = len(keys); offset = 28 + 16*n
    tables = []; data = b''
    for strings in (originals, translations):
        table = []
        for s in strings:
            table.append((len(s), offset+len(data)));data += s+b'\0'
        tables.append(b''.join(struct.pack('<II', *entry) for entry in table))
    return struct.pack('<7I', 0x950412de, 0, n, 28, 28+8*n, 0, 0)+b''.join(tables)+data
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=pathlib.Path,required=True);args=parser.parse_args()
    required = set()
    for file in ('src/common/McmSettings.cpp','src/common/Translate.h'):
        text=(ROOT/file).read_text(encoding='utf-8')
        if file.endswith('.cpp'):
            required.update(ast.literal_eval(s) for s in re.findall(r'\bTr\(("(?:[^"\\]|\\.)*")\)',text))
    assert required <= {r[0] for r in ROWS}, sorted(required-{r[0] for r in ROWS})
    for col,lang in enumerate(('en_GB','ru_RU','zh_CN')):
        header=f'Language: {lang}\nContent-Type: text/plain; charset=UTF-8\nMIME-Version: 1.0\n'
        catalog={'':header}
        po=['# StackSort MCM fork. Original StackSort by Jakematt2004. GPL-3.0.','msgid ""','msgstr '+json.dumps(header,ensure_ascii=False),'']
        for row in ROWS:
            en=row[0];translated=row[col]
            assert translated and re.findall(r'\{[^}]+\}',en)==re.findall(r'\{[^}]+\}',translated)
            catalog[en]=translated
            po += ['msgid '+json.dumps(en,ensure_ascii=False),'msgstr '+json.dumps(translated,ensure_ascii=False),'']
        source=ROOT/'locale'/lang/'LC_MESSAGES'/'stack_sort.po';source.parent.mkdir(parents=True,exist_ok=True);source.write_text('\n'.join(po),encoding='utf-8')
        output=args.output/'locale'/lang/'LC_MESSAGES'/'stack_sort.mo';output.parent.mkdir(parents=True,exist_ok=True);output.write_bytes(mo_bytes(catalog))
        import gettext
        with output.open('rb') as stream: loaded=gettext.GNUTranslations(stream)
        assert all(loaded.gettext(row[0])==row[col] for row in ROWS)
        print(f'{lang}: {len(ROWS)} translated entries, MO round-trip verified')
if __name__=='__main__':main()
