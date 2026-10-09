param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [string]$KenshiPath = "",
    [string]$WallDllPath = "",
    [string]$WallRepoPath = ""
)

$ErrorActionPreference = "Stop"

$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $PSCommandPath }
$coreRepoPath = Split-Path -Parent $scriptDir
$repoParentPath = Split-Path -Parent $coreRepoPath

$candidateRepoPaths = @()
if ($WallRepoPath) {
    $candidateRepoPaths += $WallRepoPath
}
if ($repoParentPath) {
    $candidateRepoPaths += Join-Path $repoParentPath "Wall-B-Gone"
}
$candidateRepoPaths = $candidateRepoPaths | Select-Object -Unique

$consumerScript = ""
foreach ($candidateRepoPath in $candidateRepoPaths) {
    $candidateScript = Join-Path $candidateRepoPath "scripts/phase6_wall_b_gone_fallback_test.ps1"
    if (Test-Path -LiteralPath $candidateScript) {
        $consumerScript = $candidateScript
        break
    }
}

if (-not $consumerScript) {
    $searched = $candidateRepoPaths -join "; "
    Write-Host "SKIPPED: phase6 moved to Wall-B-Gone repo but script not found. Searched: $searched"
    exit 2
}

if (-not $WallDllPath) {
    $candidateWallDll = Join-Path (Split-Path -Parent $DllPath) "Wall-B-Gone.dll"
    if (Test-Path -LiteralPath $candidateWallDll) {
        $WallDllPath = $candidateWallDll
    }
}

if (-not $WallDllPath -or -not (Test-Path -LiteralPath $WallDllPath)) {
    Write-Host "SKIPPED: Wall-B-Gone.dll not found. Provide -WallDllPath to run consumer-owned phase6 harness."
    exit 2
}

& $consumerScript -DllPath $WallDllPath -CoreDllPath $DllPath -KenshiPath $KenshiPath
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

exit 0
