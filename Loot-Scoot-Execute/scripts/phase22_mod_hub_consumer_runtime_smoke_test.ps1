param(
    [string]$KenshiPath = "",
    [string]$LogPath = "",
    [ValidateSet("attached", "fallback", "either")][string]$ExpectedMode = "attached",
    [int]$MaxAgeMinutes = 120
)

$ErrorActionPreference = "Stop"

function Assert-Condition {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Get-DefaultLogCandidates {
    param(
        [string]$ProvidedKenshiPath
    )

    $candidates = New-Object System.Collections.Generic.List[string]

    if (-not [string]::IsNullOrWhiteSpace($ProvidedKenshiPath)) {
        [void]$candidates.Add((Join-Path $ProvidedKenshiPath "RE_Kenshi_log.txt"))
    }

    if (-not [string]::IsNullOrWhiteSpace($env:KENSHI_PATH)) {
        [void]$candidates.Add((Join-Path $env:KENSHI_PATH "RE_Kenshi_log.txt"))
    }

    foreach ($path in @(
            "/mnt/h/SteamLibrary/steamapps/common/Kenshi/RE_Kenshi_log.txt",
            "H:\SteamLibrary\steamapps\common\Kenshi\RE_Kenshi_log.txt",
            "H:\steamlibrary\steamapps\common\kenshi\RE_Kenshi_log.txt")) {
        [void]$candidates.Add($path)
    }

    return $candidates
}

function Resolve-SmokeLogPath {
    param(
        [string]$ProvidedLogPath,
        [string]$ProvidedKenshiPath
    )

    if (-not [string]::IsNullOrWhiteSpace($ProvidedLogPath)) {
        return $ProvidedLogPath
    }

    foreach ($candidate in Get-DefaultLogCandidates -ProvidedKenshiPath $ProvidedKenshiPath) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path -LiteralPath $candidate)) {
            return $candidate
        }
    }

    throw "Could not resolve RE_Kenshi_log.txt. Provide -LogPath or -KenshiPath."
}

function Get-LastMatch {
    param(
        [Parameter(Mandatory = $true)][string[]]$Lines,
        [Parameter(Mandatory = $true)][string]$Pattern
    )

    return $Lines | Select-String -Pattern $Pattern | Select-Object -Last 1
}

$resolvedLogPath = Resolve-SmokeLogPath -ProvidedLogPath $LogPath -ProvidedKenshiPath $KenshiPath
Assert-Condition -Condition (Test-Path -LiteralPath $resolvedLogPath) -Message "Log file not found: $resolvedLogPath"

$logItem = Get-Item -LiteralPath $resolvedLogPath
$ageMinutes = ((Get-Date).ToUniversalTime() - $logItem.LastWriteTimeUtc).TotalMinutes
Assert-Condition -Condition ($ageMinutes -le $MaxAgeMinutes) -Message ("Log file is stale ({0:N1} minutes old): {1}" -f $ageMinutes, $resolvedLogPath)

$lines = Get-Content -LiteralPath $resolvedLogPath
$coreStartup = Get-LastMatch -Lines $lines -Pattern "Emkejs-Mod-Core INFO: startup complete"
$loadedConfig = Get-LastMatch -Lines $lines -Pattern "Loot-Scoot-Execute INFO: loaded config"
$initialized = Get-LastMatch -Lines $lines -Pattern "Loot-Scoot-Execute INFO: initialized"
$hubWarning = Get-LastMatch -Lines $lines -Pattern "Loot-Scoot-Execute WARN: event=loot_scoot_execute_hub_(attach_failed|fallback)"
$pluginError = Get-LastMatch -Lines $lines -Pattern "Loot-Scoot-Execute ERROR:"

Assert-Condition -Condition ($null -ne $loadedConfig) -Message "Missing Loot-Scoot-Execute loaded-config line in RE_Kenshi_log.txt."
Assert-Condition -Condition ($null -ne $initialized) -Message "Missing Loot-Scoot-Execute initialized line in RE_Kenshi_log.txt."
Assert-Condition -Condition ($null -eq $pluginError) -Message ("Found Loot-Scoot-Execute error line: " + $pluginError.Line)
Assert-Condition -Condition ($initialized.Line.Contains("hook_verification=passed")) -Message "Initialization line does not contain hook_verification=passed."

$resolvedMode = "unknown"
if ($initialized.Line.Contains("mod_hub_use_ui=true")) {
    $resolvedMode = "attached"
} elseif ($initialized.Line.Contains("mod_hub_use_ui=false")) {
    $resolvedMode = "fallback"
}

switch ($ExpectedMode) {
    "attached" {
        Assert-Condition -Condition ($resolvedMode -eq "attached") -Message "Expected attached mode, but initialized line did not report mod_hub_use_ui=true."
        Assert-Condition -Condition ($initialized.Line.Contains("mod_hub_last_result=0")) -Message "Attached mode should report mod_hub_last_result=0."
        Assert-Condition -Condition ($null -ne $coreStartup) -Message "Attached mode should include Emkejs-Mod-Core startup complete in the log."
    }
    "fallback" {
        Assert-Condition -Condition ($resolvedMode -eq "fallback") -Message "Expected fallback mode, but initialized line did not report mod_hub_use_ui=false."
        Assert-Condition -Condition ($null -ne $hubWarning) -Message "Fallback mode should emit a Mod Hub attach/fallback warning line."
    }
    "either" {
        Assert-Condition -Condition ($resolvedMode -ne "unknown") -Message "Initialized line did not report either mod_hub_use_ui=true or mod_hub_use_ui=false."
    }
}

Write-Host ("PASS: phase22 Mod Hub consumer runtime smoke completed ({0})" -f $resolvedMode)
Write-Host ("Log: {0}" -f $resolvedLogPath)
Write-Host ("Log age (minutes): {0:N1}" -f $ageMinutes)
Write-Host ("Loaded config: {0}" -f $loadedConfig.Line)
Write-Host ("Initialized: {0}" -f $initialized.Line)
if ($null -ne $coreStartup) {
    Write-Host ("Core startup: {0}" -f $coreStartup.Line)
}
if ($null -ne $hubWarning) {
    Write-Host ("Hub warning: {0}" -f $hubWarning.Line)
}

exit 0
