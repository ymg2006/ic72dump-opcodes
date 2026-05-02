@echo off
setlocal
:: ----------------------------------------------------------------
:: dump.bat  ?  produce an opcodes dump for one or more PHP files
::              that are ionCube-encoded for PHP 7.2.
::
:: Usage:
::   dump.bat  path\to\encoded.php
::   dump.bat  file1.php  file2.php  ...
::
:: Output:
::   One  <filename>.opcodes.txt  per input file (same directory
::   as the input file).  A short summary is printed to stdout.
:: ----------------------------------------------------------------
set SCRIPT_DIR=%~dp0
set PHP=%SCRIPT_DIR%runtime\php.exe
set PHP_INI=%SCRIPT_DIR%runtime\php.ini
set DUMPER=%SCRIPT_DIR%opcodedump.php

if not exist "%PHP%" (
    echo ERROR: runtime\php.exe not found. Keep dump.bat inside the ic72dump folder.
    exit /b 1
)
if "%~1"=="" (
    echo Usage: dump.bat path\to\encoded.php [more files ...]
    exit /b 1
)

"%PHP%" -c "%PHP_INI%" "%DUMPER%" %*
endlocal & exit /b %ERRORLEVEL%
