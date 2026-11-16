@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;%PATH%"
cd /d "%~dp0.."
if not exist build\patch_thread_test mkdir build\patch_thread_test
if /I "%~1"=="baseline" (
 cl /nologo /EHsc /O2 /I src /DKEO_THREAD_EXCLUSION_BASELINE leopard\test_code_patch_threads.cpp /Fobuild\patch_thread_test\ /Febuild\patch_thread_test\threads.exe
) else (
 cl /nologo /EHsc /O2 /I src leopard\test_code_patch_threads.cpp /Fobuild\patch_thread_test\ /Febuild\patch_thread_test\threads.exe
)
if errorlevel 1 exit /b 2
build\patch_thread_test\threads.exe
