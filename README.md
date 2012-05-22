pet
===

A PIN tool to achieve DBI through javascript

Build
=====

Follow the instructions on http://code.google.com/p/v8/wiki/BuildingWithGYP on the Visual Studio section to download cygwin and python (Prerequisites).
JUST THAT PART!, do not use the gyp tool or all solution and project files will be overwritten (this should be fixed at some point by modifying the .gyp scripts).

It's assumed that you installed this repository into C:\pet\v8 (should be changed too).
And that PIN 2.11 is installed into C:\pet\pin-2.11-49306-msvc9-ia32_intel64-windows.
You need Visual Studio 2008 to compile this. It might work with 2010 (VC10) given you change the PIN Kit.

Open build/all.sln and Build from the GUI.
