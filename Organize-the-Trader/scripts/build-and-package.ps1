# Local wrapper: delegates to shared build scripts.
param(
    [string]$ModName = "",
    [string]$ProjectFileName = "",
    [string]$OutputSubdir = "",
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$PlatformToolset = "v100",
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
$SharedScript = Join-Path $SharedRoot "build-and-package.ps1"

if (-not (Test-Path $SharedScript)) {
    Write-Host "ERROR: Shared script not found: $SharedScript" -ForegroundColor Red
    Write-Host "Sync tools\build-scripts from the shared repo and retry." -ForegroundColor Yellow
    exit 1
}

$LoadEnvScript = Join-Path $SharedRoot "load-env.ps1"
if (Test-Path $LoadEnvScript) {
    . $LoadEnvScript -RepoDir $RepoDir
}

$Forward = @{}
foreach ($k in @('ModName','ProjectFileName','OutputSubdir','Configuration','Platform','PlatformToolset','DllName','ModFileName','ConfigFileName','OutDir','ZipName','Version')) {
    if ($PSBoundParameters.ContainsKey($k)) { $Forward[$k] = (Get-Variable -Name $k -ValueOnly) }
}

& $SharedScript @Forward
exit $LASTEXITCODE
