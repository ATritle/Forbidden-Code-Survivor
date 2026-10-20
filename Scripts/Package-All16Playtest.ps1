$ErrorActionPreference='Stop'
# The regular game now supports all 16 attacks. The playtest launcher uses this same package with isolated saves.
& (Join-Path $PSScriptRoot 'Package-Windows.ps1') -Version 0.4.6
if(-not $?){throw 'Windows packaging failed'}
