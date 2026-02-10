# Build, deploy, and optionally package script for Auto-Pause-on-Load mod.
# Deprecated: prefer build-deploy.ps1 and package.ps1.

param(
    [string]$KenshiPath = "H:\SteamLibrary\steamapps\common\Kenshi",
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$PlatformToolset = "v100",
    [string]$OutDir = "",
    [string]$ZipName = "",
    [string]$Version = "",
    [switch]$Package
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

$BuildScript = Join-Path $ScriptDir "build-deploy.ps1"
$PackageScript = Join-Path $ScriptDir "package.ps1"

if (-not (Test-Path $BuildScript)) {
    Write-Host "ERROR: build-deploy.ps1 not found at $BuildScript" -ForegroundColor Red
    exit 1
}

Write-Host "=== Auto-Pause-on-Load Build + Package (legacy) ===" -ForegroundColor Cyan

& $BuildScript -KenshiPath $KenshiPath -Configuration $Configuration -Platform $Platform -PlatformToolset $PlatformToolset
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: build-deploy.ps1 failed" -ForegroundColor Red
    exit 1
}

if (-not $Package) {
    Write-Host "Package step skipped (use -Package to create a zip)." -ForegroundColor Gray
    exit 0
}

if (-not $OutDir) {
    $OutDir = Join-Path $RepoDir "dist"
}

if (-not (Test-Path $PackageScript)) {
    Write-Host "ERROR: package.ps1 not found at $PackageScript" -ForegroundColor Red
    exit 1
}

& $PackageScript -KenshiPath $KenshiPath -OutDir $OutDir -ZipName $ZipName -Version $Version

