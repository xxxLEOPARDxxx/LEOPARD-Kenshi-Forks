@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;%PATH%"
cd /d "%~dp0.."
if not exist build\save_string_test mkdir build\save_string_test
set "OPT=/O2"
if /I "%~1"=="Od" set "OPT=/Od"
set "BASE="
if /I "%~1"=="baseline" set "BASE=/DKEO_SAVE_STRING_BASELINE"
cl /nologo /EHsc /MD %OPT% %BASE% /D_ITERATOR_DEBUG_LEVEL=0 /I src leopard\test_save_string_transfer.cpp /Fobuild\save_string_test\ /Febuild\save_string_test\save_string.exe
if errorlevel 1 exit /b 2
build\save_string_test\save_string.exe
