@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;%PATH%"
cd /d "%~dp0.."
if not exist build\resource_cache_test mkdir build\resource_cache_test
cl /nologo /EHsc /MD /O2 /DBOOST_ALL_NO_LIB /DBOOST_ERROR_CODE_HEADER_ONLY /D_ITERATOR_DEBUG_LEVEL=0 /I src /I D:\DEV\Kenshi\deps\KenshiLib_keo\Include /I D:\DEV\Kenshi\deps\boost_1_60_0 leopard\test_resource_cache_native.cpp src\resources\resource_cache_store.cpp /Fobuild\resource_cache_test\ /Febuild\resource_cache_test\native.exe /link /LIBPATH:D:\DEV\Kenshi\deps\KenshiLib_keo\Libraries\ogre OgreMain_x64.lib
if errorlevel 1 exit /b 1
set "PATH=D:\SteamLibrary\steamapps\common\Kenshi;%PATH%"
build\resource_cache_test\native.exe




