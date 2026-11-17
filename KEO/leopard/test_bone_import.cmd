@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;D:\SteamLibrary\steamapps\common\Kenshi;%PATH%"
cd /d "%~dp0.."
if not exist build\bone_import mkdir build\bone_import
set "OPT=/O2"
set "LINKOPT="
if "%BONE_IMPORT_SOURCE%"=="" set "BONE_IMPORT_SOURCE=src\fixes\world\bone_import.cpp"
if /I "%~1"=="Od" set "OPT=/Od"
if /I "%~1"=="LTCG" set "OPT=/O2 /GL"
if /I "%~1"=="LTCG" set "LINKOPT=/LTCG"
cl /nologo /EHsc /MD %OPT% /DBOOST_ALL_NO_LIB /DBOOST_ERROR_CODE_HEADER_ONLY /D_ITERATOR_DEBUG_LEVEL=0 /I src /I D:\DEV\Kenshi\deps\KenshiLib_keo\Include /I D:\DEV\Kenshi\deps\boost_1_60_0 leopard\test_bone_import.cpp "%BONE_IMPORT_SOURCE%" src\fixes\near_page.cpp src\fixes\code_patch_threads.cpp src\fixes\code_patch_batch.cpp /Fobuild\bone_import\ /Febuild\bone_import\test.exe /link %LINKOPT% /LIBPATH:D:\DEV\Kenshi\deps\KenshiLib_keo\Libraries\ogre OgreMain_x64.lib
if errorlevel 1 exit /b 2
build\bone_import\test.exe %1
