# Local wrapper: delegates to shared scripts submodule.
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [object[]]$PassthroughArgs
)

$ErrorActionPreference = "Stop"
$ScriptDir = $PSScriptRoot
if (-not $ScriptDir -and $PSCommandPath) {
    $ScriptDir = Split-Path -Parent $PSCommandPath
}
if ($ScriptDir) {
    $RepoDir = Split-Path -Parent $ScriptDir
} else {
    $RepoDir = (Get-Location).Path
}

$env:KENSHI_REPO_DIR = $RepoDir
$SharedRoot = Join-Path $RepoDir "tools\build-scripts"
$LoadEnvScript = Join-Path $SharedRoot "load-env.ps1"
$SharedScript = Join-Path $SharedRoot "build-and-deploy.ps1"

if (-not (Test-Path $SharedScript)) {
    Write-Host "ERROR: Shared script not found: $SharedScript" -ForegroundColor Red
    Write-Host "Run: git submodule update --init --recursive" -ForegroundColor Yellow
    exit 1
}

if (Test-Path $LoadEnvScript) {
    . $LoadEnvScript -RepoDir $RepoDir
}

$ArgsToPass = @()
if ($PassthroughArgs) {
    $ArgsToPass = @($PassthroughArgs | Where-Object { $_ -ne $null -and $_.ToString() -ne "" })
}

& $SharedScript @ArgsToPass
exit $LASTEXITCODE
