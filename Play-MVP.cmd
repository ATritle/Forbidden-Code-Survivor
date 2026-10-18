@echo off
setlocal
rem Double-click for the regular game. Optional PowerShell parameters are forwarded.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Play-MVP.ps1" %*
set "launchExitCode=%ERRORLEVEL%"
if not "%launchExitCode%"=="0" (
    echo.
    echo The game launcher encountered an error. See the message above.
    pause
)
exit /b %launchExitCode%
