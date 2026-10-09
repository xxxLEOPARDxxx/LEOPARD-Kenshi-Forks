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

$initScript = Join-Path $RepoRoot "scripts\init-mod-template.ps1"
Assert-Condition -Condition (Test-Path $initScript) -Message "Missing init script: $initScript"

if (-not $TempRoot) {
    $TempRoot = Join-Path ([System.IO.Path]::GetTempPath()) "emc_phase9_scaffold"
}
if (-not (Test-Path $TempRoot)) {
    New-Item -ItemType Directory -Path $TempRoot -Force | Out-Null
}

$runRoot = Join-Path $TempRoot ([Guid]::NewGuid().ToString("N"))
$scaffoldRepo = Join-Path $runRoot "consumer"
New-Item -ItemType Directory -Path $scaffoldRepo -Force | Out-Null

try {
    $originalKenshiPath = $env:KENSHI_PATH
    $originalKenshiDefaultPath = $env:KENSHI_DEFAULT_PATH
    $env:KENSHI_PATH = ""
    $env:KENSHI_DEFAULT_PATH = ""

    & $initScript `
        -RepoDir $scaffoldRepo `
        -ModName "Phase9Consumer" `
        -DllName "Phase9Consumer.dll" `
        -ModFileName "Phase9Consumer.mod" `
        -KenshiPath "" `
        -WithHub `
        -HubNamespaceId "phase9.scaffold" `
        -HubNamespaceDisplayName "Phase9 Scaffold" `
        -HubModId "phase9_consumer" `
        -HubModDisplayName "Phase9 Consumer"

    $headerPath = Join-Path $scaffoldRepo "src\mod_hub_consumer_adapter.h"
    $sourcePath = Join-Path $scaffoldRepo "src\mod_hub_consumer_adapter.cpp"
    $singleTuPath = Join-Path $scaffoldRepo "samples\mod_hub_consumer_single_tu.cpp"
    Assert-Condition -Condition (Test-Path $headerPath) -Message "Missing scaffold header: $headerPath"
    Assert-Condition -Condition (Test-Path $sourcePath) -Message "Missing scaffold source: $sourcePath"
    Assert-Condition -Condition (-not (Test-Path $singleTuPath)) -Message "Default scaffold should stay unchanged unless -WithHubSingleTuSample is requested."

    $header = Get-Content -Path $headerPath -Raw
    $source = Get-Content -Path $sourcePath -Raw

    Assert-Condition -Condition ($header.Contains("ModHubConsumerAdapter_ShouldCreateLocalTab")) -Message "Header missing fallback helper declaration."
    Assert-Condition -Condition ($header.Contains("ModHubConsumerAdapter_OnStartup")) -Message "Header missing startup wiring declaration."
    Assert-Condition -Condition ($source.Contains("config.table_registration = &kRegistration;")) -Message "Source missing table-registration helper wiring."
    Assert-Condition -Condition ($source.Contains("ModHubConsumerAdapter_OnOptionsWindowInit")) -Message "Source missing options-window-init wiring."
    Assert-Condition -Condition ($source.Contains("ModHubConsumerAdapter_ShouldCreateLocalTab")) -Message "Source missing duplicate-safe local fallback helper."
    Assert-Condition -Condition ($source.Contains("ModHubConsumerAdapter_UseHubUi")) -Message "Source missing UseHubUi helper."

    Assert-Condition -Condition (-not $source.Contains("src/hub_")) -Message "Scaffold should not reference hub internals (src/hub_*)."
    Assert-Condition -Condition (-not $source.Contains('#include "hub_')) -Message "Scaffold should not include hub internal headers."

    $cl = Get-Command "cl.exe" -ErrorAction SilentlyContinue
    if ($null -ne $cl) {
        $objPath = Join-Path $runRoot "mod_hub_consumer_adapter.obj"
        $includePath = Join-Path $RepoRoot "include"
        Push-Location (Join-Path $scaffoldRepo "src")
        try {
            & $cl.Source /nologo /c /EHsc /I"$includePath" "mod_hub_consumer_adapter.cpp" /Fo"$objPath" | Out-Null
            Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "cl.exe compile smoke check failed."
            Assert-Condition -Condition (Test-Path $objPath) -Message "Expected object file was not produced by compile smoke check."
        }
        finally {
            Pop-Location
        }
    }

    Write-Host "PASS"
}
finally {
    if ($null -ne $originalKenshiPath) {
        $env:KENSHI_PATH = $originalKenshiPath
    }
    else {
        Remove-Item -Path Env:KENSHI_PATH -ErrorAction SilentlyContinue
    }

    if ($null -ne $originalKenshiDefaultPath) {
        $env:KENSHI_DEFAULT_PATH = $originalKenshiDefaultPath
    }
    else {
        Remove-Item -Path Env:KENSHI_DEFAULT_PATH -ErrorAction SilentlyContinue
    }

    if (Test-Path $runRoot) {
        Remove-Item -Path $runRoot -Recurse -Force
    }
}
