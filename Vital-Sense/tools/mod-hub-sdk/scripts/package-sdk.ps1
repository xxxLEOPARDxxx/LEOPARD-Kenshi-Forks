param(
    [string]$RepoDir = "",
    [string]$ModName = "",
    [string]$OutDir = "",
    [string]$ZipName = "",
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"

function Get-SupportedHubApiVersions {
    param(
        [Parameter(Mandatory = $true)][string]$ApiHeaderPath
    )

    $versions = @()
    foreach ($line in Get-Content -Path $ApiHeaderPath) {
        if ($line -match '^\s*#define\s+EMC_HUB_API_VERSION_[0-9]+\s+\(\(uint32_t\)([0-9]+)u\)') {
            $versions += [int]$Matches[1]
        }
    }

    $versions = @($versions | Sort-Object -Unique)
    if ($versions.Count -eq 0) {
        throw "No EMC_HUB_API_VERSION_* defines found in: $ApiHeaderPath"
    }

    return $versions
}

function Get-ApiHeaderStringDefine {
    param(
        [Parameter(Mandatory = $true)][string]$ApiHeaderPath,
        [Parameter(Mandatory = $true)][string]$DefineName
    )

    foreach ($line in Get-Content -Path $ApiHeaderPath) {
        if ($line -match ('^\s*#define\s+' + [Regex]::Escape($DefineName) + '\s+"([^"]+)"')) {
            return $Matches[1]
        }
    }

    throw "Required $DefineName define not found in: $ApiHeaderPath"
}

function Get-DefaultHubScaffoldReplacements {
    $stateFields = @"
    int32_t enabled;
    EMC_KeybindValueV1 hotkey;
    int32_t count;
    float radius;
"@

    $stateInitializers = @"
    1,
    { 42, 0u },
    10,
    2.5f
"@

    $accessors = @"
EMC_Result __cdecl GetEnabled(void* user_data, int32_t* out_value)
{
    return emc::consumer::GetBoolFieldValue(user_data, out_value, &ExampleModState::enabled);
}

EMC_Result __cdecl SetEnabled(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
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
            updated.enabled = value != 0 ? 1 : 0;
        });
}

EMC_Result __cdecl GetHotkey(void* user_data, EMC_KeybindValueV1* out_value)
{
    return emc::consumer::GetFieldValue(user_data, out_value, &ExampleModState::hotkey);
}

EMC_Result __cdecl SetHotkey(void* user_data, EMC_KeybindValueV1 value, char* err_buf, uint32_t err_buf_size)
{
    return ApplyExampleModStateUpdate(
        user_data,
        err_buf,
        err_buf_size,
        [value](ExampleModState& updated) {
            updated.hotkey = value;
        });
}

EMC_Result __cdecl GetCount(void* user_data, int32_t* out_value)
{
    return emc::consumer::GetFieldValue(user_data, out_value, &ExampleModState::count);
}

EMC_Result __cdecl SetCount(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
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
            updated.count = value;
        });
}

EMC_Result __cdecl GetRadius(void* user_data, float* out_value)
{
    return emc::consumer::GetFieldValue(user_data, out_value, &ExampleModState::radius);
}

EMC_Result __cdecl SetRadius(void* user_data, float value, char* err_buf, uint32_t err_buf_size)
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
            updated.radius = value;
        });
}

EMC_Result __cdecl RefreshNow(void* user_data, char* err_buf, uint32_t err_buf_size)
{
    return emc::consumer::ActionNoopSuccess(user_data, err_buf, err_buf_size);
}
"@

    $settingDefs = @"
const EMC_BoolSettingDefV1 kBoolSettingEnabled = {
    "enabled",
    "Enabled",
    "Generated bool setting for Enabled.",
    &g_state,
    &GetEnabled,
    &SetEnabled };

const EMC_KeybindSettingDefV1 kKeybindSetting = {
    "hotkey",
    "Hotkey",
    "Primary feature hotkey",
    &g_state,
    &GetHotkey,
    &SetHotkey };

const EMC_IntSettingDefV1 kIntSetting = {
    "count",
    "Count",
    "Example integer setting",
    &g_state,
    0,
    100,
    5,
    &GetCount,
    &SetCount };

const EMC_FloatSettingDefV1 kFloatSetting = {
    "radius",
    "Radius",
    "Example float setting",
    &g_state,
    0.0f,
    10.0f,
    0.5f,
    EMC_FLOAT_DISPLAY_DECIMALS_DEFAULT,
    &GetRadius,
    &SetRadius };

const EMC_ActionRowDefV1 kActionRow = {
    "refresh_now",
    "Refresh now",
    "Re-sync values from runtime state",
    &g_state,
    EMC_ACTION_FORCE_REFRESH,
    &RefreshNow };
"@

    $rowEntries = @"
    { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kBoolSettingEnabled },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND, &kKeybindSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_INT, &kIntSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_FLOAT, &kFloatSetting },
    { emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION, &kActionRow }
