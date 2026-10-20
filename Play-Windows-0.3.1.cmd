@echo off
setlocal
set "GAME=%~dp0Artifacts\Windows-0.3.1\ForbiddenCodeSurvivor.exe"
if not exist "%GAME%" set "GAME=%~dp0Artifacts\Windows-0.3.1\Windows\ForbiddenCodeSurvivor.exe"
if not exist "%GAME%" (
  echo The preserved Windows 0.3.1 package is missing.
  pause
  exit /b 1
)
start "Forbidden Code Survivor 0.3.1" "%GAME%"
