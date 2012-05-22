@echo off

cd build\Release
..\..\third_party\pin-2.11-49306-msvc9-ia32_intel64-windows\pin.bat -tool_exit -tool_exit_timeout 10 -t pet.dll -- cmd.exe
cd ..\..\
