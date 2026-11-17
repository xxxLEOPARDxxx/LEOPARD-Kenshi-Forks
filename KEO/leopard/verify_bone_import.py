"""Verify authenticated native contract and final Win64 helper/bridge offline."""
from pathlib import Path
import hashlib, importlib.util, json, re, struct, sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, 'D:/DEV/Kenshi/tools/_native_python')
import capstone

spec = importlib.util.spec_from_file_location('pe', root / 'tools/kenshilib/check_imports.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
native = m.PE(Path('D:/SteamLibrary/steamapps/common/Kenshi/OgreMain_x64.dll'))
assert hashlib.sha256(native.data).hexdigest() == '7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6'
text = (root / 'src/fixes/world/bone_import_contract.h').read_text()
arrays = {int(n): bytes(int(x, 16) for x in re.findall(r'0x([0-9a-f]+)', body))
          for n, body in re.findall(r'proof(\d+)\[\]=\{(.*?)\};', text, re.S)}
proofs = [(int(rva, 16), arrays[int(n)]) for rva, n in re.findall(r'\{0x([0-9a-f]+),proof(\d+),sizeof', text)]
assert len(proofs) == 3 and sum(len(b) for _, b in proofs) == 1225
for rva, data in proofs:
    assert native.data[native.offset(rva):native.offset(rva) + len(data)] == data

result = {'nativeProofBytes': 1225, 'variants': {}}
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
for variant in ('dev', 'prod'):
    pe = m.PE(root / ('build/KEO_step4_' + variant + '/KEO.dll'))
    map_text = (root / ('build/obj_step4_' + variant + '/KEO.map')).read_text()
    table, size = pe.directories[3]
    ranges = list(struct.iter_unpack('<III', pe.data[pe.offset(table):pe.offset(table) + size]))
    following = [int(x, 16) - pe.image_base for x in re.findall(r'\s([0-9a-fA-F]{16})\s+f\b', map_text)]
    functions = {}
    for name in ('?Add@bone_import@@', '?Import@bone_import_patch@@'):
        line = next(line for line in map_text.splitlines() if name in line)
        rva = int(re.search(r'\s([0-9a-fA-F]{16})\s+f', line).group(1), 16) - pe.image_base
        entry = next(span for span in ranges if span[0] <= rva < span[1])
        end = min(x for x in following if x > rva)
        assert 0 < end - rva <= 4096
        instructions = list(md.disasm(pe.data[pe.offset(rva):pe.offset(rva) + end - rva], rva))
        calls = [(hex(i.address), i.mnemonic, i.op_str) for i in instructions if i.mnemonic in ('call', 'jmp')]
        if name.startswith('?Add@'):
            callbacks = [i for i in instructions if i.mnemonic == 'call' and re.fullmatch(r'r(?:[0-9]+|[abcd]x|[sd]i|[bs]p)', i.op_str)]
            assert len(callbacks) == 2  # admitted native allocation, then link
            assert [i.address for i in instructions if i.mnemonic == 'call' and callbacks[0].address <= i.address <= callbacks[1].address] == [i.address for i in callbacks]
        # All record copies/comparisons stay integer/bitwise; the helper must
        # never normalize or otherwise calculate with an imported weight.
        assert not any(i.mnemonic in ('addss', 'subss', 'mulss', 'divss', 'addps', 'mulps') for i in instructions)
        functions[name] = {'rva': hex(rva), 'entryUnwind': [hex(entry[0]), hex(entry[1])],
                           'mapRange': [hex(rva), hex(end)], 'callsAndBranches': calls}
    result['variants'][variant] = {'sha256': hashlib.sha256(pe.data).hexdigest(), 'bytes': len(pe.data), 'functions': functions}
out = root / 'build/bone_import/binary-verification.json'
out.write_text(json.dumps(result, indent=2))
print(json.dumps(result, indent=2))
