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

function Get-PlatformVersionAddressLiterals {
    param(
        [Parameter(Mandatory = $true)][string]$SourcePath,
        [Parameter(Mandatory = $true)][string]$PlatformSelectorText,
        [Parameter(Mandatory = $true)][string]$Version
    )

    $text = Get-Content -Path $SourcePath -Raw

    $platformIndex = $text.IndexOf($PlatformSelectorText, [StringComparison]::Ordinal)
    Assert-Condition -Condition ($platformIndex -ge 0) -Message "Could not locate platform block '$PlatformSelectorText' in $SourcePath."

    $platformSection = $text.Substring($platformIndex)
    $match = [Regex]::Match(
        $platformSection,
        ('if \(version == "{0}"\)\s*\{{(?<body>[\s\S]*?)return true;\s*\}}' -f [Regex]::Escape($Version)),
        [System.Text.RegularExpressions.RegexOptions]::Singleline)
    Assert-Condition -Condition $match.Success -Message "Could not locate version block '$Version' in $SourcePath."

    $addresses = New-Object System.Collections.Generic.List[string]
    foreach ($line in ($match.Groups["body"].Value -split "`r?`n")) {
        $lineMatch = [Regex]::Match($line, 'base_addr\s*\+\s*(?<address>0x[0-9A-Fa-f]{6,8})')
        if ($lineMatch.Success) {
            $addresses.Add($lineMatch.Groups["address"].Value.ToUpperInvariant())
        }
    }

    Assert-Condition -Condition ($addresses.Count -gt 0) -Message "No address literals found for platform '$PlatformSelectorText' version '$Version' in $SourcePath."
    return @($addresses | Select-Object -Unique)
}

function Get-DocumentedAddresses {
    param(
        [Parameter(Mandatory = $true)][string]$DocPath
    )

    $docText = Get-Content -Path $DocPath -Raw

    Assert-Condition -Condition ($docText.Contains("## Ownership")) -Message "Address SSOT doc must define ownership."
    Assert-Condition -Condition ($docText.Contains("## Update Process")) -Message "Address SSOT doc must define the update process."
    Assert-Condition -Condition ($docText.Contains('`required`')) -Message "Address SSOT doc must define the required status."
    Assert-Condition -Condition ($docText.Contains('`deprecated`')) -Message "Address SSOT doc must define the deprecated status."
    Assert-Condition -Condition ($docText.Contains('`removal-target`')) -Message "Address SSOT doc must define the removal-target status."

    $matches = [Regex]::Matches($docText, '\b0x[0-9A-Fa-f]{6,8}\b')
    $addresses = @()
    foreach ($match in $matches) {
        $addresses += $match.Value.ToUpperInvariant()
    }

    Assert-Condition -Condition ($addresses.Count -gt 0) -Message "Address SSOT doc does not list any address literals."
    return @($addresses | Select-Object -Unique)
}

function Get-StrayAddressMatches {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string[]]$AllowedRelativePaths
    )

    $scanRootRelPaths = @("src", "include", "scripts")
    $patterns = @(
        'base_addr\s*\+\s*(?<address>0x[0-9A-Fa-f]{6,8})',
        'GetRealAddress\s*\(\s*(?<address>0x[0-9A-Fa-f]{6,8})'
    )
    $results = New-Object System.Collections.Generic.List[pscustomobject]

    foreach ($scanRootRelPath in $scanRootRelPaths) {
        $scanRoot = Join-Path $RepoRoot $scanRootRelPath
        if (-not (Test-Path $scanRoot)) {
            continue
        }

        $files = Get-ChildItem -Path $scanRoot -Recurse -File
        foreach ($file in $files) {
            $repoRootWithSeparator = $RepoRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
            if ($file.FullName.StartsWith($repoRootWithSeparator, [StringComparison]::OrdinalIgnoreCase)) {
                $relativePath = $file.FullName.Substring($repoRootWithSeparator.Length).Replace('\', '/')
            }
            else {
                $relativePath = $file.FullName.Replace('\', '/')
            }
            if ($AllowedRelativePaths -contains $relativePath) {
                continue
            }

            $lineNumber = 0
            foreach ($line in (Get-Content -Path $file.FullName)) {
                $lineNumber += 1
                foreach ($pattern in $patterns) {
                    $match = [Regex]::Match($line, $pattern)
                    if ($match.Success) {
                        $results.Add([pscustomobject]@{
                            RelativePath = $relativePath
                            Line = $lineNumber
                            Address = $match.Groups["address"].Value.ToUpperInvariant()
                            Text = $line.Trim()
                        })
                    }
                }
            }
        }
    }

    return @($results)
}

function Write-FailAndExit {
    param(
        [Parameter(Mandatory = $true)][string]$Message
    )

    Write-Host ("FAIL " + $Message)
    exit 1
}

if (-not $RepoRoot) {
    $scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $PSCommandPath }
    $RepoRoot = Split-Path -Parent $scriptDir
}

