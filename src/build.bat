@echo off
setlocal
cd /d "%~dp0"

set CFLAGS=-O2 -mwindows
set LIBS=-lgdi32 -luser32 -ladvapi32 -lwinmm -lshell32 -lwininet -lwinhttp -lm

if not exist "..\..\exe" mkdir "..\..\exe"

echo   -^> Compiling icon resource
windres --codepage=65001 icon.rc -O coff -o icon.res
if errorlevel 1 (
  echo   -^> --codepage not supported, retrying
  windres icon.rc -O coff -o icon.res
  if errorlevel 1 goto :fail
)

echo   -^> KimiPet_x64.exe ^(64-bit^)
gcc %CFLAGS% -o ..\..\exe\KimiPet_x64.exe util.c config.c mod.c render.c update.c winhttpdl.c main.c icon.res %LIBS%
if errorlevel 1 goto :fail

echo   -^> KimiPet_x86.exe ^(32-bit^)
gcc %CFLAGS% -o ..\..\exe\KimiPet_x86.exe util.c config.c mod.c render.c update.c winhttpdl.c main.c icon.res %LIBS%
if errorlevel 1 goto :fail

del /q icon.res
echo Done.
if not "%CI%"=="1" pause
exit /b 0

:fail
echo Build failed. Install MinGW-w64 and make sure gcc / windres are in PATH.
if not "%CI%"=="1" pause
exit /b 1
