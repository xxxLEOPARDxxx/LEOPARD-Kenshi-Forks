@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;D:\SteamLibrary\steamapps\common\Kenshi;%PATH%"
cd /d "%~dp0.."
if not exist build\mesh_weights mkdir build\mesh_weights
set "OPT=/O2"
set "LINKOPT="
set "RUNARGS=%1"
if /I "%~1"=="Od" set "OPT=/Od"
if /I "%~1"=="LTCG" set "OPT=/O2 /GL"
if /I "%~1"=="LTCG" set "LINKOPT=/LTCG"
if /I "%~1"=="LTCG" set "RUNARGS=assets"
cl /nologo /EHsc /MD %OPT% /fp:precise /DBOOST_ALL_NO_LIB /DBOOST_ERROR_CODE_HEADER_ONLY /D_ITERATOR_DEBUG_LEVEL=0 /I leopard /I src /I D:\DEV\Kenshi\deps\KenshiLib_keo\Include /I D:\DEV\Kenshi\deps\boost_1_60_0 leopard\test_mesh_weights.cpp /Fobuild\mesh_weights\ /Febuild\mesh_weights\test.exe /link %LINKOPT% /LIBPATH:D:\DEV\Kenshi\deps\KenshiLib_keo\Libraries\ogre OgreMain_x64.lib
if errorlevel 1 exit /b 2
build\mesh_weights\test.exe %RUNARGS%
