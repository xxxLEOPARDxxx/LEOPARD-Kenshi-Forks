# Build and Deploy Script for Kenshi mod plugin
# This script builds the project and copies files to Kenshi mods directory on success

param(
    [string]$ModName = "",
    [string]$ProjectFileName = "",
    [string]$OutputSubdir = "",
    [string]$DllName = "",
    [string]$ModFileName = "",
    [string]$ConfigFileName = "RE_Kenshi.json",
    [string]$KenshiPath = "H:\SteamLibrary\steamapps\common\Kenshi",
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$PlatformToolset = "v100"
)

$ErrorActionPreference = "Stop"

# Get script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDir = Split-Path -Parent $ScriptDir
$LoadEnv = Join-Path $ScriptDir "load-env.ps1"
if (Test-Path $LoadEnv) {
    . $LoadEnv -RepoDir $ProjectDir
}

if (-not $PSBoundParameters.ContainsKey("KenshiPath") -and $env:KENSHI_PATH) {
    $KenshiPath = $env:KENSHI_PATH
}
if (-not $PSBoundParameters.ContainsKey("Configuration") -and $env:KENSHI_CONFIGURATION) {
    $Configuration = $env:KENSHI_CONFIGURATION
}
if (-not $PSBoundParameters.ContainsKey("Platform") -and $env:KENSHI_PLATFORM) {
    $Platform = $env:KENSHI_PLATFORM
}
if (-not $PSBoundParameters.ContainsKey("PlatformToolset") -and $env:KENSHI_PLATFORM_TOOLSET) {
    $PlatformToolset = $env:KENSHI_PLATFORM_TOOLSET
}
if (-not $ModName) {
    if ($env:KENSHI_MOD_NAME) {
        $ModName = $env:KENSHI_MOD_NAME
    } else {
        $ModName = Split-Path -Leaf $ProjectDir
    }
}
if (-not $ProjectFileName) {
    if ($env:KENSHI_PROJECT_FILE) {
        $ProjectFileName = $env:KENSHI_PROJECT_FILE
    } else {
        $ProjectFileName = "Wall-B-Gone.vcxproj"
    }
}
if (-not $OutputSubdir) {
    if ($env:KENSHI_OUTPUT_SUBDIR) {
        $OutputSubdir = $env:KENSHI_OUTPUT_SUBDIR
    } else {
        $OutputSubdir = "$Platform\$Configuration"
    }
}
if (-not $DllName) {
    if ($env:KENSHI_DLL_NAME) {
        $DllName = $env:KENSHI_DLL_NAME
    } else {
        $DllName = "$ModName.dll"
    }
}
if (-not $ModFileName) {
    if ($env:KENSHI_MOD_FILE_NAME) {
        $ModFileName = $env:KENSHI_MOD_FILE_NAME
    } else {
        $ModFileName = "$ModName.mod"
    }
}
if (-not $ConfigFileName -and $env:KENSHI_CONFIG_FILE_NAME) {
    $ConfigFileName = $env:KENSHI_CONFIG_FILE_NAME
}

$ProjectFile = Join-Path $ProjectDir $ProjectFileName
$OutputDir = Join-Path $ProjectDir $OutputSubdir
$DllPath = Join-Path $OutputDir $DllName
$ModDir = Join-Path $ProjectDir $ModName
$KenshiModPath = Join-Path $KenshiPath "mods\$ModName"

Write-Host "=== Kenshi Mod Build and Deploy Script ===" -ForegroundColor Cyan
Write-Host "Project: $ProjectFile" -ForegroundColor Gray
Write-Host "Output: $OutputDir" -ForegroundColor Gray
Write-Host "Kenshi Path: $KenshiPath" -ForegroundColor Gray
Write-Host "Mod Destination: $KenshiModPath" -ForegroundColor Gray
Write-Host ""

# Ensure env vars are set and point to the intended deps for CLI builds
$setupEnvPath = Join-Path $ScriptDir "setup_env.ps1"
$expectedDepsDir = $null
if (Test-Path $setupEnvPath) {
    $expectedDepsDir = (Select-String -Path $setupEnvPath -Pattern 'KENSHILIB_DEPS_DIR' | Select-Object -First 1).Line
    if ($expectedDepsDir -match '"([^"]+)"') {
        $expectedDepsDir = $matches[1]
    } else {
        $expectedDepsDir = $null
    }
    if (-not $expectedDepsDir) {
        $expectedDepsDir = (Select-String -Path $setupEnvPath -Pattern '^\$defaultDepsDir\s*=' | Select-Object -First 1).Line
        if ($expectedDepsDir -match '"([^"]+)"') {
            $expectedDepsDir = $matches[1]
        } else {
            $expectedDepsDir = $null
        }
    }
}

