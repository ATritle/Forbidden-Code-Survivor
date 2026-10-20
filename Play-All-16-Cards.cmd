@echo off
setlocal
set "GAME=%~dp0Artifacts\Windows-0.4.7\ForbiddenCodeSurvivor.exe"
if not exist "%GAME%" set "GAME=%~dp0Artifacts\Windows-0.4.7\Windows\ForbiddenCodeSurvivor.exe"
if not exist "%GAME%" (
  echo Build the all-16-card playtest first with Scripts\Package-All16Playtest.ps1.
  pause
  exit /b 1
)
start "Forbidden Code Survivor - All 16 Cards Playtest" "%GAME%" -AllCardsPlaytest "-UserDir=%~dp0Saved\All16Playtest"
