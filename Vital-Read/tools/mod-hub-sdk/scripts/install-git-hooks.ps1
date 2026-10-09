param(
    [string]$RepoRoot = ""
)

$ErrorActionPreference = "Stop"

if (-not $RepoRoot) {
    $ScriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $PSCommandPath }
    $RepoRoot = Split-Path -Parent $ScriptDir
}

$RepoRoot = (Resolve-Path -Path $RepoRoot).ProviderPath
$HookDir = Join-Path $RepoRoot ".githooks"

if (-not (Test-Path $HookDir)) {
    throw "Hook directory not found: $HookDir"
}

$git = Get-Command "git" -ErrorAction SilentlyContinue
if ($null -eq $git) {
    throw "git command not found."
}

& $git.Source -C $RepoRoot config core.hooksPath .githooks
if ($LASTEXITCODE -ne 0) {
    throw "Failed to set core.hooksPath to .githooks"
}

$resolvedHooksPath = & $git.Source -C $RepoRoot config --get core.hooksPath
if ($LASTEXITCODE -ne 0 -or $resolvedHooksPath -ne ".githooks") {
    throw "core.hooksPath verification failed."
}

Write-Host "Configured core.hooksPath=.githooks"
