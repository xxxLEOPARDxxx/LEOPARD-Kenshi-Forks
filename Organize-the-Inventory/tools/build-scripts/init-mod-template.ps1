# Initializes the mod template folder used for packaging.
# Creates missing baseline files but does not overwrite existing content.

param(
    [string]$RepoDir = "",
    [string]$ModName = "",
    [string]$DllName = "",
    [string]$ModFileName = "",
    [string]$ConfigFileName = ""
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$CommonScript = Join-Path $scriptDir "kenshi-common.ps1"
if (-not (Test-Path $CommonScript)) {
    Write-Host "ERROR: Missing shared helper: $CommonScript" -ForegroundColor Red
    exit 1
}
. $CommonScript

$ctx = Initialize-KenshiScriptContext -InvocationPath $MyInvocation.MyCommand.Path -RepoDirOverride $RepoDir
$resolved = Resolve-KenshiBuildContext -BoundParameters $PSBoundParameters -RepoDir $ctx.RepoDir -ModName $ModName -DllName $DllName -ModFileName $ModFileName -ConfigFileName $ConfigFileName

$modTemplateDir = $resolved.ModDir
if (-not (Test-Path $modTemplateDir)) {
    New-Item -ItemType Directory -Path $modTemplateDir -Force | Out-Null
    Write-Host "Created mod template folder: $modTemplateDir" -ForegroundColor Gray
}

$defaultModTemplatePath = Join-Path $ctx.ScriptDir "templates\default.mod"
$destModPath = Join-Path $modTemplateDir $resolved.ModFileName
try {
    Ensure-ModFile -Path $destModPath -TemplatePath $defaultModTemplatePath
} catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
    exit 1
}

$reKenshiJsonPath = Join-Path $modTemplateDir $resolved.ConfigFileName
Ensure-ReKenshiJson -Path $reKenshiJsonPath -PluginDllName $resolved.DllName

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
