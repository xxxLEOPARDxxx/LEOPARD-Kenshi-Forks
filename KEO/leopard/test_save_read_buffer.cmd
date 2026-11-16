@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;%PATH%"
cd /d "%~dp0.."
if not exist build\save_read_buffer_test mkdir build\save_read_buffer_test
set "OPT=/O2"
if /I "%~1"=="Od" set "OPT=/Od"
cl /nologo /EHsc /MD %OPT% /D_ITERATOR_DEBUG_LEVEL=0 /I src leopard\test_save_read_buffer.cpp /Fobuild\save_read_buffer_test\ /Febuild\save_read_buffer_test\save_read_buffer.exe
if errorlevel 1 exit /b 2
build\save_read_buffer_test\save_read_buffer.exe
