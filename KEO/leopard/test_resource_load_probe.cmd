@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;%PATH%"
cd /d "%~dp0.."
if not exist build\resource_load_probe_test mkdir build\resource_load_probe_test
cl /nologo /EHsc /MT /Od /I src leopard\test_resource_load_probe.cpp /Fobuild\resource_load_probe_test\test_od.obj /Febuild\resource_load_probe_test\test_od.exe
if errorlevel 1 exit /b 1
build\resource_load_probe_test\test_od.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /MT /O2 /I src leopard\test_resource_load_probe.cpp /Fobuild\resource_load_probe_test\test_o2.obj /Febuild\resource_load_probe_test\test_o2.exe
if errorlevel 1 exit /b 1
build\resource_load_probe_test\test_o2.exe
if errorlevel 1 exit /b 1
cl /nologo /c /EHsc /MD /O2 /DKEO_DEBUG /DBOOST_ALL_NO_LIB /DBOOST_ERROR_CODE_HEADER_ONLY /D_ITERATOR_DEBUG_LEVEL=0 /I src /I D:\DEV\Kenshi\deps\KenshiLib_keo\Include /I D:\DEV\Kenshi\deps\KenshiLib_keo\Include\ogre /I D:\DEV\Kenshi\deps\boost_1_60_0 src\resources\resource_load_probe.cpp /Fobuild\resource_load_probe_test\native_dev.obj
if errorlevel 1 exit /b 1
cl /nologo /c /EHsc /MD /O2 /DBOOST_ALL_NO_LIB /DBOOST_ERROR_CODE_HEADER_ONLY /D_ITERATOR_DEBUG_LEVEL=0 /I src /I D:\DEV\Kenshi\deps\KenshiLib_keo\Include /I D:\DEV\Kenshi\deps\KenshiLib_keo\Include\ogre /I D:\DEV\Kenshi\deps\boost_1_60_0 src\resources\resource_load_probe.cpp /Fobuild\resource_load_probe_test\native_prod.obj
exit /b %errorlevel%
