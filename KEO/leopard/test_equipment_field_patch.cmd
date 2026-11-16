@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include;D:\DEV\Kenshi\deps\KenshiLib_keo\Include;D:\DEV\Kenshi\deps\boost_1_60_0"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;%PATH%"
cd /d "%~dp0.."
if not exist build\equipment_patch_test mkdir build\equipment_patch_test
set "BASE="
if /I "%~1"=="baseline" set "BASE=/DKEO_EQUIPMENT_PATCH_BASELINE"
set "OPT=/O2"
if /I "%~1"=="Od" set "OPT=/Od"
cl /nologo /EHsc /MD %OPT% %BASE% /DOGRE_STATIC_LIB /D_ITERATOR_DEBUG_LEVEL=0 /I src leopard\test_equipment_field_patch.cpp src\fixes\world\equipment_field_lookup.cpp src\fixes\near_page.cpp src\fixes\code_patch_threads.cpp src\fixes\code_patch_batch.cpp /Fobuild\equipment_patch_test\ /Febuild\equipment_patch_test\equipment.exe
if errorlevel 1 exit /b 2
build\equipment_patch_test\equipment.exe
