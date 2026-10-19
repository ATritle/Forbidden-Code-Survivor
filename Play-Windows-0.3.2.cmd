@echo off
setlocal
set "GAME=%~dp0Artifacts\Windows-0.3.2\Windows\ForbiddenCodeSurvivor.exe"
if not exist "%GAME%" set "GAME=%~dp0Artifacts\Windows-0.3.2\ForbiddenCodeSurvivor.exe"
if not exist "%GAME%" (
  echo The preserved Windows 0.3.2 package is not available in this workspace.
  pause
  exit /b 1
)
start "Forbidden Code Survivor - Windows 0.3.2" "%GAME%"
