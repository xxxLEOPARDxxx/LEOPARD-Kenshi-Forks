$ErrorActionPreference = 'Stop'
& 'D:/DEV/Kenshi/build.ps1' -SrcDir $PSScriptRoot -Name StackSortCheck `
    -SubDirs src/common,src/packer,src/plugin,src/ui,src/worker `
    -Include src/common,src/packer,src/plugin,src/ui,src/worker `
    -Define STACKSORT_ENABLE_ROTATION
if ($LASTEXITCODE) { exit $LASTEXITCODE }
