# Kenshi Mod Workspace Plan (YAML Only)

This plan is YAML-only to serve as a single source of truth.

```yaml
type: kenshi_mod_scaffolding
task: Create and configure a new Kenshi mod repository from a template
principles:
  - Keep KenshiLib_Examples read-only reference only.
  - Use shared deps from KenshiLib_Examples_deps.
  - Prefer portability (env vars) over hard-coded relative paths.
  - Copy-first, no destructive moves.
non_goals:
  - No code refactors.
  - No new features.
  - No moving existing example mods.
decision_points:
  - MOD_NAME: Final mod name (e.g., HelloWorldPlus).
  - WORKSPACE_ROOT: Root folder for mod repos (e.g., I:\Kenshi_modding).
  - RENAME_MOD_DATA: Whether to rename the mod data folder and .mod file to match MOD_NAME.
assumptions:
  - VS v100 toolset (VS2010 compilers) is installed.
  - KenshiLib_Examples_deps is present at DEPS_PATH.
inputs:
  MOD_NAME: The new mod name.
  WORKSPACE_ROOT: Absolute path where the new mod repo will be created.
  EXAMPLE_MOD_PATH: Path to the example mod template (HelloWorld).
  DEPS_PATH: Path to KenshiLib_Examples_deps.
outputs:
  MOD_DIR_PATH: "{WORKSPACE_ROOT}\\{MOD_NAME}"
  MOD_SOLUTION_PATH: "{WORKSPACE_ROOT}\\{MOD_NAME}\\{MOD_NAME}.sln"
  MOD_PROJECT_PATH: "{WORKSPACE_ROOT}\\{MOD_NAME}\\{MOD_NAME}.vcxproj"
  MOD_DATA_PATH: "{WORKSPACE_ROOT}\\{MOD_NAME}\\{MOD_NAME}"

steps:
  - title: Create Mod Directory
    details: Create a new directory for the mod using MOD_NAME inside WORKSPACE_ROOT.
    inputs: [MOD_NAME, WORKSPACE_ROOT]
    verification:
      command: 'Test-Path -Path "{WORKSPACE_ROOT}\\{MOD_NAME}"'

  - title: Initialize Git Repository
    details: Initialize a new Git repository within the new mod directory.
    inputs: [MOD_DIR_PATH]
    verification:
      command: 'Test-Path -Path "{MOD_DIR_PATH}\\.git"'

  - title: Create .gitignore
    details: Create a .gitignore in the mod root with standard VS C++ ignores.
    inputs: [MOD_DIR_PATH]
    artifacts:
      gitignore_content: |
        # Visual Studio
        .vs/
        *.suo
        *.user
        *.vcxproj.user
        *.sdf
        *.opensdf
        *.VC.db

        # Build outputs
        [Dd]ebug/
        [Rr]elease/
        x64/
        build/

        # Files
        *.obj
        *.pdb
        *.ilk
        *.tlog
    verification:
      command: 'Test-Path -Path "{MOD_DIR_PATH}\\.gitignore" -PathType Leaf'

  - title: Copy Template Files
    details: Copy source, project, and data files from EXAMPLE_MOD_PATH into MOD_DIR_PATH. Do not copy build outputs.
    inputs: [EXAMPLE_MOD_PATH, MOD_DIR_PATH]
    copy_list:
      source_project:
        - HelloWorld.cpp
        - HelloWorld.vcxproj
        - HelloWorld.vcxproj.filters
        - README.md
        - scripts/build-and-deploy.ps1
        - include/
        - ammintrin.h
      mod_data:
        - HelloWorld\\HelloWorld.mod
        - HelloWorld\\RE_Kenshi.json
    skip:
      - Debug/
      - x64/
      - HelloWorld\\x64/
    verification:
      command: 'Test-Path -Path "{MOD_DIR_PATH}\\HelloWorld.cpp"'

  - title: Rename Core Files and Mod Data
    details: Rename files and folders to match MOD_NAME. Update RE_Kenshi.json to reference MOD_NAME.dll.
    inputs: [MOD_NAME, MOD_DIR_PATH, RENAME_MOD_DATA]
    actions:
      - Rename HelloWorld.cpp -> {MOD_NAME}.cpp
      - Rename HelloWorld.vcxproj -> {MOD_NAME}.vcxproj
      - Rename HelloWorld.vcxproj.filters -> {MOD_NAME}.vcxproj.filters
      - If RENAME_MOD_DATA is true:
          - Rename folder HelloWorld/ -> {MOD_NAME}/
          - Rename file HelloWorld.mod -> {MOD_NAME}.mod
      - Update RE_Kenshi.json Plugins list to use {MOD_NAME}.dll
    verification:
      command: 'Test-Path -Path "{MOD_DIR_PATH}\\{MOD_NAME}.cpp"'

  - title: Create Visual Studio Solution
    details: Create a new .sln file named after MOD_NAME. Using a solution file is standard practice for easier project management.
    inputs: [MOD_NAME, MOD_DIR_PATH]
    verification:
      command: 'Test-Path -Path "{MOD_DIR_PATH}\\{MOD_NAME}.sln"'

  - title: Update Project File References
    details: Update the .vcxproj to reference the renamed source file, TargetName, and RootNamespace.
    inputs: [MOD_NAME, MOD_PROJECT_PATH]
    verification:
      command: '(Get-Content -Path "{MOD_PROJECT_PATH}") | Select-String -Pattern "<RootNamespace>{MOD_NAME}</RootNamespace>"'

  - title: Create Environment Setup Script
    details: Create scripts/setup_env.ps1 to define dependency environment variables for portability. The script uses the DEPS_PATH provided during setup.
    inputs: [DEPS_PATH, MOD_DIR_PATH]
    artifacts:
      setup_env_ps1: |
        # Run this script in your PowerShell terminal before opening Visual Studio:
        # . .\scripts\setup_env.ps1
        # The leading dot sources the script in the current scope.
        
        # This path is configured during the project scaffolding.
        $env:KENSHILIB_DEPS_DIR = "{DEPS_PATH}"
        $env:KENSHILIB_DIR = Join-Path $env:KENSHILIB_DEPS_DIR "KenshiLib"
        $env:BOOST_INCLUDE_PATH = Join-Path $env:KENSHILIB_DEPS_DIR "boost_1_60_0"

        Write-Host "Environment variables set for this session:"
        Write-Host "KENSHILIB_DEPS_DIR = $env:KENSHILIB_DEPS_DIR"
    verification:
      command: 'Test-Path -Path "{MOD_DIR_PATH}\\scripts\\setup_env.ps1"'

  - title: Stage Shared Deps Folder
    details: Copy KenshiLib_Examples_deps into a shared workspace deps folder (copy-only, no destructive moves).
    inputs: [DEPS_PATH, WORKSPACE_ROOT]
    outputs:
      SHARED_DEPS_PATH: "{WORKSPACE_ROOT}\\_deps\\KenshiLib_Examples_deps"
    verification:
      command: 'Test-Path -Path "{WORKSPACE_ROOT}\\_deps\\KenshiLib_Examples_deps"'

  - title: Port Dependency Paths to Env Vars
    details: Replace hard-coded relative deps in the .vcxproj with env vars like $(KENSHILIB_DIR).
    inputs: [MOD_PROJECT_PATH]
    verification:
      command: '(Get-Content -Path "{MOD_PROJECT_PATH}") | Select-String -Pattern "\$\(KENSHILIB_DIR\)"'

  - title: Update Build and Deploy Script
    details: Update scripts/build-and-deploy.ps1 to use MOD_NAME for DLL, mod folder, and deploy paths. Ensure KenshiPath remains configurable.
    inputs: [MOD_NAME, MOD_DIR_PATH]
    verification:
      command: '(Get-Content -Path "{MOD_DIR_PATH}\\scripts\\build-and-deploy.ps1") | Select-String -Pattern "{MOD_NAME}"'

  - title: Enhance Build Script for RE_Kenshi.json
    details: Add logic to update RE_Kenshi.json Plugins list with the new DLL name during build.
    inputs: [MOD_DIR_PATH]
    verification:
      command: '(Get-Content -Path "{MOD_DIR_PATH}\\scripts\\build-and-deploy.ps1") | Select-String -Pattern "RE_Kenshi.json"'

  - title: Verify Toolset Availability
    details: Verify v100 toolset is installed and selected before building.
    inputs: [MOD_PROJECT_PATH]
    verification:
      command: '(Get-Content -Path "{MOD_PROJECT_PATH}") | Select-String -Pattern "<PlatformToolset>v100</PlatformToolset>"'

  - title: Update Documentation
    details: Update README.md with scripts/setup_env.ps1 usage, build steps, and deploy steps. Note mod data folder name.
    inputs: [MOD_DIR_PATH]
    verification:
      command: '(Get-Content -Path "{MOD_DIR_PATH}\\README.md") | Select-String -Pattern "scripts\\setup_env.ps1"

verification_checklist:
  - Build succeeds from the new repo path after sourcing scripts/setup_env.ps1.
  - scripts/build-and-deploy.ps1 deploys the correctly named mod and assets to the Kenshi mods folder.
  - RE_Kenshi.json in the deployed mod folder points to the correct DLL name.
  - mods\{MOD_NAME}\ contains the renamed .mod file (if renamed).
  - No build output is committed to git.

rollback:
  - Since this is a copy-first approach, rollback is simply deleting the new repo folder.
```
