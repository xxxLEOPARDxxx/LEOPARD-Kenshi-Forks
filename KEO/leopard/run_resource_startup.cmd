@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;%PATH%"
cd /d "%~dp0.."
if not exist build\resource-startup mkdir build\resource-startup
cl /nologo /EHsc /MD /O2 /I src leopard\test_resource_startup.cpp /Fobuild\resource-startup\ /Febuild\resource-startup\test.exe /link /MAP:build\resource-startup\test.map > build\resource-startup\compile.log 2>&1
if errorlevel 1 (type build\resource-startup\compile.log & exit /b 1)
build\resource-startup\test.exe > build\resource-startup\test.log 2>&1
set "RESULT=%errorlevel%"
type build\resource-startup\test.log
exit /b %RESULT%
