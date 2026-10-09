# Runtime smoke test for the Job-B-Gone Mod Hub consumer path.
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

    throw "Could not find a recent Job-B-Gone startup line in the log."
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
$sessionLines = Get-LastSessionLines -Lines $allLines -StartupPattern "Job-B-Gone: startPlugin\(\)"

$loadedConfig = Get-LastMatch -Lines $sessionLines -Pattern "Job-B-Gone INFO: loaded config enabled="
$attached = Get-LastMatch -Lines $sessionLines -Pattern "Job-B-Gone INFO: event=mod_hub_attached use_hub_ui=1"
$fallback = Get-LastMatch -Lines $sessionLines -Pattern "Job-B-Gone (INFO|WARN): event=mod_hub_fallback .* use_hub_ui=0"
$pluginError = Get-LastMatch -Lines $sessionLines -Pattern "Job-B-Gone ERROR:"

Assert-Condition -Condition ($null -ne $loadedConfig) -Message "Missing Job-B-Gone loaded-config line in the latest RE_Kenshi session."
Assert-Condition -Condition ($null -eq $pluginError) -Message ("Found Job-B-Gone error line: " + $pluginError.Line)

$resolvedMode = "unknown"
$lastModeEvent = $null
foreach ($match in @($fallback, $attached)) {
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
    } else {
        $resolvedMode = "fallback"
    }
}

switch ($ExpectedMode) {
    "attached" {
        Assert-Condition -Condition ($resolvedMode -eq "attached") -Message "Expected attached mode, but the latest session did not reach a successful Mod Hub attach/registration path."
    }
    "fallback" {
        Assert-Condition -Condition ($resolvedMode -eq "fallback") -Message "Expected fallback mode, but the latest session did not end in the file-config-only path."
    }
    "either" {
        Assert-Condition -Condition ($resolvedMode -ne "unknown") -Message "Latest session did not emit a recognizable Mod Hub attach or fallback result."
    }
}

Write-Host ("PASS: phase22 Job-B-Gone Mod Hub runtime smoke completed ({0})" -f $resolvedMode)
Write-Host ("Log: {0}" -f $resolvedLogPath)
Write-Host ("Log age (minutes): {0:N1}" -f $ageMinutes)
Write-Host ("Loaded config: {0}" -f $loadedConfig.Line)
if ($null -ne $attached) {
    Write-Host ("Attach success: {0}" -f $attached.Line)
}
if ($null -ne $fallback) {
    Write-Host ("Fallback: {0}" -f $fallback.Line)
}

exit 0
