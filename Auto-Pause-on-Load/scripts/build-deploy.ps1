# Build + deploy script shim for Kenshi mod plugin.
# This preserves the existing build-and-deploy.ps1 as the core implementation.

param(
    [string]$ModName = "",
    [string]$ProjectFileName = "",
    [string]$OutputSubdir = "",
    [string]$DllName = "",
    [string]$ModFileName = "",
    [string]$ConfigFileName = "RE_Kenshi.json",
    [string]$KenshiPath = "",
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
if (-not $KenshiPath -and $env:KENSHI_DEFAULT_PATH) {
    $KenshiPath = $env:KENSHI_DEFAULT_PATH
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
if (-not $ModName -and $env:KENSHI_MOD_NAME) {
    $ModName = $env:KENSHI_MOD_NAME
}
if (-not $ProjectFileName -and $env:KENSHI_PROJECT_FILE) {
    $ProjectFileName = $env:KENSHI_PROJECT_FILE
}
if (-not $OutputSubdir -and $env:KENSHI_OUTPUT_SUBDIR) {
    $OutputSubdir = $env:KENSHI_OUTPUT_SUBDIR
}
if (-not $DllName -and $env:KENSHI_DLL_NAME) {
    $DllName = $env:KENSHI_DLL_NAME
}
if (-not $ModFileName -and $env:KENSHI_MOD_FILE_NAME) {
    $ModFileName = $env:KENSHI_MOD_FILE_NAME
}
if (-not $ConfigFileName -and $env:KENSHI_CONFIG_FILE_NAME) {
    $ConfigFileName = $env:KENSHI_CONFIG_FILE_NAME
}

$BuildScript = Join-Path $ScriptDir "build-and-deploy.ps1"

if (-not (Test-Path $BuildScript)) {
    Write-Host "ERROR: build-and-deploy.ps1 not found at $BuildScript" -ForegroundColor Red
    exit 1
}

Write-Host "=== Kenshi Mod Build + Deploy ===" -ForegroundColor Cyan

& $BuildScript -ModName $ModName -ProjectFileName $ProjectFileName -OutputSubdir $OutputSubdir -DllName $DllName -ModFileName $ModFileName -ConfigFileName $ConfigFileName -KenshiPath $KenshiPath -Configuration $Configuration -Platform $Platform -PlatformToolset $PlatformToolset
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: build-and-deploy.ps1 failed" -ForegroundColor Red
    exit 1
}
