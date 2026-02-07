# Run this script in your PowerShell terminal before opening Visual Studio:
# . .\scripts\setup_env.ps1
# The leading dot sources the script in the current scope.

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoDir = Split-Path -Parent $ScriptDir
$LoadEnv = Join-Path $ScriptDir "load-env.ps1"
if (Test-Path $LoadEnv) {
    . $LoadEnv -RepoDir $RepoDir
}

# This path is configured during the project scaffolding.
$defaultDepsDir = "I:\Kenshi_modding\_deps\KenshiLib_Examples_deps"
$needsDepsReset = -not $env:KENSHILIB_DEPS_DIR
if (-not $needsDepsReset) {
    $depsRoot = [IO.Path]::GetFullPath($env:KENSHILIB_DEPS_DIR)
    $expectedRoot = [IO.Path]::GetFullPath("I:\Kenshi_modding\_deps")
    if (-not $depsRoot.StartsWith($expectedRoot, [StringComparison]::OrdinalIgnoreCase)) {
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

$expectedBoost = Join-Path $env:KENSHILIB_DEPS_DIR "boost_1_60_0"
if (-not $env:BOOST_INCLUDE_PATH -or ($env:BOOST_INCLUDE_PATH -ne $expectedBoost)) {
    $env:BOOST_INCLUDE_PATH = $expectedBoost
}

Write-Host "Environment variables set for this session:"
Write-Host "KENSHILIB_DEPS_DIR = $env:KENSHILIB_DEPS_DIR"
