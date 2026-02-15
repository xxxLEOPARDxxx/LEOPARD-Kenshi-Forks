# AGENTS.md

## Commit Message Convention
- Use Conventional Commit format.
- Use imperative mood with an uppercase first letter in the subject after the type prefix.
- Example: `feat: Add selected-member delete-all action`

## Build Verification Policy
- For compile verification, use the build-only flow (build script / direct project build).
- Do not use `scripts/build-and-package.sh` just to verify compilation.
- Run packaging only when a package artifact is explicitly needed.
