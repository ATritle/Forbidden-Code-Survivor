@echo off
setlocal
rem Launch the adjacent PowerShell script from any working directory.
rem Optional arguments are forwarded, for example: -Enemies 1000
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Play-CinematicTest.ps1" %*
set "launchExitCode=%ERRORLEVEL%"
if not "%launchExitCode%"=="0" (
    echo.
    echo The cinematic test launcher encountered an error. See the message above.
    pause
)
exit /b %launchExitCode%
