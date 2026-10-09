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
    --hub-keybind-setting) echo "-HubKeybindSetting" ;;
    --hub-int-setting) echo "-HubIntSetting" ;;
    --hub-float-setting) echo "-HubFloatSetting" ;;
    --hub-action-row) echo "-HubActionRow" ;;
    --hub-select-setting) echo "-HubSelectSetting" ;;
    --hub-text-setting) echo "-HubTextSetting" ;;
    --hub-color-setting) echo "-HubColorSetting" ;;
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
    --hub-keybind-setting=*) echo "-HubKeybindSetting=${1#*=}" ;;
    --hub-int-setting=*) echo "-HubIntSetting=${1#*=}" ;;
    --hub-float-setting=*) echo "-HubFloatSetting=${1#*=}" ;;
    --hub-action-row=*) echo "-HubActionRow=${1#*=}" ;;
    --hub-select-setting=*) echo "-HubSelectSetting=${1#*=}" ;;
    --hub-text-setting=*) echo "-HubTextSetting=${1#*=}" ;;
    --hub-color-setting=*) echo "-HubColorSetting=${1#*=}" ;;
    *) echo "" ;;
  esac
}

ARGS=()
BOOL_SETTINGS=()
KEYBIND_SETTINGS=()
INT_SETTINGS=()
FLOAT_SETTINGS=()
ACTION_ROWS=()
SELECT_SETTINGS=()
TEXT_SETTINGS=()
COLOR_SETTINGS=()
EXPECT_PATH=0
EXPECT_HUB_LIST_KIND=""
HAS_KENSHI_PATH=0
for arg in "$@"; do
  if [[ "$EXPECT_PATH" -eq 1 ]]; then
    ARGS+=("$(normalize_path "$arg")")
    EXPECT_PATH=0
    continue
  fi

  if [[ -n "$EXPECT_HUB_LIST_KIND" ]]; then
    case "$EXPECT_HUB_LIST_KIND" in
      bool) BOOL_SETTINGS+=("$arg") ;;
      keybind) KEYBIND_SETTINGS+=("$arg") ;;
      int) INT_SETTINGS+=("$arg") ;;
      float) FLOAT_SETTINGS+=("$arg") ;;
      action) ACTION_ROWS+=("$arg") ;;
      select) SELECT_SETTINGS+=("$arg") ;;
      text) TEXT_SETTINGS+=("$arg") ;;
      color) COLOR_SETTINGS+=("$arg") ;;
    esac
    EXPECT_HUB_LIST_KIND=""
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
      EXPECT_HUB_LIST_KIND="bool"
    elif [[ "$mapped" == "-HubKeybindSetting" ]]; then
      EXPECT_HUB_LIST_KIND="keybind"
    elif [[ "$mapped" == "-HubIntSetting" ]]; then
      EXPECT_HUB_LIST_KIND="int"
    elif [[ "$mapped" == "-HubFloatSetting" ]]; then
      EXPECT_HUB_LIST_KIND="float"
    elif [[ "$mapped" == "-HubActionRow" ]]; then
      EXPECT_HUB_LIST_KIND="action"
    elif [[ "$mapped" == "-HubSelectSetting" ]]; then
      EXPECT_HUB_LIST_KIND="select"
    elif [[ "$mapped" == "-HubTextSetting" ]]; then
      EXPECT_HUB_LIST_KIND="text"
    elif [[ "$mapped" == "-HubColorSetting" ]]; then
      EXPECT_HUB_LIST_KIND="color"
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
    elif [[ "$mapped_inline" == -HubKeybindSetting=* ]]; then
      KEYBIND_SETTINGS+=("${mapped_inline#*=}")
    elif [[ "$mapped_inline" == -HubIntSetting=* ]]; then
      INT_SETTINGS+=("${mapped_inline#*=}")
    elif [[ "$mapped_inline" == -HubFloatSetting=* ]]; then
      FLOAT_SETTINGS+=("${mapped_inline#*=}")
    elif [[ "$mapped_inline" == -HubActionRow=* ]]; then
      ACTION_ROWS+=("${mapped_inline#*=}")
    elif [[ "$mapped_inline" == -HubSelectSetting=* ]]; then
      SELECT_SETTINGS+=("${mapped_inline#*=}")
    elif [[ "$mapped_inline" == -HubTextSetting=* ]]; then
      TEXT_SETTINGS+=("${mapped_inline#*=}")
    elif [[ "$mapped_inline" == -HubColorSetting=* ]]; then
      COLOR_SETTINGS+=("${mapped_inline#*=}")
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
    EXPECT_HUB_LIST_KIND="bool"
  fi
  if [[ "$arg" == "-HubKeybindSetting" ]]; then
    EXPECT_HUB_LIST_KIND="keybind"
  fi
  if [[ "$arg" == "-HubIntSetting" ]]; then
    EXPECT_HUB_LIST_KIND="int"
  fi
  if [[ "$arg" == "-HubFloatSetting" ]]; then
    EXPECT_HUB_LIST_KIND="float"
  fi
  if [[ "$arg" == "-HubActionRow" ]]; then
    EXPECT_HUB_LIST_KIND="action"
  fi
  if [[ "$arg" == "-HubSelectSetting" ]]; then
    EXPECT_HUB_LIST_KIND="select"
  fi
  if [[ "$arg" == "-HubTextSetting" ]]; then
    EXPECT_HUB_LIST_KIND="text"
  fi
  if [[ "$arg" == "-HubColorSetting" ]]; then
    EXPECT_HUB_LIST_KIND="color"
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

