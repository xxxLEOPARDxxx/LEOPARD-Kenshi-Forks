# Local wrapper: delegates to shared build scripts.
param(
    [string]$ModName = "",
    [string]$KenshiPath = "",
    [string]$SourceModPath = "",
    [string]$DllName = "",
    [string]$ModFileName = "",
    [string]$ConfigFileName = "RE_Kenshi.json",
    [string]$OutDir = "",
    [string]$ZipName = "",
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"
$ScriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $PSCommandPath }
$RepoDir = if ($ScriptDir) { Split-Path -Parent $ScriptDir } else { (Get-Location).Path }
$env:KENSHI_REPO_DIR = $RepoDir
$SharedRoot = Join-Path $RepoDir "tools\build-scripts"
$SharedScript = Join-Path $SharedRoot "package.ps1"

if (-not (Test-Path $SharedScript)) {
    Write-Host "ERROR: Shared script not found: $SharedScript" -ForegroundColor Red
    Write-Host "Sync tools\build-scripts from the shared repo and retry." -ForegroundColor Yellow
    exit 1
}

$LoadEnvScript = Join-Path $SharedRoot "load-env.ps1"
if (Test-Path $LoadEnvScript) {
    . $LoadEnvScript -RepoDir $RepoDir
}

if (-not $ModName) { $ModName = "Organize-the-Crafting-Stations" }
if (-not $DllName) { $DllName = "Organize-the-Crafting-Stations.dll" }
if (-not $ModFileName) { $ModFileName = "Organize-the-Crafting-Stations.mod" }

$Forward = @{
    ModName = $ModName
    DllName = $DllName
    ModFileName = $ModFileName
    ConfigFileName = $ConfigFileName
}

foreach ($k in @('KenshiPath','SourceModPath','OutDir','ZipName','Version')) {
    if ($PSBoundParameters.ContainsKey($k)) { $Forward[$k] = (Get-Variable -Name $k -ValueOnly) }
}

& $SharedScript @Forward
exit $LASTEXITCODE
