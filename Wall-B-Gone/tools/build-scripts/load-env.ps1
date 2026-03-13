# Loads .env from repo root into the current process environment.
# Lines are KEY=VALUE, comments start with #, and blank lines are ignored.

param(
    [string]$RepoDir
)

if (-not $RepoDir) {
    $RepoDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
}

$EnvPath = Join-Path $RepoDir ".env"
if (-not (Test-Path $EnvPath)) {
    return
}

Get-Content -Path $EnvPath | ForEach-Object {
    $line = $_.Trim()
    if (-not $line) { return }
    if ($line.StartsWith("#")) { return }
    $idx = $line.IndexOf("=")
    if ($idx -lt 1) { return }
    $key = $line.Substring(0, $idx).Trim()
    $val = $line.Substring($idx + 1).Trim()
    if ($val.Length -ge 2) {
        if (($val.StartsWith('"') -and $val.EndsWith('"')) -or ($val.StartsWith("'") -and $val.EndsWith("'"))) {
            $val = $val.Substring(1, $val.Length - 2)
        }
    }
    if ($key) {
        Set-Item -Path "env:$key" -Value $val
    }
}

