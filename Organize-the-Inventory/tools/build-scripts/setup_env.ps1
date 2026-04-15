# Run this script in your PowerShell terminal before opening Visual Studio:
# . .\scripts\setup_env.ps1
# The leading dot sources the script in the current scope.

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$CommonScript = Join-Path $scriptDir "kenshi-common.ps1"
if (-not (Test-Path $CommonScript)) {
    Write-Host "ERROR: Missing shared helper: $CommonScript" -ForegroundColor Red
    return
}
. $CommonScript

$ctx = Initialize-KenshiScriptContext -InvocationPath $MyInvocation.MyCommand.Path

# Prefer explicit env override; otherwise use repo-relative defaults.
if ($env:KENSHI_DEFAULT_DEPS_DIR) {
    $defaultDepsDir = $env:KENSHI_DEFAULT_DEPS_DIR
} else {
    $workspaceRoot = Split-Path -Parent $ctx.RepoDir
    $defaultDepsDir = Join-Path $workspaceRoot "_deps\KenshiLib_Examples_deps"
}

$expectedRoot = if ($env:KENSHI_DEPS_ROOT) { $env:KENSHI_DEPS_ROOT } else { Join-Path (Split-Path -Parent $ctx.RepoDir) "_deps" }
$needsDepsReset = -not $env:KENSHILIB_DEPS_DIR
if (-not $needsDepsReset) {
    $depsRoot = [IO.Path]::GetFullPath($env:KENSHILIB_DEPS_DIR)
    $expectedRootFull = [IO.Path]::GetFullPath($expectedRoot)
    if (-not $depsRoot.StartsWith($expectedRootFull, [StringComparison]::OrdinalIgnoreCase)) {
        $needsDepsReset = $true
    }
}
if ($needsDepsReset) {
    $env:KENSHILIB_DEPS_DIR = $defaultDepsDir
}

$expectedKenshiLib = Join-Path $env:KENSHILIB_DEPS_DIR "KenshiLib"
if (-not $env:KENSHILIB_DIR -or ($env:KENSHILIB_DIR -ne $expectedKenshiLib)) {
    $env:KENSHILIB_DIR = $expectedKenshiLib
}

Write-Host "Environment variables set for this session:"
Write-Host "KENSHILIB_DEPS_DIR = $env:KENSHILIB_DEPS_DIR"
