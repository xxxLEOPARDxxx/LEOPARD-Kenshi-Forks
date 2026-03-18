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

function Get-LastSessionLines {
    param(
        [Parameter(Mandatory = $true)][string[]]$Lines,
        [Parameter(Mandatory = $true)][string]$StartupPattern
    )

    for ($index = $Lines.Count - 1; $index -ge 0; --$index) {
        if ($Lines[$index] -match $StartupPattern) {
            if ($index -eq ($Lines.Count - 1)) {
                return ,$Lines[$index]
            }

            return $Lines[$index..($Lines.Count - 1)]
        }
    }

    throw "Could not find a recent Organize-the-Crafting-Stations startup line in the log."
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

$allLines = Get-Content -LiteralPath $resolvedLogPath
$sessionLines = Get-LastSessionLines -Lines $allLines -StartupPattern "Organize-the-Crafting-Stations INFO: startPlugin\(\)"

$loadedConfig = Get-LastMatch -Lines $sessionLines -Pattern "Organize-the-Crafting-Stations INFO: mod config loaded"
$attached = Get-LastMatch -Lines $sessionLines -Pattern "Organize-the-Crafting-Stations INFO: event=mod_hub_attached use_hub_ui=1"
$retryPending = Get-LastMatch -Lines $sessionLines -Pattern "Organize-the-Crafting-Stations INFO: event=mod_hub_attach_retry_pending use_hub_ui=0"
$fallback = Get-LastMatch -Lines $sessionLines -Pattern "Organize-the-Crafting-Stations WARN: event=mod_hub_fallback .* use_hub_ui=0"
$pluginError = Get-LastMatch -Lines $sessionLines -Pattern "Organize-the-Crafting-Stations ERROR:"

Assert-Condition -Condition ($null -ne $loadedConfig) -Message "Missing Organize-the-Crafting-Stations mod-config load line in the latest RE_Kenshi session."
Assert-Condition -Condition ($null -eq $pluginError) -Message ("Found Organize-the-Crafting-Stations error line: " + $pluginError.Line)

$resolvedMode = "unknown"
$lastModeEvent = $null
foreach ($match in @($fallback, $retryPending, $attached)) {
    if ($null -eq $match) {
        continue
    }

    if ($null -eq $lastModeEvent -or $match.LineNumber -gt $lastModeEvent.LineNumber) {
        $lastModeEvent = $match
    }
}

if ($null -ne $lastModeEvent) {
    if ($lastModeEvent.Line -match "use_hub_ui=1") {
        $resolvedMode = "attached"
    } elseif ($lastModeEvent.Line -match "event=mod_hub_attach_retry_pending") {
        $resolvedMode = "pending_retry"
    } else {
        $resolvedMode = "fallback"
    }
}

switch ($ExpectedMode) {
    "attached" {
        Assert-Condition -Condition ($resolvedMode -eq "attached") -Message "Expected attached mode, but the latest session did not reach event=mod_hub_attached."
    }
    "fallback" {
        Assert-Condition -Condition ($resolvedMode -eq "fallback") -Message "Expected fallback mode, but the latest session did not end in the file-config-only path."
    }
    "either" {
        Assert-Condition -Condition (($resolvedMode -eq "attached") -or ($resolvedMode -eq "fallback")) -Message "Latest session did not emit a final Mod Hub attach or fallback result."
    }
}

Write-Host ("PASS: phase22 Organize-the-Crafting-Stations Mod Hub runtime smoke completed ({0})" -f $resolvedMode)
Write-Host ("Log: {0}" -f $resolvedLogPath)
Write-Host ("Log age (minutes): {0:N1}" -f $ageMinutes)
Write-Host ("Loaded config: {0}" -f $loadedConfig.Line)
if ($null -ne $attached) {
    Write-Host ("Attach success: {0}" -f $attached.Line)
}
if ($null -ne $retryPending) {
    Write-Host ("Retry pending: {0}" -f $retryPending.Line)
}
if ($null -ne $fallback) {
    Write-Host ("Fallback: {0}" -f $fallback.Line)
}
exit 0
