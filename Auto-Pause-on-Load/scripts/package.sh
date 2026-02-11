#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR_UNIX="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR_UNIX/_env.sh"
PS_SCRIPT="$(to_windows_path "$SCRIPT_DIR_UNIX/package.ps1")"
if command -v pwsh >/dev/null 2>&1; then
  PSH="pwsh"
elif command -v powershell.exe >/dev/null 2>&1; then
  PSH="powershell.exe"
else
  PSH="powershell"
fi
exec "$PSH" -NoProfile -ExecutionPolicy Bypass -File "$PS_SCRIPT" "$@"