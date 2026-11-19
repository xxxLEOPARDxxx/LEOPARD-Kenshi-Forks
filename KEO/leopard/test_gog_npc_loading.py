"""Check NPC/resource hook contracts on local licensed Steam and GOG files."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

sys.path.insert(0, 'D:/DEV/Kenshi/tools/_native_python')
import pefile
from capstone import CS_ARCH_X86, CS_GRP_CALL, CS_GRP_JUMP, CS_MODE_64, Cs
from capstone.x86_const import X86_OP_IMM, X86_OP_MEM, X86_OP_REG, X86_REG_RIP

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('gog', type=Path)
parser.add_argument('other_gog', type=Path)
parser.add_argument('--game', type=Path, default=Path('D:/SteamLibrary/steamapps/common/Kenshi'))
args = parser.parse_args()


def image(path, expected_sha, stamp, size):
    binary = path.read_bytes()
    assert hashlib.sha256(binary).hexdigest() == expected_sha, path
    pe = pefile.PE(data=binary, fast_load=True)
    assert pe.FILE_HEADER.TimeDateStamp == stamp and pe.OPTIONAL_HEADER.SizeOfImage == size, path
    return pe


steam = image(args.game / 'RE_Kenshi/Kenshi_x64.exe',
              '504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e', 0x65D604D7, 0x232C000)
gog = image(args.gog, '2defc287c813798b9682b93acf68d340da895cf5164d342145ba0faeb9024681', 0x65D60519, 0x232A000)
other = image(args.other_gog, 'c2fbac686513bc73a5058b8706fc4be5043598c21e2e4d4b248925fceb2cfe56', 0x6602D5E3, 0x232C000)
ogre = image(args.game / 'OgreMain_x64.dll',
             '7d3888d21398647519f10a89b1f300d8606f761ad159e6021a797b1b9b3df5f6', 0x5CA5F929, 0x9C9000)


def source(filename):
    return (ROOT / filename).read_text(encoding='utf-8')


def definition(code, name):
    return re.search(r'\b' + name + r'(?:\[\d*\])*\s*=\s*(.*?);', code, re.S).group(1)


def byte_values(code, name):
    return bytes(int(value, 16) for value in re.findall(r'0x[0-9a-fA-F]+', definition(code, name)))


def native_proofs(code, table, pe, count):
    rows = re.findall(r'\{(0x[0-9a-f]+),(\w+),sizeof\(\2\)\}', definition(code, table))
    assert len(rows) == count, table
    for rva, name in rows:
        expected = byte_values(code, name)
        assert pe.get_data(int(rva, 16), len(expected)) == expected, (table, rva, name)


def relative(pe, at, opcode):
    encoded = pe.get_data(at, 5)
    assert encoded[0] == opcode, hex(at)
    return at + 5 + int.from_bytes(encoded[1:], 'little', signed=True)


equipment = source('src/fixes/world/equipment_field_patch.cpp')
native_proofs(equipment, 'proofs', steam, 10)
native_proofs(equipment, 'gogProofs', gog, 10)
assert 'Arm(sites,gog?gogCallBytes:callBytes)' in equipment
gog_sites = (0x535EBC, 0x537FA6, 0x5381EE)
assert tuple(int(rva, 16) for rva in re.findall(r'0x[0-9a-f]+', definition(equipment, 'gogCallSites'))) == gog_sites
assert byte_values(equipment, 'gogCallBytes') == b''.join(gog.get_data(rva, 5) for rva in gog_sites)
for rva, slot_lea in zip(gog_sites, (0x535E8D, 0x537F79, 0x5381CB)):
    assert relative(gog, rva, 0xE8) == 0x48171 and gog.get_data(rva + 7, 1) == b'\x28'
    slot = gog.get_data(slot_lea, 7)
    assert slot[:3] == b'\x48\x8d\x15'
    slot_rva = slot_lea + 7 + int.from_bytes(slot[3:], 'little', signed=True)
    assert gog.get_data(slot_rva, 5) == b'slot\0'
for thunk, entry in ((0x48171, 0x6CEF0), (0x13F39, 0x6A6B0), (0x5065F, 0x6A290)):
    assert relative(gog, thunk, 0xE9) == entry

world = 'src/fixes/world/'
for filename, count in (('animation_dirty_patch.cpp', 5), ('equipment_animation_patch.cpp', 10)):
    code = source(world + filename)
    native_proofs(code, 'proofs', ogre, count)
    assert 'NativeProfileIsGog()' in code and 'stamp!=(gog?0x65d60519:0x65d604d7)' in code

morph = source(world + 'morph_pose_scope.cpp')
native_proofs(morph, 'gameProofs', steam, 4)
native_proofs(morph, 'gogGameProofs', gog, 4)
native_proofs(morph, 'ogreProofs', ogre, 7)
assert 'caller==gameBase+0x71ae7' in morph and 'caller==gameBase+0x71ce2' in morph
assert other.get_data(0x71A80, 32) != byte_values(morph, 'proof3')

probe = source('src/resources/resource_load_probe.cpp')
for name, steam_rva, gog_rva in (
    ('textureBytes', 0x44ACD0, 0x44A8F0), ('morphSwitchBytes', 0x71920, 0x71920),
    ('morphCreateBytes', 0x71A80, 0x71A80),
):
    declared = byte_values(probe, name)
    assert len(declared) == 32 and steam.get_data(steam_rva, 32) == gog.get_data(gog_rva, 32) == declared
for name, rva in (('gogTextureContract', 0x44AA20), ('gogMorphSwitchContract', 0x71A5A), ('gogMorphCreateContract', 0x71CD0)):
    declared = byte_values(probe, name)
    assert len(declared) == 32 and gog.get_data(rva, 32) == declared

disassembler = Cs(CS_ARCH_X86, CS_MODE_64)
disassembler.detail = True


def imports(pe):
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_IMPORT']])
    return {entry.address - pe.OPTIONAL_HEADER.ImageBase: (module.dll, entry.name)
            for module in pe.DIRECTORY_ENTRY_IMPORT for entry in module.imports}


def instruction_shape(pe, names, start, size):
    for ins in disassembler.disasm(pe.get_data(start, size), start):
        operands = []
        for operand in ins.operands:
            if operand.type == X86_OP_REG:
                operands.append(operand.reg)
            elif operand.type == X86_OP_IMM:
                target = operand.imm
                if ins.group(CS_GRP_CALL) or ins.group(CS_GRP_JUMP):
                    target = target - start if start <= target < start + size else 'external'
                operands.append(target)
            elif operand.type == X86_OP_MEM:
                mem = operand.mem
                disp = mem.disp
                if mem.base == X86_REG_RIP:
                    disp = names.get(ins.address + ins.size + disp)
                operands.append((mem.base, mem.index, mem.scale, disp))
        yield ins.mnemonic, operands


steam_imports, gog_imports = imports(steam), imports(gog)
for steam_rva, gog_rva, size, count in (
    (0x44ACD0, 0x44A8F0, 0x16F, 81), (0x71920, 0x71920, 0x15A, 94),
    (0x71A80, 0x71A80, 0x2F4, 204),
):
    steam_shape = list(instruction_shape(steam, steam_imports, steam_rva, size))
    gog_shape = list(instruction_shape(gog, gog_imports, gog_rva, size))
    assert len(steam_shape) == count and steam_shape == gog_shape, hex(gog_rva)

bridge = source('src/resources/resource_gpu_bridge.cpp')
windows = re.findall(r'MatchGogArrayBody\(exe\+(0x[0-9a-f]+),(0x[0-9a-f]+),(0x[0-9a-f]+)ULL\)', bridge)
assert len(windows) == 2
for rva, size, fingerprint in (tuple(int(part, 16) for part in window) for window in windows):
    actual = 14695981039346656037
    for byte in gog.get_data(rva, size):
        actual = ((actual ^ byte) * 1099511628211) & ((1 << 64) - 1)
    assert actual == fingerprint, hex(rva)
assert 'target==exe+0x4487e0' in bridge
assert list(instruction_shape(steam, steam_imports, 0x448BC0, 0x1CE)) == list(
    instruction_shape(gog, gog_imports, 0x4487E0, 0x1CE))
assert gog.get_data(0x447A61, 7) == bytes.fromhex('488d8fa0000000')

print('PASS pinned GOG NPC, texture and terrain-array proofs, Steam parity and alternate-image refusal')
