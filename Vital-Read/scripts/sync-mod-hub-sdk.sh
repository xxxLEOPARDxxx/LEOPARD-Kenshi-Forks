#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR_UNIX="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR_UNIX/_env.sh"
PS_SCRIPT="$SCRIPT_DIR_UNIX/sync-mod-hub-sdk.ps1"
CONVERT_PATHS=0
if command -v pwsh >/dev/null 2>&1; then
  PSH="pwsh"
elif command -v powershell.exe >/dev/null 2>&1; then
  PSH="powershell.exe"
  CONVERT_PATHS=1
elif [[ -x /mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe ]]; then
  PSH="/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe"
  CONVERT_PATHS=1
else
  PSH="powershell"
fi

normalize_path() {
  local p="$1"
  if [[ "$CONVERT_PATHS" -eq 1 ]]; then
    to_windows_path "$p"
  else
    printf '%s' "$p"
  fi
}

map_flag() {
  case "$1" in
    --repo-dir) echo "-RepoDir" ;;
    --sdk-path) echo "-SdkPath" ;;
    --skip-pull) echo "-SkipPull" ;;
    *) echo "" ;;
  esac
}

map_inline_flag() {
  case "$1" in
    --repo-dir=*) echo "-RepoDir=${1#*=}" ;;
    --sdk-path=*) echo "-SdkPath=${1#*=}" ;;
    *) echo "" ;;
  esac
}

ARGS=()
EXPECT_PATH=0
for arg in "$@"; do
  if [[ "$EXPECT_PATH" -eq 1 ]]; then
    ARGS+=("$(normalize_path "$arg")")
    EXPECT_PATH=0
    continue
  fi

  mapped="$(map_flag "$arg")"
  if [[ -n "$mapped" ]]; then
    ARGS+=("$mapped")
    if [[ "$mapped" == "-RepoDir" || "$mapped" == "-SdkPath" ]]; then
      EXPECT_PATH=1
    fi
    continue
  fi

  mapped_inline="$(map_inline_flag "$arg")"
  if [[ -n "$mapped_inline" ]]; then
    if [[ "$mapped_inline" == -RepoDir=* || "$mapped_inline" == -SdkPath=* ]]; then
      value="${mapped_inline#*=}"
      if [[ "$mapped_inline" == -RepoDir=* ]]; then
        ARGS+=("-RepoDir=$(normalize_path "$value")")
      else
        ARGS+=("-SdkPath=$(normalize_path "$value")")
      fi
    else
      ARGS+=("$mapped_inline")
    fi
    continue
  fi

  ARGS+=("$arg")
done

PS_SCRIPT="$(normalize_path "$PS_SCRIPT")"
exec "$PSH" -NoProfile -ExecutionPolicy Bypass -File "$PS_SCRIPT" "${ARGS[@]}"
