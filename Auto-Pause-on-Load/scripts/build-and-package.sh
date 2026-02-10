#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -W)"
source "$SCRIPT_DIR/_env.sh"
if command -v pwsh >/dev/null 2>&1; then
  PSH="pwsh"
else
  PSH="powershell"
fi
exec "$PSH" -NoProfile -ExecutionPolicy Bypass -File "$SCRIPT_DIR\\build-and-package.ps1" "$@"
