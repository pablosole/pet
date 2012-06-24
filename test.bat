@echo off

set options=
set tooloptions=
set binary=ping.exe

:initial
if "%1"=="-d" set options=%options% -debug_instrumented_processes
if "%1"=="-v" set tooloptions=%tooloptions% -e "--log-all --print-all-code --trace-lazy --trace-ic --trace-exception --trace-bailout --trace-opt_verbose --trace --code-comments --print-source --print-builtin-source --print-ast --print-builtin-ast"
if "%1"=="-s" set tooloptions=%tooloptions% -separate_memory 1  
if exist %~$PATH:1 set binary=%~$PATH:1
if "%1"=="" goto done
shift
goto initial

:done
..\..\third_party\pin-2.11-49306-msvc9-ia32_intel64-windows\pin.bat %options% -inline -log_inline -t pet.dll %tooloptions% -- %binary%