$envValid = $true
if (-not $env:KENSHILIB_DEPS_DIR -or -not $env:KENSHILIB_DIR -or -not $env:BOOST_INCLUDE_PATH) {
    $envValid = $false
} elseif (-not (Test-Path (Join-Path $env:KENSHILIB_DEPS_DIR "boost_1_60_0"))) {
    $envValid = $false
} elseif (-not (Test-Path (Join-Path $env:KENSHILIB_DEPS_DIR "KenshiLib"))) {
    $envValid = $false
} elseif ($expectedDepsDir -and ($env:KENSHILIB_DEPS_DIR -ne $expectedDepsDir)) {
    $envValid = $false
}

if (-not $envValid) {
    if (Test-Path $setupEnvPath) {
        . $setupEnvPath
    } else {
        Write-Host "ERROR: setup_env.ps1 not found and required env vars are missing or invalid." -ForegroundColor Red
        Write-Host "Expected at: $setupEnvPath" -ForegroundColor Yellow
        exit 1
    }
    if (-not (Test-Path (Join-Path $env:KENSHILIB_DEPS_DIR "boost_1_60_0")) -or -not (Test-Path (Join-Path $env:KENSHILIB_DEPS_DIR "KenshiLib"))) {
        Write-Host "ERROR: KENSHILIB_DEPS_DIR is invalid: $env:KENSHILIB_DEPS_DIR" -ForegroundColor Red
        exit 1
    }
}

# Check if project file exists
if (-not (Test-Path $ProjectFile)) {
    Write-Host "ERROR: Project file not found: $ProjectFile" -ForegroundColor Red
    exit 1
}

# Check if Kenshi directory exists
if (-not (Test-Path $KenshiPath)) {
    Write-Host "ERROR: Kenshi directory not found: $KenshiPath" -ForegroundColor Red
    Write-Host "Please update the KenshiPath parameter or create the directory." -ForegroundColor Yellow
    exit 1
}

# FIND MSBUILD using vswhere or standard paths
Write-Host "Locating MSBuild..." -ForegroundColor Gray
$MSBuildPath = $null

# Try finding vswhere
$vsWherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWherePath) {
    # Find latest MSBuild
    $latestVS = & $vsWherePath -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
    if ($latestVS) {
        $possiblePath = Join-Path $latestVS "MSBuild\Current\Bin\MSBuild.exe"
        if (Test-Path $possiblePath) { $MSBuildPath = $possiblePath }
        else {
            $possiblePath = Join-Path $latestVS "MSBuild\15.0\Bin\MSBuild.exe"
            if (Test-Path $possiblePath) { $MSBuildPath = $possiblePath }
        }
    }
}

# Fallback to PATH
if (-not $MSBuildPath) {
    if (Get-Command "msbuild" -ErrorAction SilentlyContinue) {
        $MSBuildPath = "msbuild"
    }
}

if (-not $MSBuildPath) {
    Write-Host "ERROR: MSBuild.exe not found! Please run this script from a Developer Command Prompt or ensure VS is installed." -ForegroundColor Red
    exit 1
}

Write-Host "Using MSBuild: $MSBuildPath" -ForegroundColor Gray

# Build the project
Write-Host "Building project..." -ForegroundColor Yellow
$buildArgs = @(
    $ProjectFile,
    "/t:Clean,Rebuild",
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform",
    "/p:PlatformToolset=$PlatformToolset",
    "/nologo",
    "/v:minimal"
)

try {
    $buildOutput = & $MSBuildPath $buildArgs 2>&1
    $buildSuccess = $LASTEXITCODE -eq 0

    if (-not $buildSuccess) {
        Write-Host "`nBUILD FAILED!" -ForegroundColor Red
        Write-Host $buildOutput
        exit 1
    }

    Write-Host "Build succeeded!" -ForegroundColor Green
} catch {
    Write-Host "ERROR: Build failed with exception: $_" -ForegroundColor Red
    exit 1
}

