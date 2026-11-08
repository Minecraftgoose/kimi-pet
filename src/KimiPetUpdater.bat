@echo off
setlocal
title KimiPet Updater

REM Pure ASCII: no codepage switching, no non-ASCII characters (some cmd break on them).
REM MIRROR: optional proxy prefix, e.g. https://ghproxy.net/  (leave empty for direct).
REM The built-in checker reads the same value from env var KIMIPET_MIRROR.
set "MIRROR="

set "REPO=https://github.com/Minecraftgoose/kimi-pet/releases/latest/download"

if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (set "ASSET=KimiPet_x64.exe") else (set "ASSET=KimiPet_x86.exe")

REM Locate the main exe next to this script.
set "DST=%~dp0%ASSET%"
if not exist "%DST%" set "DST=%~dp0KimiPet_x86.exe"
if not exist "%DST%" set "DST=%~dp0KimiPet_x64.exe"
if not exist "%DST%" (
  for %%f in ("%~dp0KimiPet*.exe") do set "DST=%%~f"
)
if not exist "%DST%" (
  echo KimiPet.exe not found.
  echo Put this .bat in the same folder as KimiPet and run it again.
  pause >nul
  exit /b 1
)

echo Target : %DST%
echo Package: %ASSET%
echo.
echo Exit KimiPet first (right click the ball - Quit), then press any key...
pause >nul

set "SRC=%TEMP%\KimiPet_new_%RANDOM%.exe"
set "URL=%MIRROR%%REPO%/%ASSET%"

echo.
echo Downloading... this may take a while.
REM Try curl directly; if unavailable or failed, fall back to PowerShell.
curl.exe -L -f -sS --retry 2 --connect-timeout 15 --max-time 180 -o "%SRC%" "%URL%"
if errorlevel 1 (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ProgressPreference='SilentlyContinue'; [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; try { (New-Object System.Net.WebClient).DownloadFile('%URL%','%SRC%') } catch { exit 1 }"
  if errorlevel 1 goto fail
)

REM Validate PE header (MZ) and size; never overwrite with an HTML error page.
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "&{$p='%SRC%'; if(-not (Test-Path $p)){exit 1}; if((Get-Item $p).Length -lt 65536){exit 1}; $s=[IO.File]::OpenRead($p); $b=New-Object byte[] 2; $n=$s.Read($b,0,2); $s.Close(); if($n -ne 2 -or $b[0] -ne 77 -or $b[1] -ne 90){exit 1}}"
if errorlevel 1 goto fail

echo Download OK. Replacing...
copy /y "%SRC%" "%DST%" >nul
if errorlevel 1 (
  ping -n 3 127.0.0.1 >nul
  copy /y "%SRC%" "%DST%" >nul
)
if errorlevel 1 goto fail2
del /f /q "%SRC%" >nul 2>&1
echo Update complete. Starting KimiPet...
start "" "%DST%"
ping -n 2 127.0.0.1 >nul
exit /b 0

:fail
del /f /q "%SRC%" >nul 2>&1
echo.
echo Download failed (network blocked or GitHub download host unreachable).
echo Try setting MIRROR at the top of this file, or open manually:
echo   https://github.com/Minecraftgoose/kimi-pet/releases
echo.
echo Your current KimiPet was NOT changed. You can reopen it safely.
pause >nul
exit /b 1

:fail2
echo.
echo Replace failed. Make sure KimiPet is fully closed, then run this again.
echo Downloaded file kept at: %SRC%
pause >nul
exit /b 1
