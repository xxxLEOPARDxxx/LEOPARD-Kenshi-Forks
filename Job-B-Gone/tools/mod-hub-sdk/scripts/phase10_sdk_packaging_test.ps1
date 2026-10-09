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

$packageScript = Join-Path $RepoRoot "scripts\package-sdk.ps1"
Assert-Condition -Condition (Test-Path $packageScript) -Message "Missing SDK package script: $packageScript"

if (-not $TempRoot) {
    $TempRoot = Join-Path ([System.IO.Path]::GetTempPath()) "emc_phase10_sdk_packaging"
}
if (-not (Test-Path $TempRoot)) {
    New-Item -ItemType Directory -Path $TempRoot -Force | Out-Null
}

$runRoot = Join-Path $TempRoot ([Guid]::NewGuid().ToString("N"))
$outDir = Join-Path $runRoot "dist"
$extractRoot = Join-Path $runRoot "extract"
$zipName = "Phase10-SDK.zip"
$sdkVersion = "10.0.0-test"

New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
New-Item -ItemType Directory -Path $outDir -Force | Out-Null

try {
    & $packageScript -RepoDir $RepoRoot -OutDir $outDir -Version $sdkVersion -ZipName $zipName

    $zipPath = Join-Path $outDir $zipName
    Assert-Condition -Condition (Test-Path $zipPath) -Message "SDK zip not created: $zipPath"

    Expand-Archive -Path $zipPath -DestinationPath $extractRoot -Force
    $bundleDir = Get-ChildItem -Path $extractRoot -Directory | Select-Object -First 1
    Assert-Condition -Condition ($null -ne $bundleDir) -Message "Extracted SDK bundle directory not found."

    $requiredRelPaths = @(
        "include\emc\mod_hub_api.h",
        "include\emc\mod_hub_client.h",
        "include\emc\mod_hub_consumer_helpers.h",
        "src\mod_hub_client.cpp",
        "samples\minimal\mod_hub_consumer_adapter.h",
        "samples\minimal\mod_hub_consumer_adapter.cpp",
        "samples\single-tu\mod_hub_consumer_single_tu.cpp",
        "docs\mod-hub-sdk.md",
        "docs\mod-hub-sdk-quickstart.md",
        "sdk-metadata.json",
        "VERSION"
    )

    foreach ($relPath in $requiredRelPaths) {
        $fullPath = Join-Path $bundleDir.FullName $relPath
        Assert-Condition -Condition (Test-Path $fullPath) -Message "Missing SDK asset: $fullPath"
    }

    $metadataPath = Join-Path $bundleDir.FullName "sdk-metadata.json"
    $metadata = Get-Content -Path $metadataPath -Raw | ConvertFrom-Json
    Assert-Condition -Condition ($metadata.sdk_package_version -eq $sdkVersion) -Message "sdk_package_version mismatch in metadata."
    Assert-Condition -Condition ($metadata.assets.integration_doc -eq "docs/mod-hub-sdk.md") -Message "Metadata integration_doc path mismatch."
    Assert-Condition -Condition ($metadata.assets.quickstart_doc -eq "docs/mod-hub-sdk-quickstart.md") -Message "Metadata quickstart_doc path mismatch."
    Assert-Condition -Condition ($metadata.assets.single_tu_sample_source -eq "samples/single-tu/mod_hub_consumer_single_tu.cpp") -Message "Metadata single_tu_sample_source path mismatch."
    Assert-Condition -Condition ($null -ne $metadata.export_contract) -Message "Metadata export_contract block is missing."
    Assert-Condition -Condition ($metadata.export_contract.canonical_get_api_export -eq "EMC_ModHub_GetApi") -Message "Metadata canonical_get_api_export mismatch."
    $compatExports = @($metadata.export_contract.compatibility_get_api_exports)
    Assert-Condition -Condition ($compatExports.Count -ge 1) -Message "Metadata compatibility_get_api_exports is empty."
    Assert-Condition -Condition ($compatExports -contains "EMC_ModHub_GetApi_v1_compat") -Message "Metadata compatibility alias export missing."
    Assert-Condition -Condition ($metadata.export_contract.compatibility_alias_removal_target -eq "v1.2.0") -Message "Metadata compatibility_alias_removal_target mismatch."

    $supported = @($metadata.supported_hub_api_versions)
    Assert-Condition -Condition ($supported.Count -ge 1) -Message "supported_hub_api_versions is empty."
    Assert-Condition -Condition ($supported -contains 1) -Message "supported_hub_api_versions must include Hub API version 1."

    $sampleSourcePath = Join-Path $bundleDir.FullName "samples\minimal\mod_hub_consumer_adapter.cpp"
    $singleTuSamplePath = Join-Path $bundleDir.FullName "samples\single-tu\mod_hub_consumer_single_tu.cpp"
    $sampleSource = Get-Content -Path $sampleSourcePath -Raw
    $singleTuSample = Get-Content -Path $singleTuSamplePath -Raw
    Assert-Condition -Condition (-not $sampleSource.Contains("src/hub_")) -Message "Sample should not reference hub internals (src/hub_*)."
    Assert-Condition -Condition (-not $sampleSource.Contains('#include "hub_')) -Message "Sample should not include hub internal headers."
    Assert-Condition -Condition (-not $singleTuSample.Contains("src/hub_")) -Message "Single-TU sample should not reference hub internals (src/hub_*)."
    Assert-Condition -Condition (-not $singleTuSample.Contains('#include "hub_')) -Message "Single-TU sample should not include hub internal headers."

    $cl = Get-Command "cl.exe" -ErrorAction SilentlyContinue
    if ($null -ne $cl) {
        $sampleObj = Join-Path $runRoot "sample.obj"
        $singleTuObj = Join-Path $runRoot "single_tu_sample.obj"
        $clientObj = Join-Path $runRoot "mod_hub_client.obj"
        $includePath = Join-Path $bundleDir.FullName "include"

        & $cl.Source /nologo /c /EHsc /I"$includePath" $sampleSourcePath /Fo"$sampleObj" | Out-Null
        Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "cl.exe failed to compile sample adapter."
        Assert-Condition -Condition (Test-Path $sampleObj) -Message "Sample object file missing after compile."

        & $cl.Source /nologo /c /EHsc /I"$includePath" $singleTuSamplePath /Fo"$singleTuObj" | Out-Null
        Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "cl.exe failed to compile single-TU sample."
        Assert-Condition -Condition (Test-Path $singleTuObj) -Message "Single-TU sample object file missing after compile."

        $clientSourcePath = Join-Path $bundleDir.FullName "src\mod_hub_client.cpp"
        & $cl.Source /nologo /c /EHsc /I"$includePath" $clientSourcePath /Fo"$clientObj" | Out-Null
        Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "cl.exe failed to compile mod_hub_client.cpp from SDK bundle."
        Assert-Condition -Condition (Test-Path $clientObj) -Message "Client helper object file missing after compile."
    }
    else {
        $gpp = Get-Command "g++" -ErrorAction SilentlyContinue
        Assert-Condition -Condition ($null -ne $gpp) -Message "No compiler found (neither cl.exe nor g++)."

        $sampleObj = Join-Path $runRoot "sample.o"
        $singleTuObj = Join-Path $runRoot "single_tu_sample.o"
        $clientObj = Join-Path $runRoot "mod_hub_client.o"
        $includePath = Join-Path $bundleDir.FullName "include"
        $clientSourcePath = Join-Path $bundleDir.FullName "src\mod_hub_client.cpp"

        & $gpp.Source "-std=c++11" "-I$includePath" "-c" $sampleSourcePath "-o" $sampleObj
        Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "g++ failed to compile sample adapter."
        Assert-Condition -Condition (Test-Path $sampleObj) -Message "Sample object file missing after g++ compile."

        & $gpp.Source "-std=c++11" "-I$includePath" "-c" $singleTuSamplePath "-o" $singleTuObj
        Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "g++ failed to compile single-TU sample."
        Assert-Condition -Condition (Test-Path $singleTuObj) -Message "Single-TU sample object file missing after g++ compile."

        & $gpp.Source "-std=c++11" "-I$includePath" "-c" $clientSourcePath "-o" $clientObj
        Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "g++ failed to compile mod_hub_client.cpp from SDK bundle."
        Assert-Condition -Condition (Test-Path $clientObj) -Message "Client helper object file missing after g++ compile."
    }

    Write-Host "PASS"
}
finally {
    if (Test-Path $runRoot) {
        Remove-Item -Path $runRoot -Recurse -Force
    }
}
