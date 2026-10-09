# Local wrapper: delegates to shared scripts submodule.
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
    Write-Host "Run: git submodule update --init --recursive" -ForegroundColor Yellow
    exit 1
}

$LoadEnvScript = Join-Path $SharedRoot "load-env.ps1"
if (Test-Path $LoadEnvScript) {
    . $LoadEnvScript -RepoDir $RepoDir
}

$Forward = @{}
foreach ($k in @('ModName','KenshiPath','SourceModPath','DllName','ModFileName','ConfigFileName','OutDir','ZipName','Version')) {
    if ($PSBoundParameters.ContainsKey($k)) { $Forward[$k] = (Get-Variable -Name $k -ValueOnly) }
}

& $SharedScript @Forward
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if ($SkipSdkPackage) {
    exit 0
}

$SdkScript = Join-Path $ScriptDir "package-sdk.ps1"
if (-not (Test-Path $SdkScript)) {
    Write-Host "ERROR: SDK package script not found: $SdkScript" -ForegroundColor Red
    exit 1
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
exit $LASTEXITCODE
