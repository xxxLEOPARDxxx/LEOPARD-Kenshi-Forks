$ErrorActionPreference='Stop'
$module=Split-Path $PSScriptRoot -Parent
$out='D:/DEV/Kenshi/build/TauntExpansionMutation'
$vc='C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC'
$sdk='C:/Program Files/Microsoft SDKs/Windows/v7.1'
New-Item -ItemType Directory -Force "$out/tests" | Out-Null
$env:INCLUDE="$vc/include;$sdk/Include"
$env:LIB="$vc/lib/amd64;$sdk/Lib/x64"
$env:PATH="$vc/bin/amd64;$env:PATH"
Copy-Item -LiteralPath "$module/TauntPolicy.h","$module/PatchSignature.h" -Destination $out
Copy-Item -LiteralPath "$PSScriptRoot/core_tests.cpp" -Destination "$out/tests/core_tests.cpp"
$original=[IO.File]::ReadAllText("$module/BonusTransaction.h")
$changed=$original.Replace('if (address) *address=previous;','if (address) *address=2.0f;')
if($changed -eq $original){throw 'Production restoration boundary missing'}
[IO.File]::WriteAllText("$out/BonusTransaction.h",$changed)
& "$vc/bin/amd64/cl.exe" /nologo /EHsc /MD /O2 /D_CRT_SECURE_NO_WARNINGS "/Fo$out/" "/Fe$out/mutant.exe" "$out/tests/core_tests.cpp"
if($LASTEXITCODE){throw 'Mutation must compile before checking regression detection'}
$result=& "$out/mutant.exe" 2>&1
if($LASTEXITCODE -ne 1 -or $result -notmatch 'active == 10.0f'){throw "Regression mutation not detected as expected: $result"}
Write-Host "PASS: nested-bonus corruption mutation rejected: $result"
exit 0
