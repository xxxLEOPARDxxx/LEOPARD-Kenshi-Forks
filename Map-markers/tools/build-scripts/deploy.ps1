# Deploy-only script for Kenshi mod plugins.

param(
    [string]$ModName = "",
    [string]$ProjectFileName = "",
    [string]$OutputSubdir = "",
    [string]$DllName = "",
    [string]$ModFileName = "",
    [string]$ConfigFileName = "",
    [string]$KenshiPath = "",
    [string]$Configuration = "",
    [string]$Platform = ""
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$CommonScript = Join-Path $scriptDir "kenshi-common.ps1"
$initTemplateScript = Join-Path $scriptDir "init-mod-template.ps1"
if (-not (Test-Path $CommonScript)) {
    Write-Host "ERROR: Missing shared helper: $CommonScript" -ForegroundColor Red
    exit 1
}
. $CommonScript

$ctx = Initialize-KenshiScriptContext -InvocationPath $MyInvocation.MyCommand.Path
$resolved = Resolve-KenshiBuildContext -BoundParameters $PSBoundParameters -RepoDir $ctx.RepoDir -ModName $ModName -ProjectFileName $ProjectFileName -OutputSubdir $OutputSubdir -DllName $DllName -ModFileName $ModFileName -ConfigFileName $ConfigFileName -KenshiPath $KenshiPath -Configuration $Configuration -Platform $Platform

Write-Host "=== $($resolved.ModName) Deploy ===" -ForegroundColor Cyan
Write-Host "DLL Source:       $($resolved.DllPath)" -ForegroundColor Gray
Write-Host "Kenshi Path:      $($resolved.KenshiPath)" -ForegroundColor Gray
Write-Host "Mod Destination:  $($resolved.KenshiModPath)" -ForegroundColor Gray

if (-not $resolved.KenshiPath) {
    Write-Host "ERROR: Kenshi path is not set. Provide -KenshiPath or set KENSHI_PATH in .env." -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $resolved.KenshiPath)) {
    Write-Host "ERROR: Kenshi directory not found: $($resolved.KenshiPath)" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $resolved.DllPath)) {
    Write-Host "ERROR: DLL not found: $($resolved.DllPath)" -ForegroundColor Red
    Write-Host "Run build.ps1 (or a build wrapper) before deploy." -ForegroundColor Yellow
    exit 1
}

if (-not (Test-Path $resolved.KenshiModPath)) {
    New-Item -ItemType Directory -Path $resolved.KenshiModPath -Force | Out-Null
    Write-Host "Created mod directory: $($resolved.KenshiModPath)" -ForegroundColor Gray
}

if (-not (Test-Path $resolved.ModDir) -and (Test-Path $initTemplateScript)) {
    Write-Host "Local mod template folder missing. Initializing baseline template..." -ForegroundColor Yellow
    try {
        & $initTemplateScript -RepoDir $ctx.RepoDir -ModName $resolved.ModName -DllName $resolved.DllName -ModFileName $resolved.ModFileName -ConfigFileName $resolved.ConfigFileName
    } catch {
        Write-Host "ERROR: Failed while initializing mod template folder. Details: $_" -ForegroundColor Red
        exit 1
    }
}

if (Test-Path $resolved.ModDir) {
    Copy-Item -Path "$($resolved.ModDir)\*" -Destination $resolved.KenshiModPath -Recurse -Force
    Write-Host "Copied mod files from: $($resolved.ModDir)" -ForegroundColor Gray
} else {
    Write-Host "WARNING: Mod template directory not found: $($resolved.ModDir)" -ForegroundColor Yellow
    Write-Host "Only DLL will be copied." -ForegroundColor Yellow
}

$destDllPath = Join-Path $resolved.KenshiModPath $resolved.DllName
try {
    Copy-Item -Path $resolved.DllPath -Destination $destDllPath -Force
} catch {
    Write-Host "ERROR: Failed to copy DLL. File might be in use (is Kenshi running?)." -ForegroundColor Red
    Write-Host "Details: $_" -ForegroundColor Red
    exit 1
}

$sourceInfo = Get-Item $resolved.DllPath
$destInfo = Get-Item $destDllPath
if ($sourceInfo.Length -ne $destInfo.Length) {
    Write-Host "ERROR: Deployment failed. Destination DLL size mismatch." -ForegroundColor Red
    Write-Host "Source bytes: $($sourceInfo.Length)"
    Write-Host "Dest bytes:   $($destInfo.Length)"
    exit 1
}

Write-Host "Copied DLL: $($resolved.DllPath) -> $destDllPath" -ForegroundColor Gray

Write-Host "Updating RE_Kenshi.json Plugins list..." -ForegroundColor Yellow
$reKenshiJsonPath = Join-Path $resolved.KenshiModPath $resolved.ConfigFileName
try {
    Ensure-ReKenshiJson -Path $reKenshiJsonPath -PluginDllName $resolved.DllName
    $jsonContent = Get-Content -Path $reKenshiJsonPath | ConvertFrom-Json
    if (-not ($jsonContent.PSObject.Properties.Name -contains 'Plugins')) {
        $jsonContent | Add-Member -MemberType NoteProperty -Name Plugins -Value @()
    } elseif ($jsonContent.Plugins -isnot [array]) {
        $jsonContent.Plugins = @($jsonContent.Plugins)
    }
    $jsonContent.Plugins = @($resolved.DllName)
    $jsonContent | ConvertTo-Json -Depth 4 | Set-Content -Path $reKenshiJsonPath
    Write-Host "Successfully updated RE_Kenshi.json in deploy directory." -ForegroundColor Green
} catch {
    Write-Host "ERROR: Failed to update RE_Kenshi.json. Details: $_" -ForegroundColor Red
    exit 1
}

$destModFilePath = Join-Path $resolved.KenshiModPath $resolved.ModFileName
$defaultModTemplatePath = Join-Path $ctx.ScriptDir "templates\default.mod"
try {
    Ensure-ModFile -Path $destModFilePath -TemplatePath $defaultModTemplatePath
} catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
    exit 1
}

Write-Host "`nVerifying deployment..." -ForegroundColor Yellow
$dllDeployed = Test-Path $destDllPath
$modFileDeployed = Test-Path (Join-Path $resolved.KenshiModPath $resolved.ModFileName)
$jsonFileDeployed = Test-Path (Join-Path $resolved.KenshiModPath $resolved.ConfigFileName)

if ($dllDeployed) {
    Write-Host "[OK] $($resolved.DllName)" -ForegroundColor Green
} else {
    Write-Host "[FAIL] $($resolved.DllName) (MISSING!)" -ForegroundColor Red
}

if ($modFileDeployed) {
    Write-Host "[OK] $($resolved.ModFileName)" -ForegroundColor Green
} else {
    Write-Host "[WARN] $($resolved.ModFileName) (optional)" -ForegroundColor Yellow
}

if ($jsonFileDeployed) {
    Write-Host "[OK] $($resolved.ConfigFileName)" -ForegroundColor Green
} else {
    Write-Host "[WARN] $($resolved.ConfigFileName) (optional)" -ForegroundColor Yellow
}

Write-Host "`n=== Deployment Complete ===" -ForegroundColor Cyan
Write-Host "Mod location: $($resolved.KenshiModPath)" -ForegroundColor Gray
