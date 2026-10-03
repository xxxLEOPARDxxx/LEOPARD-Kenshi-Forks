"""Read-only checks against this machine's installed runtime; never loads DLLs."""
import pathlib
import struct
import sys
import ctypes
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[3] / 'tools' / 'binary_inspect_deps'))
import pefile

root = pathlib.Path(__file__).resolve().parents[1]
game = pathlib.Path('E:/SteamLibrary/steamapps/common/Kenshi')
exe = pefile.PE(str(game / 'RE_Kenshi/Kenshi_x64.exe'))
base = exe.OPTIONAL_HEADER.ImageBase
targets = [0x14EEA0]
# This executable uses incremental-link E9 thunks in its virtual tables.
code_section = exe.sections[0]
code = code_section.get_data()
offset = code.find(b'\xe9')
while offset >= 0:
    if offset + 5 <= len(code):
        destination = code_section.VirtualAddress + offset + 5 + struct.unpack_from('<i', code, offset + 1)[0]
        if destination == 0x14EEA0:
            targets.append(code_section.VirtualAddress + offset)
    offset = code.find(b'\xe9', offset + 1)
matches = []
for target_rva in targets:
  target = struct.pack('<Q', base + target_rva)
  for section in exe.sections:
    if not section.Name.startswith(b'.rdata'):
        continue
    data = section.get_data()
    start = 0
    while True:
        offset = data.find(target, start)
        if offset < 0:
            break
        start = offset + 1
        # setupSections is slot 1 after the destructor, per installed header.
        if offset < 16:
            continue
        col = struct.unpack_from('<Q', data, offset - 16)[0] - base
        try:
            signature, _, _, type_rva, _, _ = struct.unpack('<6I', exe.get_data(col, 24))
            if signature != 1:
                continue
            name = exe.get_string_at_rva(type_rva + 16).decode('ascii')
            matches.append(name)
            print('RVA 0x14EEA0: vtable slot 1 via', hex(target_rva), 'RTTI', name)
        except (pefile.PEFormatError, struct.error, UnicodeDecodeError):
            pass
assert matches, 'No slot-1 RTTI match for upstream hook target'

dll = pefile.PE(str(root / 'build/Release/Organize-the-Trader.dll'))
live_exports = {}
if len(sys.argv) == 2:
    # Optional running-game PID. ReadProcessMemory only; no injection or writes.
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.restype = ctypes.c_void_p
    kernel.ReadProcessMemory.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p]
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    process = kernel.OpenProcess(0x410, False, int(sys.argv[1]))
    assert process, 'Cannot open process for read-only inspection'
    def read(address, size):
        buffer = ctypes.create_string_buffer(size)
        assert kernel.ReadProcessMemory(process, address, buffer, size, None)
        return buffer.raw
    try:
        psapi = ctypes.WinDLL('psapi')
        psapi.EnumProcessModulesEx.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ulong, ctypes.c_void_p, ctypes.c_ulong]
        psapi.GetModuleFileNameExW.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_ulong]
        modules = (ctypes.c_void_p * 1024)()
        needed = ctypes.c_ulong()
        assert psapi.EnumProcessModulesEx(process, modules, ctypes.sizeof(modules), ctypes.byref(needed), 3)
        for address in list(modules)[:needed.value // ctypes.sizeof(ctypes.c_void_p)]:
            path = ctypes.create_unicode_buffer(2048)
            psapi.GetModuleFileNameExW(process, address, path, 2048)
            name = pathlib.Path(path.value).name.lower()
            if name not in ('kenshilib.dll', 'myguiengine_x64.dll', 'ogremain_x64.dll'):
                continue
            header = pefile.PE(data=read(address, 4096), fast_load=True)
            export_rva = header.OPTIONAL_HEADER.DATA_DIRECTORY[0].VirtualAddress
            table = struct.unpack('<IIHHIIIIIII', read(address + export_rva, 40))
            name_rvas = struct.unpack('<' + 'I' * table[7], read(address + table[9], 4 * table[7]))
            live_exports[name] = {read(address + rva, 512).split(b'\0')[0] for rva in name_rvas}
    finally:
        kernel.CloseHandle(process)
checked = 0
upstream = pefile.PE(str(game.parents[1] / 'workshop/content/233860/3683557203/Organize-the-Trader.dll'))
upstream_imports = {d.dll.decode('ascii').lower(): {i.name for i in d.imports} for d in upstream.DIRECTORY_ENTRY_IMPORT}
external = []
for descriptor in dll.DIRECTORY_ENTRY_IMPORT:
    name = descriptor.dll.decode('ascii')
    if name.lower() not in ('kenshilib.dll', 'myguiengine_x64.dll', 'ogremain_x64.dll'):
        continue
    runtime = pefile.PE(str(game / name))
    exports = live_exports.get(name.lower(), {s.name for s in runtime.DIRECTORY_ENTRY_EXPORT.symbols})
    for entry in descriptor.imports:
        if entry.name not in exports:
            if live_exports:
                raise AssertionError((name, entry.name))
            assert entry.name in upstream_imports.get(name.lower(), set()), ('NEW missing import', name, entry.name)
            external.append((name, entry.name.decode('ascii')))
        checked += 1
if external:
    print('PASS: no new missing dependencies versus installed Workshop DLL;', checked, 'imports checked')
    print('These existing dependencies require runtime export augmentation (absent on disk):', external)
else:
    print('PASS:', checked, 'KenshiLib/MyGUI/Ogre imports present in', 'running game' if live_exports else 'disk DLLs')
