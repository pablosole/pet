@echo off

set petbinary=%~dp0\pinocchio.dll
set script=%~dp0\main.js
set logfile=%~dp0\pinocchio.log
set options=
set tooloptions=
set binary=ping.exe
set jsoptions=
set binaryargs=
set pid=

:initial
if exist "%~$PATH:1" set binary=%~$PATH:1
if exist "%~$PATH:1" shift
if "%1"=="-p" set pid=%2
if "%1"=="-p" shift /2
if "%1"=="-j" set script=%~$PATH:2
if "%1"=="-j" shift /2
if "%1"=="-l" set logfile=%~$PATH:2
if "%1"=="-l" shift /2
if "%1"=="-f" set options=%options% -follow_execv
if "%1"=="-d" set options=%options% -debug_instrumented_processes
if "%1"=="-v" set jsoptions=%jsoptions% --log-all --print-all-code --trace-lazy --trace-ic --trace-exception --trace-deopt --trace-bailout --trace-opt_verbose --trace --code-comments --print-source --print-builtin-source --print-ast --print-builtin-ast
if "%1"=="-r" set jsoptions=%jsoptions% --prof
if "%1"=="-s" set tooloptions=%tooloptions% -separate_memory 1  
if "%1"=="--"  goto args
if "%1"=="" goto done
shift
goto initial

:args
shift
set binaryargs=%1 %2 %3 %4 %5 %6 %7 %8 %9

:done
if not "%jsoptions%"=="" set tooloptions=%tooloptions% -e "%jsoptions%"
if defined pid set binary=
if defined binary set pid=
if defined pid set options=%options% -pid %pid%
if defined binary set binary= -- "%binary%" %binaryargs%
..\..\third_party\pin-2.12-53271-msvc9-ia32_intel64-windows\pin_bat.bat %options% -inline -log_inline -t %petbinary% %tooloptions% -f %script% -d %logfile% %binary%
