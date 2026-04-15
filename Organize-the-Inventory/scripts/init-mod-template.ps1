# Local wrapper: uses shared build-script helpers and the SDK scaffold when hub flags are requested.
param(
    [string]$RepoDir = "",
    [string]$ModName = "",
    [string]$DllName = "",
    [string]$ModFileName = "",
    [string]$ConfigFileName = "RE_Kenshi.json",
    [string]$KenshiPath = "",
    [switch]$WithHub,
    [switch]$WithHubSingleTuSample,
    [string]$HubNamespaceId = "",
    [string]$HubNamespaceDisplayName = "",
    [string]$HubModId = "",
    [string]$HubModDisplayName = "",
    [string]$HubSettingsManifest = "",
    [string[]]$HubBoolSetting = @()
)

$ErrorActionPreference = "Stop"
$ScriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $PSCommandPath }
$LocalRepoDir = if ($ScriptDir) { Split-Path -Parent $ScriptDir } else { (Get-Location).Path }
if (-not $RepoDir) { $RepoDir = $LocalRepoDir }
$env:KENSHI_REPO_DIR = $RepoDir
$BoundArguments = @{}
foreach ($entry in $PSBoundParameters.GetEnumerator()) {
    $BoundArguments[$entry.Key] = $entry.Value
}
$BoundArguments["RepoDir"] = $RepoDir
$CommonScript = Join-Path $LocalRepoDir "tools\build-scripts\kenshi-common.ps1"
$DefaultModTemplatePath = Join-Path $LocalRepoDir "tools\build-scripts\templates\default.mod"
$HubTemplateRoot = Join-Path $LocalRepoDir "tools\mod-hub-sdk\scripts\templates"

if (-not (Test-Path $CommonScript)) {
    Write-Host "ERROR: Shared helper not found: $CommonScript" -ForegroundColor Red
    Write-Host "Sync tools\build-scripts from the shared repo and retry." -ForegroundColor Yellow
    exit 1
}

if (-not (Test-Path $DefaultModTemplatePath)) {
    Write-Host "ERROR: Missing shared mod template: $DefaultModTemplatePath" -ForegroundColor Red
    Write-Host "Sync tools\build-scripts from the shared repo and retry." -ForegroundColor Yellow
    exit 1
}

. $CommonScript

function Initialize-LocalModTemplate {
    param(
        [Parameter(Mandatory = $true)][hashtable]$EffectiveBoundArguments,
        [Parameter(Mandatory = $true)][string]$EffectiveRepoDir,
        [string]$EffectiveModName = "",
        [string]$EffectiveDllName = "",
        [string]$EffectiveModFileName = "",
        [string]$EffectiveConfigFileName = "RE_Kenshi.json",
        [string]$EffectiveKenshiPath = ""
    )

    $resolved = Resolve-KenshiBuildContext `
        -BoundParameters $EffectiveBoundArguments `
        -RepoDir $EffectiveRepoDir `
        -ModName $EffectiveModName `
        -DllName $EffectiveDllName `
        -ModFileName $EffectiveModFileName `
        -ConfigFileName $EffectiveConfigFileName `
        -KenshiPath $EffectiveKenshiPath

    $modTemplateDir = $resolved.ModDir
    if (-not (Test-Path $modTemplateDir)) {
        New-Item -ItemType Directory -Path $modTemplateDir -Force | Out-Null
        Write-Host "Created mod template folder: $modTemplateDir" -ForegroundColor Gray
    }

    $destModPath = Join-Path $modTemplateDir $resolved.ModFileName
    try {
        Ensure-ModFile -Path $destModPath -TemplatePath $DefaultModTemplatePath
    }
    catch {
        Write-Host "ERROR: $_" -ForegroundColor Red
        exit 1
    }

    $reKenshiJsonPath = Join-Path $modTemplateDir $resolved.ConfigFileName
    Ensure-ReKenshiJson -Path $reKenshiJsonPath -PluginDllName $resolved.DllName

    $modConfigPath = Join-Path $modTemplateDir "mod-config.json"
    if (-not (Test-Path $modConfigPath)) {
        $modConfig = @{
            enabled = $true
            pause_debounce_ms = 2000
            debug_log_transitions = $false
        }
        $modConfig | ConvertTo-Json -Depth 4 | Set-Content -Path $modConfigPath
        Write-Host "Created missing mod-config.json: $modConfigPath" -ForegroundColor Gray
    }

    return $resolved
}

$shouldUseHubScaffold = $WithHub -or
    $WithHubSingleTuSample -or
    (-not [string]::IsNullOrWhiteSpace($HubNamespaceId)) -or
    (-not [string]::IsNullOrWhiteSpace($HubNamespaceDisplayName)) -or
    (-not [string]::IsNullOrWhiteSpace($HubModId)) -or
    (-not [string]::IsNullOrWhiteSpace($HubModDisplayName)) -or
    (-not [string]::IsNullOrWhiteSpace($HubSettingsManifest)) -or
    ($HubBoolSetting.Count -gt 0)

if (-not $shouldUseHubScaffold) {
    [void](Initialize-LocalModTemplate `
        -EffectiveBoundArguments $BoundArguments `
        -EffectiveRepoDir $RepoDir `
        -EffectiveModName $ModName `
        -EffectiveDllName $DllName `
        -EffectiveModFileName $ModFileName `
        -EffectiveConfigFileName $ConfigFileName `
        -EffectiveKenshiPath $KenshiPath)
    exit 0
}

