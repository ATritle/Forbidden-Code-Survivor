@echo off
title Forbidden Code: Survivor - Browser Edition
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Play-Browser.ps1" %*
if errorlevel 1 pause
