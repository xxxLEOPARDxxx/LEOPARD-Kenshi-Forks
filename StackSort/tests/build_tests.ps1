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
    @('static bool RemoveOurButtons(', '// Removes our buttons from liveGui', 'ui_remove_under_test.h'),
    @('void StackSortUI::InitializeUI(', '// Pre-sort setup for every sort click', 'ui_init_under_test.h'),
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
exit $LASTEXITCODE
