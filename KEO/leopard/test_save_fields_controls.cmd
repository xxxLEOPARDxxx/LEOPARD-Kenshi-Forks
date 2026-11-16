@echo off
set "VC=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"
set "INCLUDE=%VC%\include;%SDK%\Include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"
set "PATH=%VC%\bin\amd64;%SDK%\Bin;D:\SteamLibrary\steamapps\common\Kenshi;%PATH%"
set "PYTHONUTF8=1"
cd /d "%~dp0.."
"C:\Users\User\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe" leopard\test_save_fields_controls.py
