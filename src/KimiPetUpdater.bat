@echo off
setlocal
title KimiPet Updater

REM Pure ASCII: no codepage switching, no non-ASCII characters (some cmd break on them).
REM MIRROR: optional proxy prefix, e.g. https://ghproxy.net/  (leave empty for direct).
REM The built-in checker reads the same value from env var KIMIPET_MIRROR.
REM NOTE: /releases/latest/download points to the newest PUBLISHED release.
REM Draft releases are invisible here and their assets return 404 anonymously.
set "MIRROR="

set "REPO=https://github.com/Minecraftgoose/kimi-pet/releases/latest/download"
set "PAGE=https://github.com/Minecraftgoose/kimi-pet/releases"

if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (set "ASSET=KimiPet_x64.exe") else (set "ASSET=KimiPet_x86.exe")

REM Locate the main exe next to this script. Accept the current name and the
REM old MiniPet_x64.exe name used by early releases.
set "DST=%~dp0%ASSET%"
if not exist "%DST%" set "DST=%~dp0KimiPet_x86.exe"
if not exist "%DST%" set "DST=%~dp0KimiPet_x64.exe"
if not exist "%DST%" (
  REM Quoted wildcards do NOT expand in cmd's for, and pushd fails on some
  REM systems. dir /b accepts a quoted wildcard path and is reliable.
  set "DSTFOUND="
  for /f "delims=" %%f in ('dir /b /o-d "%~dp0KimiPet*.exe" "%~dp0MiniPet*.exe" 2^>nul') do if not defined DSTFOUND set "DST=%~dp0%%f" & set "DSTFOUND=1"
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

REM ------------------------------------------------------------------------
REM Pickup mode: if you already downloaded the new exe with your BROWSER,
REM just run this bat again. It scans the usual download folders and uses
REM that file directly -- no command line download needed.
REM This matters on Win7 / .NET 3.5 and behind security software (Huorong),
REM where curl is missing and certutil / PowerShell are often blocked while
REM the browser itself can still reach GitHub just fine.
REM ------------------------------------------------------------------------
set "PICK="
set "PICKED=0"
for %%D in ("%USERPROFILE%\Downloads" "%USERPROFILE%\Desktop" "%~dp0" "%TEMP%") do call :scan "%%~D"
if defined PICK goto picked

set "SRC=%TEMP%\KimiPet_new_%RANDOM%.exe"
set "URL=%MIRROR%%REPO%/%ASSET%"
set "GOT=0"

echo.
echo Downloading... this may take a while.

REM 1) curl.exe - Win10+ ships it. Try real System32, Sysnative (32-bit
REM    process on 64-bit Windows), then whatever is on PATH.
call :try_curl
if "%GOT%"=="1" goto verify

REM 2) certutil.exe - built into Win7+, uses the system HTTP stack, so it is
REM    NOT limited by the old .NET Framework behind PowerShell 2.0.
call :try_certutil
if "%GOT%"=="1" goto verify

REM 3) PowerShell WebClient. TLS 1.2 is enabled by NUMERIC value via
REM    [Enum]::ToObject: .NET 2.0/3.5 has no Tls12 member, and assigning
REM    the named Tls12 member throws PropertyAssignmentException
REM    ("possible enumeration values are Ssl3, Tls") before any download.
call :try_ps
if not "%GOT%"=="1" goto fail
goto verify

:picked
echo.
echo Found a file downloaded by your browser:
echo   %PICK%
for %%A in ("%PICK%") do echo   Size %%~zA bytes, dated %%~tA
echo Make sure this is the version you want, then press any key to use it.
echo (Press Ctrl+C or close this window to cancel.)
pause >nul
set "SRC=%PICK%"
set "PICKED=1"

:verify
REM Validate PE header (MZ) and size; never overwrite with an HTML error page.
if not exist "%SRC%" goto fail
for %%F in ("%SRC%") do if %%~zF LSS 65536 goto fail
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "&{$s=[IO.File]::OpenRead('%SRC%'); $b=New-Object byte[] 2; $n=$s.Read($b,0,2); $s.Close(); if($n -ne 2 -or $b[0] -ne 77 -or $b[1] -ne 90){exit 1}}" >nul 2>&1
if errorlevel 1 (
  findstr /m /b /c:"MZ" "%SRC%" >nul 2>&1
  if errorlevel 1 goto fail
)

