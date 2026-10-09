# Build Scripts Subtree Migration Plan (2026-03-07)

## Status
Implemented on 2026-03-07.

Actual results:
- `Emkejs-Mod-Core` now consumes `tools/build-scripts` as regular tracked files imported from the shared consumer subtree.
- `Loot-Scoot-Execute` now consumes `tools/build-scripts` the same way.
- `tools/mod-hub-sdk` remains a submodule in consumer repos where it was already used that way.

Reference commits:
- `Emkejs-Mod-Core`: `1c073bf`, `195b036`
- `Loot-Scoot-Execute`: `d65fe3e`, `e842d66`

## Summary
This plan migrates shared build script consumption from the current submodule/vendored-copy hybrid into a clean `git subtree` workflow.

The shared repository remains the single source of truth:
- shared repo: `/mnt/i/Kenshi_modding/kenshi-mod-build-scripts`
- consumer repo: `/mnt/i/Kenshi_modding/Emkejs-Mod-Core`

The plan keeps one shared repository, but separates:
- consumer-facing subtree content
- maintainer-only repo-management tooling

## Execution Checklist
- [x] Reshape `kenshi-mod-build-scripts` into `consumer/` + `maintainer/`
- [x] Untrack `.env` and rewrite both shared repo READMEs for their correct audience
- [x] Generate and validate `consumer-main`
- [x] Prove subtree import in a scratch clone/worktree
- [x] Clean or park unrelated Mod Core changes
- [x] Migrate `Emkejs-Mod-Core/tools/build-scripts` to subtree
- [x] Update Mod Core wrapper/doc wording away from submodule language
- [x] Run build/package validation after migration

## Current State
- Shared PowerShell script content is aligned between:
  - `kenshi-mod-build-scripts`
  - `Emkejs-Mod-Core/tools/build-scripts`
- Remaining differences are shared-repo-only tracked files:
  - `.env`
  - `.env.example`
  - `mod-repos.txt`
  - `update-mod-repos.sh`
- `Emkejs-Mod-Core/.gitmodules` still declares `tools/build-scripts` as a submodule path.
- Multiple local wrapper scripts and docs in Mod Core still tell users to run `git submodule update --init --recursive`.
- The current hybrid state creates two unrelated histories even when file contents match.

## Goal
Create a history-aware subtree workflow where:
- shared scripts are maintained once
- consumer repos pull those scripts via `git subtree`
- maintainer-only files do not get copied into consumer repos
- future syncs are branch-based instead of manual copy/sync

## Decision
Use one shared repo with two logical areas:
- `consumer/`: all files intended for `tools/build-scripts` in mod repos
- `maintainer/`: repo-management tooling and local maintainer config support

Use two shared-repo branches:
- `main`: editable source branch
- `consumer-main`: generated branch from `consumer/`, used only for subtree import/pull

`consumer-main` should be treated as generated output and not edited directly.

## Desired Shared Repo Layout
Inside `kenshi-mod-build-scripts`:

- root:
  - `README.md` (maintainer-facing)
  - `.gitignore`
  - `consumer/`
  - `maintainer/`
- `consumer/`:
  - all build/deploy/package scripts
  - templates
  - consumer-facing `README.md`
  - consumer `.gitignore`
  - `AGENTS.md`
- `maintainer/`:
  - `update-mod-repos.sh`
  - `mod-repos.txt`
  - `.env.example`

Local-only files:
- `.env` should be ignored, not tracked
- `.idea/` should remain ignored

## Files That Must Stay Out Of Consumers
These files should not enter `tools/build-scripts` via subtree:
- `.env`
- `.env.example`
- `mod-repos.txt`
- `update-mod-repos.sh`

Reason:
- they are maintainer-only, not consumer runtime/build dependencies
- `git subtree add/pull` imports tracked content under the chosen prefix
- keeping them in the subtree source would copy them into every mod repo

## Phase 1: Shared Repo Reshape
Repository:
- `/mnt/i/Kenshi_modding/kenshi-mod-build-scripts`

Steps:
1. Merge the current feature branch into `main`.
2. Create:
   - `consumer/`
   - `maintainer/`
3. Move consumer-facing files into `consumer/`:
   - `AGENTS.md`
   - `README.md`
   - `.gitignore`
   - `_env.sh`
   - `_run-powershell.sh`
   - `build*.ps1`
   - `build*.sh`
   - `deploy.ps1`
   - `deploy.sh`
   - `package.ps1`
   - `package.sh`
   - `init-mod-template.ps1`
   - `kenshi-common.ps1`
   - `load-env.ps1`
   - `setup_env.ps1`
   - `templates/`
4. Move maintainer-only tracked files into `maintainer/`:
   - `.env.example`
   - `mod-repos.txt`
   - `update-mod-repos.sh`
