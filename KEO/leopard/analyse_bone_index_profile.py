"""Reaggregate existing decoded CPU data only; never start a collector."""
from pathlib import Path
from collections import Counter, defaultdict
import argparse, csv, json

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('run', type=Path)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
counts = defaultdict(Counter)
with (a.run / 'decoded/cpu-samples.csv').open(encoding='utf-8-sig', newline='') as f:
    for r in csv.DictReader(f):
        if int(r['tid']) == 16080:
            counts[int(float(r['timeMs']) // 5000)][int(r['stack'])] += 1
wanted = {sid for c in counts.values() for sid in c}
spans = {'buildIndexMap': (0x1a5e30, 0x1a621e),
         'setInsert': (0x1af410, 0x1af547),
         'edgeRead': (0x1ccf50, 0x1cd550)}
inclusive, leaf = defaultdict(set), defaultdict(set)
with (a.run / 'decoded/stack-frames.csv').open(encoding='utf-8-sig', newline='') as f:
    for r in csv.DictReader(f):
        sid = int(r['stack'])
        if sid not in wanted or not r['rva'] or r['module'].replace('\\', '/').rsplit('/', 1)[-1].lower() != 'ogremain_x64.dll':
            continue
        rva = int(r['rva'], 16)
        for name, (begin, end) in spans.items():
            if begin <= rva < end:
                inclusive[sid].add(name)
                if int(r['depth']) == 0:
                    leaf[sid].add(name)
result = {'source': a.run.name, 'tid': 16080,
          'warning': 'Existing sampled CPU only; inclusive overlaps, bins are not exact phases, no whole-save speedup.',
          'bins': [], 'totals': {n: {'inclusive': 0, 'leaf': 0} for n in spans}}
for bin_, c in sorted(counts.items()):
    row = {'beginMs': bin_ * 5000, 'samples': sum(c.values()), 'functions': {}}
    for name in spans:
        inc = sum(n for sid, n in c.items() if name in inclusive[sid])
        lf = sum(n for sid, n in c.items() if name in leaf[sid])
        row['functions'][name] = {'inclusive': inc, 'leaf': lf}
        result['totals'][name]['inclusive'] += inc
        result['totals'][name]['leaf'] += lf
    result['bins'].append(row)
a.output.write_text(json.dumps(result, indent=2))
print(json.dumps(result['totals']))
print(json.dumps(next(b for b in result['bins'] if b['beginMs'] == 35000)))
