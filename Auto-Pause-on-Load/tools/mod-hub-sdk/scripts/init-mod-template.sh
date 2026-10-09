#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR_UNIX="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR_UNIX/_env.sh"
PS_SCRIPT="$SCRIPT_DIR_UNIX/init-mod-template.ps1"
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
    --kenshi-path) echo "-KenshiPath" ;;
    --mod-name) echo "-ModName" ;;
    --dll-name) echo "-DllName" ;;
    --mod-file-name) echo "-ModFileName" ;;
    --config-file-name) echo "-ConfigFileName" ;;
    --hub-namespace-id) echo "-HubNamespaceId" ;;
    --hub-namespace-display-name) echo "-HubNamespaceDisplayName" ;;
    --hub-mod-id) echo "-HubModId" ;;
    --hub-mod-display-name) echo "-HubModDisplayName" ;;
    --hub-settings-manifest) echo "-HubSettingsManifest" ;;
    --hub-bool-setting) echo "-HubBoolSetting" ;;
    *) echo "" ;;
  esac
}

map_inline_flag() {
  case "$1" in
    --repo-dir=*) echo "-RepoDir=${1#*=}" ;;
    --kenshi-path=*) echo "-KenshiPath=${1#*=}" ;;
    --mod-name=*) echo "-ModName=${1#*=}" ;;
    --dll-name=*) echo "-DllName=${1#*=}" ;;
    --mod-file-name=*) echo "-ModFileName=${1#*=}" ;;
    --config-file-name=*) echo "-ConfigFileName=${1#*=}" ;;
    --hub-namespace-id=*) echo "-HubNamespaceId=${1#*=}" ;;
    --hub-namespace-display-name=*) echo "-HubNamespaceDisplayName=${1#*=}" ;;
    --hub-mod-id=*) echo "-HubModId=${1#*=}" ;;
    --hub-mod-display-name=*) echo "-HubModDisplayName=${1#*=}" ;;
    --hub-settings-manifest=*) echo "-HubSettingsManifest=${1#*=}" ;;
    --hub-bool-setting=*) echo "-HubBoolSetting=${1#*=}" ;;
    *) echo "" ;;
  esac
}

ARGS=()
BOOL_SETTINGS=()
EXPECT_PATH=0
EXPECT_BOOL_SETTING=0
HAS_KENSHI_PATH=0
for arg in "$@"; do
  if [[ "$EXPECT_PATH" -eq 1 ]]; then
    ARGS+=("$(normalize_path "$arg")")
    EXPECT_PATH=0
    continue
  fi

  if [[ "$EXPECT_BOOL_SETTING" -eq 1 ]]; then
    BOOL_SETTINGS+=("$arg")
    EXPECT_BOOL_SETTING=0
    continue
  fi

  if [[ "$arg" == "--with-hub" ]]; then
    ARGS+=("-WithHub")
    continue
  fi

  if [[ "$arg" == "--with-hub-single-tu-sample" ]]; then
    ARGS+=("-WithHubSingleTuSample")
    continue
  fi

  mapped="$(map_flag "$arg")"
  if [[ -n "$mapped" ]]; then
    if [[ "$mapped" == "-RepoDir" || "$mapped" == "-KenshiPath" || "$mapped" == "-HubSettingsManifest" ]]; then
      ARGS+=("$mapped")
      EXPECT_PATH=1
    elif [[ "$mapped" == "-HubBoolSetting" ]]; then
      EXPECT_BOOL_SETTING=1
    else
      ARGS+=("$mapped")
    fi
    if [[ "$mapped" == "-KenshiPath" ]]; then
      HAS_KENSHI_PATH=1
    fi
    continue
  fi

  mapped_inline="$(map_inline_flag "$arg")"
  if [[ -n "$mapped_inline" ]]; then
    if [[ "$mapped_inline" == -RepoDir=* || "$mapped_inline" == -KenshiPath=* || "$mapped_inline" == -HubSettingsManifest=* ]]; then
      value="${mapped_inline#*=}"
      if [[ "$mapped_inline" == -RepoDir=* ]]; then
        ARGS+=("-RepoDir=$(normalize_path "$value")")
      elif [[ "$mapped_inline" == -HubSettingsManifest=* ]]; then
        ARGS+=("-HubSettingsManifest=$(normalize_path "$value")")
      else
        ARGS+=("-KenshiPath=$(normalize_path "$value")")
        HAS_KENSHI_PATH=1
      fi
    elif [[ "$mapped_inline" == -HubBoolSetting=* ]]; then
      BOOL_SETTINGS+=("${mapped_inline#*=}")
    else
      ARGS+=("$mapped_inline")
    fi
    continue
  fi

  ARGS+=("$arg")
  if [[ "$arg" == "-RepoDir" || "$arg" == "-KenshiPath" || "$arg" == "-HubSettingsManifest" ]]; then
    EXPECT_PATH=1
  fi
  if [[ "$arg" == "-HubBoolSetting" ]]; then
    EXPECT_BOOL_SETTING=1
  fi
  if [[ "$arg" == "-KenshiPath" ]]; then
    HAS_KENSHI_PATH=1
  fi
done

if [[ "$CONVERT_PATHS" -eq 0 && "$HAS_KENSHI_PATH" -eq 0 ]]; then
  ARGS+=("-KenshiPath" ".")
fi

if [[ "${#BOOL_SETTINGS[@]}" -gt 0 ]]; then
  joined_bool_settings="$(IFS=,; printf '%s' "${BOOL_SETTINGS[*]}")"
  ARGS+=("-HubBoolSetting")
  ARGS+=("$joined_bool_settings")
fi

PS_SCRIPT="$(normalize_path "$PS_SCRIPT")"
exec "$PSH" -NoProfile -ExecutionPolicy Bypass -File "$PS_SCRIPT" "${ARGS[@]}"
