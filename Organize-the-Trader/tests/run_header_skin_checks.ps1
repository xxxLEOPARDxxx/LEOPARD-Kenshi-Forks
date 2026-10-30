$ErrorActionPreference='Stop'
$module=Split-Path $PSScriptRoot -Parent
$out='D:/DEV/Kenshi/build/Organize-the-TraderOfflineTests'
$vc='C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC'
$sdk='C:/Program Files/Microsoft SDKs/Windows/v7.1'
$env:INCLUDE="$vc/include;$sdk/Include;$out"
$env:LIB="$vc/lib/amd64;$sdk/Lib/x64"
New-Item -ItemType Directory -Force $out | Out-Null
& C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe "$PSScriptRoot/header_skin_fixtures.py"
if($LASTEXITCODE){exit $LASTEXITCODE}
$source=[IO.File]::ReadAllText("$module/src/TraderSearchUi.cpp")
$start=$source.IndexOf('void DestroyTradeBalance();');$end=$source.IndexOf('// The drop-down list',$start)
if($start -lt 0 -or $end -lt $start){throw 'Production header boundaries changed'}
[IO.File]::WriteAllText("$out/HeaderProductionUnderTest.h",$source.Substring($start,$end-$start))
& "$vc/bin/amd64/cl.exe" /nologo /EHsc /MD "/Fo$out/" "/Fe$out/HeaderSkinTests.exe" "$PSScriptRoot/HeaderSkinTests.cpp"
if($LASTEXITCODE){exit $LASTEXITCODE}
& "$out/HeaderSkinTests.exe" "$out/header-skin-fixtures.txt"
exit $LASTEXITCODE