echo File OK. Replacing...
copy /y "%SRC%" "%DST%" >nul
if errorlevel 1 (
  ping -n 3 127.0.0.1 >nul
  copy /y "%SRC%" "%DST%" >nul
)
if errorlevel 1 goto fail2
if not "%PICKED%"=="1" del /f /q "%SRC%" >nul 2>&1
echo Update complete. Starting KimiPet...
start "" "%DST%"
ping -n 2 127.0.0.1 >nul
exit /b 0

:scan
REM "for %%f in ("C:\path\*.exe")" does NOT expand wildcards, and pushd
REM fails on some systems. dir /b DOES accept a quoted wildcard path.
set "SD=%~1"
if "%SD:~-1%"=="\" set "SD=%SD:~0,-1%"
if not exist "%SD%\" exit /b 0
for /f "delims=" %%f in ('dir /b /o-d "%SD%\KimiPet_x64*.exe" "%SD%\KimiPet_x86*.exe" "%SD%\MiniPet_x64*.exe" 2^>nul') do if not defined PICK if /i not "%SD%\%%f"=="%DST%" set "PICK=%SD%\%%f"
exit /b 0

:try_curl
"%SystemRoot%\System32\curl.exe" -L -f -sS --retry 2 --connect-timeout 15 --max-time 180 -o "%SRC%" "%URL%" >nul 2>&1
if not errorlevel 1 if exist "%SRC%" goto got
"%SystemRoot%\Sysnative\curl.exe" -L -f -sS --retry 2 --connect-timeout 15 --max-time 180 -o "%SRC%" "%URL%" >nul 2>&1
if not errorlevel 1 if exist "%SRC%" goto got
curl.exe -L -f -sS --retry 2 --connect-timeout 15 --max-time 180 -o "%SRC%" "%URL%" >nul 2>&1
if not errorlevel 1 if exist "%SRC%" goto got
del /f /q "%SRC%" >nul 2>&1
exit /b 0

:try_certutil
certutil.exe -urlcache -split -f "%URL%" "%SRC%" >nul 2>&1
if not errorlevel 1 if exist "%SRC%" goto got
del /f /q "%SRC%" >nul 2>&1
exit /b 0

:try_ps
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ProgressPreference='SilentlyContinue'; try { [Net.ServicePointManager]::SecurityProtocol=[Enum]::ToObject([Net.SecurityProtocolType],3072) } catch {}; try { (New-Object System.Net.WebClient).DownloadFile('%URL%','%SRC%'); exit 0 } catch { exit 1 }" >nul 2>&1
if not errorlevel 1 if exist "%SRC%" goto got
del /f /q "%SRC%" >nul 2>&1
exit /b 0

:got
set "GOT=1"
exit /b 0

:fail
if not "%PICKED%"=="1" if exist "%SRC%" del /f /q "%SRC%" >nul 2>&1
echo.
echo Download failed. Tried: curl.exe, certutil.exe, PowerShell WebClient.
echo.
echo Why this happens on older Windows:
echo   - curl.exe is missing (it ships with Win10+, not Win7)
echo   - Win7 often has system TLS 1.2 disabled; GitHub requires TLS 1.2
echo   - security software (Huorong etc.) blocks certutil / PowerShell downloads
echo If a security prompt appeared, click Allow, then RUN THIS BAT AGAIN:
echo the prompt may have arrived after the tool already gave up.
echo.
echo Your browser CAN reach GitHub (you opened the Releases page).
echo Opening it now - download %ASSET% there, then run this bat once more.
echo It will pick up the downloaded file automatically and replace the old exe.
echo.
echo Manual page: %PAGE%
echo.
start "" "%PAGE%" >nul 2>&1
echo Your current KimiPet was NOT changed. You can reopen it safely.
pause >nul
exit /b 1

:fail2
echo.
echo Replace failed. Make sure KimiPet is fully closed, then run this again.
echo Downloaded file kept at: %SRC%
pause >nul
exit /b 1
