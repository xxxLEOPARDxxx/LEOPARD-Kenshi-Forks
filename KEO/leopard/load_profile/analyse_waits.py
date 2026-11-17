"""Offline coverage and wait accounting. Never attaches to a process or WPR.

CPU samples are approximate; summed time from different threads overlaps.
The owner boundaries describe instruction/readiness, not exact engine phases.
"""
from pathlib import Path
from collections import Counter, defaultdict
import argparse, bisect, csv, ctypes, json


def overlap(start, end, begin, finish):
    return max(0.0, min(end, finish) - max(start, begin))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--symbols', required=True, type=Path)
    args = parser.parse_args()
    run, decoded = args.run, args.run / 'decoded'
    record = json.loads((run / 'result.json').read_text(encoding='utf-8-sig'))
    metadata_rows = list(csv.reader((decoded / 'summary.csv').read_text(encoding='utf-8-sig').splitlines()))
    meta = dict(metadata_rows)
    caveats = [v for k, v in metadata_rows if k == 'caveat']
    symbols = json.loads(args.symbols.read_text(encoding='utf-8-sig'))
    tables = {}
    for module, data in symbols.items():
        names = defaultdict(list)
        for rva, name in data['exports']:
            names[rva].append(name)
        tables[module] = ([r[0] for r in data['ranges']], data['ranges'], names)
    demangled = {}
    undname = ctypes.WinDLL('dbghelp').UnDecorateSymbolName
    undname.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint]
    undname.restype = ctypes.c_uint

    def human(name):
        if name not in demangled:
            buf = ctypes.create_string_buffer(4096)
            ok = undname(name.encode(), buf, len(buf), 0x1000)
            demangled[name] = buf.value.decode(errors='replace') if ok else name
        return demangled[name]

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
                return module + '!' + (human(names[start][0]) if start in names else 'sub_%x' % start)
        return module + '!rva_%x' % rva

    # Derive the ETL relative QPC origin from the SAME event's timestamp/QPC.
    # Frequency comes from the recorder's Stopwatch API, not a guessed CPU rate.
    histogram = defaultdict(Counter)
    qpc_origin = None
    cpu_count = 0
    with (decoded / 'cpu-samples.csv').open(encoding='utf-8-sig', newline='') as f:
        for row in csv.DictReader(f):
            if qpc_origin is None:
                qpc_origin = int(row['qpc']) - float(row['timeMs']) * record['qpcFrequency'] / 1000
            histogram[int(float(row['timeMs']) // 1000)][int(row['tid'])] += 1
            cpu_count += 1
    if qpc_origin is None:
        raise ValueError('No target CPU samples; no coverage claim possible')
    boundaries = {b['name']: (b['qpc'] - qpc_origin) * 1000 / record['qpcFrequency'] for b in record['boundaries']}
    begin, finish = boundaries['owner_told_to_load'], boundaries['owner_npcs_ready']
    if not 0 <= begin < finish <= float(meta['sessionDurationMs']):
        raise ValueError('Owner boundaries fall outside the recorded session')

    waits, total, unknown = defaultdict(Counter), Counter(), Counter()
    intervals = Counter()
    # Streaming: the million-row wait table is not loaded into memory.
    with (decoded / 'off-cpu.csv').open(encoding='utf-8-sig', newline='') as f:
        for row in csv.DictReader(f):
            a, b = float(row['beginMs']), float(row['endMs'])
            ms = overlap(a, b, begin, finish)
            if not ms:
                continue
            tid, stack = int(row['tid']), int(row['blockingStack'])
            total[tid] += ms
            intervals[tid] += 1
            if stack < 0:
                unknown[tid] += ms
            # Keep sizeable individual waits; tiny-wait totals remain in total.
            if ms >= 1:
                waits[tid][stack] += ms
    wanted = {s for counter in waits.values() for s, _ in counter.most_common(8) if s >= 0}
    stacks = defaultdict(list)
    with (decoded / 'stack-frames.csv').open(encoding='utf-8-sig', newline='') as f:
        for row in csv.DictReader(f):
            stack = int(row['stack'])
            if stack in wanted:
                stacks[stack].append(label(row))
    edge_gaps = []
    with (decoded / 'edge-gaps.csv').open(encoding='utf-8-sig', newline='') as f:
        for row in csv.DictReader(f):
            if overlap(float(row['beginMs']), float(row['endMs']), begin, finish):
                edge_gaps.append(row)
    result = {
        'ownerBoundaryMs': boundaries, 'metadata': meta, 'countedCpuSamples': cpu_count,
        'metadataCaveats': caveats,
        'unmatchedEdgeGapsIntersectingOwnerWindow': edge_gaps,
        'cpuSamplesPerSecondPerTid': {s: dict(c) for s, c in sorted(histogram.items())},
        'coverage': {
            'firstTargetSampleMs': float(meta['firstTargetSampleMs']),
            'lastTargetSampleMs': float(meta['lastTargetSampleMs']),
            'eventsLost': int(meta['eventsLost']),
            'sampleMissingStackFraction': int(meta['missingSampleStacks']) / max(cpu_count, 1),
            'secondsWithTargetSamples': len(histogram),
            'limitation': 'Continuous sampling coverage and zero reported loss do not measure profiling overhead or prove every individual event was retained.',
        },
        'waitsPerTid': {
            tid: {'matchedOffCpuMs': total[tid], 'matchedIntervals': intervals[tid], 'matchedUnknownStackMs': unknown[tid],
                  'largestStacksFromIntervalsAtLeast1ms': [
                      {'ms': ms, 'stackId': s, 'stack': stacks.get(s, [])} for s, ms in waits[tid].most_common(8)]}
            for tid in sorted(total)
        },
        'warning': 'Wait totals cover MATCHED intervals only; unmatched edge gaps are separate unknown coverage, not measured waiting. Off-CPU time overlaps across threads. Idle workers are not loading bottlenecks. Compare with the critical executing thread and its sampled CPU work.',
    }
    (run / 'wait-analysis.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps({'ownerBoundaryMs': boundaries, 'coverage': result['coverage']}, indent=2))


if __name__ == '__main__':
    assert overlap(0, 20, 5, 15) == 10
    assert overlap(0, 3, 5, 15) == 0
    assert overlap(8, 12, 5, 15) == 4
    main()
