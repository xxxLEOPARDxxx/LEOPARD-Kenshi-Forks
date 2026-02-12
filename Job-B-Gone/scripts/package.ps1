# Package-only script for Auto-Pause-on-Load.

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

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoDir = Split-Path -Parent $ScriptDir
$LoadEnv = Join-Path $ScriptDir "load-env.ps1"
if (Test-Path $LoadEnv) {
    . $LoadEnv -RepoDir $RepoDir
}

if (-not $PSBoundParameters.ContainsKey("KenshiPath") -and $env:KENSHI_PATH) {
    $KenshiPath = $env:KENSHI_PATH
}
if (-not $KenshiPath -and $env:KENSHI_DEFAULT_PATH) {
    $KenshiPath = $env:KENSHI_DEFAULT_PATH
}
if (-not $ModName) {
    if ($env:KENSHI_MOD_NAME) {
        $ModName = $env:KENSHI_MOD_NAME
    } else {
        $ModName = Split-Path -Leaf $RepoDir
    }
}
if (-not $DllName) {
    if ($env:KENSHI_DLL_NAME) {
        $DllName = $env:KENSHI_DLL_NAME
    } else {
        $DllName = "$ModName.dll"
    }
}
if (-not $ModFileName) {
    if ($env:KENSHI_MOD_FILE_NAME) {
        $ModFileName = $env:KENSHI_MOD_FILE_NAME
    } else {
        $ModFileName = "$ModName.mod"
    }
}
if (-not $ConfigFileName -and $env:KENSHI_CONFIG_FILE_NAME) {
    $ConfigFileName = $env:KENSHI_CONFIG_FILE_NAME
}

$VersionFile = Join-Path $RepoDir "VERSION"

$PackageSourcePath = $SourceModPath
if (-not $PackageSourcePath) {
    if (-not $KenshiPath) {
        Write-Host "ERROR: Kenshi path is not set. Provide -SourceModPath or set KENSHI_PATH in .env." -ForegroundColor Red
        exit 1
    }
    $PackageSourcePath = Join-Path $KenshiPath "mods\$ModName"
}

if (-not (Test-Path $PackageSourcePath)) {
    Write-Host "ERROR: Mod folder not found: $PackageSourcePath" -ForegroundColor Red
    exit 1
}

$RequiredFiles = @(
    $DllName,
    $ConfigFileName,
    $ModFileName
)

foreach ($f in $RequiredFiles) {
    $p = Join-Path $PackageSourcePath $f
    if (-not (Test-Path $p)) {
        Write-Host "ERROR: Missing required file in package source: $p" -ForegroundColor Red
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
        $ZipName = "$ModName-$Version.zip"
    } else {
        $ZipName = "$ModName-alpha.zip"
    }
}

$ZipPath = Join-Path $OutDir $ZipName
if (Test-Path $ZipPath) {
    Remove-Item -Path $ZipPath -Force
}

Write-Host "Packaging: $PackageSourcePath" -ForegroundColor Yellow
Write-Host "Output:    $ZipPath" -ForegroundColor Gray

Compress-Archive -Path $PackageSourcePath -DestinationPath $ZipPath

Write-Host "Package created: $ZipPath" -ForegroundColor Green
