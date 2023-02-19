@echo off
setlocal ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

REM set this as the path to your curl
set "CURL=X:\j\curl\curl\build\Win32\VC10\DLL Release - DLL Windows SSPI - DLL WinIDN\curl.exe"

set START_TIME=%TIME%

REM 20000 is approx 30-60 min of repeated transfers to http://curl.se
set MAX=20000

echo Use Sysinternals dbgview to collect debug output while this runs.
timeout 5 /nobreak

REM note the host here is purposely http://curl.se so the command does a real
REM resolve but then completes faster, since http://curl.se just serves a
REM redirect that is not followed.

FOR /L %%G IN (1,1,%MAX%) DO (
  "%CURL%" -sS -f http://curl.se -o NUL --write-out "%%{time_total} "
  if !ERRORLEVEL! NEQ 0 (
    echo.
    if !ERRORLEVEL! EQU 6 (
      echo Reproduced in %%G tries.
    ) else (
      echo Unexpected error code !ERRORLEVEL!, likely not a repro.
    )
    goto bye
  )
)

echo Failed to reproduce the bug with %MAX% tries.

:bye
echo Ran from %START_TIME% to !TIME!.
