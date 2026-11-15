@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;%PATH%"
cd /d "%~dp0.."
if not exist build\retention-victim-index mkdir build\retention-victim-index
set "OPT=/O2"
if /I "%~1"=="Od" set "OPT=/Od"
cl /nologo /EHsc /MD %OPT% /D_ITERATOR_DEBUG_LEVEL=0 /I src leopard\test_retention_victim_index.cpp /Fobuild\retention-victim-index\ /Febuild\retention-victim-index\index.exe
if errorlevel 1 exit /b 1
build\retention-victim-index\index.exe
