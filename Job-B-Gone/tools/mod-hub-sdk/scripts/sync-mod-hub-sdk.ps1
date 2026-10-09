# Sync Mod Hub SDK submodule and validate expected SDK contract assets.
param(
    [string]$RepoDir = "",
    [string]$SdkPath = "tools/mod-hub-sdk",
    [switch]$SkipPull
)

$ErrorActionPreference = "Stop"

function Resolve-AbsolutePath {
    param(
        [Parameter(Mandatory = $true)][string]$BasePath,
        [Parameter(Mandatory = $true)][string]$InputPath
    )

    if ([System.IO.Path]::IsPathRooted($InputPath)) {
        return [System.IO.Path]::GetFullPath($InputPath)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $BasePath $InputPath))
}

function Get-GitCommandPath {
    $git = Get-Command "git" -ErrorAction SilentlyContinue
    if ($null -eq $git) {
        throw "git command not found in PATH."
    }

    return $git.Source
}

function Assert-SdkContractFiles {
    param(
        [Parameter(Mandatory = $true)][string]$SdkRoot
    )

    $requiredRelativePaths = @(
        "include/emc/mod_hub_api.h",
        "include/emc/mod_hub_client.h",
        "include/emc/mod_hub_consumer_helpers.h",
        "src/mod_hub_client.cpp",
        "docs/mod-hub-sdk.md",
        "docs/mod-hub-sdk-quickstart.md"
    )

    $missingPaths = New-Object System.Collections.Generic.List[string]
    foreach ($relativePath in $requiredRelativePaths) {
        $fullPath = Resolve-Path -LiteralPath (Join-Path $SdkRoot $relativePath) -ErrorAction SilentlyContinue
        if ($null -eq $fullPath) {
            [void]$missingPaths.Add($relativePath)
        }
    }

    if ($missingPaths.Count -gt 0) {
        throw ("SDK validation failed. Missing assets: " + ($missingPaths -join ", "))
    }

    $apiHeaderPath = Join-Path $SdkRoot "include/emc/mod_hub_api.h"
    $apiHeaderText = Get-Content -LiteralPath $apiHeaderPath -Raw
    foreach ($requiredSymbol in @(
            "EMC_HUB_API_VERSION_1",
            "EMC_HUB_API_V1_MIN_SIZE",
            "EMC_HUB_API_V1_OPTIONS_WINDOW_INIT_OBSERVER_MIN_SIZE")) {
        if (-not $apiHeaderText.Contains($requiredSymbol)) {
            throw "SDK validation failed. Missing symbol '$requiredSymbol' in include/emc/mod_hub_api.h."
        }
    }
}

$ScriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $PSCommandPath }
$LocalRepoDir = if ($ScriptDir) { Split-Path -Parent $ScriptDir } else { (Get-Location).Path }
if (-not $RepoDir) { $RepoDir = $LocalRepoDir }
$RepoDir = [System.IO.Path]::GetFullPath($RepoDir)

$git = Get-GitCommandPath
$SdkRoot = Resolve-AbsolutePath -BasePath $RepoDir -InputPath $SdkPath
$SdkSubmodulePath = $SdkPath -replace '\\', '/'

if (-not $SkipPull) {
    if ([System.IO.Path]::IsPathRooted($SdkPath)) {
        throw "Pull mode expects a repo-relative submodule path. Use -SkipPull for absolute-path validation."
    }

    $submoduleStatus = & $git -C $RepoDir submodule status -- $SdkSubmodulePath 2>$null
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace(($submoduleStatus -join "`n"))) {
        throw "Expected '$SdkSubmodulePath' to be a git submodule. Use -SkipPull for validation-only mode."
    }

    $previousRevision = (& $git -C $SdkRoot rev-parse --short HEAD 2>$null) -join ""
    & $git -C $RepoDir submodule update --init --remote --recursive -- $SdkSubmodulePath
    if ($LASTEXITCODE -ne 0) {
        throw "Submodule update failed for $SdkSubmodulePath"
    }

    $currentRevision = (& $git -C $SdkRoot rev-parse --short HEAD 2>$null) -join ""
    Write-Host ("SDK sync revision: {0} -> {1}" -f $previousRevision, $currentRevision) -ForegroundColor Gray
}
elseif (-not (Test-Path -LiteralPath $SdkRoot)) {
    throw "SDK path not found: $SdkRoot"
}

Assert-SdkContractFiles -SdkRoot $SdkRoot
Write-Host "SDK validation passed." -ForegroundColor Green
Write-Host "Note: changelog updates are manual; this command does not modify changelog files." -ForegroundColor Yellow

exit 0
