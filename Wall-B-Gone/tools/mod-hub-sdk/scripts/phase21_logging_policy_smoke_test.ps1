param(
    [string]$RepoRoot = ""
)

$ErrorActionPreference = "Stop"

function Assert-Condition {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

if (-not $RepoRoot) {
    $scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $PSCommandPath }
    $RepoRoot = Split-Path -Parent $scriptDir
}

if (-not (Test-Path $RepoRoot)) {
    throw "RepoRoot not found: $RepoRoot"
}

$configPath = Join-Path $RepoRoot "Emkejs-Mod-Core\mod-config.json"
$config = Get-Content -Path $configPath -Raw | ConvertFrom-Json
Assert-Condition -Condition ($config.PSObject.Properties.Name -contains "debugLogging") -Message "mod-config.json missing debugLogging."
Assert-Condition -Condition ($config.PSObject.Properties.Name -contains "debugSearchLogging") -Message "mod-config.json missing debugSearchLogging."
Assert-Condition -Condition ($config.PSObject.Properties.Name -contains "debugBindingLogging") -Message "mod-config.json missing debugBindingLogging."
Assert-Condition -Condition ($config.debugLogging -eq $false) -Message "debugLogging should default to false."
Assert-Condition -Condition ($config.debugSearchLogging -eq $false) -Message "debugSearchLogging should default to false."
Assert-Condition -Condition ($config.debugBindingLogging -eq $false) -Message "debugBindingLogging should default to false."

$vcxprojPath = Join-Path $RepoRoot "Emkejs-Mod-Core.vcxproj"
$vcxprojText = Get-Content -Path $vcxprojPath -Raw
Assert-Condition -Condition ($vcxprojText.Contains("PLUGIN_ENABLE_VERBOSE_DIAGNOSTICS=1")) -Message "Debug config missing PLUGIN_ENABLE_VERBOSE_DIAGNOSTICS=1."
$releaseBlock = [Regex]::Match(
    $vcxprojText,
    '<ItemDefinitionGroup Condition="''\$\((Configuration)\)\|\$\((Platform)\)''==''Release\|x64''">(?<body>[\s\S]*?)</ItemDefinitionGroup>')
Assert-Condition -Condition $releaseBlock.Success -Message "Release ItemDefinitionGroup missing from vcxproj."
Assert-Condition -Condition (-not $releaseBlock.Groups["body"].Value.Contains("PLUGIN_ENABLE_VERBOSE_DIAGNOSTICS")) -Message "Release config should not define PLUGIN_ENABLE_VERBOSE_DIAGNOSTICS."

$loggingHeaderPath = Join-Path $RepoRoot "src\logging.h"
$loggingHeaderText = Get-Content -Path $loggingHeaderPath -Raw
foreach ($symbol in @(
    "LoadLoggingConfig",
    "LogInfoLine",
    "LogWarnLine",
    "LogErrorLine",
    "ShouldCompileVerboseDiagnostics",
    "ShouldLogDebug",
    "ShouldLogSearchDebug",
    "ShouldLogBindingDebug",
    "LogDebugLine",
    "LogSearchDebugLine",
    "LogBindingDebugLine"
)) {
    Assert-Condition -Condition ($loggingHeaderText.Contains($symbol)) -Message "logging.h missing symbol: $symbol"
}

$loggingSourcePath = Join-Path $RepoRoot "src\logging.cpp"
$loggingSourceText = Get-Content -Path $loggingSourcePath -Raw
Assert-Condition -Condition ($loggingSourceText.Contains("g_debug_logging = false;")) -Message "logging.cpp should reset g_debug_logging to false."
Assert-Condition -Condition ($loggingSourceText.Contains("g_debug_search_logging = false;")) -Message "logging.cpp should reset g_debug_search_logging to false."
Assert-Condition -Condition ($loggingSourceText.Contains("g_debug_binding_logging = false;")) -Message "logging.cpp should reset g_debug_binding_logging to false."
Assert-Condition -Condition ($loggingSourceText.Contains("return g_debug_logging && g_debug_search_logging;")) -Message "ShouldLogSearchDebug should require debugLogging."
Assert-Condition -Condition ($loggingSourceText.Contains("return g_debug_logging && g_debug_binding_logging;")) -Message "ShouldLogBindingDebug should require debugLogging."
Assert-Condition -Condition ($loggingSourceText.Contains('TryParseJsonBoolByKey(config_text, "debugLogging", &parsed_value)')) -Message "logging.cpp should parse debugLogging."
Assert-Condition -Condition ($loggingSourceText.Contains('TryParseJsonBoolByKey(config_text, "debugSearchLogging", &parsed_value)')) -Message "logging.cpp should parse debugSearchLogging."
Assert-Condition -Condition ($loggingSourceText.Contains('TryParseJsonBoolByKey(config_text, "debugBindingLogging", &parsed_value)')) -Message "logging.cpp should parse debugBindingLogging."

$startupPath = Join-Path $RepoRoot "Emkejs-Mod-Core.cpp"
$startupText = Get-Content -Path $startupPath -Raw
Assert-Condition -Condition ($startupText.Contains('LogInfoLine("startPlugin()")')) -Message "startPlugin should log through LogInfoLine."
Assert-Condition -Condition ($startupText.Contains('LoadLoggingConfig();')) -Message "startPlugin should load logging config."
Assert-Condition -Condition ($startupText.Contains('LogInfoLine("startup complete")')) -Message "startup complete should log through LogInfoLine."

$hubRegistryText = Get-Content -Path (Join-Path $RepoRoot "src\hub_registry.cpp") -Raw
Assert-Condition -Condition ($hubRegistryText.Contains("LogDebugLine(line.str());")) -Message "hub_registry warning path should use LogDebugLine."

$hubCommitText = Get-Content -Path (Join-Path $RepoRoot "src\hub_commit.cpp") -Raw
Assert-Condition -Condition ($hubCommitText.Contains("LogDebugLine(line.str());")) -Message "hub_commit summary path should use LogDebugLine."

$hubExportsText = Get-Content -Path (Join-Path $RepoRoot "src\hub_exports.cpp") -Raw
Assert-Condition -Condition ($hubExportsText.Contains('LogDebugLine("event=hub_get_api_alias_deprecated')) -Message "hub_exports deprecated alias log should use LogDebugLine."

Write-Host "PASS: phase21 logging policy smoke completed"
