# Package-only script for Auto-Pause-on-Load mod.

param(
    [string]$KenshiPath = "H:\SteamLibrary\steamapps\common\Kenshi",
    [string]$OutDir = "",
    [string]$ZipName = "",
    [string]$Version = ""
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

$VersionFile = Join-Path $RepoDir "VERSION"

$KenshiModPath = Join-Path $KenshiPath "mods\Auto-Pause-on-Load"
if (-not (Test-Path $KenshiModPath)) {
    Write-Host "ERROR: Mod folder not found: $KenshiModPath" -ForegroundColor Red
    exit 1
}

$RequiredFiles = @(
    "Auto-Pause-on-Load.dll",
    "RE_Kenshi.json",
    "Auto-Pause-on-Load.mod"
)

foreach ($f in $RequiredFiles) {
    $p = Join-Path $KenshiModPath $f
    if (-not (Test-Path $p)) {
        Write-Host "ERROR: Missing required file in mod folder: $p" -ForegroundColor Red
        exit 1
    }
}

if (-not $OutDir) {
    $OutDir = Join-Path $RepoDir "dist"
}

if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
}

if (-not $Version) {
    if (Test-Path $VersionFile) {
        $Version = (Get-Content -Path $VersionFile | Select-Object -First 1).Trim()
    }
}

if (-not $ZipName) {
    if ($Version) {
        $ZipName = "Auto-Pause-on-Load-$Version.zip"
    } else {
        $ZipName = "Auto-Pause-on-Load-alpha.zip"
    }
}

$ZipPath = Join-Path $OutDir $ZipName
if (Test-Path $ZipPath) {
    Remove-Item -Path $ZipPath -Force
}

Write-Host "Packaging: $KenshiModPath" -ForegroundColor Yellow
Write-Host "Output:    $ZipPath" -ForegroundColor Gray

Compress-Archive -Path $KenshiModPath -DestinationPath $ZipPath

Write-Host "Package created: $ZipPath" -ForegroundColor Green