if (-not (Test-Path $HubTemplateRoot)) {
    Write-Host "ERROR: Mod Hub scaffold templates not found under tools\mod-hub-sdk." -ForegroundColor Red
    Write-Host "Initialize the tools/mod-hub-sdk submodule or rerun without hub flags." -ForegroundColor Yellow
    exit 1
}

function Assert-HubBoolSettingIdentifier {
    param(
        [Parameter(Mandatory = $true)][string]$Value
    )

    if (-not $Value) {
        throw "Hub bool setting identifiers must not be empty."
    }

    if ($Value -notmatch '^[A-Za-z_][A-Za-z0-9_]*$') {
        throw "Invalid -HubBoolSetting value '$Value'. Use C-style identifiers only (letters, digits, underscore; cannot start with a digit)."
    }
}

function Convert-HubTokenToWords {
    param(
        [Parameter(Mandatory = $true)][string]$Value
    )

    $normalized = $Value -creplace '([a-z0-9])([A-Z])', '$1 $2'
    $normalized = $normalized -replace '[_\s]+', ' '
    $normalized = $normalized.Trim()
    if (-not $normalized) {
        return @()
    }

    return @($normalized -split '\s+')
}

function Convert-HubTokenToDisplayName {
    param(
        [Parameter(Mandatory = $true)][string]$Value
    )

    $words = Convert-HubTokenToWords -Value $Value
    if ($words.Count -eq 0) {
        return "Setting"
    }

    $displayWords = foreach ($word in $words) {
        if ($word.Length -eq 1) {
            $word.ToUpperInvariant()
        }
        else {
            $word.Substring(0, 1).ToUpperInvariant() + $word.Substring(1).ToLowerInvariant()
        }
    }

    return ($displayWords -join " ")
}

function Convert-HubTokenToPascalCase {
    param(
        [Parameter(Mandatory = $true)][string]$Value
    )

    $words = Convert-HubTokenToWords -Value $Value
    if ($words.Count -eq 0) {
        return "Setting"
    }

    $segments = foreach ($word in $words) {
        if ($word.Length -eq 1) {
            $word.ToUpperInvariant()
        }
        else {
            $word.Substring(0, 1).ToUpperInvariant() + $word.Substring(1)
        }
    }

    return ($segments -join "")
}

