$ErrorActionPreference = 'Stop'
$module = Split-Path $PSScriptRoot -Parent
$out = 'D:/DEV/Kenshi/build/StackSortTests'
$vc = 'C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC'
$sdk = 'C:/Program Files/Microsoft SDKs/Windows/v7.1'
New-Item -ItemType Directory -Force $out | Out-Null
$env:INCLUDE = "$vc/include;$sdk/Include;$module/src/common;$module/src/packer;$module/src/ui;$module/src/worker"
$env:LIB = "$vc/lib/amd64;$sdk/Lib/x64"
$env:PATH = "$vc/bin/amd64;$env:PATH"
$sources = @((Join-Path $PSScriptRoot 'core_tests.cpp'))
$sources += @(Get-ChildItem "$module/src/packer/*.cpp" | ForEach-Object { $_.FullName })
$sources += "$module/src/ui/TargetMemory.cpp"
$sources += "$module/src/worker/ResultCache.cpp"
& "$vc/bin/amd64/cl.exe" /nologo /EHsc /MD /O2 /DWIN32 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS "/Fo$out/" "/Fe$out/core_tests.exe" $sources
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& "$out/core_tests.exe"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& "$vc/bin/amd64/cl.exe" /nologo /EHsc /MD /O2 /DWIN32 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS "/Fo$out/" "/Fe$out/native_model_tests.exe" "$PSScriptRoot/native_model_tests.cpp"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& "$out/native_model_tests.exe"
exit $LASTEXITCODE
