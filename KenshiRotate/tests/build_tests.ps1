$ErrorActionPreference='Stop'
$module=Split-Path $PSScriptRoot -Parent
$out='D:/DEV/Kenshi/build/KenshiRotateDiagnosticsTests'
$vc='C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC'
$sdk='C:/Program Files/Microsoft SDKs/Windows/v7.1'
New-Item -ItemType Directory -Force $out | Out-Null
$env:INCLUDE="$vc/include;$sdk/Include;$module/src;$out"
$env:LIB="$vc/lib/amd64;$sdk/Lib/x64"
$env:PATH="$vc/bin/amd64;$env:PATH"
$source=[IO.File]::ReadAllText("$module/src/KenshiRotate.cpp")
foreach($part in @(@('bool canStackWith_hook(','// Defense-in-depth','pair_hook_under_test.h'),@('bool placeItemFromMouse_hook(','// Kenshi''s RClickAutoTrade','place_hook_under_test.h'))){
 $begin=$source.IndexOf($part[0]);$end=$source.IndexOf($part[1],$begin)
 if($begin -lt 0 -or $end -lt $begin){throw 'Production boundary missing'}
 [IO.File]::WriteAllText("$out/$($part[2])",$source.Substring($begin,$end-$begin))
}
& "$vc/bin/amd64/cl.exe" /nologo /EHsc /MD /O2 /DWIN32 /DNDEBUG "/Fo$out/" "/Fe$out/hook_tests.exe" "$PSScriptRoot/hook_tests.cpp"
if($LASTEXITCODE){exit $LASTEXITCODE}
& "$out/hook_tests.exe"
if($LASTEXITCODE){exit $LASTEXITCODE}
$diagnostics=[IO.File]::ReadAllText("$module/src/PairDiagnostics.cpp")
foreach($part in @(@('PairTrace::Context s_pairPlaceContext;','class DiagnosticItemAccess','pair_thread_under_test.h'),@('static PairTrace::Key Hash(','static const char* Sid(','pair_hash_under_test.h'),@('void RecordStackPair(', $null,'pair_record_under_test.h'))){
 $begin=$diagnostics.IndexOf($part[0]);$end=if($part[1]){$diagnostics.IndexOf($part[1],$begin)}else{$diagnostics.Length}
 [IO.File]::WriteAllText("$out/$($part[2])",$diagnostics.Substring($begin,$end-$begin))
}
& "$vc/bin/amd64/cl.exe" /nologo /EHsc /MD /O2 /DWIN32 /DNDEBUG "/Fo$out/" "/Fe$out/record_tests.exe" "$PSScriptRoot/record_tests.cpp"
if($LASTEXITCODE){exit $LASTEXITCODE}
& "$out/record_tests.exe"
exit $LASTEXITCODE
