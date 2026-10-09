#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <script-name.ps1> <path-arg-csv> [args...]" >&2
  exit 2
fi

SCRIPT_DIR_UNIX="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR_UNIX/_env.sh"

PS_SCRIPT_NAME="$1"
shift
PATH_ARG_CSV="$1"
shift

PS_SCRIPT_UNIX="$SCRIPT_DIR_UNIX/$PS_SCRIPT_NAME"
if [[ ! -f "$PS_SCRIPT_UNIX" ]]; then
  echo "ERROR: PowerShell script not found: $PS_SCRIPT_UNIX" >&2
  exit 1
fi

if [[ -x /mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe ]]; then
  PSH="/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe"
  CONVERT_PATHS=1
elif command -v powershell.exe >/dev/null 2>&1; then
  PSH="powershell.exe"
  CONVERT_PATHS=1
elif command -v pwsh >/dev/null 2>&1; then
  PSH="pwsh"
  CONVERT_PATHS=0
else
  PSH="powershell"
  CONVERT_PATHS=0
fi

normalize_path() {
  local p="$1"
  if [[ "$CONVERT_PATHS" -eq 1 ]]; then
    to_windows_path "$p"
  else
    printf '%s' "$p"
  fi
}

IFS=',' read -r -a PATH_ARGS <<< "$PATH_ARG_CSV"

is_path_switch() {
  local candidate="$1"
  local arg
  for arg in "${PATH_ARGS[@]}"; do
    [[ -z "$arg" ]] && continue
    if [[ "$candidate" == "$arg" ]]; then
      return 0
    fi
  done
  return 1
}

ARGS=()
EXPECT_PATH=0
for arg in "$@"; do
  if [[ "$EXPECT_PATH" -eq 1 ]]; then
    ARGS+=("$(normalize_path "$arg")")
    EXPECT_PATH=0
    continue
  fi

  if [[ "$arg" == *=* ]]; then
    name="${arg%%=*}"
    value="${arg#*=}"
    if is_path_switch "$name"; then
      ARGS+=("$name=$(normalize_path "$value")")
      continue
    fi
  fi

  ARGS+=("$arg")
  if is_path_switch "$arg"; then
    EXPECT_PATH=1
  fi
done

PS_SCRIPT="$(normalize_path "$PS_SCRIPT_UNIX")"
exec "$PSH" -NoProfile -ExecutionPolicy Bypass -File "$PS_SCRIPT" "${ARGS[@]}"
