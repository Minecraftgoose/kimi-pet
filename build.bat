@echo off
REM Kimi 小圆球 · Windows 本机编译脚本
REM 需要 MinGW-w64：MSYS2 的 mingw64 shell，或 w64devkit（https://github.com/skeeto/w64devkit）
REM 双击本文件即可，图标与版本信息由 icon.rc 自动嵌入。
setlocal
cd /d "%~dp0"

set CFLAGS=-O2 -mwindows
set LIBS=-lgdi32 -luser32 -ladvapi32 -lwinmm -lshell32 -lm

echo   -^> 编译图标资源
windres --codepage=65001 icon.rc -O coff -o icon.res
if errorlevel 1 (
  echo    （--codepage 不被支持，退回默认编码重试）
  windres icon.rc -O coff -o icon.res
  if errorlevel 1 goto :fail
)

echo   -^> KimiPet_x64.exe ^(64 位^)
gcc %CFLAGS% -o KimiPet_x64.exe pet.c icon.res %LIBS%
if errorlevel 1 goto :fail

echo   -^> KimiPet_x86.exe ^(32 位^)
gcc %CFLAGS% -o KimiPet_x86.exe pet.c icon.res %LIBS%
if errorlevel 1 goto :fail

del /q icon.res
echo 完成。
if not "%CI%"=="1" pause
exit /b 0

:fail
echo 编译失败：确认已安装 MinGW-w64，且 gcc / windres 在 PATH 中。
if not "%CI%"=="1" pause
exit /b 1
