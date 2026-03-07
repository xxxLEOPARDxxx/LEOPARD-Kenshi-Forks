# Package-only script for Kenshi mod plugins.

param(
    [string]$ModName = "",
    [string]$ProjectFileName = "",
    [string]$OutputSubdir = "",
    [string]$Configuration = "",
    [string]$Platform = "",
    [string]$KenshiPath = "",
    [string]$SourceModPath = "",
    [string]$DllName = "",
    [string]$ModFileName = "",
    [string]$ConfigFileName = "",
    [string]$OutDir = "",
    [string]$ZipName = "",
    [string]$Version = "",
    [switch]$DeployIfMissing
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$CommonScript = Join-Path $scriptDir "kenshi-common.ps1"
if (-not (Test-Path $CommonScript)) {
    Write-Host "ERROR: Missing shared helper: $CommonScript" -ForegroundColor Red
    exit 1
}
. $CommonScript

$ctx = Initialize-KenshiScriptContext -InvocationPath $MyInvocation.MyCommand.Path
$resolved = Resolve-KenshiBuildContext -BoundParameters $PSBoundParameters -RepoDir $ctx.RepoDir -ModName $ModName -ProjectFileName $ProjectFileName -OutputSubdir $OutputSubdir -DllName $DllName -ModFileName $ModFileName -ConfigFileName $ConfigFileName -KenshiPath $KenshiPath -Configuration $Configuration -Platform $Platform
$versionFile = Join-Path $ctx.RepoDir "VERSION"
$deployScript = Join-Path $scriptDir "deploy.ps1"

function Get-LocalPackagingStatus {
    param(
        [Parameter(Mandatory = $true)]$Resolved,
        [Parameter(Mandatory = $true)][string]$LocalModPath
    )

    $missingModFiles = @()
    if (-not (Test-Path $LocalModPath)) {
        $missingModFiles += $LocalModPath
    } else {
        foreach ($fileName in @($Resolved.ModFileName, $Resolved.ConfigFileName)) {
            $filePath = Join-Path $LocalModPath $fileName
            if (-not (Test-Path $filePath)) {
                $missingModFiles += $filePath
            }
        }
    }

    return [pscustomobject]@{
        MissingModFiles = $missingModFiles
        DllMissing = -not (Test-Path $Resolved.DllPath)
    }
}

function Can-PromptUser {
    try {
        $null = $Host.UI.RawUI
        return $true
    } catch {
        return $false
    }
}

$packageSourcePath = $SourceModPath
if (-not $packageSourcePath) {
    $status = Get-LocalPackagingStatus -Resolved $resolved -LocalModPath $resolved.ModDir
    if ($status.MissingModFiles.Count -gt 0) {
        Write-Host "Local mod template files are missing for package source:" -ForegroundColor Yellow
        foreach ($missingPath in $status.MissingModFiles) {
            Write-Host " - $missingPath" -ForegroundColor Yellow
        }

        $runDeploy = $DeployIfMissing.IsPresent
        if (-not $runDeploy -and (Can-PromptUser)) {
            $deployAnswer = (Read-Host "Run deploy.ps1 now and retry local package checks? [y/N]").Trim().ToLowerInvariant()
            $runDeploy = $deployAnswer -in @("y", "yes")
        }

        if ($runDeploy) {
            if (-not (Test-Path $deployScript)) {
                Write-Host "ERROR: deploy.ps1 not found at $deployScript" -ForegroundColor Red
                return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
            }

            $deployParams = Get-ForwardedParameters -BoundParameters $PSBoundParameters -AllowedKeys @(
                "ModName",
                "ProjectFileName",
                "OutputSubdir",
                "DllName",
                "ModFileName",
                "ConfigFileName",
                "KenshiPath",
                "Configuration",
                "Platform"
            )

            if (-not $deployParams.ContainsKey("ModName") -or -not $deployParams.ModName) {
                $deployParams.ModName = $resolved.ModName
            }
            if (-not $deployParams.ContainsKey("ProjectFileName") -or -not $deployParams.ProjectFileName) {
                $deployParams.ProjectFileName = $resolved.ProjectFileName
            }
            if (-not $deployParams.ContainsKey("OutputSubdir") -or -not $deployParams.OutputSubdir) {
                $deployParams.OutputSubdir = $resolved.OutputSubdir
            }
            if (-not $deployParams.ContainsKey("DllName") -or -not $deployParams.DllName) {
                $deployParams.DllName = $resolved.DllName
            }
            if (-not $deployParams.ContainsKey("ModFileName") -or -not $deployParams.ModFileName) {
                $deployParams.ModFileName = $resolved.ModFileName
            }
            if (-not $deployParams.ContainsKey("ConfigFileName") -or -not $deployParams.ConfigFileName) {
                $deployParams.ConfigFileName = $resolved.ConfigFileName
            }
            if ((-not $deployParams.ContainsKey("KenshiPath") -or -not $deployParams.KenshiPath) -and $resolved.KenshiPath) {
                $deployParams.KenshiPath = $resolved.KenshiPath
            }
            if (-not $deployParams.ContainsKey("Configuration") -or -not $deployParams.Configuration) {
                $deployParams.Configuration = $resolved.Configuration
            }
            if (-not $deployParams.ContainsKey("Platform") -or -not $deployParams.Platform) {
                $deployParams.Platform = $resolved.Platform
            }

            Write-Host "Running deploy before package..." -ForegroundColor Yellow
            Invoke-KenshiScriptWithSuppressedTimestamp { & $deployScript @deployParams }
            if ($LASTEXITCODE -ne 0) {
                Write-Host "ERROR: deploy.ps1 failed" -ForegroundColor Red
                return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
            }

            $status = Get-LocalPackagingStatus -Resolved $resolved -LocalModPath $resolved.ModDir
        }
    }

    if ($status.MissingModFiles.Count -gt 0) {
        Write-Host "ERROR: Local mod template files are still missing. Package source must be local." -ForegroundColor Red
        foreach ($missingPath in $status.MissingModFiles) {
            Write-Host " - $missingPath" -ForegroundColor Red
        }
        return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
    }

    if ($status.DllMissing) {
        Write-Host "ERROR: Built DLL not found: $($resolved.DllPath)" -ForegroundColor Red
        Write-Host "Run build.ps1 (or a build wrapper) before package." -ForegroundColor Yellow
        return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
    }

    $stagingRoot = Join-Path $ctx.RepoDir ".packaging\$($resolved.ModName)"
    if (Test-Path $stagingRoot) {
        Remove-Item -Path $stagingRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Path $stagingRoot -Force | Out-Null
    Copy-Item -Path "$($resolved.ModDir)\*" -Destination $stagingRoot -Recurse -Force
    Copy-Item -Path $resolved.DllPath -Destination (Join-Path $stagingRoot $resolved.DllName) -Force
    $packageSourcePath = $stagingRoot
}

if (-not (Test-Path $packageSourcePath)) {
    Write-Host "ERROR: Mod folder not found: $packageSourcePath" -ForegroundColor Red
    return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
}

$requiredFiles = @(
    $resolved.DllName,
    $resolved.ConfigFileName,
    $resolved.ModFileName
)

foreach ($fileName in $requiredFiles) {
    $filePath = Join-Path $packageSourcePath $fileName
    if (-not (Test-Path $filePath)) {
        Write-Host "ERROR: Missing required file in package source: $filePath" -ForegroundColor Red
        return (Exit-KenshiScriptWithTimestamp -ExitCode 1)
    }
}

if (-not $OutDir) {
    $OutDir = Join-Path $ctx.RepoDir "dist"
}

if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
}

if (-not $Version -and (Test-Path $versionFile)) {
    $Version = (Get-Content -Path $versionFile | Select-Object -First 1).Trim()
}

if (-not $ZipName) {
    if ($Version) {
        $ZipName = "$($resolved.ModName)-$Version.zip"
    } else {
        $ZipName = "$($resolved.ModName)-alpha.zip"
    }
}

$zipPath = Join-Path $OutDir $ZipName
if (Test-Path $zipPath) {
    Remove-Item -Path $zipPath -Force
}

Write-Host "Packaging: $packageSourcePath" -ForegroundColor Yellow
Write-Host "Output:    $zipPath" -ForegroundColor Gray

Compress-Archive -Path $packageSourcePath -DestinationPath $zipPath

Write-Host "Package created: $zipPath" -ForegroundColor Green
return (Exit-KenshiScriptWithTimestamp -ExitCode 0)
