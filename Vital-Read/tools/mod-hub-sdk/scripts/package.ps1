# Local wrapper: delegates to shared build scripts.
param(
    [string]$ModName = "",
    [string]$KenshiPath = "",
    [string]$SourceModPath = "",
    [string]$DllName = "",
    [string]$ModFileName = "",
    [string]$ConfigFileName = "RE_Kenshi.json",
    [string]$OutDir = "",
    [string]$ZipName = "",
    [string]$Version = "",
    [switch]$SkipSdkPackage,
    [string]$SdkOutDir = "",
    [string]$SdkZipName = "",
    [string]$SdkVersion = ""
)

$ErrorActionPreference = "Stop"
$ScriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $PSCommandPath }
$RepoDir = if ($ScriptDir) { Split-Path -Parent $ScriptDir } else { (Get-Location).Path }
$env:KENSHI_REPO_DIR = $RepoDir
$SharedRoot = Join-Path $RepoDir "tools\build-scripts"
$SharedScript = Join-Path $SharedRoot "package.ps1"

if (-not (Test-Path $SharedScript)) {
    Write-Host "ERROR: Shared script not found: $SharedScript" -ForegroundColor Red
    Write-Host "Sync tools\\build-scripts from the shared repo and retry." -ForegroundColor Yellow
    exit 1
}

$LoadEnvScript = Join-Path $SharedRoot "load-env.ps1"
if (Test-Path $LoadEnvScript) {
    . $LoadEnvScript -RepoDir $RepoDir
}

$CommonScript = Join-Path $SharedRoot "kenshi-common.ps1"
if (-not (Test-Path $CommonScript)) {
    Write-Host "ERROR: Shared helper script not found: $CommonScript" -ForegroundColor Red
    exit 1
}
. $CommonScript

$Forward = @{}
foreach ($k in @('ModName','KenshiPath','SourceModPath','DllName','ModFileName','ConfigFileName','OutDir','ZipName','Version')) {
    if ($PSBoundParameters.ContainsKey($k)) { $Forward[$k] = (Get-Variable -Name $k -ValueOnly) }
}

Invoke-KenshiScriptWithSuppressedTimestamp { & $SharedScript @Forward }
if ($LASTEXITCODE -ne 0) {
    return (Exit-KenshiScriptWithTimestamp -ExitCode $LASTEXITCODE)
}

if ($SkipSdkPackage) {
    return (Exit-KenshiScriptWithTimestamp -ExitCode 0)
}

$SdkScript = Join-Path $ScriptDir "package-sdk.ps1"
if (-not (Test-Path $SdkScript)) {
    Write-Host "ERROR: SDK package script not found: $SdkScript" -ForegroundColor Red
    return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
}

$SdkParams = @{
    RepoDir = $RepoDir
}

if ($PSBoundParameters.ContainsKey("ModName")) { $SdkParams.ModName = $ModName }
if ($PSBoundParameters.ContainsKey("SdkOutDir")) { $SdkParams.OutDir = $SdkOutDir }
elseif ($PSBoundParameters.ContainsKey("OutDir")) { $SdkParams.OutDir = $OutDir }

if ($PSBoundParameters.ContainsKey("SdkZipName")) { $SdkParams.ZipName = $SdkZipName }

if ($PSBoundParameters.ContainsKey("SdkVersion")) { $SdkParams.Version = $SdkVersion }
elseif ($PSBoundParameters.ContainsKey("Version")) { $SdkParams.Version = $Version }

& $SdkScript @SdkParams
if ($LASTEXITCODE -ne 0) {
    return (Exit-KenshiScriptWithTimestamp -ExitCode $LASTEXITCODE)
}

return (Exit-KenshiScriptWithTimestamp -ExitCode 0)
