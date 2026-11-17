"""Authenticate native contracts and verify final helper stack/unwind offline."""
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
text = (root / 'src/fixes/world/bone_index_contract.h').read_text()
arrays = {int(n): bytes(int(x, 16) for x in re.findall(r'0x([0-9a-f]+)', body))
          for n, body in re.findall(r'proof(\d+)\[\]=\{(.*?)\};', text, re.S)}
proofs = [(int(rva, 16), arrays[int(n)]) for rva, n in re.findall(r'\{0x([0-9a-f]+),proof(\d+),sizeof', text)]
assert len(proofs) == 2 and sum(len(b) for _, b in proofs) == 1135
for rva, data in proofs:
    assert native.data[native.offset(rva):native.offset(rva) + len(data)] == data
result = {'nativeProofBytes': 1135, 'scratchBytes': 8320, 'variants': {}}
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
for variant in ('dev', 'prod'):
    pe = m.PE(root / ('build/KEO_step4_' + variant + '/KEO.dll'))
    map_text = (root / ('build/obj_step4_' + variant + '/KEO.map')).read_text()
    table, size = pe.directories[3]
    ranges = list(struct.iter_unpack('<III', pe.data[pe.offset(table):pe.offset(table) + size]))
    symbols = [(int(addr, 16) - pe.image_base, name) for name, addr in re.findall(r'^\s*[0-9a-fA-F]+:[0-9a-fA-F]+\s+(\S+)\s+([0-9a-fA-F]{16})\s+f\b', map_text, re.M)]
    probe = next(rva for rva, name in symbols if name == '__chkstk')
    functions = {}
    for prefix in ('?Try@bone_index@@', '?Build@bone_index_patch@@'):
        rva = next(rva for rva, name in symbols if name.startswith(prefix))
        end = min(x for x, _ in symbols if x > rva)
        entry = next(span for span in ranges if span[0] <= rva < span[1])
        assert 0 < end - rva < 8192
        insns = list(md.disasm(pe.data[pe.offset(rva):pe.offset(rva) + end - rva], rva))
        assert not any(i.mnemonic in ('addss', 'subss', 'mulss', 'divss', 'addps', 'mulps') for i in insns)
        calls = [(hex(i.address), i.mnemonic, i.op_str) for i in insns if i.mnemonic == 'call']
        if prefix.startswith('?Try@'):
            assert any(i.mnemonic == 'call' and i.op_str == hex(probe) for i in insns)
            frames = [int(i.op_str.split(', ')[1], 16) for i in insns if i.mnemonic == 'mov' and i.op_str.startswith('eax, 0x')]
            assert any(8320 <= n < 12288 for n in frames)
            assert any(i.mnemonic == 'bsf' for i in insns)
        functions[prefix] = {'rva': hex(rva), 'mapEnd': hex(end), 'unwindRange': [hex(entry[0]), hex(entry[1])], 'calls': calls}
    result['variants'][variant] = {'sha256': hashlib.sha256(pe.data).hexdigest(), 'bytes': len(pe.data), 'functions': functions}
out = root / 'build/bone_index/binary-verification.json'
out.parent.mkdir(exist_ok=True)
out.write_text(json.dumps(result, indent=2))
print(json.dumps(result, indent=2))
