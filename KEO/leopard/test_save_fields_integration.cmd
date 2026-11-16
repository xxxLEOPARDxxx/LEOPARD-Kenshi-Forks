@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;%PATH%"
cd /d "%~dp0.."
if not exist build\save_fields_integration mkdir build\save_fields_integration
set "PYTHONUTF8=1"
"C:\Users\User\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe" leopard\test_save_fields_contract.py
if errorlevel 1 exit /b 2
set "OPT=/O2"
if /I "%~1"=="Od" set "OPT=/Od"
cl /nologo /EHsc /MD %OPT% /D_ITERATOR_DEBUG_LEVEL=0 /I src leopard\test_save_fields_integration.cpp src\fixes\world\save_field_key_leaf.cpp src\fixes\world\save_field_value_transfer.cpp src\fixes\near_page.cpp src\fixes\code_patch_threads.cpp src\fixes\code_patch_batch.cpp /Fobuild\save_fields_integration\ /Febuild\save_fields_integration\test.exe
if errorlevel 1 exit /b 2
build\save_fields_integration\test.exe
