# Initializes the mod template folder used for packaging.
# Creates missing baseline files but does not overwrite existing content.

param(
    [string]$RepoDir = "",
    [string]$ModName = "",
    [string]$DllName = "",
    [string]$ModFileName = "",
    [string]$ConfigFileName = "RE_Kenshi.json"
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $RepoDir) {
    $RepoDir = Split-Path -Parent $ScriptDir
}

if (-not $ModName) {
    if ($env:KENSHI_MOD_NAME) {
        $ModName = $env:KENSHI_MOD_NAME
    } else {
        $ModName = Split-Path -Leaf $RepoDir
    }
}

if (-not $DllName) {
    if ($env:KENSHI_DLL_NAME) {
        $DllName = $env:KENSHI_DLL_NAME
    } else {
        $DllName = "$ModName.dll"
    }
}

if (-not $ModFileName) {
    if ($env:KENSHI_MOD_FILE_NAME) {
        $ModFileName = $env:KENSHI_MOD_FILE_NAME
    } else {
        $ModFileName = "$ModName.mod"
    }
}

$modTemplateDir = Join-Path $RepoDir $ModName
if (-not (Test-Path $modTemplateDir)) {
    New-Item -ItemType Directory -Path $modTemplateDir -Force | Out-Null
    Write-Host "Created mod template folder: $modTemplateDir" -ForegroundColor Gray
}

$defaultModTemplatePath = Join-Path $ScriptDir "templates\default.mod"
$destModPath = Join-Path $modTemplateDir $ModFileName
if (-not (Test-Path $destModPath)) {
    if (-not (Test-Path $defaultModTemplatePath)) {
        throw "Default .mod template not found: $defaultModTemplatePath"
    }
    Copy-Item -Path $defaultModTemplatePath -Destination $destModPath -Force
    Write-Host "Created missing mod file: $destModPath" -ForegroundColor Gray
}

$reKenshiJsonPath = Join-Path $modTemplateDir $ConfigFileName
if (-not (Test-Path $reKenshiJsonPath)) {
    $seedObject = @{ Plugins = @($DllName) }
    $seedObject | ConvertTo-Json -Depth 4 | Set-Content -Path $reKenshiJsonPath
    Write-Host "Created missing config file: $reKenshiJsonPath" -ForegroundColor Gray
}

$modConfigPath = Join-Path $modTemplateDir "mod-config.json"
if (-not (Test-Path $modConfigPath)) {
    $modConfig = @{
        enabled = $true
        pause_debounce_ms = 2000
        debug_log_transitions = $false
    }
    $modConfig | ConvertTo-Json -Depth 4 | Set-Content -Path $modConfigPath
    Write-Host "Created missing mod-config.json: $modConfigPath" -ForegroundColor Gray
}
