@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;D:\SteamLibrary\steamapps\common\Kenshi;%PATH%"
cd /d "%~dp0.."
if not exist build\pose_cpu mkdir build\pose_cpu
set "OPT=/O2"
set "SOURCE="
set "BASE="
if /I "%~1"=="baseline" set "BASE=/DKEO_POSE_CPU_BASELINE"
if /I "%~1"=="baseline" set "SOURCE="
if /I "%~1"=="Od" set "OPT=/Od"
cl /nologo /EHsc /MD %OPT% %BASE% /DBOOST_ALL_NO_LIB /DBOOST_ERROR_CODE_HEADER_ONLY /D_ITERATOR_DEBUG_LEVEL=0 /I leopard\pose_cpu_spike /I src /I D:\DEV\Kenshi\deps\KenshiLib_keo\Include /I D:\DEV\Kenshi\deps\boost_1_60_0 leopard\test_pose_cpu.cpp %SOURCE% /Fobuild\pose_cpu\ /Febuild\pose_cpu\test.exe /link /LIBPATH:D:\DEV\Kenshi\deps\KenshiLib_keo\Libraries\ogre OgreMain_x64.lib advapi32.lib
if errorlevel 1 exit /b 2
if /I "%~1"=="assets" (build\pose_cpu\test.exe assets) else if /I "%~1"=="bench" (build\pose_cpu\test.exe bench) else (build\pose_cpu\test.exe)
