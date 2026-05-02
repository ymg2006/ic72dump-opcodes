@echo off
setlocal
:: ----------------------------------------------------------------
:: build.bat  ?  recompile php_opcodedump.dll  (PHP 7.2 NTS x86 VC15)
::
:: Requirements:
::   1. Visual Studio Build Tools 2017 (VC15) with x86 native tools
::      ? provides cl.exe, link.exe, vcvars32.bat
::   2. PHP 7.2.34 NTS x86 devel pack extracted to:
::      build\devel\php-7.2.34-devel-VC15-x86\
::      Download: https://windows.php.net/downloads/releases/archives/
::        php-devel-pack-7.2.34-nts-Win32-VC15-x86.zip
:: ----------------------------------------------------------------
set SCRIPT_DIR=%~dp0
set SRC=%SCRIPT_DIR%..\src
set OUT=%SCRIPT_DIR%out
set DEVEL=%SCRIPT_DIR%devel\php-7.2.34-devel-VC15-x86
set PHP_INC=%DEVEL%\include

if not exist "%PHP_INC%\main\php.h" (
    echo ERROR: PHP 7.2 devel headers not found at %PHP_INC%\main\php.h
    echo        Extract php-devel-pack-7.2.34-nts-Win32-VC15-x86.zip into build\devel\
    exit /b 1
)

if not exist "%OUT%" mkdir "%OUT%"

call "C:\BuildTools\VC\Auxiliary\Build\vcvars32.bat" > nul 2>&1
if errorlevel 1 (
    echo ERROR: vcvars32.bat not found. Install Visual Studio Build Tools 2017.
    exit /b 1
)

echo [1/2] Compiling opcodedump.c ...
cl.exe /nologo /c /MD /O2 /W3 /wd4996 ^
    /DWIN32 /D_WINDOWS /DZEND_WIN32=1 /DPHP_WIN32=1 ^
    /DCOMPILE_DL_OPCODEDUMP /DZEND_DEBUG=0 ^
    /D_USE_32BIT_TIME_T=1 ^
    /I"%PHP_INC%" /I"%PHP_INC%\main" /I"%PHP_INC%\Zend" ^
    /I"%PHP_INC%\TSRM" /I"%PHP_INC%\win32" /I"%PHP_INC%\ext" ^
    /Fo"%OUT%\opcodedump.obj" ^
    "%SRC%\opcodedump.c"
if errorlevel 1 ( echo ERROR: Compilation failed. & exit /b 1 )

echo [2/2] Linking php_opcodedump.dll ...
link.exe /nologo /DLL ^
    /OUT:"%OUT%\php_opcodedump.dll" ^
    /LIBPATH:"%DEVEL%\lib" ^
    "%OUT%\opcodedump.obj" ^
    php7.lib
if errorlevel 1 ( echo ERROR: Linking failed. & exit /b 1 )

echo.
echo SUCCESS: %OUT%\php_opcodedump.dll
echo Copy it to  ..\runtime\ext\php_opcodedump.dll  to deploy.
endlocal
