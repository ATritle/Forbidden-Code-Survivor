param([string]$CompilerPath='')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$packageRoot=Join-Path $projectRoot 'Artifacts\Windows'
if(-not(Test-Path -LiteralPath (Join-Path $packageRoot 'ForbiddenCodeSurvivor.exe'))){$packageRoot=Join-Path $packageRoot 'Windows'}
if(-not(Test-Path -LiteralPath (Join-Path $packageRoot 'ForbiddenCodeSurvivor.exe'))){throw 'Package the Windows edition first with Scripts/Package-Windows.ps1.'}
if(-not $CompilerPath){$compilerCommand=Get-Command makensis.exe -ErrorAction SilentlyContinue;$CompilerPath=if($compilerCommand){$compilerCommand.Source}else{Join-Path $projectRoot 'Tools\nsis\nsis-3.13\makensis.exe'}}
if(-not(Test-Path -LiteralPath $CompilerPath)){throw 'Run Scripts/setup_windows_installer.py to fetch the portable official NSIS compiler.'}
$output=Join-Path $projectRoot 'Artifacts\Installer'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$uninstallLines=[Collections.Generic.List[string]]::new()
foreach($file in Get-ChildItem -LiteralPath $packageRoot -File -Recurse | Where-Object {$_.Extension -ne '.pdb' -and $_.Name -notlike 'Manifest_DebugFiles*'}){$relative=$file.FullName.Substring($packageRoot.Length).TrimStart('\');$uninstallLines.Add('Delete "$INSTDIR\'+$relative+'"')}
foreach($directory in Get-ChildItem -LiteralPath $packageRoot -Directory -Recurse | Sort-Object {$_.FullName.Length} -Descending){$relative=$directory.FullName.Substring($packageRoot.Length).TrimStart('\');$uninstallLines.Add('RMDir "$INSTDIR\'+$relative+'"')}
[IO.File]::WriteAllLines((Join-Path $output 'uninstall-files.nsh'),$uninstallLines,[Text.UTF8Encoding]::new($false))
& $CompilerPath ('/DPACKAGE_ROOT='+$packageRoot) (Join-Path $projectRoot 'Packaging\ForbiddenCodeSurvivor.nsi')
if($LASTEXITCODE -ne 0){throw 'NSIS installer compilation failed.'}
Write-Output ('WINDOWS_INSTALLER_READY '+(Join-Path $output 'ForbiddenCode-Survivor-Setup.exe'))
