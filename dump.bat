@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem ----------------------------------------------------------------
rem dump.bat - produce opcode dumps for PHP 7.2 ionCube files.
rem Logs are always written relative to this batch file, not relative
rem to the caller's current directory.
rem ----------------------------------------------------------------

set "SCRIPT_DIR=%~dp0"
set "PHP=%SCRIPT_DIR%runtime\php.exe"
set "PHP_INI=%SCRIPT_DIR%runtime\php.ini"
set "DUMPER=%SCRIPT_DIR%opcodedump.php"
set "LOG_DIR=%SCRIPT_DIR%logs"

rem Always create the log directory before any validation can exit.
if not exist "%LOG_DIR%" md "%LOG_DIR%" >nul 2>nul
if not exist "%LOG_DIR%" (
    set "LOG_DIR=%TEMP%\ic72dump-opcodes-logs"
    if not exist "%LOG_DIR%" md "%LOG_DIR%" >nul 2>nul
)

set "LOG=%LOG_DIR%\dump_%DATE:/=-%_%TIME::=-%_%RANDOM%.log"
set "LOG=%LOG: =_%"

> "%LOG%" echo ==== ic72dump-opcodes dump.bat ====
>> "%LOG%" echo Date: %DATE% %TIME%
>> "%LOG%" echo Script dir: %SCRIPT_DIR%
>> "%LOG%" echo Current dir: %CD%
>> "%LOG%" echo PHP: %PHP%
>> "%LOG%" echo PHP_INI: %PHP_INI%
>> "%LOG%" echo DUMPER: %DUMPER%
>> "%LOG%" echo Args: %*
>> "%LOG%" echo.

echo Log file: "%LOG%"

if not exist "%PHP%" (
    echo ERROR: runtime\php.exe not found. Keep dump.bat inside the ic72dump folder.
    >> "%LOG%" echo ERROR: runtime\php.exe not found.
    type "%LOG%"
    endlocal & exit /b 1
)
if not exist "%PHP_INI%" (
    echo ERROR: runtime\php.ini not found.
    >> "%LOG%" echo ERROR: runtime\php.ini not found.
    type "%LOG%"
    endlocal & exit /b 1
)
if not exist "%DUMPER%" (
    echo ERROR: opcodedump.php not found.
    >> "%LOG%" echo ERROR: opcodedump.php not found.
    type "%LOG%"
    endlocal & exit /b 1
)
if "%~1"=="" (
    echo Usage: dump.bat path\to\encoded.php [more files ...]
    >> "%LOG%" echo ERROR: no input file supplied.
    type "%LOG%"
    endlocal & exit /b 1
)

rem Safe defaults for protected/runtime-captured ionCube op arrays.
if "%OPCODEDUMP_USE_RUNTIME_CAPTURE%"=="" set "OPCODEDUMP_USE_RUNTIME_CAPTURE=1"
if "%OPCODEDUMP_IC_OPERAND_MATERIALIZE%"=="" set "OPCODEDUMP_IC_OPERAND_MATERIALIZE=1"
if "%OPCODEDUMP_IC_OPERAND_MATERIALIZE_INLINE%"=="" set "OPCODEDUMP_IC_OPERAND_MATERIALIZE_INLINE=1"

rem Static jump materialization can hard-crash on large protected files.
if "%OPCODEDUMP_STATIC_JUMP_MATERIALIZE%"=="" set "OPCODEDUMP_STATIC_JUMP_MATERIALIZE=0"
if "%OPCODEDUMP_JUMP_MATERIALIZE_METHODS%"=="" set "OPCODEDUMP_JUMP_MATERIALIZE_METHODS=*"

>> "%LOG%" echo OPCODEDUMP_USE_RUNTIME_CAPTURE=%OPCODEDUMP_USE_RUNTIME_CAPTURE%
>> "%LOG%" echo OPCODEDUMP_IC_OPERAND_MATERIALIZE=%OPCODEDUMP_IC_OPERAND_MATERIALIZE%
>> "%LOG%" echo OPCODEDUMP_IC_OPERAND_MATERIALIZE_INLINE=%OPCODEDUMP_IC_OPERAND_MATERIALIZE_INLINE%
>> "%LOG%" echo OPCODEDUMP_STATIC_JUMP_MATERIALIZE=%OPCODEDUMP_STATIC_JUMP_MATERIALIZE%
>> "%LOG%" echo OPCODEDUMP_JUMP_MATERIALIZE_METHODS=%OPCODEDUMP_JUMP_MATERIALIZE_METHODS%
>> "%LOG%" echo.
>> "%LOG%" echo PHP overrides: memory_limit=-1 max_execution_time=0
>> "%LOG%" echo.
>> "%LOG%" echo Running PHP...

"%PHP%" -c "%PHP_INI%" -d memory_limit=-1 -d max_execution_time=0 -d display_errors=1 -d error_reporting=-1 "%DUMPER%" %* >> "%LOG%" 2>&1
set "RC=%ERRORLEVEL%"

>> "%LOG%" echo.
>> "%LOG%" echo Exit code: %RC%

type "%LOG%"

if not "%RC%"=="0" (
    echo.
    echo ERROR: dump failed with exit code %RC%.
    echo Log file: "%LOG%"
)

endlocal & exit /b %RC%
