param(
    [string]$RepoRoot = "",
    [string]$TempRoot = ""
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

function Get-PowerShellExecutable {
    foreach ($candidate in @("powershell.exe", "pwsh.exe", "pwsh", "powershell")) {
        $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($null -ne $cmd) {
            return $cmd.Source
        }
    }

    throw "Unable to locate a PowerShell executable for subprocess test runs."
}

function Invoke-DeploySubprocess {
    param(
        [Parameter(Mandatory = $true)][string]$PowerShellExe,
        [Parameter(Mandatory = $true)][string]$DeployScript,
        [Parameter(Mandatory = $true)][string]$ModName,
        [Parameter(Mandatory = $true)][string]$DllName,
        [Parameter(Mandatory = $true)][string]$KenshiPath,
        [Parameter(Mandatory = $true)][string]$OutputSubdir
    )

    $invokeArgs = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $DeployScript,
        "-ModName", $ModName,
        "-DllName", $DllName,
        "-ModFileName", "$ModName.mod",
        "-ConfigFileName", "RE_Kenshi.json",
        "-KenshiPath", $KenshiPath,
        "-OutputSubdir", $OutputSubdir,
        "-Configuration", "Release",
        "-Platform", "x64"
    )

    $rawOutput = & $PowerShellExe @invokeArgs 2>&1
    $outputText = ($rawOutput | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine
    return [pscustomobject]@{
        ExitCode = $LASTEXITCODE
        Output = $outputText
    }
}

if (-not $RepoRoot) {
    $scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $PSCommandPath }
    $RepoRoot = Split-Path -Parent $scriptDir
}

if (-not (Test-Path $RepoRoot)) {
    throw "RepoRoot not found: $RepoRoot"
}

if ($env:OS -ne "Windows_NT") {
    Write-Host "SKIP: phase12 deploy lock preflight test requires Windows."
    exit 0
}

$deployScript = Join-Path $RepoRoot "tools\build-scripts\deploy.ps1"
Assert-Condition -Condition (Test-Path $deployScript) -Message "Missing deploy script: $deployScript"

if (-not $TempRoot) {
    $TempRoot = Join-Path ([System.IO.Path]::GetTempPath()) "emc_phase12_deploy_preflight"
}
if (-not (Test-Path $TempRoot)) {
    New-Item -ItemType Directory -Path $TempRoot -Force | Out-Null
}

$runRoot = Join-Path $TempRoot ([Guid]::NewGuid().ToString("N"))
$repoSandbox = Join-Path $runRoot "repo"
$kenshiSandbox = Join-Path $runRoot "kenshi"
$modName = "Phase12LockTestMod"
$dllName = "$modName.dll"
$outputSubdir = "x64\Release"

$sourceDllDir = Join-Path $repoSandbox $outputSubdir
$sourceDllPath = Join-Path $sourceDllDir $dllName
$sourceModDir = Join-Path $repoSandbox $modName
$sourceMarkerPath = Join-Path $sourceModDir "phase12_marker.txt"
$destModDir = Join-Path $kenshiSandbox "mods\$modName"
$destDllPath = Join-Path $destModDir $dllName
$destMarkerPath = Join-Path $destModDir "phase12_marker.txt"

$sourceBytes = [byte[]](11, 22, 33, 44, 55, 66)
$destSeedBytes = [byte[]](9, 8, 7)
$lockStream = $null
$powerShellExe = Get-PowerShellExecutable
$originalRepoEnv = Get-Item -Path "env:KENSHI_REPO_DIR" -ErrorAction SilentlyContinue
$hadOriginalRepoEnv = $null -ne $originalRepoEnv

New-Item -ItemType Directory -Path $repoSandbox -Force | Out-Null
New-Item -ItemType Directory -Path $kenshiSandbox -Force | Out-Null
New-Item -ItemType Directory -Path $sourceDllDir -Force | Out-Null
New-Item -ItemType Directory -Path $sourceModDir -Force | Out-Null
New-Item -ItemType Directory -Path $destModDir -Force | Out-Null
[System.IO.File]::WriteAllBytes($sourceDllPath, $sourceBytes)
[System.IO.File]::WriteAllBytes($destDllPath, $destSeedBytes)
Set-Content -Path $sourceMarkerPath -Value "phase12_marker" -NoNewline

try {
    $env:KENSHI_REPO_DIR = $repoSandbox

    $lockStream = [System.IO.File]::Open(
        $destDllPath,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::Read
    )

    $lockedRun = Invoke-DeploySubprocess `
        -PowerShellExe $powerShellExe `
        -DeployScript $deployScript `
        -ModName $modName `
        -DllName $dllName `
        -KenshiPath $kenshiSandbox `
        -OutputSubdir $outputSubdir

    Assert-Condition -Condition ($lockedRun.ExitCode -eq 32) -Message "Expected lock preflight exit code 32, got $($lockedRun.ExitCode). Output: $($lockedRun.Output)"
    Assert-Condition -Condition ($lockedRun.Output.Contains("Deploy preflight failed")) -Message "Preflight failure message missing from locked deploy output."
    Assert-Condition -Condition ($lockedRun.Output.Contains($destDllPath)) -Message "Locked deploy output missing target path."
    Assert-Condition -Condition ($lockedRun.Output.Contains("Next steps")) -Message "Locked deploy output missing remediation guidance."
    Assert-Condition -Condition (-not (Test-Path $destMarkerPath)) -Message "Mod files were copied before lock preflight failed."
    $lockedBytes = [System.IO.File]::ReadAllBytes($destDllPath)
    Assert-Condition -Condition ([System.BitConverter]::ToString($lockedBytes) -eq [System.BitConverter]::ToString($destSeedBytes)) -Message "Destination DLL changed during locked preflight failure."

    $lockStream.Dispose()
    $lockStream = $null

    $unlockedRun = Invoke-DeploySubprocess `
        -PowerShellExe $powerShellExe `
        -DeployScript $deployScript `
        -ModName $modName `
        -DllName $dllName `
        -KenshiPath $kenshiSandbox `
        -OutputSubdir $outputSubdir

    Assert-Condition -Condition ($unlockedRun.ExitCode -eq 0) -Message "Expected successful deploy after lock release. Output: $($unlockedRun.Output)"
    Assert-Condition -Condition (Test-Path $destMarkerPath) -Message "Mod files were not copied after lock release."
    $finalBytes = [System.IO.File]::ReadAllBytes($destDllPath)
    Assert-Condition -Condition ([System.BitConverter]::ToString($finalBytes) -eq [System.BitConverter]::ToString($sourceBytes)) -Message "Destination DLL bytes do not match source after successful deploy."

    Write-Host "PASS"
}
finally {
    if ($null -ne $lockStream) {
        $lockStream.Dispose()
    }

    if ($hadOriginalRepoEnv) {
        Set-Item -Path "env:KENSHI_REPO_DIR" -Value $originalRepoEnv.Value
    } else {
        Remove-Item -Path "env:KENSHI_REPO_DIR" -ErrorAction SilentlyContinue
    }

    if (Test-Path $runRoot) {
        Remove-Item -Path $runRoot -Recurse -Force
    }
}