"@

    return [ordered]@{
        "__BOOL_STATE_FIELDS__" = $stateFields
        "__BOOL_STATE_INITIALIZERS__" = $stateInitializers
        "__BOOL_ACCESSORS__" = $accessors
        "__BOOL_SETTING_DEFS__" = $settingDefs
        "__BOOL_ROW_ENTRIES__" = $rowEntries
    }
}

$ScriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $PSCommandPath }
if (-not $RepoDir) {
    $RepoDir = if ($ScriptDir) { Split-Path -Parent $ScriptDir } else { (Get-Location).Path }
}
$env:KENSHI_REPO_DIR = $RepoDir

if (-not $ModName) {
    if ($env:KENSHI_MOD_NAME) {
        $ModName = $env:KENSHI_MOD_NAME
    }
    elseif ($env:MOD_NAME) {
        $ModName = $env:MOD_NAME
    }
    else {
        $ModName = Split-Path -Leaf $RepoDir
    }
}

$ctx = [pscustomobject]@{
    RepoDir = $RepoDir
}

$apiHeaderPath = Join-Path $ctx.RepoDir "include\emc\mod_hub_api.h"
$clientHeaderPath = Join-Path $ctx.RepoDir "include\emc\mod_hub_client.h"
$consumerHelpersHeaderPath = Join-Path $ctx.RepoDir "include\emc\mod_hub_consumer_helpers.h"
$clientSourcePath = Join-Path $ctx.RepoDir "src\mod_hub_client.cpp"
$sdkDocPath = Join-Path $ctx.RepoDir "docs\mod-hub-sdk.md"
$sdkQuickstartDocPath = Join-Path $ctx.RepoDir "docs\mod-hub-sdk-quickstart.md"
$sampleHeaderTemplatePath = Join-Path $ctx.RepoDir "scripts\templates\mod_hub_consumer_adapter.h.template"
$sampleSourceTemplatePath = Join-Path $ctx.RepoDir "scripts\templates\mod_hub_consumer_adapter.cpp.template"
$singleTuSampleTemplatePath = Join-Path $ctx.RepoDir "scripts\templates\mod_hub_consumer_single_tu.cpp.template"

foreach ($requiredPath in @(
        $apiHeaderPath,
        $clientHeaderPath,
        $consumerHelpersHeaderPath,
        $clientSourcePath,
        $sdkDocPath,
        $sdkQuickstartDocPath,
        $sampleHeaderTemplatePath,
        $sampleSourceTemplatePath,
        $singleTuSampleTemplatePath)) {
    if (-not (Test-Path $requiredPath)) {
        Write-Host "ERROR: Required SDK asset missing: $requiredPath" -ForegroundColor Red
        exit 1
    }
}

$versionFile = Join-Path $ctx.RepoDir "VERSION"
if (-not $Version -and (Test-Path $versionFile)) {
    $Version = (Get-Content -Path $versionFile | Select-Object -First 1).Trim()
}
if (-not $Version) {
    $Version = "0.0.0"
}

if (-not $OutDir) {
    $OutDir = Join-Path $ctx.RepoDir "dist"
}
if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
}

if (-not $ZipName) {
    $ZipName = "$ModName-SDK-$Version.zip"
}
$zipPath = Join-Path $OutDir $ZipName

$stagingRoot = Join-Path $ctx.RepoDir ".packaging\$ModName-sdk"
if (Test-Path $stagingRoot) {
    Remove-Item -Path $stagingRoot -Recurse -Force
}

$bundleRootName = "$ModName-sdk-$Version"
$bundleRoot = Join-Path $stagingRoot $bundleRootName
$bundleIncludeDir = Join-Path $bundleRoot "include\emc"
$bundleSrcDir = Join-Path $bundleRoot "src"
$bundleDocsDir = Join-Path $bundleRoot "docs"
$bundleSampleDir = Join-Path $bundleRoot "samples\minimal"
$bundleSingleTuSampleDir = Join-Path $bundleRoot "samples\single-tu"

foreach ($dir in @($bundleIncludeDir, $bundleSrcDir, $bundleDocsDir, $bundleSampleDir, $bundleSingleTuSampleDir)) {
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
}

$bundleReadme = @'
# __MOD_NAME__ Mod Hub SDK

Start with `docs/mod-hub-sdk-quickstart.md`.

Preferred path:

1. Use `samples/minimal/mod_hub_consumer_adapter.h` and `samples/minimal/mod_hub_consumer_adapter.cpp` as the production starting point.
2. Treat `samples/single-tu/mod_hub_consumer_single_tu.cpp` as a reference-only sample.
3. Replace only the example row IDs you need.
4. Implement `PersistExampleModState(...)` as your local persistence seam; the shared helper-backed callbacks already cover validation and rollback.

Reference assets:

- Quick start: `docs/mod-hub-sdk-quickstart.md`
- Full reference: `docs/mod-hub-sdk.md`
- Minimal sample: `samples/minimal/mod_hub_consumer_adapter.cpp`
- Single-TU reference: `samples/single-tu/mod_hub_consumer_single_tu.cpp`
'@.Replace("__MOD_NAME__", $ModName)

