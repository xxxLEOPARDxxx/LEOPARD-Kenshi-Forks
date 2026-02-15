# AGENTS.md

## Commit Message Convention
- Use Conventional Commit format.
- Use imperative mood with an uppercase first letter in the subject after the type prefix.
- Example: `feat: Add selected-member delete-all action`

## Build Verification Policy
- For compile verification, use the build-only flow (build script / direct project build).
- Do not use `scripts/build-and-package.sh` just to verify compilation.
- Run packaging only when a package artifact is explicitly needed.

## Build Scripts
- Build script: `scripts/build-deploy.sh` (`scripts/build-deploy.ps1`).
- Build + deploy script: `scripts/build-and-deploy.sh` (`scripts/build-and-deploy.ps1`).
- Build + package script: `scripts/build-and-package.sh` (`scripts/build-and-package.ps1`).
- Package-only script: `scripts/package.sh` (`scripts/package.ps1`).

## Compile-Only Checks
- To only verify compilation passes, use a build-only invocation (direct project build), not packaging.
- Do not use `scripts/build-and-package.sh` for compile-only checks.
