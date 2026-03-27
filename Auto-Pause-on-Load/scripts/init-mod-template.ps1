# Local wrapper: delegates to shared build scripts.
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
    [string[]]$HubBoolSetting = @(),
    [string[]]$HubKeybindSetting = @(),
    [string[]]$HubIntSetting = @(),
    [string[]]$HubFloatSetting = @(),
    [string[]]$HubActionRow = @()
)

$ErrorActionPreference = "Stop"
$ScriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $PSCommandPath }
$LocalRepoDir = if ($ScriptDir) { Split-Path -Parent $ScriptDir } else { (Get-Location).Path }
if (-not $RepoDir) { $RepoDir = $LocalRepoDir }
$env:KENSHI_REPO_DIR = $RepoDir
$SharedScript = Join-Path $LocalRepoDir "tools\build-scripts\init-mod-template.ps1"

if (-not (Test-Path $SharedScript)) {
    Write-Host "ERROR: Shared script not found: $SharedScript" -ForegroundColor Red
    Write-Host "Sync tools\\build-scripts from the shared repo and retry." -ForegroundColor Yellow
    exit 1
}

$Forward = @{}
foreach ($k in @('RepoDir','ModName','DllName','ModFileName','ConfigFileName','KenshiPath')) {
    if ($PSBoundParameters.ContainsKey($k)) { $Forward[$k] = (Get-Variable -Name $k -ValueOnly) }
}

if ($PSBoundParameters.ContainsKey('KenshiPath')) {
    $hadKenshiPath = Test-Path Env:KENSHI_PATH
    $hadKenshiDefaultPath = Test-Path Env:KENSHI_DEFAULT_PATH
    $previousKenshiPath = $env:KENSHI_PATH
    $previousKenshiDefaultPath = $env:KENSHI_DEFAULT_PATH

    $env:KENSHI_PATH = $KenshiPath
    $env:KENSHI_DEFAULT_PATH = $KenshiPath

    try {
        & $SharedScript @Forward
    }
    finally {
        if ($hadKenshiPath) {
            $env:KENSHI_PATH = $previousKenshiPath
        }
        else {
            Remove-Item -Path Env:KENSHI_PATH -ErrorAction SilentlyContinue
        }

        if ($hadKenshiDefaultPath) {
            $env:KENSHI_DEFAULT_PATH = $previousKenshiDefaultPath
        }
        else {
            Remove-Item -Path Env:KENSHI_DEFAULT_PATH -ErrorAction SilentlyContinue
        }
    }
}
else {
    & $SharedScript @Forward
}

if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (-not $WithHub) {
    exit 0
}