5. Untrack local-only maintainer config that should not stay in Git:
   - `git rm --cached .env`
6. Rewrite `consumer/README.md` so it is consumer-facing only:
   - subtree pull/update guidance only
   - no submodule setup instructions
   - no maintainer-tooling content
7. Replace root `.gitignore` with a maintainer-oriented version that ignores:
   - `.env`
   - `.idea/`
8. Replace root `README.md` with a maintainer-facing document that explains:
   - `consumer/` is the subtree source
   - `maintainer/` contains repo-maintainer tooling
   - `consumer-main` is generated from `consumer/`
9. Commit the reshape on `main`.

Command sketch:
```bash
git checkout main
git merge feat/add-timestamp-to-output
mkdir consumer maintainer
git mv AGENTS.md README.md .gitignore _env.sh _run-powershell.sh build*.ps1 build*.sh deploy.ps1 deploy.sh package.ps1 package.sh init-mod-template.ps1 kenshi-common.ps1 load-env.ps1 setup_env.ps1 templates consumer/
git mv .env.example mod-repos.txt update-mod-repos.sh maintainer/
git rm --cached .env
git add .
git commit -m "refactor: Separate consumer subtree from maintainer tooling"
```

Validation:
- `git diff --check`
- verify `.env` is no longer tracked and is ignored
- verify `consumer/` contains everything needed for a consumer repo build path
- verify `consumer/README.md` no longer references submodule consumption or maintainer-only tooling
- verify maintainer-only files are no longer under `consumer/`

Rollback:
- reset to the pre-reshape commit in the shared repo if layout validation fails

## Phase 2: Publish Consumer Branch
Repository:
- `/mnt/i/Kenshi_modding/kenshi-mod-build-scripts`

Steps:
1. Generate a subtree branch from `consumer/`:
   - `git subtree split --prefix=consumer -b consumer-main`
2. Push:
   - `main`
   - `consumer-main`

Command sketch:
```bash
git subtree split --prefix=consumer -b consumer-main
git push origin main consumer-main
```

Validation:
- inspect `consumer-main` tree and confirm it matches the desired `tools/build-scripts` content exactly
- confirm excluded maintainer files are absent from `consumer-main`

Rollback:
- delete and regenerate `consumer-main` if the split is wrong

## Phase 2.5: Scratch Import Gate
Repository:
- temporary clone or worktree of `/mnt/i/Kenshi_modding/Emkejs-Mod-Core`

Purpose:
- prove that `consumer-main` imports cleanly before touching the live Mod Core working tree

Steps:
1. Create a temporary clone or worktree of `Emkejs-Mod-Core`.
2. Add the shared repo as a remote in that scratch copy.
3. Import `consumer-main` into `tools/build-scripts` with `git subtree add`.
4. Compare the imported tree against the currently aligned `tools/build-scripts` content.

Command sketch:
```bash
git worktree add /tmp/emc-subtree-check <mod-core-branch>
git -C /tmp/emc-subtree-check remote add build-scripts git@github.com:Emkej/kenshi-mod-build-scripts.git
git -C /tmp/emc-subtree-check fetch build-scripts consumer-main
git -C /tmp/emc-subtree-check subtree add --prefix=tools/build-scripts build-scripts consumer-main
diff -rq /tmp/emc-subtree-check/tools/build-scripts /mnt/i/Kenshi_modding/Emkejs-Mod-Core/tools/build-scripts
```

Validation:
- `diff -rq` between the scratch imported `tools/build-scripts` and the known-good aligned content should show no differences
- scratch wrappers should still resolve `tools/build-scripts/*.ps1`

Rollback:
- delete the temporary clone/worktree

## Phase 3: Prepare Emkejs-Mod-Core
Repository:
- `/mnt/i/Kenshi_modding/Emkejs-Mod-Core`

Precondition:
- existing unrelated local changes must be committed or intentionally parked before migration
- do not mix subtree migration with unrelated Mod Hub UI or wrapper work

Steps:
1. Add the shared repo as a normal remote:
   - `build-scripts`
2. Fetch:
   - `consumer-main`
3. Remove stale submodule declaration:
   - remove the `tools/build-scripts` entry from `.gitmodules`
   - delete `.gitmodules` only if it becomes empty
4. Identify and update submodule-specific docs/messages:
   - `README.md`
   - `scripts/build-and-package.ps1`
   - `scripts/build-and-deploy.ps1`
   - `scripts/package.ps1`
   - `scripts/build-deploy.ps1`
   - `scripts/init-mod-template.ps1`
   - `scripts/load-env.ps1`
   - `scripts/setup_env.ps1`

Message changes should replace:
- `git submodule update --init --recursive`

