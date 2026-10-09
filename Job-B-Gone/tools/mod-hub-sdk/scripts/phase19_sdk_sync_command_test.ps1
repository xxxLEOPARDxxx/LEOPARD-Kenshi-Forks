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

if (-not $RepoRoot) {
    $scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $PSCommandPath }
    $RepoRoot = Split-Path -Parent $scriptDir
}

if (-not (Test-Path $RepoRoot)) {
    throw "RepoRoot not found: $RepoRoot"
}

$syncScript = Join-Path $RepoRoot "scripts\sync-mod-hub-sdk.ps1"
Assert-Condition -Condition (Test-Path $syncScript) -Message "Missing sync script: $syncScript"
$syncShellScript = Join-Path $RepoRoot "scripts/sync-mod-hub-sdk.sh"
Assert-Condition -Condition (Test-Path $syncShellScript) -Message "Missing sync shell wrapper: $syncShellScript"

$syncScriptText = Get-Content -Path $syncScript -Raw
Assert-Condition -Condition (-not $syncScriptText.ToUpperInvariant().Contains("CHANGELOG.MD")) -Message "Sync script must not auto-edit changelog files."
Assert-Condition -Condition ($syncScriptText.Contains("does not modify changelog files")) -Message "Sync script should state changelog edits are manual."

$missingSdkPath = "tools/missing-mod-hub-sdk"
try {
    & $syncScript -RepoDir $RepoRoot -SdkPath $missingSdkPath
    throw "Expected sync script to fail for non-submodule pull path."
}
catch {
    $message = $_.Exception.Message
    Assert-Condition -Condition ($message.Contains("Expected")) -Message "Pull-mode failure should explain missing submodule registration."
    Assert-Condition -Condition ($message.Contains("git submodule")) -Message "Pull-mode failure should mention git submodule expectation."
}

if (-not $TempRoot) {
    $TempRoot = Join-Path ([System.IO.Path]::GetTempPath()) "emc_phase19_sdk_sync"
}
if (-not (Test-Path $TempRoot)) {
    New-Item -ItemType Directory -Path $TempRoot -Force | Out-Null
}

$runRoot = Join-Path $TempRoot ([Guid]::NewGuid().ToString("N"))
$fakeSdkRoot = Join-Path $runRoot "sdk"

try {
    foreach ($dir in @(
            (Join-Path $fakeSdkRoot "include\emc"),
            (Join-Path $fakeSdkRoot "src"),
            (Join-Path $fakeSdkRoot "docs"))) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }

    Set-Content -Path (Join-Path $fakeSdkRoot "include\emc\mod_hub_api.h") -Value @'
#define EMC_HUB_API_VERSION_1 ((uint32_t)1u)
#define EMC_HUB_API_V1_MIN_SIZE ((uint32_t)56u)
#define EMC_HUB_API_V1_OPTIONS_WINDOW_INIT_OBSERVER_MIN_SIZE ((uint32_t)72u)
'@
    Set-Content -Path (Join-Path $fakeSdkRoot "include\emc\mod_hub_client.h") -Value "// test"
    Set-Content -Path (Join-Path $fakeSdkRoot "include\emc\mod_hub_consumer_helpers.h") -Value "// test"
    Set-Content -Path (Join-Path $fakeSdkRoot "src\mod_hub_client.cpp") -Value "// test"
    Set-Content -Path (Join-Path $fakeSdkRoot "docs\mod-hub-sdk.md") -Value "# sdk"
    Set-Content -Path (Join-Path $fakeSdkRoot "docs\mod-hub-sdk-quickstart.md") -Value "# quickstart"

    & $syncScript -RepoDir $RepoRoot -SdkPath $fakeSdkRoot -SkipPull
    Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "sync-mod-hub-sdk.ps1 failed in validation-only mode."

    $bash = Get-Command "bash" -ErrorAction SilentlyContinue
    if ($null -ne $bash) {
        & $bash.Source $syncShellScript "--repo-dir" $RepoRoot "--sdk-path" $fakeSdkRoot "--skip-pull" | Out-Null
        Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "sync-mod-hub-sdk.sh failed in validation-only mode."
    }

    Write-Host "PASS"
}
finally {
    if (Test-Path $runRoot) {
        Remove-Item -Path $runRoot -Recurse -Force
    }
}
