"""Offline evidence; never loads a DLL or opens a process."""
from pathlib import Path
import hashlib
import json
import struct
import sys
import pefile
sys.path.insert(0, 'D:/DEV/Kenshi/tools')
import slots
from qol_binary_check import PE

game = Path('D:/SteamLibrary/steamapps/common/Kenshi')
tables = {}
for platform in ('Steam', 'GOG'):
    path = game / 'RE_Kenshi/RVAs' / (platform + '_1.0.65.br')
    tables[platform] = struct.unpack('<7630I', path.read_bytes())
rows = []
for kind, slot in (('Character', 2019), ('Animal', 600), ('Trader', 6959)):
    pattern = kind + 'InventoryLayout_setupSections_PLACEHOLDER'
    matches = slots.resolve(pattern)
    assert len(matches) == 1 and matches[0][2] == slot, matches
    rows.append({'layout': kind, 'export': pattern, 'slot': slot,
                 **{p: hex(t[slot]) for p, t in tables.items()}})
assert tables['GOG'][600] == 0x14f450  # Original bug: hooked Animal on GOG.
assert tables['GOG'][2019] == 0x14edc0
assert tables['Steam'][2019] == 0x14eea0

report = {'offline_only': True, 'layouts': rows, 'binaries': []}
for name in ('Organize-the-Trader', 'UITweaks'):
    path = Path('D:/DEV/Kenshi/build') / name / (name + '.dll')
    pe = PE(path)
    assert pe.machine == 0x8664
    assert '?startPlugin@@YAXXZ' in pe.exports()
    assert 'MCM_Describe' in pe.exports()
    checked = 0
    if name == 'Organize-the-Trader':
        all_imports = [symbol for names in pe.imports().values() for symbol in names]
        assert any('getAnyPlayerCharacter@PlayerInterface' in symbol for symbol in all_imports), 'Player-only wallet source missing'
        # getMoney is virtual: requiring a DLL import would reject its correct
        # vtable dispatch. Verify the emitted call following the player getter.
        image = pefile.PE(str(path))
        wallet_iat = [entry.address - image.OPTIONAL_HEADER.ImageBase
                      for dep in image.DIRECTORY_ENTRY_IMPORT for entry in dep.imports
                      if entry.name and b'getAnyPlayerCharacter@PlayerInterface' in entry.name]
        assert len(wallet_iat) == 1
        calls = []
        for section in image.sections:
            if not section.Characteristics & 0x20000000:
                continue
            body = section.get_data()
            for offset in range(len(body) - 64):
                if body[offset:offset+2] != b'\xff\x15':
                    continue
                target = section.VirtualAddress + offset + 6 + struct.unpack_from('<i', body, offset+2)[0]
                if target == wallet_iat[0]:
                    following = body[offset+6:offset+64]
                    assert bytes.fromhex('488b10488bc8ff92b8010000') in following, 'Wrong native wallet virtual slot'
                    calls.append(hex(section.VirtualAddress + offset))
        assert len(calls) == 1, calls
        report['player_wallet'] = {'player_getter_callsites': calls, 'wallet_vtable_slot': '0x1b8',
                                  'read_only': True, 'character_pointer_cached': False}
    for dep, names in pe.imports().items():
        if dep.lower() not in {'kenshilib.dll', 'ogremain_x64.dll', 'myguiengine_x64.dll'}:
            continue
        missing = set(names) - PE(game / dep).exports()
        assert not missing, (name, dep, missing)
        checked += len(names)
    report['binaries'].append({'name': name, 'bytes': path.stat().st_size,
        'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'verified_native_imports': checked})
for platform in tables:
    path = game / 'RE_Kenshi/RVAs' / (platform + '_1.0.65.br')
    report[platform + '_table_sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
print(json.dumps(report, indent=2))
