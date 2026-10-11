@echo off
setlocal
cd /d "%~dp0"

set CFLAGS=-O2 -mwindows
set LIBS=-lgdi32 -luser32 -ladvapi32 -lwinmm -lshell32 -lm

if not exist "..\exe" mkdir "..\exe"

echo   -^> 编译图标资源
windres --codepage=65001 icon.rc -O coff -o icon.res
if errorlevel 1 (
  echo    （--codepage 不被支持）
  windres icon.rc -O coff -o icon.res
  if errorlevel 1 goto :fail
)

echo   -^> KimiPet_x64.exe ^(64 位^)
gcc %CFLAGS% -o ..\exe\KimiPet_x64.exe pet.c icon.res %LIBS%
if errorlevel 1 goto :fail

echo   -^> KimiPet_x86.exe ^(32 位^)
gcc %CFLAGS% -o ..\exe\KimiPet_x86.exe pet.c icon.res %LIBS%
if errorlevel 1 goto :fail

del /q icon.res
echo 完成。
if not "%CI%"=="1" pause
exit /b 0

:fail
echo 编译失败：确认已安装 MinGW-w64，且 gcc / windres 在 PATH 中。
if not "%CI%"=="1" pause
exit /b 1
