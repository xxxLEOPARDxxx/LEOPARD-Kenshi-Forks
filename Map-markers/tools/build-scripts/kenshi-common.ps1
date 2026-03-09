# Shared helper functions for Kenshi build scripts.

function Initialize-KenshiScriptContext {
    param(
        [Parameter(Mandatory = $true)][string]$InvocationPath,
        [string]$RepoDirOverride = ""
    )

    $scriptDir = Split-Path -Parent $InvocationPath

    if ($RepoDirOverride) {
        $repoDir = $RepoDirOverride
    } elseif ($env:KENSHI_REPO_DIR) {
        $repoDir = $env:KENSHI_REPO_DIR
    } else {
        $repoDir = Split-Path -Parent $scriptDir
    }

    $loadEnv = Join-Path $scriptDir "load-env.ps1"
    if (Test-Path $loadEnv) {
        . $loadEnv -RepoDir $repoDir
    }

    return [pscustomobject]@{
        ScriptDir = $scriptDir
        RepoDir = $repoDir
    }
}

function Get-KenshiEnvValue {
    param(
        [Parameter(Mandatory = $true)][string]$Name
    )

    $path = "env:$Name"
    if (-not (Test-Path $path)) {
        return ""
    }

    $value = (Get-Item -Path $path).Value
    if ($null -eq $value) {
        return ""
    }

    return [string]$value
}

function Resolve-KenshiValue {
    param(
        [string]$CurrentValue = "",
        [string]$EnvVar = "",
        [string]$DefaultValue = ""
    )

    if ($CurrentValue) {
        return $CurrentValue
    }

    if ($EnvVar) {
        $envValue = Get-KenshiEnvValue -Name $EnvVar
        if ($envValue) {
            return $envValue
        }
    }

    return $DefaultValue
}

function Resolve-KenshiBoundValue {
    param(
        [Parameter(Mandatory = $true)][hashtable]$BoundParameters,
        [Parameter(Mandatory = $true)][string]$ParameterName,
        [string]$CurrentValue = "",
        [string]$EnvVar = "",
        [string]$DefaultValue = ""
    )

    if ($BoundParameters.ContainsKey($ParameterName)) {
        if ($CurrentValue) {
            return $CurrentValue
        }
        return $DefaultValue
    }

    if ($EnvVar) {
        $envValue = Get-KenshiEnvValue -Name $EnvVar
        if ($envValue) {
            return $envValue
        }
    }

    if ($CurrentValue) {
        return $CurrentValue
    }

    return $DefaultValue
}

function Resolve-KenshiBuildContext {
    param(
        [Parameter(Mandatory = $true)][hashtable]$BoundParameters,
        [Parameter(Mandatory = $true)][string]$RepoDir,
        [string]$ModName = "",
        [string]$ProjectFileName = "",
        [string]$OutputSubdir = "",
        [string]$DllName = "",
        [string]$ModFileName = "",
        [string]$ConfigFileName = "",
        [string]$KenshiPath = "",
        [string]$Configuration = "",
        [string]$Platform = "",
        [string]$PlatformToolset = ""
    )

    $resolvedConfiguration = Resolve-KenshiBoundValue -BoundParameters $BoundParameters -ParameterName "Configuration" -CurrentValue $Configuration -EnvVar "KENSHI_CONFIGURATION" -DefaultValue "Release"
    $resolvedPlatform = Resolve-KenshiBoundValue -BoundParameters $BoundParameters -ParameterName "Platform" -CurrentValue $Platform -EnvVar "KENSHI_PLATFORM" -DefaultValue "x64"
    $resolvedPlatformToolset = Resolve-KenshiBoundValue -BoundParameters $BoundParameters -ParameterName "PlatformToolset" -CurrentValue $PlatformToolset -EnvVar "KENSHI_PLATFORM_TOOLSET" -DefaultValue "v100"

    $resolvedModName = Resolve-KenshiValue -CurrentValue $ModName -EnvVar "KENSHI_MOD_NAME"
    if (-not $resolvedModName) {
        $resolvedModName = Resolve-KenshiValue -EnvVar "MOD_NAME"
    }
    if (-not $resolvedModName) {
        $resolvedModName = Split-Path -Leaf $RepoDir
    }
    $resolvedProjectFileName = Resolve-KenshiValue -CurrentValue $ProjectFileName -EnvVar "KENSHI_PROJECT_FILE" -DefaultValue "$resolvedModName.vcxproj"
    $resolvedOutputSubdir = Resolve-KenshiValue -CurrentValue $OutputSubdir -EnvVar "KENSHI_OUTPUT_SUBDIR" -DefaultValue "$resolvedPlatform\$resolvedConfiguration"
    $resolvedDllName = Resolve-KenshiValue -CurrentValue $DllName -EnvVar "KENSHI_DLL_NAME" -DefaultValue "$resolvedModName.dll"
    $resolvedModFileName = Resolve-KenshiValue -CurrentValue $ModFileName -EnvVar "KENSHI_MOD_FILE_NAME" -DefaultValue "$resolvedModName.mod"
    $resolvedConfigFileName = Resolve-KenshiBoundValue -BoundParameters $BoundParameters -ParameterName "ConfigFileName" -CurrentValue $ConfigFileName -EnvVar "KENSHI_CONFIG_FILE_NAME" -DefaultValue "RE_Kenshi.json"

    $resolvedKenshiPath = Resolve-KenshiBoundValue -BoundParameters $BoundParameters -ParameterName "KenshiPath" -CurrentValue $KenshiPath -EnvVar "KENSHI_PATH"
    if (-not $resolvedKenshiPath) {
        $resolvedKenshiPath = Resolve-KenshiValue -EnvVar "KENSHI_DEFAULT_PATH"
    }

    $projectFile = Join-Path $RepoDir $resolvedProjectFileName
    $outputDir = Join-Path $RepoDir $resolvedOutputSubdir
    $dllPath = Join-Path $outputDir $resolvedDllName
    $modDir = Join-Path $RepoDir $resolvedModName
    $kenshiModPath = ""
    if ($resolvedKenshiPath) {
        $kenshiModPath = Join-Path $resolvedKenshiPath "mods\$resolvedModName"
    }

    return [pscustomobject]@{
        RepoDir = $RepoDir
        ModName = $resolvedModName
        ProjectFileName = $resolvedProjectFileName
        OutputSubdir = $resolvedOutputSubdir
        DllName = $resolvedDllName
        ModFileName = $resolvedModFileName
        ConfigFileName = $resolvedConfigFileName
        KenshiPath = $resolvedKenshiPath
        Configuration = $resolvedConfiguration
        Platform = $resolvedPlatform
        PlatformToolset = $resolvedPlatformToolset
        ProjectFile = $projectFile
        OutputDir = $outputDir
        DllPath = $dllPath
        ModDir = $modDir
        KenshiModPath = $kenshiModPath
    }
}

