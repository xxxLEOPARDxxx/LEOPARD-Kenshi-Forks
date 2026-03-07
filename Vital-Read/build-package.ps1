# Backward-compatible shim: delegates to build-and-package.ps1.

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$CommonScript = Join-Path $scriptDir "kenshi-common.ps1"
if (-not (Test-Path $CommonScript)) {
    Write-Host "ERROR: Missing shared helper: $CommonScript" -ForegroundColor Red
    exit 1
}
. $CommonScript

Invoke-ScriptShim -ScriptDir $scriptDir -TargetScript "build-and-package.ps1" -Arguments $args