# Check if DLL was created
if (-not (Test-Path $DllPath)) {
    Write-Host "ERROR: DLL not found after build: $DllPath" -ForegroundColor Red
    exit 1
}

Write-Host "`nDeploying mod files..." -ForegroundColor Yellow

# Create mod directory if it doesn't exist
if (-not (Test-Path $KenshiModPath)) {
    New-Item -ItemType Directory -Path $KenshiModPath -Force | Out-Null
    Write-Host "Created mod directory: $KenshiModPath" -ForegroundColor Gray
}

# Copy mod files (Wall-B-Gone.mod, RE_Kenshi.json, etc.)
if (Test-Path $ModDir) {
    Copy-Item -Path "$ModDir\*" -Destination $KenshiModPath -Recurse -Force
    Write-Host "Copied mod files from: $ModDir" -ForegroundColor Gray
} else {
    Write-Host "WARNING: Mod directory not found: $ModDir" -ForegroundColor Yellow
    Write-Host "Only DLL will be copied." -ForegroundColor Yellow
}

# Copy DLL with verification
$DestDllPath = Join-Path $KenshiModPath $DllName
try {
    Copy-Item -Path $DllPath -Destination $DestDllPath -Force
} catch {
    Write-Host "ERROR: Failed to copy DLL. File might be in use (is Kenshi running?)" -ForegroundColor Red
    Write-Host "Details: $_" -ForegroundColor Red
    exit 1
}

# Verify timestamp to ensure it's the new file
$SourceTime = (Get-Item $DllPath).LastWriteTime
$DestTime = (Get-Item $DestDllPath).LastWriteTime

if ($SourceTime -ne $DestTime) {
    Write-Host "ERROR: Deployment failed! Destination file timestamp mismatch." -ForegroundColor Red
    Write-Host "Source: $SourceTime"
    Write-Host "Dest:   $DestTime"
    exit 1
}

Write-Host "Copied DLL: $DllPath -> $DestDllPath" -ForegroundColor Gray

# Update RE_Kenshi.json Plugins list in deploy directory
Write-Host "Updating RE_Kenshi.json Plugins list..." -ForegroundColor Yellow
$reKenshiJsonPath = Join-Path $KenshiModPath $ConfigFileName
if (Test-Path $reKenshiJsonPath) {
    try {
        $jsonContent = Get-Content -Path $reKenshiJsonPath | ConvertFrom-Json
if (-not ($jsonContent.PSObject.Properties.Name -contains 'Plugins')) {
            $jsonContent | Add-Member -MemberType NoteProperty -Name Plugins -Value @()
        } elseif ($jsonContent.Plugins -isnot [Array]) {
            $jsonContent.Plugins = @($jsonContent.Plugins)
        }
        $jsonContent.Plugins = @($DllName)
        $jsonContent | ConvertTo-Json -Depth 4 | Set-Content -Path $reKenshiJsonPath
        Write-Host "Successfully updated RE_Kenshi.json in deploy directory." -ForegroundColor Green
    } catch {
        Write-Host "ERROR: Failed to update RE_Kenshi.json. Details: $_" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "WARNING: RE_Kenshi.json not found in deploy directory. Skipping update." -ForegroundColor Yellow
}

# Verify deployment
Write-Host "`nVerifying deployment..." -ForegroundColor Yellow
$dllDeployed = Test-Path $DestDllPath
$modFileDeployed = Test-Path (Join-Path $KenshiModPath $ModFileName)
$jsonFileDeployed = Test-Path (Join-Path $KenshiModPath $ConfigFileName)

if ($dllDeployed) {
    Write-Host "[OK] $DllName" -ForegroundColor Green
} else {
    Write-Host "[FAIL] $DllName (MISSING!)" -ForegroundColor Red
}

if ($modFileDeployed) {
    Write-Host "[OK] $ModFileName" -ForegroundColor Green
} else {
    Write-Host "[WARN] $ModFileName (optional)" -ForegroundColor Yellow
}

if ($jsonFileDeployed) {
    Write-Host "[OK] $ConfigFileName" -ForegroundColor Green
} else {
    Write-Host "[WARN] $ConfigFileName (optional)" -ForegroundColor Yellow
}

Write-Host "`n=== Deployment Complete ===" -ForegroundColor Cyan
Write-Host "Mod location: $KenshiModPath" -ForegroundColor Gray
Write-Host "`nYou can now test the mod in Kenshi!" -ForegroundColor Green
