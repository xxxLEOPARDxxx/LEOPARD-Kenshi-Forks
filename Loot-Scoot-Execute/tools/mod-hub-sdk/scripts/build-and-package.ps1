# Local wrapper: delegates to shared scripts submodule.
param(
    [string]$ModName = "",
    [string]$ProjectFileName = "",
    [string]$OutputSubdir = "",
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$PlatformToolset = "v100",
    [string]$DllName = "",
    [string]$ModFileName = "",
    [string]$ConfigFileName = "RE_Kenshi.json",
    [string]$OutDir = "",
    [string]$ZipName = "",
    [string]$Version = "",
    [switch]$SkipSdkPackage,
    [switch]$RunReliabilitySmoke,
    [string]$SmokeKenshiPath = "",
    [string]$SdkOutDir = "",
    [string]$SdkZipName = "",
    [string]$SdkVersion = ""
)

$ErrorActionPreference = "Stop"
$ScriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $PSCommandPath }
$RepoDir = if ($ScriptDir) { Split-Path -Parent $ScriptDir } else { (Get-Location).Path }
$env:KENSHI_REPO_DIR = $RepoDir
$SharedRoot = Join-Path $RepoDir "tools\build-scripts"
$SharedScript = Join-Path $SharedRoot "build-and-package.ps1"

if (-not (Test-Path $SharedScript)) {
    Write-Host "ERROR: Shared script not found: $SharedScript" -ForegroundColor Red
    Write-Host "Run: git submodule update --init --recursive" -ForegroundColor Yellow
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

$BuildContext = $null
if ($RunReliabilitySmoke) {
    $BuildContext = Resolve-KenshiBuildContext `
        -BoundParameters $PSBoundParameters `
        -RepoDir $RepoDir `
        -ModName $ModName `
        -ProjectFileName $ProjectFileName `
        -OutputSubdir $OutputSubdir `
        -DllName $DllName `
        -ModFileName $ModFileName `
        -ConfigFileName $ConfigFileName `
        -Configuration $Configuration `
        -Platform $Platform `
        -PlatformToolset $PlatformToolset

    if ($BuildContext.Configuration -ne "Debug") {
        Write-Host "ERROR: -RunReliabilitySmoke requires -Configuration Debug." -ForegroundColor Red
        exit 1
    }
}

$Forward = @{}
foreach ($k in @('ModName','ProjectFileName','OutputSubdir','Configuration','Platform','PlatformToolset','DllName','ModFileName','ConfigFileName','OutDir','ZipName','Version')) {
    if ($PSBoundParameters.ContainsKey($k)) { $Forward[$k] = (Get-Variable -Name $k -ValueOnly) }
}

& $SharedScript @Forward
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if ($RunReliabilitySmoke) {
    if ($null -eq $BuildContext) {
        throw "Build context was not resolved for reliability smoke."
    }

    $SmokeScript = Join-Path $ScriptDir "phase16_hub_attach_reliability_smoke_test.ps1"
    if (-not (Test-Path $SmokeScript)) {
        Write-Host "ERROR: Reliability smoke script not found: $SmokeScript" -ForegroundColor Red
        exit 1
    }

    $ResolvedSmokeKenshiPath = Resolve-KenshiValue -CurrentValue $SmokeKenshiPath -EnvVar "KENSHI_PATH"
    if (-not $ResolvedSmokeKenshiPath) {
        $ResolvedSmokeKenshiPath = Resolve-KenshiValue -EnvVar "KENSHI_DEFAULT_PATH"
    }

    $SmokeParams = @{
        DllPath = $BuildContext.DllPath
    }

    if ($ResolvedSmokeKenshiPath) {
        $SmokeParams.KenshiPath = $ResolvedSmokeKenshiPath
    }

    & $SmokeScript @SmokeParams
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
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
