"""Test the hook against archived v15 saves and temporary truncated copies.

Build with leopard/test_save_transform.cmd O2 (or Od), then pass the resulting
build/save-transform/actual-O2.exe and one or more v15 save directories here.
"""
import argparse
import hashlib
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, 'D:/DEV/Kenshi/tools')
from modread import Reader


PAIRS = ('transform', 'record header', 'reference arguments')
WIDTHS = (28, 8, 8)


def read_offsets(data, allow_tail=False):
    reader = Reader(data)
    assert reader.i32() == 15
    reader.i32()
    offsets = ([], [], [])
    for _ in range(reader.i32()):
        offsets[1].append(reader.p + 4)  # type and numeric ID follow the instance counter
        reader.p += 12  # instance counter, type, numeric ID
        reader.string()
        reader.string()
        reader.p += 4   # change flag
        for width in (1, 4, 4, 12, 16, 0, 0):
            for _ in range(reader.i32()):
                reader.string()
                if width:
                    reader.p += width
                else:
                    reader.string()
        for _ in range(reader.i32()):
            reader.string()
            for _ in range(reader.i32()):
                reader.string()
                offsets[2].append(reader.p + 4)  # second and third argument
                reader.p += 12  # saved v15 references have three integer arguments
        for _ in range(reader.i32()):
            reader.string()
            reader.string()
            offsets[0].append(reader.p)
            reader.p += 28
            for _ in range(reader.i32()):
                reader.string()
    # World and zone files can carry data after this first GameDataContainer.
    assert reader.p <= len(data) and (allow_tail or reader.p == len(data)), (reader.p, len(data))
    return offsets


def fixtures(roots):
    known = set()
    for root in roots:
        files = [root / 'quick.save']
        for name in ('platoon', 'zone'):
            group = sorted((root / name).glob('*.' + name), key=lambda p: (-p.stat().st_size, p.name))
            files.extend(group[:2])
        for file in files:
            data = file.read_bytes()
            digest = hashlib.sha256(data).digest()
            if digest in known:
                continue
            known.add(digest)
            offsets = read_offsets(data, file.suffix != '.platoon')
            if any(offsets):
                yield file, data, offsets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('saves', type=Path, nargs='+')
    options = parser.parse_args()
    exe = options.exe.resolve(strict=True)
    full = list(fixtures(options.saves))
    assert {file.suffix for file, _, _ in full} == {'.save', '.platoon', '.zone'}
    print(f'{len(full)} complete v15 files', flush=True)
    build = exe.parent
    with tempfile.TemporaryDirectory(prefix='save-cuts-', dir=build) as work:
        stage = Path(work).resolve(strict=True)
        assert stage.parent == build
        for kind, (name, width) in enumerate(zip(PAIRS, WIDTHS)):
            files = [(file, data, offsets[kind]) for file, data, offsets in full if offsets[kind]]
            suffixes = {file.suffix for file, _, _ in files}
            assert {'.save', '.platoon'} <= suffixes
            print(f'{name}: {sum(len(offsets) for _, _, offsets in files)} decoded offsets', flush=True)
            args = []
            for file, data, offsets in files:
                for offset in sorted({offsets[0], offsets[len(offsets) // 2], offsets[-1]}):
                    args.extend((str(file.resolve()), str(offset), str(len(data) - offset)))
            for suffix in sorted(suffixes):
                file, data, offsets = min((row for row in files if row[0].suffix == suffix), key=lambda row: row[2][0])
                offset = offsets[0]
                lengths = (0, 11, 12, 13, 27, 28) if kind == 0 and suffix != '.save' else range(width + 1)
                for retained in lengths:
                    cut = stage / f'cut-{kind}-{retained}{file.suffix}'
                    cut.write_bytes(data[:offset + retained])
                    args.extend((str(cut), str(offset), str(retained)))
            subprocess.run([str(exe), str(kind), *args], cwd=stage, check=True)


if __name__ == '__main__':
    main()
