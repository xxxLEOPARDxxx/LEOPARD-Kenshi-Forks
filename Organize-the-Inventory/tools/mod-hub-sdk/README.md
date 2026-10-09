## Emkejs Mod Core (shared RE_Kenshi Mod Hub runtime)

This repository contains the shared RE_Kenshi runtime and SDK surface behind `Emkejs-Mod-Core` Mod Hub integrations.

## Setup
Clone normally. Shared build scripts are tracked in `tools/build-scripts` via `git subtree`, so no submodule init step is required.
This repo also keeps local wrapper scripts under `scripts/` that delegate into `tools/build-scripts`; shared build logic should stay in the subtree, not the wrappers.
If you want to pull subtree updates from the shared build-tools repo, add a local `build-scripts` git remote in your clone and use the shared repo's documented `git subtree pull --prefix=tools/build-scripts build-scripts consumer-main` workflow.

1) Open a PowerShell terminal in this repo.
2) (Optional) Create `.env` from `.env.example` to set local paths.
3) Source the env script:
   - `. .\scripts\setup_env.ps1`

This sets:
- `KENSHILIB_DEPS_DIR`
- `KENSHILIB_DIR`
- `BOOST_INCLUDE_PATH`

## Build
You can build in Visual Studio, or via the script below.

### Scripted build + deploy
Run:
- `.\scripts\build-deploy.ps1`

Optional parameters:
- `-KenshiPath "H:\SteamLibrary\steamapps\common\Kenshi"`
- `-Configuration "Release"`
- `-Platform "x64"`

### Deploy troubleshooting (Phase 12)
- Deploy now performs a lock preflight before any file copy into the target mod folder.
- If the destination DLL is in use, deploy fails early with exit code `32` and prints:
  - the target DLL path
  - suspected process name/PID when detectable
  - concrete next steps to release the lock and retry
- Typical fix:
  - close Kenshi (and any tool/debugger loading the DLL)
  - stop the listed PID if shown
  - rerun `.\scripts\build-deploy.ps1` or `.\scripts\build-and-deploy.ps1`

## Deploy layout
Mod data folder name: `Emkejs-Mod-Core`

After deploy, expected files:
- `[Kenshi install dir]\mods\Emkejs-Mod-Core\Emkejs-Mod-Core.mod`
- `[Kenshi install dir]\mods\Emkejs-Mod-Core\RE_Kenshi.json`
- `[Kenshi install dir]\mods\Emkejs-Mod-Core\Emkejs-Mod-Core.dll`
- `[Kenshi install dir]\mods\Emkejs-Mod-Core\mod-config.json`

## Config
`mod-config.json` now includes logging flags (`debugLogging`, `debugSearchLogging`, `debugBindingLogging`) that default to `false`.

## Load Order Recommendation (Mod Hub Consumers)
- If other mods use this Mod Hub integration, place `Emkejs-Mod-Core` before those consumer mods in load order.
- Consumer mods have attach + one retry fallback logic, but loading this mod first gives the most reliable hub UI startup path.

## Hub Scaffold (Phase 9)
- Generate the standard Mod Hub adapter scaffold:
  - `./scripts/init-mod-template.sh --with-hub`
  - `./scripts/init-mod-template.ps1 -WithHub`
- Quick-start integration guide:
  - [`docs/mod-hub-sdk-quickstart.md`](docs/mod-hub-sdk-quickstart.md)
- Optional overrides:
  - `-HubNamespaceId`, `-HubNamespaceDisplayName`, `-HubModId`, `-HubModDisplayName`
  - `-HubBoolSetting`, `-HubKeybindSetting`, `-HubIntSetting`, `-HubFloatSetting`, `-HubActionRow`, `-HubSelectSetting`, `-HubTextSetting`, `-HubColorSetting` to replace the per-kind example rows with generated skeletons
  - `-HubSettingsManifest .\hub-settings.json` to load `bool_settings`, `keybind_settings`, `int_settings`, `float_settings`, `action_rows`, `select_settings`, `text_settings`, and `color_settings` from a small JSON manifest
- Output files:
  - `src/mod_hub_consumer_adapter.h`
  - `src/mod_hub_consumer_adapter.cpp`

## SDK Packaging (Phase 10)
- Build versioned SDK asset only:
  - `./scripts/package-sdk.sh`
  - `./scripts/package-sdk.ps1`
