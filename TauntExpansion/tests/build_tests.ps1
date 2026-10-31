$ErrorActionPreference='Stop'
$module=Split-Path $PSScriptRoot -Parent
$out='D:/DEV/Kenshi/build/TauntExpansionTests'
$vc='C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC'
$sdk='C:/Program Files/Microsoft SDKs/Windows/v7.1'
New-Item -ItemType Directory -Force $out | Out-Null
$env:INCLUDE="$vc/include;$sdk/Include"
$env:LIB="$vc/lib/amd64;$sdk/Lib/x64"
$env:PATH="$vc/bin/amd64;$env:PATH"
& "$vc/bin/amd64/cl.exe" /nologo /EHsc /MD /O2 /D_CRT_SECURE_NO_WARNINGS "/Fo$out/" "/Fe$out/core_tests.exe" "$PSScriptRoot/core_tests.cpp"
if($LASTEXITCODE){exit $LASTEXITCODE}
& "$out/core_tests.exe"
if($LASTEXITCODE){exit $LASTEXITCODE}
$source=[IO.File]::ReadAllText("$module/TauntExpansion.cpp")
$start=$source.IndexOf('extern "C" __declspec(dllexport) void MCM_Describe(')
if($start -lt 0){throw 'Production MCM_Describe boundary missing'}
[IO.File]::WriteAllText("$out/mcm_under_test.h",$source.Substring($start))
& "$vc/bin/amd64/cl.exe" /nologo /EHsc /MD /O2 "/ID:/DEV/Kenshi/shared" "/I$out" "/Fo$out/" "/Fe$out/mcm_contract_tests.exe" "$PSScriptRoot/mcm_contract_tests.cpp"
if($LASTEXITCODE){exit $LASTEXITCODE}
& "$out/mcm_contract_tests.exe"
if($LASTEXITCODE){exit $LASTEXITCODE}
$begin=$source.IndexOf('    struct SelectionCall {')
$end=$source.IndexOf('    bool InvokeSelection(',$begin)
if($begin -lt 0 -or $end -lt $begin){throw 'Production RunSelection boundaries missing'}
[IO.File]::WriteAllText("$out/selection_under_test.h",$source.Substring($begin,$end-$begin))
& "$vc/bin/amd64/cl.exe" /nologo /EHsc /MD /O2 "/I$out" "/Fo$out/" "/Fe$out/selection_tests.exe" "$PSScriptRoot/selection_tests.cpp"
if($LASTEXITCODE){exit $LASTEXITCODE}
& "$out/selection_tests.exe"
exit $LASTEXITCODE