if [[ "${#KEYBIND_SETTINGS[@]}" -gt 0 ]]; then
  joined_keybind_settings="$(IFS=,; printf '%s' "${KEYBIND_SETTINGS[*]}")"
  ARGS+=("-HubKeybindSetting")
  ARGS+=("$joined_keybind_settings")
fi

if [[ "${#INT_SETTINGS[@]}" -gt 0 ]]; then
  joined_int_settings="$(IFS=,; printf '%s' "${INT_SETTINGS[*]}")"
  ARGS+=("-HubIntSetting")
  ARGS+=("$joined_int_settings")
fi

if [[ "${#FLOAT_SETTINGS[@]}" -gt 0 ]]; then
  joined_float_settings="$(IFS=,; printf '%s' "${FLOAT_SETTINGS[*]}")"
  ARGS+=("-HubFloatSetting")
  ARGS+=("$joined_float_settings")
fi

if [[ "${#ACTION_ROWS[@]}" -gt 0 ]]; then
  joined_action_rows="$(IFS=,; printf '%s' "${ACTION_ROWS[*]}")"
  ARGS+=("-HubActionRow")
  ARGS+=("$joined_action_rows")
fi

if [[ "${#SELECT_SETTINGS[@]}" -gt 0 ]]; then
  joined_select_settings="$(IFS=,; printf '%s' "${SELECT_SETTINGS[*]}")"
  ARGS+=("-HubSelectSetting")
  ARGS+=("$joined_select_settings")
fi

if [[ "${#TEXT_SETTINGS[@]}" -gt 0 ]]; then
  joined_text_settings="$(IFS=,; printf '%s' "${TEXT_SETTINGS[*]}")"
  ARGS+=("-HubTextSetting")
  ARGS+=("$joined_text_settings")
fi

if [[ "${#COLOR_SETTINGS[@]}" -gt 0 ]]; then
  joined_color_settings="$(IFS=,; printf '%s' "${COLOR_SETTINGS[*]}")"
  ARGS+=("-HubColorSetting")
  ARGS+=("$joined_color_settings")
fi

PS_SCRIPT="$(normalize_path "$PS_SCRIPT")"
exec "$PSH" -NoProfile -ExecutionPolicy Bypass -File "$PS_SCRIPT" "${ARGS[@]}"
