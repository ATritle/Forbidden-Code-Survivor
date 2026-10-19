@echo off
setlocal
set "GAME=%~dp0Artifacts\Windows-0.3.0\ForbiddenCodeSurvivor.exe"
if not exist "%GAME%" set "GAME=%~dp0Artifacts\Windows-0.3.0\Windows\ForbiddenCodeSurvivor.exe"
if not exist "%GAME%" (
  echo The preserved Windows 0.3.0 package is missing.
  pause
  exit /b 1
)
start "Forbidden Code Survivor 0.3.0" "%GAME%"
