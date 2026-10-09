# Local wrapper: delegates to shared build scripts.
param(
    [string]$RepoDir
)

$ErrorActionPreference = "Stop"
$ScriptDir = $PSScriptRoot
if (-not $ScriptDir -and $PSCommandPath) {
    $ScriptDir = Split-Path -Parent $PSCommandPath
}
if (-not $RepoDir) {
    if ($ScriptDir) {
        $RepoDir = Split-Path -Parent $ScriptDir
    } else {
        $RepoDir = (Get-Location).Path
    }
}
$env:KENSHI_REPO_DIR = $RepoDir
$SharedScript = Join-Path $RepoDir "tools\build-scripts\load-env.ps1"

if (-not (Test-Path $SharedScript)) {
    Write-Host "ERROR: Shared script not found: $SharedScript" -ForegroundColor Red
    Write-Host "Sync tools\build-scripts from the shared repo and retry." -ForegroundColor Yellow
    return
}

. $SharedScript -RepoDir $RepoDir
