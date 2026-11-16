@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;%PATH%"
cd /d "%~dp0.."
if not exist build\save_instance_test mkdir build\save_instance_test
ml64 /nologo /c /Fobuild\save_instance_test\frame.obj leopard\test_save_instance_stage.asm
if errorlevel 1 exit /b 2
set "BASE="
if /I "%~1"=="baseline" set "BASE=/DKEO_INSTANCE_BASELINE"
set "OPT=/O2"
if /I "%~1"=="Od" set "OPT=/Od"
cl /nologo /EHsc /MD %OPT% %BASE% /D_ITERATOR_DEBUG_LEVEL=0 /I src leopard\test_save_instance_lookup.cpp src\fixes\near_page.cpp src\fixes\code_patch_threads.cpp src\fixes\code_patch_batch.cpp build\save_instance_test\frame.obj /Fobuild\save_instance_test\ /Febuild\save_instance_test\instance.exe
if errorlevel 1 exit /b 2
build\save_instance_test\instance.exe
