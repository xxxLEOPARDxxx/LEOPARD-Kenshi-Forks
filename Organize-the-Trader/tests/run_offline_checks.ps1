$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$out = 'D:/DEV/Kenshi/build/Organize-the-Trader'
$python = 'C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
$vc = 'C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC'
$sdk = 'C:/Program Files/Microsoft SDKs/Windows/v7.1'
$env:INCLUDE = "$vc/include;$sdk/Include"
$env:LIB = "$vc/lib/amd64;$sdk/Lib/x64"
New-Item -ItemType Directory -Force $out | Out-Null
function RunCheck([string]$path) {
    & $python $path
    if ($LASTEXITCODE) { throw "Failed: $path" }
}
function CompileTest([string]$name, [string[]]$sources) {
    & "$vc/bin/amd64/cl.exe" /nologo /EHsc /MD "/Fe$out/$name.exe" "/Fo$out/" $sources
    if ($LASTEXITCODE) { throw "Compile failed: $name" }
}
RunCheck "$PSScriptRoot/CheckAdaptiveUi.py"
RunCheck "$PSScriptRoot/CheckFilterCoordinates.py"
CompileTest 'AdaptiveLayoutTests' @("$PSScriptRoot/AdaptiveLayoutTests.cpp")
& "$out/AdaptiveLayoutTests.exe"
if ($LASTEXITCODE) { throw 'Adaptive layout regression' }
CompileTest 'RenderedTextTests' @("$PSScriptRoot/RenderedTextTests.cpp")
& "$out/RenderedTextTests.exe"
if ($LASTEXITCODE) { throw 'Rendered client text/measurement cache regression' }
@'
from pathlib import Path
import xml.etree.ElementTree as ET
target = Path('D:/DEV/Kenshi/build/Organize-the-Trader/installed-layout-fixtures.txt')
with target.open('w', encoding='utf-8') as out:
    for path in sorted(Path('D:/SteamLibrary/steamapps/common/Kenshi/data/gui/layout').glob('Kenshi_Inventory*.layout')):
        out.write(path.name + '\n')
        for node in ET.parse(path).getroot().iter('Widget'):
            if node.get('name'):
                out.write(node.get('type', 'Widget') + ' ' + node.get('name') + '\n')
        out.write('END\n')
'@ | & $python -
if ($LASTEXITCODE) { throw 'Layout fixture generation' }
CompileTest 'LayoutRecognitionTests' @("$PSScriptRoot/LayoutRecognitionTests.cpp", "$taskRoot/src/TraderTextUnicode.cpp")
& "$out/LayoutRecognitionTests.exe" "$out/installed-layout-fixtures.txt"
if ($LASTEXITCODE) { throw 'Layout recognition/packing regression' }
RunCheck "$PSScriptRoot/CheckNativeContract.py"
