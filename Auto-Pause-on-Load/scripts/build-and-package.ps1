# Build and package script for Auto-Pause-on-Load.
# This script builds the DLL and creates a zip package without deploying to Kenshi mods.

param(
    [string]$ModName = "",
    [string]$ProjectFileName = "",
    [string]$OutputSubdir = "",
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$PlatformToolset = "v100",
    [string]$DllName = "",
    [string]$ModFileName = "",
    [string]$ConfigFileName = "RE_Kenshi.json",
    [string]$OutDir = "",
    [string]$ZipName = "",
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoDir = Split-Path -Parent $ScriptDir
$LoadEnv = Join-Path $ScriptDir "load-env.ps1"
if (Test-Path $LoadEnv) {
    . $LoadEnv -RepoDir $RepoDir
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
        $ModName = Split-Path -Leaf $RepoDir
    }
}
if (-not $ProjectFileName) {
    if ($env:KENSHI_PROJECT_FILE) {
        $ProjectFileName = $env:KENSHI_PROJECT_FILE
    } else {
        $ProjectFileName = "$ModName.vcxproj"
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
if (-not $OutputSubdir) {
    if ($env:KENSHI_OUTPUT_SUBDIR) {
        $OutputSubdir = $env:KENSHI_OUTPUT_SUBDIR
    } else {
        $OutputSubdir = "$Platform\$Configuration"
    }
}

$ProjectFile = Join-Path $RepoDir $ProjectFileName
$OutputDir = Join-Path $RepoDir $OutputSubdir
$DllPath = Join-Path $OutputDir $DllName
$ModTemplateDir = Join-Path $RepoDir $ModName
$PackageScript = Join-Path $ScriptDir "package.ps1"
$StagingRoot = Join-Path $RepoDir ".packaging\$ModName"

Write-Host "=== $ModName Build + Package ===" -ForegroundColor Cyan
Write-Host "Project: $ProjectFile" -ForegroundColor Gray
Write-Host "Output:  $OutputDir" -ForegroundColor Gray
Write-Host "Staging: $StagingRoot" -ForegroundColor Gray

if (-not (Test-Path $ProjectFile)) {
    Write-Host "ERROR: Project file not found: $ProjectFile" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $ModTemplateDir)) {
    Write-Host "ERROR: Mod template folder not found: $ModTemplateDir" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $PackageScript)) {
    Write-Host "ERROR: package.ps1 not found at $PackageScript" -ForegroundColor Red
    exit 1
}

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

# Locate MSBuild
Write-Host "Locating MSBuild..." -ForegroundColor Gray
$MSBuildPath = $null
$vsWherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWherePath) {
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
if (-not $MSBuildPath) {
    if (Get-Command "msbuild" -ErrorAction SilentlyContinue) {
        $MSBuildPath = "msbuild"
    }
}
if (-not $MSBuildPath) {
    Write-Host "ERROR: MSBuild.exe not found! Please run from a Developer Command Prompt or ensure VS Build Tools are installed." -ForegroundColor Red
    exit 1
}
Write-Host "Using MSBuild: $MSBuildPath" -ForegroundColor Gray

# Build
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

if (-not (Test-Path $DllPath)) {
    Write-Host "ERROR: DLL not found after build: $DllPath" -ForegroundColor Red
    exit 1
}

# Stage package content in repo
Write-Host "Preparing package staging..." -ForegroundColor Yellow
if (Test-Path $StagingRoot) {
    Remove-Item -Path $StagingRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $StagingRoot -Force | Out-Null
Copy-Item -Path "$ModTemplateDir\*" -Destination $StagingRoot -Recurse -Force
Copy-Item -Path $DllPath -Destination (Join-Path $StagingRoot $DllName) -Force

& $PackageScript -ModName $ModName -SourceModPath $StagingRoot -DllName $DllName -ModFileName $ModFileName -ConfigFileName $ConfigFileName -OutDir $OutDir -ZipName $ZipName -Version $Version
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: package.ps1 failed" -ForegroundColor Red
    exit 1
}
