@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;D:\SteamLibrary\steamapps\common\Kenshi;%PATH%"
cd /d "%~dp0.."
if not exist build\animation_patch_test mkdir build\animation_patch_test
set "OPT=/O2"
if /I "%~1"=="Od" set "OPT=/Od"
cl /nologo /EHsc /MD %OPT% /D_ITERATOR_DEBUG_LEVEL=0 /I src leopard\equipment_animation_patch_test.cpp src\fixes\world\equipment_animation_lookup.cpp src\fixes\near_page.cpp src\fixes\code_patch_threads.cpp src\fixes\code_patch_batch.cpp /Fobuild\animation_patch_test\ /Febuild\animation_patch_test\test.exe
if errorlevel 1 exit /b 2
build\animation_patch_test\test.exe
