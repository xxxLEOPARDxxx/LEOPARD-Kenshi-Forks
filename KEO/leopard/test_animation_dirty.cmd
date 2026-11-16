@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;D:\SteamLibrary\steamapps\common\Kenshi;%PATH%"
cd /d "%~dp0.."
if not exist build\animation_dirty mkdir build\animation_dirty
python leopard\test_animation_dirty_contract.py
if errorlevel 1 exit /b 2
set "SOURCE=src\fixes\world\animation_dirty_reuse.cpp"
set "BASE="
if /I "%~1"=="baseline" set "BASE=/DKEO_DIRTY_BASELINE"
if /I "%~1"=="baseline" set "SOURCE="
set "OPT=/O2"
if /I "%~1"=="Od" set "OPT=/Od"
cl /nologo /EHsc /MD %OPT% /GL %BASE% /I src leopard\test_animation_dirty.cpp %SOURCE% src\fixes\near_page.cpp src\fixes\code_patch_threads.cpp src\fixes\code_patch_batch.cpp /Fobuild\animation_dirty\ /Febuild\animation_dirty\test.exe /link /LTCG
if errorlevel 1 exit /b 2
build\animation_dirty\test.exe
