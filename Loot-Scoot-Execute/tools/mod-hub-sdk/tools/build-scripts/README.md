# Kenshi Shared Build Scripts

Shared build, deploy, and package scripts for Kenshi plugin mods.

## Scope
This repository stores the canonical script set extracted from `Job-B-Gone/scripts`.

## Consumption (Submodule)
Add this repository as a submodule in each mod repo at `tools/build-scripts`.

## Script Layout
Single-purpose scripts:
- `build.ps1` / `build.sh`: compile only
- `deploy.ps1` / `deploy.sh`: deploy only
- `package.ps1` / `package.sh`: package only (from local workspace mod files)

Orchestration wrappers:
- `build-deploy.ps1` / `build-deploy.sh`: build then deploy
- `build-package.ps1` / `build-package.sh`: build then package

Backward-compatible aliases:
- `build-and-deploy.ps1` / `build-and-deploy.sh` -> `build-deploy`
- `build-and-package.ps1` / `build-and-package.sh` -> `build-package`

## Notes
- Shared defaults and environment resolution live in `kenshi-common.ps1`.
- Mod-specific values should be supplied through `.env` or script parameters.
- Mod name can be set via `.env` as `KENSHI_MOD_NAME` (preferred) or `MOD_NAME`.
- `package.ps1` no longer falls back to `KENSHI_PATH/mods/<mod>` when `-SourceModPath` is not supplied.
- Scripts expect to run from the consuming mod repository layout.
