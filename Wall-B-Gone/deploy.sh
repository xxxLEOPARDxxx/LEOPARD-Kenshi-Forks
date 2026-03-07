#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR_UNIX="$(cd "$(dirname "$0")" && pwd)"
exec "$SCRIPT_DIR_UNIX/_run-powershell.sh" "deploy.ps1" "-KenshiPath,-ProjectFileName,-OutputSubdir" "$@"
