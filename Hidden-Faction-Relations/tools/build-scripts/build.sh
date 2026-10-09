#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR_UNIX="$(cd "$(dirname "$0")" && pwd)"
exec bash "$SCRIPT_DIR_UNIX/_run-powershell.sh" "build.ps1" "-ProjectFileName,-OutputSubdir" "$@"
