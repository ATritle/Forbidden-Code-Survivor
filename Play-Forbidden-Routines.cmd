@echo off
setlocal
set "GAME=%~dp0Artifacts\Windows-0.3.0\Windows\ForbiddenCodeSurvivor.exe"
if not exist "%GAME%" set "GAME=%~dp0Artifacts\Windows-0.3.0\ForbiddenCodeSurvivor.exe"
if not exist "%GAME%" (
  echo The Windows 0.3.0 package has not been built in this workspace.
  echo Run Scripts\Package-Windows.ps1 to build it.
  pause
  exit /b 1
)
start "Forbidden Code Survivor - Forbidden Routines" "%GAME%"
