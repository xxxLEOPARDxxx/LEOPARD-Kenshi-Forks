@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;D:\SteamLibrary\steamapps\common\Kenshi;%PATH%"
cd /d "%~dp0.."
if not exist build\resource_gpu_bridge_test mkdir build\resource_gpu_bridge_test
cl /nologo /EHsc /MD /O2 /DBOOST_ALL_NO_LIB /DBOOST_ERROR_CODE_HEADER_ONLY /I src /I D:\DEV\Kenshi\deps\KenshiLib_keo\Include /I D:\DEV\Kenshi\deps\boost_1_60_0 leopard\test_resource_gpu_bridge.cpp src\resources\resource_dds.cpp /Fobuild\resource_gpu_bridge_test\ /Febuild\resource_gpu_bridge_test\bridge.exe /link d3d11.lib dxgi.lib /LIBPATH:D:\DEV\Kenshi\deps\KenshiLib_keo\Libraries\ogre OgreMain_x64.lib
if errorlevel 1 exit /b 1
build\resource_gpu_bridge_test\bridge.exe
