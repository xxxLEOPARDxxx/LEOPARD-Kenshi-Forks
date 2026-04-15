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
        $repoDir = Split-Path -Parent (Split-Path -Parent $scriptDir)
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

function Initialize-KenshiScriptTiming {
    $existingStartMs = Get-KenshiEnvValue -Name "KENSHI_SCRIPT_START_MS"
    if ($existingStartMs) {
        return
    }

    $env:KENSHI_SCRIPT_START_MS = [string][DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
}

function Get-KenshiScriptElapsedTime {
    $startMsValue = Get-KenshiEnvValue -Name "KENSHI_SCRIPT_START_MS"
    if (-not $startMsValue) {
        return $null
    }

    $parsedStartMs = 0L
    if (-not [long]::TryParse($startMsValue, [ref]$parsedStartMs)) {
        return $null
    }

    $elapsedMs = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds() - $parsedStartMs
    if ($elapsedMs -lt 0) {
        $elapsedMs = 0
    }

    return [TimeSpan]::FromMilliseconds([double]$elapsedMs)
}

function Format-KenshiScriptElapsedTime {
    param(
        [Parameter(Mandatory = $true)][TimeSpan]$ElapsedTime
    )

    $hours = [int]$ElapsedTime.TotalHours
    $minutes = [int]$ElapsedTime.Minutes
    $seconds = [int]$ElapsedTime.Seconds
    $hundredths = [int][math]::Floor($ElapsedTime.Milliseconds / 10)
    return ("{0:D2}:{1:D2}:{2:D2}.{3:D2}" -f $hours, $minutes, $seconds, $hundredths)
}

function Write-KenshiScriptElapsedTime {
    $suppressTimestamp = Get-KenshiEnvValue -Name "KENSHI_SUPPRESS_FINISH_TIMESTAMP"
    if ($suppressTimestamp -and $suppressTimestamp -ne "0") {
        return
    }

    $elapsedTime = Get-KenshiScriptElapsedTime
    if ($null -eq $elapsedTime) {
        return
    }

    $formattedElapsedTime = Format-KenshiScriptElapsedTime -ElapsedTime $elapsedTime
    Write-Host "Duration: $formattedElapsedTime" -ForegroundColor Gray
}

function Write-KenshiBuildFinishedTimestamp {
    $suppressTimestamp = Get-KenshiEnvValue -Name "KENSHI_SUPPRESS_FINISH_TIMESTAMP"
    if ($suppressTimestamp -and $suppressTimestamp -ne "0") {
        return
    }

    $finishedAt = (Get-Date).ToString("HH:mm:ss, dd.MM.yyyy")
    Write-Host $finishedAt -ForegroundColor Gray
}

function Exit-KenshiScriptWithTimestamp {
    param(
        [int]$ExitCode = 0
    )

    $global:LASTEXITCODE = $ExitCode
    Write-KenshiScriptElapsedTime
    Write-KenshiBuildFinishedTimestamp
    exit $ExitCode
}

function Invoke-KenshiScriptWithSuppressedTimestamp {
    param(
        [Parameter(Mandatory = $true)][scriptblock]$Action
    )

    $previousSuppressTimestamp = Get-KenshiEnvValue -Name "KENSHI_SUPPRESS_FINISH_TIMESTAMP"
    $env:KENSHI_SUPPRESS_FINISH_TIMESTAMP = "1"
    try {
        & $Action
        $global:LASTEXITCODE = $LASTEXITCODE
    } finally {
        if ($previousSuppressTimestamp) {
            $env:KENSHI_SUPPRESS_FINISH_TIMESTAMP = $previousSuppressTimestamp
        } else {
            Remove-Item Env:KENSHI_SUPPRESS_FINISH_TIMESTAMP -ErrorAction SilentlyContinue
        }
    }
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
    if (-not $env:KENSHILIB_DEPS_DIR -or -not $env:KENSHILIB_DIR) {
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

        if (-not (Test-Path (Join-Path $env:KENSHILIB_DEPS_DIR "KenshiLib"))) {
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
        [Parameter(Mandatory = $true)][string]$PlatformToolset,
        [switch]$Clean
    )

    Write-Host "Locating MSBuild..." -ForegroundColor Gray
    $msBuildPath = Get-MSBuildPath
    Write-Host "Using MSBuild: $msBuildPath" -ForegroundColor Gray

    $buildTarget = if ($Clean) { "Clean,Rebuild" } else { "Build" }
    $buildModeLabel = if ($Clean) { "clean rebuild" } else { "incremental build" }

    Write-Host "Building project ($buildModeLabel)..." -ForegroundColor Yellow

    $buildArgs = @(
        $ProjectFile,
        "/t:$buildTarget",
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

function Get-SuspectedKenshiLockProcesses {
    param(
        [string]$KenshiPath = ""
    )

    $suspects = @()
    $seenPids = @{}

    foreach ($candidateName in @("kenshi_x64", "kenshi")) {
        $matching = Get-Process -Name $candidateName -ErrorAction SilentlyContinue
        foreach ($proc in $matching) {
            if ($seenPids.ContainsKey($proc.Id)) {
                continue
            }

            $seenPids[$proc.Id] = $true
            $suspects += [pscustomobject]@{
                Name = [string]$proc.ProcessName
                Id = [int]$proc.Id
            }
        }
    }

    if ($suspects.Count -gt 0 -or -not $KenshiPath) {
        return $suspects
    }

    $resolvedKenshiPath = ""
    try {
        $resolvedKenshiPath = [System.IO.Path]::GetFullPath($KenshiPath)
    } catch {
        return $suspects
    }

    if (-not (Test-Path $resolvedKenshiPath)) {
        return $suspects
    }

    $allProcesses = Get-Process -ErrorAction SilentlyContinue
    foreach ($proc in $allProcesses) {
        if ($seenPids.ContainsKey($proc.Id)) {
            continue
        }

        try {
            $mainModule = $proc.MainModule
            if ($null -eq $mainModule -or -not $mainModule.FileName) {
                continue
            }

            $processPath = [System.IO.Path]::GetFullPath([string]$mainModule.FileName)
            if (-not $processPath.StartsWith($resolvedKenshiPath, [System.StringComparison]::OrdinalIgnoreCase)) {
                continue
            }

            $seenPids[$proc.Id] = $true
            $suspects += [pscustomobject]@{
                Name = [string]$proc.ProcessName
                Id = [int]$proc.Id
            }
        } catch {
            continue
        }
    }

    return $suspects
}

function Test-DeploySharingViolationException {
    param(
        [Parameter(Mandatory = $true)][System.Exception]$Exception
    )

    $current = $Exception
    while ($null -ne $current) {
        if ($current -is [System.IO.IOException]) {
            $nativeCode = $current.HResult -band 0xFFFF
            if ($nativeCode -eq 32 -or $nativeCode -eq 33) {
                return $true
            }
        }

        $current = $current.InnerException
    }

    return $false
}

function Test-DeployTargetPreflight {
    param(
        [Parameter(Mandatory = $true)][string]$TargetPath,
        [string]$KenshiPath = ""
    )

    $result = [ordered]@{
        CanProceed = $true
        FailureReason = ""
        ExitCode = 0
        Details = ""
        TargetPath = $TargetPath
        SuspectedProcesses = @()
    }

    if (-not (Test-Path $TargetPath)) {
        return [pscustomobject]$result
    }

    $stream = $null
    try {
        $stream = [System.IO.File]::Open(
            $TargetPath,
            [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::ReadWrite,
            [System.IO.FileShare]::None
        )
        return [pscustomobject]$result
    } catch {
        $result.CanProceed = $false
        $result.Details = [string]$_.Exception.Message

        if (Test-DeploySharingViolationException -Exception $_.Exception) {
            $result.FailureReason = "in_use"
            $result.ExitCode = 32
            $result.SuspectedProcesses = @(Get-SuspectedKenshiLockProcesses -KenshiPath $KenshiPath)
        } else {
            $result.FailureReason = "not_writable"
            $result.ExitCode = 33
        }

        return [pscustomobject]$result
    } finally {
        if ($null -ne $stream) {
            $stream.Dispose()
        }
    }
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
