# Build + deploy script shim for Auto-Pause-on-Load mod.
# This preserves the existing build-and-deploy.ps1 as the core implementation.

param(
    [string]$KenshiPath = "H:\SteamLibrary\steamapps\common\Kenshi",
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$PlatformToolset = "v100"
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoDir = Split-Path -Parent $ScriptDir
$LoadEnv = Join-Path $ScriptDir "load-env.ps1"
if (Test-Path $LoadEnv) {
    . $LoadEnv -RepoDir $RepoDir
}

if (-not $PSBoundParameters.ContainsKey("KenshiPath") -and $env:KENSHI_PATH) {
    $KenshiPath = $env:KENSHI_PATH
}
if (-not $PSBoundParameters.ContainsKey("Configuration") -and $env:KENSHI_CONFIGURATION) {
    $Configuration = $env:KENSHI_CONFIGURATION
}
if (-not $PSBoundParameters.ContainsKey("Platform") -and $env:KENSHI_PLATFORM) {
    $Platform = $env:KENSHI_PLATFORM
}
if (-not $PSBoundParameters.ContainsKey("PlatformToolset") -and $env:KENSHI_PLATFORM_TOOLSET) {
    $PlatformToolset = $env:KENSHI_PLATFORM_TOOLSET
}

$BuildScript = Join-Path $ScriptDir "build-and-deploy.ps1"

if (-not (Test-Path $BuildScript)) {
    Write-Host "ERROR: build-and-deploy.ps1 not found at $BuildScript" -ForegroundColor Red
    exit 1
}

Write-Host "=== Auto-Pause-on-Load Build + Deploy ===" -ForegroundColor Cyan

& $BuildScript -KenshiPath $KenshiPath -Configuration $Configuration -Platform $Platform -PlatformToolset $PlatformToolset
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: build-and-deploy.ps1 failed" -ForegroundColor Red
    exit 1
}

