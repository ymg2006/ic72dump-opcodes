@echo off
setlocal EnableExtensions
rem ----------------------------------------------------------------
rem build.bat - recompile php_opcodedump.dll (PHP 7.2.34 NTS x86 VC15)
rem Safe for AppVeyor cmd.exe parsing. Avoids ProgramFiles(x86) in IF lines.
rem ----------------------------------------------------------------
set "SCRIPT_DIR=%~dp0"
set "SRC=%SCRIPT_DIR%..\src"
set "OUT=%SCRIPT_DIR%out"
set "DEVEL=%SCRIPT_DIR%devel\php-7.2.34-devel-VC15-x86"
set "PHP_INC=%DEVEL%\include"

if exist "%PHP_INC%\main\php.h" goto have_php_headers
echo ERROR: PHP 7.2 devel headers not found at %PHP_INC%\main\php.h
echo        Extract php-devel-pack-7.2.34-nts-Win32-VC15-x86.zip into build\devel\
exit /b 1

:have_php_headers
if exist "%OUT%" goto have_out
mkdir "%OUT%"
if errorlevel 1 exit /b 1
:have_out

set "VCVARS="

rem AppVeyor Visual Studio 2017 usually has one of these non-parenthesized paths.
if defined VCVARS goto found_vcvars
if exist "C:\BuildTools\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=C:\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
if defined VCVARS goto found_vcvars
if exist "C:\Program Files\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
if defined VCVARS goto found_vcvars
if exist "C:\Program Files\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars32.bat"
if defined VCVARS goto found_vcvars
if exist "C:\Program Files\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvars32.bat"
if defined VCVARS goto found_vcvars
if exist "C:\Program Files\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
if defined VCVARS goto found_vcvars

rem Fallback to 8.3 Program Files (x86) path. This avoids cmd parser issues with parentheses.
if exist "C:\Progra~2\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=C:\Progra~2\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
if defined VCVARS goto found_vcvars
if exist "C:\Progra~2\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=C:\Progra~2\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars32.bat"
if defined VCVARS goto found_vcvars
if exist "C:\Progra~2\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=C:\Progra~2\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvars32.bat"
if defined VCVARS goto found_vcvars
if exist "C:\Progra~2\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvars32.bat" set "VCVARS=C:\Progra~2\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
if defined VCVARS goto found_vcvars

where cl.exe > nul 2>&1
if not errorlevel 1 goto compiler_already_ready

echo ERROR: vcvars32.bat not found. Install Visual Studio Build Tools 2017 or use AppVeyor image Visual Studio 2017.
exit /b 1

:found_vcvars
echo Using VC environment: %VCVARS%
call "%VCVARS%" > nul 2>&1
if not errorlevel 1 goto check_tools
echo ERROR: Failed to initialize VC15 x86 compiler environment: %VCVARS%
exit /b 1

:compiler_already_ready
echo Using existing compiler environment.

:check_tools
where cl.exe > nul 2>&1
if not errorlevel 1 goto have_cl
echo ERROR: cl.exe not found after vcvars32.bat.
exit /b 1
:have_cl

where link.exe > nul 2>&1
if not errorlevel 1 goto have_link
echo ERROR: link.exe not found after vcvars32.bat.
exit /b 1
:have_link

echo [1/2] Compiling opcodedump.c ...
cl.exe /nologo /c /MD /O2 /W3 /wd4996 ^
    /DWIN32 /D_WINDOWS /DZEND_WIN32=1 /DPHP_WIN32=1 ^
    /DCOMPILE_DL_OPCODEDUMP /DZEND_DEBUG=0 ^
    /D_USE_32BIT_TIME_T=1 ^
    /I"%PHP_INC%" /I"%PHP_INC%\main" /I"%PHP_INC%\Zend" ^
    /I"%PHP_INC%\TSRM" /I"%PHP_INC%\win32" /I"%PHP_INC%\ext" ^
    /Fo"%OUT%\opcodedump.obj" ^
    "%SRC%\opcodedump.c"
if not errorlevel 1 goto compile_ok
echo ERROR: Compilation failed.
exit /b 1
:compile_ok

echo [2/2] Linking php_opcodedump.dll ...
link.exe /nologo /DLL ^
    /OUT:"%OUT%\php_opcodedump.dll" ^
    /LIBPATH:"%DEVEL%\lib" ^
    "%OUT%\opcodedump.obj" ^
    php7.lib
if not errorlevel 1 goto link_ok
echo ERROR: Linking failed.
exit /b 1
:link_ok

echo.
echo SUCCESS: %OUT%\php_opcodedump.dll
endlocal
