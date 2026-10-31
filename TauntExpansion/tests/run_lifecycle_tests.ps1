$ErrorActionPreference='Stop'
$module=Split-Path $PSScriptRoot -Parent
$out='D:/DEV/Kenshi/build/TauntExpansionTests'
$vc='C:/Program Files (x86)/Microsoft Visual Studio 10.0/VC'
$sdk='C:/Program Files/Microsoft SDKs/Windows/v7.1'
New-Item -ItemType Directory -Force $out | Out-Null
Remove-Item -LiteralPath "$out/lifecycle_proof.json" -Force -ErrorAction SilentlyContinue
$env:INCLUDE="$vc/include;$sdk/Include"
$env:LIB="$vc/lib/amd64;$sdk/Lib/x64"
$env:PATH="$vc/bin/amd64;$env:PATH"
$source=[IO.File]::ReadAllText("$module/TauntExpansion.cpp")
$fragments=@(
    @('    TauntExpansion::BonusTransaction g_transaction;','    bool ReadBool(','lifecycle_state_under_test.h'),
    @('    bool ReadBool(','    struct TauntPatchState','lifecycle_config_under_test.h')
)
foreach($fragment in $fragments){
    $start=$source.IndexOf($fragment[0]);$end=$source.IndexOf($fragment[1],$start)
    if($start -lt 0 -or $end -lt $start){throw "Production lifecycle boundary missing: $($fragment[2])"}
    [IO.File]::WriteAllText("$out/$($fragment[2])",$source.Substring($start,$end-$start))
}
$start=$source.IndexOf('__declspec(dllexport) void startPlugin()')
$lineStart=$source.LastIndexOf("`n",$start)+1
if($start -lt 0){throw 'Production startPlugin boundary missing'}
[IO.File]::WriteAllText("$out/lifecycle_entries_under_test.h",$source.Substring($lineStart))
& "$vc/bin/amd64/cl.exe" /nologo /EHsc /MD /O2 "/ID:/DEV/Kenshi/shared" "/I$out" "/Fo$out/" "/Fe$out/lifecycle_tests.exe" "$PSScriptRoot/lifecycle_tests.cpp"
if($LASTEXITCODE){exit $LASTEXITCODE}
& "$out/lifecycle_tests.exe" early
if($LASTEXITCODE){exit $LASTEXITCODE}
& "$out/lifecycle_tests.exe"
if($LASTEXITCODE){exit $LASTEXITCODE}
$proof=[ordered]@{
    production_source_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath "$module/TauntExpansion.cpp").Hash.ToLowerInvariant()
    transaction_header_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath "$module/BonusTransaction.h").Hash.ToLowerInvariant()
    fixture_exe_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath "$out/lifecycle_tests.exe").Hash.ToLowerInvariant()
    loader_entry='?startPlugin@@YAXXZ'
    actual_extracted_startup_invoked_via_GetProcAddress=$true
    actual_extracted_LoadConfig_callback=$true
    early_describe_and_apply_before_startup=$true
    normal_startup_before_describe_and_apply=$true
    duplicate_startup_safe=$true
    target_patch_and_hook_failure_callbacks_safe=$true
    limitation='Game API dependencies are fixtures; live game startup and target behavior require separate acceptance.'
}
$proof | ConvertTo-Json | Set-Content -LiteralPath "$out/lifecycle_proof.json" -Encoding UTF8
exit 0
