@echo off
REM LEOPARD: local build of the KEO fork on the D:\DEV\Kenshi stand.
REM The stand has no vcvarsall amd64, so the VC10 + SDK 7.1 environment is set
REM by hand (as D:\DEV\Kenshi\ours\build.ps1 does), then build_opt_step4.bat
REM runs without its vcvarsall call (build_step4_noenv.bat). Output:
REM build\KEO_step4_prod\KEO.dll.
setlocal
set "VCDIR=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDKDIR=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VCDIR%\include;%SDKDIR%\Include"
set "LIB=%VCDIR%\lib\amd64;%SDKDIR%\Lib\x64"
set "PATH=%VCDIR%\bin\amd64;%SDKDIR%\Bin;%PATH%"
if "%KENSHILIB%"=="" set "KENSHILIB=D:\DEV\Kenshi\deps\KenshiLib_keo"
if "%BOOST_ROOT%"=="" set "BOOST_ROOT=D:\DEV\Kenshi\deps\boost_1_60_0"
call "%~dp0build_step4_noenv.bat"
exit /b %errorlevel%
