$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'Package-Windows.ps1') -OutputDirectory (Join-Path $root 'Artifacts/Windows-All16-Playtest') -Version 0.4.5
if(-not $?){throw 'All-16-card playtest packaging failed'}
