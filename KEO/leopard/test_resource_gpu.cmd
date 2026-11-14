@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;%PATH%"
cd /d "%~dp0.."
if not exist build\resource_gpu_test mkdir build\resource_gpu_test
set "OPT=/O2"
if /I "%~1"=="Od" set "OPT=/Od"
cl /nologo /EHsc /MD %OPT% /DRESOURCE_GPU_INCLUDE_STORE /I src leopard\test_resource_gpu.cpp src\resources\resource_dds.cpp /Fobuild\resource_gpu_test\ /Febuild\resource_gpu_test\gpu.exe /link d3d11.lib dxgi.lib
if errorlevel 1 exit /b 1
build\resource_gpu_test\gpu.exe