$RepoRoot = (Resolve-Path -Path $RepoRoot).ProviderPath

$allowedSourceRelPath = "src/hub_menu_bridge.cpp"
$allowedSourcePath = Join-Path $RepoRoot $allowedSourceRelPath
$addressTables = @(
    [pscustomobject]@{
        Label = "kenshi_1_0_65_x64"
        PlatformSelectorText = "if (platform == 1u)"
        Version = "1.0.65"
        DocRelativePath = "docs/addresses/kenshi_1_0_65_x64.md"
    },
    [pscustomobject]@{
        Label = "kenshi_1_0_68_x64"
        PlatformSelectorText = "if (platform == 1u)"
        Version = "1.0.68"
        DocRelativePath = "docs/addresses/kenshi_1_0_68_x64.md"
    },
    [pscustomobject]@{
        Label = "kenshi_1_0_65_x86"
        PlatformSelectorText = "else if (platform == 0u)"
        Version = "1.0.65"
        DocRelativePath = "docs/addresses/kenshi_1_0_65_x86.md"
    },
    [pscustomobject]@{
        Label = "kenshi_1_0_68_x86"
        PlatformSelectorText = "else if (platform == 0u)"
        Version = "1.0.68"
        DocRelativePath = "docs/addresses/kenshi_1_0_68_x86.md"
    }
)

try {
    Assert-Condition -Condition (Test-Path $allowedSourcePath) -Message "Missing allowed source file: $allowedSourcePath"

    $allDocumentedAddresses = New-Object System.Collections.Generic.List[string]
    $allRuntimeAddresses = New-Object System.Collections.Generic.List[string]
    foreach ($table in $addressTables) {
        $docPath = Join-Path $RepoRoot $table.DocRelativePath
        Assert-Condition -Condition (Test-Path $docPath) -Message "Missing address SSOT doc: $docPath"

        $documentedAddresses = Get-DocumentedAddresses -DocPath $docPath
        foreach ($address in $documentedAddresses) {
            $allDocumentedAddresses.Add($address)
        }

        $runtimeAddresses = Get-PlatformVersionAddressLiterals `
            -SourcePath $allowedSourcePath `
            -PlatformSelectorText $table.PlatformSelectorText `
            -Version $table.Version
        foreach ($address in $runtimeAddresses) {
            $allRuntimeAddresses.Add($address)
        }

        $missingAddresses = @()
        foreach ($address in $runtimeAddresses) {
            if ($documentedAddresses -notcontains $address) {
                $missingAddresses += $address
            }
        }

        Assert-Condition -Condition ($missingAddresses.Count -eq 0) -Message (
            "Address SSOT doc '{0}' is missing runtime addresses: {1}" -f $table.DocRelativePath, ($missingAddresses -join ", "))
    }

    $strayMatches = Get-StrayAddressMatches `
        -RepoRoot $RepoRoot `
        -AllowedRelativePaths @($allowedSourceRelPath)

    Assert-Condition -Condition ($strayMatches.Count -eq 0) -Message (
        "Unexpected hard-coded address literal(s) outside approved files:`n" +
        (($strayMatches | ForEach-Object {
            "{0}:{1} {2} :: {3}" -f $_.RelativePath, $_.Line, $_.Address, $_.Text
        }) -join [Environment]::NewLine))

    $uniqueDocumentedCount = @($allDocumentedAddresses | Select-Object -Unique).Count
    $uniqueRuntimeCount = @($allRuntimeAddresses | Select-Object -Unique).Count
    Write-Host ("PASS tables=" + $addressTables.Count + " runtime_addresses=" + $uniqueRuntimeCount + " documented_addresses=" + $uniqueDocumentedCount + " stray_matches=0")
}
catch {
    $failure = $_.Exception
    while ($failure.InnerException) {
        $failure = $failure.InnerException
    }

    Write-FailAndExit -Message $failure.Message
}
