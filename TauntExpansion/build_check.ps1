$ErrorActionPreference='Stop'
$module=$PSScriptRoot
$python='C:/Users/User/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
$out='D:/DEV/Kenshi/build/TauntExpansion'
& 'D:/DEV/Kenshi/ours/build.ps1' -SrcDir $module -Name TauntExpansion
if($LASTEXITCODE){exit $LASTEXITCODE}
& "$module/tests/build_tests.ps1"
if($LASTEXITCODE){exit $LASTEXITCODE}
& $python "$module/tests/compile_locales.py" --output $out
if($LASTEXITCODE){exit $LASTEXITCODE}
& $python "$module/tests/audit_binary.py" --output "$out/binary_audit.json"
if($LASTEXITCODE){exit $LASTEXITCODE}
Copy-Item -LiteralPath "$out/binary_audit.json" -Destination "$module/release/binary_audit.json"
Copy-Item -LiteralPath 'D:/DEV/Kenshi/build/TauntExpansionTests/lifecycle_proof.json' -Destination "$module/release/lifecycle_proof.json"
& $python "$module/tests/prepare_metadata.py"
exit $LASTEXITCODE
