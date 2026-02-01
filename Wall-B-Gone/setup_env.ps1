# Run this script in your PowerShell terminal before opening Visual Studio:
# . .\setup_env.ps1
# The leading dot sources the script in the current scope.

# This path is configured during the project scaffolding.
$env:KENSHILIB_DEPS_DIR = "I:\Kenshi_modding\_deps\KenshiLib_Examples_deps"
$env:KENSHILIB_DIR = Join-Path $env:KENSHILIB_DEPS_DIR "KenshiLib"
$env:BOOST_INCLUDE_PATH = Join-Path $env:KENSHILIB_DEPS_DIR "boost_1_60_0"

Write-Host "Environment variables set for this session:"
Write-Host "KENSHILIB_DEPS_DIR = $env:KENSHILIB_DEPS_DIR"
