@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Artifacts\Baselines\Windows-PreOverhaul-20261019T021817Z\Restore-Baseline.ps1" -Destination "%~dp0Artifacts\RestoredOriginal"
pause
