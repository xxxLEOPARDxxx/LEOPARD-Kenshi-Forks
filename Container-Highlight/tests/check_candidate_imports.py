"""Check candidate named imports against the installed game and system DLLs."""
from pathlib import Path
import re
import subprocess
import sys

DUMP = Path(r'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.38.33130/bin/Hostx64/x64/dumpbin.exe')
GAME = Path(r'D:/SteamLibrary/steamapps/common/Kenshi')
DLL = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(r'D:/DEV/Kenshi/build/Container-Highlight/Container-Highlight.dll')


def dump(option, path):
    return subprocess.check_output([str(DUMP), option, str(path)], text=True, errors='replace')


imports = {}
current = None
for line in dump('/imports', DLL).splitlines():
    module = re.match(r'^    (\S+\.dll)\s*$', line, re.I)
    if module:
        current = module[1]
        imports[current] = []
        continue
    symbol = re.match(r'^\s{20,}[0-9A-F]+ (\S+)\s*$', line)
    if current and symbol:
        imports[current].append(symbol[1])

count = 0
missing = []
for module, symbols in imports.items():
    path = GAME / module
    if not path.exists():
        path = Path(r'C:/Windows/System32') / module
    exports = dump('/exports', path)
    names = set(re.findall(r'^\s+\d+\s+[0-9A-F]+\s+(?:[0-9A-F]+\s+)?(\S+)', exports, re.M))
    for symbol in symbols:
        count += 1
        if symbol not in names:
            missing.append((module, symbol))

for module, symbol in missing:
    print('MISSING:', module, symbol)
print(f'{count} named imports, {len(imports)} modules, {len(missing)} missing')
if count == 0:
    raise RuntimeError('No imports parsed')
raise SystemExit(bool(missing))
