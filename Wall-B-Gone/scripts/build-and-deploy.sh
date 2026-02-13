#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR_UNIX="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR_UNIX/_env.sh"
PS_SCRIPT="$(to_windows_path "$SCRIPT_DIR_UNIX/build-and-deploy.ps1")"
if [[ -x /mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe ]]; then
  PSH="/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe"
elif command -v powershell.exe >/dev/null 2>&1; then
  PSH="powershell.exe"
elif command -v pwsh >/dev/null 2>&1; then
  PSH="pwsh"
else
  PSH="powershell"
fi

ARGS=()
EXPECT_PATH=0
for arg in "$@"; do
  if [[ "$EXPECT_PATH" -eq 1 ]]; then
    ARGS+=("$(to_windows_path "$arg")")
    EXPECT_PATH=0
    continue
  fi

  case "$arg" in
    -KenshiPath|-ProjectFileName|-OutputSubdir)
      ARGS+=("$arg")
      EXPECT_PATH=1
      ;;
    *)
      ARGS+=("$arg")
      ;;
  esac
done

exec "$PSH" -NoProfile -ExecutionPolicy Bypass -File "$PS_SCRIPT" "${ARGS[@]}"
