"""Verify save optimization native profiles from pinned local Steam/GOG files.

No game executable is launched or redistributed. Include the noncanonical GOG
file to verify that a similarly shaped image still has a different identity.
"""

from pathlib import Path
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import os
import re
import struct
import subprocess
import sys
from tempfile import TemporaryDirectory

sys.path.insert(0, 'D:/DEV/Kenshi/tools/_native_python')
import pefile

REPO = Path(__file__).resolve().parents[1]
ROOT = REPO / 'src/fixes/world'
GOG_SHA = '2defc287c813798b9682b93acf68d340da895cf5164d342145ba0faeb9024681'
STEAM_SHA = '504b362cde850d56afb1cea6f5b7b0ee014d9dd7b47e188599d91c804502cd3e'
DEFAULT_STEAM = Path('D:/SteamLibrary/steamapps/common/Kenshi/RE_Kenshi/Kenshi_x64.exe')

# Pair each GOG range with the established Steam proof in the same TU. Includes
# the exact parser context, helper ABI, branch thunks and stack use on these paths.
RANGES = {
    'read_buffer': [0x6c7c40, 0x6c0221, 0x38bfe, 0x6c7c7b, 0x6c7c9b, 0x6c7cd3, 0x13a1b, 0x1322e0],
    'string_transfer': [0xa47b0, 0x1d51b, 0x6c17b8, 0x6c1843, 0x6c196c, 0x6ca51e, 0xed4d2a],
    'field_key_patch': [0x6c88e0, 0x6c8f50, 0x6c98e0, 0x6c9c30, 0x6c9fa0, 0x6c92a0,
                        0x69cd0, 0x4d83d, 0x6c0c4e, 0x6c0fd0, 0x69b80, 0x280fb, 0x6bba40, 0x4e93b],
    'instance_lookup': [0x6ca420, 0x100630, 0xfe310, 0x691a0, 0x5163b, 0x216b1, 0x240a5,
                        0x6c0110, 0x6c1a9a, 0xed5312],
    'state_list_transfer': [0x6ca420, 0x6c1a9a, 0x6c1530, 0x9f8a0, 0x9a9d0, 0xfe490],
}


def fnv(data):
    result = 14695981039346656037
    for byte in data:
        result = ((result ^ byte) * 1099511628211) & 0xffffffffffffffff
    return result


def image(path, sha, directories=()):
    data = path.read_bytes()
    assert hashlib.sha256(data).hexdigest() == sha, path
    result = pefile.PE(data=data, fast_load=True)
    if directories:
        result.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY[name] for name in directories])
    return result


def rel(pe, rva, opcode, offset=1):
    assert pe.get_data(rva, len(opcode)) == opcode, hex(rva)
    return rva + offset + 4 + struct.unpack('<i', pe.get_data(rva + offset, 4))[0]


def import_at(pe, rva, dll, name):
    addr = pe.OPTIONAL_HEADER.ImageBase + rva
    found = [(row.dll.lower(), i.name or i.ordinal) for row in pe.DIRECTORY_ENTRY_IMPORT
             for i in row.imports if i.address == addr]
    assert found == [(dll.lower(), name)], (hex(rva), found)


def source_proof(pe, steam, name, ranges):
    source = (ROOT / f'save_{name}.cpp').read_text()
    arrays = {name: bytes.fromhex(raw.replace('0x', '').replace(',', '')) for name, raw
              in re.findall(r'static const unsigned char (\w+)\[\]=\{([^\n]*?)\};', source)}
    proof = re.findall(r'\{0x([\da-f]+),(\w+),sizeof\(\2\)\}', source)
    windows = re.findall(r'\{0x([\da-f]+),(\d+),0x([\da-f]{16})ULL\}', source)
    assert len(proof) == len(ranges) == len(windows), (name, len(windows))
    reloc = {entry.rva for block in pe.DIRECTORY_ENTRY_BASERELOC for entry in block.entries if entry.type}
    for (steam_rva, sym), (rva, size, digest), gog_rva in zip(proof, windows, ranges):
        old = arrays[sym]
        assert (int(rva, 16), int(size)) == (gog_rva, len(old)), (name, sym)
        assert steam.get_data(int(steam_rva, 16), len(old)) == old, (name, sym, 'steam')
        assert not any(gog_rva <= at < gog_rva + len(old) for at in reloc), (name, sym, 'relocation')
        current = pe.get_data(gog_rva, len(old))
        assert fnv(current) == int(digest, 16), (name, sym, 'gog')
        changed = bytes([current[0] ^ 1]) + current[1:]
        assert fnv(changed) != int(digest, 16), (name, sym, 'negative')
    return len(windows)


