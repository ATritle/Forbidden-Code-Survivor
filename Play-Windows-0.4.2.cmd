@echo off
setlocal
set "GAME=%~dp0Artifacts\Windows-0.4.2\Windows\ForbiddenCodeSurvivor.exe"
if not exist "%GAME%" set "GAME=%~dp0Artifacts\Windows-0.4.2\ForbiddenCodeSurvivor.exe"
if not exist "%GAME%" (
  echo The Windows 0.4.2 package has not been built in this workspace.
  echo Run Scripts\Package-Windows.ps1 to build it.
  pause
  exit /b 1
)
start "Forbidden Code Survivor - Forbidden Routines" "%GAME%"