function New-HubBoolScaffoldSections {
    param(
        [string[]]$RequestedSettingIds = @()
    )

    $settingIds = @()
    if ($RequestedSettingIds.Count -gt 0) {
        $settingIds = $RequestedSettingIds
    }
    else {
        $settingIds = @("enabled")
    }

    $settings = New-Object System.Collections.Generic.List[object]
    $seenIds = @{}
    $seenFunctionSuffixes = @{}
    for ($index = 0; $index -lt $settingIds.Count; ++$index) {
        $settingId = [string]$settingIds[$index]
        Assert-HubBoolSettingIdentifier -Value $settingId

        $normalizedKey = $settingId.ToLowerInvariant()
        if ($seenIds.ContainsKey($normalizedKey)) {
            throw "Duplicate -HubBoolSetting value '$settingId'."
        }
        $seenIds[$normalizedKey] = $true

        $functionSuffix = Convert-HubTokenToPascalCase -Value $settingId
        $functionKey = $functionSuffix.ToLowerInvariant()
        if ($seenFunctionSuffixes.ContainsKey($functionKey)) {
            throw "Bool setting '$settingId' collides with another generated function name."
        }
        $seenFunctionSuffixes[$functionKey] = $true

        $displayName = Convert-HubTokenToDisplayName -Value $settingId
        $defaultValue = if ($index -eq 0) { 1 } else { 0 }

        $settings.Add([pscustomobject]@{
                SettingId = $settingId
                FieldName = $settingId
                DisplayName = $displayName
                FunctionSuffix = $functionSuffix
                ConstantName = "kBoolSetting$functionSuffix"
                Description = "Generated bool setting for $displayName."
                DefaultValue = $defaultValue
            })
    }

    $stateFields = ($settings | ForEach-Object {
            "    int32_t $($_.FieldName);"
        }) -join [Environment]::NewLine

    $stateInitializers = ($settings | ForEach-Object {
            "    $($_.DefaultValue),"
        }) -join [Environment]::NewLine

    $boolAccessorWrappers = ($settings | ForEach-Object {
@"
EMC_Result __cdecl Get$($_.FunctionSuffix)(void* user_data, int32_t* out_value)
{
    return emc::consumer::GetBoolFieldValue(user_data, out_value, &ExampleModState::$($_.FieldName));
}

EMC_Result __cdecl Set$($_.FunctionSuffix)(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    return emc::consumer::SetBoolFieldValueWithRollback(user_data, value, err_buf, err_buf_size, &ExampleModState::$($_.FieldName));
}
"@
        }) -join ([Environment]::NewLine + [Environment]::NewLine)

    $boolSettingDefs = ($settings | ForEach-Object {
@"
const EMC_BoolSettingDefV1 $($_.ConstantName) = {
    "$($_.SettingId)",
    "$($_.DisplayName)",
    "$($_.Description)",
    &g_state,
    &Get$($_.FunctionSuffix),
    &Set$($_.FunctionSuffix) };
"@
        }) -join ([Environment]::NewLine + [Environment]::NewLine)

    $boolRowEntries = ($settings | ForEach-Object {
            '    {{ emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, "{0}", &{1}, 0, 0 }},' -f $_.SettingId, $_.ConstantName
        }) -join [Environment]::NewLine

    return [pscustomobject]@{
        StateFields = $stateFields
        StateInitializers = $stateInitializers
        Accessors = $boolAccessorWrappers
        SettingDefs = $boolSettingDefs
        RowEntries = $boolRowEntries
    }
}

function Expand-HubBoolSettingValues {
    param(
        [string[]]$Values = @()
    )

    $expanded = New-Object System.Collections.Generic.List[string]
    foreach ($value in $Values) {
        if ($null -eq $value) {
            continue
        }

        foreach ($token in ($value -split ',')) {
            $trimmed = $token.Trim()
            if ($trimmed.Length -gt 0) {
                [void]$expanded.Add($trimmed)
            }
        }
    }

    return ,$expanded.ToArray()
}

function Get-HubBoolSettingsFromManifest {
    param(
        [string]$Path = ""
    )

    if (-not $Path) {
        return @()
    }

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Hub settings manifest not found: $Path"
    }

    try {
        $rawManifest = Get-Content -LiteralPath $Path -Raw
        $manifest = ConvertFrom-Json -InputObject $rawManifest
    }
    catch {
        throw "Failed to parse -HubSettingsManifest '$Path': $($_.Exception.Message)"
    }

    if ($null -eq $manifest) {
        return @()
    }

    $property = $manifest.PSObject.Properties['bool_settings']
    if ($null -eq $property) {
        return @()
    }

    $rawValue = $property.Value
    if ($null -eq $rawValue) {
        return @()
    }

    $boolSettings = New-Object System.Collections.Generic.List[string]
    if ($rawValue -is [System.Array]) {
        foreach ($item in $rawValue) {
            if ($null -ne $item) {
                [void]$boolSettings.Add([string]$item)
            }
        }
    }
    else {
        [void]$boolSettings.Add([string]$rawValue)
    }

    return ,$boolSettings.ToArray()
}

