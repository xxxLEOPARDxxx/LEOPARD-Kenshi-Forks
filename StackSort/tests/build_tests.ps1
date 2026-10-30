$ErrorActionPreference = 'Stop'
$module = Split-Path $PSScriptRoot -Parent
$out = 'D:/DEV/Kenshi/build/StackSortTests'
$vc = 'C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC'
$sdk = 'C:/Program Files/Microsoft SDKs/Windows/v7.1'
New-Item -ItemType Directory -Force $out | Out-Null
$apply=[IO.File]::ReadAllText("$module/src/ui/StackSortUIApply.cpp")
$begin=$apply.IndexOf('static bool RevertSection(')
$finish='if (placements.empty()) return false;';$end=$apply.IndexOf($finish,$begin)+$finish.Length
[IO.File]::WriteAllText("$out/revert_match_under_test.h",$apply.Substring($begin,$end-$begin)+"`n return true; }`n")
$env:INCLUDE = "$vc/include;$sdk/Include;$module/src/common;$module/src/packer;$module/src/ui;$module/src/worker"
$env:LIB = "$vc/lib/amd64;$sdk/Lib/x64"
$env:PATH = "$vc/bin/amd64;$env:PATH"
$sources = @((Join-Path $PSScriptRoot 'core_tests.cpp'))
$sources += @(Get-ChildItem "$module/src/packer/*.cpp" | ForEach-Object { $_.FullName })
$sources += "$module/src/ui/TargetMemory.cpp"
$sources += "$module/src/worker/ResultCache.cpp"
& "$vc/bin/amd64/cl.exe" /nologo /EHsc /MD /O2 /DWIN32 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS "/I$out" "/Fo$out/" "/Fe$out/core_tests.exe" $sources
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& "$out/core_tests.exe"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& "$vc/bin/amd64/cl.exe" /nologo /EHsc /MD /O2 /DWIN32 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS "/Fo$out/" "/Fe$out/native_model_tests.exe" "$PSScriptRoot/native_model_tests.cpp"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& "$out/native_model_tests.exe"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& "$vc/bin/amd64/cl.exe" /nologo /EHsc /MD /O2 /DWIN32 /DNDEBUG "/Fo$out/" "/Fe$out/control_strip_tests.exe" "$PSScriptRoot/control_strip_tests.cpp"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& "$out/control_strip_tests.exe"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
$source = [IO.File]::ReadAllText("$module/src/plugin/StackSort.cpp")
$start = $source.IndexOf('static void hook_autoArrangeButton(')
$finish = $source.IndexOf('typedef void (*show_t)', $start)
if ($start -lt 0 -or $finish -lt $start) { throw 'Production click-hook boundary not found' }
[IO.File]::WriteAllText("$out/ui_click_hook_under_test.h", $source.Substring($start, $finish - $start))
& "$vc/bin/amd64/cl.exe" /nologo /EHsc /MD /O2 /DWIN32 /DNDEBUG "/I$out" "/Fo$out/" "/Fe$out/ui_click_tests.exe" "$PSScriptRoot/ui_click_tests.cpp"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& "$out/ui_click_tests.exe"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
$ui = [IO.File]::ReadAllText("$module/src/ui/StackSortUI.cpp")
$fragments = @(
    @('struct BackpackBoxLease', '// Removes our buttons from liveGui', 'ui_remove_under_test.h'),
    @('static ControlStrip::Rect BackpackGridAnchor(', '// Pre-sort setup for every sort click', 'ui_init_under_test.h'),
    @('void StackSortUI::OnClose(', 'void StackSortUI::OnDestroyed(', 'ui_close_under_test.h'),
    @('void StackSortUI::MaintainSlots(', 'void StackSortUI::OnUpdate(', 'ui_maintain_under_test.h')
)
foreach ($fragment in $fragments) {
    $begin = $ui.IndexOf($fragment[0]); $end = $ui.IndexOf($fragment[1], $begin)
    if ($begin -lt 0 -or $end -lt $begin) { throw "UI source boundary not found: $($fragment[2])" }
    [IO.File]::WriteAllText("$out/$($fragment[2])", $ui.Substring($begin, $end - $begin))
}
& "$vc/bin/amd64/cl.exe" /nologo /EHsc /MD /O2 /DWIN32 /DNDEBUG "/I$out" "/Fo$out/" "/Fe$out/ui_init_tests.exe" "$PSScriptRoot/ui_init_tests.cpp"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& "$out/ui_init_tests.exe"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
# Compile the real binder and the entry prefixes that must precede snapshots
# and first-click sorting. The rest of those native/worker functions is outside
# this deterministic DLL load-order model.
$begin=$source.IndexOf('#ifdef STACKSORT_ENABLE_ROTATION');$end=$source.IndexOf('#endif',$begin)
[IO.File]::WriteAllText("$out/rotation_types_under_test.h",$source.Substring($begin,$end-$begin)+"`n#endif`n")
$begin=$source.IndexOf('void StackSort_InitRotateAPI()');$end=$source.IndexOf('#endif',$begin)
[IO.File]::WriteAllText("$out/rotation_api_under_test.h",$source.Substring($begin,$end-$begin))
$worker=[IO.File]::ReadAllText("$module/src/worker/SortWorker.cpp")
$begin=$worker.IndexOf('void SortWorker::OnInventoryOpened(')
$finish='SnapshotSections(idx, gui, &needsCompute);';$end=$worker.IndexOf($finish,$begin)+$finish.Length
[IO.File]::WriteAllText("$out/rotation_open_under_test.h",$worker.Substring($begin,$end-$begin)+"`n }catch(...){} }`n")
$begin=$ui.IndexOf('void StackSortUI::FirstClickInit(');$end=$ui.IndexOf('if (!ui.stacksConsolidated)',$begin)
[IO.File]::WriteAllText("$out/rotation_click_under_test.h",$ui.Substring($begin,$end-$begin)+"`n}`n")
& "$vc/bin/amd64/cl.exe" /nologo /EHsc /MD /O2 /DWIN32 /DNDEBUG /DSTACKSORT_ENABLE_ROTATION "/I$out" "/Fo$out/" "/Fe$out/rotation_binding_tests.exe" "$PSScriptRoot/rotation_binding_tests.cpp"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& "$out/rotation_binding_tests.exe"
exit $LASTEXITCODE
