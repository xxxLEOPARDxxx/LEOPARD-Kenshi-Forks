@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;D:\SteamLibrary\steamapps\common\Kenshi;%PATH%"
cd /d "%~dp0.."
if not exist build\save_state_list_test mkdir build\save_state_list_test
ml64 /nologo /c /Fobuild\save_state_list_test\frame.obj leopard\test_save_state_stage.asm
if errorlevel 1 exit /b 2
set "OPT=/O2"
if /I "%~1"=="Od" set "OPT=/Od"
set "BASE="
if /I "%~1"=="baseline" set "BASE=/DKEO_STATE_LIST_BASELINE"
cl /nologo /EHsc /MD %OPT% %BASE% /D_ITERATOR_DEBUG_LEVEL=0 /I src leopard\test_save_state_list.cpp src\fixes\near_page.cpp src\fixes\code_patch_threads.cpp build\save_state_list_test\frame.obj /Fobuild\save_state_list_test\ /Febuild\save_state_list_test\save_state.exe
if errorlevel 1 exit /b 2
build\save_state_list_test\save_state.exe