$resolved = Initialize-LocalModTemplate `
    -EffectiveBoundArguments $BoundArguments `
    -EffectiveRepoDir $RepoDir `
    -EffectiveModName $ModName `
    -EffectiveDllName $DllName `
    -EffectiveModFileName $ModFileName `
    -EffectiveConfigFileName $ConfigFileName `
    -EffectiveKenshiPath $KenshiPath

$safeModToken = ($resolved.ModName.ToLowerInvariant() -replace '[^a-z0-9]+', '_').Trim('_')
if (-not $safeModToken) { $safeModToken = "mod" }

if (-not $HubNamespaceId) { $HubNamespaceId = "example.$safeModToken" }
if (-not $HubNamespaceDisplayName) { $HubNamespaceDisplayName = "$($resolved.ModName) Namespace" }
if (-not $HubModId) { $HubModId = $safeModToken }
if (-not $HubModDisplayName) { $HubModDisplayName = $resolved.ModName }

$requestedHubBoolSettings = New-Object System.Collections.Generic.List[string]
foreach ($settingId in (Get-HubBoolSettingsFromManifest -Path $HubSettingsManifest)) {
    [void]$requestedHubBoolSettings.Add($settingId)
}
foreach ($settingId in $HubBoolSetting) {
    [void]$requestedHubBoolSettings.Add($settingId)
}

$HubBoolSetting = Expand-HubBoolSettingValues -Values $requestedHubBoolSettings.ToArray()
$boolSections = New-HubBoolScaffoldSections -RequestedSettingIds $HubBoolSetting

$srcDir = Join-Path $RepoDir "src"
if (-not (Test-Path $srcDir)) {
    New-Item -ItemType Directory -Path $srcDir -Force | Out-Null
}

$templateSpecs = @(
    @{
        TemplatePath = Join-Path $HubTemplateRoot "mod_hub_consumer_adapter.h.template"
        DestinationPath = Join-Path $srcDir "mod_hub_consumer_adapter.h"
    },
    @{
        TemplatePath = Join-Path $HubTemplateRoot "mod_hub_consumer_adapter.cpp.template"
        DestinationPath = Join-Path $srcDir "mod_hub_consumer_adapter.cpp"
    }
)

if ($WithHubSingleTuSample) {
    $sampleDir = Join-Path $RepoDir "samples"
    if (-not (Test-Path $sampleDir)) {
        New-Item -ItemType Directory -Path $sampleDir -Force | Out-Null
    }

    $templateSpecs += @{
        TemplatePath = Join-Path $HubTemplateRoot "mod_hub_consumer_single_tu.cpp.template"
        DestinationPath = Join-Path $sampleDir "mod_hub_consumer_single_tu.cpp"
    }
}

$createdCount = 0
foreach ($spec in $templateSpecs) {
    if (-not (Test-Path $spec.TemplatePath)) {
        Write-Host "ERROR: Hub scaffold template not found: $($spec.TemplatePath)" -ForegroundColor Red
        exit 1
    }

    if (Test-Path $spec.DestinationPath) {
        Write-Host "Hub scaffold exists, skipping: $($spec.DestinationPath)" -ForegroundColor Yellow
        continue
    }

    $template = Get-Content -Path $spec.TemplatePath -Raw
    $content = $template.Replace("__NAMESPACE_ID__", $HubNamespaceId)
    $content = $content.Replace("__NAMESPACE_DISPLAY_NAME__", $HubNamespaceDisplayName)
    $content = $content.Replace("__MOD_ID__", $HubModId)
    $content = $content.Replace("__MOD_DISPLAY_NAME__", $HubModDisplayName)
    $content = $content.Replace("__BOOL_STATE_FIELDS__", $boolSections.StateFields)
    $content = $content.Replace("__BOOL_STATE_INITIALIZERS__", $boolSections.StateInitializers)
    $content = $content.Replace("__BOOL_ACCESSORS__", $boolSections.Accessors)
    $content = $content.Replace("__BOOL_SETTING_DEFS__", $boolSections.SettingDefs)
    $content = $content.Replace("__BOOL_ROW_ENTRIES__", $boolSections.RowEntries)
    $content | Set-Content -Path $spec.DestinationPath -NoNewline
    Write-Host "Created Hub scaffold: $($spec.DestinationPath)" -ForegroundColor Gray
    $createdCount += 1
}

if ($createdCount -eq 0) {
    Write-Host "Hub scaffold unchanged: all target files already exist." -ForegroundColor Yellow
}

exit 0
