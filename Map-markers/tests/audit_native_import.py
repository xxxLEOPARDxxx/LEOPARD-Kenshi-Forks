"""Bounded ReadProcessMemory-only save/import call audit of an existing game.

No game launch, UI, native function invocation, process write or suspension.
One .text read (at most 32 MiB), one .rdata read (at most 16 MiB), bounded
unwind metadata and selected listings/hashes only. No large image is saved.
Export labels are retained BEFORE following an installed detour.
"""
from pathlib import Path
import bisect
import ctypes
import hashlib
import json
import re
import struct
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, 'D:/DEV/Kenshi/tools')
import capstone
import livedis
import slots

OUT = Path(__file__).parent/'_generated/native-import'
OUT.mkdir(parents=True, exist_ok=True)
pid = livedis.find_pid()
if not pid:
    raise SystemExit('Kenshi is closed; audit does not launch it')
base = livedis.module_base(pid)
handle = livedis.k32.OpenProcess(0x0410, False, pid)
if not handle or not base:
    raise SystemExit('Read-only process access unavailable')
livedis.k32.CloseHandle.argtypes = [ctypes.c_void_p]
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
symbols = {}
for name, rva, slot in slots.resolve(''):
    if rva:
        symbols.setdefault(rva, []).append(name)
keys = sorted(symbols)
patterns = {
    'manager-execute': ('execute@SaveManager', 0x880),
    'manager-import-request': ('import@SaveManager', 0x100),
    'manager-import': ('importGame@SaveManager', 0x1580),
    'manager-save': ('saveGame@SaveManager', 0x3200),
    'manager-load': ('loadGame@SaveManager', 0x2500),
    'sfs-new': ('newGame@SaveFileSystem', 0x160),
    'sfs-load': ('loadGame@SaveFileSystem', 0x1200),
    'sfs-save': ('saveGame@SaveFileSystem', 0x1000),
    'sfs-thread': ('_NV_threadProc@SaveFileSystem', 0x1000),
    'sfs-singleton': ('getSingleton@SaveFileSystem', 0x80),
    'manager-save-request': ('save@SaveManager', 0x300),
    'main-gpu-loop': ('_NV_mainLoop_GPUSensitiveStuff@GameWorld', 0x400),
    'unnamed-frame': ('0x82aaf0', 0xb00),
    'world-new': ('newGame@KingOfRenderThread', 0x100),
    'world-init': ('initialisation@GameWorld', 0x1800),
    'world-reset': ('resetGame@GameWorld', 0x2000),
}


def nearest(rva):
    index = bisect.bisect_right(keys, rva)-1
    if index < 0 or rva-keys[index] > 0x4000:
        return {'rva': hex(rva), 'nearest': None}
    return {'rva': hex(rva), 'nearestRva': hex(keys[index]),
            'nearest': symbols[keys[index]], 'offset': rva-keys[index]}


