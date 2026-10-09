#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
HOOK_DIR="$REPO_ROOT/.githooks"

if [[ ! -d "$HOOK_DIR" ]]; then
  echo "ERROR: Hook directory not found: $HOOK_DIR" >&2
  exit 1
fi

chmod +x "$HOOK_DIR/pre-commit" "$HOOK_DIR/pre-push"

git -C "$REPO_ROOT" config core.hooksPath .githooks

if [[ "$(git -C "$REPO_ROOT" config --get core.hooksPath)" != ".githooks" ]]; then
  echo "ERROR: core.hooksPath verification failed." >&2
  exit 1
fi

echo "Configured core.hooksPath=.githooks"
