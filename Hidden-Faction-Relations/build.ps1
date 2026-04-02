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

function Ensure-BoostImportLibOverride {
    param(
        [Parameter(Mandatory = $true)][string]$RepoDir
    )

    if (-not $env:BOOST_INCLUDE_PATH) {
        return
    }

    $boostStageLibDir = Join-Path $env:BOOST_INCLUDE_PATH "stage\lib"
    if (-not (Test-Path $boostStageLibDir)) {
        return
    }

    $aliases = @(
        @{ Source = "boost_system-vc100-mt-1_60.lib"; Alias = "libboost_system-vc100-mt-1_60.lib" },
        @{ Source = "boost_thread-vc100-mt-1_60.lib"; Alias = "libboost_thread-vc100-mt-1_60.lib" }
    )

    foreach ($entry in $aliases) {
        if (-not (Test-Path (Join-Path $boostStageLibDir $entry.Source))) {
            return
        }
    }

    $overrideDir = Join-Path $RepoDir ".boost-link-override"
    New-Item -ItemType Directory -Path $overrideDir -Force | Out-Null

    foreach ($entry in $aliases) {
        Copy-Item -Path (Join-Path $boostStageLibDir $entry.Source) -Destination (Join-Path $overrideDir $entry.Alias) -Force
    }

    $env:BOOST_LINK_OVERRIDE_PATH = $overrideDir
    $env:BOOST_RUNTIME_DLL_SOURCE_DIR = $boostStageLibDir
    Write-Host "Prepared Boost link override: $overrideDir" -ForegroundColor Gray
}

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

Ensure-BoostImportLibOverride -RepoDir $ctx.RepoDir

try {
    Invoke-KenshiBuild -ProjectFile $resolved.ProjectFile -Configuration $resolved.Configuration -Platform $resolved.Platform -PlatformToolset $resolved.PlatformToolset -Clean:$Clean
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