Copy-Item -Path $apiHeaderPath -Destination (Join-Path $bundleIncludeDir "mod_hub_api.h") -Force
Copy-Item -Path $clientHeaderPath -Destination (Join-Path $bundleIncludeDir "mod_hub_client.h") -Force
Copy-Item -Path $consumerHelpersHeaderPath -Destination (Join-Path $bundleIncludeDir "mod_hub_consumer_helpers.h") -Force
Copy-Item -Path $clientSourcePath -Destination (Join-Path $bundleSrcDir "mod_hub_client.cpp") -Force
Copy-Item -Path $sdkDocPath -Destination (Join-Path $bundleDocsDir "mod-hub-sdk.md") -Force
Copy-Item -Path $sdkQuickstartDocPath -Destination (Join-Path $bundleDocsDir "mod-hub-sdk-quickstart.md") -Force
Set-Content -Path (Join-Path $bundleRoot "README.md") -Value $bundleReadme -NoNewline

$sampleTokens = [ordered]@{
    "__NAMESPACE_ID__" = "example.mod_hub"
    "__NAMESPACE_DISPLAY_NAME__" = "Example Mod Hub"
    "__MOD_ID__" = "example_consumer"
    "__MOD_DISPLAY_NAME__" = "Example Consumer"
}

$sampleHeaderContent = Get-Content -Path $sampleHeaderTemplatePath -Raw
$sampleSourceContent = Get-Content -Path $sampleSourceTemplatePath -Raw
$singleTuSampleContent = Get-Content -Path $singleTuSampleTemplatePath -Raw
foreach ($token in $sampleTokens.Keys) {
    $sampleHeaderContent = $sampleHeaderContent.Replace($token, $sampleTokens[$token])
    $sampleSourceContent = $sampleSourceContent.Replace($token, $sampleTokens[$token])
    $singleTuSampleContent = $singleTuSampleContent.Replace($token, $sampleTokens[$token])
}

$defaultHubReplacements = Get-DefaultHubScaffoldReplacements
foreach ($token in $defaultHubReplacements.Keys) {
    $sampleSourceContent = $sampleSourceContent.Replace($token, $defaultHubReplacements[$token])
    $singleTuSampleContent = $singleTuSampleContent.Replace($token, $defaultHubReplacements[$token])
}

Set-Content -Path (Join-Path $bundleSampleDir "mod_hub_consumer_adapter.h") -Value $sampleHeaderContent -NoNewline
Set-Content -Path (Join-Path $bundleSampleDir "mod_hub_consumer_adapter.cpp") -Value $sampleSourceContent -NoNewline
Set-Content -Path (Join-Path $bundleSingleTuSampleDir "mod_hub_consumer_single_tu.cpp") -Value $singleTuSampleContent -NoNewline

$supportedHubApiVersions = @(Get-SupportedHubApiVersions -ApiHeaderPath $apiHeaderPath)
$canonicalGetApiExport = Get-ApiHeaderStringDefine -ApiHeaderPath $apiHeaderPath -DefineName "EMC_MOD_HUB_GET_API_EXPORT_NAME"
$compatGetApiExport = Get-ApiHeaderStringDefine -ApiHeaderPath $apiHeaderPath -DefineName "EMC_MOD_HUB_GET_API_COMPAT_EXPORT_NAME"
$compatRemovalTarget = Get-ApiHeaderStringDefine -ApiHeaderPath $apiHeaderPath -DefineName "EMC_MOD_HUB_GET_API_COMPAT_REMOVAL_TARGET"
$sdkMetadata = [ordered]@{
    sdk_package_version = $Version
    supported_hub_api_versions = $supportedHubApiVersions
    default_hub_api_version = [int]$supportedHubApiVersions[0]
    export_contract = [ordered]@{
        canonical_get_api_export = $canonicalGetApiExport
        compatibility_get_api_exports = @($compatGetApiExport)
        compatibility_alias_removal_target = $compatRemovalTarget
    }
    assets = [ordered]@{
        bundle_readme = "README.md"
        api_header = "include/emc/mod_hub_api.h"
        client_header = "include/emc/mod_hub_client.h"
        client_source = "src/mod_hub_client.cpp"
        minimal_sample_header = "samples/minimal/mod_hub_consumer_adapter.h"
        minimal_sample_source = "samples/minimal/mod_hub_consumer_adapter.cpp"
        single_tu_sample_source = "samples/single-tu/mod_hub_consumer_single_tu.cpp"
        integration_doc = "docs/mod-hub-sdk.md"
        quickstart_doc = "docs/mod-hub-sdk-quickstart.md"
    }
}
$sdkMetadata | ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $bundleRoot "sdk-metadata.json")
Set-Content -Path (Join-Path $bundleRoot "VERSION") -Value $Version -NoNewline

if (Test-Path $zipPath) {
    Remove-Item -Path $zipPath -Force
}

Write-Host "Packaging SDK bundle: $bundleRoot" -ForegroundColor Yellow
Write-Host "Output:               $zipPath" -ForegroundColor Gray

Compress-Archive -Path $bundleRoot -DestinationPath $zipPath

Write-Host "SDK package created: $zipPath" -ForegroundColor Green