def details(gog):
    for call, thunk, body in [
        (0x6c0237, 0x38bfe, 0x6c7c40),
        *[(call, 0x1d51b, 0xa47b0) for call in [0x6c17c7, 0x6c1852, 0x6c197b, 0x6ca52c]],
        *[(call, 0x4d83d, 0x69cd0) for call in [0x6c89f0, 0x6c9060, 0x6c99f0, 0x6c9d4a, 0x6ca0ba, 0x6c93b0]],
        (0x6c9508, 0x280fb, 0x69b80),
        (0x6ca452, 0x5163b, 0xfe310), (0x100667, 0x5163b, 0xfe310),
        (0x6ca48d, 0x216b1, 0x100630), (0x6ca469, 0x240a5, 0x691a0),
        (0x6c1ad2, 0x2d10f, 0x6ca420),
    ]:
        assert rel(gog, call, b'\xe8') == thunk and rel(gog, thunk, b'\xe9') == body
    assert rel(gog, 0x4e93b, b'\xe9') == 0x6bba40
    assert rel(gog, 0x13a1b, b'\xe9') == 0x1322e0
    assert struct.unpack('<Q', gog.get_data(0x168df78, 8))[0] == gog.OPTIONAL_HEADER.ImageBase + 0x13a1b
    assert rel(gog, 0x6c7cd3, b'\x48\x8d\x05', 3) == 0x168df18
    import_at(gog, rel(gog, 0xed5312, b'\xff\x25', 2), b'MSVCR100.dll', b'memcmp')
    import_at(gog, rel(gog, 0xed4d2a, b'\xff\x25', 2), b'mfc100u.dll', 1278)
    assert (0x6ca4d7 + 1) % 8 == 0
    import_at(gog, rel(gog, 0x6ca4d7, b'\xff\x15', 2), b'OgreMain_x64.dll',
              b'??4Quaternion@Ogre@@QEAAAEAV01@AEBV01@@Z')


def runtime_admission(gog):
    scripts = ['read_buffer', 'string_transfer', 'fields_integration', 'instance_lookup', 'state_list']
    with TemporaryDirectory(prefix='keo-save-gog-') as temp:
        path = Path(temp) / 'image.bin'
        mapped = gog.get_memory_mapped_image()
        size = gog.OPTIONAL_HEADER.SizeOfImage
        assert len(mapped) <= size
        path.write_bytes(mapped.ljust(size, b'\0'))
        env = {**os.environ, 'KEO_GOG_SAVE_TEST_IMAGE': str(path)}

        def run(script):
            command = ['cmd.exe', '/d', '/c', f'leopard\\test_save_{script}.cmd']
            process = subprocess.run(command, cwd=REPO, env=env, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, text=True, errors='replace')
            return script, process

        with ThreadPoolExecutor(max_workers=len(scripts)) as workers:
            results = list(workers.map(run, scripts))
        for _, process in results:
            print(process.stdout, end='')
        failed = [script for script, process in results if process.returncode]
        assert not failed, failed


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--gog-exe', type=Path, required=True)
    parser.add_argument('--other-gog-exe', type=Path)
    parser.add_argument('--steam-exe', type=Path, default=DEFAULT_STEAM)
    parser.add_argument('--runtime', action='store_true', help='Compile and run offline production admission fixtures')
    args = parser.parse_args()
    steam = image(args.steam_exe, STEAM_SHA)
    gog = image(args.gog_exe, GOG_SHA, ('IMAGE_DIRECTORY_ENTRY_IMPORT', 'IMAGE_DIRECTORY_ENTRY_BASERELOC'))
    assert (gog.FILE_HEADER.TimeDateStamp, gog.OPTIONAL_HEADER.SizeOfImage) == (0x65d60519, 0x232a000)
    if args.other_gog_exe:
        other = pefile.PE(str(args.other_gog_exe), fast_load=True)
        assert (other.FILE_HEADER.TimeDateStamp, other.OPTIONAL_HEADER.SizeOfImage) != (0x65d60519, 0x232a000)
    details(gog)
    checked = sum(source_proof(gog, steam, name, ranges) for name, ranges in RANGES.items())
    print(f'PASS: five GOG save paths, {checked} production proof windows, Steam parity, branch/slot/import contracts')
    if args.runtime:
        runtime_admission(gog)
