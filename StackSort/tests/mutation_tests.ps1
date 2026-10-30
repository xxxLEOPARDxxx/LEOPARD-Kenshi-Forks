# Compile the unchanged native adapter model against two deliberately broken
# production headers in an isolated build folder. Both must fail assertions.
$ErrorActionPreference = 'Stop'
$module = Split-Path $PSScriptRoot -Parent
$out = 'D:/DEV/Kenshi/build/StackSortMutationTests'
$vc = 'C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC'
$sdk = 'C:/Program Files/Microsoft SDKs/Windows/v7.1'
New-Item -ItemType Directory -Force $out | Out-Null
$env:INCLUDE = "$out;$vc/include;$sdk/Include;$module/src/common"
$env:LIB = "$vc/lib/amd64;$sdk/Lib/x64"
$env:PATH = "$vc/bin/amd64;$env:PATH"
$original = [IO.File]::ReadAllText("$module/src/common/NativeMergeAdapter.h")
$variants = @{
    doubled_count = $original.Replace('_NV_addItem(source, 1)', '_NV_addItem(source, source->quantity)')
    missing_recovery = $original.Replace('RestoreUncredited(remainder, originalUnits, full, credits)', 'true')
    missing_charge_capacity = $original.Replace('const int maxStack = target->isStackable(section);', 'if (target->quantity >= target->isStackable(section)) continue; const int maxStack = target->isStackable(section);')
}
foreach ($name in $variants.Keys) {
    if ($variants[$name] -eq $original) { throw "Mutation did not apply: $name" }
    [IO.File]::WriteAllText("$out/NativeMergeAdapter.h", $variants[$name])
    & "$vc/bin/amd64/cl.exe" /nologo /EHsc /MD /O2 /DWIN32 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS "/Fo$out/" "/Fe$out/$name.exe" "$PSScriptRoot/native_model_tests.cpp"
    if ($LASTEXITCODE) { throw "Mutant compile failed: $name" }
    & "$out/$name.exe"
    if ($LASTEXITCODE -eq 0) { throw "Mutant survived: $name" }
    Write-Host "Expected assertion failure verified: $name"
}
$planner = [IO.File]::ReadAllText("$module/src/common/ControlStripPlan.h")
$mutantPlanner = $planner.Replace('b<obstacles.size()', 'b<0')
if ($planner -eq $mutantPlanner) { throw 'Planner mutation did not apply' }
[IO.File]::WriteAllText("$out/ControlStripPlan.h", $mutantPlanner)
& "$vc/bin/amd64/cl.exe" /nologo /EHsc /MD /O2 /DWIN32 /DNDEBUG "/Fo$out/" "/Fe$out/ignored_neighbors.exe" "$PSScriptRoot/control_strip_tests.cpp"
if ($LASTEXITCODE) { throw 'Planner mutant did not compile' }
& "$out/ignored_neighbors.exe"
if ($LASTEXITCODE -eq 0) { throw 'Ignored-neighbor planner mutation survived' }
Write-Host 'Expected assertion failure verified: ignored_neighbors'
exit 0