- Build mod zip + SDK zip together:
  - `./scripts/build-and-package.sh`
  - `./scripts/build-and-package.ps1`
- Run the Debug reliability smoke matrix through the build wrapper:
  - `./scripts/build-and-package.ps1 -Configuration Debug -SkipSdkPackage -RunReliabilitySmoke [-SmokeKenshiPath <path-to-Kenshi>]`
- Default SDK output:
  - `dist/Emkejs-Mod-Core-SDK-<VERSION>.zip`
- Phase 10 validation harness:
  - `./scripts/phase10_sdk_packaging_test.ps1`

## Reliability Harnesses (v1.1)
- Phase 5 numeric harness:
  - `./scripts/phase5_numeric_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll> [-KenshiPath <path-to-Kenshi>]`
  - Requires a Debug DLL built with `EMC_ENABLE_TEST_EXPORTS`.
  - Verifies numeric snap/clamp behavior plus pending-text normalization semantics (text remains user-entered until normalize, then canonicalizes).
- Phase 12 deploy lock preflight harness:
  - `./scripts/phase12_deploy_lock_preflight_test.ps1`
- Phase 13 export contract stability harness:
  - `./scripts/phase13_export_contract_stability_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll> [-KenshiPath <path-to-Kenshi>]`
  - Canonical + compatibility export checks work with Release or Debug DLLs; extra helper lookup assertions run when Debug test exports are present.
- Phase 14 options-init observer harness:
  - `./scripts/phase14_options_init_observer_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll> [-KenshiPath <path-to-Kenshi>]`
  - Requires a Debug DLL built with `EMC_ENABLE_TEST_EXPORTS`.
- Phase 15 scaffold + single-TU sample harness:
  - `./scripts/phase15_scaffold_single_tu_test.ps1`
- Phase 16 reliability smoke matrix harness:
  - `./scripts/phase16_hub_attach_reliability_smoke_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll> [-KenshiPath <path-to-Kenshi>]`
  - Requires a Debug DLL built with `EMC_ENABLE_TEST_EXPORTS`.
- Phase 17 address SSOT guard:
  - `./scripts/phase17_address_ssot_guard_test.ps1`
- Phase 18 dummy-consumer menu smoke harness:
  - `./scripts/phase18_dummy_consumer_smoke_test.ps1 -DllPath <path-to-Emkejs-Mod-Core.dll> [-KenshiPath <path-to-Kenshi>]`
  - Requires a Debug DLL built with `EMC_ENABLE_TEST_EXPORTS`.
  - Verifies registration count plus one bool commit, one int commit, and bad-value rejection paths.
- Use `-KenshiPath` / `-SmokeKenshiPath` when Kenshi runtime DLLs are not already on `PATH`.

## Address SSOT (Phase 17)
- Current address SSOT tables:
  - `docs/addresses/kenshi_1_0_65_x64.md`
  - `docs/addresses/kenshi_1_0_68_x64.md`
  - `docs/addresses/kenshi_1_0_65_x86.md`
  - `docs/addresses/kenshi_1_0_68_x86.md`
- While hub-owned game hooks still exist, hard-coded hook RVAs are restricted to:
  - `src/hub_menu_bridge.cpp`
- Any address change must:
  - update the matching `docs/addresses/*` table in the same commit
  - pass `./scripts/phase17_address_ssot_guard_test.ps1`
  - rerun the Debug reliability smoke command when hook behavior changes
- Local hook automation is the intended enforcement path for this repo; the checked-in scripts and hooks are the supported guardrail path.

## Local Git Hooks (Recommended)
- Repo-managed hooks live in:
  - `.githooks/`
- Install them for this clone:
  - `./scripts/install-git-hooks.sh`
  - `./scripts/install-git-hooks.ps1`
- Manual equivalent:
  - `git config core.hooksPath .githooks`
- Included hooks:
  - `pre-commit` runs `./scripts/phase17_address_ssot_guard_test.ps1`
  - `pre-push` runs `./scripts/build-and-package.sh -Configuration Debug -SkipSdkPackage -RunReliabilitySmoke`
- `pre-push` uses `KENSHI_PATH` / `KENSHI_DEFAULT_PATH` automatically when the Phase 16 smoke path needs Kenshi runtime DLLs.
- Hooks are the intended automation path for this repo. They can still be bypassed with `--no-verify`.

## License
This project is licensed under the GNU General Public License v3.0.
It uses KenshiLib, which is released under GPLv3.
