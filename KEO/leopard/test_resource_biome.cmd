@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;%PATH%"
cd /d "%~dp0.."
if not exist build\resource_biome_test mkdir build\resource_biome_test
cl /nologo /EHsc /MD /O2 /DRESOURCE_BIOME_PURE_TEST /I src leopard\test_resource_biome.cpp src\resources\resource_biome_manifest.cpp /Fobuild\resource_biome_test\ /Febuild\resource_biome_test\test.exe
if errorlevel 1 exit /b 1
build\resource_biome_test\test.exe
