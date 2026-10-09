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
        -HubKeybindSetting "toggle_overlay" `
        -HubIntSetting "max_markers" `
        -HubFloatSetting "search_radius" `
        -HubActionRow "refresh_cache" `
        -HubSelectSetting "palette" `
        -HubTextSetting "title" `
        -HubColorSetting "accent_color" `
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
    Assert-Condition -Condition ($source.Contains("GetToggleOverlay")) -Message "Scaffold adapter missing generated keybind getter."
    Assert-Condition -Condition ($source.Contains('const EMC_KeybindSettingDefV1 kKeybindSettingToggleOverlay')) -Message "Scaffold adapter missing generated keybind setting definition."
    Assert-Condition -Condition (-not $source.Contains('const EMC_KeybindSettingDefV1 kKeybindSetting =')) -Message "Scaffold adapter should replace the legacy single keybind definition when custom keybind settings are requested."
    Assert-Condition -Condition ($source.Contains("GetMaxMarkers")) -Message "Scaffold adapter missing generated int getter."
    Assert-Condition -Condition ($source.Contains('const EMC_IntSettingDefV1 kIntSettingMaxMarkers')) -Message "Scaffold adapter missing generated int setting definition."
    Assert-Condition -Condition (-not $source.Contains('const EMC_IntSettingDefV1 kIntSetting =')) -Message "Scaffold adapter should replace the legacy single int definition when custom int settings are requested."
    Assert-Condition -Condition ($source.Contains("GetSearchRadius")) -Message "Scaffold adapter missing generated float getter."
    Assert-Condition -Condition ($source.Contains('const EMC_FloatSettingDefV1 kFloatSettingSearchRadius')) -Message "Scaffold adapter missing generated float setting definition."
    Assert-Condition -Condition (-not $source.Contains('const EMC_FloatSettingDefV1 kFloatSetting =')) -Message "Scaffold adapter should replace the legacy single float definition when custom float settings are requested."
    Assert-Condition -Condition ($source.Contains("RefreshCache")) -Message "Scaffold adapter missing generated action callback."
    Assert-Condition -Condition ($source.Contains('const EMC_ActionRowDefV1 kActionRowRefreshCache')) -Message "Scaffold adapter missing generated action row definition."
    Assert-Condition -Condition (-not $source.Contains('const EMC_ActionRowDefV1 kActionRow =')) -Message "Scaffold adapter should replace the legacy single action definition when custom action rows are requested."
    Assert-Condition -Condition ($source.Contains("GetPalette")) -Message "Scaffold adapter missing generated select getter."
    Assert-Condition -Condition ($source.Contains("SetPalette")) -Message "Scaffold adapter missing generated select setter."
    Assert-Condition -Condition ($source.Contains('const EMC_SelectSettingDefV1 kSelectSettingPalette')) -Message "Scaffold adapter missing generated select setting definition."
    Assert-Condition -Condition ($source.Contains("GetTitle")) -Message "Scaffold adapter missing generated text getter."
    Assert-Condition -Condition ($source.Contains("SetTitle")) -Message "Scaffold adapter missing generated text setter."
    Assert-Condition -Condition ($source.Contains('const EMC_TextSettingDefV1 kTextSettingTitle')) -Message "Scaffold adapter missing generated text setting definition."
    Assert-Condition -Condition ($source.Contains("GetAccentColor")) -Message "Scaffold adapter missing generated color getter."
    Assert-Condition -Condition ($source.Contains("SetAccentColor")) -Message "Scaffold adapter missing generated color setter."
    Assert-Condition -Condition ($source.Contains('const EMC_ColorSettingDefV1 kColorSettingAccentColor')) -Message "Scaffold adapter missing generated color setting definition."
    Assert-Condition -Condition ($singleTu.Contains("GetShowOverlay")) -Message "Single-TU sample missing generated show_overlay getter."
    Assert-Condition -Condition ($singleTu.Contains("SetAutoSave")) -Message "Single-TU sample missing generated auto_save setter."
    Assert-Condition -Condition ($singleTu.Contains("GetToggleOverlay")) -Message "Single-TU sample missing generated keybind getter."
    Assert-Condition -Condition ($singleTu.Contains("GetMaxMarkers")) -Message "Single-TU sample missing generated int getter."
    Assert-Condition -Condition ($singleTu.Contains("GetSearchRadius")) -Message "Single-TU sample missing generated float getter."
    Assert-Condition -Condition ($singleTu.Contains("RefreshCache")) -Message "Single-TU sample missing generated action callback."
    Assert-Condition -Condition ($singleTu.Contains("GetPalette")) -Message "Single-TU sample missing generated select getter."
    Assert-Condition -Condition ($singleTu.Contains("SetTitle")) -Message "Single-TU sample missing generated text setter."
    Assert-Condition -Condition ($singleTu.Contains("SetAccentColor")) -Message "Single-TU sample missing generated color setter."
    Assert-Condition -Condition ($source.Contains("PersistExampleModState")) -Message "Scaffold adapter should include the local persistence seam helper."
    Assert-Condition -Condition ($source.Contains("ApplyExampleModStateUpdate")) -Message "Scaffold adapter should include the local state-update helper."
    Assert-Condition -Condition ($source.Contains("ValidateBoolValue")) -Message "Scaffold adapter should validate bool setters through the shared helper surface."
    Assert-Condition -Condition ($source.Contains("ValidateValueInRange<int32_t>")) -Message "Scaffold adapter should validate int setters through the shared helper surface."
    Assert-Condition -Condition ($source.Contains("ValidateValueInRange<float>")) -Message "Scaffold adapter should validate float setters through the shared helper surface."
    Assert-Condition -Condition ($singleTu.Contains("ApplyUpdateWithRollback")) -Message "Single-TU sample should delegate persistence rollback through the shared helper."
    Assert-Condition -Condition ($source.Contains("invalid_select_option")) -Message "Scaffold adapter should validate generated select rows."
    Assert-Condition -Condition ($source.Contains("text_too_long")) -Message "Scaffold adapter should bound generated text rows."
    Assert-Condition -Condition ($source.Contains("invalid_color_hex")) -Message "Scaffold adapter should validate generated color rows."

    $consumerHelpersPath = Join-Path $RepoRoot "include\emc\mod_hub_consumer_helpers.h"
    Assert-Condition -Condition (Test-Path $consumerHelpersPath) -Message "Missing shared consumer helper header: $consumerHelpersPath"
    $consumerHelpers = Get-Content -Path $consumerHelpersPath -Raw
    Assert-Condition -Condition ($consumerHelpers.Contains("previous_value")) -Message "Shared consumer helper should retain persistence rollback placeholder state."
    Assert-Condition -Condition ($consumerHelpers.Contains("ValidateBoolValue")) -Message "Shared consumer helper should expose bool validation."
    Assert-Condition -Condition ($consumerHelpers.Contains("ValidateValueInRange")) -Message "Shared consumer helper should expose numeric range validation."
    Assert-Condition -Condition ($consumerHelpers.Contains("ApplyUpdateWithRollback")) -Message "Shared consumer helper should expose the common apply/persist/rollback transaction helper."

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
  ],
  "keybind_settings": [
    "toggle_overlay"
  ],
  "int_settings": [
    "max_markers"
  ],
  "float_settings": [
    "search_radius"
  ],
  "action_rows": [
    "refresh_cache"
  ],
  "select_settings": [
    "palette"
  ],
  "text_settings": [
    "title"
  ],
  "color_settings": [
    "accent_color"
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
        Assert-Condition -Condition ($shellSource.Contains("GetToggleOverlay")) -Message "Shell wrapper missing generated keybind getter."
        Assert-Condition -Condition ($shellSource.Contains("GetMaxMarkers")) -Message "Shell wrapper missing generated int getter."
        Assert-Condition -Condition ($shellSource.Contains("GetSearchRadius")) -Message "Shell wrapper missing generated float getter."
        Assert-Condition -Condition ($shellSource.Contains("RefreshCache")) -Message "Shell wrapper missing generated action callback."
        Assert-Condition -Condition ($shellSource.Contains("GetPalette")) -Message "Shell wrapper missing generated select getter."
        Assert-Condition -Condition ($shellSource.Contains("SetTitle")) -Message "Shell wrapper missing generated text setter."
        Assert-Condition -Condition ($shellSource.Contains("SetAccentColor")) -Message "Shell wrapper missing generated color setter."
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
