# Local wrapper: delegates to shared build scripts.
param(
    [string]$RepoDir = "",
    [string]$ModName = "",
    [string]$DllName = "",
    [string]$ModFileName = "",
    [string]$ConfigFileName = "RE_Kenshi.json"
)

$ErrorActionPreference = "Stop"
$ScriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $PSCommandPath }
$LocalRepoDir = if ($ScriptDir) { Split-Path -Parent $ScriptDir } else { (Get-Location).Path }
if (-not $RepoDir) { $RepoDir = $LocalRepoDir }
$env:KENSHI_REPO_DIR = $RepoDir
$SharedScript = Join-Path $LocalRepoDir "tools\build-scripts\init-mod-template.ps1"

if (-not (Test-Path $SharedScript)) {
    Write-Host "ERROR: Shared script not found: $SharedScript" -ForegroundColor Red
    Write-Host "Sync tools\build-scripts from the shared repo and retry." -ForegroundColor Yellow
    exit 1
}

if (-not $ModName) { $ModName = "Organize-the-Crafting-Stations" }
if (-not $DllName) { $DllName = "Organize-the-Crafting-Stations.dll" }
if (-not $ModFileName) { $ModFileName = "Organize-the-Crafting-Stations.mod" }

$Forward = @{
    RepoDir = $RepoDir
    ModName = $ModName
    DllName = $DllName
    ModFileName = $ModFileName
    ConfigFileName = $ConfigFileName
}

& $SharedScript @Forward
exit $LASTEXITCODE