With subtree-compatible guidance, for example:
- sync shared build scripts from the shared repo branch

Command sketch:
```bash
git remote add build-scripts git@github.com:Emkej/kenshi-mod-build-scripts.git
git fetch build-scripts consumer-main
```

Validation:
- targeted grep over the files above should no longer contain `git submodule update --init --recursive` or describe `tools/build-scripts` as a submodule

Rollback:
- restore `.gitmodules` and wrapper/doc messages if migration preparation is aborted

## Phase 4: Replace Vendored Copy With Subtree
Repository:
- `/mnt/i/Kenshi_modding/Emkejs-Mod-Core`

Steps:
1. Remove the existing tracked `tools/build-scripts` directory in a dedicated commit.
2. Import the shared consumer branch as a subtree:
   - `git subtree add --prefix=tools/build-scripts build-scripts consumer-main`
3. Do not use `--squash`.

Reason for avoiding `--squash`:
- preserves a usable history bridge for future subtree split/pull operations
- makes consumer-first fixes easier to upstream later

Validation:
- `diff -rq tools/build-scripts <shared-consumer-tree>` should show no differences
- local wrappers should still resolve `tools/build-scripts/*.ps1`

Command sketch:
```bash
git rm -r tools/build-scripts
git commit -m "chore: Remove vendored build scripts before subtree import"
git subtree add --prefix=tools/build-scripts build-scripts consumer-main
```

Rollback:
- revert the subtree-add commit
- restore the prior tracked directory if needed

## Phase 5: Validate In Emkejs-Mod-Core
Repository:
- `/mnt/i/Kenshi_modding/Emkejs-Mod-Core`

Minimum validation:
1. `scripts/build-and-package.ps1 -Configuration Debug -Platform x64 -SkipSdkPackage`
2. confirm final timestamp footer still appears exactly once
3. verify package staging path remains:
   - `.packaging/<mod>`
4. verify deploy preflight behavior still exists in shared `deploy.ps1`

Recommended additional validation:
1. one failure-path build/package check to confirm footer still appears on error
2. one deploy preflight check if a locked DLL scenario is available

Rollback:
- revert the subtree import and wrapper/doc cleanup commits

## Phase 6: Ongoing Workflow After Migration
### Shared-first change flow
1. edit `consumer/` in `kenshi-mod-build-scripts` on `main`
2. commit
3. regenerate `consumer-main`
4. push `main` and `consumer-main`
5. in each mod repo:
   - `git subtree pull --prefix=tools/build-scripts build-scripts consumer-main`

## Appendix: Post-Migration Consumer-first Quick Fix Flow
Use this only after the shared-first migration has already succeeded in at least one real consumer repo.

1. edit `tools/build-scripts` in one mod repo
2. commit locally
3. split that subtree history
4. pull it back into shared repo `consumer/`
5. validate in shared repo
6. regenerate `consumer-main`
7. other mods pull from `consumer-main`

## Required Follow-up In Shared Repo
`maintainer/update-mod-repos.sh` currently assumes a submodule workflow.

It must be either:
- rewritten for subtree-based updates, or
- retired if manual `git subtree pull` is acceptable

This is follow-up work and should not block the initial migration.

## Risks
1. Mixed migration + unrelated local changes can create avoidable conflicts.
2. Forgetting to exclude maintainer-only files would leak them into every consumer repo.
3. Keeping `consumer-main` hand-editable would create drift against `main`.
4. Using `--squash` would make future two-way subtree work less practical.

## Stop Conditions
Abort the migration and fix the upstream/shared state first if any of the following happens:
1. `consumer/README.md` still contains submodule instructions after the shared repo reshape.
2. `consumer-main` contains `.env`, `.env.example`, `mod-repos.txt`, or `update-mod-repos.sh`.
3. The scratch subtree import differs from the known-good aligned `tools/build-scripts` content.
4. Targeted grep in Mod Core still describes `tools/build-scripts` as a submodule after wrapper/doc cleanup.
5. `scripts/build-and-package.ps1 -Configuration Debug -Platform x64 -SkipSdkPackage` fails after migration.

## Acceptance Criteria
This plan is complete only when all of the following are true:
1. `kenshi-mod-build-scripts` has a `consumer/` subtree source and `maintainer/` area.
2. `consumer-main` exists and contains only consumer-facing build script content.
3. `Emkejs-Mod-Core/tools/build-scripts` is imported from `consumer-main` via `git subtree`.
4. `Emkejs-Mod-Core` no longer instructs users to initialize `tools/build-scripts` as a submodule.
5. `scripts/build-and-package.ps1 -Configuration Debug -Platform x64 -SkipSdkPackage` passes after migration.
6. Maintainer-only files are absent from consumer repos.
