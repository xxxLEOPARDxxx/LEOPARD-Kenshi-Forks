@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
cd /d "%~dp0.."
if not exist build\menu-startup mkdir build\menu-startup
for %%O in (Od O2) do (
 "%VC%\bin\amd64\cl.exe" /nologo /EHsc /MD /%%O /I src leopard\test_resource_menu_startup.cpp /Fobuild\menu-startup\menu-%%O.obj /Febuild\menu-startup\menu-%%O.exe
 if errorlevel 1 exit /b 1
 build\menu-startup\menu-%%O.exe
 if errorlevel 1 exit /b 1
)
