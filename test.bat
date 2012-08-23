@echo off

set options=
set tooloptions=
set binary=ping.exe
set jsoptions=

:initial
if "%1"=="-d" set options=%options% -debug_instrumented_processes
if "%1"=="-v" set jsoptions=%jsoptions% --log-all --print-all-code --trace-lazy --trace-ic --trace-exception --trace-deopt --trace-bailout --trace-opt_verbose --trace --code-comments --print-source --print-builtin-source --print-ast --print-builtin-ast
if "%1"=="-p" set jsoptions=%jsoptions% --prof
if "%1"=="-s" set tooloptions=%tooloptions% -separate_memory 1  
if exist %~$PATH:1 set binary=%~$PATH:1
if "%1"=="" goto done
shift
goto initial

if "%jsoptions%"!="" set tooloptions=%tooloptions% -e "%jsoptions%"

:done
..\..\third_party\pin-2.12-53271-msvc9-ia32_intel64-windows\pin_bat.bat %options% -inline -log_inline -t pet.dll %tooloptions% -- %binary%
