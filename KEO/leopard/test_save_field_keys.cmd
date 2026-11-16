@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;D:\SteamLibrary\steamapps\common\Kenshi;%PATH%"
cd /d "%~dp0.."
if not exist build\save_field_keys mkdir build\save_field_keys
set "BASE="
set "SOURCE=src\fixes\world\save_field_key_leaf.cpp"
if /I "%~1"=="baseline" set "BASE=/DKEO_FIELD_KEY_BASELINE"
if /I "%~1"=="baseline" set "SOURCE="
set "OPT=/O2"
if /I "%~1"=="Od" set "OPT=/Od"
cl /nologo /EHsc /MD %OPT% %BASE% /D_ITERATOR_DEBUG_LEVEL=0 /I src leopard\test_save_field_keys.cpp %SOURCE% src\fixes\near_page.cpp /Fobuild\save_field_keys\ /Febuild\save_field_keys\test.exe
if errorlevel 1 exit /b 2
build\save_field_keys\test.exe
