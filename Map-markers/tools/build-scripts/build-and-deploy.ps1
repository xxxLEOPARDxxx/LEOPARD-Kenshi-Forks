# Build + deploy orchestrator: runs build.ps1 then deploy.ps1.

param(
    [string]$ModName = "",
    [string]$ProjectFileName = "",
    [string]$OutputSubdir = "",
    [string]$DllName = "",
    [string]$ModFileName = "",
    [string]$ConfigFileName = "",
    [string]$KenshiPath = "",
    [string]$Configuration = "",
    [string]$Platform = "",
    [string]$PlatformToolset = ""
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$CommonScript = Join-Path $scriptDir "kenshi-common.ps1"
if (-not (Test-Path $CommonScript)) {
    Write-Host "ERROR: Missing shared helper: $CommonScript" -ForegroundColor Red
    exit 1
}
. $CommonScript

$buildScript = Join-Path $scriptDir "build.ps1"
$deployScript = Join-Path $scriptDir "deploy.ps1"

if (-not (Test-Path $buildScript)) {
    Write-Host "ERROR: build.ps1 not found at $buildScript" -ForegroundColor Red
    return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
}
if (-not (Test-Path $deployScript)) {
    Write-Host "ERROR: deploy.ps1 not found at $deployScript" -ForegroundColor Red
    return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
}

Write-Host "=== Kenshi Mod Build + Deploy ===" -ForegroundColor Cyan

$buildParams = Get-ForwardedParameters -BoundParameters $PSBoundParameters -AllowedKeys @(
    "ModName",
    "ProjectFileName",
    "OutputSubdir",
    "DllName",
    "ModFileName",
    "ConfigFileName",
    "Configuration",
    "Platform",
    "PlatformToolset"
)

Invoke-KenshiScriptWithSuppressedTimestamp { & $buildScript @buildParams }
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: build.ps1 failed (exit code $LASTEXITCODE)" -ForegroundColor Red
    return (Exit-KenshiScriptWithTimestamp -ExitCode $LASTEXITCODE)
}

$deployParams = Get-ForwardedParameters -BoundParameters $PSBoundParameters -AllowedKeys @(
    "ModName",
    "ProjectFileName",
    "OutputSubdir",
    "DllName",
    "ModFileName",
    "ConfigFileName",
    "KenshiPath",
    "Configuration",
    "Platform"
)

Invoke-KenshiScriptWithSuppressedTimestamp { & $deployScript @deployParams }
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: deploy.ps1 failed (exit code $LASTEXITCODE)" -ForegroundColor Red
    return (Exit-KenshiScriptWithTimestamp -ExitCode $LASTEXITCODE)
}

return (Exit-KenshiScriptWithTimestamp -ExitCode 0)