function Assert-HubSettingIdentifier {
    param(
        [Parameter(Mandatory = $true)][string]$Value
    )

    if (-not $Value) {
        throw "Hub setting identifiers must not be empty."
    }

    if ($Value -notmatch '^[A-Za-z_][A-Za-z0-9_]*$') {
        throw "Invalid Hub setting identifier '$Value'. Use C-style identifiers only (letters, digits, underscore; cannot start with a digit)."
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

function New-HubSettingSpecs {
    param(
        [string[]]$RequestedSettingIds = @(),
        [string[]]$DefaultSettingIds = @(),
        [Parameter(Mandatory = $true)][string]$KindLabel,
        [Parameter(Mandatory = $true)][string]$ConstantPrefix,
        [string]$LegacySingleConstantName = ""
    )

    $settings = New-Object System.Collections.Generic.List[object]
    $settingIds = if ($RequestedSettingIds.Count -gt 0) { @($RequestedSettingIds) } else { @($DefaultSettingIds) }
    $seenIds = @{}
    $seenFunctionSuffixes = @{}
    $useLegacySingleConstantName = ($RequestedSettingIds.Count -eq 0 -and $settingIds.Count -eq 1 -and -not [string]::IsNullOrWhiteSpace($LegacySingleConstantName))

    foreach ($rawSettingId in @($settingIds)) {
        $settingId = [string]$rawSettingId
        Assert-HubSettingIdentifier -Value $settingId

        $normalizedKey = $settingId.ToLowerInvariant()
        if ($seenIds.ContainsKey($normalizedKey)) {
            throw "Duplicate $KindLabel value '$settingId'."
        }
        $seenIds[$normalizedKey] = $true

        $functionSuffix = Convert-HubTokenToPascalCase -Value $settingId
        $functionKey = $functionSuffix.ToLowerInvariant()
        if ($seenFunctionSuffixes.ContainsKey($functionKey)) {
            throw "$KindLabel '$settingId' collides with another generated function name."
        }
        $seenFunctionSuffixes[$functionKey] = $true

        $displayName = Convert-HubTokenToDisplayName -Value $settingId
        $constantName = if ($useLegacySingleConstantName) { $LegacySingleConstantName } else { "$ConstantPrefix$functionSuffix" }

        $settings.Add([pscustomobject]@{
            SettingId = $settingId
            FieldName = $settingId
            DisplayName = $displayName
            FunctionSuffix = $functionSuffix
            ConstantName = $constantName
            IsDefaultScaffold = ($RequestedSettingIds.Count -eq 0)
        })
    }

    return $settings.ToArray()
}

function Join-HubScaffoldSections {
    param(
        [string[]]$Sections = @(),
        [string]$Separator = [Environment]::NewLine
    )

    $filtered = New-Object System.Collections.Generic.List[string]
    foreach ($section in $Sections) {
        if (-not [string]::IsNullOrWhiteSpace($section)) {
            [void]$filtered.Add($section.TrimEnd())
        }
    }

    if ($filtered.Count -eq 0) {
        return ""
    }

    return ($filtered -join $Separator)
}

function Assert-HubUniqueSettingIds {
    param(
        [object[]]$Groups = @()
    )

    $seenIds = @{}
    foreach ($group in $Groups) {
        if ($null -eq $group) {
            continue
        }

        foreach ($settingId in $group) {
            if ($null -eq $settingId) {
                continue
            }

            $normalizedKey = ([string]$settingId).ToLowerInvariant()
            if ($seenIds.ContainsKey($normalizedKey)) {
                throw "Duplicate Hub setting identifier '$settingId' across generated row kinds."
            }

            $seenIds[$normalizedKey] = $true
        }
    }
}

function New-HubBoolScaffoldSections {
    param(
        [string[]]$RequestedSettingIds = @()
    )

    $settings = @(New-HubSettingSpecs `
        -RequestedSettingIds $RequestedSettingIds `
        -DefaultSettingIds @("enabled") `
        -KindLabel "bool setting" `
        -ConstantPrefix "kBoolSetting")

    $stateFields = ($settings | ForEach-Object {
        "    int32_t $($_.FieldName);"
    }) -join [Environment]::NewLine

    $stateInitializerLines = New-Object System.Collections.Generic.List[string]
    for ($index = 0; $index -lt $settings.Count; ++$index) {
        $defaultValue = if ($index -eq 0) { 1 } else { 0 }
        [void]$stateInitializerLines.Add("    $defaultValue,")
    }
    $stateInitializers = $stateInitializerLines -join [Environment]::NewLine

    $boolAccessorWrappers = ($settings | ForEach-Object {
@"
EMC_Result __cdecl Get$($_.FunctionSuffix)(void* user_data, int32_t* out_value)
{
    return emc::consumer::GetBoolFieldValue(user_data, out_value, &ExampleModState::$($_.FieldName));
}

EMC_Result __cdecl Set$($_.FunctionSuffix)(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    const EMC_Result boolValidation = emc::consumer::ValidateBoolValue(value, err_buf, err_buf_size);
    if (boolValidation != EMC_OK)
    {
        return boolValidation;
    }

    return ApplyExampleModStateUpdate(
        user_data,
        err_buf,
        err_buf_size,
        [value](ExampleModState& updated) {
            updated.$($_.FieldName) = value != 0 ? 1 : 0;
        });
}
"@
    }) -join ([Environment]::NewLine + [Environment]::NewLine)

    $boolAccessors = $boolAccessorWrappers

    $boolSettingDefs = ($settings | ForEach-Object {
@"
const EMC_BoolSettingDefV1 $($_.ConstantName) = {
    "$($_.SettingId)",
    "$($_.DisplayName)",
    "Generated bool setting for $($_.DisplayName).",
    &g_state,
    &Get$($_.FunctionSuffix),
    &Set$($_.FunctionSuffix) };
"@
    }) -join ([Environment]::NewLine + [Environment]::NewLine)

    $boolRowEntries = ($settings | ForEach-Object {
        "    { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &$($_.ConstantName) },"
    }) -join [Environment]::NewLine

    return [pscustomobject]@{
        StateFields = $stateFields
        StateInitializers = $stateInitializers
        Accessors = $boolAccessors
        SettingDefs = $boolSettingDefs
        RowEntries = $boolRowEntries
    }
}

function New-HubKeybindScaffoldSections {
    param(
        [string[]]$RequestedSettingIds = @()
    )

    $settings = @(New-HubSettingSpecs `
        -RequestedSettingIds $RequestedSettingIds `
        -DefaultSettingIds @("hotkey") `
        -KindLabel "keybind setting" `
        -ConstantPrefix "kKeybindSetting" `
        -LegacySingleConstantName "kKeybindSetting")

    $stateFields = ($settings | ForEach-Object {
        "    EMC_KeybindValueV1 $($_.FieldName);"
    }) -join [Environment]::NewLine

    $stateInitializerLines = New-Object System.Collections.Generic.List[string]
    $settingDefsList = New-Object System.Collections.Generic.List[string]
    foreach ($setting in $settings) {
        $defaultInitializer = if ($setting.IsDefaultScaffold -and $setting.SettingId -eq "hotkey" -and $settings.Count -eq 1) {
            "{ 42, 0u }"
        }
        else {
            "{ EMC_KEY_UNBOUND, 0u }"
        }

        $description = if ($setting.IsDefaultScaffold -and $setting.SettingId -eq "hotkey" -and $settings.Count -eq 1) {
            "Primary feature hotkey"
        }
        else {
            "Generated keybind setting for $($setting.DisplayName)."
        }

        [void]$stateInitializerLines.Add("    $defaultInitializer,")
        [void]$settingDefsList.Add(@"
const EMC_KeybindSettingDefV1 $($setting.ConstantName) = {
    "$($setting.SettingId)",
    "$($setting.DisplayName)",
    "$description",
    &g_state,
    &Get$($setting.FunctionSuffix),
    &Set$($setting.FunctionSuffix) };
"@)
    }

    $accessors = ($settings | ForEach-Object {
@"
EMC_Result __cdecl Get$($_.FunctionSuffix)(void* user_data, EMC_KeybindValueV1* out_value)
{
    return emc::consumer::GetFieldValue(user_data, out_value, &ExampleModState::$($_.FieldName));
}

EMC_Result __cdecl Set$($_.FunctionSuffix)(void* user_data, EMC_KeybindValueV1 value, char* err_buf, uint32_t err_buf_size)
{
    return ApplyExampleModStateUpdate(
        user_data,
        err_buf,
        err_buf_size,
        [value](ExampleModState& updated) {
            updated.$($_.FieldName) = value;
        });
}
"@
    }) -join ([Environment]::NewLine + [Environment]::NewLine)

    $settingDefs = $settingDefsList -join ([Environment]::NewLine + [Environment]::NewLine)
    $rowEntries = ($settings | ForEach-Object {
        "    { emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND, &$($_.ConstantName) },"
    }) -join [Environment]::NewLine

    return [pscustomobject]@{
        StateFields = $stateFields
        StateInitializers = $stateInitializerLines -join [Environment]::NewLine
        Accessors = $accessors
        SettingDefs = $settingDefs
        RowEntries = $rowEntries
    }
}

function New-HubIntScaffoldSections {
    param(
        [string[]]$RequestedSettingIds = @()
    )

    $settings = @(New-HubSettingSpecs `
        -RequestedSettingIds $RequestedSettingIds `
        -DefaultSettingIds @("count") `
        -KindLabel "int setting" `
        -ConstantPrefix "kIntSetting" `
        -LegacySingleConstantName "kIntSetting")

    $stateFields = ($settings | ForEach-Object {
        "    int32_t $($_.FieldName);"
    }) -join [Environment]::NewLine

    $stateInitializerLines = New-Object System.Collections.Generic.List[string]
    $settingDefsList = New-Object System.Collections.Generic.List[string]
    foreach ($setting in $settings) {
        $defaultValue = if ($setting.IsDefaultScaffold -and $setting.SettingId -eq "count" -and $settings.Count -eq 1) { 10 } else { 0 }
        $minValue = 0
        $maxValue = 100
        $stepValue = if ($setting.IsDefaultScaffold -and $setting.SettingId -eq "count" -and $settings.Count -eq 1) { 5 } else { 1 }
        $description = if ($setting.IsDefaultScaffold -and $setting.SettingId -eq "count" -and $settings.Count -eq 1) {
            "Example integer setting"
        }
        else {
            "Generated integer setting for $($setting.DisplayName)."
        }

        [void]$stateInitializerLines.Add("    $defaultValue,")
        [void]$settingDefsList.Add(@"
const EMC_IntSettingDefV1 $($setting.ConstantName) = {
    "$($setting.SettingId)",
    "$($setting.DisplayName)",
    "$description",
    &g_state,
    $minValue,
    $maxValue,
    $stepValue,
    &Get$($setting.FunctionSuffix),
    &Set$($setting.FunctionSuffix) };
"@)
    }

    $accessors = ($settings | ForEach-Object {
@"
EMC_Result __cdecl Get$($_.FunctionSuffix)(void* user_data, int32_t* out_value)
{
    return emc::consumer::GetFieldValue(user_data, out_value, &ExampleModState::$($_.FieldName));
}

EMC_Result __cdecl Set$($_.FunctionSuffix)(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    const EMC_Result rangeValidation = emc::consumer::ValidateValueInRange<int32_t>(
        value,
        0,
        100,
        err_buf,
        err_buf_size);
    if (rangeValidation != EMC_OK)
    {
        return rangeValidation;
    }

    return ApplyExampleModStateUpdate(
        user_data,
        err_buf,
        err_buf_size,
        [value](ExampleModState& updated) {
            updated.$($_.FieldName) = value;
        });
}
"@
    }) -join ([Environment]::NewLine + [Environment]::NewLine)

    $settingDefs = $settingDefsList -join ([Environment]::NewLine + [Environment]::NewLine)
    $rowEntries = ($settings | ForEach-Object {
        "    { emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &$($_.ConstantName) },"
    }) -join [Environment]::NewLine

    return [pscustomobject]@{
        StateFields = $stateFields
        StateInitializers = $stateInitializerLines -join [Environment]::NewLine
        Accessors = $accessors
        SettingDefs = $settingDefs
        RowEntries = $rowEntries
    }
}

function New-HubFloatScaffoldSections {
    param(
        [string[]]$RequestedSettingIds = @()
    )

    $settings = @(New-HubSettingSpecs `
        -RequestedSettingIds $RequestedSettingIds `
        -DefaultSettingIds @("radius") `
        -KindLabel "float setting" `
        -ConstantPrefix "kFloatSetting" `
        -LegacySingleConstantName "kFloatSetting")

    $stateFields = ($settings | ForEach-Object {
        "    float $($_.FieldName);"
    }) -join [Environment]::NewLine

    $stateInitializerLines = New-Object System.Collections.Generic.List[string]
    $settingDefsList = New-Object System.Collections.Generic.List[string]
    foreach ($setting in $settings) {
        $defaultValue = if ($setting.IsDefaultScaffold -and $setting.SettingId -eq "radius" -and $settings.Count -eq 1) { "2.5f" } else { "0.0f" }
        $description = if ($setting.IsDefaultScaffold -and $setting.SettingId -eq "radius" -and $settings.Count -eq 1) {
            "Example float setting"
        }
        else {
            "Generated float setting for $($setting.DisplayName)."
        }

        [void]$stateInitializerLines.Add("    $defaultValue,")
        [void]$settingDefsList.Add(@"
const EMC_FloatSettingDefV1 $($setting.ConstantName) = {
    "$($setting.SettingId)",
    "$($setting.DisplayName)",
    "$description",
    &g_state,
    0.0f,
    10.0f,
    0.5f,
    EMC_FLOAT_DISPLAY_DECIMALS_DEFAULT,
    &Get$($setting.FunctionSuffix),
    &Set$($setting.FunctionSuffix) };
"@)
    }

    $accessors = ($settings | ForEach-Object {
@"
EMC_Result __cdecl Get$($_.FunctionSuffix)(void* user_data, float* out_value)
{
    return emc::consumer::GetFieldValue(user_data, out_value, &ExampleModState::$($_.FieldName));
}

EMC_Result __cdecl Set$($_.FunctionSuffix)(void* user_data, float value, char* err_buf, uint32_t err_buf_size)
{
    const EMC_Result rangeValidation = emc::consumer::ValidateValueInRange<float>(
        value,
        0.0f,
        10.0f,
        err_buf,
        err_buf_size);
    if (rangeValidation != EMC_OK)
    {
        return rangeValidation;
    }

    return ApplyExampleModStateUpdate(
        user_data,
        err_buf,
        err_buf_size,
        [value](ExampleModState& updated) {
            updated.$($_.FieldName) = value;
        });
}
"@
    }) -join ([Environment]::NewLine + [Environment]::NewLine)

    $settingDefs = $settingDefsList -join ([Environment]::NewLine + [Environment]::NewLine)
    $rowEntries = ($settings | ForEach-Object {
        "    { emc::MOD_HUB_CLIENT_SETTING_KIND_FLOAT, &$($_.ConstantName) },"
    }) -join [Environment]::NewLine

    return [pscustomobject]@{
        StateFields = $stateFields
        StateInitializers = $stateInitializerLines -join [Environment]::NewLine
        Accessors = $accessors
        SettingDefs = $settingDefs
        RowEntries = $rowEntries
    }
}

function New-HubActionScaffoldSections {
    param(
        [string[]]$RequestedSettingIds = @()
    )

    $settings = @(New-HubSettingSpecs `
        -RequestedSettingIds $RequestedSettingIds `
        -DefaultSettingIds @("refresh_now") `
        -KindLabel "action row" `
        -ConstantPrefix "kActionRow" `
        -LegacySingleConstantName "kActionRow")

    $accessors = ($settings | ForEach-Object {
@"
EMC_Result __cdecl $($_.FunctionSuffix)(void* user_data, char* err_buf, uint32_t err_buf_size)
{
    return emc::consumer::ActionNoopSuccess(user_data, err_buf, err_buf_size);
}
"@
    }) -join ([Environment]::NewLine + [Environment]::NewLine)

    $settingDefsList = New-Object System.Collections.Generic.List[string]
    foreach ($setting in $settings) {
        $description = if ($setting.IsDefaultScaffold -and $setting.SettingId -eq "refresh_now" -and $settings.Count -eq 1) {
            "Re-sync values from runtime state"
        }
        else {
            "Generated action row for $($setting.DisplayName)."
        }

        [void]$settingDefsList.Add(@"
const EMC_ActionRowDefV1 $($setting.ConstantName) = {
    "$($setting.SettingId)",
    "$($setting.DisplayName)",
    "$description",
    &g_state,
    EMC_ACTION_FORCE_REFRESH,
    &$($setting.FunctionSuffix) };
"@)
    }

    $settingDefs = $settingDefsList -join ([Environment]::NewLine + [Environment]::NewLine)
    $rowEntries = ($settings | ForEach-Object {
        "    { emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION, &$($_.ConstantName) },"
    }) -join [Environment]::NewLine

    return [pscustomobject]@{
        StateFields = ""
        StateInitializers = ""
        Accessors = $accessors
        SettingDefs = $settingDefs
        RowEntries = $rowEntries
    }
}

function Expand-HubSettingValues {
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

    return $expanded.ToArray()
}

function Get-HubSettingsFromManifestProperty {
    param(
        [string]$Path = "",
        [string]$PropertyName = ""
    )

    if (-not $Path -or -not $PropertyName) {
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

    $property = $manifest.PSObject.Properties[$PropertyName]
    if ($null -eq $property) {
        return @()
    }

    $rawValue = $property.Value
    if ($null -eq $rawValue) {
        return @()
    }

    $values = New-Object System.Collections.Generic.List[string]
    if ($rawValue -is [System.Array]) {
        foreach ($item in $rawValue) {
            if ($null -ne $item) {
                [void]$values.Add([string]$item)
            }
        }
    }
    else {
        [void]$values.Add([string]$rawValue)
    }

    return $values.ToArray()
}

$CommonScript = Join-Path $LocalRepoDir "tools\build-scripts\kenshi-common.ps1"
if (-not (Test-Path $CommonScript)) {
    Write-Host "ERROR: Shared helper not found: $CommonScript" -ForegroundColor Red
    exit 1
}

. $CommonScript
$resolved = Resolve-KenshiBuildContext `
    -BoundParameters $PSBoundParameters `
    -RepoDir $RepoDir `
    -ModName $ModName `
    -DllName $DllName `
    -ModFileName $ModFileName `
    -ConfigFileName $ConfigFileName `
    -KenshiPath $KenshiPath

$safeModToken = ($resolved.ModName.ToLowerInvariant() -replace '[^a-z0-9]+', '_').Trim('_')
if (-not $safeModToken) { $safeModToken = "mod" }

if (-not $HubNamespaceId) { $HubNamespaceId = "example.$safeModToken" }
if (-not $HubNamespaceDisplayName) { $HubNamespaceDisplayName = "$($resolved.ModName) Namespace" }
if (-not $HubModId) { $HubModId = $safeModToken }
if (-not $HubModDisplayName) { $HubModDisplayName = $resolved.ModName }

$requestedHubBoolSettings = @(
    (Get-HubSettingsFromManifestProperty -Path $HubSettingsManifest -PropertyName "bool_settings")
    $HubBoolSetting
)
$requestedHubKeybindSettings = @(
    (Get-HubSettingsFromManifestProperty -Path $HubSettingsManifest -PropertyName "keybind_settings")
    $HubKeybindSetting
)
$requestedHubIntSettings = @(
    (Get-HubSettingsFromManifestProperty -Path $HubSettingsManifest -PropertyName "int_settings")
    $HubIntSetting
)
$requestedHubFloatSettings = @(
    (Get-HubSettingsFromManifestProperty -Path $HubSettingsManifest -PropertyName "float_settings")
    $HubFloatSetting
)
$requestedHubActionRows = @(
    (Get-HubSettingsFromManifestProperty -Path $HubSettingsManifest -PropertyName "action_rows")
    $HubActionRow
)

$HubBoolSetting = @(Expand-HubSettingValues -Values $requestedHubBoolSettings)
$HubKeybindSetting = @(Expand-HubSettingValues -Values $requestedHubKeybindSettings)
$HubIntSetting = @(Expand-HubSettingValues -Values $requestedHubIntSettings)
$HubFloatSetting = @(Expand-HubSettingValues -Values $requestedHubFloatSettings)
$HubActionRow = @(Expand-HubSettingValues -Values $requestedHubActionRows)

Assert-HubUniqueSettingIds -Groups @(
    $HubBoolSetting,
    $HubKeybindSetting,
    $HubIntSetting,
    $HubFloatSetting,
    $HubActionRow)

$boolSections = New-HubBoolScaffoldSections -RequestedSettingIds $HubBoolSetting
$keybindSections = New-HubKeybindScaffoldSections -RequestedSettingIds $HubKeybindSetting
$intSections = New-HubIntScaffoldSections -RequestedSettingIds $HubIntSetting
$floatSections = New-HubFloatScaffoldSections -RequestedSettingIds $HubFloatSetting
$actionSections = New-HubActionScaffoldSections -RequestedSettingIds $HubActionRow

$combinedStateFields = Join-HubScaffoldSections -Sections @(
    $boolSections.StateFields,
    $keybindSections.StateFields,
    $intSections.StateFields,
    $floatSections.StateFields) -Separator ([Environment]::NewLine)

$combinedStateInitializers = Join-HubScaffoldSections -Sections @(
    $boolSections.StateInitializers,
    $keybindSections.StateInitializers,
    $intSections.StateInitializers,
    $floatSections.StateInitializers) -Separator ([Environment]::NewLine)

$combinedAccessors = Join-HubScaffoldSections -Sections @(
    $boolSections.Accessors,
    $keybindSections.Accessors,
    $intSections.Accessors,
    $floatSections.Accessors,
    $actionSections.Accessors) -Separator ([Environment]::NewLine + [Environment]::NewLine)

$combinedSettingDefs = Join-HubScaffoldSections -Sections @(
    $boolSections.SettingDefs,
    $keybindSections.SettingDefs,
    $intSections.SettingDefs,
    $floatSections.SettingDefs,
    $actionSections.SettingDefs) -Separator ([Environment]::NewLine + [Environment]::NewLine)

$combinedRowEntries = Join-HubScaffoldSections -Sections @(
    $boolSections.RowEntries,
    $keybindSections.RowEntries,
    $intSections.RowEntries,
    $floatSections.RowEntries,
    $actionSections.RowEntries) -Separator ([Environment]::NewLine)

$srcDir = Join-Path $RepoDir "src"
if (-not (Test-Path $srcDir)) {
    New-Item -ItemType Directory -Path $srcDir -Force | Out-Null
}

$templateSpecs = @(
    @{
        TemplatePath = Join-Path $LocalRepoDir "scripts\templates\mod_hub_consumer_adapter.h.template"
        DestinationPath = Join-Path $srcDir "mod_hub_consumer_adapter.h"
    },
    @{
        TemplatePath = Join-Path $LocalRepoDir "scripts\templates\mod_hub_consumer_adapter.cpp.template"
        DestinationPath = Join-Path $srcDir "mod_hub_consumer_adapter.cpp"
    }
)

if ($WithHubSingleTuSample) {
    $sampleDir = Join-Path $RepoDir "samples"
    if (-not (Test-Path $sampleDir)) {
        New-Item -ItemType Directory -Path $sampleDir -Force | Out-Null
    }

    $templateSpecs += @{
        TemplatePath = Join-Path $LocalRepoDir "scripts\templates\mod_hub_consumer_single_tu.cpp.template"
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
    $content = $content.Replace("__BOOL_STATE_FIELDS__", $combinedStateFields)
    $content = $content.Replace("__BOOL_STATE_INITIALIZERS__", $combinedStateInitializers)
    $content = $content.Replace("__BOOL_ACCESSORS__", $combinedAccessors)
    $content = $content.Replace("__BOOL_SETTING_DEFS__", $combinedSettingDefs)
    $content = $content.Replace("__BOOL_ROW_ENTRIES__", $combinedRowEntries)
    $content | Set-Content -Path $spec.DestinationPath -NoNewline
    Write-Host "Created Hub scaffold: $($spec.DestinationPath)" -ForegroundColor Gray
    $createdCount += 1
}

if ($createdCount -eq 0) {
    Write-Host "Hub scaffold unchanged: all target files already exist." -ForegroundColor Yellow
}

exit 0
