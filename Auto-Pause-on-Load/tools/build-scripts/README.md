# Kenshi shared build scripts

Shared build, deploy, and package scripts for Kenshi plugin mods.

This `consumer/` directory is the shared build-scripts source inside `kenshi-mod-build-scripts`. It is not a mod repo by itself; consumer mod repos import it at `tools/build-scripts`.

## Consumption (Subtree)
Import this directory into each mod repo at `tools/build-scripts` via `git subtree`.

Initial import:
```bash
git remote add build-scripts git@github.com:Emkej/kenshi-mod-build-scripts.git
git fetch build-scripts consumer-main
git subtree add --prefix=tools/build-scripts build-scripts consumer-main
```

Update an existing consumer:
```bash
git fetch build-scripts consumer-main
git subtree pull --prefix=tools/build-scripts build-scripts consumer-main
```

## Script layout
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