try:
    head = livedis.read(handle, base, 0x1000)
    pe = struct.unpack_from('<I', head, 0x3c)[0]
    count = struct.unpack_from('<H', head, pe+6)[0]
    opt = struct.unpack_from('<H', head, pe+20)[0]
    table = pe+24+opt
    sections = []
    rdata_section = None
    for i in range(count):
        p = table+40*i
        name = head[p:p+8].split(b'\0')[0]
        size, rva = struct.unpack_from('<II', head, p+8)
        if name == b'.text':
            sections.append((rva, size))
        if name == b'.rdata':
            rdata_section = (rva, size)
    assert len(sections) == 1
    text_rva, text_size = sections[0]
    assert 0 < text_size <= 32*1024*1024, text_size
    text = livedis.read(handle, base+text_rva, text_size)
    assert len(text) == text_size
    exception_rva, exception_size = struct.unpack_from('<II', head, pe+24+112+3*8)
    assert 0 < exception_size <= 2*1024*1024
    exception = livedis.read(handle, base+exception_rva, exception_size)
    functions = [struct.unpack_from('<III', exception, p) for p in range(0, len(exception)-11, 12)]
    function_keys = [f[0] for f in functions]

    def owner(rva):
        i = bisect.bisect_right(function_keys, rva)-1
        if i >= 0 and functions[i][0] <= rva < functions[i][1]:
            return functions[i][:2]
        return None

    def chain_labels(address):
        chain = []
        for _ in range(8):
            rva = address-base
            chain.append(hex(rva))
            if rva in symbols:
                return {'chain': chain, 'symbol': symbols[rva]}
            try:
                code = livedis.read(handle, address, 6)
            except OSError:
                break
            if code[0] != 0xe9:
                break
            address += 5+struct.unpack_from('<i', code, 1)[0]
        return {'chain': chain, 'symbol': []}

    edges = []
    at = text.find(b'\xe9')
    while 0 <= at <= len(text)-5:
        target = text_rva+at+5+struct.unpack_from('<i', text, at+1)[0]
        edges.append((text_rva+at, target))
        at = text.find(b'\xe9', at+1)

    methods = {}
    for key, (pattern, limit) in patterns.items():
        if pattern.startswith('0x'):
            rva = int(pattern, 16)
            name, slot = '<unexported runtime function>', None
        else:
            hits = slots.resolve(pattern)
            assert len(hits) == 1, (key, hits)
            name, rva, slot = hits[0]
        code = livedis.read(handle, base+rva, limit)
        lines = [f'{name} RVA={rva:x} slot={slot} PID={pid}']
        calls = []
        for ins in md.disasm(code, base+rva):
            line = f'{ins.address-base:08x} {ins.mnemonic:9} {ins.op_str}'
            if ins.mnemonic in ('call', 'jmp') and ins.op_str.startswith('0x'):
                label = chain_labels(int(ins.op_str, 16))
                calls.append(dict(site=hex(ins.address-base), kind=ins.mnemonic, **label))
                if label['symbol']:
                    line += ' ; '+' | '.join(label['symbol'])
            lines.append(line)
            if ins.mnemonic == 'int3':
                break
        (OUT/(key+'.asm')).write_text('\n'.join(lines)+'\n', encoding='utf-8')
        aliases = {rva}
        for _ in range(8):
            added = {a for a, target in edges if target in aliases}-aliases
            if not added:
                break
            aliases.update(added)
        assert len(aliases) <= 128, (key, len(aliases))
        methods[key] = {'name': name, 'rva': hex(rva), 'slot': slot,
                        'bytesRead': len(code), 'sha256': hashlib.sha256(code).hexdigest(),
                        'calls': calls, 'aliases': aliases, 'refs': []}

    by_alias = {}
    for key, method in methods.items():
        for rva in method['aliases']:
            by_alias.setdefault(rva, []).append(key)
    # Candidate direct branches, not an indirect-call completeness claim.
    for byte, kind in [(b'\xe8', 'call'), (b'\xe9', 'jmp')]:
        at = text.find(byte)
        while 0 <= at <= len(text)-5:
            target = text_rva+at+5+struct.unpack_from('<i', text, at+1)[0]
            if target in by_alias:
                for key in by_alias[target]:
                    methods[key]['refs'].append(dict(kind=kind, target=hex(target), **nearest(text_rva+at)))
                    assert len(methods[key]['refs']) <= 128, key
            at = text.find(byte, at+1)
    for method in methods.values():
        method['aliases'] = sorted(hex(rva) for rva in method['aliases'])
        for ref in method['refs']:
            boundary = owner(int(ref['rva'], 16))
            if boundary:
                ref['runtimeFunction'] = [hex(v) for v in boundary]
    save_code_escapes = []
    save_aliases = {int(alias, 16): key for key in ['manager-save', 'sfs-save']
                    for alias in methods[key]['aliases']}
    for prefix in [b'\x48\x8d', b'\x4c\x8d']:
        at = text.find(prefix)
        while 0 <= at <= len(text)-7:
            if text[at+2] & 0xc7 == 5:
                target = text_rva+at+7+struct.unpack_from('<i', text, at+3)[0]
                if target in save_aliases:
                    save_code_escapes.append({'kind': 'lea', 'method': save_aliases[target],
                                              'site': hex(text_rva+at), 'target': hex(target)})
            at = text.find(prefix, at+1)
    for target, key in save_aliases.items():
        at = text.find(struct.pack('<Q', base+target))
        if at >= 0:
            save_code_escapes.append({'kind': 'literal64', 'method': key,
                                      'site': hex(text_rva+at), 'target': hex(target)})
    caller_windows = []
    for ref in methods['manager-execute']['refs']:
        if ref['kind'] != 'call':
            continue
        site = int(ref['rva'], 16)
        boundary = owner(site)
        assert boundary and 0 < boundary[1]-boundary[0] <= 0x6000
        begin, end = boundary
        raw = livedis.read(handle, base+begin, end-begin)
        lines = []
        for ins in md.disasm(raw, base+begin):
            if ins.address-base < site-0x100 or ins.address-base > site+0x100:
                continue
            line = f'{ins.address-base:08x} {ins.mnemonic:9} {ins.op_str}'
            if ins.mnemonic in ('call', 'jmp') and ins.op_str.startswith('0x'):
                label = chain_labels(int(ins.op_str, 16))
                line += ' ; '+' | '.join(label['symbol'])
            lines.append(line)
        key = 'execute-caller-'+hex(site)[2:]
        (OUT/(key+'.asm')).write_text('\n'.join(lines)+'\n', encoding='utf-8')
        caller_windows.append({'site': hex(site), 'begin': hex(begin), 'end': hex(end),
                               'sha256': hashlib.sha256(raw).hexdigest()})
    frame_types = []
    save_pointer_escapes = []
    if rdata_section:
        rrva, rsize = rdata_section
        assert 0 < rsize <= 16*1024*1024
        rdata = livedis.read(handle, base+rrva, rsize)
        for key in ['manager-save', 'sfs-save']:
            for alias in methods[key]['aliases']:
                at = rdata.find(struct.pack('<Q', base+int(alias, 16)))
                if at >= 0:
                    save_pointer_escapes.append({'method': key, 'alias': alias,
                                                 'rdataRva': hex(rrva+at)})
        for alias in methods['unnamed-frame']['aliases']:
            needle = struct.pack('<Q', base+int(alias, 16))
            at = rdata.find(needle)
            while at >= 0:
                for index in range(64):
                    before = at-8*(index+1)
                    if before < 0:
                        break
                    col = struct.unpack_from('<Q', rdata, before)[0]-base
                    if not (rrva <= col <= rrva+rsize-24):
                        continue
                    fields = struct.unpack_from('<6I', rdata, col-rrva)
                    if fields[0] != 1 or fields[5] != col:
                        continue
                    typ = livedis.read(handle, base+fields[3]+16, 160).split(b'\0')[0].decode('ascii', 'replace')
                    if typ.startswith('.?A'):
                        hierarchy = struct.unpack('<4I', livedis.read(handle, base+fields[4], 16))
                        assert 0 < hierarchy[2] <= 16
                        array = livedis.read(handle, base+hierarchy[3], 4*hierarchy[2])
                        bases = []
                        for b in range(hierarchy[2]):
                            descriptor = struct.unpack_from('<I', array, b*4)[0]
                            type_rva = struct.unpack('<I', livedis.read(handle, base+descriptor, 4))[0]
                            bases.append(livedis.read(handle, base+type_rva+16, 160).split(b'\0')[0].decode('ascii', 'replace'))
                        frame_types.append({'methodAlias': alias, 'vtableRva': hex(rrva+before+8),
                                            'index': index, 'type': typ, 'bases': bases})
                        break
                at = rdata.find(needle, at+1)
    # Preserve small, reviewable assertions for the supported native paths.
    def call_refs(key):
        return [r for r in methods[key]['refs'] if r['kind'] == 'call']

    assert len(call_refs('manager-import')) == 1
    assert len(call_refs('manager-save')) == 1
    assert len(call_refs('sfs-save')) == 1
    assert call_refs('manager-import')[0]['nearestRva'] == methods['manager-execute']['rva']
    assert call_refs('manager-save')[0]['nearestRva'] == methods['manager-execute']['rva']
    assert call_refs('sfs-save')[0]['nearestRva'] == methods['manager-save']['rva']
    assert any('resetGame@GameWorld' in n for c in methods['manager-import']['calls'] for n in c['symbol'])
    assert any('newGame@SaveFileSystem' in n for c in methods['world-reset']['calls'] for n in c['symbol'])
    assert any(t['type'] == '.?AVMainListener@@' for t in frame_types)
    globals_by_method = {}
    for key in ['sfs-singleton', 'manager-save', 'manager-load', 'manager-import', 'world-reset']:
        rva = int(methods[key]['rva'], 16)
        raw = livedis.read(handle, base+rva, methods[key]['bytesRead'])
        loads = []
        for ins in md.disasm(raw, base+rva):
            if ins.mnemonic == 'mov' and ins.op_str.startswith('rax, qword ptr [rip'):
                match = re.search(r'rip ([+-]) (0x[0-9a-f]+)', ins.op_str)
                if match:
                    displacement = int(match[2], 16)*(1 if match[1] == '+' else -1)
                    loads.append({'site': hex(ins.address-base),
                                  'storageRva': hex(ins.address-base+ins.size+displacement)})
            if ins.mnemonic == 'int3':
                break
        globals_by_method[key] = loads
    assert len(globals_by_method['sfs-singleton']) == 1
    canonical_storage = globals_by_method['sfs-singleton'][0]['storageRva']
    for key in ['manager-save', 'manager-load', 'manager-import', 'world-reset']:
        assert any(item['storageRva'] == canonical_storage for item in globals_by_method[key]), key
    report = {'format': 'MAP_MARKERS_NATIVE_IMPORT_READ_ONLY', 'version': 1,
              'pid': pid, 'base': hex(base), 'textRva': hex(text_rva),
              'textBytes': len(text), 'textSha256': hashlib.sha256(text).hexdigest(),
              'methods': methods, 'limits': {'textMiB': 32, 'thunkDepth': 8,
              'refsPerMethod': 128}, 'directXrefsOnly': True,
              'executeCallers': caller_windows, 'exceptionDirectoryBytes': len(exception),
              'frameCallerTypes': frame_types, 'savePointerEscapesInRdata': save_pointer_escapes,
              'saveAddressEscapesInCode': save_code_escapes,
              'canonicalSfsStorageRva': canonical_storage, 'singletonLoads': globals_by_method,
              'assertions': 'PASS native reset chain, direct save/import ownership and canonical singleton'}
    (OUT/'native-contract.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(f'Read-only audit PID {pid}: {len(text)} executable bytes, {len(methods)} methods')
    for key in ['manager-import', 'world-init', 'world-reset', 'manager-execute', 'sfs-new', 'sfs-save', 'sfs-load']:
        method = methods[key]
        print(key, 'refs=', len(method['refs']))
        for call in method['calls']:
            if any(s in name for name in call['symbol'] for s in ['SaveFileSystem', 'SaveManager', 'resetGame', 'newGame@', 'Thread']):
                print(' ', call['site'], call['symbol'])
finally:
    livedis.k32.CloseHandle(handle)
