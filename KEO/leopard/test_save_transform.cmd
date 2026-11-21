@echo off
setlocal
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin\x64;%PATH%"
cd /d "%~dp0.."
if not exist build\save-transform mkdir build\save-transform
set "OPT=O2"
if /I "%~1"=="Od" set "OPT=Od"
ml64 /nologo /c /Fobuild\save-transform\stage.obj leopard\test_save_transform_stage.asm
if errorlevel 1 exit /b 2
cl /nologo /EHsc /MD /%OPT% /D_ITERATOR_DEBUG_LEVEL=0 /Isrc leopard\test_save_transform.cpp src\fixes\near_page.cpp src\fixes\code_patch_threads.cpp src\fixes\code_patch_batch.cpp build\save-transform\stage.obj /Fobuild\save-transform\ /Febuild\save-transform\actual-%OPT%.exe
exit /b %errorlevel%
