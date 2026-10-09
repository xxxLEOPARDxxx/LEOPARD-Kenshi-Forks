# Local wrapper: delegates to shared scripts submodule.
$ScriptDir = $PSScriptRoot
if (-not $ScriptDir -and $PSCommandPath) {
    $ScriptDir = Split-Path -Parent $PSCommandPath
}
if ($ScriptDir) {
    $RepoDir = Split-Path -Parent $ScriptDir
} else {
    $RepoDir = (Get-Location).Path
}
$env:KENSHI_REPO_DIR = $RepoDir
$SharedScript = Join-Path $RepoDir "tools\build-scripts\setup_env.ps1"

if (-not (Test-Path $SharedScript)) {
    Write-Host "ERROR: Shared script not found: $SharedScript" -ForegroundColor Red
    Write-Host "Run: git submodule update --init --recursive" -ForegroundColor Yellow
    return
}

. $SharedScript
