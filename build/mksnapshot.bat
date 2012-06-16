@echo off
cd %~dp0\Release
call ..\..\third_party\pin-2.11-49306-msvc9-ia32_intel64-windows\pin.bat -pause_tool 5 -t mksnapshot.dll -l "%1" -o "%2" -- cmd.exe
cd %~dp0
