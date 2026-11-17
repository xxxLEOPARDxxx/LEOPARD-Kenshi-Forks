"""Offline authenticated native proof and final Win64 binary checks."""
from pathlib import Path
import hashlib, importlib.util, json, re, sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, 'D:/DEV/Kenshi/tools/_native_python')
import capstone

spec = importlib.util.spec_from_file_location('pe', root / 'tools/kenshilib/check_imports.py')
pe_mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pe_mod)
game = Path('D:/SteamLibrary/steamapps/common/Kenshi')
native = pe_mod.PE(game / 'OgreMain_x64.dll')
assert hashlib.sha256(native.data).hexdigest() == '7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6'
text = (root / 'src/fixes/world/mesh_weights_contract.h').read_text()
arrays = {int(n): bytes(int(x, 16) for x in re.findall(r'0x([0-9a-f]+)', body))
          for n, body in re.findall(r'proof(\d+)\[\]=\{(.*?)\};', text, re.S)}
proofs = [(int(rva, 16), arrays[int(n)]) for rva, n in re.findall(r'\{0x([0-9a-f]+),proof(\d+),sizeof', text)]
assert len(proofs) == 5 and sum(len(b) for _, b in proofs) == 2383
for rva, data in proofs:
    assert native.data[native.offset(rva):native.offset(rva) + len(data)] == data

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
result = {'nativeProofBytes': 2383, 'variants': {}}
for variant in ('dev', 'prod'):
    path = root / ('build/KEO_step4_' + variant + '/KEO.dll')
    pe = pe_mod.PE(path)
    map_text = (root / ('build/obj_step4_' + variant + '/KEO.map')).read_text()
    found = {}
    for name in ('Try@mesh_weights@@', 'Rationalise@mesh_weights_patch@@'):
        line = next(line for line in map_text.splitlines() if name in line)
        address = int(re.search(r'\s([0-9a-fA-F]{16})\s+f', line).group(1), 16)
        rva = address - pe.image_base
        import struct
        table_rva, table_size = pe.directories[3]
        ranges = list(struct.iter_unpack('<III', pe.data[pe.offset(table_rva):pe.offset(table_rva) + table_size]))
        entry_begin, entry_end, _ = next(span for span in ranges if span[0] <= rva < span[1])
        # LTCG splits this function's early-return and normalization regions
        # into multiple unwind entries. Inspect through the next MAP function,
        # not merely the first .pdata fragment (which contains no arithmetic).
        following = [int(x, 16) - pe.image_base for x in re.findall(r'\s([0-9a-fA-F]{16})\s+f\b', map_text)]
        begin, end = rva, min(x for x in following if x > rva)
        assert 0 < end - begin <= 4096
        instructions = list(md.disasm(pe.data[pe.offset(begin):pe.offset(begin) + end - begin], begin))
        if name.startswith('Try@'):
            arithmetic = [(hex(i.address), i.mnemonic, i.op_str) for i in instructions if i.mnemonic in ('addss', 'subss', 'divss', 'mulss')]
            assert all(any(op == wanted for _, op, _ in arithmetic) for wanted in ('addss', 'subss', 'divss', 'mulss'))
            assert not any(i.mnemonic.startswith(('vfm', 'rcp', 'fadd', 'fdiv')) for i in instructions)
        else:
            arithmetic = []
        found[name] = {'rva': hex(rva), 'entryUnwindRange': [hex(entry_begin), hex(entry_end)],
                       'mapCodeRange': [hex(begin), hex(end)],
                       'overlappingUnwindRanges': [[hex(x), hex(y)] for x, y, _ in ranges if x < end and y > begin],
                       'scalarArithmetic': arithmetic}
    result['variants'][variant] = {'sha256': hashlib.sha256(pe.data).hexdigest(), 'bytes': len(pe.data), 'functions': found}
out = root / 'build/mesh-weights-binary-verification.json'
out.write_text(json.dumps(result, indent=2))
print(json.dumps(result, indent=2))
