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
    $TempRoot = Join-Path ([System.IO.Path]::GetTempPath()) "emc_phase15_scaffold"
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
        -ModName "Phase15Consumer" `
        -DllName "Phase15Consumer.dll" `
        -ModFileName "Phase15Consumer.mod" `
        -KenshiPath "" `
        -WithHub `
        -WithHubSingleTuSample `
        -HubBoolSetting "show_overlay", "auto_save" `
        -HubNamespaceId "phase15.scaffold" `
        -HubNamespaceDisplayName "Phase15 Scaffold" `
        -HubModId "phase15_consumer" `
        -HubModDisplayName "Phase15 Consumer"

    $headerPath = Join-Path $scaffoldRepo "src\mod_hub_consumer_adapter.h"
    $sourcePath = Join-Path $scaffoldRepo "src\mod_hub_consumer_adapter.cpp"
    $singleTuPath = Join-Path $scaffoldRepo "samples\mod_hub_consumer_single_tu.cpp"
    Assert-Condition -Condition (Test-Path $headerPath) -Message "Missing scaffold header: $headerPath"
    Assert-Condition -Condition (Test-Path $sourcePath) -Message "Missing scaffold source: $sourcePath"
    Assert-Condition -Condition (Test-Path $singleTuPath) -Message "Missing single-TU scaffold sample: $singleTuPath"

    $header = Get-Content -Path $headerPath -Raw
    $source = Get-Content -Path $sourcePath -Raw
    $singleTu = Get-Content -Path $singleTuPath -Raw

    Assert-Condition -Condition ($header.Contains("ModHubConsumerAdapter_OnStartup")) -Message "Header missing startup wiring declaration."
    Assert-Condition -Condition ($source.Contains("config.table_registration = &kRegistration;")) -Message "Source missing table-registration helper wiring."
    Assert-Condition -Condition ($singleTu.Contains("ModHubSingleTuSample_OnStartup")) -Message "Single-TU sample missing startup wiring."
    Assert-Condition -Condition ($source.Contains('#include "emc/mod_hub_consumer_helpers.h"')) -Message "Scaffold adapter should include shared consumer helper header."
    Assert-Condition -Condition ($singleTu.Contains('#include "emc/mod_hub_consumer_helpers.h"')) -Message "Single-TU sample should include shared consumer helper header."
    Assert-Condition -Condition ($source.Contains("GetShowOverlay")) -Message "Scaffold adapter missing generated show_overlay getter."
    Assert-Condition -Condition ($source.Contains("SetAutoSave")) -Message "Scaffold adapter missing generated auto_save setter."
    Assert-Condition -Condition ($source.Contains('const EMC_BoolSettingDefV1 kBoolSettingShowOverlay')) -Message "Scaffold adapter missing generated bool setting definition."
    Assert-Condition -Condition (-not $source.Contains('const EMC_BoolSettingDefV1 kBoolSetting =')) -Message "Scaffold adapter should replace the legacy single bool definition when custom bool settings are requested."
    Assert-Condition -Condition ($singleTu.Contains("GetShowOverlay")) -Message "Single-TU sample missing generated show_overlay getter."
    Assert-Condition -Condition ($singleTu.Contains("SetAutoSave")) -Message "Single-TU sample missing generated auto_save setter."
    Assert-Condition -Condition ($singleTu.Contains("SetBoolFieldValueWithRollback")) -Message "Generated bool setter should delegate to shared rollback helper."

    $consumerHelpersPath = Join-Path $RepoRoot "include\emc\mod_hub_consumer_helpers.h"
    Assert-Condition -Condition (Test-Path $consumerHelpersPath) -Message "Missing shared consumer helper header: $consumerHelpersPath"
    $consumerHelpers = Get-Content -Path $consumerHelpersPath -Raw
    Assert-Condition -Condition ($consumerHelpers.Contains("previous_value")) -Message "Shared consumer helper should retain persistence rollback placeholder state."

    foreach ($text in @($source, $singleTu)) {
        Assert-Condition -Condition (-not $text.Contains("src/hub_")) -Message "Scaffold output should not reference hub internals (src/hub_*)."
        Assert-Condition -Condition (-not $text.Contains('#include "hub_')) -Message "Scaffold output should not include hub internal headers."
        Assert-Condition -Condition (-not $text.Contains("KenshiLib::")) -Message "Scaffold output should not depend on per-consumer Kenshi hook APIs."
        Assert-Condition -Condition (-not $text.Contains("AddHook(")) -Message "Scaffold output should not install consumer hook stubs."
        Assert-Condition -Condition (-not $text.Contains("GetRealAddress(")) -Message "Scaffold output should not require consumer RVA lookup."
    }

    $bash = Get-Command "bash" -ErrorAction SilentlyContinue
    if (($null -ne $bash) -and (-not $IsWindows)) {
        $shellInitScript = Join-Path $RepoRoot "scripts/init-mod-template.sh"
        $shellRepo = Join-Path $runRoot "shell-consumer"
        $shellManifestPath = Join-Path $runRoot "hub-settings.json"
        New-Item -ItemType Directory -Path $shellRepo -Force | Out-Null
        Set-Content -Path $shellManifestPath -Value @'
{
  "bool_settings": [
    "show_overlay",
    "auto_save"
  ]
}
'@

        & $bash.Source $shellInitScript `
            "--repo-dir" $shellRepo `
            "--mod-name" "Phase15ShellConsumer" `
            "--dll-name" "Phase15ShellConsumer.dll" `
            "--mod-file-name" "Phase15ShellConsumer.mod" `
            "--kenshi-path" "." `
            "--with-hub" `
            "--hub-settings-manifest" $shellManifestPath | Out-Null

        Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "Shell wrapper failed to generate Hub scaffold."

        $shellSourcePath = Join-Path $shellRepo "src/mod_hub_consumer_adapter.cpp"
        $shellConfigPath = Join-Path $shellRepo "Phase15ShellConsumer/RE_Kenshi.json"
        Assert-Condition -Condition (Test-Path $shellSourcePath) -Message "Shell wrapper missing scaffold adapter source."
        Assert-Condition -Condition (Test-Path $shellConfigPath) -Message "Shell wrapper should keep RE_Kenshi.json at the default path."

        $shellSource = Get-Content -Path $shellSourcePath -Raw
        Assert-Condition -Condition ($shellSource.Contains("GetShowOverlay")) -Message "Shell wrapper missing generated show_overlay getter."
        Assert-Condition -Condition ($shellSource.Contains("SetAutoSave")) -Message "Shell wrapper missing generated auto_save setter."
    }

    $includePath = Join-Path $RepoRoot "include"
    $helperSourcePath = Join-Path $RepoRoot "src\mod_hub_client.cpp"
    $cl = Get-Command "cl.exe" -ErrorAction SilentlyContinue
    if ($null -ne $cl) {
        $adapterObj = Join-Path $runRoot "mod_hub_consumer_adapter.obj"
        $singleTuObj = Join-Path $runRoot "mod_hub_consumer_single_tu.obj"
        $helperObj = Join-Path $runRoot "mod_hub_client.obj"

        Push-Location $scaffoldRepo
        try {
            & $cl.Source /nologo /c /EHsc /I"$includePath" "src\mod_hub_consumer_adapter.cpp" /Fo"$adapterObj" | Out-Null
            Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "cl.exe failed to compile scaffold adapter."
            Assert-Condition -Condition (Test-Path $adapterObj) -Message "Scaffold adapter object file missing."

            & $cl.Source /nologo /c /EHsc /I"$includePath" "samples\mod_hub_consumer_single_tu.cpp" /Fo"$singleTuObj" | Out-Null
            Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "cl.exe failed to compile single-TU scaffold sample."
            Assert-Condition -Condition (Test-Path $singleTuObj) -Message "Single-TU scaffold object file missing."
        }
        finally {
            Pop-Location
        }

        & $cl.Source /nologo /c /EHsc /I"$includePath" $helperSourcePath /Fo"$helperObj" | Out-Null
        Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "cl.exe failed to compile mod_hub_client.cpp."
        Assert-Condition -Condition (Test-Path $helperObj) -Message "Helper object file missing."
    }
    else {
        $gpp = Get-Command "g++" -ErrorAction SilentlyContinue
        Assert-Condition -Condition ($null -ne $gpp) -Message "No compiler found (neither cl.exe nor g++)."

        $adapterObj = Join-Path $runRoot "mod_hub_consumer_adapter.o"
        $singleTuObj = Join-Path $runRoot "mod_hub_consumer_single_tu.o"
        $helperObj = Join-Path $runRoot "mod_hub_client.o"

        & $gpp.Source "-std=c++11" "-I$includePath" "-c" $sourcePath "-o" $adapterObj
        Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "g++ failed to compile scaffold adapter."
        Assert-Condition -Condition (Test-Path $adapterObj) -Message "Scaffold adapter object file missing."

        & $gpp.Source "-std=c++11" "-I$includePath" "-c" $singleTuPath "-o" $singleTuObj
        Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "g++ failed to compile single-TU scaffold sample."
        Assert-Condition -Condition (Test-Path $singleTuObj) -Message "Single-TU scaffold object file missing."

        & $gpp.Source "-std=c++11" "-I$includePath" "-c" $helperSourcePath "-o" $helperObj
        Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "g++ failed to compile mod_hub_client.cpp."
        Assert-Condition -Condition (Test-Path $helperObj) -Message "Helper object file missing."
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
