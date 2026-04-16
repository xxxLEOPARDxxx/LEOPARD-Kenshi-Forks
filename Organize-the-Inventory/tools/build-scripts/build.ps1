# Build-only script for Kenshi mod plugins.

param(
    [string]$ModName = "",
    [string]$ProjectFileName = "",
    [string]$OutputSubdir = "",
    [string]$DllName = "",
    [string]$ModFileName = "",
    [string]$ConfigFileName = "",
    [string]$Configuration = "",
    [string]$Platform = "",
    [string]$PlatformToolset = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$CommonScript = Join-Path $scriptDir "kenshi-common.ps1"
if (-not (Test-Path $CommonScript)) {
    Write-Host "ERROR: Missing shared helper: $CommonScript" -ForegroundColor Red
    exit 1
}
. $CommonScript
Initialize-KenshiScriptTiming

$ctx = Initialize-KenshiScriptContext -InvocationPath $MyInvocation.MyCommand.Path
$resolved = Resolve-KenshiBuildContext -BoundParameters $PSBoundParameters -RepoDir $ctx.RepoDir -ModName $ModName -ProjectFileName $ProjectFileName -OutputSubdir $OutputSubdir -DllName $DllName -ModFileName $ModFileName -ConfigFileName $ConfigFileName -Configuration $Configuration -Platform $Platform -PlatformToolset $PlatformToolset

Write-Host "=== $($resolved.ModName) Build ===" -ForegroundColor Cyan
Write-Host "Project: $($resolved.ProjectFile)" -ForegroundColor Gray
Write-Host "Output:  $($resolved.OutputDir)" -ForegroundColor Gray

if (-not (Test-Path $resolved.ProjectFile)) {
    Write-Host "ERROR: Project file not found: $($resolved.ProjectFile)" -ForegroundColor Red
    return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
}

try {
    Ensure-KenshiBuildEnvironment -ScriptDir $ctx.ScriptDir
} catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
    return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
}

try {
    Invoke-KenshiBuild -ProjectFile $resolved.ProjectFile -Configuration $resolved.Configuration -Platform $resolved.Platform -PlatformToolset $resolved.PlatformToolset -OutputDir $resolved.OutputDir -Clean:$Clean
    Write-Host "Build succeeded!" -ForegroundColor Green
} catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
    return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
}

if (-not (Test-Path $resolved.DllPath)) {
    Write-Host "ERROR: DLL not found after build: $($resolved.DllPath)" -ForegroundColor Red
    return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
}

Write-Host "Built DLL: $($resolved.DllPath)" -ForegroundColor Gray
return (Exit-KenshiScriptWithTimestamp -ExitCode 0)
