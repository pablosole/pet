pet
===

A PIN tool to achieve DBI through javascript

Build
=====

Follow the instructions on http://code.google.com/p/v8/wiki/BuildingWithGYP on the Visual Studio section to download cygwin and python (Prerequisites).
JUST THAT PART!, do not use the gyp tool or all solution and project files will be overwritten (this should be fixed at some point by modifying the .gyp scripts).

Then download latest Pin (2.11) for Visual Studio 2008 (it might work with other too given you use the correct Pin Kit) and uncompress it inside third_party/.

Open build/all.sln and Build from the GUI.

The third_party directory must have 3 subdirectories then: cygwin, python-26 and pin-2.11-49306-msvc9-ia32_intel64-windows.
