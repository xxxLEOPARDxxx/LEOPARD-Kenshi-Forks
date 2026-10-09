# Build + package orchestrator: runs build.ps1, stages, then package.ps1.

param(
    [string]$ModName = "",
    [string]$ProjectFileName = "",
    [string]$OutputSubdir = "",
    [string]$Configuration = "",
    [string]$Platform = "",
    [string]$PlatformToolset = "",
    [switch]$Clean,
    [string]$DllName = "",
    [string]$ModFileName = "",
    [string]$ConfigFileName = "",
    [string]$OutDir = "",
    [string]$ZipName = "",
    [string]$Version = ""
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

$buildScript = Join-Path $scriptDir "build.ps1"
$packageScript = Join-Path $scriptDir "package.ps1"
$initTemplateScript = Join-Path $scriptDir "init-mod-template.ps1"

if (-not (Test-Path $buildScript)) {
    Write-Host "ERROR: build.ps1 not found at $buildScript" -ForegroundColor Red
    return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
}
if (-not (Test-Path $packageScript)) {
    Write-Host "ERROR: package.ps1 not found at $packageScript" -ForegroundColor Red
    return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
}

Write-Host "=== Kenshi Mod Build + Package ===" -ForegroundColor Cyan

$buildParams = Get-ForwardedParameters -BoundParameters $PSBoundParameters -AllowedKeys @(
    "ModName",
    "ProjectFileName",
    "OutputSubdir",
    "DllName",
    "ModFileName",
    "ConfigFileName",
    "Configuration",
    "Platform",
    "PlatformToolset",
    "Clean"
)

Invoke-KenshiScriptWithSuppressedTimestamp { & $buildScript @buildParams }
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: build.ps1 failed" -ForegroundColor Red
    return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
}

$ctx = Initialize-KenshiScriptContext -InvocationPath $MyInvocation.MyCommand.Path
$resolved = Resolve-KenshiBuildContext -BoundParameters $PSBoundParameters -RepoDir $ctx.RepoDir -ModName $ModName -ProjectFileName $ProjectFileName -OutputSubdir $OutputSubdir -DllName $DllName -ModFileName $ModFileName -ConfigFileName $ConfigFileName -Configuration $Configuration -Platform $Platform -PlatformToolset $PlatformToolset

$stagingRoot = Join-Path $ctx.RepoDir ".packaging\$($resolved.ModName)"
Write-Host "Staging: $stagingRoot" -ForegroundColor Gray

if (-not (Test-Path $resolved.ModDir)) {
    if (-not (Test-Path $initTemplateScript)) {
        Write-Host "ERROR: Mod template folder not found: $($resolved.ModDir)" -ForegroundColor Red
        Write-Host "ERROR: init-mod-template.ps1 not found at $initTemplateScript" -ForegroundColor Red
        return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
    }

    Write-Host "Mod template folder missing. Initializing baseline template..." -ForegroundColor Yellow
    try {
        & $initTemplateScript -RepoDir $ctx.RepoDir -ModName $resolved.ModName -DllName $resolved.DllName -ModFileName $resolved.ModFileName -ConfigFileName $resolved.ConfigFileName
    } catch {
        Write-Host "ERROR: Failed while initializing mod template folder. Details: $_" -ForegroundColor Red
        return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
    }

    if (-not (Test-Path $resolved.ModDir)) {
        Write-Host "ERROR: Failed to initialize mod template folder: $($resolved.ModDir)" -ForegroundColor Red
        return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
    }
}

Write-Host "Preparing package staging..." -ForegroundColor Yellow
if (Test-Path $stagingRoot) {
    Remove-Item -Path $stagingRoot -Recurse -Force
}

New-Item -ItemType Directory -Path $stagingRoot -Force | Out-Null
Copy-Item -Path "$($resolved.ModDir)\*" -Destination $stagingRoot -Recurse -Force
Copy-Item -Path $resolved.DllPath -Destination (Join-Path $stagingRoot $resolved.DllName) -Force

$packageParams = @{
    ModName = $resolved.ModName
    SourceModPath = $stagingRoot
    DllName = $resolved.DllName
    ModFileName = $resolved.ModFileName
    ConfigFileName = $resolved.ConfigFileName
}

if ($PSBoundParameters.ContainsKey("OutDir")) {
    $packageParams.OutDir = $OutDir
}
if ($PSBoundParameters.ContainsKey("ZipName")) {
    $packageParams.ZipName = $ZipName
}
if ($PSBoundParameters.ContainsKey("Version")) {
    $packageParams.Version = $Version
}

Invoke-KenshiScriptWithSuppressedTimestamp { & $packageScript @packageParams }
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: package.ps1 failed" -ForegroundColor Red
    return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
}

return (Exit-KenshiScriptWithTimestamp -ExitCode 0)
