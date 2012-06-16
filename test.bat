@echo off

set options=
set tooloptions=

if "%1"=="-d" set options=%options% -pause_tool 5
if "%2"=="-d" set options=%options% -pause_tool 5

if "%1"=="-v" set tooloptions=%tooloptions% -e "--log-all --print-all-code --trace-lazy --trace-ic --trace-exception --trace-bailout --trace-opt_verbose --trace --code-comments --print-source --print-builtin-source --print-ast --print-builtin-ast"
if "%2"=="-v" set tooloptions=%tooloptions% -e "--log-all --print-all-code --trace-lazy --trace-ic --trace-exception --trace-bailout --trace-opt_verbose --trace --code-comments --print-source --print-builtin-source --print-ast --print-builtin-ast"

..\..\third_party\pin-2.11-49306-msvc9-ia32_intel64-windows\pin.bat %options% -t pet.dll %tooloptions% -- cmd.exe