function Ensure-KenshiBuildEnvironment {
    param(
        [Parameter(Mandatory = $true)][string]$ScriptDir
    )

    $setupEnvPath = Join-Path $ScriptDir "setup_env.ps1"

    $envValid = $true
    if (-not $env:KENSHILIB_DEPS_DIR -or -not $env:KENSHILIB_DIR -or -not $env:BOOST_INCLUDE_PATH) {
        $envValid = $false
    } elseif (-not (Test-Path (Join-Path $env:KENSHILIB_DEPS_DIR "boost_1_60_0"))) {
        $envValid = $false
    } elseif (-not (Test-Path (Join-Path $env:KENSHILIB_DEPS_DIR "KenshiLib"))) {
        $envValid = $false
    }

    if (-not $envValid) {
        if (Test-Path $setupEnvPath) {
            . $setupEnvPath
        } else {
            throw "setup_env.ps1 not found and required env vars are missing or invalid. Expected at: $setupEnvPath"
        }

        if (-not (Test-Path (Join-Path $env:KENSHILIB_DEPS_DIR "boost_1_60_0")) -or -not (Test-Path (Join-Path $env:KENSHILIB_DEPS_DIR "KenshiLib"))) {
            throw "KENSHILIB_DEPS_DIR is invalid: $env:KENSHILIB_DEPS_DIR"
        }
    }
}

function Get-MSBuildPath {
    $msBuildPath = $null
    $vsWherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

    if (Test-Path $vsWherePath) {
        $latestVS = & $vsWherePath -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if ($latestVS) {
            $possiblePath = Join-Path $latestVS "MSBuild\Current\Bin\MSBuild.exe"
            if (Test-Path $possiblePath) {
                $msBuildPath = $possiblePath
            } else {
                $possiblePath = Join-Path $latestVS "MSBuild\15.0\Bin\MSBuild.exe"
                if (Test-Path $possiblePath) {
                    $msBuildPath = $possiblePath
                }
            }
        }
    }

    if (-not $msBuildPath -and (Get-Command "msbuild" -ErrorAction SilentlyContinue)) {
        $msBuildPath = "msbuild"
    }

    if (-not $msBuildPath) {
        throw "MSBuild.exe not found. Run from a Developer Command Prompt or install VS Build Tools."
    }

    return $msBuildPath
}

function Invoke-KenshiBuild {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectFile,
        [Parameter(Mandatory = $true)][string]$Configuration,
        [Parameter(Mandatory = $true)][string]$Platform,
        [Parameter(Mandatory = $true)][string]$PlatformToolset
    )

    Write-Host "Locating MSBuild..." -ForegroundColor Gray
    $msBuildPath = Get-MSBuildPath
    Write-Host "Using MSBuild: $msBuildPath" -ForegroundColor Gray

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

    $buildOutput = & $msBuildPath $buildArgs 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "BUILD FAILED!`n$buildOutput"
    }
}

function Ensure-ReKenshiJson {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$PluginDllName
    )

    if (Test-Path $Path) {
        return
    }

    $seedObject = @{ Plugins = @($PluginDllName) }
    $seedObject | ConvertTo-Json -Depth 4 | Set-Content -Path $Path
    Write-Host "Created missing RE_Kenshi.json: $Path" -ForegroundColor Gray
}

function Ensure-ModFile {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$TemplatePath
    )

    if (Test-Path $Path) {
        return
    }

    if (-not (Test-Path $TemplatePath)) {
        throw "Default .mod template not found: $TemplatePath"
    }

    Copy-Item -Path $TemplatePath -Destination $Path -Force
    Write-Host "Created missing mod file: $Path" -ForegroundColor Gray
}

function Get-ForwardedParameters {
    param(
        [Parameter(Mandatory = $true)][hashtable]$BoundParameters,
        [Parameter(Mandatory = $true)][string[]]$AllowedKeys
    )

    $forwarded = @{}
    foreach ($key in $AllowedKeys) {
        if ($BoundParameters.ContainsKey($key)) {
            $forwarded[$key] = $BoundParameters[$key]
        }
    }

    return $forwarded
}

function Invoke-ScriptShim {
    param(
        [Parameter(Mandatory = $true)][string]$ScriptDir,
        [Parameter(Mandatory = $true)][string]$TargetScript,
        [array]$Arguments = @()
    )

    $targetPath = Join-Path $ScriptDir $TargetScript
    if (-not (Test-Path $targetPath)) {
        Write-Host "ERROR: $TargetScript not found at $targetPath" -ForegroundColor Red
        exit 1
    }
    & $targetPath @Arguments
    exit $LASTEXITCODE
}
