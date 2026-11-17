"""Offline sampled-CPU attribution by native unwind ranges/export names."""
from pathlib import Path
from collections import Counter, defaultdict
import argparse, bisect, csv, ctypes, json

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('run', type=Path)
p.add_argument('--symbols', type=Path, required=True)
p.add_argument('--display-tid', type=int, help='Thread printed to stdout; default is the most sampled thread')
a = p.parse_args()
d = a.run / 'decoded'
symbols = json.loads(a.symbols.read_text(encoding='utf-8-sig'))
tables = {}
for module, data in symbols.items():
    names = defaultdict(list)
    for rva, name in data['exports']:
        names[rva].append(name)
    tables[module] = ([r[0] for r in data['ranges']], data['ranges'], names)
undname = ctypes.WinDLL('dbghelp').UnDecorateSymbolName
undname.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint]
undname.restype = ctypes.c_uint
resolved = {}


def label(row):
    module = row['module'].replace('\\', '/').rsplit('/', 1)[-1].lower() or 'unknown'
    if not row['rva']:
        return module + '!' + row['ip']
    rva = int(row['rva'], 16)
    if module in tables:
        starts, ranges, names = tables[module]
        i = bisect.bisect_right(starts, rva) - 1
        if i >= 0 and ranges[i][0] <= rva < ranges[i][1]:
            start = ranges[i][0]
            key = (module, start)
            if key not in resolved:
                name = names[start][0] if start in names else 'sub_%x' % start
                buf = ctypes.create_string_buffer(4096)
                if undname(name.encode(), buf, len(buf), 0x1000):
                    name = buf.value.decode(errors='replace')
                resolved[key] = module + '!' + name
            return resolved[key]
    return module + '!rva_%x' % rva


counts = defaultdict(Counter)
with (d / 'cpu-samples.csv').open(encoding='utf-8-sig', newline='') as f:
    for row in csv.DictReader(f):
        # Five-second bins preserve menu/load/streaming separation. Full stacks
        # are deduplicated per function before inclusive sample attribution.
        counts[(int(float(row['timeMs']) // 5000), int(row['tid']))][int(row['stack'])] += 1
wanted = {s for c in counts.values() for s in c if s >= 0}
stacks = defaultdict(list)
with (d / 'stack-frames.csv').open(encoding='utf-8-sig', newline='') as f:
    for row in csv.DictReader(f):
        sid = int(row['stack'])
        if sid in wanted:
            stacks[sid].append(label(row))
result = {'warning': 'Approximate sampled CPU, inclusive functions overlap; not elapsed latency. Five-second bins are not automatic engine phase boundaries. Small thread/bins and function lists are truncated as specified below.',
          'allCpuSamples': sum(sum(c.values()) for c in counts.values()),
          'minimumSamplesPerThreadBin': 100, 'leafRowsPerBin': 25,
          'inclusiveRowsPerBin': 70, 'bins': []}
for (second_bin, tid), counter in sorted(counts.items()):
    if sum(counter.values()) < 100:
        continue
    inclusive, leaf = Counter(), Counter()
    for sid, count in counter.items():
        chain = stacks.get(sid, ['missing'])
        leaf[chain[0]] += count
        for name in set(chain):
            inclusive[name] += count
    result['bins'].append({'beginMs': second_bin * 5000, 'tid': tid, 'samples': sum(counter.values()),
                           'leaf': leaf.most_common(25), 'inclusive': inclusive.most_common(70),
                           'topStacks': [{'samples': n, 'stack': stacks.get(s, [])} for s, n in counter.most_common(5)]})
(a.run / 'cpu-analysis.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
threads = Counter()
for (_, tid), counter in counts.items():
    threads[tid] += sum(counter.values())
display_tid = a.display_tid if a.display_tid is not None else threads.most_common(1)[0][0]
for b in result['bins']:
    if b['tid'] == display_tid:
        print(json.dumps({k: b[k] for k in ('beginMs', 'tid', 'samples', 'leaf')}, ensure_ascii=False))
